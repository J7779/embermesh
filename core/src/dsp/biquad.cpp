#include "dsp/biquad.h"

#include <cmath>

namespace ember {
namespace dsp {

void Biquad::set(double b0, double b1, double b2, double a0, double a1, double a2) {
  if (!(std::fabs(a0) > 1e-30)) a0 = 1;
  b0_ = static_cast<float>(b0 / a0);
  b1_ = static_cast<float>(b1 / a0);
  b2_ = static_cast<float>(b2 / a0);
  a1_ = static_cast<float>(a1 / a0);
  a2_ = static_cast<float>(a2 / a0);
  reset();
}

double Biquad::gain(double f, double fs) const {
  double w = kTwoPi * f / fs;
  double c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2 * w), s2 = std::sin(2 * w);
  double nr = b0_ + b1_ * c1 + b2_ * c2, ni = -(b1_ * s1 + b2_ * s2);
  double dr = 1 + a1_ * c1 + a2_ * c2, di = -(a1_ * s1 + a2_ * s2);
  return std::sqrt((nr * nr + ni * ni) / (dr * dr + di * di));
}

namespace {
struct Rbj {
  double w0, cw, sw;
  Rbj(double f, double fs) {
    if (!(fs > 0)) fs = 8000;
    if (!(f > 0)) f = 1;
    if (f > 0.499 * fs) f = 0.499 * fs;
    w0 = kTwoPi * f / fs;
    cw = std::cos(w0);
    sw = std::sin(w0);
  }
};
double safe_q(double q) { return q > 1e-3 ? q : 1e-3; }
}  // namespace

Biquad Biquad::lowpass(double f, double fs, double q) {
  Rbj r(f, fs);
  double al = r.sw / (2 * safe_q(q));
  Biquad b;
  b.set((1 - r.cw) / 2, 1 - r.cw, (1 - r.cw) / 2, 1 + al, -2 * r.cw, 1 - al);
  return b;
}

Biquad Biquad::highpass(double f, double fs, double q) {
  Rbj r(f, fs);
  double al = r.sw / (2 * safe_q(q));
  Biquad b;
  b.set((1 + r.cw) / 2, -(1 + r.cw), (1 + r.cw) / 2, 1 + al, -2 * r.cw, 1 - al);
  return b;
}

Biquad Biquad::bandpass(double f, double fs, double q) {
  Rbj r(f, fs);
  double al = r.sw / (2 * safe_q(q));
  Biquad b;
  b.set(al, 0, -al, 1 + al, -2 * r.cw, 1 - al);
  return b;
}

Biquad Biquad::notch(double f, double fs, double q) {
  Rbj r(f, fs);
  double al = r.sw / (2 * safe_q(q));
  Biquad b;
  b.set(1, -2 * r.cw, 1, 1 + al, -2 * r.cw, 1 - al);
  return b;
}

Biquad Biquad::peaking(double f, double fs, double q, double gain_db) {
  Rbj r(f, fs);
  double A = std::pow(10.0, gain_db / 40), al = r.sw / (2 * safe_q(q));
  Biquad b;
  b.set(1 + al * A, -2 * r.cw, 1 - al * A, 1 + al / A, -2 * r.cw, 1 - al / A);
  return b;
}

Biquad Biquad::lowshelf(double f, double fs, double gain_db, double slope) {
  Rbj r(f, fs);
  double A = std::pow(10.0, gain_db / 40);
  double al = r.sw / 2 * std::sqrt((A + 1 / A) * (1 / slope - 1) + 2);
  double sa = 2 * std::sqrt(A) * al;
  Biquad b;
  b.set(A * ((A + 1) - (A - 1) * r.cw + sa), 2 * A * ((A - 1) - (A + 1) * r.cw), A * ((A + 1) - (A - 1) * r.cw - sa),
        (A + 1) + (A - 1) * r.cw + sa, -2 * ((A - 1) + (A + 1) * r.cw), (A + 1) + (A - 1) * r.cw - sa);
  return b;
}

Biquad Biquad::highshelf(double f, double fs, double gain_db, double slope) {
  Rbj r(f, fs);
  double A = std::pow(10.0, gain_db / 40);
  double al = r.sw / 2 * std::sqrt((A + 1 / A) * (1 / slope - 1) + 2);
  double sa = 2 * std::sqrt(A) * al;
  Biquad b;
  b.set(A * ((A + 1) + (A - 1) * r.cw + sa), -2 * A * ((A - 1) + (A + 1) * r.cw), A * ((A + 1) + (A - 1) * r.cw - sa),
        (A + 1) - (A - 1) * r.cw + sa, 2 * ((A - 1) - (A + 1) * r.cw), (A + 1) - (A - 1) * r.cw - sa);
  return b;
}

namespace {
// Bilinear transform of H(s) = (n1 s + n0) / (d1 s + d0) with K = 2 fs (no warping: the
// time constants describe the analog networks in radios, and fs >> corner here).
Biquad first_order(double n1, double n0, double d1, double d0, double fs) {
  double K = 2 * fs;
  Biquad b;
  b.set(n1 * K + n0, n0 - n1 * K, 0, d1 * K + d0, d0 - d1 * K, 0);
  return b;
}
Biquad normalise_1k(Biquad b, double fs, double n1, double n0, double d1, double d0) {
  double g = b.gain(1000.0, fs);
  if (!(g > 1e-9)) return b;
  return first_order(n1 / g, n0 / g, d1, d0, fs);
}
}  // namespace

Biquad Biquad::deemphasis(double tau, double fs) {
  if (!(tau > 0) || !(fs > 0)) return Biquad();
  Biquad b = first_order(0, 1, tau, 1, fs);
  return normalise_1k(b, fs, 0, 1, tau, 1);
}

Biquad Biquad::preemphasis(double tau, double fs, double tau_hf) {
  if (!(tau > 0) || !(fs > 0)) return Biquad();
  if (!(tau_hf > 0)) tau_hf = 1.0 / (kTwoPi * 0.45 * fs);  // flatten just below Nyquist
  Biquad b = first_order(tau, 1, tau_hf, 1, fs);
  return normalise_1k(b, fs, tau, 1, tau_hf, 1);
}

}  // namespace dsp
}  // namespace ember
