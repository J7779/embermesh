#include "modem/cw_rx.h"

#include <algorithm>
#include <cmath>

namespace ember {
namespace modem {

using dsp::Cf;

bool CwReceiver::init(const Config& c) {
  cfg_ = c;
  if (!(cfg_.bw_hz >= 30 && cfg_.bw_hz <= 400)) cfg_.bw_hz = 100;
  if (!(cfg_.afc_lo_hz >= 100)) cfg_.afc_lo_hz = 300;
  if (!(cfg_.afc_hi_hz <= 3500 && cfg_.afc_hi_hz > cfg_.afc_lo_hz)) cfg_.afc_hi_hz = 1500;
  if (!(cfg_.tone_hz >= 100 && cfg_.tone_hz <= 3500)) cfg_.tone_hz = 700;
  tone_ = cfg_.tone_hz;
  double bw = cfg_.bw_hz;
  lp_.init(dsp::fir_lowpass(dsp::kaiser_taps(50, 250 - bw / 2 > 50 ? 250 - bw / 2 : 50, kRate, 255), bw / 2, kRate,
                            dsp::Window::Kaiser, dsp::kaiser_beta(50)),
           kDecim);
  nco_.set_freq(tone_, kRate);
  fft_.init(512);
  spec_.assign(256, 0.0f);
  fbuf_.resize(512);
  hist_.assign(512, 0.0f);
  timing_.configure(cfg_.timing);
  reset();
  return true;
}

void CwReceiver::reset() {
  env_s_ = 0;
  floor_ = peak_ = 1e-6f;
  key_ = seen_ = false;
  warmup_ = 0;
  last_mark_end_ms_ = -1e9;
  timing_.reset();
}

bool CwReceiver::busy() const {
  return key_ || !timing_.pending().empty() || (seen_ && time_ms() - last_mark_end_ms_ < 7 * timing_.dot_ms());
}

double CwReceiver::snr_db() const { return 20 * std::log10((peak_ + 1e-12) / (floor_ + 1e-12)); }

double CwReceiver::level() const {
  double r = peak_ > 0 ? env_s_ / peak_ : 0;
  return std::min(std::max(r, 0.0), 1.0);
}

void CwReceiver::process(const float* x, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    hist_[hpos_] = x[i];
    hpos_ = (hpos_ + 1) % hist_.size();
    if (cfg_.afc && ++fcount_ >= 256) {
      fcount_ = 0;
      afc_step();
    }
    Cf z;
    if (!lp_.push(nco_.next_conj() * x[i], &z)) continue;
    tick(dsp::cabs(z));
  }
}

void CwReceiver::tick(float e) {
  ++ticks_;
  double t = time_ms();
  env_s_ += 0.5f * (e - env_s_);  // light smoothing (~ 4 ms)
  float v = env_s_;
  if (warmup_ < 100) {  // first 200 ms: learn the floor, do not key
    floor_ = warmup_ == 0 ? v : floor_ + 0.1f * (v - floor_);
    peak_ = floor_;
    ++warmup_;
    timing_.advance(t);
    return;
  }
  // Floor: fast down, ~100 ms up, only while the key is up (long dahs do not raise it).
  // Peak: fast up, slow decay toward the floor.
  if (v < floor_) floor_ += 0.2f * (v - floor_);
  else if (!key_) floor_ += 0.02f * (v - floor_);
  if (v > peak_) peak_ += 0.3f * (v - peak_);
  else peak_ -= 0.0008f * (peak_ - floor_);
  if (peak_ < floor_) peak_ = floor_;
  bool squelch_ok = peak_ > 6.0f * floor_ + 1e-9f;  // Rayleigh noise peaks very rarely reach 6x the floor
  float range = peak_ - floor_;
  bool on = key_ ? (v > floor_ + 0.40f * range) : (v > floor_ + 0.55f * range);
  if (!squelch_ok) on = false;
  if (on != key_) {
    key_ = on;
    if (on) seen_ = true;
    else last_mark_end_ms_ = t;
    timing_.key(on, t);
  } else {
    timing_.advance(t);
  }
}

void CwReceiver::afc_step() {
  for (size_t i = 0; i < 512; ++i) {
    float w = 0.5f - 0.5f * std::cos(static_cast<float>(dsp::kTwoPi) * i / 511);
    fbuf_[i] = Cf(hist_[(hpos_ + i) % 512] * w, 0);
  }
  fft_.forward(fbuf_.data());
  for (size_t k = 0; k < 256; ++k) spec_[k] += 0.03f * (dsp::norm2(fbuf_[k]) - spec_[k]);
  if (++afc_n_ < 16 || key_ || !timing_.pending().empty()) return;
  const double bin = kRate / 512;
  size_t lo = static_cast<size_t>(cfg_.afc_lo_hz / bin), hi = static_cast<size_t>(cfg_.afc_hi_hz / bin);
  if (hi > 255) hi = 255;
  if (hi <= lo + 2) return;
  std::vector<float> band(spec_.begin() + static_cast<std::ptrdiff_t>(lo), spec_.begin() + static_cast<std::ptrdiff_t>(hi));
  size_t pk = 0;
  for (size_t k = 1; k < band.size(); ++k)
    if (band[k] > band[pk]) pk = k;
  std::vector<float> sorted = band;
  std::nth_element(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(sorted.size() / 2), sorted.end());
  float med = sorted[sorted.size() / 2];
  if (!(band[pk] > 10 * med) || pk == 0 || pk + 1 >= band.size()) return;
  double a = band[pk - 1], b = band[pk], c = band[pk + 1];
  double den = a - 2 * b + c;
  double d = std::fabs(den) > 1e-20 ? 0.5 * (a - c) / den : 0;
  double f = (static_cast<double>(lo + pk) + d) * bin;
  if (std::fabs(f - tone_) > 12) {
    tone_ = f;
    nco_.set_freq(tone_, kRate);
  }
}

}  // namespace modem
}  // namespace ember
