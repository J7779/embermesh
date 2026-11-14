// EmberMesh core / dsp — numerically controlled oscillator.
//
// 32-bit phase accumulator + 1024-entry sine table with linear interpolation (spurs < -100 dBc).
// Phase continuous across frequency changes, so it is also the FSK/MFSK tone generator.
#pragma once

#include <cstdint>

#include "dsp/dsp.h"

namespace ember {
namespace dsp {

// sin(2*pi*phase/2^32) from the shared table (immutable after first use).
float sin_u32(uint32_t phase);
inline float cos_u32(uint32_t phase) { return sin_u32(phase + 0x40000000u); }

class Nco {
 public:
  Nco() = default;
  Nco(double freq_hz, double sample_rate) { set_freq(freq_hz, sample_rate); }

  // Sets the frequency (may be negative or above Nyquist; it wraps like any NCO).
  void set_freq(double freq_hz, double sample_rate);
  // Phase increment per sample, in cycles (freq/sample_rate).
  void set_cycles_per_sample(double cps);
  double cycles_per_sample() const;
  void set_phase(double cycles) { phase_ = cycles_to_u32(cycles); }
  double phase_cycles() const { return static_cast<double>(phase_) / 4294967296.0; }
  void reset() { phase_ = 0; }

  // Returns the current value then advances one sample.
  float next_sin() { float v = sin_u32(phase_); phase_ += inc_; return v; }
  float next_cos() { float v = cos_u32(phase_); phase_ += inc_; return v; }
  // e^{j*phase} (cos + j sin), then advance.
  Cf next() { Cf v{cos_u32(phase_), sin_u32(phase_)}; phase_ += inc_; return v; }
  // e^{-j*phase}: the down-conversion mixer, then advance.
  Cf next_conj() { Cf v{cos_u32(phase_), -sin_u32(phase_)}; phase_ += inc_; return v; }

  static uint32_t cycles_to_u32(double cycles);

 private:
  uint32_t phase_ = 0;
  uint32_t inc_ = 0;
};

}  // namespace dsp
}  // namespace ember
