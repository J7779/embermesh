#include "dsp/agc.h"

#include <cmath>

namespace ember {
namespace dsp {

float ema_coeff(double tc, double fs) {
  if (!(tc > 0) || !(fs > 0)) return 1.0f;
  return static_cast<float>(1.0 - std::exp(-1.0 / (tc * fs)));
}

void Agc::init(double fs, double attack_s, double release_s, float target, float max_gain, float min_gain) {
  att_ = ema_coeff(attack_s, fs);
  rel_ = ema_coeff(release_s, fs);
  target_ = target;
  max_gain_ = max_gain;
  min_gain_ = min_gain;
  env_ = 0;
}

float Agc::gain() const {
  // env_ tracks |x|; for a sine |x| averages 2/pi * A, RMS is A/sqrt2 -> rms ~ 1.11 * mean|x|.
  float rms = env_ * 1.1107f;
  if (rms <= target_ / max_gain_) return max_gain_;
  float g = target_ / rms;
  return g < min_gain_ ? min_gain_ : (g > max_gain_ ? max_gain_ : g);
}

float Agc::process(float x) {
  float a = std::fabs(x);
  if (!(a < 1e30f)) a = 0;  // NaN / Inf guard (an infinite envelope would never decay)
  env_ += (a > env_ ? att_ : rel_) * (a - env_);
  return x * gain();
}

void DcBlocker::init(double fs, double corner_hz) {
  double r = 1.0 - kTwoPi * corner_hz / (fs > 0 ? fs : 8000);
  if (!(r >= 0.5)) r = 0.5;  // also NaN
  if (r > 0.99999) r = 0.99999;
  r_ = static_cast<float>(r);
  reset();
}

}  // namespace dsp
}  // namespace ember
