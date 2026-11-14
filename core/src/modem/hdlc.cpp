#include "modem/hdlc.h"

#include "ember/crc.h"

namespace ember {
namespace modem {

void hdlc_append_flags(std::vector<uint8_t>& bits, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    for (int b = 0; b < 8; ++b) bits.push_back((0x7E >> b) & 1);
  }
}

void hdlc_append_frame(std::vector<uint8_t>& bits, ByteView frame) {
  uint16_t fcs = crc16_x25_of(frame);
  int ones = 0;
  auto put = [&](uint8_t byte) {
    for (int b = 0; b < 8; ++b) {
      uint8_t bit = (byte >> b) & 1;
      bits.push_back(bit);
      if (bit) {
        if (++ones == 5) { bits.push_back(0); ones = 0; }
      } else {
        ones = 0;
      }
    }
  };
  for (uint8_t c : frame) put(c);
  put(static_cast<uint8_t>(fcs & 0xFF));
  put(static_cast<uint8_t>(fcs >> 8));
}

std::vector<uint8_t> hdlc_encode(const std::vector<Bytes>& frames, size_t pre_flags, size_t post_flags) {
  std::vector<uint8_t> bits;
  if (post_flags < 1) post_flags = 1;
  hdlc_append_flags(bits, pre_flags);
  for (const Bytes& f : frames) {
    hdlc_append_frame(bits, ByteView(f));
    hdlc_append_flags(bits, 1);
  }
  hdlc_append_flags(bits, post_flags - 1);
  return bits;
}

void nrzi_encode(std::vector<uint8_t>& bits, uint8_t* level) {
  uint8_t l = *level ? 1 : 0;
  for (uint8_t& b : bits) {
    if (!b) l ^= 1;
    b = l;
  }
  *level = l;
}

void HdlcDecoder::init(size_t min_len, size_t max_len) {
  min_len_ = min_len < 1 ? 1 : min_len;
  max_len_ = max_len < min_len_ ? min_len_ : max_len;
  buf_.assign(max_len_ + 2, 0);
  reset();
}

void HdlcDecoder::reset() {
  pat_ = acc_ = 0;
  nacc_ = 0;
  len_ = frame_len_ = 0;
  collecting_ = false;
  since_flag_ = 0xFFFF;
}

bool HdlcDecoder::push(uint8_t bit) {
  bit &= 1;
  frame_len_ = 0;
  pat_ = static_cast<uint8_t>((pat_ >> 1) | (bit << 7));
  if (since_flag_ < 0xFFFF) ++since_flag_;
  if (pat_ == 0x7E) {
    bool good = false;
    // The first 7 bits of the closing flag were shifted into acc_ (nacc_ == 7) if the frame ended
    // on a byte boundary.
    if (collecting_ && nacc_ == 7 && len_ >= min_len_ + 2) {
      if (crc16_x25(ByteView(buf_.data(), len_)) == 0xF0B8) {
        frame_len_ = len_ - 2;
        good = true;
      } else {
        ++fcs_errors_;
      }
    }
    collecting_ = true;
    len_ = 0;
    nacc_ = 0;
    acc_ = 0;
    since_flag_ = 0;
    return good;
  }
  if (!collecting_) return false;
  if (pat_ == 0xFE) {  // seven ones: abort
    collecting_ = false;
    len_ = 0;
    return false;
  }
  if ((pat_ & 0xFC) == 0x7C) return false;  // a 0 after five 1s: stuffed bit
  acc_ = static_cast<uint8_t>((acc_ >> 1) | (bit << 7));
  if (++nacc_ == 8) {
    if (len_ >= buf_.size()) {  // too long: wait for the next flag
      collecting_ = false;
      len_ = 0;
      return false;
    }
    buf_[len_++] = acc_;
    nacc_ = 0;
  }
  return false;
}

}  // namespace modem
}  // namespace ember
