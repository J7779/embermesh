#include "dsp/channel_sim.h"

#include <cmath>

#include "dsp/resampler.h"

namespace ember {
namespace dsp {

HilbertIir::HilbertIir() {
  // Coefficients a of the two polyphase all-pass paths (Olli Niemitalo, "Hilbert transform" IIR
  // design, 8 coefficients); each section is y[n] = a^2 (x[n] + y[n-2]) - x[n-2].
  static const float c1[4] = {0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f};
  static const float c2[4] = {0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f};
  for (int i = 0; i < 4; ++i) { p1_[i].a = c1[i] * c1[i]; p2_[i].a = c2[i] * c2[i]; }
}

void HilbertIir::reset() {
  for (int i = 0; i < 4; ++i) {
    float a1 = p1_[i].a, a2 = p2_[i].a;
    p1_[i] = Ap();
    p2_[i] = Ap();
    p1_[i].a = a1;
    p2_[i].a = a2;
  }
  d1_ = 0;
}

void HilbertIir::process(float x, float* i_out, float* q_out) {
  float a = x, b = x;
  for (int k = 0; k < 4; ++k) a = p1_[k].run(a);
  for (int k = 0; k < 4; ++k) b = p2_[k].run(b);
  *i_out = d1_;
  d1_ = a;
  *q_out = b;
}

void FreqShifter::init(double hz, double fs) {
  h_.reset();
  phase_ = 0;
  inc_ = (fs > 0) ? kTwoPi * hz / fs : 0;
}

float FreqShifter::process(float x) {
  float i, q;
  h_.process(x, &i, &q);
  // q leads i by 90 degrees (for i = cos, q = -sin), so z = i - j*q is the analytic signal and
  // Re(z * e^{j phase}) = i*c + q*s moves every component up by the shift (verified by test).
  float c = static_cast<float>(std::cos(phase_)), s = static_cast<float>(std::sin(phase_));
  phase_ += inc_;
  if (phase_ > kTwoPi) phase_ -= kTwoPi;
  if (phase_ < -kTwoPi) phase_ += kTwoPi;
  return i * c + q * s;
}

ChannelSim::ChannelSim(const ChannelParams& p, double fs) : p_(p), fs_(fs > 0 ? fs : 8000), rng_(p.seed) {}

std::vector<float> ChannelSim::run(const std::vector<float>& tx, size_t lead, size_t trail) {
  std::vector<float> x(lead, 0.0f);
  x.insert(x.end(), tx.begin(), tx.end());
  x.insert(x.end(), trail, 0.0f);

  // 1. tilt
  if (p_.tilt != 0) {
    double tau = 1.0 / (kTwoPi * (p_.tilt_corner_hz > 1 ? p_.tilt_corner_hz : 1));
    Biquad b = p_.tilt < 0 ? Biquad::deemphasis(tau, fs_) : Biquad::preemphasis(tau, fs_, 1.0 / (kTwoPi * 6000.0));
    for (float& v : x) v = b.process(v);
  }
  // 2. band-limit
  if (p_.bandlimit) {
    Biquad h1 = Biquad::highpass(p_.bandlimit_lo_hz, fs_), h2 = Biquad::highpass(p_.bandlimit_lo_hz, fs_);
    Biquad l1 = Biquad::lowpass(p_.bandlimit_hi_hz, fs_), l2 = Biquad::lowpass(p_.bandlimit_hi_hz, fs_);
    for (float& v : x) v = l2.process(l1.process(h2.process(h1.process(v))));
  }
  // 3. frequency offset
  if (p_.freq_offset_hz != 0) {
    FreqShifter fsh;
    fsh.init(p_.freq_offset_hz, fs_);
    for (float& v : x) v = fsh.process(v);
  }
  // 4. clock drift: a receiver clock that is fast by ppm takes more samples per second.
  if (p_.clock_ppm != 0) {
    Resampler rs;
    rs.init(fs_, fs_);
    rs.set_step_scale(1.0 / (1.0 + p_.clock_ppm * 1e-6));
    std::vector<float> y;
    y.reserve(x.size() + x.size() / 1000 + 64);
    rs.process(x.data(), x.size(), y);
    rs.flush(y);
    x.swap(y);
  }
  // 5. fading
  if (p_.fade_depth_db > 0) {
    double depth = p_.fade_depth_db;
    double w = kTwoPi * p_.fade_hz / fs_;
    for (size_t i = 0; i < x.size(); ++i) {
      double db = -depth * 0.5 * (1 - std::cos(w * static_cast<double>(i)));
      x[i] *= static_cast<float>(std::pow(10.0, db / 20));
    }
  }
  // 6. noise, SNR measured on the signal portion.
  sig_pow_ = 0;
  noise_var_ = 0;
  {
    size_t a = lead < x.size() ? lead : x.size();
    size_t b = a + tx.size();
    if (b > x.size()) b = x.size();
    double acc = 0;
    for (size_t i = a; i < b; ++i) acc += static_cast<double>(x[i]) * x[i];
    sig_pow_ = b > a ? acc / static_cast<double>(b - a) : 0;
  }
  if (p_.snr_db < 150) {
    double pref = sig_pow_ > 0 ? sig_pow_ : 1e-6;
    double nref = pref / std::pow(10.0, p_.snr_db / 10);
    noise_var_ = nref * (fs_ / 2) / p_.noise_ref_bw_hz;
    double sd = std::sqrt(noise_var_);
    for (float& v : x) v += static_cast<float>(sd * rng_.gauss());
  }
  // 7. gain, dc, clip, quantise
  float g = static_cast<float>(std::pow(10.0, p_.gain_db / 20));
  float dc = static_cast<float>(p_.dc_offset);
  float cl = static_cast<float>(p_.clip_level);
  for (float& v : x) {
    v = v * g + dc;
    if (cl > 0) v = v > cl ? cl : (v < -cl ? -cl : v);
    if (p_.quantize_pcm16) v = pcm16_to_float(float_to_pcm16(v));
  }
  return x;
}

}  // namespace dsp
}  // namespace ember
