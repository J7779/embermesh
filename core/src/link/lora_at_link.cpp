// EmberMesh core / link — "lora_at": AT-command LoRa modules of the Reyax family (RYLR998 / RYLR498,
// RYLR896 / RYLR406 and clones) on a serial port (115200 8N1 by default).
//
// AT dialogue (REYAX "LoRa AT Command Guide"): every command ends in CR LF and is answered by
// "+OK", "+<NAME>=<value>" for queries, or "+ERR=<n>". "+READY" is printed after power-up/reset.
//   AT                          probe
//   AT+VER?                     -> +VER=RYLRxx8_Vx.x.x
//   AT+ADDRESS? / AT+ADDRESS=n  0..65535 (stored in flash: only written when different)
//   AT+NETWORKID? / =n          RYLR998: 3..15 or 18 (default 18); RYLR896: 0..16 (default 0) (flash)
//   AT+BAND=<Hz>                not stored in flash
//   AT+PARAMETER=SF,BW,CR,PRE   BW code 0..9 = 7.8 .. 500 kHz (RYLR998: 7..9 only), CR 1..4 = 4/5..4/8
//   AT+CRFOP=<dBm>              RYLR998 0..22, RYLR896 0..15
//   AT+CPIN=<8 hex>             optional AES "domain password"
//   AT+SEND=<addr>,<len>,<data> addr 0 = broadcast, len <= 240 characters
//   +RCV=<addr>,<len>,<data>,<RSSI>,<SNR>   unsolicited on reception
//
// The AT channel is text and <len> counts characters, so mesh frames are sent as
//   "EM1:" + base64(frame)        (4 + 4*ceil(n/3) <= 240 characters -> MTU 177 bytes)
// Any other received payload is foreign text and is reported as interop text (network "lora_at");
// send_interop_text() sends plain text the same way, so stock Reyax sketches can chat with us.
#include <cstdio>
#include <deque>
#include <memory>

#include "link/drivers.h"
#include "link/lora_common.h"

