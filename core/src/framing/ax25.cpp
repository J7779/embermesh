#include "framing/ax25.h"

#include "ember/crc.h"

namespace ember::ax25 {

static bool is_call_char(char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }

bool Address::valid() const {
  if (call.empty() || call.size() > kMaxCallLen || ssid > 15) return false;
  for (char c : call)
    if (!is_call_char(c)) return false;
  return true;
}

std::string Address::to_string() const {
  std::string s = call;
  if (ssid) {
    s.push_back('-');
    if (ssid >= 10) s.push_back('1');
    s.push_back(static_cast<char>('0' + ssid % 10));
  }
  return s;
}

bool parse_address(std::string_view text, Address* out) {
  Address a;
  if (!text.empty() && text.back() == '*') {
    a.flag = true;
    text.remove_suffix(1);
  }
  size_t dash = text.find('-');
  std::string_view call = dash == std::string_view::npos ? text : text.substr(0, dash);
  if (call.empty() || call.size() > kMaxCallLen) return false;
  for (char c : call) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (!is_call_char(c)) return false;
    a.call.push_back(c);
  }
  if (dash != std::string_view::npos) {
    std::string_view s = text.substr(dash + 1);
    if (s.empty() || s.size() > 2) return false;
    unsigned v = 0;
    for (char c : s) {
      if (c < '0' || c > '9') return false;
      v = v * 10 + static_cast<unsigned>(c - '0');
    }
    if (v > 15 || (s.size() == 2 && s[0] == '0')) return false;
    a.ssid = static_cast<uint8_t>(v);
  }
  *out = a;
  return true;
}

Address make_address(std::string_view text) {
  Address a;
  if (!parse_address(text, &a)) return Address();
  return a;
}

bool encode_address(const Address& a, bool last, uint8_t out[kAddrLen]) {
  if (!a.valid()) return false;
  for (size_t i = 0; i < kMaxCallLen; ++i) {
    char c = i < a.call.size() ? a.call[i] : ' ';
    out[i] = static_cast<uint8_t>(static_cast<uint8_t>(c) << 1);
  }
  out[6] = static_cast<uint8_t>((a.flag ? 0x80 : 0) | ((a.reserved & 3) << 5) | (a.ssid << 1) | (last ? 1 : 0));
  return true;
}

bool decode_address(const uint8_t in[kAddrLen], Address* out, bool* last) {
  Address a;
  bool seen_space = false;
  for (size_t i = 0; i < kMaxCallLen; ++i) {
    if (in[i] & 1) return false;  // extension bit inside the callsign
    char c = static_cast<char>(in[i] >> 1);
    if (c == ' ') {
      seen_space = true;
      continue;
    }
    if (seen_space || !is_call_char(c)) return false;  // embedded space or invalid char
    a.call.push_back(c);
  }
  if (a.call.empty()) return false;
  uint8_t s = in[6];
  a.flag = (s & 0x80) != 0;
  a.reserved = static_cast<uint8_t>((s >> 5) & 3);
  a.ssid = static_cast<uint8_t>((s >> 1) & 0x0F);
  *last = (s & 1) != 0;
  *out = a;
  return true;
}

CommandResponse Frame::command_response() const {
  if (dst.flag && !src.flag) return CommandResponse::Command;
  if (!dst.flag && src.flag) return CommandResponse::Response;
  return CommandResponse::Legacy;
}

int Frame::next_digi() const {
  for (size_t i = 0; i < digis.size(); ++i)
    if (!digis[i].flag) return static_cast<int>(i);
  return -1;
}

Frame make_ui(const Address& dst, const Address& src, const std::vector<Address>& digis, ByteView info,
              uint8_t pid) {
  Frame f;
  f.dst = dst;
  f.src = src;
  f.digis = digis;
  f.control = kControlUI;
  f.pid = pid;
  f.info = info.to_bytes();
  f.set_command(true);
  return f;
}

bool encode(const Frame& f, Bytes* out) {
  out->clear();
  if (f.digis.size() > kMaxDigis) return false;
  out->resize(kAddrLen * (2 + f.digis.size()));
  uint8_t* p = out->data();
  bool ok = encode_address(f.dst, false, p) && encode_address(f.src, f.digis.empty(), p + kAddrLen);
  for (size_t i = 0; ok && i < f.digis.size(); ++i)
    ok = encode_address(f.digis[i], i + 1 == f.digis.size(), p + kAddrLen * (2 + i));
  if (!ok) {
    out->clear();
    return false;
  }
  out->push_back(f.control);
  if (f.has_pid()) out->push_back(f.pid);
  out->insert(out->end(), f.info.begin(), f.info.end());
  return true;
}

Bytes encode(const Frame& f) {
  Bytes out;
  encode(f, &out);
  return out;
}

bool decode(ByteView in, Frame* out, size_t max_info) {
  Frame f;
  size_t pos = 0;
  size_t naddr = 0;
  bool last = false;
  while (!last) {
    if (naddr >= 2 + kMaxDigis || in.size() - pos < kAddrLen) return false;
    Address a;
    if (!decode_address(in.data() + pos, &a, &last)) return false;
    pos += kAddrLen;
    if (naddr == 0) {
      if (last) return false;  // need at least dst + src
      f.dst = a;
    } else if (naddr == 1) {
      f.src = a;
    } else {
      f.digis.push_back(a);
    }
    ++naddr;
  }
  if (pos >= in.size()) return false;  // no control byte
  f.control = in[pos++];
  if (f.has_pid()) {
    if (pos >= in.size()) return false;
    f.pid = in[pos++];
  } else {
    f.pid = 0;
  }
  if (in.size() - pos > max_info) return false;
  f.info.assign(in.begin() + pos, in.end());
  *out = std::move(f);
  return true;
}

uint16_t fcs(ByteView frame) { return crc16_x25_of(frame); }

void append_fcs(Bytes& frame) {
  uint16_t c = fcs(ByteView(frame));
  frame.push_back(static_cast<uint8_t>(c & 0xFF));
  frame.push_back(static_cast<uint8_t>(c >> 8));
}

bool check_fcs(ByteView in) {
  if (in.size() < 3) return false;
  size_t n = in.size() - 2;
  uint16_t c = fcs(in.sub(0, n));
  return in[n] == static_cast<uint8_t>(c & 0xFF) && in[n + 1] == static_cast<uint8_t>(c >> 8);
}

bool decode_with_fcs(ByteView in, Frame* out, size_t max_info) {
  if (!check_fcs(in)) return false;
  return decode(in.sub(0, in.size() - 2), out, max_info);
}

}  // namespace ember::ax25
