// EmberMesh core / dsp — automatic gain control, DC removal and envelope trackers.
#pragma once

#include "dsp/dsp.h"

namespace ember {
namespace dsp {

// Feed-forward-ish AGC on the signal envelope: fast attack, slow release. Output targets an RMS of
// `target`. Gain is limited to [min_gain, max_gain] so silence is not amplified into noise forever.
class Agc {
 public:
  Agc() = default;
  // attack_s / release_s are time constants for the envelope estimate.
  void init(double fs, double attack_s = 0.005, double release_s = 0.5, float target = 0.25f,
            float max_gain = 1e4f, float min_gain = 1e-2f);
  void reset() { env_ = 0; }
  float process(float x);
  float gain() const;
  float envelope() const { return env_; }

 private:
  float att_ = 0.1f, rel_ = 0.001f, target_ = 0.25f, max_gain_ = 1e4f, min_gain_ = 1e-2f;
  float env_ = 0;
};

// One-pole DC blocker: y[n] = x[n] - x[n-1] + r*y[n-1], corner ~ (1-r)*fs/(2 pi).
class DcBlocker {
 public:
  DcBlocker() = default;
  void init(double fs, double corner_hz = 20.0);
  void reset() { x1_ = y1_ = 0; }
  float process(float x) {
    if (!(x < 1e30f && x > -1e30f)) x = 0;  // NaN / Inf would stick in the recursion forever
    float y = x - x1_ + r_ * y1_;
    if (!(y < 1e30f && y > -1e30f)) y = 0;
    x1_ = x;
    y1_ = y;
    return y;
  }

 private:
  float r_ = 0.995f, x1_ = 0, y1_ = 0;
};

// Peak / valley tracker with fast attack and slow decay towards the input (the "ATC" used by FSK
// demodulators to normalise each tone's envelope independently, which cancels audio tilt).
class PeakValley {
 public:
  PeakValley() = default;
  // attack, decay: per-sample smoothing factors in (0, 1].
  void init(float attack, float decay) { att_ = attack; dec_ = decay; peak_ = 0; valley_ = 0; }
  void reset() { peak_ = valley_ = 0; }
  void push(float x) {
    peak_ += (x >= peak_ ? att_ : dec_) * (x - peak_);
    valley_ += (x <= valley_ ? att_ : dec_) * (x - valley_);
  }
  float peak() const { return peak_; }
  float valley() const { return valley_; }
  // x mapped so that valley -> -0.5, peak -> +0.5.
  float normalise(float x) const {
    float r = peak_ - valley_;
    return r > 1e-20f ? (x - 0.5f * (peak_ + valley_)) / r : 0.0f;
  }

 private:
  float att_ = 0.5f, dec_ = 0.001f, peak_ = 0, valley_ = 0;
};

// Per-sample exponential smoothing coefficient for a time constant.
float ema_coeff(double time_constant_s, double fs);

}  // namespace dsp
}  // namespace ember
