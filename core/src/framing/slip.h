// EmberMesh framing — SLIP (RFC 1055) byte-stuffed framing, encoder + streaming decoder.
//
// Wire format:  END  escaped(payload)  END
//   END=0xC0 ESC=0xDB; inside a frame 0xC0 is sent as DB DC and 0xDB as DB DD.
// The same escaping is used by KISS (framing/kiss.h builds on this decoder).
//
// The decoder is a byte-at-a-time state machine that tolerates arbitrary chunking, back-to-back
// END bytes (empty frames are silently skipped), oversize frames (dropped up to the next END) and
// invalid escape sequences (frame dropped, resync at the next END). It never allocates more than
// `max_frame` bytes, whatever the input.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ember/bytes.h"

namespace ember::slip {

constexpr uint8_t END = 0xC0;
constexpr uint8_t ESC = 0xDB;
constexpr uint8_t ESC_END = 0xDC;
constexpr uint8_t ESC_ESC = 0xDD;

// Appends the escaped bytes of `in` to `out` (no END delimiters).
void escape_append(ByteView in, Bytes& out);

// Appends one complete frame: [END] escaped(payload) END. A leading END (recommended by RFC 1055)
// flushes any line noise accumulated by the receiver.
void encode_append(ByteView payload, Bytes& out, bool leading_end = true);
Bytes encode(ByteView payload, bool leading_end = true);

// Unescapes one frame body that contains no END byte. Returns false (and clears `out`) on an
// invalid escape, a dangling ESC or an END inside the body.
bool unescape(ByteView in, Bytes& out);

struct DecoderStats {
  uint64_t frames = 0;           // frames delivered
  uint64_t oversize = 0;         // frames dropped because they exceeded max_frame
  uint64_t bad_escape = 0;       // frames dropped because of an invalid escape sequence
  uint64_t discarded_bytes = 0;  // bytes thrown away while hunting / discarding
};

class Decoder {
 public:
  // max_frame: largest unescaped frame accepted (frames of exactly max_frame bytes are accepted).
  // require_leading_end: if true, bytes before the first END ever seen are discarded (useful when
  // opening a serial port mid-stream). Afterwards every END both ends and starts a frame.
  explicit Decoder(size_t max_frame = 2048, bool require_leading_end = false);

  // Low-level pull interface. Consumes bytes from `in` up to and including the END that completes
  // the next frame (or all of `in` if none completes) and returns the number consumed (>= 1 when
  // `in` is non-empty). When a frame completed, *frame_ready is set and frame() holds it until the
  // next call to feed_some()/feed()/reset().
  size_t feed_some(ByteView in, bool* frame_ready);
  ByteView frame() const { return ByteView(buf_); }

  // Convenience push interface: calls on_frame(ByteView) for every complete frame in `in`.
  template <class F>
  void feed(ByteView in, F&& on_frame) {
    while (!in.empty()) {
      bool got = false;
      size_t n = feed_some(in, &got);
      in = in.sub(n);
      if (got) on_frame(frame());
    }
  }

  // Forget any partial frame (e.g. when the endpoint reconnects). Stats are kept.
  void reset();
  const DecoderStats& stats() const { return stats_; }
  size_t max_frame() const { return max_; }

 private:
  enum class State : uint8_t { Hunt, Data, Escape, Discard };
  Bytes buf_;
  size_t max_;
  bool require_leading_;
  bool clear_pending_ = false;
  State st_;
  DecoderStats stats_;
};

}  // namespace ember::slip
