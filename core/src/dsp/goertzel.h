// EmberMesh core / dsp — Goertzel single-frequency detector (works at any, non-bin-centred frequency).
#pragma once

#include <cstddef>

#include "dsp/dsp.h"

namespace ember {
namespace dsp {

class Goertzel {
 public:
  Goertzel() = default;
  Goertzel(double freq_hz, double fs) { set_freq(freq_hz, fs); }
  void set_freq(double freq_hz, double fs);
  void reset() { s1_ = s2_ = 0; n_ = 0; }
  void push(float x) {
    float s0 = x + coeff_ * s1_ - s2_;
    s2_ = s1_;
    s1_ = s0;
    ++n_;
  }
  size_t count() const { return n_; }
  // |X(f)|^2 of the samples pushed since reset (unnormalised: a full-scale sine of amplitude A over
  // N samples gives (A*N/2)^2).
  float power() const { return s1_ * s1_ + s2_ * s2_ - coeff_ * s1_ * s2_; }
  // Power normalised so that a sine of amplitude A gives A^2/2 (i.e. mean-square of the tone).
  float tone_power() const;

 private:
  float coeff_ = 0;
  float s1_ = 0, s2_ = 0;
  size_t n_ = 0;
};

}  // namespace dsp
}  // namespace ember
