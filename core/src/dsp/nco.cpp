#include "dsp/nco.h"

#include <cmath>

namespace ember {
namespace dsp {

namespace {
constexpr int kTableBits = 10;
constexpr int kTableSize = 1 << kTableBits;

struct SineTable {
  float v[kTableSize + 1];
  SineTable() {
    for (int i = 0; i <= kTableSize; ++i) v[i] = static_cast<float>(std::sin(kTwoPi * i / kTableSize));
  }
};

const SineTable& table() {
  static const SineTable t;  // immutable after construction
  return t;
}
}  // namespace

float sin_u32(uint32_t phase) {
  const SineTable& t = table();
  uint32_t idx = phase >> (32 - kTableBits);
  float frac = static_cast<float>((phase >> (32 - kTableBits - 16)) & 0xFFFF) * (1.0f / 65536.0f);
  float a = t.v[idx], b = t.v[idx + 1];
  return a + (b - a) * frac;
}

uint32_t Nco::cycles_to_u32(double cycles) {
  double f = cycles - std::floor(cycles);  // [0,1)
  double v = f * 4294967296.0;
  if (!(v >= 0) || v >= 4294967295.0) return 0;  // NaN / rounding guard
  return static_cast<uint32_t>(v);
}

void Nco::set_freq(double freq_hz, double sample_rate) {
  if (!(sample_rate > 0)) { inc_ = 0; return; }
  set_cycles_per_sample(freq_hz / sample_rate);
}

void Nco::set_cycles_per_sample(double cps) {
  if (!std::isfinite(cps)) { inc_ = 0; return; }
  inc_ = cycles_to_u32(cps);
}

double Nco::cycles_per_sample() const {
  double c = static_cast<double>(inc_) / 4294967296.0;
  return c >= 0.5 ? c - 1.0 : c;
}

}  // namespace dsp
}  // namespace ember
