// EmberMesh core — deterministic CSPRNG (ChaCha20 with fast key erasure).
//
// The core never reads OS entropy: the host passes entropy to seed() (ember_create) and may mix
// more in at any time. Output is a pure function of the seed history, which makes simulations and
// tests reproducible. After every fill() the internal key is replaced by fresh keystream, so a
// later state compromise does not reveal earlier outputs.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ember/bytes.h"

namespace ember::crypto {

class Csprng {
 public:
  Csprng();
  explicit Csprng(ByteView seed_material) : Csprng() { seed(seed_material); }
  ~Csprng();
  Csprng(const Csprng&) = delete;
  Csprng& operator=(const Csprng&) = delete;

  // Mixes entropy into the state: key = BLAKE2b(key || counter || entropy). Any length.
  void seed(ByteView entropy);
  // True once at least 32 bytes of entropy have been mixed in (total over all seed() calls).
  bool seeded() const { return seeded_bytes_ >= 32; }

  void fill(uint8_t* out, size_t n);
  template <size_t N>
  void fill(std::array<uint8_t, N>& a) { fill(a.data(), N); }
  Bytes bytes(size_t n);
  uint32_t u32();
  uint64_t u64();
  // Uniform in [0, n) without modulo bias; returns 0 for n == 0.
  uint32_t uniform(uint32_t n);

 private:
  uint8_t key_[32];
  uint64_t reseed_ctr_ = 0;
  uint64_t stream_ctr_ = 0;
  size_t seeded_bytes_ = 0;
};

}  // namespace ember::crypto
