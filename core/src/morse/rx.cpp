#include "morse/rx.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ember {
namespace morse_rx {

namespace {

double expf_alpha(double dt, double tau) { return dt <= 0 ? 0 : 1.0 - std::exp(-dt / tau); }

size_t utf8_count(const std::string& s) {
  size_t n = 0;
  for (unsigned char c : s) n += (c & 0xC0) != 0x80;
  return n;
}

// Counts characters as the contract does: a prosign "<XX>" is one character, spaces are not.
uint64_t count_chars(const std::string& s) {
  uint64_t n = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c == ' ') continue;
    if (c == '<') {
      size_t e = s.find('>', i);
      if (e != std::string::npos && e - i <= 5) { ++n; i = e; continue; }
    }
    n += (c & 0xC0) != 0x80;
  }
  return n;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Light

LightKeyer::LightKeyer(const modem::MorseTiming::Config& tc) : timing_(tc) {}

void LightKeyer::reset() {
  timing_.reset();
  have_ = key_ = false;
  last_t_ = last_v_ = offset_ = 0;
  hi_ = lo_ = 0;
  noise_ = 0.015;
  frame_ms_ = 33;
  nprev_ = 0;
}

double LightKeyer::threshold() const { return lo_ + 0.5 * (hi_ - lo_); }

double LightKeyer::level() const {
  double r = hi_ - lo_;
  if (r < kMinContrast) return 0;
  return std::min(std::max((last_v_ - lo_) / r, 0.0), 1.0);
}

void LightKeyer::sample(double t, double v) {
  if (!std::isfinite(t) || !std::isfinite(v)) return;
  v = std::min(std::max(v, 0.0), 1.0);
  if (!have_) {
    have_ = true;
    offset_ = -t;  // our axis starts at 0 whatever the caller's origin
    last_t_ = 0;
    last_v_ = hi_ = lo_ = v;
    timing_.advance(0);
    return;
  }
  double tt = t + offset_;
  double dt = tt - last_t_;
  if (dt <= 0) {
    // Out of order or duplicate: drop it. A large jump back is a new time origin (camera
    // restarted, host clock reset): continue our axis one frame after the last sample.
    if (dt < -5000) offset_ += last_t_ - tt + 33;
    return;
  }
  if (dt > 30000) {  // a long pause (or an absurd jump): end the words, then compress the gap
    timing_.advance(last_t_ + 30000);
    offset_ -= dt - 30000;
    tt = last_t_ + 30000;
    dt = 30000;
  }
  // Frame interval (bounded): drop-outs up to ~1.6 frames inside a mark are bridged.
  frame_ms_ += 0.05 * (std::min(dt, 200.0) - frame_ms_);
  timing_.set_max_bridge_ms(1.6 * frame_ms_);
  // Envelope: the level of the current state is tracked closely (auto-exposure pumping, hand
  // shake), the other extreme decays slowly toward the signal (ambient drift, lost contrast).
  const double kTrack = 350, kSlow = 6000;
  if (v > hi_) hi_ += 0.7 * (v - hi_);
  if (v < lo_) lo_ += 0.7 * (v - lo_);
  if (key_) {
    if (v < hi_) hi_ += expf_alpha(dt, kTrack) * (v - hi_);
    lo_ += expf_alpha(dt, kSlow) * (std::min(v, hi_) - lo_) * 0.25;
  } else {
    if (v > lo_) lo_ += expf_alpha(dt, kTrack) * (v - lo_);
    hi_ += expf_alpha(dt, kSlow) * (std::max(v, lo_) - hi_);
  }
  if (hi_ < lo_) hi_ = lo_;
  // Noise: frame-to-frame changes, clipped so that real transitions barely move the estimate.
  double range = hi_ - lo_;
  double dv = std::fabs(v - last_v_);
  // Quantile tracker: transitions (even 40 % of frames at 2 frames per dot) do not bias the 25th
  // percentile, which is what makes it a usable noise scale.
  noise_ += (0.02 * noise_ + 0.00002) * ((dv > noise_ ? 1.0 : 0.0) - 0.75) * 4;
  if (noise_ < 0.0005) noise_ = 0.0005;
  bool on = key_;
  if (!contrast_ok()) {
    on = false;
  } else {
    double up = lo_ + 0.58 * range, down = lo_ + 0.42 * range;
    on = key_ ? v > down : v > up;
    // A real light switching on is a sharp edge (within about two frames); auto-exposure ramps
    // and ambient drift are slow. Require the rise from the last couple of frames to be a large
    // part of the range and well above the noise.
    if (on && !key_) {
      double base = last_v_;
      for (int i = 0; i < nprev_; ++i) base = std::min(base, prev_v_[i]);
      if (v - base < std::max({0.5 * range, 13 * noise_, kMinContrast})) on = false;
    }
  }
  if (on != key_) {
    // Interpolate the crossing inside the frame interval (low frame rates).
    double thr = lo_ + 0.5 * range;
    double f = 0.5;
    if (v != last_v_) f = std::min(std::max((thr - last_v_) / (v - last_v_), 0.0), 1.0);
    double tc = last_t_ + f * dt;
    timing_.key(on, tc);
    key_ = on;
  }
  timing_.advance(tt);
  prev_v_[1] = prev_v_[0];
  prev_v_[0] = last_v_;
  if (nprev_ < 2) ++nprev_;
  last_t_ = tt;
  last_v_ = v;
}

// ---------------------------------------------------------------------------------------------
// Audio

AudioDecoder::AudioDecoder(uint32_t rate, double tone_hz, const modem::MorseTiming::Config& tc)
    : rate_(rate), tone_(tone_hz), tc_(tc) {
  reset();
}

void AudioDecoder::reset() {
  rs_.init(rate_, modem::CwReceiver::kRate);
  modem::CwReceiver::Config c;
  c.timing = tc_;
  c.afc = true;
  if (tone_ > 0) {  // given tone: follow drift near it only
    c.tone_hz = tone_;
    c.afc_lo_hz = std::max(150.0, tone_ - 150);
    c.afc_hi_hz = std::min(3400.0, tone_ + 150);
  } else {
    c.tone_hz = 700;
    c.afc_lo_hz = 300;
    c.afc_hi_hz = 1500;
  }
  rx_.init(c);
}

void AudioDecoder::feed(const int16_t* pcm, size_t n) {
  const size_t kChunk = 4096;
  for (size_t off = 0; off < n; off += kChunk) {
    size_t k = std::min(kChunk, n - off);
    in_.resize(k);
    for (size_t i = 0; i < k; ++i) in_[i] = static_cast<float>(pcm[off + i]) / 32768.0f;
    if (rs_.passthrough()) {
      rx_.process(in_.data(), k);
    } else {
      out_.clear();
      rs_.process(in_.data(), k, out_);
      if (!out_.empty()) rx_.process(out_.data(), out_.size());
    }
  }
}

// ---------------------------------------------------------------------------------------------
// Session

std::unique_ptr<Session> Session::create(const std::string& source, uint32_t rate, double wpm_hint, double tone_hz,
                                         std::string* error) {
  auto fail = [&](const char* m) {
    if (error) *error = m;
    return std::unique_ptr<Session>();
  };
  if (source != "light" && source != "audio") return fail("source must be \"light\" or \"audio\"");
  if (!(wpm_hint >= 0 && wpm_hint <= 60)) return fail("wpm_hint must be 0..60");
  std::unique_ptr<Session> s(new Session());
  s->source_ = source;
  s->tc_.wpm_hint = wpm_hint;
  s->tc_.prosign_names = true;
  if (source == "light") {
    s->tc_.min_wpm = 2;
    s->tc_.max_wpm = 30;
    s->light_.reset(new LightKeyer(s->tc_));
  } else {
    if (rate < kMinAudioRate || rate > kMaxAudioRate) return fail("sample_rate must be 8000..192000");
    if (!(tone_hz == 0 || (tone_hz >= 300 && tone_hz <= 1500))) return fail("tone_hz must be 300..1500 (or omitted)");
    s->tc_.min_wpm = 4;
    s->tc_.max_wpm = 50;
    s->tc_.min_mark_ms = 8;
    s->tc_.max_bridge_ms = 15;  // envelope drop-outs are short; 40 WPM element gaps are 30 ms
    s->audio_.reset(new AudioDecoder(rate, tone_hz, s->tc_));
  }
  return s;
}

modem::MorseTiming& Session::timing() { return light_ ? light_->timing() : audio_->timing(); }
const modem::MorseTiming& Session::timing() const {
  return light_ ? light_->timing() : audio_->rx().timing();
}

void Session::collect() {
  std::string t;
  timing().take_text(&t);
  if (t.empty()) return;
  // No leading or doubled word spaces in the transcript.
  std::string clean;
  for (char c : t) {
    char prev = !clean.empty() ? clean.back() : !transcript_.empty() ? transcript_.back() : ' ';
    if (c == ' ' && prev == ' ') continue;
    clean.push_back(c);
  }
  if (clean.empty()) return;
  chars_ += count_chars(clean);
  transcript_ += clean;
  new_ += clean;
  if (new_.size() > 16384) new_.erase(0, new_.size() - 16384);
  size_t n = utf8_count(transcript_);
  if (n > kTranscriptChars) {
    size_t drop = n - kTranscriptChars, i = 0;
    while (drop && i < transcript_.size()) {
      ++i;
      while (i < transcript_.size() && (static_cast<unsigned char>(transcript_[i]) & 0xC0) == 0x80) ++i;
      --drop;
    }
    transcript_.erase(0, i);
  }
}

void Session::feed_uniform(double t0, double dt, const std::vector<double>& levels) {
  if (!light_) return;
  for (size_t i = 0; i < levels.size(); ++i) light_->sample(t0 + dt * static_cast<double>(i), levels[i]);
  collect();
}

void Session::feed_times(const std::vector<double>& times, const std::vector<double>& levels) {
  if (!light_) return;
  size_t n = std::min(times.size(), levels.size());
  for (size_t i = 0; i < n; ++i) light_->sample(times[i], levels[i]);
  collect();
}

void Session::feed_pcm(const int16_t* pcm, size_t n) {
  if (!audio_) return;
  audio_->feed(pcm, n);
  collect();
}

std::string Session::take_new() {
  std::string t;
  t.swap(new_);
  return t;
}

Status Session::status() const {
  Status s;
  s.transcript = transcript_;
  s.chars = chars_;
  const modem::MorseTiming& tm = timing();
  s.wpm = tm.wpm();
  s.on = tm.on();
  s.quality = std::round(tm.quality() * 100) / 100;
  s.pending = tm.pending();
  if (light_) {
    s.level = light_->level();
    s.threshold = std::min(std::max(light_->threshold(), 0.0), 1.0);
  } else {
    s.audio = true;
    s.level = audio_->rx().level();
    s.tone_hz = std::round(audio_->rx().tone_hz());
    s.snr_db = std::round(audio_->rx().snr_db() * 10) / 10;
  }
  return s;
}

void Session::reset() {
  if (light_) light_->reset();
  if (audio_) audio_->reset();
  transcript_.clear();
  new_.clear();
  chars_ = 0;
}

// ---------------------------------------------------------------------------------------------
// Sessions

std::string Sessions::start(std::unique_ptr<Session> s, uint64_t now, std::string* error) {
  expire(now);
  if (s_.size() >= kMaxSessions) {
    if (error) *error = "busy";
    return std::string();
  }
  char id[24];
  do {
    uint64_t v = next_++ * 0x9E3779B97F4A7C15ull;
    std::snprintf(id, sizeof id, "%08x", static_cast<unsigned>((v >> 32) ^ v));
  } while (s_.count(id));
  Entry e;
  e.s = std::move(s);
  e.last = now;
  s_[id] = std::move(e);
  return id;
}

Session* Sessions::get(const std::string& id, uint64_t now) {
  auto it = s_.find(id);
  if (it == s_.end()) return nullptr;
  if (now >= it->second.last + kIdleExpiryMs) {
    s_.erase(it);
    return nullptr;
  }
  it->second.last = now;
  return it->second.s.get();
}

bool Sessions::stop(const std::string& id, std::string* transcript) {
  auto it = s_.find(id);
  if (it == s_.end()) return false;
  if (transcript) *transcript = it->second.s->transcript();
  s_.erase(it);
  return true;
}

std::vector<std::string> Sessions::expire(uint64_t now) {
  std::vector<std::string> out;
  for (auto it = s_.begin(); it != s_.end();) {
    if (now >= it->second.last + kIdleExpiryMs) {
      out.push_back(it->first);
      it = s_.erase(it);
    } else {
      ++it;
    }
  }
  return out;
}

}  // namespace morse_rx
}  // namespace ember
