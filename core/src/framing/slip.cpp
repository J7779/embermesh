#include "framing/slip.h"

namespace ember::slip {

void escape_append(ByteView in, Bytes& out) {
  for (uint8_t b : in) {
    if (b == END) {
      out.push_back(ESC);
      out.push_back(ESC_END);
    } else if (b == ESC) {
      out.push_back(ESC);
      out.push_back(ESC_ESC);
    } else {
      out.push_back(b);
    }
  }
}

void encode_append(ByteView payload, Bytes& out, bool leading_end) {
  out.reserve(out.size() + payload.size() + payload.size() / 16 + 2);
  if (leading_end) out.push_back(END);
  escape_append(payload, out);
  out.push_back(END);
}

Bytes encode(ByteView payload, bool leading_end) {
  Bytes out;
  encode_append(payload, out, leading_end);
  return out;
}

bool unescape(ByteView in, Bytes& out) {
  out.clear();
  out.reserve(in.size());
  for (size_t i = 0; i < in.size(); ++i) {
    uint8_t b = in[i];
    if (b == END) { out.clear(); return false; }
    if (b == ESC) {
      if (i + 1 >= in.size()) { out.clear(); return false; }
      uint8_t n = in[++i];
      if (n == ESC_END) out.push_back(END);
      else if (n == ESC_ESC) out.push_back(ESC);
      else { out.clear(); return false; }
    } else {
      out.push_back(b);
    }
  }
  return true;
}

Decoder::Decoder(size_t max_frame, bool require_leading_end)
    : max_(max_frame), require_leading_(require_leading_end),
      st_(require_leading_end ? State::Hunt : State::Data) {}

void Decoder::reset() {
  buf_.clear();
  clear_pending_ = false;
  st_ = require_leading_ ? State::Hunt : State::Data;
}

size_t Decoder::feed_some(ByteView in, bool* frame_ready) {
  *frame_ready = false;
  if (clear_pending_) {
    buf_.clear();
    clear_pending_ = false;
  }
  size_t i = 0;
  while (i < in.size()) {
    uint8_t b = in[i++];
    switch (st_) {
      case State::Hunt:
        if (b == END) st_ = State::Data;
        else ++stats_.discarded_bytes;
        break;
      case State::Discard:
        if (b == END) { st_ = State::Data; buf_.clear(); }
        else ++stats_.discarded_bytes;
        break;
      case State::Escape:
        if (b == END) {
          // ESC END: the frame was cut short; END still starts a new frame.
          ++stats_.bad_escape;
          stats_.discarded_bytes += buf_.size();
          buf_.clear();
          st_ = State::Data;
          break;
        }
        if (b != ESC_END && b != ESC_ESC) {
          ++stats_.bad_escape;
          stats_.discarded_bytes += buf_.size() + 1;
          buf_.clear();
          st_ = State::Discard;
          break;
        }
        st_ = State::Data;
        b = (b == ESC_END) ? END : ESC;
        if (buf_.size() >= max_) {
          ++stats_.oversize;
          stats_.discarded_bytes += buf_.size() + 1;
          buf_.clear();
          st_ = State::Discard;
          break;
        }
        buf_.push_back(b);
        break;
      case State::Data:
        if (b == END) {
          if (!buf_.empty()) {
            ++stats_.frames;
            clear_pending_ = true;
            *frame_ready = true;
            return i;
          }
          break;  // back-to-back END: empty frame, skip
        }
        if (b == ESC) { st_ = State::Escape; break; }
        if (buf_.size() >= max_) {
          ++stats_.oversize;
          stats_.discarded_bytes += buf_.size() + 1;
          buf_.clear();
          st_ = State::Discard;
          break;
        }
        buf_.push_back(b);
        break;
    }
  }
  return i;
}

}  // namespace ember::slip
