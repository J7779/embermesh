// EmberMesh framing — COBS (Consistent Overhead Byte Stuffing, Cheshire & Baker 1999).
//
// encode() removes every 0x00 from the payload at a cost of at most 1 byte per 254 (+1). On a
// byte stream each encoded frame is followed by a single 0x00 delimiter (encode_frame()). The
// encoding of an empty payload is the single byte 0x01. Decoding is strict: a zero byte inside the
// data, a code byte pointing past the end, or an empty input are all rejected, so corrupted
// frames are dropped rather than silently mangled (an upper-layer CRC is still recommended — COBS
// itself carries no checksum).
#pragma once

#include <cstddef>
#include <cstdint>

#include "ember/bytes.h"

namespace ember::cobs {

// Worst-case encoded size (without delimiter) for n payload bytes.
constexpr size_t max_encoded_size(size_t n) { return n + n / 254 + 1; }

void encode_append(ByteView in, Bytes& out);
Bytes encode(ByteView in);
// encode(in) followed by the 0x00 delimiter. If leading_delimiter, a 0x00 is also prepended so
// the receiver discards any line noise before the frame.
Bytes encode_frame(ByteView in, bool leading_delimiter = false);

// Strict decode of one encoded frame (no delimiter). Returns false and clears `out` on error.
bool decode(ByteView in, Bytes& out);

struct DecoderStats {
  uint64_t frames = 0;
  uint64_t oversize = 0;   // encoded frame longer than max_encoded_size(max_frame)
  uint64_t invalid = 0;    // malformed COBS
  uint64_t discarded_bytes = 0;
};

// Streaming decoder: splits the stream at 0x00, decodes each piece, skips empty pieces, drops
// malformed or oversize ones and resynchronises at the next 0x00.
class Decoder {
 public:
  explicit Decoder(size_t max_frame = 2048);

  // Pull interface (see slip::Decoder::feed_some for the contract).
  size_t feed_some(ByteView in, bool* frame_ready);
  ByteView frame() const { return ByteView(out_); }

  template <class F>
  void feed(ByteView in, F&& on_frame) {
    while (!in.empty()) {
      bool got = false;
      size_t n = feed_some(in, &got);
      in = in.sub(n);
      if (got) on_frame(frame());
    }
  }

  void reset() { enc_.clear(); discarding_ = false; }
  const DecoderStats& stats() const { return stats_; }

 private:
  Bytes enc_;
  Bytes out_;
  size_t max_;
  bool discarding_ = false;
  DecoderStats stats_;
};

}  // namespace ember::cobs
