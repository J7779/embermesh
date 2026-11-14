// EmberMesh node — small shared helpers: key encodings, JSON <-> proto conversions, flags.
#pragma once

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "base/json.h"
#include "crypto/identity.h"
#include "ember/bytes.h"
#include "mesh/engine.h"
#include "proto/payload.h"

namespace ember::node {

// MethodDef flags.
enum : uint8_t {
  kAllowLocked = 1,  // callable while storage is locked
  kNeedEngine = 2,   // needs the running mesh engine (identity + clock)
};

inline ByteView B(std::string_view s) { return ByteView(s); }

inline std::optional<Json> parse_json_bytes(const Bytes& b) {
  return Json::parse(std::string_view(reinterpret_cast<const char*>(b.data()), b.size()));
}

inline std::string hex32(uint32_t v) {
  uint8_t b[4] = {static_cast<uint8_t>(v >> 24), static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 8),
                  static_cast<uint8_t>(v)};
  return to_hex(ByteView(b, 4));
}

inline bool parse_hex32(std::string_view s, uint32_t* out) {
  if (s.size() != 8) return false;
  Bytes b;
  if (!from_hex(s, b)) return false;
  *out = uint32_t(b[0]) << 24 | uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | b[3];
  return true;
}

inline ByteView nid_view(const NodeId& id) { return ByteView(id.data(), id.size()); }

// 12-byte binary message id (origin || be32 pkt_id) and its 24-char hex form.
inline Bytes id12(const proto::MsgId& m) {
  Bytes b(m.origin.begin(), m.origin.end());
  b.push_back(static_cast<uint8_t>(m.pkt_id >> 24));
  b.push_back(static_cast<uint8_t>(m.pkt_id >> 16));
  b.push_back(static_cast<uint8_t>(m.pkt_id >> 8));
  b.push_back(static_cast<uint8_t>(m.pkt_id));
  return b;
}
inline bool id12_from_hex(std::string_view hex, Bytes* out) {
  if (hex.size() != 24) return false;
  return from_hex(hex, *out) && out->size() == 12;
}

inline bool valid_node_hex(std::string_view s) {
  if (s.size() != 16) return false;
  for (char c : s)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

inline const char* prio_name(proto::Prio p) {
  switch (p) {
    case proto::Prio::Low: return "low";
    case proto::Prio::High: return "high";
    case proto::Prio::Emergency: return "emergency";
    default: return "normal";
  }
}
inline bool parse_prio(std::string_view s, proto::Prio* out) {
  if (s == "low") *out = proto::Prio::Low;
  else if (s == "normal") *out = proto::Prio::Normal;
  else if (s == "high") *out = proto::Prio::High;
  else if (s == "emergency") *out = proto::Prio::Emergency;
  else return false;
  return true;
}

inline int status_rank(std::string_view s) {
  if (s == "queued") return 0;
  if (s == "sent") return 1;
  if (s == "relayed") return 2;
  if (s == "delivered") return 3;
  if (s == "read") return 4;
  return -1;  // failed / received / unknown
}

inline Json default_settings() {
  return Json::object({{"region", "US"},
                       {"relay_enabled", true},
                       {"store_forward_days", 3},
                       {"beacon_interval_s", 1800},
                       {"low_power", false},
                       {"position_sharing", "manual"},
                       {"default_ttl_hops", 7},
                       {"units", "metric"},
                       {"theme", "dark"},
                       {"links_autostart", Json::array()},
                       {"read_receipts", true}});
}

// Position JSON {lat, lon, alt?, accuracy?, ts}.
inline Json position_to_json(const proto::Position& p, uint64_t ts) {
  Json j = Json::object();
  j.set("lat", p.lat()).set("lon", p.lon());
  if (p.has_alt) j.set("alt", static_cast<int>(p.alt_m));
  if (p.has_accuracy) j.set("accuracy", static_cast<int>(p.accuracy_m));
  j.set("ts", ts);
  return j;
}

// Strict: lat/lon numbers in range; alt/accuracy optional numbers. *err set on failure.
inline bool json_to_position(const Json& j, proto::Position* out, std::string* err = nullptr) {
  auto fail = [&](const char* m) {
    if (err) *err = m;
    return false;
  };
  if (!j.is_object()) return fail("position must be an object {lat, lon}");
  const Json& la = j.get("lat");
  const Json& lo = j.get("lon");
  if (!la.is_number() || !lo.is_number()) return fail("position.lat and position.lon must be numbers");
  double lat = la.as_double(), lon = lo.as_double();
  if (!std::isfinite(lat) || !std::isfinite(lon) || lat < -90 || lat > 90 || lon < -180 || lon > 180)
    return fail("position lat/lon out of range");
  proto::Position p = proto::Position::from_degrees(lat, lon);
  const Json& alt = j.get("alt");
  if (!alt.is_null()) {
    if (!alt.is_number() || !std::isfinite(alt.as_double()) || alt.as_double() < -32768 || alt.as_double() > 32767)
      return fail("position.alt must be a number of metres");
    p.has_alt = true;
    p.alt_m = static_cast<int16_t>(std::lround(alt.as_double()));
  }
  const Json& acc = j.get("accuracy");
  if (!acc.is_null()) {
    if (!acc.is_number() || !std::isfinite(acc.as_double()) || acc.as_double() < 0)
      return fail("position.accuracy must be a non-negative number");
    p.has_accuracy = true;
    double a = acc.as_double();
    p.accuracy_m = static_cast<uint16_t>(a > 65535 ? 65535 : std::lround(a));
  }
  if (!p.valid()) return fail("position out of range");
  *out = p;
  return true;
}

// Net plan modes: the modem catalogue names (modem/modem.h, e.g. "afsk1200", "robust62", "psk31",
// "cw") plus "voice", "lora", "meshtastic" and "other". canonical_plan_mode() maps old / loose
// spellings ("bpsk31", "PSK-31", "AFSK 1200", "morse") to those names and returns anything else
// unchanged (plans from other nodes may carry modes we do not know).
std::string canonical_plan_mode(const std::string& mode);
Json netplan_modes();
bool json_to_netplan(const Json& j, proto::NetPlanBody* out, std::string* err);
Json netplan_entries_json(const proto::NetPlanBody& b);

// Public keys stored in a contact record ("keys": base64(sign_pk || dh_pk)).
bool contact_keys(const Json& rec, PublicKeys* out);
std::string keys_b64(const PublicKeys& k);

// Shortest round-trip decimal text of a double (for tools.run inputs).
std::string number_text(double v);

}  // namespace ember::node
