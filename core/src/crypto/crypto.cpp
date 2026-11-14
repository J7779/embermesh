#include "crypto/crypto.h"

#include <cstdlib>
#include <cstring>
#include <new>

#include "monocypher/monocypher-ed25519.h"
#include "monocypher/monocypher.h"

namespace ember::crypto {

static_assert(sizeof(crypto_blake2b_ctx) <= 256, "Blake2b context buffer too small");

void wipe(void* p, size_t n) {
  if (p && n) crypto_wipe(p, n);
}

static size_t clamp_hash(size_t n) { return n < 1 ? 1 : (n > 64 ? 64 : n); }

void blake2b(uint8_t* out, size_t out_len, ByteView msg, ByteView key) {
  out_len = clamp_hash(out_len);
  size_t ks = key.size() > 64 ? 64 : key.size();
  if (ks) crypto_blake2b_keyed(out, out_len, key.data(), ks, msg.data(), msg.size());
  else crypto_blake2b(out, out_len, msg.data(), msg.size());
}

Key32 blake2b_256(ByteView msg, ByteView key) {
  Key32 k;
  blake2b(k.data(), 32, msg, key);
  return k;
}

Blake2b::Blake2b(size_t out_len, ByteView key) : out_len_(clamp_hash(out_len)) {
  auto* ctx = reinterpret_cast<crypto_blake2b_ctx*>(ctx_);
  size_t ks = key.size() > 64 ? 64 : key.size();
  if (ks) crypto_blake2b_keyed_init(ctx, out_len_, key.data(), ks);
  else crypto_blake2b_init(ctx, out_len_);
}
Blake2b::~Blake2b() { crypto_wipe(ctx_, sizeof ctx_); }
Blake2b& Blake2b::update(ByteView d) {
  if (!d.empty()) crypto_blake2b_update(reinterpret_cast<crypto_blake2b_ctx*>(ctx_), d.data(), d.size());
  return *this;
}
Blake2b& Blake2b::field(ByteView d) {
  uint8_t len[2] = {static_cast<uint8_t>(d.size() >> 8), static_cast<uint8_t>(d.size())};
  update(len, 2);
  return update(d);
}
void Blake2b::final(uint8_t* out) { crypto_blake2b_final(reinterpret_cast<crypto_blake2b_ctx*>(ctx_), out); }
Key32 Blake2b::final32() {
  Key32 k{};
  if (out_len_ == 32) final(k.data());
  return k;
}

void sha512(uint8_t out[64], ByteView msg) { crypto_sha512(out, msg.data(), msg.size()); }

void aead_seal(uint8_t* ct, uint8_t mac[16], const Key32& key, const uint8_t nonce[24], ByteView ad,
               ByteView pt) {
  crypto_aead_lock(ct, mac, key.data(), nonce, ad.data(), ad.size(), pt.data(), pt.size());
}

bool aead_open(uint8_t* pt, const uint8_t mac[16], const Key32& key, const uint8_t nonce[24],
               ByteView ad, ByteView ct) {
  if (crypto_aead_unlock(pt, mac, key.data(), nonce, ad.data(), ad.size(), ct.data(), ct.size()) != 0) {
    if (pt && ct.size()) std::memset(pt, 0, ct.size());
    return false;
  }
  return true;
}

Bytes seal(const Key32& key, const uint8_t nonce[24], ByteView ad, ByteView pt) {
  Bytes out(pt.size() + kMacSize);
  aead_seal(out.data(), out.data() + pt.size(), key, nonce, ad, pt);
  return out;
}

bool open(const Key32& key, const uint8_t nonce[24], ByteView ad, ByteView ct_mac, Bytes& out) {
  out.clear();
  if (ct_mac.size() < kMacSize) return false;
  size_t n = ct_mac.size() - kMacSize;
  out.resize(n);
  if (!aead_open(out.data(), ct_mac.data() + n, key, nonce, ad, ct_mac.sub(0, n))) {
    out.clear();
    return false;
  }
  return true;
}

void x25519_public(Key32& pub, const Key32& secret) { crypto_x25519_public_key(pub.data(), secret.data()); }

bool x25519(Key32& shared, const Key32& secret, const Key32& their_public) {
  crypto_x25519(shared.data(), secret.data(), their_public.data());
  uint8_t acc = 0;
  for (uint8_t b : shared) acc |= b;
  return acc != 0;
}

void ed25519_keypair(const Key32& seed, SignSecret& secret, Key32& pub) {
  Key32 s = seed;  // monocypher wipes the seed argument
  crypto_ed25519_key_pair(secret.data(), pub.data(), s.data());
}

void ed25519_sign(uint8_t sig[64], const SignSecret& secret, ByteView msg) {
  crypto_ed25519_sign(sig, secret.data(), msg.data(), msg.size());
}

bool ed25519_verify(const uint8_t sig[64], const Key32& pub, ByteView msg) {
  return crypto_ed25519_check(sig, pub.data(), msg.data(), msg.size()) == 0;
}

bool argon2id(uint8_t* out, size_t out_len, ByteView passphrase, ByteView salt, const KdfParams& p) {
  if (!p.valid() || salt.size() < 8 || salt.size() > 1024 || out_len < 4 || out_len > 1024 ||
      passphrase.size() > (1u << 20))
    return false;
  size_t bytes = static_cast<size_t>(p.mem_kib) * 1024u;
  void* work = std::malloc(bytes);
  if (!work) return false;
  crypto_argon2_config cfg;
  cfg.algorithm = CRYPTO_ARGON2_ID;
  cfg.nb_blocks = p.mem_kib;
  cfg.nb_passes = p.passes;
  cfg.nb_lanes = 1;
  crypto_argon2_inputs in;
  static const uint8_t kEmpty[1] = {0};
  in.pass = passphrase.size() ? passphrase.data() : kEmpty;
  in.pass_size = static_cast<uint32_t>(passphrase.size());
  in.salt = salt.data();
  in.salt_size = static_cast<uint32_t>(salt.size());
  crypto_argon2(out, static_cast<uint32_t>(out_len), work, cfg, in, crypto_argon2_no_extras);
  crypto_wipe(work, bytes);
  std::free(work);
  return true;
}

}  // namespace ember::crypto
