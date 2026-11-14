#include "host/ws.h"

#include <cstring>

#include "host/util.h"

namespace ember_host {

const char* const kWsGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

std::string ws_encode_frame(uint8_t opcode, const void* data, size_t len, bool fin, const uint8_t* mask) {
  std::string f;
  f.reserve(len + 14);
  f += char((fin ? 0x80 : 0) | (opcode & 0x0F));
  uint8_t mbit = mask ? 0x80 : 0;
  if (len < 126) {
    f += char(mbit | uint8_t(len));
  } else if (len <= 0xFFFF) {
    f += char(mbit | 126);
    f += char((len >> 8) & 0xFF);
    f += char(len & 0xFF);
  } else {
    f += char(mbit | 127);
    for (int i = 7; i >= 0; --i) f += char((uint64_t(len) >> (8 * i)) & 0xFF);
  }
  const uint8_t* p = static_cast<const uint8_t*>(data);
  if (mask) {
    f.append(reinterpret_cast<const char*>(mask), 4);
    size_t base = f.size();
    f.resize(base + len);
    for (size_t i = 0; i < len; ++i) f[base + i] = char(p[i] ^ mask[i & 3]);
  } else if (len) {
    f.append(reinterpret_cast<const char*>(p), len);
  }
  return f;
}

std::string ws_close_payload(uint16_t code, std::string_view reason) {
  std::string p;
  p += char(code >> 8);
  p += char(code & 0xFF);
  if (reason.size() > 123) reason = reason.substr(0, 123);
  // Never cut a UTF-8 sequence in half.
  while (!reason.empty() && !utf8_valid(reason)) reason.remove_suffix(1);
  p.append(reason.data(), reason.size());
  return p;
}

bool ws_close_code_valid(uint16_t c) {
  if (c >= 1000 && c <= 1003) return true;
  if (c >= 1007 && c <= 1014) return true;
  if (c >= 3000 && c <= 4999) return true;
  return false;
}

void WsDecoder::feed(const uint8_t* data, size_t len) {
  if (dead_) return;
  if (off_ > 0 && off_ == buf_.size()) {
    buf_.clear();
    off_ = 0;
  } else if (off_ > 65536) {
    buf_.erase(0, off_);
    off_ = 0;
  }
  buf_.append(reinterpret_cast<const char*>(data), len);
}

WsEvent WsDecoder::fail(uint16_t code, const char* msg) {
  dead_ = true;
  dead_event_ = WsEvent();
  dead_event_.kind = WsEvent::Error;
  dead_event_.close_code = code;
  dead_event_.error = msg;
  buf_.clear();
  off_ = 0;
  msg_.clear();
  return dead_event_;
}

WsEvent WsDecoder::next() {
  if (dead_) return dead_event_;
  while (true) {
    size_t avail = buf_.size() - off_;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(buf_.data()) + off_;
    if (avail < 2) return WsEvent();
    bool fin = (p[0] & 0x80) != 0;
    uint8_t rsv = p[0] & 0x70;
    uint8_t op = p[0] & 0x0F;
    bool masked = (p[1] & 0x80) != 0;
    uint64_t plen = p[1] & 0x7F;
    size_t hdr = 2;
    if (rsv) return fail(kWsCloseProtocol, "reserved bits set (no extension negotiated)");
    bool control = (op & 0x08) != 0;
    if (!(op == kWsCont || op == kWsText || op == kWsBinary || op == kWsClose || op == kWsPing || op == kWsPong))
      return fail(kWsCloseProtocol, "unknown opcode");
    if (control && !fin) return fail(kWsCloseProtocol, "fragmented control frame");
    if (control && plen > 125) return fail(kWsCloseProtocol, "control frame too long");
    if (masked != opt_.expect_masked)
      return fail(kWsCloseProtocol, opt_.expect_masked ? "client frame not masked" : "server frame masked");
    if (plen == 126) {
      if (avail < 4) return WsEvent();
      plen = (uint64_t(p[2]) << 8) | p[3];
      hdr = 4;
    } else if (plen == 127) {
      if (avail < 10) return WsEvent();
      plen = 0;
      for (int i = 0; i < 8; ++i) plen = (plen << 8) | p[2 + i];
      if (plen >> 63) return fail(kWsCloseProtocol, "64-bit length MSB set");
      hdr = 10;
    }
    // Size limits are checked before buffering the payload.
    if (!control) {
      uint64_t total = (op == kWsCont ? msg_.size() : 0) + plen;
      if (total > opt_.max_message) return fail(kWsCloseTooBig, "message too big");
    }
    if (masked) hdr += 4;
    if (avail < hdr + plen) return WsEvent();
    std::string payload(reinterpret_cast<const char*>(p + hdr), size_t(plen));
    if (masked) {
      const uint8_t* mk = p + hdr - 4;
      for (size_t i = 0; i < payload.size(); ++i) payload[i] = char(uint8_t(payload[i]) ^ mk[i & 3]);
    }
    off_ += hdr + size_t(plen);

    if (control) {
      WsEvent ev;
      if (op == kWsPing) {
        ev.kind = WsEvent::Ping;
        ev.payload = std::move(payload);
        return ev;
      }
      if (op == kWsPong) {
        ev.kind = WsEvent::Pong;
        ev.payload = std::move(payload);
        return ev;
      }
      // Close
      ev.kind = WsEvent::Close;
      if (payload.empty()) {
        ev.close_code = kWsCloseNoStatus;
      } else if (payload.size() == 1) {
        return fail(kWsCloseProtocol, "close frame with 1-byte payload");
      } else {
        uint16_t code = uint16_t((uint8_t(payload[0]) << 8) | uint8_t(payload[1]));
        if (!ws_close_code_valid(code)) return fail(kWsCloseProtocol, "invalid close code");
        std::string reason = payload.substr(2);
        if (!utf8_valid(reason)) return fail(kWsCloseBadData, "close reason not UTF-8");
        ev.close_code = code;
        ev.payload = std::move(reason);
      }
      return ev;
    }

    if (op == kWsCont) {
      if (!in_message_) return fail(kWsCloseProtocol, "continuation without a message");
      msg_ += payload;
    } else {
      if (in_message_) return fail(kWsCloseProtocol, "new message inside a fragmented message");
      in_message_ = true;
      msg_opcode_ = op;
      msg_ = std::move(payload);
    }
    if (!fin) continue;
    in_message_ = false;
    WsEvent ev;
    ev.kind = WsEvent::Message;
    ev.opcode = msg_opcode_;
    ev.payload = std::move(msg_);
    msg_.clear();
    if (ev.opcode == kWsText && !utf8_valid(ev.payload)) return fail(kWsCloseBadData, "text message not UTF-8");
    return ev;
  }
}

// ---- handshake -----------------------------------------------------------------------------------

std::string ws_accept_key(std::string_view client_key) {
  std::string s(client_key);
  s += kWsGuid;
  std::string d = sha1_raw(s);
  return base64_encode(d.data(), d.size());
}

bool ws_is_upgrade_request(const HttpRequest& req) {
  const std::string* up = req.header("upgrade");
  return up && header_has_token(*up, "websocket");
}

int ws_validate_upgrade(const HttpRequest& req, std::string* why) {
  auto bad = [why](const char* m) {
    if (why) *why = m;
    return 400;
  };
  if (req.method != "GET") return bad("WebSocket upgrade requires GET");
  if (req.version_minor < 1) return bad("WebSocket upgrade requires HTTP/1.1");
  if (!ws_is_upgrade_request(req)) return bad("missing Upgrade: websocket");
  if (!header_has_token(req.header_or("connection"), "upgrade")) return bad("missing Connection: Upgrade");
  if (trim(req.header_or("sec-websocket-version")) != "13") {
    if (why) *why = "unsupported WebSocket version";
    return 426;
  }
  std::string key = trim(req.header_or("sec-websocket-key")), raw;
  if (key.empty() || !base64_decode(key, &raw) || raw.size() != 16) return bad("bad Sec-WebSocket-Key");
  return 0;
}

std::string ws_server_handshake_response(const HttpRequest& req) {
  std::string key = trim(req.header_or("sec-websocket-key"));
  std::string r = "HTTP/1.1 101 Switching Protocols\r\n";
  r += "Upgrade: websocket\r\nConnection: Upgrade\r\n";
  r += "Sec-WebSocket-Accept: " + ws_accept_key(key) + "\r\n";
  r += "Server: emberd\r\n\r\n";
  return r;
}

std::string ws_make_client_key() {
  uint8_t k[16];
  random_bytes(k, sizeof k);
  return base64_encode(k, sizeof k);
}

std::string ws_client_handshake_request(const std::string& host_header, const std::string& path_and_query,
                                        const std::string& key, const std::string& origin) {
  std::string r = "GET " + (path_and_query.empty() ? std::string("/") : path_and_query) + " HTTP/1.1\r\n";
  r += "Host: " + host_header + "\r\n";
  r += "Upgrade: websocket\r\nConnection: Upgrade\r\n";
  r += "Sec-WebSocket-Key: " + key + "\r\n";
  r += "Sec-WebSocket-Version: 13\r\n";
  if (!origin.empty()) r += "Origin: " + origin + "\r\n";
  r += "User-Agent: emberd\r\n\r\n";
  return r;
}

bool ws_check_server_response(std::string_view head, const std::string& key, std::string* why) {
  auto bad = [why](const std::string& m) {
    if (why) *why = m;
    return false;
  };
  size_t eol = head.find("\r\n");
  if (eol == std::string_view::npos) return bad("truncated response");
  std::string status_line(head.substr(0, eol));
  if (!starts_with(status_line, "HTTP/1.1 101")) return bad("server refused upgrade: " + status_line);
  bool upgrade = false, connection = false, accept_ok = false;
  std::string expect = ws_accept_key(key);
  for (const auto& line : split(head.substr(eol + 2), '\n')) {
    std::string l = trim(line);
    if (l.empty()) continue;
    size_t c = l.find(':');
    if (c == std::string::npos) continue;
    std::string n = to_lower(trim(std::string_view(l).substr(0, c)));
    std::string v = trim(std::string_view(l).substr(c + 1));
    if (n == "upgrade" && header_has_token(v, "websocket")) upgrade = true;
    if (n == "connection" && header_has_token(v, "upgrade")) connection = true;
    if (n == "sec-websocket-accept" && v == expect) accept_ok = true;
    if (n == "sec-websocket-extensions" && !v.empty()) return bad("server negotiated an extension we did not offer");
  }
  if (!upgrade || !connection) return bad("missing Upgrade/Connection headers");
  if (!accept_ok) return bad("bad Sec-WebSocket-Accept");
  return true;
}

bool ws_parse_url(const std::string& url, std::string* host, uint16_t* port, std::string* path, std::string* err) {
  std::string u = trim(url);
  size_t pos;
  uint16_t def_port = 80;
  if (starts_with(u, "ws://")) {
    pos = 5;
  } else if (starts_with(u, "http://")) {
    pos = 7;
  } else if (starts_with(u, "wss://") || starts_with(u, "https://")) {
    if (err) *err = "wss:// (TLS) is not supported; use ws:// on the local network";
    return false;
  } else {
    if (err) *err = "URL must start with ws://";
    return false;
  }
  size_t slash = u.find('/', pos);
  std::string hostport = u.substr(pos, slash == std::string::npos ? std::string::npos : slash - pos);
  *path = slash == std::string::npos ? std::string("/") : u.substr(slash);
  if (hostport.empty()) {
    if (err) *err = "missing host";
    return false;
  }
  std::string h = hostport, p;
  if (hostport[0] == '[') {
    size_t rb = hostport.find(']');
    if (rb == std::string::npos) {
      if (err) *err = "bad IPv6 literal";
      return false;
    }
    h = hostport.substr(1, rb - 1);
    if (rb + 1 < hostport.size()) {
      if (hostport[rb + 1] != ':') {
        if (err) *err = "bad host";
        return false;
      }
      p = hostport.substr(rb + 2);
    }
  } else {
    size_t c = hostport.rfind(':');
    if (c != std::string::npos) {
      h = hostport.substr(0, c);
      p = hostport.substr(c + 1);
    }
  }
  uint64_t pv = def_port;
  if (!p.empty() && (!parse_u64(p, &pv) || pv == 0 || pv > 65535)) {
    if (err) *err = "bad port";
    return false;
  }
  if (h.empty()) {
    if (err) *err = "missing host";
    return false;
  }
  *host = h;
  *port = uint16_t(pv);
  return true;
}

}  // namespace ember_host
