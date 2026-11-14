// EmberMesh store — INSECURE stand-in cipher for tests.
//
// Behaves like an AEAD (keystream XOR + 16-byte keyed tag over nonce, AAD and ciphertext; any
// change to key, nonce, AAD or ciphertext makes open() fail with overwhelming probability), but
// it is built from a non-cryptographic mixer. It exists so store tests — and tests of modules
// built on the store — can exercise encryption at rest without the crypto module.
// NEVER use it for real data.
#pragma once

#include <string_view>

#include "store/cipher.h"

namespace ember::store {

class InsecureTestCipher : public RecordCipher {
 public:
  explicit InsecureTestCipher(ByteView key);
  explicit InsecureTestCipher(std::string_view key) : InsecureTestCipher(ByteView(key)) {}

  size_t overhead() const override { return kTagSize; }
  bool seal(ByteView nonce, ByteView aad, ByteView plaintext, Bytes* out) override;
  bool open(ByteView nonce, ByteView aad, ByteView ciphertext, Bytes* out) override;

  // Number of seal()/open() calls so far (lets tests check nonce use / caching).
  uint64_t seals() const { return seals_; }
  uint64_t opens() const { return opens_; }

 private:
  static constexpr size_t kTagSize = 16;
  void keystream(ByteView nonce, ByteView in, uint8_t* out) const;
  void tag(ByteView nonce, ByteView aad, ByteView ct, uint8_t out[kTagSize]) const;
  uint64_t k0_, k1_;
  uint64_t seals_ = 0, opens_ = 0;
};

}  // namespace ember::store
