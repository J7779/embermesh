// EmberMesh proto — internet relay rendezvous and sealing (docs/PROTOCOL.md §12,
// docs/CONNECTIVITY.md §4).
//
//   R          = BLAKE2b-256(key = K_pair, "ember-relay-pair-v1")       (contact pair)
//              = BLAKE2b-256(key = K_chan, "ember-relay-chan-v1")       (private channel)
//   H          = u8 |host| || host        host = canonical "hostname:port" of the broker
//   topic(e)   = hex(BLAKE2b-128(key = R, "ember-relay-topic-v1" || u32be e || H))   32 lowercase hex
//   K_frame(e) = BLAKE2b-256(key = R, "ember-relay-aead-v1" || u32be e || H)
//   P          = u64be sent_ms || u16be len || frame[len] || zero padding to 128/256/…/4096 bytes
//   nonce      = BLAKE2b-192(key = K_frame(e), "ember-relay-nonce-v1" || rnd16 || u64be sent_ms || P)
//   AD         = "ember-relay-v1" || u8 1 || H || topic (ASCII)
//   message    = 0x01 || nonce[24] || XChaCha20-Poly1305(K_frame(e), nonce, AD, P)
//
// e = floor(unix ms / 86 400 000) (mesh clock). All functions are pure; randomness is passed in.
#pragma once

#include <cstdint>
#include <string>

#include "crypto/crypto.h"
#include "ember/bytes.h"

namespace ember::proto::relay {

constexpr uint8_t kVersion = 1;
constexpr size_t kNonceLen = 24;
constexpr size_t kTagLen = 16;
constexpr size_t kHeaderLen = 1 + kNonceLen;                    // version + nonce
constexpr size_t kPlainOverhead = 8 + 2;                        // sent_ms + len
constexpr size_t kMaxPadded = 4096;
constexpr size_t kMaxFrame = kMaxPadded - kPlainOverhead;       // 4086
constexpr size_t kMaxMessage = kHeaderLen + kMaxPadded + kTagLen;  // 4137
constexpr size_t kTopicLen = 32;

crypto::Key32 pair_root(const crypto::Key32& k_pair);
crypto::Key32 channel_root(const crypto::Key32& k_chan);
uint32_t epoch_of(uint64_t unix_ms);
// Canonical broker host for derivations: lower-case host (IPv6 without brackets) + ":" + port
// (default 443 for wss://, 80 for ws://). Empty for an invalid URL or a host longer than 200 bytes.
std::string canonical_host(const std::string& url);
std::string topic(const crypto::Key32& root, uint32_t epoch, const std::string& host);
crypto::Key32 frame_key(const crypto::Key32& root, uint32_t epoch, const std::string& host);
// Padded plaintext size for a frame of `len` bytes (0 if too large).
size_t padded_size(size_t len);

// Seals one mesh frame. `rnd` must be 16 fresh random bytes (the nonce is derived from them and the
// plaintext, so a weak RNG never repeats a nonce for different frames). Empty result if the frame is
// empty or larger than kMaxFrame.
Bytes seal(const crypto::Key32& k_frame, const std::string& host, const std::string& topic, const uint8_t rnd[16],
           uint64_t sent_ms, ByteView frame);

struct Opened {
  Bytes frame;
  uint64_t sent_ms = 0;
  uint8_t nonce[kNonceLen] = {};
};
// Opens a message received on `topic`. False on anything malformed: wrong version, bad size (not
// one of the padding classes), authentication failure, length beyond the padding, non-zero padding.
bool open(const crypto::Key32& k_frame, const std::string& host, const std::string& topic, ByteView msg, Opened* out);

}  // namespace ember::proto::relay
