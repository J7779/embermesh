// EmberMesh framing — KISS TNC protocol (Chepponis & Karn, 1987), encoder + streaming decoder.
//
// Wire format:  FEND  type  escaped(payload)  FEND
//   FEND=0xC0 FESC=0xDB TFEND=0xDC TFESC=0xDD (same escaping as SLIP, see framing/slip.h).
//   type = (port << 4) | command, port 0..15; type 0xFF = "Return" (leave KISS mode).
//   The type byte is escaped like data (port 12 data frames have type 0xC0!).
//   Commands: 0 data frame (payload = AX.25 frame *without* FCS), 1 TXDELAY (10 ms units),
//   2 P (persistence 0..255), 3 SLOTTIME (10 ms units), 4 TXTAIL (10 ms units),
//   5 FULLDUPLEX (0 = half), 6 SETHARDWARE (vendor specific payload).
//
// The decoder by default discards bytes until the first FEND (so a serial port opened mid-frame
// does not deliver garbage), skips back-to-back FENDs, drops frames with an invalid escape or
// longer than max_frame payload bytes, and resynchronises at the next FEND. Frames whose command
// nibble is not one of the standard ones are still delivered (command() holds the raw nibble) so
// that drivers can support vendor extensions such as ACKMODE (0x0C); a driver that only wants
// data should check `command == Command::Data`.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ember/bytes.h"
#include "framing/slip.h"

namespace ember::kiss {

constexpr uint8_t FEND = 0xC0;
constexpr uint8_t FESC = 0xDB;
constexpr uint8_t TFEND = 0xDC;
constexpr uint8_t TFESC = 0xDD;

enum class Command : uint8_t {
  Data = 0,
  TxDelay = 1,
  Persistence = 2,
  SlotTime = 3,
  TxTail = 4,
  FullDuplex = 5,
  SetHardware = 6,
  Return = 0xFF,  // whole type byte 0xFF; port is reported as 0
};

struct Frame {
  uint8_t port = 0;
  Command command = Command::Data;  // may hold a non-standard nibble value (7..15)
  ByteView payload;                 // valid until the decoder is fed again
};

// Appends one KISS frame. Returns false (appending nothing) if port > 15.
// For Command::Return the port is ignored and the frame is C0 FF C0.
bool encode_append(ByteView payload, Bytes& out, uint8_t port = 0, Command cmd = Command::Data);
// Same, returning a new buffer; empty on invalid port.
Bytes encode(ByteView payload, uint8_t port = 0, Command cmd = Command::Data);
// Data frame carrying an AX.25 frame (without FCS).
inline Bytes encode_data(ByteView ax25_frame, uint8_t port = 0) { return encode(ax25_frame, port, Command::Data); }
// One-byte parameter frame (TXDELAY, P, SLOTTIME, TXTAIL, FULLDUPLEX).
Bytes encode_param(Command cmd, uint8_t value, uint8_t port = 0);
// C0 FF C0: tells the TNC to leave KISS mode.
Bytes encode_return();

struct DecoderStats {
  uint64_t frames = 0;           // frames delivered
  uint64_t oversize = 0;         // dropped: payload longer than max_frame
  uint64_t bad_escape = 0;       // dropped: invalid FESC sequence
  uint64_t discarded_bytes = 0;  // garbage before first FEND / in dropped frames
};

class Decoder {
 public:
  // max_frame: largest payload (excluding the type byte) accepted.
  explicit Decoder(size_t max_frame = 1024, bool require_leading_fend = true);

  // Pull interface, same contract as slip::Decoder::feed_some(): returns bytes consumed; when
  // *frame_ready is set, frame() is valid until the next feed.
  size_t feed_some(ByteView in, bool* frame_ready);
  const Frame& frame() const { return frame_; }

  // Push interface: calls on_frame(const kiss::Frame&) for each complete frame.
  template <class F>
  void feed(ByteView in, F&& on_frame) {
    while (!in.empty()) {
      bool got = false;
      size_t n = feed_some(in, &got);
      in = in.sub(n);
      if (got) on_frame(frame_);
    }
  }

  void reset() { slip_.reset(); frame_ = Frame(); }
  DecoderStats stats() const;

 private:
  slip::Decoder slip_;
  Frame frame_;
};

}  // namespace ember::kiss
