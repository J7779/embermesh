// Deterministic PRNG for fec tests (SplitMix64). Never seeded from the OS.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ember/bytes.h"

namespace fec_test {

struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed) {}
  uint64_t next() {
    uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
  // Uniform in [0, n) (n > 0).
  size_t below(size_t n) { return static_cast<size_t>(next() % n); }
  uint8_t byte() { return static_cast<uint8_t>(next()); }
  double uniform() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
  ember::Bytes bytes(size_t n) {
    ember::Bytes b(n);
    for (auto& x : b) x = byte();
    return b;
  }
};

}  // namespace fec_test
