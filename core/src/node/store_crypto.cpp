#include "node/store_crypto.h"

#include <algorithm>

namespace ember::node {

bool AeadRecordCipher::seal(ByteView nonce, ByteView aad, ByteView plaintext, Bytes* out) {
  if (!out || nonce.size() != kNonceSize) return false;
  *out = crypto::seal(key_, nonce.data(), aad, plaintext);
  return out->size() == plaintext.size() + crypto::kMacSize;
}

bool AeadRecordCipher::open(ByteView nonce, ByteView aad, ByteView ciphertext, Bytes* out) {
  if (!out || nonce.size() != kNonceSize) return false;
  return crypto::open(key_, nonce.data(), aad, ciphertext, *out);
}

Bytes StorageParams::encode() const {
  ByteWriter w(kSizeV2);
  w.u8(version == 1 ? 1 : 2).u32be(kdf.mem_kib).u8(static_cast<uint8_t>(kdf.passes));
  w.bytes(salt.data(), salt.size()).bytes(duress_salt.data(), duress_salt.size());
  w.bytes(duress_hash.data(), duress_hash.size());
  if (version != 1) w.bytes(decoy.data(), decoy.size());
  return w.take();
}

bool StorageParams::decode(ByteView in, StorageParams* out) {
  if (!out || (in.size() != kSize && in.size() != kSizeV2)) return false;
  ByteReader r(in);
  StorageParams p;
  p.version = r.u8();
  if ((p.version == 1 && in.size() != kSize) || (p.version == 2 && in.size() != kSizeV2) || p.version > 2 ||
      p.version == 0)
    return false;
  p.kdf.mem_kib = r.u32be();
  p.kdf.passes = r.u8();
  r.copy(p.salt.data(), p.salt.size());
  r.copy(p.duress_salt.data(), p.duress_salt.size());
  r.copy(p.duress_hash.data(), p.duress_hash.size());
  if (p.version == 2) r.copy(p.decoy.data(), p.decoy.size());
  if (!r.at_end() || !p.kdf.valid()) return false;
  if (p.kdf.mem_kib > kMaxHeaderMemKib || p.kdf.passes > kMaxHeaderPasses) return false;  // L8
  *out = p;
  return true;
}

StorageParams StorageParams::fresh(const crypto::KdfParams& kdf, crypto::Csprng& rng) {
  StorageParams p;
  p.kdf = kdf;
  rng.fill(p.salt);
  rng.fill(p.duress_salt);
  rng.fill(p.duress_hash);
  rng.fill(p.decoy);
  return p;
}

namespace {
crypto::Key32 keyed(const crypto::Key32& d, std::string_view label) {
  crypto::Blake2b h(32, ByteView(d.data(), 32));
  h.update(label);
  return h.final32();
}
}  // namespace

crypto::Key32 decoy_key_for(const crypto::Key32& d) { return keyed(d, "ember-decoy-v2"); }

bool duress_matches(const StorageParams& p, const crypto::Key32& d) {
  if (p.version == 1) return ct_equal(ByteView(d.data(), 32), ByteView(p.duress_hash.data(), 32));
  crypto::Key32 c = keyed(d, "ember-duress-check-v2");
  bool ok = ct_equal(ByteView(c.data(), 32), ByteView(p.duress_hash.data(), 32));
  crypto::wipe(c);
  return ok;
}

void reseal_decoy(StorageParams& p, const crypto::Key32& dk, ByteView presentation, crypto::Csprng& rng) {
  Bytes pt(kDecoyPlain, 0);
  std::copy(presentation.begin(), presentation.begin() + static_cast<std::ptrdiff_t>(std::min(presentation.size(), kDecoyPlain)), pt.begin());
  uint8_t nonce[24];
  rng.fill(nonce, 24);
  Bytes ct = crypto::seal(dk, nonce, ByteView(), pt);
  crypto::wipe(pt);
  std::copy(nonce, nonce + 24, p.decoy.begin());
  std::copy(ct.begin(), ct.end(), p.decoy.begin() + 24);
  p.version = 2;
}

void set_duress_slot(StorageParams& p, const crypto::Key32& d, ByteView presentation, crypto::Csprng& rng) {
  p.version = 2;
  p.duress_hash = keyed(d, "ember-duress-check-v2");
  crypto::Key32 dk = decoy_key_for(d);
  reseal_decoy(p, dk, presentation, rng);
  crypto::wipe(dk);
}

bool open_decoy(const StorageParams& p, const crypto::Key32& d, Bytes* out) {
  if (p.version != 2) return false;
  crypto::Key32 dk = decoy_key_for(d);
  bool ok = crypto::open(dk, p.decoy.data(), ByteView(), ByteView(p.decoy.data() + 24, kDecoySize - 24), *out);
  crypto::wipe(dk);
  return ok;
}

bool derive_storage_key(std::string_view passphrase, ByteView salt, const crypto::KdfParams& kdf, crypto::Key32* out) {
  return crypto::argon2id(out->data(), out->size(), ByteView(passphrase), salt, kdf);
}

}  // namespace ember::node
