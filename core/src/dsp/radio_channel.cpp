#include "dsp/radio_channel.h"

#include <cmath>

#include "dsp/biquad.h"
#include "dsp/dsp.h"

namespace ember {
namespace dsp {

namespace {

// Schroeder reverb: 4 parallel combs + 2 series all-passes, ~300 ms decay.
class Reverb {
 public:
  Reverb(double fs, double t60_s) {
    const double comb_ms[4] = {29.7, 37.1, 41.1, 43.7};
    for (int k = 0; k < 4; ++k) {
      size_t d = static_cast<size_t>(comb_ms[k] * fs / 1000.0);
      comb_[k].assign(d ? d : 1, 0.0f);
      g_[k] = static_cast<float>(std::pow(10.0, -3.0 * (comb_ms[k] / 1000.0) / t60_s));
    }
    const double ap_ms[2] = {5.0, 1.7};
    for (int k = 0; k < 2; ++k) {
      size_t d = static_cast<size_t>(ap_ms[k] * fs / 1000.0);
      ap_[k].assign(d ? d : 1, 0.0f);
    }
  }
  float process(float x) {
    float s = 0;
    for (int k = 0; k < 4; ++k) {
      std::vector<float>& b = comb_[k];
      float y = b[ci_[k]];
      b[ci_[k]] = x + g_[k] * y;
      ci_[k] = (ci_[k] + 1) % b.size();
      s += y;
    }
    s *= 0.25f;
    for (int k = 0; k < 2; ++k) {
      std::vector<float>& b = ap_[k];
      float d = b[ai_[k]];
      float y = -0.7f * s + d;
      b[ai_[k]] = s + 0.7f * y;
      ai_[k] = (ai_[k] + 1) % b.size();
      s = y;
    }
    return s;
  }

