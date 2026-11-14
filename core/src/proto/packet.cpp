#include "proto/packet.h"

#include <cstring>

#include "crypto/crypto.h"

namespace ember::proto {

// --- frames ---------------------------------------------------------------------------------------

bool valid_callsign(std::string_view cs) {
  if (cs.empty() || cs.size() > kMaxCallsign) return false;
  for (char c : cs) {
    bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == '-';
    if (!ok) return false;
  }
  return true;
}

size_t frame_overhead(size_t station_len) { return 1 + (station_len ? 1 + station_len : 0) + 4; }

void write_frame_header(ByteWriter& w, const FrameHeader& h) {
  bool st = !h.station.empty();
  w.u8(static_cast<uint8_t>(kVersion << 6 | (static_cast<uint8_t>(h.type) & 3) << 4 | (st ? 0x08 : 0)));
  if (st) w.u8(static_cast<uint8_t>(h.station.size())).str(h.station);
  w.u32be(h.hop_src);
}

bool parse_frame(ByteView frame, FrameHeader& h, ByteView& body) {
  ByteReader r(frame);
  uint8_t b0 = r.u8();
  if (!r.ok() || (b0 >> 6) != kVersion || (b0 & 0x07) != 0) return false;
  uint8_t t = (b0 >> 4) & 3;
  if (t > 2) return false;
  h.type = static_cast<FrameType>(t);
  h.station.clear();
  if (b0 & 0x08) {
    uint8_t n = r.u8();
    ByteView s = r.bytes(n);
    if (!r.ok()) return false;
    h.station = s.to_string();
    if (!valid_callsign(h.station)) return false;
  }
  h.hop_src = r.u32be();
  if (!r.ok() || r.remaining() == 0) return false;
  body = r.rest();
  return true;
}

bool parse_fragment(ByteView body, Fragment& out) {
  ByteReader r(body);
  out.frag_id = r.u16be();
  out.index = r.u8();
  out.count = r.u8();
  if (!r.ok() || out.count < 2 || out.count > kMaxFragments || out.index >= out.count) return false;
  out.total_len = 0;
  if (out.index == 0) {
    out.total_len = r.u16be();
    if (!r.ok() || out.total_len < 2 || out.total_len > kMaxUnit) return false;
  }
  out.data = r.rest();
  return r.ok() && !out.data.empty();
}

void write_fragment(ByteWriter& w, uint16_t frag_id, uint8_t index, uint8_t count, uint16_t total_len,
                    ByteView data) {
  w.u16be(frag_id).u8(index).u8(count);
  if (index == 0) w.u16be(total_len);
  w.bytes(data);
}

std::vector<Bytes> fragment_unit(ByteView unit, size_t mtu, const FrameHeader& h, uint16_t frag_id) {
  std::vector<Bytes> out;
  size_t ovh = frame_overhead(h.station.size());
  if (unit.size() < 2 || unit.size() > kMaxUnit || mtu < ovh + kFragHeader0 + 1) return out;
  size_t cap0 = mtu - ovh - kFragHeader0;
  size_t capn = mtu - ovh - kFragHeaderN;
  size_t count = 1;
  if (unit.size() > cap0) count += (unit.size() - cap0 + capn - 1) / capn;
  if (count < 2) count = 2;  // caller only fragments units that do not fit; keep the format closed
  if (count > kMaxFragments) return out;
  // Spread the data evenly so the last fragment is not tiny (fewer bytes of a frame wasted).
  size_t off = 0;
  FrameHeader fh = h;
  fh.type = FrameType::Fragment;
  for (size_t i = 0; i < count; ++i) {
    size_t cap = i == 0 ? cap0 : capn;
    size_t remaining = unit.size() - off;
    size_t left_frags = count - i;
    size_t take = (remaining + left_frags - 1) / left_frags;
    if (i == 0 && take < 1) take = 1;
    if (take > cap) take = cap;
    if (take > remaining) take = remaining;
    if (take == 0) return {};
    ByteWriter w(mtu);
    write_frame_header(w, fh);
    write_fragment(w, frag_id, static_cast<uint8_t>(i), static_cast<uint8_t>(count),
                   static_cast<uint16_t>(unit.size()), unit.sub(off, take));
    off += take;
    out.push_back(w.take());
  }
  if (off != unit.size()) return {};
  return out;
}

// --- packets --------------------------------------------------------------------------------------

bool known_kind(uint8_t k) { return k >= 0x01 && k <= 0x0F; }

const char* kind_name(Kind k) {
  switch (k) {
    case Kind::Text: return "text";
    case Kind::Ack: return "ack";
    case Kind::Read: return "read";
    case Kind::Announce: return "announce";
    case Kind::Position: return "position";
    case Kind::Sos: return "sos";
    case Kind::SosAck: return "sos_ack";
    case Kind::Checkin: return "checkin";
    case Kind::Bulletin: return "bulletin";
    case Kind::File: return "file";
    case Kind::NetPlan: return "netplan";
    case Kind::Ping: return "ping";
    case Kind::Pong: return "pong";
    case Kind::Whois: return "whois";
    case Kind::App: return "app";
  }
  return "unknown";
}

size_t tag_size(Sec s) {
  switch (s) {
    case Sec::None: return 0;
    case Sec::Signed: return 64;
    case Sec::PairMac: return 16;
    case Sec::PairAead: return 16;
    case Sec::ChanAead: return 16;
  }
  return 0;
}

size_t header_size(const Header& h) {
  size_t n = 1 + 1 + 1 + 1 + 4 + 8 + 4 + 2;
  if (h.dst_type == DestType::Node) n += 8;
  else if (h.dst_type == DestType::Channel) n += 4;
  if (h.has_ack_commit()) n += 8;
  if (h.has_salt()) n += 8;
  return n;
}

void write_header(ByteWriter& w, const Header& h) {
  bool ack = h.has_ack_commit();
  w.u8(static_cast<uint8_t>((static_cast<uint8_t>(h.sec) & 7) << 5 | (static_cast<uint8_t>(h.dst_type) & 3) << 3 |
                            (static_cast<uint8_t>(h.prio) & 3) << 1 | (ack ? 1 : 0)));
  w.u8(static_cast<uint8_t>(h.kind));
  uint8_t ttl = h.ttl > kMaxTtl ? kMaxTtl : h.ttl;
  uint8_t lim = h.hop_limit > kMaxTtl ? kMaxTtl : h.hop_limit;
  if (lim < ttl) lim = ttl;
  w.u8(static_cast<uint8_t>(ttl << 4 | (h.hops > 15 ? 15 : h.hops)));
  w.u8(static_cast<uint8_t>(lim << 4));
  w.u32be(h.pkt_id);
  w.bytes(h.origin.data(), 8);
  if (h.dst_type == DestType::Node) w.bytes(h.dst_node.data(), 8);
  else if (h.dst_type == DestType::Channel) w.u32be(h.channel);
  w.u32be(h.created);
  w.u16be(h.lifetime_min);
  if (ack) w.bytes(h.ack_commit.data(), 8);
  if (h.has_salt()) w.bytes(h.salt.data(), 8);
}

bool parse_packet(ByteView raw, PacketView& out) {
  if (raw.size() > kMaxPacket) return false;
  ByteReader r(raw);
  uint8_t p0 = r.u8();
  uint8_t kind = r.u8();
  uint8_t hop = r.u8();
  uint8_t lim = r.u8();
  if (!r.ok() || (lim & 0x0F)) return false;
  uint8_t sec = p0 >> 5;
  uint8_t dst = (p0 >> 3) & 3;
  if (sec > 4 || dst > 2 || kind == 0) return false;
  Header& h = out.h;
  h.sec = static_cast<Sec>(sec);
  h.dst_type = static_cast<DestType>(dst);
  h.prio = static_cast<Prio>((p0 >> 1) & 3);
  h.want_ack = p0 & 1;
  h.kind = static_cast<Kind>(kind);
  h.ttl = hop >> 4;
  h.hops = hop & 15;
  h.hop_limit = lim >> 4;
  if (h.ttl > h.hop_limit) return false;  // a copy may never carry more reach than its originator gave
  if (h.want_ack && h.dst_type != DestType::Node) return false;
  if ((h.sec == Sec::PairMac || h.sec == Sec::PairAead) && h.dst_type != DestType::Node) return false;
  if (h.sec == Sec::ChanAead && h.dst_type != DestType::Channel) return false;
  if (h.sec == Sec::None && h.kind != Kind::Ack && h.kind != Kind::Whois) return false;
  h.pkt_id = r.u32be();
  r.copy(h.origin.data(), 8);
  h.dst_node = NodeId{};
  h.channel = 0;
  if (h.dst_type == DestType::Node) r.copy(h.dst_node.data(), 8);
  else if (h.dst_type == DestType::Channel) h.channel = r.u32be();
  h.created = r.u32be();
  h.lifetime_min = r.u16be();
  h.ack_commit = AckCommit{};
  if (h.want_ack) r.copy(h.ack_commit.data(), 8);
  h.salt = {};
  if (h.has_salt()) r.copy(h.salt.data(), 8);
  if (!r.ok() || h.lifetime_min == 0) return false;
  size_t hl = r.pos();
  size_t ts = tag_size(h.sec);
  if (raw.size() < hl + ts) return false;
  size_t body_len = raw.size() - hl - ts;
  out.raw = raw;
  out.auth_header = raw.sub(0, hl);
  out.body = raw.sub(hl, body_len);
  out.tag = raw.sub(hl + body_len, ts);
  return true;
}

Bytes auth_header_bytes(const PacketView& p) {
  Bytes b = p.auth_header.to_bytes();
  if (b.size() > kHopByteOffset) b[kHopByteOffset] = 0;
  return b;
}

uint64_t wire_id(ByteView raw) {
  crypto::Blake2b h(8);
  h.update(std::string_view("ember-wire-v1"));
  if (raw.size() > kHopByteOffset) {
    uint8_t zero = 0;
    h.update(raw.sub(0, kHopByteOffset)).update(&zero, 1).update(raw.sub(kHopByteOffset + 1));
  } else {
    h.update(raw);
  }
  uint8_t d[8];
  h.final(d);
  uint64_t v = 0;
  for (uint8_t b : d) v = v << 8 | b;
  return v;
}

void set_hop_byte(Bytes& raw, uint8_t ttl, uint8_t hops) {
  if (raw.size() <= kHopByteOffset) return;
  raw[kHopByteOffset] = static_cast<uint8_t>((ttl > kMaxTtl ? kMaxTtl : ttl) << 4 | (hops > 15 ? 15 : hops));
}

std::string MsgId::to_hex() const {
  uint8_t b[12];
  std::memcpy(b, origin.data(), 8);
  b[8] = static_cast<uint8_t>(pkt_id >> 24);
  b[9] = static_cast<uint8_t>(pkt_id >> 16);
  b[10] = static_cast<uint8_t>(pkt_id >> 8);
  b[11] = static_cast<uint8_t>(pkt_id);
  return ember::to_hex(ByteView(b, 12));
}

bool MsgId::parse(std::string_view hex, MsgId& out) {
  if (hex.size() != 24) return false;
  Bytes b;
  if (!from_hex(hex, b) || b.size() != 12) return false;
  std::memcpy(out.origin.data(), b.data(), 8);
  out.pkt_id = uint32_t(b[8]) << 24 | uint32_t(b[9]) << 16 | uint32_t(b[10]) << 8 | b[11];
  return true;
}

// --- control --------------------------------------------------------------------------------------

Bytes encode_beacon(const Beacon& b) {
  ByteWriter w(40);
  w.u8(static_cast<uint8_t>(Ctrl::Beacon));
  w.bytes(b.node.data(), 8);
  w.u8(static_cast<uint8_t>((b.relay ? 1 : 0) | (b.has_time ? 2 : 0)));
  if (b.has_time) w.u64be(b.time_ms).u8(b.time_quality > 15 ? 15 : b.time_quality);
  w.varint(b.store_count).u32be(b.store_sum);
  return w.take();
}

bool decode_beacon(ByteView body, Beacon& out) {
  ByteReader r(body);
  r.copy(out.node.data(), 8);
  uint8_t f = r.u8();
  if (!r.ok() || (f & ~3u)) return false;
  out.relay = f & 1;
  out.has_time = f & 2;
  out.time_ms = 0;
  out.time_quality = 15;
  if (out.has_time) {
    out.time_ms = r.u64be();
    out.time_quality = r.u8();
    if (out.time_quality > 15) return false;
  }
  uint64_t cnt = r.varint();
  out.store_sum = r.u32be();
  if (!r.ok() || !r.at_end() || cnt > 0xFFFFFFFFull) return false;
  out.store_count = static_cast<uint32_t>(cnt);
  return true;
}

size_t sync_item_size(const SyncItem& it) {
  switch (it.type) {
    case SyncItem::Dig: return 1 + 1 + 4 + 16 * 6;
    case SyncItem::Ids: return 1 + 1 + 4 + 1 + 4 * it.ids.size();
    case SyncItem::Want: return 1 + 1 + 4 * it.ids.size();
  }
  return 0;
}

Bytes encode_sync(const SyncMsg& m) {
  ByteWriter w;
  w.u8(static_cast<uint8_t>(Ctrl::Sync)).u32be(m.target);
  size_t n = m.items.size() > kMaxSyncItems ? kMaxSyncItems : m.items.size();
  w.u8(static_cast<uint8_t>(n));
  for (size_t i = 0; i < n; ++i) {
    const SyncItem& it = m.items[i];
    w.u8(it.type);
    if (it.type == SyncItem::Dig) {
      w.u8(it.bits).u32be(it.prefix);
      for (const auto& c : it.children) w.u16be(c.count).u32be(c.sum);
    } else {
      size_t k = it.ids.size() > 255 ? 255 : it.ids.size();
      if (it.type == SyncItem::Ids) w.u8(it.bits).u32be(it.prefix);
      w.u8(static_cast<uint8_t>(k));
      for (size_t j = 0; j < k; ++j) w.u32be(it.ids[j]);
    }
  }
  return w.take();
}

static bool valid_prefix(uint8_t bits, uint32_t prefix) {
  if (bits % 4 || bits > 28) return false;
  uint32_t mask = bits == 0 ? 0 : ~0u << (32 - bits);
  return (prefix & ~mask) == 0;
}

bool decode_sync(ByteView body, SyncMsg& out) {
  out = SyncMsg();
  ByteReader r(body);
  out.target = r.u32be();
  uint8_t n = r.u8();
  if (!r.ok() || n > kMaxSyncItems) return false;
  for (uint8_t i = 0; i < n; ++i) {
    SyncItem it;
    uint8_t t = r.u8();
    if (!r.ok()) return false;
    if (t == SyncItem::Dig) {
      it.type = SyncItem::Dig;
      it.bits = r.u8();
      it.prefix = r.u32be();
      for (auto& c : it.children) { c.count = r.u16be(); c.sum = r.u32be(); }
      if (!r.ok() || !valid_prefix(it.bits, it.prefix)) return false;
    } else if (t == SyncItem::Ids || t == SyncItem::Want) {
      it.type = static_cast<SyncItem::Type>(t);
      if (t == SyncItem::Ids) {
        it.bits = r.u8();
        it.prefix = r.u32be();
        if (!r.ok() || !valid_prefix(it.bits, it.prefix)) return false;
      }
      uint8_t k = r.u8();
      if (!r.ok() || r.remaining() < size_t(k) * 4) return false;
      it.ids.reserve(k);
      uint32_t mask = it.bits == 0 ? 0 : ~0u << (32 - it.bits);
      for (uint8_t j = 0; j < k; ++j) {
        uint32_t id = r.u32be();
        if (t == SyncItem::Ids && (id & mask) != it.prefix) return false;
        it.ids.push_back(id);
      }
    } else {
      return false;
    }
    out.items.push_back(std::move(it));
  }
  return r.ok() && r.at_end();
}

Bytes encode_frag_nack(const FragNack& n) {
  ByteWriter w;
  w.u8(static_cast<uint8_t>(Ctrl::FragNack)).u32be(n.target).u16be(n.frag_id).u8(n.count);
  uint8_t bitmap[kMaxFragments / 8] = {};
  for (uint8_t i : n.missing)
    if (i < n.count && i < kMaxFragments) bitmap[i / 8] |= static_cast<uint8_t>(1u << (i % 8));
  w.bytes(bitmap, (n.count + 7u) / 8u);
  return w.take();
}

bool decode_frag_nack(ByteView body, FragNack& out) {
  out = FragNack();
  ByteReader r(body);
  out.target = r.u32be();
  out.frag_id = r.u16be();
  out.count = r.u8();
  if (!r.ok() || out.count < 2 || out.count > kMaxFragments) return false;
  ByteView bm = r.bytes((out.count + 7u) / 8u);
  if (!r.ok() || !r.at_end()) return false;
  for (uint8_t i = 0; i < out.count; ++i)
    if (bm[i / 8] & (1u << (i % 8))) out.missing.push_back(i);
  return !out.missing.empty();
}

}  // namespace ember::proto
