#include "framing/mqtt.h"

#include "base/json.h"

namespace ember::mqtt {

namespace {

void put_len(Bytes& o, size_t n) {
  do {
    uint8_t b = static_cast<uint8_t>(n % 128);
    n /= 128;
    if (n) b |= 0x80;
    o.push_back(b);
  } while (n);
}

void put16(Bytes& o, uint16_t v) {
  o.push_back(static_cast<uint8_t>(v >> 8));
  o.push_back(static_cast<uint8_t>(v));
}

void put_str(Bytes& o, const std::string& s) {
  put16(o, static_cast<uint16_t>(s.size()));
  o.insert(o.end(), s.begin(), s.end());
}

Bytes frame(uint8_t header, const Bytes& body) {
  Bytes o;
  o.reserve(body.size() + 5);
  o.push_back(header);
  put_len(o, body.size());
  o.insert(o.end(), body.begin(), body.end());
  return o;
}

bool str_ok(const std::string& s) {
  if (s.size() > 65535) return false;
  for (char c : s)
    if (c == '\0') return false;
  return utf8_valid(s);
}

// Reader over a packet body.
struct R {
  ByteView v;
  size_t p = 0;
  bool ok = true;
  size_t left() const { return v.size() - p; }
  uint8_t u8() {
    if (left() < 1) { ok = false; return 0; }
    return v[p++];
  }
  uint16_t u16() {
    if (left() < 2) { ok = false; return 0; }
    uint16_t x = static_cast<uint16_t>(v[p] << 8 | v[p + 1]);
    p += 2;
    return x;
  }
  bool str(std::string* out, size_t max) {
    uint16_t n = u16();
    if (!ok || n > left() || n > max) { ok = false; return false; }
    out->assign(reinterpret_cast<const char*>(v.data() + p), n);
    p += n;
    if (!str_ok(*out)) { ok = false; return false; }
    return true;
  }
  bool bin(std::string* out, size_t max) {  // CONNECT password: binary data
    uint16_t n = u16();
    if (!ok || n > left() || n > max) { ok = false; return false; }
    out->assign(reinterpret_cast<const char*>(v.data() + p), n);
    p += n;
    return true;
  }
};

}  // namespace

bool valid_topic(const std::string& t, bool allow_wildcards) {
  if (t.empty() || t.size() > kMaxTopic || !str_ok(t)) return false;
  for (size_t i = 0; i < t.size(); ++i) {
    char c = t[i];
    if (c != '+' && c != '#') continue;
    if (!allow_wildcards) return false;
    bool start = i == 0 || t[i - 1] == '/';
    if (c == '+' && !(start && (i + 1 == t.size() || t[i + 1] == '/'))) return false;
    if (c == '#' && !(start && i + 1 == t.size())) return false;
  }
  return true;
}

Bytes connect(const std::string& client_id, uint16_t keepalive_s, bool clean_session, const std::string* username,
              const std::string* password) {
  if (client_id.size() > 65535 || !str_ok(client_id)) return Bytes();
  if (username && !str_ok(*username)) return Bytes();
  if (password && (password->size() > 65535 || !username)) return Bytes();  // 3.1.1: password needs a username
  Bytes b;
  put_str(b, "MQTT");
  b.push_back(4);  // protocol level 3.1.1
  uint8_t flags = clean_session ? 0x02 : 0;
  if (username) flags |= 0x80;
  if (password) flags |= 0x40;
  b.push_back(flags);
  put16(b, keepalive_s);
  put_str(b, client_id);
  if (username) put_str(b, *username);
  if (password) put_str(b, *password);
  return frame(0x10, b);
}

Bytes connack(bool session_present, uint8_t rc) {
  Bytes b = {static_cast<uint8_t>(session_present ? 1 : 0), rc};
  return frame(0x20, b);
}

Bytes publish(const std::string& topic, ByteView payload) {
  if (!valid_topic(topic, false) || payload.size() > kMaxRemainingLength - 2 - topic.size()) return Bytes();
  Bytes b;
  b.reserve(2 + topic.size() + payload.size());
  put_str(b, topic);
  b.insert(b.end(), payload.begin(), payload.end());
  return frame(0x30, b);
}

static Bytes filter_list(uint8_t header, uint16_t pid, const std::vector<std::string>& filters, bool with_qos) {
  if (pid == 0 || filters.empty() || filters.size() > kMaxFilters) return Bytes();
  Bytes b;
  put16(b, pid);
  for (const std::string& f : filters) {
    if (!valid_topic(f, true)) return Bytes();
    put_str(b, f);
    if (with_qos) b.push_back(0);
  }
  return frame(header, b);
}

Bytes subscribe(uint16_t pid, const std::vector<std::string>& filters) { return filter_list(0x82, pid, filters, true); }
Bytes unsubscribe(uint16_t pid, const std::vector<std::string>& filters) { return filter_list(0xA2, pid, filters, false); }

Bytes suback(uint16_t pid, const std::vector<uint8_t>& codes) {
  if (codes.empty()) return Bytes();
  Bytes b;
  put16(b, pid);
  b.insert(b.end(), codes.begin(), codes.end());
  return frame(0x90, b);
}

Bytes unsuback(uint16_t pid) {
  Bytes b;
  put16(b, pid);
  return frame(0xB0, b);
}

Bytes pingreq() { return Bytes{0xC0, 0x00}; }
Bytes pingresp() { return Bytes{0xD0, 0x00}; }
Bytes disconnect() { return Bytes{0xE0, 0x00}; }

bool parse(uint8_t header, ByteView body, Packet* out, const char** error) {
  auto bad = [&](const char* why) {
    if (error) *error = why;
    return false;
  };
  Packet p;
  uint8_t t = header >> 4, flags = header & 0x0F;
  if (t < 1 || t > 14) return bad("reserved packet type");
  p.type = static_cast<Type>(t);
  p.flags = flags;
  R r{body};
  switch (p.type) {
    case Type::Connect: {
      if (flags) return bad("CONNECT flags");
      std::string proto;
      if (!r.str(&proto, 4) || proto != "MQTT") return bad("CONNECT protocol name");
      uint8_t level = r.u8(), cf = r.u8();
      p.keepalive = r.u16();
      if (!r.ok || level != 4) return bad("CONNECT protocol level");
      if (cf & 0x01) return bad("CONNECT reserved flag");
      if (cf & 0x04) return bad("CONNECT will not supported");
      if ((cf & 0x18) || (cf & 0x20)) return bad("CONNECT will flags without will");
      p.clean_session = (cf & 0x02) != 0;
      p.has_username = (cf & 0x80) != 0;
      p.has_password = (cf & 0x40) != 0;
      if (p.has_password && !p.has_username) return bad("CONNECT password without user name");
      if (!r.str(&p.client_id, 256)) return bad("CONNECT client id");
      if (p.has_username && !r.str(&p.username, 256)) return bad("CONNECT user name");
      if (p.has_password && !r.bin(&p.password, 256)) return bad("CONNECT password");
      if (r.left()) return bad("CONNECT trailing bytes");
      break;
    }
    case Type::Connack: {
      if (flags || body.size() != 2) return bad("CONNACK");
      uint8_t sp = r.u8();
      if (sp & 0xFE) return bad("CONNACK flags");
      p.session_present = sp != 0;
      p.return_code = r.u8();
      break;
    }
    case Type::Publish: {
      p.qos = (flags >> 1) & 3;
      p.dup = (flags & 8) != 0;
      p.retain = (flags & 1) != 0;
      if (p.qos == 3) return bad("PUBLISH QoS 3");
      if (p.qos == 0 && p.dup) return bad("PUBLISH DUP with QoS 0");
      if (!r.str(&p.topic, kMaxTopic) || !valid_topic(p.topic, false)) return bad("PUBLISH topic");
      if (p.qos) {
        p.packet_id = r.u16();
        if (!r.ok || p.packet_id == 0) return bad("PUBLISH packet id");
      }
      p.payload.assign(body.begin() + static_cast<long>(r.p), body.end());
      break;
    }
    case Type::Puback: case Type::Pubrec: case Type::Pubcomp: case Type::Unsuback:
    case Type::Pubrel: {
      uint8_t want = p.type == Type::Pubrel ? 2 : 0;
      if (flags != want || body.size() != 2) return bad("ack packet");
      p.packet_id = r.u16();
      break;
    }
    case Type::Subscribe: case Type::Unsubscribe: {
      if (flags != 2) return bad("SUBSCRIBE flags");
      p.packet_id = r.u16();
      if (!r.ok || p.packet_id == 0) return bad("SUBSCRIBE packet id");
      while (r.left()) {
        if (p.filters.size() >= kMaxFilters) return bad("too many filters");
        std::string f;
        if (!r.str(&f, kMaxTopic) || !valid_topic(f, true)) return bad("topic filter");
        uint8_t q = 0;
        if (p.type == Type::Subscribe) {
          q = r.u8();
          if (!r.ok || q > 2) return bad("requested QoS");
        }
        p.filters.emplace_back(std::move(f), q);
      }
      if (p.filters.empty()) return bad("no topic filter");
      break;
    }
    case Type::Suback: {
      if (flags) return bad("SUBACK flags");
      p.packet_id = r.u16();
      if (!r.ok || !r.left()) return bad("SUBACK");
      while (r.left()) {
        uint8_t c = r.u8();
        if (c > 2 && c != 0x80) return bad("SUBACK return code");
        p.codes.push_back(c);
        if (p.codes.size() > kMaxFilters) return bad("SUBACK too long");
      }
      break;
    }
    case Type::Pingreq: case Type::Pingresp: case Type::Disconnect:
      if (flags || !body.empty()) return bad("control packet");
      break;
  }
  if (!r.ok) return bad("truncated");
  *out = std::move(p);
  return true;
}

bool Decoder::fail(const char* why) {
  failed_ = true;
  error_ = why;
  buf_.clear();
  head_ = 0;
  return false;
}

void Decoder::reset() {
  buf_.clear();
  head_ = 0;
  failed_ = false;
  error_ = "";
}

bool Decoder::feed(ByteView in) {
  if (failed_) return false;
  if (head_ > 0 && head_ >= buf_.size() / 2) {
    buf_.erase(buf_.begin(), buf_.begin() + static_cast<long>(head_));
    head_ = 0;
  }
  if (buf_.size() - head_ + in.size() > max_ + 5 + 64 * 1024) {
    // More than a whole packet plus a generous read-ahead unparsed: the peer floods us.
    return fail("receive buffer overflow");
  }
  buf_.insert(buf_.end(), in.begin(), in.end());
  return true;
}

bool Decoder::next(Packet* out) {
  if (failed_) return false;
  size_t avail = buf_.size() - head_;
  if (avail < 2) return false;
  const uint8_t* p = buf_.data() + head_;
  size_t len = 0, mul = 1, i = 1;
  for (;; ++i) {
    if (i >= avail) return false;  // length still incomplete
    if (i > 4) return fail("remaining length longer than 4 bytes");
    uint8_t b = p[i];
    len += static_cast<size_t>(b & 0x7F) * mul;
    mul *= 128;
    if (!(b & 0x80)) {
      if (i > 1 && b == 0) return fail("non-minimal remaining length");
      break;
    }
  }
  if (len > max_) return fail("packet too large");
  if (avail < i + 1 + len) return false;
  const char* err = "malformed";
  Packet pk;
  if (!parse(p[0], ByteView(p + i + 1, len), &pk, &err)) return fail(err);
  head_ += i + 1 + len;
  if (head_ == buf_.size()) {
    buf_.clear();
    head_ = 0;
  }
  *out = std::move(pk);
  return true;
}

}  // namespace ember::mqtt
