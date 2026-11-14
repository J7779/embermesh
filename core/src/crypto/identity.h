// EmberMesh core — node identity.
//
// An identity is a single 32-byte master seed from which two keypairs are derived:
//   sign seed  = BLAKE2b-256(key = seed, "ember-sign-v1")  -> Ed25519 keypair
//   dh secret  = BLAKE2b-256(key = seed, "ember-dh-v1")    -> X25519 keypair
//   node_id    = first 8 bytes of BLAKE2b-256("ember-node-id-v1" || sign_pk || dh_pk)
// so a backup only has to protect 32 bytes. Humans compare an 8-word fingerprint
// (BLAKE2b-256("ember-fp-v1" || sign_pk || dh_pk), one word per byte from a built-in list of 256
// distinct, easy to pronounce English words) read aloud over a voice channel, or scan the
// `ember:card?...` QR code.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "crypto/crypto.h"
#include "crypto/rng.h"
#include "ember/bytes.h"

namespace ember {

using NodeId = std::array<uint8_t, 8>;

std::string node_id_hex(const NodeId& id);
// Exactly 16 hex chars (any case). Returns false otherwise.
bool parse_node_id(std::string_view hex, NodeId& out);
inline uint64_t node_id_u64(const NodeId& id) {
  uint64_t v = 0;
  for (uint8_t b : id) v = v << 8 | b;
  return v;
}
inline NodeId node_id_from_u64(uint64_t v) {
  NodeId id;
  for (int i = 7; i >= 0; --i) { id[static_cast<size_t>(i)] = static_cast<uint8_t>(v); v >>= 8; }
  return id;
}
struct NodeIdHash {
  size_t operator()(const NodeId& id) const { return static_cast<size_t>(node_id_u64(id) * 0x9E3779B97F4A7C15ull >> 16); }
};

struct PublicKeys {
  crypto::Key32 sign_pk{};
  crypto::Key32 dh_pk{};
  NodeId node_id() const;
  bool operator==(const PublicKeys& o) const { return sign_pk == o.sign_pk && dh_pk == o.dh_pk; }
  bool operator!=(const PublicKeys& o) const { return !(*this == o); }
};

// 256-entry word list used for fingerprints (index = byte value).
const char* fingerprint_word(uint8_t index);
// Reverse lookup (case-insensitive); returns -1 if not a list word.
int fingerprint_word_index(std::string_view word);
// 6 words for a set of public keys.
std::vector<std::string> fingerprint_words(const PublicKeys& keys, size_t count = 8);  // 8 words = 64 bits (review L1)

// Contact card: everything needed to message someone, shareable as text / QR.
//   ember:card?id=<16 hex>&k=<base64url(sign_pk || dh_pk)>&n=<name>&cs=<callsign>
struct ContactCard {
  PublicKeys keys;
  std::string name;      // <= 64 bytes UTF-8
  std::string callsign;  // <= 16 chars, A-Z 0-9 / -
  NodeId node_id() const { return keys.node_id(); }
  std::string to_uri() const;
  // Verifies that id matches the keys. Unknown params are ignored.
  static bool parse(std::string_view uri, ContactCard& out, std::string* error = nullptr);
};

class Identity {
 public:
  Identity();  // empty (valid() == false)
  ~Identity();
  Identity(const Identity& o);
  Identity& operator=(const Identity& o);

  static Identity generate(crypto::Csprng& rng);
  static Identity from_seed(const crypto::Key32& seed);

  bool valid() const { return valid_; }
  const NodeId& node_id() const { return node_id_; }
  const PublicKeys& public_keys() const { return pub_; }
  const crypto::Key32& sign_pk() const { return pub_.sign_pk; }
  const crypto::Key32& dh_pk() const { return pub_.dh_pk; }
  std::vector<std::string> fingerprint() const { return fingerprint_words(pub_); }

  void sign(ByteView msg, uint8_t sig[64]) const;
  crypto::Sig64 sign(ByteView msg) const;
  // Raw X25519 with a peer's dh public key. False for low-order peer keys.
  bool dh(const crypto::Key32& their_dh_pk, crypto::Key32& shared) const;

  // Secret serialization for local storage: 0x01 || seed (33 bytes). Wipe the result after use.
  Bytes serialize_secret() const;
  static bool deserialize_secret(ByteView data, Identity& out);

  // Passphrase-encrypted backup text "ember:backup?v=1&d=<base64url>" that can also carry opaque
  // `extra` bytes (e.g. the profile as JSON).
  //   blob = 0x01 | varint mem_kib | u8 passes | salt[16] | nonce[24] | AEAD(seed || extra) | mac
  std::string export_backup(std::string_view passphrase, crypto::Csprng& rng,
                            ByteView extra = ByteView(),
                            const crypto::KdfParams& params = crypto::KdfParams()) const;
  // max_mem_kib caps the Argon2 memory a (possibly hostile) backup can demand.
  static bool import_backup(std::string_view text, std::string_view passphrase, Identity& out,
                            Bytes* extra = nullptr, std::string* error = nullptr,
                            uint32_t max_mem_kib = 64 * 1024);

  ContactCard card(std::string_view name, std::string_view callsign = "") const;

 private:
  void derive();
  void wipe_all();

  bool valid_ = false;
  crypto::Key32 seed_{};
  crypto::SignSecret sign_sk_{};
  crypto::Key32 dh_sk_{};
  PublicKeys pub_;
  NodeId node_id_{};
};

}  // namespace ember
