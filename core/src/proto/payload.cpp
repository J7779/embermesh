#include "proto/payload.h"

#include <cmath>
#include <cstring>

#include "base/json.h"
#include "base/textpack.h"

namespace ember::proto {

namespace {

void put_str(ByteWriter& w, std::string_view s, size_t max) {
  std::string_view t = utf8_truncate(s, max > 255 ? 255 : max);
  w.u8(static_cast<uint8_t>(t.size())).str(t);
}

bool get_str(ByteReader& r, std::string& out, size_t max) {
  uint8_t n = r.u8();
  ByteView v = r.bytes(n);
  if (!r.ok() || n > max) return false;
  out = v.to_string();
  if (!utf8_valid(out)) { out.clear(); return false; }
  return true;
}

void put_text(ByteWriter& w, std::string_view s, size_t max) {
  w.bytes(textpack_compress(utf8_truncate(s, max)));
}

bool get_text(ByteView rest, std::string& out, size_t max) {
  if (!textpack_decompress(rest, max, out)) return false;
  if (!utf8_valid(out)) { out.clear(); return false; }
  return true;
}

}  // namespace

Position Position::from_degrees(double lat, double lon) {
  Position p;
  auto clampd = [](double v, double lim) { return std::isfinite(v) ? (v > lim ? lim : (v < -lim ? -lim : v)) : 0.0; };
  p.lat_e7 = static_cast<int32_t>(std::lround(clampd(lat, 90.0) * 1e7));
  p.lon_e7 = static_cast<int32_t>(std::lround(clampd(lon, 180.0) * 1e7));
  return p;
}

void write_position(ByteWriter& w, const Position& p) {
  w.u8(static_cast<uint8_t>((p.has_alt ? 1 : 0) | (p.has_accuracy ? 2 : 0)));
  w.u32be(static_cast<uint32_t>(p.lat_e7)).u32be(static_cast<uint32_t>(p.lon_e7));
  if (p.has_alt) w.u16be(static_cast<uint16_t>(p.alt_m));
  if (p.has_accuracy) w.u16be(p.accuracy_m);
}

bool read_position(ByteReader& r, Position& p) {
  uint8_t f = r.u8();
  if (!r.ok() || (f & ~3u)) return false;
  p.lat_e7 = static_cast<int32_t>(r.u32be());
  p.lon_e7 = static_cast<int32_t>(r.u32be());
  p.has_alt = f & 1;
  p.has_accuracy = f & 2;
  p.alt_m = p.has_alt ? static_cast<int16_t>(r.u16be()) : 0;
  p.accuracy_m = p.has_accuracy ? r.u16be() : 0;
  return r.ok() && p.valid();
}

const char* checkin_status_name(CheckinStatus s) {
  switch (s) {
    case CheckinStatus::Ok: return "ok";
    case CheckinStatus::NeedHelp: return "need_help";
    case CheckinStatus::Injured: return "injured";
    case CheckinStatus::Moving: return "moving";
    case CheckinStatus::Sheltering: return "sheltering";
  }
  return "ok";
}
bool parse_checkin_status(std::string_view s, CheckinStatus& out) {
  for (uint8_t i = 0; i <= 4; ++i) {
    if (s == checkin_status_name(static_cast<CheckinStatus>(i))) { out = static_cast<CheckinStatus>(i); return true; }
  }
  return false;
}

const char* bulletin_category_name(BulletinCategory c) {
  static const char* names[] = {"water", "food", "medical", "shelter", "hazard", "trade", "missing", "transport", "info"};
  uint8_t i = static_cast<uint8_t>(c);
  return i <= 8 ? names[i] : "info";
}
bool parse_bulletin_category(std::string_view s, BulletinCategory& out) {
  for (uint8_t i = 0; i <= 8; ++i) {
    if (s == bulletin_category_name(static_cast<BulletinCategory>(i))) { out = static_cast<BulletinCategory>(i); return true; }
  }
  return false;
}

// --- encoders -------------------------------------------------------------------------------------

Bytes encode(const TextBody& b) {
  ByteWriter w;
  w.u8(b.reply_to ? 1 : 0);
  if (b.reply_to) w.bytes(b.reply_to->origin.data(), 8).u32be(b.reply_to->pkt_id);
  put_text(w, b.text, kMaxText);
  return w.take();
}

Bytes encode(const AckBody& b) {
  ByteWriter w;
  size_t n = b.acks.size() > kMaxAcksPerPacket ? kMaxAcksPerPacket : b.acks.size();
  w.u8(static_cast<uint8_t>(n));
  for (size_t i = 0; i < n; ++i) w.u32be(b.acks[i].pkt_id).bytes(b.acks[i].secret.data(), 8);
  return w.take();
}

Bytes encode(const ReadBody& b) {
  ByteWriter w;
  size_t n = b.pkt_ids.size() > 64 ? 64 : b.pkt_ids.size();
  w.u8(static_cast<uint8_t>(n));
  for (size_t i = 0; i < n; ++i) w.u32be(b.pkt_ids[i]);
  return w.take();
}

Bytes encode(const AnnounceBody& b) {
  ByteWriter w;
  w.bytes(b.keys.sign_pk.data(), 32).bytes(b.keys.dh_pk.data(), 32);
  w.u8(static_cast<uint8_t>((b.relay ? 1 : 0) | (b.time_quality > 15 ? 15 : b.time_quality) << 4)).u8(b.caps);
  put_str(w, b.name, kMaxName);
  put_str(w, valid_callsign(b.callsign) ? std::string_view(b.callsign) : std::string_view(), kMaxCallsign);
  put_str(w, b.status, kMaxStatus);
  return w.take();
}

Bytes encode(const PositionBody& b) {
  ByteWriter w;
  write_position(w, b.pos);
  return w.take();
}

Bytes encode(const SosBody& b) {
  ByteWriter w;
  w.u8(static_cast<uint8_t>((b.active ? 1 : 0) | (b.pos ? 2 : 0))).u16be(b.seq);
  if (b.pos) write_position(w, *b.pos);
  put_text(w, b.text, kMaxNote);
  return w.take();
}

Bytes encode(const SosAckBody& b) {
  ByteWriter w;
  w.u32be(b.sos_pkt_id);
  put_text(w, b.text, kMaxNote);
  return w.take();
}

Bytes encode(const CheckinBody& b) {
  ByteWriter w;
  w.u8(static_cast<uint8_t>(b.status)).u8(b.pos ? 1 : 0);
  if (b.pos) write_position(w, *b.pos);
  put_text(w, b.note, kMaxNote);
  return w.take();
}

Bytes encode(const BulletinBody& b) {
  ByteWriter w;
  w.u8(static_cast<uint8_t>(b.category)).u8(static_cast<uint8_t>((b.pos ? 1 : 0) | (b.is_delete ? 2 : 0)));
  if (b.is_delete) w.u32be(b.delete_pkt_id);
  if (b.pos) write_position(w, *b.pos);
  put_str(w, b.title, kMaxShortField);
  put_text(w, b.text, kMaxBulletinText);
  return w.take();
}

Bytes encode(const FileBody& b) {
  ByteWriter w;
  w.u32be(b.file_id).u16be(b.index).u16be(b.count);
  if (b.index == 0) {
    w.u32be(b.total_size);
    put_str(w, b.name, kMaxShortField);
    put_str(w, b.mime, kMaxShortField);
    w.bytes(b.hash.data(), 16);
  }
  w.bytes(b.data);
  return w.take();
}

Bytes encode(const NetPlanBody& b) {
  ByteWriter w;
  size_t n = b.entries.size() > kMaxNetPlanEntries ? kMaxNetPlanEntries : b.entries.size();
  w.u8(static_cast<uint8_t>(n));
  for (size_t i = 0; i < n; ++i) {
    const auto& e = b.entries[i];
    put_str(w, e.label, kMaxShortField);
    w.varint(e.freq_hz);
    put_str(w, e.mode, 16);
    put_str(w, e.tone, 16);
    put_str(w, e.schedule, kMaxShortField);
    put_str(w, e.notes, 128);
  }
  return w.take();
}

Bytes encode(const PingBody& b) { ByteWriter w; w.u32be(b.nonce); return w.take(); }
Bytes encode(const PongBody& b) { ByteWriter w; w.u32be(b.nonce).u8(b.ping_hops); return w.take(); }
Bytes encode(const WhoisBody& b) { ByteWriter w; w.bytes(b.target.data(), 8); return w.take(); }
Bytes encode(const AppBody& b) { ByteWriter w; w.u16be(b.app_type).bytes(b.data); return w.take(); }

// --- decoders -------------------------------------------------------------------------------------

bool decode(ByteView in, TextBody& b) {
  b = TextBody();
  ByteReader r(in);
  uint8_t f = r.u8();
  if (!r.ok() || (f & ~1u)) return false;
  if (f & 1) {
    MsgId m;
    r.copy(m.origin.data(), 8);
    m.pkt_id = r.u32be();
    b.reply_to = m;
  }
  return r.ok() && get_text(r.rest(), b.text, kMaxText);
}

bool decode(ByteView in, AckBody& b) {
  b = AckBody();
  ByteReader r(in);
  uint8_t n = r.u8();
  if (!r.ok() || n == 0 || n > kMaxAcksPerPacket) return false;
  for (uint8_t i = 0; i < n; ++i) {
    AckEntry e;
    e.pkt_id = r.u32be();
    r.copy(e.secret.data(), 8);
    b.acks.push_back(e);
  }
  return r.ok() && r.at_end();
}

bool decode(ByteView in, ReadBody& b) {
  b = ReadBody();
  ByteReader r(in);
  uint8_t n = r.u8();
  if (!r.ok() || n == 0 || n > 64) return false;
  for (uint8_t i = 0; i < n; ++i) b.pkt_ids.push_back(r.u32be());
  return r.ok() && r.at_end();
}

bool decode(ByteView in, AnnounceBody& b) {
  b = AnnounceBody();
  ByteReader r(in);
  r.copy(b.keys.sign_pk.data(), 32);
  r.copy(b.keys.dh_pk.data(), 32);
  uint8_t f = r.u8();
  b.caps = r.u8();
  if (!r.ok() || (f & 0x0Eu)) return false;
  b.relay = f & 1;
  b.time_quality = static_cast<uint8_t>(f >> 4);
  if (!get_str(r, b.name, kMaxName) || !get_str(r, b.callsign, kMaxCallsign) || !get_str(r, b.status, kMaxStatus))
    return false;
  if (!b.callsign.empty() && !valid_callsign(b.callsign)) return false;
  return r.at_end();
}

bool decode(ByteView in, PositionBody& b) {
  ByteReader r(in);
  return read_position(r, b.pos) && r.at_end();
}

bool decode(ByteView in, SosBody& b) {
  b = SosBody();
  ByteReader r(in);
  uint8_t f = r.u8();
  b.seq = r.u16be();
  if (!r.ok() || (f & ~3u)) return false;
  b.active = f & 1;
  if (f & 2) {
    Position p;
    if (!read_position(r, p)) return false;
    b.pos = p;
  }
  return get_text(r.rest(), b.text, kMaxNote);
}

bool decode(ByteView in, SosAckBody& b) {
  b = SosAckBody();
  ByteReader r(in);
  b.sos_pkt_id = r.u32be();
  return r.ok() && get_text(r.rest(), b.text, kMaxNote);
}

bool decode(ByteView in, CheckinBody& b) {
  b = CheckinBody();
  ByteReader r(in);
  uint8_t st = r.u8();
  uint8_t f = r.u8();
  if (!r.ok() || st > 4 || (f & ~1u)) return false;
  b.status = static_cast<CheckinStatus>(st);
  if (f & 1) {
    Position p;
    if (!read_position(r, p)) return false;
    b.pos = p;
  }
  return get_text(r.rest(), b.note, kMaxNote);
}

bool decode(ByteView in, BulletinBody& b) {
  b = BulletinBody();
  ByteReader r(in);
  uint8_t c = r.u8();
  uint8_t f = r.u8();
  if (!r.ok() || c > 8 || (f & ~3u)) return false;
  b.category = static_cast<BulletinCategory>(c);
  b.is_delete = f & 2;
  if (b.is_delete) b.delete_pkt_id = r.u32be();
  if (f & 1) {
    Position p;
    if (!read_position(r, p)) return false;
    b.pos = p;
  }
  if (!get_str(r, b.title, kMaxShortField)) return false;
  return get_text(r.rest(), b.text, kMaxBulletinText);
}

bool decode(ByteView in, FileBody& b) {
  b = FileBody();
  ByteReader r(in);
  b.file_id = r.u32be();
  b.index = r.u16be();
  b.count = r.u16be();
  if (!r.ok() || b.count == 0 || b.count > kMaxFileChunks || b.index >= b.count) return false;
  if (b.index == 0) {
    b.total_size = r.u32be();
    if (!get_str(r, b.name, kMaxShortField) || !get_str(r, b.mime, kMaxShortField)) return false;
    r.copy(b.hash.data(), 16);
    if (!r.ok()) return false;
  }
  b.data = r.rest().to_bytes();
  return r.ok();
}

bool decode(ByteView in, NetPlanBody& b) {
  b = NetPlanBody();
  ByteReader r(in);
  uint8_t n = r.u8();
  if (!r.ok() || n > kMaxNetPlanEntries) return false;
  for (uint8_t i = 0; i < n; ++i) {
    NetPlanEntry e;
    if (!get_str(r, e.label, kMaxShortField)) return false;
    e.freq_hz = r.varint();
    if (!get_str(r, e.mode, 16) || !get_str(r, e.tone, 16) || !get_str(r, e.schedule, kMaxShortField) ||
        !get_str(r, e.notes, 128))
      return false;
    b.entries.push_back(std::move(e));
  }
  return r.ok() && r.at_end();
}

bool decode(ByteView in, PingBody& b) {
  ByteReader r(in);
  b.nonce = r.u32be();
  return r.ok() && r.at_end();
}
bool decode(ByteView in, PongBody& b) {
  ByteReader r(in);
  b.nonce = r.u32be();
  b.ping_hops = r.u8();
  return r.ok() && r.at_end();
}
bool decode(ByteView in, WhoisBody& b) {
  ByteReader r(in);
  r.copy(b.target.data(), 8);
  return r.ok() && r.at_end();
}
bool decode(ByteView in, AppBody& b) {
  b = AppBody();
  ByteReader r(in);
  b.app_type = r.u16be();
  b.data = r.rest().to_bytes();
  return r.ok();
}

}  // namespace ember::proto
