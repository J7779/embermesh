// EmberMesh core — thin, misuse-resistant wrappers around vendored Monocypher 4.0.2.
//
//   hashing   BLAKE2b (1..64 byte output, optional key), SHA-512
//   AEAD      XChaCha20-Poly1305 (RFC 8439 construction with a 24-byte nonce; libsodium
//             crypto_aead_xchacha20poly1305_ietf compatible)
//   DH        X25519 (RFC 7748), rejects all-zero (low-order) shared secrets
//   signing   Ed25519 (RFC 8032, SHA-512)
//   KDF       Argon2id with explicit, bounded parameters
//   misc      wipe(), constant-time compare
//
// Nothing here allocates except argon2id (work area, bounded by KdfParams limits) and the
// Bytes-returning convenience functions.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ember/bytes.h"

namespace ember::crypto {

constexpr size_t kKeySize = 32;
constexpr size_t kNonceSize = 24;
constexpr size_t kMacSize = 16;
constexpr size_t kSigSize = 64;
constexpr size_t kSignSecretSize = 64;  // Ed25519 secret key = seed || public key

using Key32 = std::array<uint8_t, 32>;
using Nonce24 = std::array<uint8_t, 24>;
using Mac16 = std::array<uint8_t, 16>;
using Sig64 = std::array<uint8_t, 64>;
using SignSecret = std::array<uint8_t, 64>;

// Securely erase memory (not optimised away).
void wipe(void* p, size_t n);
template <size_t N>
inline void wipe(std::array<uint8_t, N>& a) { wipe(a.data(), N); }
inline void wipe(Bytes& b) { if (!b.empty()) wipe(b.data(), b.size()); }

// --- hashing -------------------------------------------------------------------------------------
// out_len must be 1..64; key (optional) at most 64 bytes. Out-of-range sizes are clamped.
void blake2b(uint8_t* out, size_t out_len, ByteView msg, ByteView key = ByteView());
Key32 blake2b_256(ByteView msg, ByteView key = ByteView());

class Blake2b {
 public:
  explicit Blake2b(size_t out_len = 32, ByteView key = ByteView());
  ~Blake2b();
  Blake2b(const Blake2b&) = delete;
  Blake2b& operator=(const Blake2b&) = delete;
  Blake2b& update(ByteView data);
  Blake2b& update(const void* p, size_t n) { return update(ByteView(static_cast<const uint8_t*>(p), n)); }
  Blake2b& update(std::string_view s) { return update(ByteView(s)); }
  Blake2b& u8(uint8_t v) { return update(&v, 1); }
  // Length-prefixed (u16be) field — unambiguous concatenation of variable-size inputs.
  Blake2b& field(ByteView data);
  void final(uint8_t* out);  // writes out_len bytes; object must not be reused
  Key32 final32();           // only valid when constructed with out_len == 32

 private:
  alignas(8) uint8_t ctx_[256];
  size_t out_len_;
};

void sha512(uint8_t out[64], ByteView msg);

// --- AEAD ----------------------------------------------------------------------------------------
// ct and pt may alias exactly (in-place) or not overlap at all.
void aead_seal(uint8_t* ct, uint8_t mac[16], const Key32& key, const uint8_t nonce[24], ByteView ad,
               ByteView pt);
// Returns false (and zero-fills pt) if authentication fails.
bool aead_open(uint8_t* pt, const uint8_t mac[16], const Key32& key, const uint8_t nonce[24],
               ByteView ad, ByteView ct);
// Convenience: returns ciphertext || mac.
Bytes seal(const Key32& key, const uint8_t nonce[24], ByteView ad, ByteView pt);
// Input is ciphertext || mac. Returns false on any failure (too short, bad mac); out is cleared.
bool open(const Key32& key, const uint8_t nonce[24], ByteView ad, ByteView ct_mac, Bytes& out);

// --- X25519 --------------------------------------------------------------------------------------
void x25519_public(Key32& pub, const Key32& secret);
// Raw shared secret. Returns false for an all-zero result (peer sent a low-order point).
bool x25519(Key32& shared, const Key32& secret, const Key32& their_public);

// --- Ed25519 -------------------------------------------------------------------------------------
void ed25519_keypair(const Key32& seed, SignSecret& secret, Key32& pub);
void ed25519_sign(uint8_t sig[64], const SignSecret& secret, ByteView msg);
bool ed25519_verify(const uint8_t sig[64], const Key32& pub, ByteView msg);

// --- Argon2id --------------------------------------------------------------------------------------
struct KdfParams {
  // Defaults chosen to finish in well under a second on a 2015 phone and in WASM, while still
  // costing an attacker 16 MiB per guess.
  uint32_t mem_kib = 16384;  // 1 KiB blocks; 8 .. kMaxMemKib
  uint32_t passes = 3;       // 1 .. kMaxPasses
  static constexpr uint32_t kMinMemKib = 8;
  static constexpr uint32_t kMaxMemKib = 256 * 1024;  // 256 MiB hard cap (hostile backups)
  static constexpr uint32_t kMaxPasses = 32;
  bool valid() const {
    return mem_kib >= kMinMemKib && mem_kib <= kMaxMemKib && passes >= 1 && passes <= kMaxPasses;
  }
};
// Channel keys must be identical on every node, so their parameters are fixed by the protocol.
constexpr KdfParams kChannelKdf{8192, 3};

// Salt must be >= 8 bytes, out_len 4..1024. Returns false for invalid parameters or allocation
// failure.
bool argon2id(uint8_t* out, size_t out_len, ByteView passphrase, ByteView salt, const KdfParams& p);

}  // namespace ember::crypto
