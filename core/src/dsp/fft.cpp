#include "dsp/fft.h"

namespace ember {
namespace dsp {

bool Fft::init(size_t n) {
  n_ = 0;
  tw_.clear();
  rev_.clear();
  if (n < 2 || n > 65536 || (n & (n - 1)) != 0) return false;
  n_ = n;
  tw_.resize(n / 2);
  for (size_t k = 0; k < n / 2; ++k) tw_[k] = expj(-kTwoPi * static_cast<double>(k) / static_cast<double>(n));
  rev_.resize(n);
  int bits = 0;
  while ((static_cast<size_t>(1) << bits) < n) ++bits;
  for (size_t i = 0; i < n; ++i) {
    uint32_t r = 0;
    for (int b = 0; b < bits; ++b) if (i & (static_cast<size_t>(1) << b)) r |= 1u << (bits - 1 - b);
    rev_[i] = r;
  }
  return true;
}

void Fft::forward(Cf* data) const { run(data, false); }
void Fft::inverse(Cf* data) const { run(data, true); }

void Fft::run(Cf* d, bool inv) const {
  if (!n_ || !d) return;
  for (size_t i = 0; i < n_; ++i) {
    size_t r = rev_[i];
    if (r > i) { Cf t = d[i]; d[i] = d[r]; d[r] = t; }
  }
  for (size_t len = 2; len <= n_; len <<= 1) {
    size_t half = len >> 1, step = n_ / len;
    for (size_t i = 0; i < n_; i += len) {
      for (size_t k = 0; k < half; ++k) {
        Cf w = tw_[k * step];
        if (inv) w.im = -w.im;
        Cf a = d[i + k];
        Cf b = d[i + k + half] * w;
        d[i + k] = a + b;
        d[i + k + half] = a - b;
      }
    }
  }
}

}  // namespace dsp
}  // namespace ember
