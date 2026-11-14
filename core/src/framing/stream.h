// EmberMesh framing — resynchronising frame format for raw byte streams (serial cable, TCP, BLE
// UART, Bluetooth SPP, pipes). Not a radio format: it assumes the stream mostly delivers the bytes
// that were sent, but survives arbitrary chunking, line noise before/between frames, dropped or
// inserted bytes, and corrupted length fields, without ever trusting the length.
//
// Wire format (all multi-byte fields big-endian), 10 bytes overhead per frame:
//
//   offset size  field
//   0      2     sync     0xE5 0x4D
//   2      1     type     application-defined channel (0 = mesh frame); opaque to the framer
//   3      2     length   payload length L (0..65535, receiver additionally caps it)
//   5      1     hcrc     CRC-8 (ember::crc8, poly 0x07) over bytes 2..4 (type + length)
//   6      L     payload
//   6+L    4     crc      CRC-32 (ember::crc32) over bytes 2..5+L (type, length, hcrc, payload)
//
// Receiver algorithm: scan for the sync pair; check hcrc and length <= max_payload immediately (a
// corrupted length is rejected after 6 bytes instead of stalling the stream); then wait for the
// payload and CRC. On any failure only the first sync byte is discarded and scanning restarts at
// the next byte, so a genuine frame hidden inside a corrupted one (or inside garbage) is still
// found. Memory is bounded by max_payload + 10 bytes.
//
// A frame whose header was valid but whose tail never arrives (cable unplugged) holds up later
// frames only until enough bytes have arrived to disprove it; call reset() on reconnect.
//
// CPU budget (security review H5): the 8-bit header CRC is trivial to forge, so hostile input can
// place a "valid" header announcing max_payload bytes at every byte offset, and every one of them
// would cost a CRC-32 over max_payload bytes. The decoder therefore earns kWorkPerByte bytes of
// CRC credit per input byte (banked up to a cap) and spends payload length + 4 on each full check.
// A candidate it cannot afford is skipped unchecked (stats().budget_skipped). Honest traffic needs
// ~1 credit per byte, so it is never affected; under attack total work stays linear in the input.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ember/bytes.h"

namespace ember::stream {

constexpr uint8_t SYNC0 = 0xE5;
constexpr uint8_t SYNC1 = 0x4D;
constexpr size_t kHeaderSize = 6;
constexpr size_t kOverhead = 10;
constexpr size_t kMaxPayload = 65535;

// Appends one frame. Returns false (appending nothing) if payload is larger than kMaxPayload.
bool encode_append(ByteView payload, Bytes& out, uint8_t type = 0);
// Returns the encoded frame, or an empty buffer if the payload is too large.
Bytes encode(ByteView payload, uint8_t type = 0);

struct DecoderStats {
  uint64_t frames = 0;
  uint64_t bad_header = 0;       // sync found but hcrc wrong
  uint64_t oversize = 0;         // header valid but length > max_payload
  uint64_t bad_crc = 0;          // full frame received but CRC-32 mismatch
  uint64_t discarded_bytes = 0;  // bytes skipped while hunting for sync
  uint64_t budget_skipped = 0;   // candidates dropped unchecked because the CPU work budget ran out
};

class Decoder {
 public:
  static constexpr size_t kWorkPerByte = 16;

  explicit Decoder(size_t max_payload = 4096);

  // Pull interface: consumes bytes from `in` and returns how many were consumed (>= 1 if `in` is
  // non-empty). When *frame_ready is set, type()/payload() describe the frame until the next call.
  size_t feed_some(ByteView in, bool* frame_ready);
  uint8_t type() const { return type_; }
  ByteView payload() const { return payload_; }

  // Push interface: on_frame(uint8_t type, ByteView payload) for every valid frame.
  template <class F>
  void feed(ByteView in, F&& on_frame) {
    for (;;) {
      bool got = false;
      size_t n = feed_some(in, &got);
      in = in.sub(n);
      if (got) on_frame(type_, payload_);
      else if (in.empty()) break;
    }
  }

  void reset();
  const DecoderStats& stats() const { return stats_; }
  size_t buffered() const { return buf_.size() - head_; }

 private:
  bool try_parse(bool* frame_ready);
  void drop(size_t n);

  Bytes buf_;
  size_t head_ = 0;
  size_t consumed_frame_ = 0;  // bytes of the last delivered frame, dropped on the next call
  size_t max_;
  size_t credit_ = 0, credit_cap_ = 0;
  uint8_t type_ = 0;
  ByteView payload_;
  DecoderStats stats_;
};

}  // namespace ember::stream
