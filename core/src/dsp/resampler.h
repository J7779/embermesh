// EmberMesh core / dsp — arbitrary-ratio streaming resampler.
//
// Kaiser-windowed sinc interpolation evaluated from a polyphase table with linear interpolation
// between phases, so any rational or irrational ratio works (8000 <-> 11025 <-> 44100 <-> 48000 ...)
// and the ratio can be nudged while running (sample-clock drift simulation / tracking).
//
// Quality with the defaults (12 zero crossings per side, cutoff 0.45 x min(rate)): pass-band flat to
// < 0.05 dB up to 0.4 x min(rate), stop-band and imaging < -70 dB. Cost per output sample is
// 2*ceil(12 / (2*cutoff/in_rate)) multiply-adds: ~28 when up-sampling, ~110 for 48k -> 12k.
// Latency: ~12 / (2*cutoff) input samples.
#pragma once

#include <cstddef>
#include <vector>

namespace ember {
namespace dsp {

class Resampler {
 public:
  Resampler() = default;
  // Returns false for non-positive / absurd rates (> 1:64 ratio). zeros: sinc zero crossings per
  // side (4..32). cutoff_frac: anti-alias cutoff as a fraction of min(in, out) (0.1..0.5).
  bool init(double in_rate, double out_rate, int zeros = 12, double cutoff_frac = 0.45);
  bool ready() const { return ready_; }
  bool passthrough() const { return passthrough_; }
  double in_rate() const { return in_rate_; }
  double out_rate() const { return out_rate_; }
  // Multiplies the input step per output sample by `scale` (1 + ppm*1e-6 simulates an output clock
  // that is slow by ppm). Disables the pass-through fast path when != 1.
  void set_step_scale(double scale);
  // Consumes n input samples, appends the produced output samples to `out`.
  void process(const float* in, size_t n, std::vector<float>& out);
  // Appends enough zeros to push all buffered input through (end of stream). Use before reset().
  void flush(std::vector<float>& out);
  void reset();
  // Approximate group delay in input samples.
  size_t latency_in() const { return half_; }

 private:
  bool ready_ = false, passthrough_ = false;
  double in_rate_ = 0, out_rate_ = 0, step_ = 1, base_step_ = 1;
  size_t half_ = 0;     // K0: taps per side
  size_t taps_ = 0;     // 2 * K0
  size_t phases_ = 0;   // P
  std::vector<float> table_;  // (P + 1) x taps_
  std::vector<float> buf_;
  double t_ = 0;
};

}  // namespace dsp
}  // namespace ember
