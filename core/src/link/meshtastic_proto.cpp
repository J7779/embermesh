#include "link/meshtastic_proto.h"

#include <cmath>
#include <cstring>

#include "base/json.h"

namespace ember {
namespace link {
namespace mt {

namespace {
constexpr uint8_t kVarint = 0, k64 = 1, kLen = 2, k32 = 5;
constexpr size_t kMaxStr = 64;

std::string cap_string(ByteView b) {
  std::string s = utf8_sanitize(std::string_view(reinterpret_cast<const char*>(b.data()), b.size()));
  return std::string(utf8_truncate(s, kMaxStr));
}

float bits_to_float(uint32_t u) {
  float f;
  std::memcpy(&f, &u, 4);
  if (!std::isfinite(f)) f = 0;
  return f;
}
uint32_t float_to_bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}
}  // namespace

// --- reader --------------------------------------------------------------------------------------

bool PbReader::next(uint32_t* field, uint8_t* wire) {
  if (!ok_ || at_end()) return false;
  uint64_t t = varint();
  if (!ok_) return false;
  uint64_t f = t >> 3;
  uint8_t w = static_cast<uint8_t>(t & 7);
  if (f == 0 || f > 0x1FFFFFFF || w == 3 || w == 4 || w > 5) { ok_ = false; return false; }
  *field = static_cast<uint32_t>(f);
  *wire = w;
  return true;
}

uint64_t PbReader::varint() {
  uint64_t out = 0;
  for (int i = 0; i < 10; ++i) {
    if (!ok_ || pos_ >= v_.size()) { ok_ = false; return 0; }
    uint8_t b = v_[pos_++];
    if (i == 9 && (b & 0xFE)) { ok_ = false; return 0; }
    out |= static_cast<uint64_t>(b & 0x7F) << (7 * i);
    if (!(b & 0x80)) return out;
  }
  ok_ = false;
  return 0;
}

uint32_t PbReader::fixed32() {
  if (!ok_ || v_.size() - pos_ < 4) { ok_ = false; return 0; }
  uint32_t v = static_cast<uint32_t>(v_[pos_]) | static_cast<uint32_t>(v_[pos_ + 1]) << 8 |
               static_cast<uint32_t>(v_[pos_ + 2]) << 16 | static_cast<uint32_t>(v_[pos_ + 3]) << 24;
  pos_ += 4;
  return v;
}

uint64_t PbReader::fixed64() {
  uint64_t lo = fixed32();
  uint64_t hi = fixed32();
  return lo | hi << 32;
}

ByteView PbReader::bytes() {
  uint64_t n = varint();
  if (!ok_ || n > v_.size() - pos_) { ok_ = false; return ByteView(); }
  ByteView out = v_.sub(pos_, static_cast<size_t>(n));
  pos_ += static_cast<size_t>(n);
  return out;
}

bool PbReader::skip(uint8_t wire) {
  switch (wire) {
    case kVarint: varint(); break;
    case k64: fixed64(); break;
    case kLen: bytes(); break;
    case k32: fixed32(); break;
    default: ok_ = false;
  }
  return ok_;
}

// --- writer --------------------------------------------------------------------------------------

void pb_varint(Bytes& out, uint64_t v) {
  while (v >= 0x80) { out.push_back(static_cast<uint8_t>(v | 0x80)); v >>= 7; }
  out.push_back(static_cast<uint8_t>(v));
}
void pb_tag(Bytes& out, uint32_t field, uint8_t wire) { pb_varint(out, static_cast<uint64_t>(field) << 3 | wire); }
void pb_uint(Bytes& out, uint32_t field, uint64_t v) {
  if (!v) return;
  pb_tag(out, field, kVarint);
  pb_varint(out, v);
}
void pb_int32(Bytes& out, uint32_t field, int32_t v) {
  if (!v) return;
  pb_tag(out, field, kVarint);
  pb_varint(out, static_cast<uint64_t>(static_cast<int64_t>(v)));
}
void pb_bool(Bytes& out, uint32_t field, bool v) {
  if (!v) return;
  pb_tag(out, field, kVarint);
  out.push_back(1);
}
void pb_fixed32(Bytes& out, uint32_t field, uint32_t v) {
  if (!v) return;
  pb_tag(out, field, k32);
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void pb_float(Bytes& out, uint32_t field, float v) {
  if (v == 0) return;
  pb_tag(out, field, k32);
  uint32_t u = float_to_bits(v);
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(u >> (8 * i)));
}
void pb_bytes(Bytes& out, uint32_t field, ByteView v) {
  pb_tag(out, field, kLen);
  pb_varint(out, v.size());
  out.insert(out.end(), v.begin(), v.end());
}

// --- Data / MeshPacket ---------------------------------------------------------------------------

static Bytes encode_data(const Data& d) {
  Bytes o;
  pb_uint(o, 1, d.portnum);
  pb_bytes(o, 2, ByteView(d.payload));
  pb_bool(o, 3, d.want_response);
  return o;
}

static bool decode_data(ByteView in, Data* d) {
  PbReader r(in);
  uint32_t f;
  uint8_t w;
  while (r.next(&f, &w)) {
    if (f == 1 && w == kVarint) d->portnum = static_cast<uint32_t>(r.varint());
    else if (f == 2 && w == kLen) d->payload = r.bytes().to_bytes();
    else if (f == 3 && w == kVarint) d->want_response = r.varint() != 0;
    else if (f <= 2 || f == 3) return false;  // wrong wire type for a known field
    else r.skip(w);
  }
  return r.ok();
}

Bytes encode_mesh_packet(const MeshPacket& p) {
  Bytes o;
  pb_fixed32(o, 1, p.from);
  pb_fixed32(o, 2, p.to);
  pb_uint(o, 3, p.channel);
  if (p.has_decoded) pb_bytes(o, 4, ByteView(encode_data(p.decoded)));
  pb_fixed32(o, 6, p.id);
  if (p.has_snr) pb_float(o, 8, p.rx_snr);
  pb_uint(o, 9, p.hop_limit);
  pb_bool(o, 10, p.want_ack);
  pb_uint(o, 11, p.priority);
  pb_int32(o, 12, p.rx_rssi);
  pb_bool(o, 14, p.via_mqtt);
  pb_uint(o, 15, p.hop_start);
  return o;
}

bool decode_mesh_packet(ByteView in, MeshPacket* p) {
  *p = MeshPacket();
  PbReader r(in);
  uint32_t f;
  uint8_t w;
  while (r.next(&f, &w)) {
    switch (f) {
      case 1: if (w != k32) return false; p->from = r.fixed32(); break;
      case 2: if (w != k32) return false; p->to = r.fixed32(); break;
      case 3: if (w != kVarint) return false; p->channel = static_cast<uint32_t>(r.varint()); break;
      case 4:
        if (w != kLen) return false;
        {
          ByteView d = r.bytes();
          if (!r.ok() || !decode_data(d, &p->decoded)) return false;
          p->has_decoded = true;
        }
        break;
      case 5: if (w != kLen) return false; r.bytes(); p->encrypted = true; break;
      case 6: if (w != k32) return false; p->id = r.fixed32(); break;
      case 8: if (w != k32) return false; p->rx_snr = bits_to_float(r.fixed32()); p->has_snr = true; break;
      case 9: if (w != kVarint) return false; p->hop_limit = static_cast<uint32_t>(r.varint()); break;
      case 10: if (w != kVarint) return false; p->want_ack = r.varint() != 0; break;
      case 11: if (w != kVarint) return false; p->priority = static_cast<uint32_t>(r.varint()); break;
      case 12: if (w != kVarint) return false; p->rx_rssi = static_cast<int32_t>(static_cast<uint32_t>(r.varint())); break;
      case 14: if (w != kVarint) return false; p->via_mqtt = r.varint() != 0; break;
      case 15: if (w != kVarint) return false; p->hop_start = static_cast<uint32_t>(r.varint()); break;
      default: r.skip(w);
    }
  }
  return r.ok();
}

// --- ToRadio -------------------------------------------------------------------------------------

Bytes encode_to_radio(const ToRadio& t) {
  Bytes o;
  switch (t.kind) {
    case ToKind::Packet: pb_bytes(o, 1, ByteView(encode_mesh_packet(t.packet))); break;
    case ToKind::WantConfig:
      pb_tag(o, 3, kVarint);
      pb_varint(o, t.want_config_id);
      break;
    case ToKind::Disconnect: pb_tag(o, 4, kVarint); o.push_back(1); break;
    case ToKind::Heartbeat: pb_bytes(o, 7, ByteView()); break;
    default: break;
  }
  return o;
}

bool decode_to_radio(ByteView in, ToRadio* t) {
  *t = ToRadio();
  PbReader r(in);
  uint32_t f;
  uint8_t w;
  while (r.next(&f, &w)) {
    if (f == 1 && w == kLen) {
      ByteView p = r.bytes();
      if (!r.ok() || !decode_mesh_packet(p, &t->packet)) return false;
      t->kind = ToKind::Packet;
    } else if (f == 3 && w == kVarint) {
      t->want_config_id = static_cast<uint32_t>(r.varint());
      t->kind = ToKind::WantConfig;
    } else if (f == 4 && w == kVarint) {
      r.varint();
      t->kind = ToKind::Disconnect;
    } else if (f == 7 && w == kLen) {
      r.bytes();
      t->kind = ToKind::Heartbeat;
    } else {
      r.skip(w);
      if (t->kind == ToKind::None) t->kind = ToKind::Other;
    }
  }
  return r.ok();
}

// --- FromRadio -----------------------------------------------------------------------------------

static bool decode_user(ByteView in, NodeInfo* n) {
  PbReader r(in);
  uint32_t f;
  uint8_t w;
  while (r.next(&f, &w)) {
    if ((f == 1 || f == 2 || f == 3) && w == kLen) {
      std::string s = cap_string(r.bytes());
      (f == 1 ? n->user_id : f == 2 ? n->long_name : n->short_name) = s;
    } else if (f == 5 && w == kVarint) {
      n->hw_model = static_cast<uint32_t>(r.varint());
    } else if (f == 6 && w == kVarint) {
      n->is_licensed = r.varint() != 0;
    } else {
      r.skip(w);
    }
  }
  return r.ok();
}

static bool decode_node_info(ByteView in, NodeInfo* n) {
  PbReader r(in);
  uint32_t f;
  uint8_t w;
  while (r.next(&f, &w)) {
    if (f == 1 && w == kVarint) n->num = static_cast<uint32_t>(r.varint());
    else if (f == 2 && w == kLen) { ByteView u = r.bytes(); if (!r.ok() || !decode_user(u, n)) return false; }
    else if (f == 4 && w == k32) { n->snr = bits_to_float(r.fixed32()); n->has_snr = true; }
    else r.skip(w);
  }
  return r.ok();
}

static bool decode_lora(ByteView in, LoraConfig* l) {
  PbReader r(in);
  uint32_t f;
  uint8_t w;
  while (r.next(&f, &w)) {
    if (w == kVarint && f >= 1 && f <= 10 && f != 6) {
      uint64_t v = r.varint();
      switch (f) {
        case 1: l->use_preset = v != 0; break;
        case 2: l->modem_preset = static_cast<uint32_t>(v); break;
        case 3: l->bandwidth = static_cast<uint32_t>(v); break;
        case 4: l->spread_factor = static_cast<uint32_t>(v); break;
        case 5: l->coding_rate = static_cast<uint32_t>(v); break;
        case 7: l->region = static_cast<uint32_t>(v); break;
        case 8: l->hop_limit = static_cast<uint32_t>(v); break;
        case 9: l->tx_enabled = v != 0; break;
        case 10: l->tx_power = static_cast<int32_t>(static_cast<uint32_t>(v)); break;
      }
    } else {
      r.skip(w);
    }
  }
  return r.ok();
}

static bool decode_config(ByteView in, FromRadio* out) {
  PbReader r(in);
  uint32_t f;
  uint8_t w;
  while (r.next(&f, &w)) {
    if (f == 6 && w == kLen) {
      ByteView l = r.bytes();
      if (!r.ok() || !decode_lora(l, &out->lora)) return false;
      out->has_lora = true;
    } else {
      r.skip(w);
    }
  }
  return r.ok();
}

bool decode_from_radio(ByteView in, FromRadio* out) {
  *out = FromRadio();
  PbReader r(in);
  uint32_t f;
  uint8_t w;
  while (r.next(&f, &w)) {
    if (f == 1) {
      if (w != kVarint) return false;
      out->id = static_cast<uint32_t>(r.varint());
      continue;
    }
    bool ok = true;
    switch (f) {
      case 2:
        if (w != kLen) return false;
        { ByteView b = r.bytes(); ok = r.ok() && decode_mesh_packet(b, &out->packet); }
        out->kind = FromKind::Packet;
        break;
      case 3:
        if (w != kLen) return false;
        {
          PbReader m(r.bytes());
          uint32_t g;
          uint8_t x;
          while (m.next(&g, &x)) {
            if (g == 1 && x == kVarint) out->my_node_num = static_cast<uint32_t>(m.varint());
            else m.skip(x);
          }
          ok = r.ok() && m.ok();
        }
        out->kind = FromKind::MyInfo;
        break;
      case 4:
        if (w != kLen) return false;
        { ByteView b = r.bytes(); ok = r.ok() && decode_node_info(b, &out->node); }
        out->kind = FromKind::NodeInfo;
        break;
      case 5:
        if (w != kLen) return false;
        { ByteView b = r.bytes(); ok = r.ok() && decode_config(b, out); }
        out->kind = FromKind::Config;
        break;
      case 7:
        if (w != kVarint) return false;
        out->config_complete_id = static_cast<uint32_t>(r.varint());
        out->kind = FromKind::ConfigComplete;
        break;
      case 8:
        if (w != kVarint) return false;
        r.varint();
        out->kind = FromKind::Rebooted;
        break;
      case 11:
        if (w != kLen) return false;
        {
          PbReader m(r.bytes());
          uint32_t g;
          uint8_t x;
          while (m.next(&g, &x)) {
            if (x != kVarint) { m.skip(x); continue; }
            uint64_t v = m.varint();
            if (g == 1) out->queue.res = static_cast<int32_t>(static_cast<uint32_t>(v));
            else if (g == 2) out->queue.free = static_cast<uint32_t>(v);
            else if (g == 3) out->queue.maxlen = static_cast<uint32_t>(v);
            else if (g == 4) out->queue.mesh_packet_id = static_cast<uint32_t>(v);
          }
          ok = r.ok() && m.ok();
        }
        out->kind = FromKind::QueueStatus;
        break;
      case 13:
        if (w != kLen) return false;
        {
          PbReader m(r.bytes());
          uint32_t g;
          uint8_t x;
          while (m.next(&g, &x)) {
            if (g == 1 && x == kLen) out->firmware_version = cap_string(m.bytes());
            else m.skip(x);
          }
          ok = r.ok() && m.ok();
        }
        out->kind = FromKind::Metadata;
        break;
      default:
        r.skip(w);
        if (out->kind == FromKind::None) {
          out->kind = f == 6 ? FromKind::LogRecord : f == 9 ? FromKind::ModuleConfig
                    : f == 10 ? FromKind::Channel : FromKind::Other;
        }
    }
    if (!ok) return false;
  }
  return r.ok();
}

Bytes encode_from_radio(const FromRadio& f) {
  Bytes o;
  pb_uint(o, 1, f.id);
  switch (f.kind) {
    case FromKind::Packet: pb_bytes(o, 2, ByteView(encode_mesh_packet(f.packet))); break;
    case FromKind::MyInfo: {
      Bytes m;
      pb_uint(m, 1, f.my_node_num);
      pb_bytes(o, 3, ByteView(m));
      break;
    }
    case FromKind::NodeInfo: {
      Bytes u, n;
      if (!f.node.user_id.empty()) pb_bytes(u, 1, ByteView(std::string_view(f.node.user_id)));
      if (!f.node.long_name.empty()) pb_bytes(u, 2, ByteView(std::string_view(f.node.long_name)));
      if (!f.node.short_name.empty()) pb_bytes(u, 3, ByteView(std::string_view(f.node.short_name)));
      pb_uint(u, 5, f.node.hw_model);
      pb_bool(u, 6, f.node.is_licensed);
      pb_uint(n, 1, f.node.num);
      pb_bytes(n, 2, ByteView(u));
      if (f.node.has_snr) pb_float(n, 4, f.node.snr);
      pb_bytes(o, 4, ByteView(n));
      break;
    }
    case FromKind::Config: {
      Bytes l, c;
      pb_bool(l, 1, f.lora.use_preset);
      pb_uint(l, 2, f.lora.modem_preset);
      pb_uint(l, 3, f.lora.bandwidth);
      pb_uint(l, 4, f.lora.spread_factor);
      pb_uint(l, 5, f.lora.coding_rate);
      pb_uint(l, 7, f.lora.region);
      pb_uint(l, 8, f.lora.hop_limit);
      pb_bool(l, 9, f.lora.tx_enabled);
      pb_int32(l, 10, f.lora.tx_power);
      pb_bytes(c, 6, ByteView(l));
      pb_bytes(o, 5, ByteView(c));
      break;
    }
    case FromKind::ConfigComplete:
      pb_tag(o, 7, kVarint);
      pb_varint(o, f.config_complete_id);
      break;
    case FromKind::Rebooted: pb_tag(o, 8, kVarint); o.push_back(1); break;
    case FromKind::QueueStatus: {
      Bytes q;
      pb_int32(q, 1, f.queue.res);
      pb_uint(q, 2, f.queue.free);
      pb_uint(q, 3, f.queue.maxlen);
      pb_uint(q, 4, f.queue.mesh_packet_id);
      pb_bytes(o, 11, ByteView(q));
      break;
    }
    case FromKind::Metadata: {
      Bytes m;
      pb_bytes(m, 1, ByteView(std::string_view(f.firmware_version)));
      pb_bytes(o, 13, ByteView(m));
      break;
    }
    default: break;
  }
  return o;
}

Bytes frame(ByteView payload) {
  Bytes o;
  if (payload.size() > kMaxToFromRadio) return o;
  o.reserve(payload.size() + 4);
  o.push_back(kStart1);
  o.push_back(kStart2);
  o.push_back(static_cast<uint8_t>(payload.size() >> 8));
  o.push_back(static_cast<uint8_t>(payload.size()));
  o.insert(o.end(), payload.begin(), payload.end());
  return o;
}

}  // namespace mt
}  // namespace link
}  // namespace ember
