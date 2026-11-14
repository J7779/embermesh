#include "link/link_util.h"

#include <cmath>
#include <cstring>

namespace ember {
namespace link {

// --- Field ---------------------------------------------------------------------------------------

Json Field::to_json() const {
  Json j = Json::object();
  j.set("key", key).set("label", label).set("type", type);
  if (!unit.empty()) j.set("unit", unit);
  j.set("default", dflt);
  if (has_min) j.set("min", integer ? Json(static_cast<int64_t>(min)) : Json(min));
  if (has_max) j.set("max", integer ? Json(static_cast<int64_t>(max)) : Json(max));
  if (step > 0) j.set("step", integer ? Json(static_cast<int64_t>(step)) : Json(step));
  if (!options.empty()) {
    Json o = Json::array();
    for (const auto& op : options) o.push(Json::object({{"value", op.value}, {"label", op.label}}));
    j.set("options", std::move(o));
  }
  if (!help.empty()) j.set("help", help);
  j.set("required", required).set("advanced", advanced);
  return j;
}

Field num(const char* key, const char* label, int64_t dflt, int64_t min, int64_t max, const char* help,
          const char* unit) {
  Field f;
  f.key = key; f.label = label; f.type = "number"; f.dflt = Json(static_cast<long long>(dflt));
  f.has_min = f.has_max = true; f.min = static_cast<double>(min); f.max = static_cast<double>(max);
  f.step = 1; f.integer = true; f.help = help; f.unit = unit ? unit : "";
  return f;
}

Field real(const char* key, const char* label, double dflt, double min, double max, double step,
           const char* help, const char* unit) {
  Field f;
  f.key = key; f.label = label; f.type = "number"; f.dflt = Json(dflt);
  f.has_min = f.has_max = true; f.min = min; f.max = max; f.step = step; f.help = help;
  f.unit = unit ? unit : "";
  return f;
}

Field boolean(const char* key, const char* label, bool dflt, const char* help) {
  Field f;
  f.key = key; f.label = label; f.type = "bool"; f.dflt = Json(dflt); f.help = help;
  return f;
}

Field text(const char* key, const char* label, const char* dflt, const char* help, size_t max_len) {
  Field f;
  f.key = key; f.label = label; f.type = "text"; f.dflt = Json(dflt); f.help = help; f.max_len = max_len;
  return f;
}

Field select(const char* key, const char* label, const char* dflt, std::vector<FieldOption> options,
             const char* help) {
  Field f;
  f.key = key; f.label = label; f.type = "select"; f.dflt = Json(dflt); f.options = std::move(options);
  f.help = help;
  return f;
}

Json schema_json(const Schema& s) {
  Json a = Json::array();
  for (const auto& f : s) a.push(f.to_json());
  return a;
}

bool parse_config(const Schema& schema, const std::string& config_json, Json* out, std::string* error) {
  auto fail = [&](const std::string& m) {
    if (error) *error = m;
    return false;
  };
  Json in = Json::object();
  bool blank = true;
  for (char c : config_json)
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') { blank = false; break; }
  if (!blank) {
    std::string perr;
    JsonLimits lim;
    lim.max_input = 64 * 1024;
    lim.max_depth = 8;
    lim.max_elements = 4096;
    lim.max_string = 16 * 1024;
    if (!Json::parse(config_json, in, &perr, lim)) return fail("config is not valid JSON: " + perr);
    if (!in.is_object()) return fail("config must be a JSON object");
  }
  Json res = Json::object();
  for (const Field& f : schema) {
    const Json* v = in.find(f.key);
    if (!v || v->is_null()) {
      if (f.required && f.dflt.is_null()) return fail(f.key + ": required");
      res.set(f.key, f.dflt);
      continue;
    }
    if (f.type == "number") {
      if (!v->is_number()) return fail(f.key + ": must be a number");
      double d = v->as_double();
      if (!std::isfinite(d)) return fail(f.key + ": must be finite");
      if (f.has_min && d < f.min) return fail(f.key + ": must be >= " + Json(f.min).dump());
      if (f.has_max && d > f.max) return fail(f.key + ": must be <= " + Json(f.max).dump());
      if (f.integer) {
        int64_t i = 0;
        if (!v->get_int_if(&i)) return fail(f.key + ": must be a whole number");
        res.set(f.key, Json(static_cast<long long>(i)));
      } else {
        res.set(f.key, Json(d));
      }
    } else if (f.type == "bool") {
      if (!v->is_bool()) return fail(f.key + ": must be true or false");
      res.set(f.key, *v);
    } else if (f.type == "select") {
      if (!v->is_string()) return fail(f.key + ": must be a string");
      bool ok = false;
      for (const auto& o : f.options)
        if (o.value == v->as_string()) { ok = true; break; }
      if (!ok) return fail(f.key + ": unknown option '" + utf8_sanitize(utf8_truncate(v->as_string(), 40)) + "'");
      res.set(f.key, *v);
    } else {
      if (!v->is_string()) return fail(f.key + ": must be a string");
      if (v->as_string().size() > f.max_len) return fail(f.key + ": too long");
      if (f.required && v->as_string().empty()) return fail(f.key + ": required");
      res.set(f.key, *v);
    }
  }
  *out = std::move(res);
  return true;
}

// --- TxQueue -------------------------------------------------------------------------------------

void TxQueue::set_limits(size_t max_frames, size_t max_bytes) {
  max_frames_ = max_frames ? max_frames : 1;
  max_bytes_ = max_bytes ? max_bytes : 1;
  // Shrinking: drop lowest-priority newest frames until within limits.
  for (int p = kPriorities - 1; p >= 0 && (frames_ > max_frames_ || bytes_ > max_bytes_); --p) {
    while (!q_[p].empty() && (frames_ > max_frames_ || bytes_ > max_bytes_)) {
      bytes_ -= q_[p].back().size();
      --frames_;
      ++evicted_;
      q_[p].pop_back();
    }
  }
}

bool TxQueue::push(ByteView frame, uint8_t priority) {
  if (priority >= kPriorities) priority = kPriorities - 1;
  if (frame.size() > max_bytes_) return false;
  // Can we make room by evicting strictly lower-priority frames?
  size_t f = frames_, b = bytes_;
  for (int p = kPriorities - 1; p > priority && (f + 1 > max_frames_ || b + frame.size() > max_bytes_); --p) {
    for (auto it = q_[p].rbegin(); it != q_[p].rend() && (f + 1 > max_frames_ || b + frame.size() > max_bytes_); ++it) {
      --f;
      b -= it->size();
    }
  }
  if (f + 1 > max_frames_ || b + frame.size() > max_bytes_) return false;
  for (int p = kPriorities - 1; p > priority && (frames_ + 1 > max_frames_ || bytes_ + frame.size() > max_bytes_); --p) {
    while (!q_[p].empty() && (frames_ + 1 > max_frames_ || bytes_ + frame.size() > max_bytes_)) {
      bytes_ -= q_[p].back().size();
      --frames_;
      ++evicted_;
      q_[p].pop_back();
    }
  }
  q_[priority].push_back(frame.to_bytes());
  ++frames_;
  bytes_ += frame.size();
  return true;
}

const Bytes* TxQueue::peek(uint8_t* priority) const {
  for (uint8_t p = 0; p < kPriorities; ++p)
    if (!q_[p].empty()) {
      if (priority) *priority = p;
      return &q_[p].front();
    }
  return nullptr;
}

bool TxQueue::pop(Bytes* frame, uint8_t* priority) {
  for (uint8_t p = 0; p < kPriorities; ++p) {
    if (q_[p].empty()) continue;
    if (priority) *priority = p;
    bytes_ -= q_[p].front().size();
    --frames_;
    *frame = std::move(q_[p].front());
    q_[p].pop_front();
    return true;
  }
  return false;
}

bool TxQueue::pop_if_fits(size_t max_size, Bytes* frame, uint8_t* priority) {
  const Bytes* h = peek();
  if (!h || h->size() > max_size) return false;
  return pop(frame, priority);
}

void TxQueue::clear() {
  for (auto& q : q_) q.clear();
  frames_ = bytes_ = 0;
}

// --- LinkBase ------------------------------------------------------------------------------------

const char* LinkBase::state_name(LinkState s) {
  switch (s) {
    case LinkState::Down: return "down";
    case LinkState::Connecting: return "connecting";
    case LinkState::Up: return "up";
    case LinkState::Error: return "error";
  }
  return "down";
}

void LinkBase::set_state(LinkState s, const char* error) {
  if (error) last_error_ = error;
  state_ = s;
}

void LinkBase::deliver(ByteView frame, const RxMeta& meta) {
  ++st_.rx_frames;
  st_.rx_bytes += frame.size();
  st_.last_rx_ms = now();
  host_.link_frame_received(*this, frame, meta);
}

void LinkBase::io_write(ByteView bytes) {
  if (bytes.empty()) return;
  st_.io_tx_bytes += bytes.size();
  host_.link_io_write(*this, bytes);
}

void LinkBase::interop(const InteropText& t) {
  // Single chokepoint for every received interop path (APRS, Meshtastic, LoRa AT, RTTY/PSK31/CW/
  // DTMF keyboard modes): whatever the driver did, the node only ever sees printable, bounded
  // UTF-8 without control/bidi characters (security review M7).
  InteropText c;
  c.from = trim(printable_text(ByteView(std::string_view(t.from)), kMaxInteropAddrBytes));
  c.to = trim(printable_text(ByteView(std::string_view(t.to)), kMaxInteropAddrBytes));
  c.text = trim(printable_text(ByteView(std::string_view(t.text)), kMaxInteropTextBytes));
  c.network = printable_text(ByteView(std::string_view(t.network)), kMaxInteropAddrBytes);
  if (c.text.empty()) return;
  ++st_.interop_rx;
  host_.link_interop_text(*this, c);
}

void LinkBase::event(const char* name, const Json& obj) { host_.link_event(*this, name, obj.dump()); }

uint8_t LinkBase::random_u8() {
  uint8_t b = 0;
  host_.random_bytes(&b, 1);
  return b;
}

uint32_t LinkBase::random_u32() {
  uint8_t b[4] = {0, 0, 0, 0};
  host_.random_bytes(b, 4);
  return static_cast<uint32_t>(b[0]) | static_cast<uint32_t>(b[1]) << 8 | static_cast<uint32_t>(b[2]) << 16 |
         static_cast<uint32_t>(b[3]) << 24;
}

bool LinkBase::enqueue(ByteView frame, uint8_t priority, size_t mtu) {
  if (frame.empty() || frame.size() > mtu || !q_.push(frame, priority)) {
    ++st_.tx_refused;
    return false;
  }
  return true;
}

Json LinkBase::base_status() const {
  Json j = Json::object();
  j.set("type", type()).set("state", state_name(state_));
  if (!last_error_.empty()) j.set("last_error", last_error_);
  j.set("tx_frames", st_.tx_frames).set("tx_bytes", st_.tx_bytes);
  j.set("rx_frames", st_.rx_frames).set("rx_bytes", st_.rx_bytes);
  j.set("rx_errors", st_.rx_errors).set("tx_refused", st_.tx_refused);
  j.set("tx_evicted", q_.evicted());
  j.set("io_tx_bytes", st_.io_tx_bytes).set("io_rx_bytes", st_.io_rx_bytes);
  j.set("interop_rx", st_.interop_rx).set("interop_tx", st_.interop_tx);
  j.set("airtime_ms", st_.airtime_ms);
  if (st_.last_rx_ms) j.set("last_rx_ms", st_.last_rx_ms);
  j.set("queue", static_cast<uint64_t>(q_.size())).set("queue_bytes", static_cast<uint64_t>(q_.bytes()));
  return j;
}

Json caps_json(const LinkCaps& c) {
  Json j = Json::object();
  j.set("mtu", c.mtu).set("bitrate_bps", c.bitrate_bps).set("broadcast", c.broadcast);
  j.set("half_duplex", c.half_duplex).set("reliable", c.reliable);
  j.set("encryption_allowed", c.encryption_allowed).set("requires_station_id", c.requires_station_id);
  j.set("duty_cycle_permille", c.duty_cycle_permille).set("cost", c.cost);
  return j;
}

uint64_t airtime_ms(size_t bytes, uint32_t bps) {
  if (bps == 0) return 0;
  uint64_t bits = static_cast<uint64_t>(bytes) * 8u * 1000u;
  return (bits + bps - 1) / bps;
}

void Pacer::spend(uint64_t now, size_t bytes, uint32_t bps, uint32_t gap_ms) {
  uint64_t start = now > next_ ? now : next_;
  next_ = start + airtime_ms(bytes, bps) + gap_ms;
}

void DutyCycle::add(uint64_t now, uint32_t ms) {
  used(now);
  if (e_.size() >= 4096) {  // bounded: merge into the newest entry
    e_.back().ms += ms;
  } else {
    e_.push_back({now, ms});
  }
  sum_ += ms;
}

uint64_t DutyCycle::used(uint64_t now) {
  while (!e_.empty() && e_.front().t + window_ <= now) {
    sum_ -= e_.front().ms;
    e_.pop_front();
  }
  return sum_;
}

bool DutyCycle::allows(uint64_t now, uint32_t ms, uint16_t permille) {
  if (permille >= 1000) return true;
  uint64_t budget = static_cast<uint64_t>(window_) * permille / 1000;
  return used(now) + ms <= budget;
}

// --- misc ----------------------------------------------------------------------------------------

bool unescape_text(const std::string& in, Bytes* out) {
  out->clear();
  for (size_t i = 0; i < in.size(); ++i) {
    char c = in[i];
    if (c != '\\') { out->push_back(static_cast<uint8_t>(c)); continue; }
    if (i + 1 >= in.size()) return false;
    char e = in[++i];
    switch (e) {
      case 'r': out->push_back('\r'); break;
      case 'n': out->push_back('\n'); break;
      case 't': out->push_back('\t'); break;
      case '\\': out->push_back('\\'); break;
      case 'x': {
        if (in.size() - i < 3) return false;  // need two hex digits after 'x'
        Bytes b;
        if (!from_hex(std::string_view(in).substr(i + 1, 2), b) || b.size() != 1) return false;
        out->push_back(b[0]);
        i += 2;
        break;
      }
      default: return false;
    }
  }
  return true;
}

std::string trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
  return s.substr(a, b - a);
}

std::vector<std::string> split_list(const std::string& s, char sep) {
  std::vector<std::string> out;
  size_t start = 0;
  for (size_t i = 0; i <= s.size(); ++i) {
    if (i == s.size() || s[i] == sep) {
      std::string p = trim(s.substr(start, i - start));
      if (!p.empty()) out.push_back(p);
      start = i + 1;
    }
  }
  return out;
}

namespace {
// Code points that must never reach chat/status text from a hostile peer: C1 controls (U+0080..9F,
// e.g. U+009B is a CSI), line/paragraph separators (U+2028/2029, rendered as line breaks), bidi
// embeddings/overrides (U+202A..202E) and isolates (U+2066..2069) used to spoof text direction.
// Line separators become a space; the rest are dropped.
int classify_cp(uint32_t cp) {
  if (cp >= 0x80 && cp <= 0x9F) return 0;
  if (cp == 0x2028 || cp == 0x2029) return 1;
  if ((cp >= 0x202A && cp <= 0x202E) || (cp >= 0x2066 && cp <= 0x2069)) return 0;
  return 2;
}
}  // namespace

std::string printable_text(ByteView b, size_t max_bytes) {
  std::string raw;
  raw.reserve(b.size() < 4096 ? b.size() : 4096);
  for (uint8_t c : b) {
    if (c == '\r' || c == '\n' || c == '\t') raw.push_back(' ');
    else if (c < 0x20 || c == 0x7F) continue;
    else raw.push_back(static_cast<char>(c));
  }
  std::string s = utf8_sanitize(raw);  // valid UTF-8 from here on
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size();) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    if (n > s.size() - i) break;
    uint32_t cp = c;
    if (n == 2) cp = (c & 0x1Fu) << 6 | (s[i + 1] & 0x3Fu);
    else if (n == 3) cp = (c & 0x0Fu) << 12 | (s[i + 1] & 0x3Fu) << 6 | (s[i + 2] & 0x3Fu);
    int k = n == 1 ? 2 : classify_cp(cp);
    if (k == 1) out.push_back(' ');
    else if (k == 2) out.append(s, i, n);
    i += n;
  }
  return std::string(utf8_truncate(out, max_bytes));
}