namespace ember {
namespace link {
namespace {

constexpr size_t kMaxPayloadChars = 240;
constexpr const char* kMarker = "EM1:";
constexpr size_t kMarkerLen = 4;
constexpr size_t kMtu = (kMaxPayloadChars - kMarkerLen) / 4 * 3;  // 177
constexpr size_t kMaxLine = 600;
constexpr uint32_t kRetryIdleMs = 10000;

const uint32_t kBwHz[10] = {7800, 10400, 15600, 20800, 31250, 41700, 62500, 125000, 250000, 500000};

struct Model {
  const char* id;
  int sf_min, sf_max, bw_min, net_default, power_max, pre_default;
};
const Model kModels[] = {
    {"rylr998", 5, 11, 7, 18, 22, 12},
    {"rylr896", 7, 12, 0, 0, 15, 4},
};
const Model& model(const std::string& id) {
  for (const auto& m : kModels)
    if (id == m.id) return m;
  return kModels[0];
}

const char* err_text(int e) {
  switch (e) {
    case 1: return "command not terminated with CR LF";
    case 2: return "unknown command";
    case 4: return "length does not match data";
    case 5: return "TX timed out";
    case 10: return "CRC error";
    case 12: return "TX data over 240 bytes";
    case 13: return "flash write failed";
    case 14: return "unknown failure";
    case 15: return "last TX not completed";
    case 17: return "preamble value not allowed / last TX not completed";
    case 18: return "RX failed, header error";
    default: return "module error";
  }
}

Schema lora_at_schema() {
  std::vector<FieldOption> bw;
  for (int i = 0; i < 10; ++i) {
    char v[4], label[48];
    std::snprintf(v, sizeof v, "%d", i);
    std::snprintf(label, sizeof label, "%.1f kHz%s", kBwHz[i] / 1000.0, i < 7 ? " (RYLR896 only)" : "");
    bw.push_back({v, label});
  }
  return {
      select("model", "Module", "rylr998",
             {{"rylr998", "RYLR998 / RYLR498 (and clones)"}, {"rylr896", "RYLR896 / RYLR406 (older)"}},
             "The module printed on the board."),
      select("region", "Region", "eu868", lora_region_options(),
             "Where you are. Sets a legal default frequency, power and duty-cycle limit."),
      num("band_hz", "Frequency", 0, 0, 1020000000, "Exact frequency. 0 = the region's default channel. All modules must match.", "Hz"),
      num("sf", "Spreading factor", 9, 5, 12, "7 = fast/short range ... 12 = slow/long range. All modules must match."),
      select("bw", "Bandwidth", "7", bw, "LoRa bandwidth. All modules must match."),
      num("cr", "Coding rate", 1, 1, 4, "1 = 4/5 (fastest) ... 4 = 4/8 (most robust). All modules must match.").adv(),
      num("preamble", "Preamble", -1, -1, 24, "-1 = module default (12 on RYLR998, 4 on RYLR896).").adv(),
      num("network_id", "Network ID", -1, -1, 18,
          "Modules only hear others with the same network ID. -1 = module default (18 on RYLR998, 0 on RYLR896)."),
      num("address", "Module address", 0, 0, 65535, "This module's address (stored in the module)."),
      num("dest_address", "Send to address", 0, 0, 65535, "0 = broadcast to every module (normal for the mesh).").adv(),
      num("power_dbm", "Transmit power", -1, -1, 22,
          "-1 = the region's conservative default. RYLR896 max 15 dBm, RYLR998 max 22 dBm. Respect local limits.", "dBm"),
      Field(text("password", "Network password", "",
                 "Optional 8 hex digit AES password (AT+CPIN). Empty = none. All modules must match.", 8)).adv(),
      boolean("interop", "Show plain-text messages", true,
              "Show text sent by ordinary (non-EmberMesh) Reyax modules as interop messages."),
      boolean("amateur", "Amateur radio operation", false,
              "Turn on when operating under an amateur licence: traffic is then unencrypted and must carry your callsign."),
      num("duty_cycle_permille", "Duty-cycle limit", 0, 0, 1000, "Transmit time limit in thousandths. 0 = the region's legal limit.", "‰").adv(),
      num("cost", "Routing cost", 20, 1, 255, "Lower is preferred when several links reach a node.").adv(),
      num("queue_frames", "Queue size", 16, 1, 256, "Frames waiting to be sent before new ones are refused.", "frames").adv(),
      num("cmd_timeout_ms", "Command timeout", 1000, 200, 10000, "How long to wait for the module to answer a command.", "ms").adv(),
  };
}

bool hex8(const std::string& s) {
  if (s.size() != 8 || s == "00000000") return false;
  for (char c : s)
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

bool starts_with(const std::string& s, const char* p) { return s.compare(0, std::char_traits<char>::length(p), p) == 0; }

// Parses an optionally space-prefixed signed integer occupying all of `s`.
bool int_field(const std::string& s, int64_t* out) {
  size_t i = 0;
  while (i < s.size() && s[i] == ' ') ++i;
  return parse_int(s.substr(i), out);
}

struct Rcv {
  int64_t addr = 0;
  std::string data;
  int64_t rssi = 0, snr = 0;
};

// "+RCV=<addr>,<len>,<data>,<rssi>,<snr>": the data is taken by length (it may contain commas).
// Returns 1 ok, 0 malformed, -1 header ok but the line is shorter than the data (embedded newline).
int parse_rcv(const std::string& line, Rcv* out) {
  size_t p = 5;  // after "+RCV="
  size_t c1 = line.find(',', p);
  if (c1 == std::string::npos || c1 - p > 6) return 0;
  int64_t addr = 0, len = 0;
  if (!int_field(line.substr(p, c1 - p), &addr) || addr < 0 || addr > 65535) return 0;
  size_t c2 = line.find(',', c1 + 1);
  if (c2 == std::string::npos || c2 - c1 > 6) return 0;
  if (!int_field(line.substr(c1 + 1, c2 - c1 - 1), &len) || len < 0 || len > static_cast<int64_t>(kMaxPayloadChars)) return 0;
  size_t n = static_cast<size_t>(len);
  for (size_t start = c2 + 1; start <= c2 + 2; ++start) {
    if (line.size() < start + n) return -1;
    std::string tail = line.substr(start + n);
    // tail must be ",<rssi>,<snr>"
    if (tail.empty() || tail[0] != ',') {
      if (start == c2 + 1 && start < line.size() && line[start] == ' ') continue;  // "+RCV=50, 5, HELLO, ..." style
      return line.size() < start + n + 4 ? -1 : 0;
    }
    size_t c3 = tail.find(',', 1);
    if (c3 == std::string::npos) return 0;
    int64_t rssi = 0, snr = 0;
    if (!int_field(tail.substr(1, c3 - 1), &rssi) || !int_field(tail.substr(c3 + 1), &snr)) {
      if (start == c2 + 1 && line[start] == ' ') continue;
      return 0;
    }
    if (rssi < -200 || rssi > 50 || snr < -50 || snr > 100) return 0;
    out->addr = addr;
    out->data = line.substr(start, n);
    out->rssi = rssi;
    out->snr = snr;
    return 1;
  }
  return 0;
}

class LoraAtLink final : public LinkBase {
 public:
  explicit LoraAtLink(LinkHost& h) : LinkBase(h) {}

  bool configure(const Json& c, std::string* error) {
    auto bad = [&](const char* m) {
      if (error) *error = m;
      return false;
    };
    const Model& m = model(c.get_string("model"));
    const LoraRegion& reg = lora_region(c.get_string("region"));
    Settings s;
    s.model = m.id;
    int64_t band = c.get_int("band_hz");
    if (band && band < 100000000) return bad("band_hz: frequency too low");
    if (band && reg.min_hz && (band < reg.min_hz || band > reg.max_hz))
      return bad("band_hz: outside the selected region's band (choose region 'custom' to override)");
    s.band = static_cast<uint32_t>(band ? band : reg.freq_hz);
    s.sf = static_cast<int>(c.get_int("sf", 9));
    int64_t bwc = 7;
    parse_int(c.get_string("bw"), &bwc);
    s.bw = static_cast<int>(bwc);
    if (s.sf < m.sf_min || s.sf > m.sf_max) return bad("sf: not supported by this module");
    if (s.bw < m.bw_min) return bad("bw: this module only supports 125, 250 and 500 kHz");
    if (std::string(m.id) == "rylr998" && s.sf > s.bw + 2)
      return bad("sf: RYLR998 allows SF up to 9 at 125 kHz, 10 at 250 kHz, 11 at 500 kHz");
    s.cr = static_cast<int>(c.get_int("cr", 1));
    int64_t net = c.get_int("network_id", -1);
    s.net = static_cast<int>(net >= 0 ? net : m.net_default);
    if (std::string(m.id) == "rylr998" && !((s.net >= 3 && s.net <= 15) || s.net == 18))
      return bad("network_id: RYLR998 accepts 3..15 or 18");
    if (std::string(m.id) == "rylr896" && s.net > 16) return bad("network_id: RYLR896 accepts 0..16");
    int64_t pre = c.get_int("preamble", -1);
    s.pre = static_cast<int>(pre >= 0 ? pre : m.pre_default);
    if (std::string(m.id) == "rylr998" && s.net != 18 && s.pre != 12)
      return bad("preamble: RYLR998 requires 12 unless network_id is 18");
    if (std::string(m.id) == "rylr998" && (s.pre < 4 || s.pre > 24)) return bad("preamble: 4..24");
    if (std::string(m.id) == "rylr896" && (s.pre < 4 || s.pre > 7)) return bad("preamble: RYLR896 accepts 4..7");
    int64_t pw = c.get_int("power_dbm", -1);
    s.power = static_cast<int>(pw >= 0 ? pw : reg.power_dbm);
    if (s.power > m.power_max) {
      if (pw >= 0) return bad("power_dbm: above this module's maximum");
      s.power = m.power_max;
    }
    s.addr = static_cast<int>(c.get_int("address"));
    s.password = c.get_string("password");
    if (!s.password.empty() && !hex8(s.password)) return bad("password: must be 8 hex digits (00000001..FFFFFFFF)");
    for (char& ch : s.password)
      if (ch >= 'a' && ch <= 'f') ch = static_cast<char>(ch - 'a' + 'A');
    dest_ = static_cast<int>(c.get_int("dest_address"));
    interop_ = c.get_bool("interop", true);
    timeout_ms_ = static_cast<uint32_t>(c.get_int("cmd_timeout_ms", 1000));
    LinkCaps k;
    k.mtu = static_cast<uint16_t>(kMtu);
    k.bitrate_bps = lora_bitrate(static_cast<uint8_t>(s.sf), kBwHz[s.bw], static_cast<uint8_t>(s.cr + 4));
    k.broadcast = true;
    k.half_duplex = true;
    k.reliable = false;
    bool amateur = c.get_bool("amateur");
    k.encryption_allowed = !amateur;
    k.requires_station_id = amateur;
    int64_t duty = c.get_int("duty_cycle_permille");
    k.duty_cycle_permille = static_cast<uint16_t>(duty ? duty : reg.duty_permille);
    k.cost = static_cast<uint8_t>(c.get_int("cost", 20));
    bool changed = !(s == set_);
    set_ = s;
    caps_ = k;
    region_ = reg.id;
    size_t qf = static_cast<size_t>(c.get_int("queue_frames", 16));
    q_.set_limits(qf, qf * (kMaxPayloadChars + 3));
    if (connected_ && changed) start_init(now());
    return true;
  }

  const char* type() const override { return "lora_at"; }
  LinkCaps caps() const override { return caps_; }

  bool send(ByteView frame, uint8_t priority) override {
    if (frame.empty() || frame.size() > kMtu) { ++st_.tx_refused; return false; }
    Bytes e;
    e.push_back(0);
    e.push_back(static_cast<uint8_t>(dest_ >> 8));
    e.push_back(static_cast<uint8_t>(dest_));
    e.insert(e.end(), frame.begin(), frame.end());
    if (!q_.push(ByteView(e), priority)) { ++st_.tx_refused; return false; }
    pump(now());
    return true;
  }

  bool send_interop_text(const InteropText& msg) override {
    if (!msg.network.empty() && msg.network != "lora_at") return false;
    std::string t = printable_text(ByteView(std::string_view(msg.text)), kMaxPayloadChars);
    while (!t.empty() && t.back() == ' ') t.pop_back();
    if (t.empty()) return false;
    if (starts_with(t, kMarker)) t = " " + std::string(utf8_truncate(t, kMaxPayloadChars - 1));
    int dest = dest_;
    if (!msg.to.empty()) {
      int64_t d = 0;
      if (!parse_int(trim(msg.to), &d) || d < 0 || d > 65535) return false;
      dest = static_cast<int>(d);
    }
    Bytes e;
    e.push_back(1);
    e.push_back(static_cast<uint8_t>(dest >> 8));
    e.push_back(static_cast<uint8_t>(dest));
    e.insert(e.end(), t.begin(), t.end());
    if (!q_.push(ByteView(e), 3)) return false;
    ++st_.interop_tx;
    pump(now());
    return true;
  }

  bool channel_busy() const override { return cur_.active && cur_.kind == Kind::Send; }

  void on_io_state(bool connected) override {
    connected_ = connected;
    line_.clear();
    skip_line_ = false;
    if (connected) {
      start_init(now());
    } else {
      cur_ = Cmd();
      plan_.clear();
      phase_ = Phase::Idle;
      set_state(LinkState::Down);
    }
  }

  void on_io_bytes(ByteView d) override {
    st_.io_rx_bytes += d.size();
    for (uint8_t b : d) {
      if (b == '\n') {
        if (skip_line_) { skip_line_ = false; line_.clear(); continue; }
        end_of_line();
        continue;
      }
      if (skip_line_) continue;
      if (line_.size() >= kMaxLine) {
        ++st_.rx_errors;
        line_.clear();
        skip_line_ = true;
        continue;
      }
      line_.push_back(static_cast<char>(b));
    }
  }

  // A "+XYZ", "+XYZ=..." line as the module prints them ("+OK", "+RCV=", "+ERR=", "+READY" ...).
  static bool module_line(const std::string& s) {
    if (s.size() < 3 || s[0] != '+') return false;
    size_t i = 1;
    while (i < s.size() && s[i] >= 'A' && s[i] <= 'Z') ++i;
    return i >= 3 && (i == s.size() || s[i] == '=');
  }

  // A '\n' arrived; line_ holds everything since the previous complete line.
  void end_of_line() {
    for (;;) {
      std::string l = line_;
      if (!l.empty() && l.back() == '\r') l.pop_back();
      size_t k = l.find("+RCV=");
      if (k != std::string::npos) {
        Rcv r;
        if (parse_rcv(l.substr(k), &r) == -1 && line_.size() < kMaxLine) {
          // The header claims more data than arrived. Either the payload contained a newline (keep
          // collecting) or the header was corrupt: if the segment that just ended is itself a
          // module line, drop the partial +RCV and handle that line on its own (resync).
          size_t nl = line_.rfind('\n');
          if (nl != std::string::npos) {
            std::string seg = line_.substr(nl + 1);
            if (!seg.empty() && seg.back() == '\r') seg.pop_back();
            if (module_line(seg)) {
              ++st_.rx_errors;
              ++resyncs_;
              line_.erase(0, nl + 1);
              continue;
            }
          }
          line_.push_back('\n');
          return;
        }
      }
      line_.clear();
      handle_line(l);
      return;
    }
  }

  void tick(uint64_t t) override {
    if (!connected_) return;
    if (cur_.active && t >= cur_.deadline) on_timeout(t);
    if (phase_ == Phase::Failed && t >= retry_at_) start_init(t);
    pump(t);
  }

  std::string status_json() const override {
    Json j = base_status();
    j.set("phase", phase_name()).set("model", set_.model).set("region", region_);
    j.set("rx_resyncs", resyncs_);
    if (!version_.empty()) j.set("version", version_);
    j.set("address", set_.addr).set("network_id", set_.net).set("band_hz", set_.band);
    j.set("sf", set_.sf).set("bandwidth_hz", kBwHz[set_.bw]).set("cr", set_.cr).set("preamble", set_.pre);
    j.set("power_dbm", set_.power);
    if (have_sig_) j.set("last_rssi_dbm", last_rssi_).set("last_snr_db", last_snr_).set("last_from", last_from_);
    j.set("tx_failed", tx_failed_).set("tx_timeouts", tx_timeouts_).set("module_errors", module_errors_);
    j.set("reinits", reinits_).set("flash_writes", flash_writes_);
    j.set("caps", caps_json(caps_));
    return j.dump();
  }

  bool reconfigure(const std::string& cfg, std::string* error) override {
    Json c;
    if (!parse_config(lora_at_schema(), cfg, &c, error)) return false;
    LoraAtLink tmp(host_);
    if (!tmp.configure(c, error)) return false;
    return configure(c, error);
  }

 private:
  struct Settings {
    std::string model = "rylr998";
    uint32_t band = 0;
    int sf = 9, bw = 7, cr = 1, pre = 12, net = 18, power = 14, addr = 0;
    std::string password;
    bool operator==(const Settings& o) const {
      return model == o.model && band == o.band && sf == o.sf && bw == o.bw && cr == o.cr && pre == o.pre &&
             net == o.net && power == o.power && addr == o.addr && password == o.password;
    }
  };
  enum class Kind : uint8_t {
    Probe, Version, QueryAddr, SetAddr, QueryNet, SetNet, SetBand, SetParam, SetPower, SetPin, VerifyParam,
    VerifyBand, Send
  };
  enum class Phase : uint8_t { Idle, Init, Running, Failed };
  struct Cmd {
    bool active = false;
    Kind kind = Kind::Probe;
    std::string text;
    uint64_t deadline = 0;
    uint8_t tries = 0;
    uint64_t sent_at = 0;
    uint32_t air = 0;
  };

  const char* phase_name() const {
    switch (phase_) {
      case Phase::Idle: return "idle";
      case Phase::Init: return "initialising";
      case Phase::Running: return "running";
      case Phase::Failed: return "failed";
    }
    return "idle";
  }

  std::string params_text() const {
    return std::to_string(set_.sf) + "," + std::to_string(set_.bw) + "," + std::to_string(set_.cr) + "," +
           std::to_string(set_.pre);
  }

  std::string command_for(Kind k) const {
    switch (k) {
      case Kind::Probe: return "AT";
      case Kind::Version: return "AT+VER?";
      case Kind::QueryAddr: return "AT+ADDRESS?";
      case Kind::SetAddr: return "AT+ADDRESS=" + std::to_string(set_.addr);
      case Kind::QueryNet: return "AT+NETWORKID?";
      case Kind::SetNet: return "AT+NETWORKID=" + std::to_string(set_.net);
      case Kind::SetBand: return "AT+BAND=" + std::to_string(set_.band);
      case Kind::SetParam: return "AT+PARAMETER=" + params_text();
      case Kind::SetPower: return "AT+CRFOP=" + std::to_string(set_.power);
      case Kind::SetPin: return "AT+CPIN=" + set_.password;
      case Kind::VerifyParam: return "AT+PARAMETER?";
      case Kind::VerifyBand: return "AT+BAND?";
      case Kind::Send: return "";
    }
    return "AT";
  }

  void start_init(uint64_t t) {
    ++reinits_;
    phase_ = Phase::Init;
    set_state(LinkState::Connecting);
    plan_ = {Kind::Probe, Kind::Version, Kind::QueryAddr, Kind::QueryNet, Kind::SetBand, Kind::SetParam, Kind::SetPower};
    if (!set_.password.empty()) plan_.push_back(Kind::SetPin);
    plan_.push_back(Kind::VerifyParam);
    plan_.push_back(Kind::VerifyBand);
    cur_ = Cmd();
    next_step(t);
  }

  void issue(Kind k, const std::string& text, uint64_t t, uint32_t timeout) {
    cur_.active = true;
    cur_.kind = k;
    cur_.text = text;
    cur_.deadline = t + timeout;
    cur_.sent_at = t;
    ++cur_.tries;
    std::string w = text + "\r\n";
    io_write(ByteView(std::string_view(w)));
  }

  void next_step(uint64_t t) {
    cur_ = Cmd();
    if (plan_.empty()) {
      phase_ = Phase::Running;
      last_error_.clear();
      set_state(LinkState::Up);
      pump(t);
      return;
    }
    Kind k = plan_.front();
    plan_.pop_front();
    issue(k, command_for(k), t, timeout_ms_);
  }

  void fail(const std::string& why, uint64_t t) {
    cur_ = Cmd();
    plan_.clear();
    phase_ = Phase::Failed;
    set_state(LinkState::Error, why.c_str());
    retry_at_ = t + kRetryIdleMs;
  }

  void on_timeout(uint64_t t) {
    if (cur_.kind == Kind::Send) {
      ++tx_timeouts_;
      cur_ = Cmd();
      if (++consecutive_tx_timeouts_ >= 3) {
        set_error("module stopped answering AT+SEND");
        consecutive_tx_timeouts_ = 0;
        start_init(t);
      }
      return;
    }
    uint8_t max_tries = cur_.kind == Kind::Probe ? 3 : cur_.kind == Kind::Version ? 1 : 2;
    if (cur_.tries < max_tries) {
      Cmd keep = cur_;
      issue(keep.kind, keep.text, t, timeout_ms_);
      cur_.tries = static_cast<uint8_t>(keep.tries + 1);
      return;
    }
    if (cur_.kind == Kind::Version) { next_step(t); return; }  // optional
    fail(cur_.kind == Kind::Probe ? "no answer from the LoRa module (check wiring, TX/RX swap, 115200 baud)"
                                  : "module did not answer " + cur_.text,
         t);
  }

  void handle_line(const std::string& raw) {
    // Drop leading noise (power-up glitches) before the '+'.
    size_t p = raw.find('+');
    if (p == std::string::npos) {
      if (!raw.empty()) ++garbage_lines_;
      return;
    }
    std::string l = raw.substr(p);
    uint64_t t = now();
    if (starts_with(l, "+RCV=")) { on_rcv(l); return; }
    if (starts_with(l, "+READY") || starts_with(l, "+RESET")) {
      if (starts_with(l, "+READY") && phase_ != Phase::Idle && !(phase_ == Phase::Init && cur_.kind == Kind::Probe)) {
        set_error("module restarted");
        start_init(t);
      }
      return;
    }
    if (!cur_.active) return;  // unsolicited / late answer
    if (starts_with(l, "+ERR=")) {
      int64_t code = 0;
      parse_int(l.substr(5), &code);
      ++module_errors_;
      std::string why = cur_.text.substr(0, cur_.text.find('=')) + " failed: +ERR=" + std::to_string(code) + " (" +
                        err_text(static_cast<int>(code)) + ")";
      switch (cur_.kind) {
        case Kind::Probe: next_step(t); return;  // a reply of any kind proves the module is there
        case Kind::Version: next_step(t); return;
        case Kind::Send:
          ++tx_failed_;
          set_error(why);
          cur_ = Cmd();
          pump(t);
          return;
        default: fail(why, t); return;
      }
    }
    switch (cur_.kind) {
      case Kind::Probe:
      case Kind::SetAddr:
      case Kind::SetNet:
      case Kind::SetBand:
      case Kind::SetParam:
      case Kind::SetPower:
      case Kind::SetPin:
        if (l == "+OK") {
          if (cur_.kind == Kind::SetAddr || cur_.kind == Kind::SetNet) ++flash_writes_;
          next_step(t);
        }
        return;
      case Kind::Version:
        if (starts_with(l, "+VER=")) {
          version_ = printable_text(ByteView(std::string_view(l)).sub(5), 40);
          next_step(t);
        } else if (l == "+OK") {
          next_step(t);
        }
        return;
      case Kind::QueryAddr:
      case Kind::QueryNet: {
        size_t eq = l.find('=');
        bool match = cur_.kind == Kind::QueryAddr ? starts_with(l, "+ADDRESS=") : starts_with(l, "+NETWORK");
        if (!match || eq == std::string::npos) return;
        int64_t v = -1;
        int_field(l.substr(eq + 1), &v);
        int want = cur_.kind == Kind::QueryAddr ? set_.addr : set_.net;
        if (v != want) plan_.push_front(cur_.kind == Kind::QueryAddr ? Kind::SetAddr : Kind::SetNet);
        next_step(t);
        return;
      }
      case Kind::VerifyParam:
        if (starts_with(l, "+PARAMETER=")) {
          std::string got;
          for (char ch : l.substr(11))
            if (ch != ' ') got.push_back(ch);
          if (got != params_text()) fail("module reports parameters " + printable_text(ByteView(std::string_view(got)), 30) +
                                             ", expected " + params_text(), t);
          else next_step(t);
        }
        return;
      case Kind::VerifyBand:
        if (starts_with(l, "+BAND=")) {
          std::string v = l.substr(6);
          size_t comma = v.find(',');
          if (comma != std::string::npos) v = v.substr(0, comma);
          int64_t f = 0;
          if (!int_field(v, &f) || f != static_cast<int64_t>(set_.band))
            fail("module reports a different frequency than configured", t);
          else next_step(t);
        }
        return;
      case Kind::Send:
        if (l == "+OK") {
          consecutive_tx_timeouts_ = 0;
          // The module transmits after accepting the command: leave the channel time on air.
          pacer_.reset();
          pacer_.spend(cur_.sent_at, 0, 0, cur_.air + 50);
          cur_ = Cmd();
          pump(t);
        }
        return;
    }
  }

  void on_rcv(const std::string& l) {
    Rcv r;
    if (parse_rcv(l, &r) != 1) { ++st_.rx_errors; return; }
    have_sig_ = true;
    last_rssi_ = r.rssi;
    last_snr_ = r.snr;
    last_from_ = r.addr;
    if (phase_ != Phase::Running && phase_ != Phase::Init) return;
    if (starts_with(r.data, kMarker)) {
      Bytes f;
      if (!base64_decode(r.data.substr(kMarkerLen), &f, kMtu) || f.empty()) { ++st_.rx_errors; return; }
      RxMeta m;
      m.has_rssi = m.has_snr = true;
      m.rssi_dbm = static_cast<float>(r.rssi);
      m.snr_db = static_cast<float>(r.snr);
      deliver(ByteView(f), m);
      return;
    }
    if (!interop_) return;
    std::string text = printable_text(ByteView(std::string_view(r.data)), kMaxPayloadChars);
    if (text.empty()) return;
    InteropText it;
    it.from = std::to_string(r.addr);
    it.to = "";
    it.text = text;
    it.network = "lora_at";
    interop(it);
  }

  void pump(uint64_t t) {
    if (phase_ != Phase::Running || cur_.active || q_.empty() || !pacer_.ready(t)) return;
    const Bytes* head = q_.peek();
    if (head->size() < 3) { Bytes junk; q_.pop(&junk); return; }
    bool mesh = (*head)[0] == 0;
    int dest = (*head)[1] << 8 | (*head)[2];
    ByteView body = ByteView(*head).sub(3);
    std::string data = mesh ? std::string(kMarker) + base64_encode(body) : body.to_string();
    // ~8 bytes of module framing on air (Reyax guide)
    uint32_t air = lora_airtime_ms(data.size() + 8, static_cast<uint8_t>(set_.sf), kBwHz[set_.bw],
                                   static_cast<uint8_t>(set_.cr + 4), static_cast<uint16_t>(set_.pre));
    if (caps_.duty_cycle_permille < 1000 && !duty_.allows(t, air, caps_.duty_cycle_permille)) return;
    Bytes e;
    q_.pop(&e);
    if (mesh) {
      ++st_.tx_frames;
      st_.tx_bytes += e.size() - 3;
    }
    st_.airtime_ms += air;
    duty_.add(t, air);
    std::string cmdline = "AT+SEND=" + std::to_string(dest) + "," + std::to_string(data.size()) + "," + data;
    cur_ = Cmd();
    issue(Kind::Send, cmdline, t, timeout_ms_ + 2 * air);
    cur_.air = air;
  }

  LinkCaps caps_;
  Settings set_;
  std::string region_, version_;
  int dest_ = 0;
  bool interop_ = true;
  uint32_t timeout_ms_ = 1000;
  bool connected_ = false;
  Phase phase_ = Phase::Idle;
  std::deque<Kind> plan_;
  Cmd cur_;
  uint64_t retry_at_ = 0;
  std::string line_;
  bool skip_line_ = false;
  uint64_t resyncs_ = 0;
  Pacer pacer_;
  DutyCycle duty_;
  bool have_sig_ = false;
  int64_t last_rssi_ = 0, last_snr_ = 0, last_from_ = 0;
  uint64_t tx_failed_ = 0, tx_timeouts_ = 0, module_errors_ = 0, reinits_ = 0, flash_writes_ = 0, garbage_lines_ = 0;
  uint8_t consecutive_tx_timeouts_ = 0;
};

std::unique_ptr<Link> create_lora_at(const Json& c, LinkHost& h, std::string* error) {
  auto l = std::make_unique<LoraAtLink>(h);
  if (!l->configure(c, error)) return nullptr;
  return l;
}

}  // namespace

const LinkTypeInfo& lora_at_type() {
  static const LinkTypeInfo t = {
      "lora_at",
      "LoRa AT-command module (Reyax)",
      "Uses an inexpensive AT-command LoRa module such as the REYAX RYLR998 or RYLR896 on a serial port as "
      "a long-range radio, and shows plain text from ordinary modules as interop messages.",
      {"serial", "ble", "bt_spp", "tcp"},
      lora_at_schema,
      create_lora_at,
  };
  return t;
}

}  // namespace link
}  // namespace ember
