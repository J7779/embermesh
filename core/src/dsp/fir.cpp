#include "dsp/fir.h"

#include <cmath>

namespace ember {
namespace dsp {

double bessel_i0(double x) {
  double sum = 1.0, term = 1.0, q = x * x / 4.0;
  for (int k = 1; k < 60; ++k) {
    term *= q / (static_cast<double>(k) * k);
    sum += term;
    if (term < sum * 1e-12) break;
  }
  return sum;
}

double kaiser_beta(double a) {
  if (a > 50) return 0.1102 * (a - 8.7);
  if (a >= 21) return 0.5842 * std::pow(a - 21, 0.4) + 0.07886 * (a - 21);
  return 0.0;
}

size_t kaiser_taps(double atten_db, double transition_hz, double fs, size_t max_taps) {
  if (!(fs > 0) || !(transition_hz > 0)) return max_taps | 1;
  double dw = kTwoPi * transition_hz / fs;
  double n = (atten_db - 7.95) / (2.285 * dw) + 1;
  if (!(n > 3)) n = 3;
  if (n > static_cast<double>(max_taps)) n = static_cast<double>(max_taps);
  size_t t = static_cast<size_t>(std::ceil(n));
  return t | 1;
}

double window_value(Window w, size_t i, size_t n, double beta) {
  if (n <= 1) return 1.0;
  double x = static_cast<double>(i) / static_cast<double>(n - 1);  // 0..1
  switch (w) {
    case Window::Rect: return 1.0;
    case Window::Hann: return 0.5 - 0.5 * std::cos(kTwoPi * x);
    case Window::Hamming: return 0.54 - 0.46 * std::cos(kTwoPi * x);
    case Window::Blackman: return 0.42 - 0.5 * std::cos(kTwoPi * x) + 0.08 * std::cos(2 * kTwoPi * x);
    case Window::Kaiser: {
      double r = 2 * x - 1;
      double v = 1 - r * r;
      return bessel_i0(beta * std::sqrt(v < 0 ? 0 : v)) / bessel_i0(beta);
    }
  }
  return 1.0;
}

namespace {
double sinc_pi(double x) { return std::fabs(x) < 1e-12 ? 1.0 : std::sin(kPi * x) / (kPi * x); }

// Ideal low-pass impulse response with normalised cutoff fc (cycles/sample), windowed.
std::vector<double> lp_raw(size_t taps, double fc, Window w, double beta) {
  std::vector<double> h(taps);
  double mid = (static_cast<double>(taps) - 1) / 2;
  for (size_t i = 0; i < taps; ++i) {
    double t = static_cast<double>(i) - mid;
    h[i] = 2 * fc * sinc_pi(2 * fc * t) * window_value(w, i, taps, beta);
  }
  return h;
}

std::vector<float> to_float(const std::vector<double>& h) {
  std::vector<float> out(h.size());
  for (size_t i = 0; i < h.size(); ++i) out[i] = static_cast<float>(h[i]);
  return out;
}

bool bad(size_t taps, double fs) { return taps == 0 || taps > 65535 || !(fs > 0); }
}  // namespace

std::vector<float> fir_lowpass(size_t taps, double cutoff_hz, double fs, Window w, double beta) {
  if (bad(taps, fs)) return {1.0f};
  double fc = cutoff_hz / fs;
  if (!(fc > 0)) fc = 1e-6;
  if (fc > 0.5) fc = 0.5;
  std::vector<double> h = lp_raw(taps, fc, w, beta);
  double sum = 0;
  for (double v : h) sum += v;
  if (std::fabs(sum) > 1e-12) for (double& v : h) v /= sum;
  return to_float(h);
}

std::vector<float> fir_highpass(size_t taps, double cutoff_hz, double fs, Window w, double beta) {
  if (bad(taps, fs)) return {1.0f};
  taps |= 1;  // type I required for a high-pass
  std::vector<float> lp = fir_lowpass(taps, cutoff_hz, fs, w, beta);
  std::vector<float> h(taps);
  for (size_t i = 0; i < taps; ++i) h[i] = -lp[i];
  h[(taps - 1) / 2] += 1.0f;
  return h;
}

std::vector<float> fir_bandpass(size_t taps, double lo_hz, double hi_hz, double fs, Window w, double beta) {
  if (bad(taps, fs)) return {1.0f};
  if (lo_hz > hi_hz) { double t = lo_hz; lo_hz = hi_hz; hi_hz = t; }
  double f1 = lo_hz / fs, f2 = hi_hz / fs;
  if (f1 < 0) f1 = 0;
  if (f2 > 0.5) f2 = 0.5;
  std::vector<double> a = lp_raw(taps, f2, w, beta), b = lp_raw(taps, f1, w, beta);
  std::vector<double> h(taps);
  for (size_t i = 0; i < taps; ++i) h[i] = a[i] - b[i];
  // Normalise to unity at the band centre.
  double fcen = (f1 + f2) / 2, re = 0, im = 0;
  for (size_t i = 0; i < taps; ++i) {
    re += h[i] * std::cos(kTwoPi * fcen * i);
    im -= h[i] * std::sin(kTwoPi * fcen * i);
  }
  double g = std::sqrt(re * re + im * im);
  if (g > 1e-12) for (double& v : h) v /= g;
  return to_float(h);
}

std::vector<float> fir_hilbert(size_t taps, Window w) {
  if (taps == 0 || taps > 65535) return {0.0f};
  taps |= 1;
  std::vector<float> h(taps);
  long mid = static_cast<long>(taps - 1) / 2;
  for (size_t i = 0; i < taps; ++i) {
    long n = static_cast<long>(i) - mid;
    double v = 0;
    if (n % 2 != 0) v = 2.0 / (kPi * static_cast<double>(n));
    h[i] = static_cast<float>(v * window_value(w, i, taps));
  }
  return h;
}

std::vector<float> fir_rrc(size_t sps, size_t span, double alpha) {
  if (sps == 0 || span == 0 || sps * span > 65534) return {1.0f};
  if (alpha < 0.001) alpha = 0.001;
  if (alpha > 1) alpha = 1;
  size_t n = sps * span + 1;
  std::vector<double> h(n);
  double mid = static_cast<double>(n - 1) / 2;
  for (size_t i = 0; i < n; ++i) {
    double t = (static_cast<double>(i) - mid) / static_cast<double>(sps);
    double v;
    if (std::fabs(t) < 1e-9) {
      v = 1.0 - alpha + 4 * alpha / kPi;
    } else if (std::fabs(std::fabs(4 * alpha * t) - 1.0) < 1e-9) {
      v = alpha / std::sqrt(2.0) * ((1 + 2 / kPi) * std::sin(kPi / (4 * alpha)) + (1 - 2 / kPi) * std::cos(kPi / (4 * alpha)));
    } else {
      v = (std::sin(kPi * t * (1 - alpha)) + 4 * alpha * t * std::cos(kPi * t * (1 + alpha))) /
          (kPi * t * (1 - (4 * alpha * t) * (4 * alpha * t)));
    }
    h[i] = v;
  }
  double e = 0;
  for (double v : h) e += v * v;
  if (e > 0) for (double& v : h) v /= std::sqrt(e);
  return to_float(h);
}

double fir_gain(const std::vector<float>& taps, double f_hz, double fs) {
  double re = 0, im = 0;
  for (size_t i = 0; i < taps.size(); ++i) {
    re += taps[i] * std::cos(kTwoPi * f_hz / fs * i);
    im -= taps[i] * std::sin(kTwoPi * f_hz / fs * i);
  }
  return std::sqrt(re * re + im * im);
}

// ---- Fir ----------------------------------------------------------------------------------------

void Fir::set_taps(std::vector<float> taps) {
  if (taps.empty()) taps.push_back(1.0f);
  taps_.assign(taps.rbegin(), taps.rend());
  buf_.assign(taps_.size() * 2, 0.0f);
  pos_ = 0;
}

void Fir::reset() {
  for (float& v : buf_) v = 0;
  pos_ = 0;
}

void Fir::push(float x) {
  size_t n = taps_.size();
  if (!n) return;
  buf_[pos_] = x;
  buf_[pos_ + n] = x;
  pos_ = (pos_ + 1 == n) ? 0 : pos_ + 1;
}

float Fir::output() const {
  size_t n = taps_.size();
  if (!n) return 0;
  // Oldest sample is at pos_, newest at pos_ + n - 1. taps_ is reversed so taps_[n-1] = h[0]
  // multiplies the newest sample.
  const float* x = &buf_[pos_];
  const float* h = taps_.data();
  float acc0 = 0, acc1 = 0;
  size_t i = 0;
  for (; i + 1 < n; i += 2) { acc0 += x[i] * h[i]; acc1 += x[i + 1] * h[i + 1]; }
  if (i < n) acc0 += x[i] * h[i];
  return acc0 + acc1;
}

float Fir::process(float x) { push(x); return output(); }

void Fir::process(const float* in, float* out, size_t n) {
  for (size_t i = 0; i < n; ++i) out[i] = process(in[i]);
}

// ---- FirC ---------------------------------------------------------------------------------------

void FirC::set_taps(std::vector<float> taps) {
  if (taps.empty()) taps.push_back(1.0f);
  taps_.assign(taps.rbegin(), taps.rend());
  re_.assign(taps_.size() * 2, 0.0f);
  im_.assign(taps_.size() * 2, 0.0f);
  pos_ = 0;
}

void FirC::reset() {
  for (float& v : re_) v = 0;
  for (float& v : im_) v = 0;
  pos_ = 0;
}

void FirC::push(Cf x) {
  size_t n = taps_.size();
  if (!n) return;
  re_[pos_] = re_[pos_ + n] = x.re;
  im_[pos_] = im_[pos_ + n] = x.im;
  pos_ = (pos_ + 1 == n) ? 0 : pos_ + 1;
}

Cf FirC::output() const {
  size_t n = taps_.size();
  if (!n) return {};
  const float* xr = &re_[pos_];
  const float* xi = &im_[pos_];
  const float* h = taps_.data();
  float ar = 0, ai = 0;
  for (size_t i = 0; i < n; ++i) { ar += xr[i] * h[i]; ai += xi[i] * h[i]; }
  return {ar, ai};
}

// ---- decimators ---------------------------------------------------------------------------------

void FirDecimC::init(std::vector<float> taps, size_t factor) {
  fir_.set_taps(std::move(taps));
  factor_ = factor ? factor : 1;
  phase_ = 0;
}
void FirDecimC::reset() { fir_.reset(); phase_ = 0; }
bool FirDecimC::push(Cf x, Cf* out) {
  fir_.push(x);
  if (++phase_ < factor_) return false;
  phase_ = 0;
  *out = fir_.output();
  return true;
}

void FirDecim::init(std::vector<float> taps, size_t factor) {
  fir_.set_taps(std::move(taps));
  factor_ = factor ? factor : 1;
  phase_ = 0;
}
void FirDecim::reset() { fir_.reset(); phase_ = 0; }
bool FirDecim::push(float x, float* out) {
  fir_.push(x);
  if (++phase_ < factor_) return false;
  phase_ = 0;
  *out = fir_.output();
  return true;
}

}  // namespace dsp
}  // namespace ember
