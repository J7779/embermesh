// EmberMesh core / dsp — radio / audio channel simulator for tests and offline evaluation.
//
// Models what happens between one device's sound output and another device's sound input:
//   tx audio --> tilt (FM pre-/de-emphasis) --> band-limit (radio audio filter)
//            --> frequency offset (SSB mistuning; via a wideband IIR Hilbert pair)
//            --> sample-clock drift (resampling by 1 + ppm*1e-6)
//            --> slow fading (sinusoidal QSB) --> + AWGN at a stated SNR
//            --> receive gain --> clipping --> optional PCM16 quantisation
//
// SNR definition (used throughout the modem tests and docs): signal power / noise power measured
// in a 3000 Hz reference bandwidth (`noise_ref_bw_hz`), with the signal power measured at the
// point where noise is added. White noise is generated over the full band fs/2, so its total power
// is SNR-referenced power * (fs/2)/3000. This makes numbers comparable across sample rates.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "dsp/biquad.h"
#include "dsp/dsp.h"
#include "dsp/fir.h"

namespace ember {
namespace dsp {

// Wideband 90-degree phase-difference network (two 4-section all-pass chains, Olli Niemitalo's
// coefficients). Outputs (i, q) 90 +- 0.7 degrees apart (q leads i) over ~1%..99% of the band,
// i.e. ~ -44 dB image rejection when used as a frequency shifter.
class HilbertIir {
 public:
  HilbertIir();
  void reset();
  void process(float x, float* i_out, float* q_out);

 private:
  struct Ap {
    float a = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    float run(float x) {
      float y = a * (x + y2) - x2;
      x2 = x1; x1 = x; y2 = y1; y1 = y;
      return y;
    }
  };
  Ap p1_[4], p2_[4];
  float d1_ = 0;  // one-sample delay on path 1
};

// Frequency shifter for real audio (single-sideband style: every component moves by +hz).
class FreqShifter {
 public:
  void init(double hz, double fs);
  float process(float x);

 private:
  HilbertIir h_;
  double phase_ = 0, inc_ = 0;
};

struct ChannelParams {
  uint64_t seed = 1;
  double snr_db = 200;            // >= 150 means no noise
  double noise_ref_bw_hz = 3000;  // see header comment
  double freq_offset_hz = 0;      // shifts every frequency (as SSB mistuning does)
  double clock_ppm = 0;           // receiver sample clock fast (+) / slow (-) relative to sender
  double gain_db = 0;             // applied after noise (receive level), so SNR is unaffected
  double fade_depth_db = 0;       // sinusoidal fading depth (peak to trough), before noise
  double fade_hz = 0.5;
  // FM-radio style tilt. tilt = -1: de-emphasis only (as when a pre-emphasised-path radio receives
  // flat audio: highs drop ~6 dB/octave above tilt_corner_hz). +1: pre-emphasis only.
  int tilt = 0;
  double tilt_corner_hz = 300;
  bool bandlimit = false;         // 4th-order 300..3000 Hz band-pass like a voice radio
  double bandlimit_lo_hz = 300, bandlimit_hi_hz = 3000;
  double clip_level = 0;          // > 0: hard clip to +/- clip_level after gain
  bool quantize_pcm16 = false;    // round to 16-bit PCM after clipping
  double dc_offset = 0;           // added after gain
};

class ChannelSim {
 public:
  ChannelSim(const ChannelParams& p, double fs);
  // Passes the whole transmission `tx` through the channel, surrounded by `lead` and `trail`
  // samples of silence (which still receive noise). The signal power used for the SNR is measured
  // on `tx` after the linear stages. Returns the received samples (length differs from the input
  // by the clock drift).
  std::vector<float> run(const std::vector<float>& tx, size_t lead = 0, size_t trail = 0);
  // Noise power per sample that was used by the last run() (0 if none).
  double noise_power() const { return noise_var_; }
  double signal_power() const { return sig_pow_; }

 private:
  ChannelParams p_;
  double fs_;
  Prng rng_;
  double noise_var_ = 0, sig_pow_ = 0;
};

}  // namespace dsp
}  // namespace ember