bool parse_int(const std::string& s, int64_t* out) {
  // Strict: optional '-', then 1..19 decimal digits, value within int64 (no UB on overflow, M6).
  if (s.empty() || s.size() > 20) return false;
  size_t i = 0;
  bool neg = false;
  if (s[0] == '-') { neg = true; i = 1; }
  if (i >= s.size() || s.size() - i > 19) return false;
  uint64_t v = 0;
  const uint64_t limit = neg ? static_cast<uint64_t>(INT64_MAX) + 1 : static_cast<uint64_t>(INT64_MAX);
  for (; i < s.size(); ++i) {
    if (s[i] < '0' || s[i] > '9') return false;
    uint64_t d = static_cast<uint64_t>(s[i] - '0');
    if (v > (limit - d) / 10) return false;
    v = v * 10 + d;
  }
  if (neg) *out = v == static_cast<uint64_t>(INT64_MAX) + 1 ? INT64_MIN : -static_cast<int64_t>(v);
  else *out = static_cast<int64_t>(v);
  return true;
}

static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(ByteView b) {
  std::string out;
  out.reserve((b.size() + 2) / 3 * 4);
  size_t i = 0;
  for (; i + 3 <= b.size(); i += 3) {
    uint32_t v = static_cast<uint32_t>(b[i]) << 16 | static_cast<uint32_t>(b[i + 1]) << 8 | b[i + 2];
    out.push_back(kB64[v >> 18]); out.push_back(kB64[(v >> 12) & 63]);
    out.push_back(kB64[(v >> 6) & 63]); out.push_back(kB64[v & 63]);
  }
  size_t r = b.size() - i;
  if (r == 1) {
    uint32_t v = static_cast<uint32_t>(b[i]) << 16;
    out.push_back(kB64[v >> 18]); out.push_back(kB64[(v >> 12) & 63]); out += "==";
  } else if (r == 2) {
    uint32_t v = static_cast<uint32_t>(b[i]) << 16 | static_cast<uint32_t>(b[i + 1]) << 8;
    out.push_back(kB64[v >> 18]); out.push_back(kB64[(v >> 12) & 63]);
    out.push_back(kB64[(v >> 6) & 63]); out.push_back('=');
  }
  return out;
}

