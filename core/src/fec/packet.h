// EmberMesh FEC — "protect / recover": a self-describing FEC envelope a link driver can wrap
// around one mesh frame before sending it over a noisy channel (soft modem, raw radio, lossy
// serial), and unwrap on the other side, learning how many symbols were repaired.
//
// Envelope layout:
//
//   header (10 bytes) = RS(10,4) codeword over [ver_flags, nroots, len_hi, len_lo]
//                       (6 parity bytes: any 3 corrupted header bytes are repaired)
//       ver_flags: high nibble = version (1), bit0 = body interleaved
//       nroots:    RS parity bytes per body block (kMinRoots..kMaxRoots)
//       len:       payload length L (big-endian, 0..65535)
//   body: D = L + 4 bytes  (payload || CRC-32 big-endian of the payload)
//       zero-padded to B * m bytes and split into B = ceil(D / (255 - nroots)) blocks of
//       m = ceil(D / B) bytes (padding is sent and ignored by the receiver, which knows L),
//       each block RS(m + nroots, m) encoded; if interleaved the B codewords are symbol-
//       interleaved (fec/interleave.h, rows = B) so a burst is spread over all codewords.
//   total size = 10 + B * (m + nroots)    (protected_size())
//
// Each body block corrects up to nroots/2 byte errors (or nroots erasures). The CRC-32 rejects the
// rare RS mis-correction, so recover() never returns a corrupted payload with probability better
// than ~2^-32. Bytes after the envelope (e.g. modem padding) are ignored.
//
// Typical choices: nroots 16 for short packets (<= 239 B, +26 B overhead, 8 byte errors/block),
// 32 for RS(255,223)-style protection on harsh channels.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ember/bytes.h"

namespace ember::fec {

constexpr size_t kPacketHeaderSize = 10;
constexpr uint8_t kPacketVersion = 1;
constexpr uint8_t kMinRoots = 2;
constexpr uint8_t kMaxRoots = 128;
constexpr size_t kMaxProtectedPayload = 65535;

struct ProtectParams {
  uint8_t nroots = 16;     // parity bytes per block, kMinRoots..kMaxRoots
  bool interleave = true;  // symbol-interleave the blocks (no effect with a single block)
};

struct RecoverInfo {
  size_t header_corrected = 0;  // header bytes repaired
  size_t corrected = 0;         // body symbols repaired (sum over blocks)
  size_t blocks = 0;
  size_t consumed = 0;          // envelope size in bytes
  uint8_t nroots = 0;
};

// Envelope size for a payload of `len` bytes, or 0 if len/params are invalid.
size_t protected_size(size_t len, const ProtectParams& params = ProtectParams());

// Returns the envelope, or an empty buffer if the payload is too large or params invalid.
Bytes protect(ByteView payload, const ProtectParams& params = ProtectParams());

// Decodes only the header (first 10 bytes) and reports the full envelope size, so a receiver that
// gets bytes incrementally knows how many to collect. Returns false if the header is unreadable,
// has an unknown version, invalid nroots, or len > max_payload.
bool peek_header(ByteView in, size_t* total_size, size_t max_payload = 4096);
// Same, also reporting the body's nroots (so a receiver can estimate the decode cost up front).
bool peek_header(ByteView in, size_t* total_size, size_t max_payload, uint8_t* nroots);

// Recovers the payload. Returns false if the envelope is truncated, the header or any block is
// uncorrectable, the length exceeds max_payload, or the CRC fails. `info` may be null.
bool recover(ByteView in, Bytes* payload, RecoverInfo* info = nullptr, size_t max_payload = 4096);

}  // namespace ember::fec
