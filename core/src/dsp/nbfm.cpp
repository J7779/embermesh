#include "dsp/nbfm.h"

#include <cmath>

namespace ember {
namespace dsp {

bool NbfmDemod::init(const NbfmConfig& c) {
  ok_ = false;
  cfg_ = c;
  stages_.clear();
  if (c.iq_rate < 16000 || c.iq_rate > 10000000 || c.audio_rate < 8000 || c.audio_rate > 384000) return false;
  if (!(c.channel_bw_hz >= 1000) || !(c.deviation_hz > 0) || !std::isfinite(c.channel_bw_hz) ||
      !std::isfinite(c.deviation_hz) || !std::isfinite(c.offset_hz) || !std::isfinite(c.if_min_hz) ||
      !std::isfinite(c.audio_lp_hz) || !std::isfinite(c.deemphasis_us))
    return false;
  double half_bw = c.channel_bw_hz / 2;
  double if_min = c.if_min_hz > c.channel_bw_hz * 1.25 ? c.if_min_hz : c.channel_bw_hz * 1.25;
  double r = c.iq_rate;
  while (true) {
    int f = 0;
    for (int cand = 8; cand >= 2; --cand) {
      if (r / cand >= if_min) { f = cand; break; }
    }
    if (!f) break;
    double rout = r / f;
    // Pass [0, half_bw], stop from rout - half_bw (only that range can alias into the channel).
    double trans = rout - 2 * half_bw;
    if (trans < rout * 0.05) trans = rout * 0.05;
    size_t taps = kaiser_taps(60, trans, r, 511);
    FirDecimC d;
    d.init(fir_lowpass(taps, half_bw + trans / 2, r, Window::Kaiser, kaiser_beta(60)), static_cast<size_t>(f));
    stages_.push_back(d);
    r = rout;
  }
  if_rate_ = r;
  if (if_rate_ < c.channel_bw_hz) return false;  // input too slow for this channel width
  double trans = half_bw * 0.25;
  size_t ctaps = kaiser_taps(50, trans, if_rate_, 255);
  chan_.set_taps(fir_lowpass(ctaps, half_bw, if_rate_, Window::Kaiser, kaiser_beta(50)));
  tune_ = c.offset_hz != 0;
  nco_.set_freq(-c.offset_hz, c.iq_rate);
  scale_ = static_cast<float>(if_rate_ / (kTwoPi * c.deviation_hz));
  use_deemph_ = c.deemphasis_us > 0;
  if (use_deemph_) {
    deemph_ = Biquad::deemphasis(c.deemphasis_us * 1e-6, if_rate_);
  }
  if (!rs_.init(if_rate_, c.audio_rate)) return false;
  double lp = c.audio_lp_hz;
  if (lp > 0.45 * c.audio_rate) lp = 0.45 * c.audio_rate;
  use_lp_ = lp > 100;
  if (use_lp_) {
    lp1_ = Biquad::lowpass(lp, c.audio_rate, 0.5411961);
    lp2_ = Biquad::lowpass(lp, c.audio_rate, 1.3065630);
  }
  dc_.init(c.audio_rate, 10.0);
  ok_ = true;
  reset();
  return true;
}

void NbfmDemod::reset() {
  for (auto& s : stages_) s.reset();
  chan_.reset();
  nco_.reset();
  prev_ = Cf(1, 0);
  deemph_.reset();
  rs_.reset();
  lp1_.reset();
  lp2_.reset();
  dc_.reset();
  pending_byte_ = -1;
  chan_pow_ = 0;
}

void NbfmDemod::push_if(Cf z, std::vector<float>& out) {
  chan_.push(z);
  Cf y = chan_.output();
  chan_pow_ += 0.001f * (norm2(y) - chan_pow_);
  Cf d = y * conj(prev_);
  prev_ = y;
  float a = 0;
  if (d.re != 0 || d.im != 0) a = std::atan2(d.im, d.re) * scale_;
  if (use_deemph_) a = deemph_.process(a);
  out.push_back(a);
}

void NbfmDemod::process_cf(const Cf* iq, size_t n, std::vector<float>& audio) {
  if (!ok_ || !iq) return;
  if_audio_.clear();
  for (size_t i = 0; i < n; ++i) {
    Cf z = iq[i];
    // NaN / Inf IQ would stick in the channel-power EMA and the IIR stages forever.
    if (!(z.re < 1e30f && z.re > -1e30f)) z.re = 0;
    if (!(z.im < 1e30f && z.im > -1e30f)) z.im = 0;
    if (tune_) z = z * nco_.next();
    bool have = true;
    for (auto& s : stages_) {
      Cf o;
      if (!s.push(z, &o)) { have = false; break; }
      z = o;
    }
    if (have) push_if(z, if_audio_);
  }
  rs_out_.clear();
  rs_.process(if_audio_.data(), if_audio_.size(), rs_out_);
  for (float v : rs_out_) {
    if (use_lp_) v = lp2_.process(lp1_.process(v));
    audio.push_back(dc_.process(v));
  }
}

void NbfmDemod::process_u8(const uint8_t* iq, size_t nbytes, std::vector<float>& audio) {
  if (!ok_ || !iq) return;
  const float k = 1.0f / 127.5f;
  stage_buf_.clear();
  size_t i = 0;
  if (pending_byte_ >= 0 && nbytes > 0) {
    stage_buf_.push_back(Cf((pending_byte_ - 127.5f) * k, (iq[0] - 127.5f) * k));
    pending_byte_ = -1;
    i = 1;
  }
  for (; i + 1 < nbytes; i += 2) stage_buf_.push_back(Cf((iq[i] - 127.5f) * k, (iq[i + 1] - 127.5f) * k));
  if (i < nbytes) pending_byte_ = iq[i];
  process_cf(stage_buf_.data(), stage_buf_.size(), audio);
}

}  // namespace dsp
}  // namespace ember
