// EmberMesh node — encryption at rest for the node's KvStore.
//
//   storage key  = Argon2id(passphrase, salt, KdfParams)            (32 bytes)
//   RecordCipher = XChaCha20-Poly1305 keyed with the storage key     (store/cipher.h)
//
// The store keeps an opaque public "cipher params" blob in its header (readable before unlock):
//
//   v1: u8 1 | u32be mem_kib | u8 passes | salt[16] | duress_salt[16] | duress_hash[32]
//   v2: u8 2 | u32be mem_kib | u8 passes | salt[16] | duress_salt[16] | duress_check[32] | decoy[184]
//
// D = Argon2id(duress passphrase, duress_salt, same KdfParams).
// v1: duress_hash = D.  v2: duress_check = BLAKE2b-256(key=D, "ember-duress-check-v2"), and
// decoy = nonce[24] || XChaCha20-Poly1305(BLAKE2b-256(key=D, "ember-decoy-v2"), nonce, "",
// presentation[144]) holds the non-secret profile / presentation settings that a duress unlock
// carries into the fresh identity (docs/RPC_API.md security.*). After a duress unlock D itself
// becomes the new storage key with salt = the old duress_salt, so a duress unlock costs exactly
// the same two Argon2id runs as a normal one. Without a duress passphrase the check and decoy
// slots hold random bytes, so the header never reveals whether one exists.
// KDF parameters from a header (or a backup) are capped (kMaxHeaderMemKib / kMaxHeaderPasses) so a
// hostile file cannot demand absurd work (security review L8).
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>

#include "crypto/crypto.h"
#include "crypto/rng.h"
#include "ember/bytes.h"
#include "store/cipher.h"

namespace ember::node {

class AeadRecordCipher : public store::RecordCipher {
 public:
  explicit AeadRecordCipher(const crypto::Key32& key) : key_(key) {}
  ~AeadRecordCipher() override { crypto::wipe(key_); }
  size_t overhead() const override { return crypto::kMacSize; }
  bool seal(ByteView nonce, ByteView aad, ByteView plaintext, Bytes* out) override;
  bool open(ByteView nonce, ByteView aad, ByteView ciphertext, Bytes* out) override;

 private:
  crypto::Key32 key_;
};

constexpr uint32_t kMaxHeaderMemKib = 64 * 1024;  // 64 MiB
constexpr uint32_t kMaxHeaderPasses = 8;
constexpr size_t kDecoySize = 184;
constexpr size_t kDecoyPlain = kDecoySize - 24 - crypto::kMacSize;  // 144

struct StorageParams {
  uint8_t version = 2;
  crypto::KdfParams kdf;
  std::array<uint8_t, 16> salt{};
  std::array<uint8_t, 16> duress_salt{};
  crypto::Key32 duress_hash{};  // v1: D; v2: check value
  std::array<uint8_t, kDecoySize> decoy{};

  static constexpr size_t kSize = 1 + 4 + 1 + 16 + 16 + 32;  // v1
  static constexpr size_t kSizeV2 = kSize + kDecoySize;
  Bytes encode() const;
  static bool decode(ByteView in, StorageParams* out);
  // Fresh salts; duress slot filled with random bytes (no duress passphrase).
  static StorageParams fresh(const crypto::KdfParams& kdf, crypto::Csprng& rng);
};

// Does D (Argon2id of a candidate passphrase with duress_salt) open the duress slot?
bool duress_matches(const StorageParams& p, const crypto::Key32& d);
// Sets the v2 duress slot for D with `presentation` (<= kDecoyPlain bytes) as the decoy.
void set_duress_slot(StorageParams& p, const crypto::Key32& d, ByteView presentation, crypto::Csprng& rng);
// Re-seals the decoy with a stored decoy key (when the profile changes).
void reseal_decoy(StorageParams& p, const crypto::Key32& decoy_key, ByteView presentation, crypto::Csprng& rng);
crypto::Key32 decoy_key_for(const crypto::Key32& d);
bool open_decoy(const StorageParams& p, const crypto::Key32& d, Bytes* presentation);

// Argon2id(passphrase, salt). False on KDF failure (allocation / bad params).
bool derive_storage_key(std::string_view passphrase, ByteView salt, const crypto::KdfParams& kdf, crypto::Key32* out);

}  // namespace ember::node
