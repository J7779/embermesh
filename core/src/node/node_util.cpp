#include "node/node_util.h"

#include <cstdio>
#include <cstdlib>

#include "base/codec.h"
#include "modem/modem.h"

namespace ember::node {

namespace {
const char* const kExtraPlanModes[] = {"voice", "lora", "meshtastic", "other"};

const std::vector<std::string>& modem_names() {
  static const std::vector<std::string> names = [] {
    std::vector<std::string> v;
    for (const modem::ModemInfo& i : modem::modem_catalog()) v.push_back(i.name);
    return v;
  }();
  return names;
}
}  // namespace

Json netplan_modes() {
  Json a = Json::array();
  for (const char* m : kExtraPlanModes)
    if (std::string(m) == "voice") a.push(m);
  for (const std::string& n : modem_names()) a.push(n);
  for (const char* m : kExtraPlanModes)
    if (std::string(m) != "voice") a.push(m);
  return a;
}

std::string canonical_plan_mode(const std::string& mode) {
  std::string k;  // lower case, separators removed
  for (char c : mode) {
    if (c == ' ' || c == '-' || c == '_' || c == '.') continue;
    k.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c);
  }
  if (k.empty()) return mode;
  static const struct { const char* from; const char* to; } kAliases[] = {
      {"bpsk31", "psk31"}, {"morse", "cw"}, {"rtty45", "rtty"}, {"rtty4545", "rtty"}, {"bell202", "afsk1200"}};
  for (const auto& a : kAliases)
    if (k == a.from) return a.to;
  for (const std::string& n : modem_names())
    if (k == n) return n;
  for (const char* m : kExtraPlanModes)
    if (k == m) return m;
  return mode;
}

bool json_to_netplan(const Json& j, proto::NetPlanBody* out, std::string* err) {
  auto fail = [&](std::string m) {
    if (err) *err = std::move(m);
    return false;
  };
  const Json& e = j.get("entries");
  if (!e.is_array()) return fail("entries must be an array");
  if (e.size() > proto::kMaxNetPlanEntries)
    return fail("at most " + std::to_string(proto::kMaxNetPlanEntries) + " entries");
  proto::NetPlanBody b;
  for (const Json& x : e.items()) {
    if (!x.is_object()) return fail("each entry must be an object");
    proto::NetPlanEntry en;
    const Json& f = x.get("freq_hz");
    int64_t hz = 0;
    if (!f.is_number() || !f.get_int_if(&hz) || hz <= 0 || hz > 300000000000ll)
      return fail("each entry needs freq_hz (positive integer Hz)");
    en.freq_hz = static_cast<uint64_t>(hz);
    struct S {
      const char* key;
      std::string* dst;
    } fields[] = {{"label", &en.label}, {"mode", &en.mode}, {"tone", &en.tone}, {"schedule", &en.schedule}, {"notes", &en.notes}};
    for (auto& s : fields) {
      const Json& v = x.get(s.key);
      if (v.is_null()) continue;
      if (!v.is_string()) return fail(std::string("entry ") + s.key + " must be a string");
      if (v.as_string().size() > proto::kMaxShortField)
        return fail(std::string("entry ") + s.key + " is too long (max 64 bytes)");
      *s.dst = v.as_string();
    }
    if (en.mode.empty()) en.mode = "voice";
    en.mode = canonical_plan_mode(en.mode);
    b.entries.push_back(std::move(en));
  }
  *out = std::move(b);
  return true;
}

Json netplan_entries_json(const proto::NetPlanBody& b) {
  Json a = Json::array();
  for (const auto& e : b.entries) {
    Json o = Json::object();
    o.set("label", e.label).set("freq_hz", e.freq_hz).set("mode", canonical_plan_mode(e.mode));
    if (!e.tone.empty()) o.set("tone", e.tone);
    if (!e.schedule.empty()) o.set("schedule", e.schedule);
    if (!e.notes.empty()) o.set("notes", e.notes);
    a.push(std::move(o));
  }
  return a;
}

bool contact_keys(const Json& rec, PublicKeys* out) {
  Bytes k;
  if (!base64_decode(rec.get_string("keys"), k) || k.size() != 64) return false;
  std::copy(k.begin(), k.begin() + 32, out->sign_pk.begin());
  std::copy(k.begin() + 32, k.end(), out->dh_pk.begin());
  return true;
}

std::string keys_b64(const PublicKeys& k) {
  Bytes b(k.sign_pk.begin(), k.sign_pk.end());
  b.insert(b.end(), k.dh_pk.begin(), k.dh_pk.end());
  return base64_encode(b);
}

std::string number_text(double v) {
  if (!std::isfinite(v)) return "0";
  if (v == std::floor(v) && std::fabs(v) < 1e15) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(v));
    return buf;
  }
  char buf[40];
  for (int prec = 1; prec <= 17; ++prec) {
    std::snprintf(buf, sizeof buf, "%.*g", prec, v);
    if (std::strtod(buf, nullptr) == v) break;
  }
  return buf;
}

}  // namespace ember::node