bool base64_decode(const std::string& s, Bytes* out, size_t max_out) {
  out->clear();
  if (s.size() % 4 != 0) return false;
  if (s.size() / 4 * 3 > max_out + 2) return false;
  auto val = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
  };
  for (size_t i = 0; i < s.size(); i += 4) {
    bool last = i + 4 == s.size();
    int a = val(s[i]), b = val(s[i + 1]);
    if (a < 0 || b < 0) { out->clear(); return false; }
    int c = -1, d = -1;
    size_t pad = 0;
    if (last && s[i + 2] == '=') {
      if (s[i + 3] != '=') { out->clear(); return false; }
      pad = 2;
    } else if (last && s[i + 3] == '=') {
      pad = 1;
      c = val(s[i + 2]);
      if (c < 0) { out->clear(); return false; }
    } else {
      c = val(s[i + 2]);
      d = val(s[i + 3]);
      if (c < 0 || d < 0) { out->clear(); return false; }
    }
    uint32_t v = static_cast<uint32_t>(a) << 18 | static_cast<uint32_t>(b) << 12;
    if (pad < 2) v |= static_cast<uint32_t>(c) << 6;
    if (pad < 1) v |= static_cast<uint32_t>(d);
    out->push_back(static_cast<uint8_t>(v >> 16));
    if (pad < 2) out->push_back(static_cast<uint8_t>(v >> 8));
    if (pad < 1) out->push_back(static_cast<uint8_t>(v));
    // canonical: unused bits must be zero
    if (pad == 2 && (v & 0xFFFF)) { out->clear(); return false; }
    if (pad == 1 && (v & 0xFF)) { out->clear(); return false; }
  }
  if (out->size() > max_out) { out->clear(); return false; }
  return true;
}

}  // namespace link
}  // namespace ember
