// EmberMesh store — on-disk format constants and codecs for KvStore (internal, but stable: tests
// and offline repair tools may use it). The format is summarised in store/kv_store.h.
//
// File header (kHeaderSize bytes, stored twice at offsets 0 and kHeaderSize; all integers LE):
//     0  u8[8]  magic "EMBERKV\0"
//     8  u32    format version (kFormatVersion)
//    12  u32    flags (bit 0: encrypted)
//    16  u64    generation
//    24  u64    base sequence (last sequence number included in the snapshot)
//    32  u8[16] salt (seeds the frame CRCs; bound into every AEAD AAD)
//    48  u16    cipher params length (<= 256)      50 u8[256] cipher params (zero padded)
//   306  u16    key check length (<= 128)          308 u8[128] key check: nonce(24) || seal(
//                                                   "EMBERKV-KEYCHECK", aad = header[0..306))
//   508  u32    CRC-32 of header[0..508)
// Frames start at kDataStart. Frame = 20-byte header | payload | u32 frame CRC:
//     0 'E'  1 0x4B  2 type  3 flags(bit0 encrypted)  4 u32 payload_len  8 u64 seq
//    16 u32 header CRC = crc32(header[0..16), seed)   seed = crc32(salt)
//    trailer = crc32(payload, header CRC)
// Encrypted payload = nonce(24) || seal(plaintext, aad = salt || u64 generation || header[0..16)
//                    [v2 snapshot chunk: || u64 ordinal] [v2 snapshot commit: || u64 chunks || u32 rcrc]).
// Plaintext payloads:
//   batch (type 1):   varint n, n x op:  u8 kind (1 put, 2 del, 3 range-del)
//                     varint ns_len, ns, varint key_len, key,
//                     put: varint val_len, val     range-del: u8 has_upper [varint len, upper]
//                     op i has sequence number frame.seq + i.
//   snapshot chunk (type 2, seq = base seq): varint n, n x (varint ns_len, ns, varint key_len, key,
//                     varint seq, varint val_len, val)
//   snapshot commit (type 3, seq = base seq): varint records, varint chunks
#pragma once

#include <cstdint>
#include <string>

#include "ember/bytes.h"

namespace ember::store::fmt {

// Version 1: original format. Version 2 (written for encrypted stores since the security review;
// plaintext stores keep writing 1 so older builds still open them): snapshot chunk frames add the
// chunk ordinal to their AAD, and the snapshot commit adds the chunk count and a running CRC-32
// over every chunk's on-disk payload, so chunks cannot be swapped, duplicated, reordered or
// dropped undetected. The ordinal of a snapshot frame is its nonce counter minus one (the key check
// uses counter 0 of the same session prefix, chunk k uses k + 1, the commit chunks + 1); readers of
// either version reject snapshot frames whose ordinals do not strictly increase. Both versions are
// read; version-1 encrypted stores are rewritten as version 2 by the next compaction / re-key.
constexpr uint32_t kFormatVersion = 1;
constexpr uint32_t kFormatVersionMax = 2;
constexpr uint32_t kFormatVersionBoundSnapshot = 2;
constexpr size_t kHeaderSize = 512;
constexpr size_t kDataStart = 2 * kHeaderSize;
constexpr size_t kParamsMax = 256;
constexpr size_t kKeyCheckMax = 128;
constexpr size_t kKeyCheckAadLen = 306;
constexpr uint32_t kFileFlagEncrypted = 1;

constexpr uint8_t kFrameMagic0 = 'E';
constexpr uint8_t kFrameMagic1 = 0x4B;
constexpr size_t kFrameHeader = 20;
constexpr size_t kFrameTrailer = 4;
constexpr size_t kFrameOverhead = kFrameHeader + kFrameTrailer;
constexpr uint8_t kFrameBatch = 1;
constexpr uint8_t kFrameSnapChunk = 2;
constexpr uint8_t kFrameSnapCommit = 3;
constexpr uint8_t kFrameFlagEncrypted = 1;
// Hard cap on a frame payload accepted by recovery (bigger length fields are corruption).
constexpr uint32_t kMaxFramePayload = (uint32_t(128) << 20) + 4096;

constexpr char kKeyCheckPlain[] = "EMBERKV-KEYCHECK";  // 16 bytes

// Table-driven CRC-32 (IEEE), bit-identical to ember::crc32() in ember/crc.h.
uint32_t crc32(ByteView data, uint32_t prev = 0);

struct Header {
  uint32_t version = kFormatVersion;
  uint32_t flags = 0;
  uint64_t generation = 0;
  uint64_t base_seq = 0;
  uint8_t salt[16] = {};
  Bytes params;
  Bytes keycheck;
};

// Serialises to exactly kHeaderSize bytes (params/keycheck must fit their slots).
Bytes encode_header(const Header& h);
// Bytes [0, kKeyCheckAadLen) of encode_header(h) — the AAD of the key check.
Bytes header_aad(const Header& h);
// Validates magic, CRC and slot lengths. Does not check the version.
bool decode_header(ByteView b, Header* h);

// "data-<gen>.ekv" / "tmp-<gen>.ekv" with gen in decimal.
std::string data_name(uint64_t gen);
std::string tmp_name(uint64_t gen);
// Strict parse of the names above (no sign, no leading zeros, no overflow). kind: 'd' or 't'.
bool parse_name(const std::string& name, char* kind, uint64_t* gen);

}  // namespace ember::store::fmt
