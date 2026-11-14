// EmberMesh core / modem — bit-level HDLC framing as used by AX.25 / APRS on AFSK and PSK modems.
//
// Transmit order (ISO 3309 / AX.25 2.2 section 3):
//   flags (0x7E, sent LSB first = 0 1 1 1 1 1 1 0, never bit-stuffed)
//   frame bytes, each sent least-significant bit first
//   FCS = CRC-16/X-25 (ember::crc16_x25_of) of the frame bytes, low byte first, LSB first
//   closing flag (may be shared as the opening flag of the next frame)
//   Between the flags a 0 is inserted after every five consecutive 1s (bit stuffing).
//   Seven or more consecutive 1s is an abort.
// On the line the AFSK modems apply NRZI: a 0 bit toggles the tone, a 1 bit keeps it (nrzi_encode).
//
// Receive: HdlcDecoder takes the de-NRZI'd bit stream, removes stuffing, finds flags and checks the
// FCS (residue 0xF0B8 over frame+FCS, RFC 1662). Bounded memory: frames longer than max_len are
// dropped as they grow.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ember/bytes.h"

namespace ember {
namespace modem {

// Appends n flags (8 bits each).
void hdlc_append_flags(std::vector<uint8_t>& bits, size_t n);
// Appends the stuffed bits of frame + FCS (no flags). Bits are 0/1 bytes.
void hdlc_append_frame(std::vector<uint8_t>& bits, ByteView frame);
// Full bit stream: pre_flags flags, each frame followed by one flag, then (post_flags - 1) more
// flags (post_flags >= 1 is enforced).
std::vector<uint8_t> hdlc_encode(const std::vector<Bytes>& frames, size_t pre_flags, size_t post_flags);
// NRZI in place: 0 -> toggle level, 1 -> keep. `level` is the line state before the first bit and
// is updated. Output values are line levels (1 = mark, 0 = space).
void nrzi_encode(std::vector<uint8_t>& bits, uint8_t* level);

// Converts line levels back to data bits.
class NrziDecoder {
 public:
  uint8_t push(uint8_t level) {
    uint8_t b = (level == last_) ? 1 : 0;
    last_ = level;
    return b;
  }
  void reset() { last_ = 0; }

 private:
  uint8_t last_ = 0;
};

class HdlcDecoder {
 public:
  HdlcDecoder() { init(1, 1024); }
  // min_len/max_len: frame size limits in bytes, excluding the 2 FCS bytes.
  void init(size_t min_len, size_t max_len);
  void reset();
  // Feeds one data bit. Returns true when a frame with a valid FCS has just completed; its bytes
  // (without FCS) are then available from frame() until the next push().
  bool push(uint8_t bit);
  ByteView frame() const { return ByteView(buf_.data(), frame_len_); }
  // True while bytes are being collected after a flag (i.e. "inside" a possible frame).
  bool collecting() const { return collecting_ && len_ > 0; }
  // Bits since the last flag, saturating (used for lock / DCD heuristics).
  uint32_t bits_since_flag() const { return since_flag_; }
  uint32_t fcs_errors() const { return fcs_errors_; }

 private:
  std::vector<uint8_t> buf_;
  size_t min_len_ = 1, max_len_ = 1024;
  uint8_t pat_ = 0, acc_ = 0;
  int nacc_ = 0;
  size_t len_ = 0, frame_len_ = 0;
  bool collecting_ = false;
  uint32_t since_flag_ = 0xFFFF;
  uint32_t fcs_errors_ = 0;
};

}  // namespace modem
}  // namespace ember
