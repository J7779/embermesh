#include "dsp/goertzel.h"

#include <cmath>

namespace ember {
namespace dsp {

void Goertzel::set_freq(double f, double fs) {
  coeff_ = (fs > 0) ? static_cast<float>(2.0 * std::cos(kTwoPi * f / fs)) : 0.0f;
  reset();
}

float Goertzel::tone_power() const {
  if (!n_) return 0;
  float n = static_cast<float>(n_);
  return 2.0f * power() / (n * n);
}

}  // namespace dsp
}  // namespace ember
