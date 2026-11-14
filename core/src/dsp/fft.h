// EmberMesh core / dsp — in-place radix-2 complex FFT.
#pragma once

#include <cstddef>
#include <vector>

#include "dsp/dsp.h"

namespace ember {
namespace dsp {

class Fft {
 public:
  Fft() = default;
  // n must be a power of two in [2, 65536]; returns false otherwise (object stays unusable).
  bool init(size_t n);
  size_t size() const { return n_; }
  // Forward transform X[k] = sum x[n] e^{-j 2 pi k n / N} (unscaled), in place. `data` has size() items.
  void forward(Cf* data) const;
  // Inverse transform, unscaled (divide by N yourself).
  void inverse(Cf* data) const;

 private:
  void run(Cf* data, bool inv) const;
  size_t n_ = 0;
  std::vector<Cf> tw_;          // e^{-j 2 pi k / N}, k < N/2
  std::vector<uint32_t> rev_;   // bit reversal permutation
};

}  // namespace dsp
}  // namespace ember
