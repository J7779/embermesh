#include "dsp/level.h"

#include <cmath>

#include "dsp/agc.h"
#include "dsp/fir.h"

namespace ember {
namespace dsp {

void LevelMeter::init(double fs, double rms_time_s, double peak_decay_s, float clip_level) {
  a_ = ema_coeff(rms_time_s, fs);
  pdec_ = 1.0f - ema_coeff(peak_decay_s, fs);
  clip_level_ = clip_level;
  reset();
}

void LevelMeter::push(float x) {
  if (!(x == x)) x = 0;
  float ax = std::fabs(x);
  ms_ += a_ * (x * x - ms_);
  peak_ = ax > peak_ ? ax : peak_ * pdec_;
  clip_ += a_ * ((ax >= clip_level_ ? 1.0f : 0.0f) - clip_);
}

float LevelMeter::rms() const { return std::sqrt(ms_ > 0 ? ms_ : 0); }
float LevelMeter::rms_dbfs() const { return lin_to_db(rms() / 0.70710678f); }
float LevelMeter::peak_dbfs() const { return lin_to_db(peak_); }

bool BandSnrEstimator::init(double fs, double sig_lo, double sig_hi, double noise_lo, double noise_hi,
                            double ref_bw_hz, size_t fft_size, double avg_frames) {
  if (!fft_.init(fft_size) || !(fs > 0)) return false;
  fs_ = fs;
  ref_bw_ = ref_bw_hz > 0 ? ref_bw_hz : 3000;
  alpha_ = avg_frames > 1 ? 1.0 / avg_frames : 1.0;
  win_.resize(fft_size);
  double wsum2 = 0;
  for (size_t i = 0; i < fft_size; ++i) {
    win_[i] = static_cast<float>(window_value(Window::Hann, i, fft_size));
    wsum2 += win_[i] * win_[i];
  }
  // Normalise so that sum of |X|^2 over bins equals the mean-square of the input (Parseval).
  float g = static_cast<float>(1.0 / std::sqrt(wsum2 * fft_size));
  for (float& w : win_) w *= g;
  buf_.assign(fft_size, 0.0f);
  work_.resize(fft_size);
  double bin = fs / fft_size;
  auto clampbin = [&](double f) {
    long b = std::lround(f / bin);
    if (b < 1) b = 1;
    if (b > static_cast<long>(fft_size / 2) - 1) b = static_cast<long>(fft_size / 2) - 1;
    return static_cast<size_t>(b);
  };
  s0_ = clampbin(sig_lo);
  s1_ = clampbin(sig_hi);
  n0_ = clampbin(noise_lo);
  n1_ = clampbin(noise_hi);
  if (s1_ < s0_) s1_ = s0_;
  if (n1_ < n0_) n1_ = n0_;
  reset();
  return true;
}

void BandSnrEstimator::reset() {
  fill_ = 0;
  psig_ = pnoise_ = 0;
  frames_ = 0;
}

void BandSnrEstimator::push(const float* x, size_t n) {
  size_t N = fft_.size();
  if (!N) return;
  for (size_t i = 0; i < n; ++i) {
    buf_[fill_++] = x[i];
    if (fill_ < N) continue;
    fill_ = 0;
    for (size_t k = 0; k < N; ++k) work_[k] = Cf(buf_[k] * win_[k], 0);
    fft_.forward(work_.data());
    double ps = 0, pn = 0;
    for (size_t k = s0_; k <= s1_; ++k) ps += 2 * norm2(work_[k]);
    for (size_t k = n0_; k <= n1_; ++k) pn += 2 * norm2(work_[k]);
    pn /= static_cast<double>(n1_ - n0_ + 1);  // per-bin noise power
    double a = frames_ == 0 ? 1.0 : alpha_;
    psig_ += a * (ps - psig_);
    pnoise_ += a * (pn - pnoise_);
    ++frames_;
  }
}

bool BandSnrEstimator::snr_db(float* out) const {
  if (!frames_ || !out) return false;
  double bin = fs_ / static_cast<double>(fft_.size());
  double nbins = static_cast<double>(s1_ - s0_ + 1);
  double n0 = pnoise_ / bin;  // noise density
  double s = psig_ - pnoise_ * nbins;
  if (s < 1e-20) s = 1e-20;
  double nref = n0 * ref_bw_;
  if (nref < 1e-30) { *out = 99.0f; return true; }
  *out = static_cast<float>(10.0 * std::log10(s / nref));
  return true;
}

}  // namespace dsp
}  // namespace ember
