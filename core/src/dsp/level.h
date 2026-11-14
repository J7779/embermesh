// EmberMesh core / dsp — signal level metering and band-power SNR estimation.
#pragma once

#include <cstddef>
#include <vector>

#include "dsp/fft.h"

namespace ember {
namespace dsp {

// Exponentially averaged RMS + decaying peak meter, plus clip counting. Feed raw input samples.
class LevelMeter {
 public:
  LevelMeter() = default;
  void init(double fs, double rms_time_s = 0.3, double peak_decay_s = 1.0, float clip_level = 0.99f);
  void push(float x);
  void push(const float* x, size_t n) { for (size_t i = 0; i < n; ++i) push(x[i]); }
  float rms() const;
  float rms_dbfs() const;      // dB relative to a full-scale sine (0 dBFS sine RMS = 0.7071)
  float peak() const { return peak_; }
  float peak_dbfs() const;
  // Fraction of recent samples at/above the clip level (EMA over rms_time).
  float clip_fraction() const { return clip_; }
  void reset() { ms_ = 0; peak_ = 0; clip_ = 0; }

 private:
  float a_ = 0.001f, pdec_ = 0.9999f, clip_level_ = 0.99f;
  float ms_ = 0, peak_ = 0, clip_ = 0;
};

// Estimates SNR of a signal occupying [sig_lo, sig_hi] Hz from averaged power spectra, using the
// noise density measured in [noise_lo, noise_hi] (a band where the signal has no energy).
// Result is SNR referred to `ref_bw_hz` (e.g. 3000 Hz, matching ChannelSim's definition):
//   snr = (P_sig_band - N0 * B_sig) / (N0 * ref_bw).
class BandSnrEstimator {
 public:
  BandSnrEstimator() = default;
  // fft_size: power of two. avg_frames: number of spectra in the running average (EMA).
  bool init(double fs, double sig_lo, double sig_hi, double noise_lo, double noise_hi,
            double ref_bw_hz = 3000.0, size_t fft_size = 256, double avg_frames = 16);
  void push(const float* x, size_t n);
  // Returns false until at least one spectrum has been accumulated.
  bool snr_db(float* out) const;
  void reset();

 private:
  Fft fft_;
  std::vector<float> win_, buf_;
  std::vector<Cf> work_;
  size_t fill_ = 0;
  double fs_ = 8000, ref_bw_ = 3000, alpha_ = 0.1;
  size_t s0_ = 0, s1_ = 0, n0_ = 0, n1_ = 0;
  double psig_ = 0, pnoise_ = 0;
  size_t frames_ = 0;
};

}  // namespace dsp
}  // namespace ember
