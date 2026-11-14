// EmberMesh core / dsp — IIR sections: RBJ-cookbook biquads and first-order shelves used for
// FM pre-/de-emphasis.
#pragma once

#include "dsp/dsp.h"

namespace ember {
namespace dsp {

// Direct-form-II-transposed biquad: y = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2).
class Biquad {
 public:
  Biquad() = default;
  void set(double b0, double b1, double b2, double a0, double a1, double a2);
  void reset() { z1_ = z2_ = 0; }
  float process(float x) {
    float y = b0_ * x + z1_;
    z1_ = b1_ * x - a1_ * y + z2_;
    z2_ = b2_ * x - a2_ * y;
    // Self-heal: a NaN/Inf input (or a blown-up state) would otherwise poison the recursive state
    // forever. The comparison is false for NaN, so this one branch covers NaN, Inf and overflow.
    if (!(z1_ < 1e30f && z1_ > -1e30f && z2_ < 1e30f && z2_ > -1e30f)) {
      reset();
      return 0.0f;
    }
    return y;
  }
  // Magnitude response at f (diagnostics / tests).
  double gain(double f_hz, double fs) const;

  // RBJ cookbook designs. q = 0.7071 gives Butterworth for LP/HP.
  static Biquad lowpass(double f_hz, double fs, double q = 0.70710678);
  static Biquad highpass(double f_hz, double fs, double q = 0.70710678);
  static Biquad bandpass(double f_hz, double fs, double q);  // 0 dB peak gain
  static Biquad notch(double f_hz, double fs, double q);
  static Biquad peaking(double f_hz, double fs, double q, double gain_db);
  static Biquad lowshelf(double f_hz, double fs, double gain_db, double slope = 1.0);
  static Biquad highshelf(double f_hz, double fs, double gain_db, double slope = 1.0);
  // First-order sections (b2 = a2 = 0), bilinear transform with pre-warping.
  //  deemphasis: H(s) = 1 / (1 + s*tau)                       (-6 dB/oct above 1/(2 pi tau))
  //  preemphasis: H(s) = (1 + s*tau) / (1 + s*tau_hf)          (+6 dB/oct, flattened above 1/(2 pi tau_hf))
  // Both are normalised to unity gain at 1 kHz so tilt tests do not also change the level.
  static Biquad deemphasis(double tau_s, double fs);
  static Biquad preemphasis(double tau_s, double fs, double tau_hf_s = 0);

 private:
  float b0_ = 1, b1_ = 0, b2_ = 0, a1_ = 0, a2_ = 0;
  float z1_ = 0, z2_ = 0;
};

}  // namespace dsp
}  // namespace ember
