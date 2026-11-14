// EmberMesh core — packet authentication and encryption (docs/PROTOCOL.md §4).
//
//   AD      = packet header bytes with the hop byte zeroed
//   Signed  : packet = header || body || Ed25519(sign_sk, "ember-sig-v1" || AD || body)
//   PairMac : packet = header || body || BLAKE2b-128(key=K_pair, "ember-mac-v1" || AD || body)
//   PairAead: packet = header || XChaCha20-Poly1305(K_pair, nonce, AD, body)
//   ChanAead: packet = header || XChaCha20-Poly1305(K_chan, nonce, AD, body || inner_sig)
//             inner_sig = Ed25519 over ("ember-sig-v1" || AD || body), so members cannot forge
//             each other.
//   None    : packet = header || body (ACK and WHOIS only; acks are self-authenticating)
//   nonce   = BLAKE2b-192("ember-nonce-v2" || AD)  (AD contains origin, pkt_id, created and the
//             header's 8-byte salt = BLAKE2b-64(key=K, "ember-salt-v2" || u8 len || entropy ||
//             header-with-zero-salt || plaintext): random per packet, and distinct plaintexts
//             get distinct nonces even if the RNG repeats)
//   K_pair  = BLAKE2b-256(key = X25519(sk_a, pk_b), "ember-pair-v1" || min(pk_a,pk_b) || max(..))
//   K_chan  = Argon2id(passphrase, "ember-channel-v1:" || normalized name, kChannelKdf)
//   chan id = u32 of BLAKE2b-32(key = K_chan, "ember-chan-id-v1")         (encrypted channels)
//           = u32 of BLAKE2b-32("ember-open-channel-v1:" || normalized name) (open channels)
#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "crypto/crypto.h"
#include "crypto/identity.h"
#include "proto/packet.h"

namespace ember::proto {

using AckSecret = std::array<uint8_t, 8>;

bool pair_key(const Identity& me, const crypto::Key32& their_dh_pk, crypto::Key32& out);
AckSecret ack_secret(const crypto::Key32& pair_key, const NodeId& origin, uint32_t pkt_id);
AckCommit ack_commit(const AckSecret& secret);

struct ChannelKey {
  uint32_t id = 0;
  bool encrypted = false;
  std::string name;  // normalized
  crypto::Key32 key{};
};
// Lower-cases ASCII, trims and collapses whitespace, max 32 bytes.
std::string normalize_channel_name(std::string_view name);
// Slow (Argon2id, kChannelKdf). Encrypted channel.
ChannelKey derive_channel_key(std::string_view name, std::string_view passphrase);
// Open (cleartext, signed) named channel.
ChannelKey open_channel(std::string_view name);
// For callers that already hold the 32-byte key (from an invite).
ChannelKey channel_from_key(std::string_view name, const crypto::Key32& key);

// Builders. `h` must be complete except sec (set by the builder) and, for want_ack packets,
// ack_commit (the caller sets it before sealing). Return empty Bytes if the packet would exceed
// kMaxPacket.
Bytes seal_none(Header h, ByteView body);
Bytes seal_signed(Header h, ByteView body, const Identity& me);
Bytes seal_pair_mac(Header h, ByteView body, const crypto::Key32& pair_key);
// `entropy`: fresh random bytes (16 recommended) for the nonce salt.
Bytes seal_pair_aead(Header h, ByteView body, const crypto::Key32& pair_key, ByteView entropy = ByteView());
Bytes seal_chan(Header h, ByteView body, const ChannelKey& ch, const Identity& me, ByteView entropy = ByteView());

bool verify_signed(const PacketView& p, const crypto::Key32& sign_pk);
bool verify_pair_mac(const PacketView& p, const crypto::Key32& pair_key);
bool open_pair_aead(const PacketView& p, const crypto::Key32& pair_key, Bytes& body);
// Decrypts; `body` gets the cleartext body and `inner_sig` the member signature.
bool open_chan(const PacketView& p, const ChannelKey& ch, Bytes& body, crypto::Sig64& inner_sig);
bool verify_chan_sig(const PacketView& p, ByteView body, const crypto::Sig64& sig, const crypto::Key32& sign_pk);

// --- bundles (sneakernet) -------------------------------------------------------------------------
//   "EMB1" | varint count | count * (varint len | packet) | u32be CRC-32 of everything before
constexpr size_t kMaxBundlePackets = 100000;
Bytes encode_bundle(const std::vector<Bytes>& packets);
// Returns false on a corrupt container. Individual packets are NOT validated here.
bool decode_bundle(ByteView data, std::vector<ByteView>& packets);

}  // namespace ember::proto
