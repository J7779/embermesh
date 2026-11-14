#include "framing/kiss.h"

namespace ember::kiss {

bool encode_append(ByteView payload, Bytes& out, uint8_t port, Command cmd) {
  if (cmd == Command::Return) {
    out.push_back(FEND);
    out.push_back(0xFF);
    out.push_back(FEND);
    return true;
  }
  if (port > 15) return false;
  uint8_t type = static_cast<uint8_t>((port << 4) | (static_cast<uint8_t>(cmd) & 0x0F));
  out.reserve(out.size() + payload.size() + payload.size() / 16 + 4);
  out.push_back(FEND);
  slip::escape_append(ByteView(&type, 1), out);
  slip::escape_append(payload, out);
  out.push_back(FEND);
  return true;
}

Bytes encode(ByteView payload, uint8_t port, Command cmd) {
  Bytes out;
  encode_append(payload, out, port, cmd);
  return out;
}

Bytes encode_param(Command cmd, uint8_t value, uint8_t port) {
  return encode(ByteView(&value, 1), port, cmd);
}

Bytes encode_return() { return encode(ByteView(), 0, Command::Return); }

Decoder::Decoder(size_t max_frame, bool require_leading_fend)
    : slip_(max_frame + 1, require_leading_fend) {}

size_t Decoder::feed_some(ByteView in, bool* frame_ready) {
  size_t n = slip_.feed_some(in, frame_ready);
  if (*frame_ready) {
    ByteView f = slip_.frame();  // non-empty by construction
    uint8_t type = f[0];
    if (type == 0xFF) {
      frame_.port = 0;
      frame_.command = Command::Return;
    } else {
      frame_.port = static_cast<uint8_t>(type >> 4);
      frame_.command = static_cast<Command>(type & 0x0F);
    }
    frame_.payload = f.sub(1);
  }
  return n;
}

DecoderStats Decoder::stats() const {
  const slip::DecoderStats& s = slip_.stats();
  DecoderStats o;
  o.frames = s.frames;
  o.oversize = s.oversize;
  o.bad_escape = s.bad_escape;
  o.discarded_bytes = s.discarded_bytes;
  return o;
}

}  // namespace ember::kiss
