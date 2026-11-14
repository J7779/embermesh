// EmberMesh store — INSECURE stand-in cipher for tests. See store/test_cipher.h.
#include "store/test_cipher.h"

namespace ember::store {
namespace {

uint64_t mix(uint64_t z) {
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

// Absorbs bytes into a 64-bit state (length is mixed in by callers to avoid ambiguity).
uint64_t absorb(uint64_t h, ByteView v) {
  for (size_t i = 0; i < v.size(); i += 8) {
    uint64_t w = 0;
    for (size_t j = 0; j < 8 && i + j < v.size(); ++j) w |= uint64_t(v[i + j]) << (8 * j);
    h = mix(h ^ w) + 0x9E3779B97F4A7C15ull;
  }
  return mix(h ^ v.size());
}

}  // namespace

InsecureTestCipher::InsecureTestCipher(ByteView key) {
  k0_ = absorb(0x243F6A8885A308D3ull, key);
  k1_ = absorb(0x13198A2E03707344ull, key);
}

void InsecureTestCipher::keystream(ByteView nonce, ByteView in, uint8_t* out) const {
  uint64_t base = absorb(k0_ ^ 0xA5A5A5A5ull, nonce);
  for (size_t i = 0; i < in.size(); i += 8) {
    uint64_t ks = mix(base ^ mix(i / 8 + 1));
    for (size_t j = 0; j < 8 && i + j < in.size(); ++j)
      out[i + j] = static_cast<uint8_t>(in[i + j] ^ static_cast<uint8_t>(ks >> (8 * j)));
  }
}

void InsecureTestCipher::tag(ByteView nonce, ByteView aad, ByteView ct, uint8_t out[kTagSize]) const {
  uint64_t a = absorb(absorb(absorb(k1_, nonce), aad), ct);
  uint64_t b = absorb(absorb(absorb(k0_ ^ k1_, ct), aad), nonce);
  for (int i = 0; i < 8; ++i) {
    out[i] = static_cast<uint8_t>(a >> (8 * i));
    out[8 + i] = static_cast<uint8_t>(b >> (8 * i));
  }
}

bool InsecureTestCipher::seal(ByteView nonce, ByteView aad, ByteView plaintext, Bytes* out) {
  ++seals_;
  if (nonce.size() != kNonceSize) return false;
  out->resize(plaintext.size() + kTagSize);
  keystream(nonce, plaintext, out->data());
  tag(nonce, aad, ByteView(out->data(), plaintext.size()), out->data() + plaintext.size());
  return true;
}

bool InsecureTestCipher::open(ByteView nonce, ByteView aad, ByteView ciphertext, Bytes* out) {
  ++opens_;
  if (nonce.size() != kNonceSize || ciphertext.size() < kTagSize) return false;
  size_t n = ciphertext.size() - kTagSize;
  uint8_t t[kTagSize];
  tag(nonce, aad, ciphertext.sub(0, n), t);
  if (!ct_equal(ByteView(t, kTagSize), ciphertext.sub(n))) return false;
  out->resize(n);
  keystream(nonce, ciphertext.sub(0, n), out->data());
  return true;
}

}  // namespace ember::store
