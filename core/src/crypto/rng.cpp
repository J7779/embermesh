#include "crypto/rng.h"

#include <array>
#include <cstring>

#include "crypto/crypto.h"
#include "monocypher/monocypher.h"

namespace ember::crypto {

Csprng::Csprng() { std::memset(key_, 0, sizeof key_); }
Csprng::~Csprng() { crypto_wipe(key_, sizeof key_); }

void Csprng::seed(ByteView entropy) {
  Blake2b h(32, ByteView(key_, 32));
  uint8_t ctr[8];
  for (int i = 0; i < 8; ++i) ctr[i] = static_cast<uint8_t>(reseed_ctr_ >> (8 * i));
  h.update(std::string_view("ember-rng-seed")).update(ctr, 8).update(entropy);
  h.final(key_);
  ++reseed_ctr_;
  seeded_bytes_ += entropy.size();
}

void Csprng::fill(uint8_t* out, size_t n) {
  // Each fill uses a distinct nonce; block 0 becomes the next key, output starts at block 1.
  uint8_t nonce[8];
  for (int i = 0; i < 8; ++i) nonce[i] = static_cast<uint8_t>(stream_ctr_ >> (8 * i));
  ++stream_ctr_;
  uint8_t block0[64];
  crypto_chacha20_djb(block0, nullptr, sizeof block0, key_, nonce, 0);
  if (n && out) crypto_chacha20_djb(out, nullptr, n, key_, nonce, 1);
  std::memcpy(key_, block0, 32);
  crypto_wipe(block0, sizeof block0);
}

Bytes Csprng::bytes(size_t n) {
  Bytes b(n);
  fill(b.data(), n);
  return b;
}

uint32_t Csprng::u32() {
  uint8_t b[4];
  fill(b, 4);
  return uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24;
}

uint64_t Csprng::u64() { return uint64_t(u32()) << 32 | u32(); }

uint32_t Csprng::uniform(uint32_t n) {
  if (n <= 1) return 0;
  uint32_t limit = static_cast<uint32_t>(-n) % n;  // 2^32 mod n
  for (;;) {
    uint32_t r = u32();
    if (r >= limit) return r % n;
  }
}

}  // namespace ember::crypto
