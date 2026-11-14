#include "dsp/resampler.h"

#include <cmath>

#include "dsp/dsp.h"
#include "dsp/fir.h"

namespace ember {
namespace dsp {

bool Resampler::init(double in_rate, double out_rate, int zeros, double cutoff_frac) {
  ready_ = false;
  table_.clear();
  buf_.clear();
  if (!(in_rate > 0) || !(out_rate > 0) || in_rate > 1e7 || out_rate > 1e7) return false;
  double ratio = in_rate / out_rate;
  if (ratio > 64 || ratio < 1.0 / 64) return false;
  if (zeros < 4) zeros = 4;
  if (zeros > 32) zeros = 32;
  if (!(cutoff_frac >= 0.1)) cutoff_frac = 0.1;
  if (cutoff_frac > 0.5) cutoff_frac = 0.5;
  in_rate_ = in_rate;
  out_rate_ = out_rate;
  base_step_ = step_ = ratio;
  passthrough_ = (in_rate == out_rate);

  double fmin = in_rate < out_rate ? in_rate : out_rate;
  double fc = cutoff_frac * fmin / in_rate;  // cycles per input sample
  double w = zeros / (2 * fc);               // half width in input samples
  half_ = static_cast<size_t>(std::ceil(w));
  taps_ = 2 * half_;
  // Phase count scales with the kernel bandwidth so the linear phase interpolation error stays
  // around -80 dB while the table stays small.
  size_t p = static_cast<size_t>(std::ceil(128.0 * fc / 0.45));
  phases_ = p < 16 ? 16 : (p > 256 ? 256 : p);
  double beta = 8.0;
  double i0b = bessel_i0(beta);
  table_.resize((phases_ + 1) * taps_);
  for (size_t ph = 0; ph <= phases_; ++ph) {
    double f = static_cast<double>(ph) / static_cast<double>(phases_);
    for (size_t j = 0; j < taps_; ++j) {
      // tap j multiplies x[i - K0 + 1 + j]; kernel argument = f - (j - K0 + 1)
      double x = f - (static_cast<double>(j) - static_cast<double>(half_) + 1.0);
      double v = 0;
      if (std::fabs(x) < w) {
        double a = 2 * fc * x;
        double s = std::fabs(a) < 1e-12 ? 1.0 : std::sin(kPi * a) / (kPi * a);
        double r = x / w;
        double win = bessel_i0(beta * std::sqrt(1 - r * r)) / i0b;
        v = 2 * fc * s * win;
      }
      table_[ph * taps_ + j] = static_cast<float>(v);
    }
  }
  ready_ = true;
  reset();
  return true;
}

void Resampler::set_step_scale(double scale) {
  if (!(scale > 0.5 && scale < 2.0)) return;
  step_ = base_step_ * scale;
  passthrough_ = (in_rate_ == out_rate_) && scale == 1.0;
}

void Resampler::reset() {
  buf_.assign(half_ > 0 ? half_ - 1 : 0, 0.0f);
  t_ = static_cast<double>(buf_.size());
}

void Resampler::process(const float* in, size_t n, std::vector<float>& out) {
  if (!ready_ || !in || !n) return;
  if (passthrough_) {
    out.insert(out.end(), in, in + n);
    return;
  }
  buf_.insert(buf_.end(), in, in + n);
  const size_t sz = buf_.size();
  const float* tab = table_.data();
  const float P = static_cast<float>(phases_);
  while (true) {
    double fi = std::floor(t_);
    size_t i = static_cast<size_t>(fi);
    if (i + half_ >= sz) break;
    float frac = static_cast<float>(t_ - fi) * P;
    size_t ph = static_cast<size_t>(frac);
    if (ph >= phases_) ph = phases_ - 1;
    float mu = frac - static_cast<float>(ph);
    const float* c0 = tab + ph * taps_;
    const float* c1 = c0 + taps_;
    const float* x = &buf_[i + 1 - half_];
    float a0 = 0, a1 = 0;
    for (size_t j = 0; j < taps_; ++j) {
      a0 += x[j] * c0[j];
      a1 += x[j] * c1[j];
    }
    out.push_back(a0 + (a1 - a0) * mu);
    t_ += step_;
  }
  // Drop samples no longer needed by any future output.
  double fi = std::floor(t_);
  size_t keep_from = 0;
  if (fi + 1 > static_cast<double>(half_)) keep_from = static_cast<size_t>(fi) + 1 - half_;
  if (keep_from > sz) keep_from = sz;
  if (keep_from > 0) {
    buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(keep_from));
    t_ -= static_cast<double>(keep_from);
  }
}

void Resampler::flush(std::vector<float>& out) {
  if (!ready_ || passthrough_) return;
  std::vector<float> z(half_ + 2, 0.0f);
  process(z.data(), z.size(), out);
}

}  // namespace dsp
}  // namespace ember
