#include "framing/cobs.h"

namespace ember::cobs {

void encode_append(ByteView in, Bytes& out) {
  out.reserve(out.size() + max_encoded_size(in.size()));
  size_t code_idx = out.size();
  out.push_back(0);
  uint8_t code = 1;
  for (uint8_t b : in) {
    if (b == 0) {
      out[code_idx] = code;
      code_idx = out.size();
      out.push_back(0);
      code = 1;
    } else {
      out.push_back(b);
      if (++code == 0xFF) {
        out[code_idx] = code;
        code_idx = out.size();
        out.push_back(0);
        code = 1;
      }
    }
  }
  out[code_idx] = code;
}

Bytes encode(ByteView in) {
  Bytes out;
  encode_append(in, out);
  return out;
}

Bytes encode_frame(ByteView in, bool leading_delimiter) {
  Bytes out;
  if (leading_delimiter) out.push_back(0);
  encode_append(in, out);
  out.push_back(0);
  return out;
}

bool decode(ByteView in, Bytes& out) {
  out.clear();
  if (in.empty()) return false;
  out.reserve(in.size());
  size_t i = 0;
  const size_t n = in.size();
  while (i < n) {
    uint8_t code = in[i++];
    if (code == 0) { out.clear(); return false; }
    size_t run = static_cast<size_t>(code) - 1;
    if (run > n - i) { out.clear(); return false; }
    for (size_t j = 0; j < run; ++j) {
      uint8_t b = in[i + j];
      if (b == 0) { out.clear(); return false; }
      out.push_back(b);
    }
    i += run;
    if (code != 0xFF && i < n) out.push_back(0);
  }
  return true;
}

Decoder::Decoder(size_t max_frame) : max_(max_frame) {}

size_t Decoder::feed_some(ByteView in, bool* frame_ready) {
  *frame_ready = false;
  const size_t cap = max_encoded_size(max_);
  size_t i = 0;
  while (i < in.size()) {
    uint8_t b = in[i++];
    if (b == 0) {
      if (discarding_) {
        discarding_ = false;
        continue;
      }
      if (enc_.empty()) continue;
      bool ok = decode(ByteView(enc_), out_) && out_.size() <= max_;
      if (!ok) {
        ++stats_.invalid;
        stats_.discarded_bytes += enc_.size();
        enc_.clear();
        continue;
      }
      enc_.clear();
      ++stats_.frames;
      *frame_ready = true;
      return i;
    }
    if (discarding_) { ++stats_.discarded_bytes; continue; }
    if (enc_.size() >= cap) {
      ++stats_.oversize;
      stats_.discarded_bytes += enc_.size() + 1;
      enc_.clear();
      discarding_ = true;
      continue;
    }
    enc_.push_back(b);
  }
  return i;
}

}  // namespace ember::cobs