 private:
  std::vector<float> comb_[4], ap_[2];
  float g_[4] = {0, 0, 0, 0};
  size_t ci_[4] = {0, 0, 0, 0}, ai_[2] = {0, 0};
};

// Speaker -> air -> microphone: band-limit, reverb, background noise (low-passed white).
void acoustic_pass(std::vector<float>& x, const RadioPathParams& p, double fs, Prng& rng) {
  Biquad h1 = Biquad::highpass(p.acoustic_lo_hz, fs), h2 = Biquad::highpass(p.acoustic_lo_hz, fs);
  double hi = p.acoustic_hi_hz < 0.45 * fs ? p.acoustic_hi_hz : 0.45 * fs;
  Biquad l1 = Biquad::lowpass(hi, fs), l2 = Biquad::lowpass(hi, fs);
  Biquad nl = Biquad::lowpass(1500, fs);
  Reverb rv(fs, 0.3);
  // White noise low-passed at 1.5 kHz keeps ~ sqrt(1500/(fs/2)) of its RMS.
  double nscale = p.room_noise / std::sqrt(1500.0 / (fs / 2));
  for (float& v : x) {
    float y = l2.process(l1.process(h2.process(h1.process(v))));
    if (p.reverb_mix > 0) y += static_cast<float>(p.reverb_mix) * rv.process(y);
    y += nl.process(static_cast<float>(nscale * rng.gauss()));
    v = y;
  }
}

}  // namespace

RadioPath::RadioPath(const RadioPathParams& p, double fs) : p_(p), fs_(fs > 0 ? fs : 8000) {}

std::vector<float> RadioPath::run(const std::vector<float>& tx, const std::vector<uint8_t>& ptt, size_t trail) {
  st_ = RadioPathStats();
  Prng rng(p_.seed);
  const size_t n = tx.size() + trail;
  const double ms_per = 1000.0 / fs_;

  // 1. device -> radio microphone
  std::vector<float> mic(n, 0.0f);
  for (size_t i = 0; i < tx.size(); ++i) mic[i] = tx[i];
  if (p_.acoustic) acoustic_pass(mic, p_, fs_, rng);

  // 2. keying
  std::vector<uint8_t> keyed(n, 0);
  if (p_.keying == Keying::Vox) {
    double env = 0, above_ms = 0, below_ms = 0;
    double att = 1.0 - std::exp(-1.0 / (0.005 * fs_)), rel = 1.0 - std::exp(-1.0 / (0.05 * fs_));
    bool k = false;
    for (size_t i = 0; i < n; ++i) {
      double a = std::fabs(mic[i]) * p_.mic_gain;
      env += (a > env ? att : rel) * (a - env);
      bool above = env > p_.vox_threshold;
      if (!k) {
        above_ms = above ? above_ms + ms_per : 0;
        if (above_ms >= p_.vox_attack_ms) { k = true; below_ms = 0; }
      } else {
        below_ms = above ? 0 : below_ms + ms_per;
        if (below_ms >= p_.vox_hang_ms) { k = false; above_ms = 0; }
      }
      keyed[i] = k;
    }
  } else {
    size_t on_d = static_cast<size_t>(p_.ptt_delay_ms * fs_ / 1000.0);
    size_t off_d = p_.keying == Keying::Manual ? static_cast<size_t>(p_.ptt_release_ms * fs_ / 1000.0) : on_d;
    // Key on `on_d` after the line goes up, off `off_d` after it goes down.
    bool k = false;
    size_t since = 0;
    bool line_prev = false;
    for (size_t i = 0; i < n; ++i) {
      bool line = i < ptt.size() && ptt[i];
      if (line != line_prev) { since = 0; line_prev = line; }
      ++since;
      if (line && !k && since > on_d) k = true;
      if (!line && k && since > off_d) k = false;
      keyed[i] = k;
    }
  }
  for (size_t i = 0; i < n; ++i) {
    if (keyed[i] && (i == 0 || !keyed[i - 1])) {
      ++st_.key_ups;
      if (st_.first_key_ms < 0) st_.first_key_ms = static_cast<double>(i) * ms_per;
    }
    if (keyed[i]) st_.keyed_ms += ms_per;
  }

  // 3. transmitter audio chain
  const double tau = 1.0 / (kTwoPi * 300.0);
  Biquad pre = Biquad::preemphasis(tau, fs_, 1.0 / (kTwoPi * 6000.0));
  Biquad sl1 = Biquad::lowpass(3000, fs_), sl2 = Biquad::lowpass(3000, fs_);
  std::vector<float> dev(n, 0.0f);
  for (size_t i = 0; i < n; ++i) {
    float a = static_cast<float>(mic[i] * p_.mic_gain);
    if (p_.preemphasis) a = pre.process(a) * 0.35f;  // pre-emphasis gain ~ +9 dB at 1.2 kHz; keep mid-band level
    float lim = static_cast<float>(p_.limiter);
    if (a > lim) a = lim;
    if (a < -lim) a = -lim;
    a = sl2.process(sl1.process(a));
    dev[i] = keyed[i] ? a : 0.0f;
  }

  // 4. receiver
  const double noise_var = 0.5 / std::pow(10.0, p_.fm_snr_db / 10.0) * (fs_ / 2) / 3000.0;
  const double nsd = std::sqrt(noise_var);
  const double tail_sd = 0.5 * std::sqrt((fs_ / 2) / 3000.0);  // full-quieting loss: loud hiss
  Biquad de = Biquad::deemphasis(tau, fs_);
  Biquad h1 = Biquad::highpass(300, fs_), h2 = Biquad::highpass(300, fs_);
  Biquad l1 = Biquad::lowpass(3000, fs_), l2 = Biquad::lowpass(3000, fs_);
  double carrier_ms = 0, tail_left_ms = 0;
  bool sq_open = false;
  double env = 0, gain = 1;
  double agc_att = 1.0 - std::exp(-1.0 / (0.05 * fs_)), agc_rel = 1.0 - std::exp(-1.0 / (1.0 * fs_));
  std::vector<float> rx(n, 0.0f);
  for (size_t i = 0; i < n; ++i) {
    float v = 0;
    if (keyed[i]) {
      carrier_ms += ms_per;
      if (!sq_open && carrier_ms >= p_.squelch_open_ms) sq_open = true;
      if (sq_open) v = dev[i] + static_cast<float>(nsd * rng.gauss());
    } else {
      if (carrier_ms > 0 && sq_open) tail_left_ms = p_.squelch_tail_ms;
      carrier_ms = 0;
      sq_open = false;
      if (tail_left_ms > 0) {
        v = static_cast<float>(tail_sd * rng.gauss());
        tail_left_ms -= ms_per;
      }
    }
    if (p_.deemphasis) v = de.process(v) * 2.8f;  // undo the mid-band pre-emphasis scaling
    v = l2.process(l1.process(h2.process(h1.process(v))));
    if (p_.rx_agc) {
      double a = std::fabs(v);
      env += (a > env ? agc_att : agc_rel) * (a - env);
      gain = env > 1e-4 ? 0.3 / env : gain;
      if (gain > 10) gain = 10;
      if (gain < 0.1) gain = 0.1;
      v = static_cast<float>(v * gain);
    }
    v = static_cast<float>(v * p_.rx_gain);
    rx[i] = v;
  }

  // 5. radio speaker -> device microphone
  if (p_.acoustic) acoustic_pass(rx, p_, fs_, rng);
  for (float& v : rx) v = v > 1 ? 1 : (v < -1 ? -1 : v);
  return rx;
}

}  // namespace dsp
}  // namespace ember
