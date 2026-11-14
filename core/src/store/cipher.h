// EmberMesh store — encryption-at-rest plug-in interface.
//
// The store does not depend on the crypto module. To encrypt a store, the crypto module (or the
// node) implements RecordCipher with an AEAD — XChaCha20-Poly1305 is the intended one, keyed from
// the user's passphrase via Argon2 — and hands it to KvStore::Options::cipher.
//
// What the store guarantees when a cipher is installed:
//   * Every record payload (namespace names, keys, values, deletes, snapshot chunks) is sealed;
//     no key or value byte is written to disk in the clear.
//   * Every seal() gets a fresh 24-byte nonce: 16 bytes drawn from Options::entropy once per
//     open/compaction ("session") + a 64-bit little-endian counter. Nonces never repeat under one
//     key as long as the injected entropy is not repeated, even when a crash loses unsynced data
//     and sequence numbers are reused.
//   * The AAD binds each sealed blob to its file (random per-file salt), generation, frame type and
//     sequence number, so frames cannot be moved between files or positions undetected.
//   * The file header holds a key-check blob (a sealed constant) so a wrong key is reported as
//     Status::WrongKey instead of as corruption.
//
// The cipher MUST be an AEAD: open() must reject any ciphertext, nonce or AAD that was not produced
// by seal() with the same key. A cipher that does not authenticate breaks the store's "never return
// corrupt data" guarantee for encrypted stores.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ember/bytes.h"

namespace ember::store {

class RecordCipher {
 public:
  static constexpr size_t kNonceSize = 24;
  // Ciphers with more overhead are rejected (header key-check slot is fixed-size).
  static constexpr size_t kMaxOverhead = 64;

  virtual ~RecordCipher() = default;

  // Bytes added by seal(): ciphertext.size() == plaintext.size() + overhead(). Typically the tag.
  virtual size_t overhead() const = 0;

  // Encrypts and authenticates. `nonce` is exactly kNonceSize bytes and unique per call.
  // Writes the ciphertext (plaintext.size() + overhead() bytes) to *out. False on failure.
  virtual bool seal(ByteView nonce, ByteView aad, ByteView plaintext, Bytes* out) = 0;

  // Verifies and decrypts. Returns false (and leaves *out unspecified) if authentication fails.
  virtual bool open(ByteView nonce, ByteView aad, ByteView ciphertext, Bytes* out) = 0;
};

}  // namespace ember::store
