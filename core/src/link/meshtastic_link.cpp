// EmberMesh core / link — "meshtastic": a stock Meshtastic device used as a LoRa modem through its
// client API (the same protocol the Meshtastic apps / Python CLI speak over USB serial or TCP 4403).
//
// Framing (serial / TCP):  0x94 0xC3 len_hi len_lo  protobuf (ToRadio / FromRadio, len <= 512).
// Bytes outside frames are the device's debug log and are ignored (last line kept in status).
// BLE framing option: one bare protobuf per endpoint datagram (the platform reads FromRadio /
// writes ToRadio GATT characteristics).
//
// Handshake: on connect we send 32 x 0xC3 (wakes a sleeping serial API), then ToRadio.want_config_id
// = random nonce. The device answers with my_info, node_info..., config..., channels, metadata and
// finally config_complete_id = nonce -> link Up. Idle timeout per attempt, 3 attempts, then Error
// with periodic retries. A heartbeat ToRadio keeps the serial API alive.
//
// On air: each mesh frame is the payload of a broadcast MeshPacket, Data.portnum = PRIVATE_APP
// (256), on the configured channel index, hop_limit from config. Payload <= 233 bytes
// (Constants.DATA_PAYLOAD_LEN). TEXT_MESSAGE_APP (1) on that channel / to us is bridged as interop
// text "meshtastic" in both directions.
#include <cmath>
#include <cstdio>
#include <memory>

#include "link/drivers.h"
#include "link/meshtastic_proto.h"

namespace ember {
namespace link {
namespace {

struct PresetInfo {
  const char* id;
  uint32_t value;  // Config.LoRaConfig.ModemPreset enum
  const char* label;
  double bw_khz;
  uint8_t sf, cr;  // cr = 5..8 (4/5 .. 4/8)
};

const PresetInfo kPresets[] = {
    {"LONG_FAST", 0, "Long Range - Fast (default)", 250, 11, 5},
    {"LONG_MODERATE", 7, "Long Range - Moderate", 125, 11, 8},
    {"LONG_SLOW", 1, "Long Range - Slow", 125, 12, 8},
    {"VERY_LONG_SLOW", 2, "Very Long Range - Slow", 62.5, 12, 8},
    {"MEDIUM_SLOW", 3, "Medium Range - Slow", 250, 10, 5},
    {"MEDIUM_FAST", 4, "Medium Range - Fast", 250, 9, 5},
    {"SHORT_SLOW", 5, "Short Range - Slow", 250, 8, 5},
    {"SHORT_FAST", 6, "Short Range - Fast", 250, 7, 5},
    {"SHORT_TURBO", 8, "Short Range - Turbo (500 kHz)", 500, 7, 5},
    {"LONG_TURBO", 9, "Long Range - Turbo (500 kHz)", 500, 11, 8},
};

const PresetInfo* preset_by_id(const std::string& id) {
  for (const auto& p : kPresets)
    if (id == p.id) return &p;
  return nullptr;
}
const PresetInfo* preset_by_value(uint32_t v) {
  for (const auto& p : kPresets)
    if (v == p.value) return &p;
  return nullptr;
}

struct RegionInfo {
  const char* id;
  uint32_t value;  // Config.LoRaConfig.RegionCode
  const char* label;
  uint16_t duty_permille;
};

const RegionInfo kRegions[] = {
    {"US", 1, "United States (915 MHz)", 1000},  {"EU_868", 3, "Europe 868 MHz (10 % duty cycle)", 100},
    {"EU_433", 2, "Europe 433 MHz (10 % duty cycle)", 100}, {"ANZ", 6, "Australia / New Zealand (915 MHz)", 1000},
    {"UA_868", 15, "Ukraine 868 MHz (1 % duty cycle)", 10}, {"UA_433", 14, "Ukraine 433 MHz (10 % duty cycle)", 100},
    {"IN", 10, "India (865 MHz)", 1000},          {"JP", 5, "Japan (920 MHz)", 1000},
    {"KR", 7, "Korea (920 MHz)", 1000},           {"TW", 8, "Taiwan (920 MHz)", 1000},
    {"CN", 4, "China (470 MHz)", 1000},           {"RU", 9, "Russia (868 MHz)", 1000},
    {"NZ_865", 11, "New Zealand 865 MHz", 1000},  {"TH", 12, "Thailand (920 MHz)", 1000},
    {"BR_902", 26, "Brazil (902 MHz)", 1000},     {"LORA_24", 13, "2.4 GHz worldwide", 1000},
};

const RegionInfo* region_by_id(const std::string& id) {
  for (const auto& r : kRegions)
    if (id == r.id) return &r;
  return nullptr;
}
const RegionInfo* region_by_value(uint32_t v) {
  for (const auto& r : kRegions)
    if (v == r.value) return &r;
  return nullptr;
}

// LoRa time on air (Semtech AN1200.13), explicit header, CRC on, 16-symbol preamble (Meshtastic).
uint32_t lora_airtime_ms(size_t payload_len, double bw_khz, uint8_t sf, uint8_t cr) {
  double ts = std::pow(2.0, sf) / (bw_khz * 1000.0);
  int de = ts > 0.016 ? 1 : 0;
  double num = 8.0 * static_cast<double>(payload_len) - 4.0 * sf + 28 + 16;
  double den = 4.0 * (sf - 2 * de);
  double n = std::ceil(num / den) * cr;
  if (n < 0) n = 0;
  double symbols = 16 + 4.25 + 8 + n;
  return static_cast<uint32_t>(std::ceil(symbols * ts * 1000.0));
}

uint32_t lora_bitrate(double bw_khz, uint8_t sf, uint8_t cr) {
  double br = sf * (bw_khz * 1000.0) / std::pow(2.0, sf) * 4.0 / cr;
  return br < 1 ? 1 : static_cast<uint32_t>(br);
}

std::string node_id(uint32_t n) {
  char b[12];
  std::snprintf(b, sizeof b, "!%08x", n);
  return b;
}

bool parse_node_id(const std::string& s, uint32_t* out) {
  std::string t = s;
  if (!t.empty() && t[0] == '!') t = t.substr(1);
  if (t.empty() || t.size() > 8) return false;
  uint32_t v = 0;
  for (char c : t) {
    int d;
    if (c >= '0' && c <= '9') d = c - '0';
    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
    else return false;
    v = v << 4 | static_cast<uint32_t>(d);
  }
  *out = v;
  return true;
}

constexpr size_t kMtPacketOverhead = 16 + 6;  // Meshtastic radio header + Data protobuf
constexpr uint32_t kAttempts = 3;
constexpr uint32_t kErrorRetryMs = 30000;
constexpr uint32_t kWakeDelayMs = 100;
constexpr uint32_t kQueueWaitMs = 10000;   // give up waiting for a QueueStatus after this
constexpr size_t kMaxTextBytes = 200;
constexpr size_t kMaxNameBytes = 64;  // long_name is <= 39 bytes on real devices
constexpr char kTagMesh = 'M', kTagText = 'T';

Schema mt_schema() {
  std::vector<FieldOption> presets = {{"auto", "As configured on the device"}};
  for (const auto& p : kPresets) presets.push_back({p.id, p.label});
  std::vector<FieldOption> regions = {{"auto", "As configured on the device"}};
  for (const auto& r : kRegions) regions.push_back({r.id, r.label});
  return {
      select("framing", "Connection", "stream",
             {{"stream", "USB serial or TCP (Wi-Fi, port 4403)"},
              {"ble", "Bluetooth LE (one message per packet)"}},
             "How the phone or computer is connected to the Meshtastic device."),
      num("channel", "Meshtastic channel", 0, 0, 7,
          "Channel slot on the device to use (0 = primary). Every Ember node must use the same channel."),
      num("hop_limit", "Hop limit", 3, 1, 7, "How many times Meshtastic routers may repeat each packet."),
      select("modem_preset", "Radio preset", "auto", presets,
             "Only used to estimate speed and airtime; the device keeps its own setting. 'As configured' reads it from the device."),
      select("region", "Region", "auto", regions,
             "Legal region of the device; sets the duty-cycle limit (Europe 868/433 MHz: 10 %)."),
      boolean("licensed", "Licensed (ham) mode", false,
              "Turn on if the device runs Meshtastic's licensed amateur mode: traffic is then sent unencrypted with your callsign."),
      boolean("text_bridge", "Bridge Meshtastic text chat", true,
              "Show Meshtastic text messages on this channel in Ember, and allow replying to them."),
      num("heartbeat_s", "Keep-alive interval", 60, 10, 900,
          "How often to ping the device so it keeps the connection open.", "s").adv(),
      num("handshake_timeout_s", "Connect timeout", 10, 2, 120,
          "Give up a connection attempt when the device sends nothing for this long.", "s").adv(),
      num("queue_frames", "Queue size", 16, 1, 128, "Packets waiting to be handed to the device.", "frames").adv(),
      num("cost", "Routing cost", 0, 0, 255, "Lower is preferred. 0 = default (20).").adv(),
  };
}

class MeshtasticLink final : public LinkBase {
 public:
  explicit MeshtasticLink(LinkHost& h) : LinkBase(h) {}

  bool configure(const Json& c, std::string* error) {
    ble_ = c.get_string("framing") == "ble";
    channel_ = static_cast<uint32_t>(c.get_int("channel"));
    hop_limit_ = static_cast<uint32_t>(c.get_int("hop_limit", 3));
    preset_cfg_ = c.get_string("modem_preset", "auto");
    region_cfg_ = c.get_string("region", "auto");
    licensed_ = c.get_bool("licensed");
    text_bridge_ = c.get_bool("text_bridge", true);
    heartbeat_ms_ = static_cast<uint32_t>(c.get_int("heartbeat_s", 60)) * 1000u;
    timeout_ms_ = static_cast<uint32_t>(c.get_int("handshake_timeout_s", 10)) * 1000u;
    size_t qf = static_cast<size_t>(c.get_int("queue_frames", 16));
    q_.set_limits(qf, qf * (mt::kMaxToFromRadio + 1));
    int64_t cost = c.get_int("cost");
    cost_ = static_cast<uint8_t>(cost ? cost : 20);
    return true;
  }

  const char* type() const override { return "meshtastic"; }

  LinkCaps caps() const override {
    LinkCaps k;
    const PresetInfo* p = effective_preset();
    k.mtu = static_cast<uint16_t>(mt::kDataPayloadLen);
    k.bitrate_bps = lora_bitrate(p->bw_khz, p->sf, p->cr);
    k.broadcast = true;
    k.half_duplex = true;
    k.reliable = false;
    k.encryption_allowed = !licensed_;
    k.requires_station_id = licensed_;
    k.duty_cycle_permille = duty_permille();
    k.cost = cost_;
    return k;
  }

  bool send(ByteView frame, uint8_t priority) override {
    if (frame.empty() || frame.size() > mt::kDataPayloadLen) { ++st_.tx_refused; return false; }
    mt::MeshPacket p;
    p.to = mt::kBroadcast;
    p.channel = channel_;
    p.has_decoded = true;
    p.decoded.portnum = mt::kPortPrivate;
    p.decoded.payload = frame.to_bytes();
    p.hop_limit = hop_limit_;
    p.priority = priority == 0 ? 100u : priority <= 2 ? 70u : priority >= 7 ? 10u : 64u;
    if (!queue_packet(p, kTagMesh, priority)) { ++st_.tx_refused; return false; }
    pump(now());
    return true;
  }

  bool send_interop_text(const InteropText& msg) override {
    if (!text_bridge_ || msg.text.empty()) return false;
    if (!msg.network.empty() && msg.network != "meshtastic") return false;
    mt::MeshPacket p;
    p.to = mt::kBroadcast;
    if (!msg.to.empty()) {
      if (!parse_node_id(msg.to, &p.to)) return false;
      p.want_ack = true;
    }
    std::string t = utf8_sanitize(msg.text);
    t = std::string(utf8_truncate(t, kMaxTextBytes));
    if (t.empty()) return false;
    p.channel = channel_;
    p.has_decoded = true;
    p.decoded.portnum = mt::kPortText;
    p.decoded.payload.assign(t.begin(), t.end());
    p.hop_limit = hop_limit_;
    if (!queue_packet(p, kTagText, 2)) return false;
    ++st_.interop_tx;
    pump(now());
    return true;
  }

  bool channel_busy() const override { return !pacer_.ready(host_.now_ms()); }

  void on_io_state(bool connected) override {
    connected_ = connected;
    deframer_.reset();
    pacer_.reset();
    if (!connected) {
      set_state(LinkState::Down);
      return;
    }
    attempts_ = 0;
    start_handshake(now());
  }

  void on_io_bytes(ByteView d) override {
    if (d.empty()) return;
    st_.io_rx_bytes += d.size();
    uint64_t t = now();
    last_rx_io_ = t;
    if (ble_) {
      if (d.size() > mt::kMaxToFromRadio) { ++st_.rx_errors; return; }
      handle(d, t);
      return;
    }
    uint64_t bad = deframer_.bad();
    deframer_.feed(d, [&](ByteView pkt) { handle(pkt, t); }, [&](char c) { log_char(c); });
    framing_errors_ += deframer_.bad() - bad;
  }

  void tick(uint64_t t) override {
    if (!connected_) return;
    if (wake_pending_ && t >= wake_at_) {
      wake_pending_ = false;
      mt::ToRadio w;
      w.kind = mt::ToKind::WantConfig;
      w.want_config_id = nonce_;
      write_msg(w);
      hs_last_ = t;
    }
    if (state_ != LinkState::Up) {
      bool waiting = !wake_pending_;
      if (state_ == LinkState::Error) {
        if (t >= retry_at_) { attempts_ = 0; start_handshake(t); }
      } else if (waiting && ((t >= hs_last_ + timeout_ms_ && t >= hs_progress_ + timeout_ms_) ||
                             t >= hs_started_ + attempt_deadline_ms())) {
        // Only decoded API messages count as progress: a device that just prints its debug log
        // (or line noise) must not keep an attempt alive forever (security review M5).
        if (attempts_ >= kAttempts) {
          set_state(LinkState::Error, "Meshtastic device did not answer the configuration request");
          retry_at_ = t + kErrorRetryMs;
        } else {
          start_handshake(t);
        }
      }
      return;
    }
    if (t >= last_heartbeat_ + heartbeat_ms_) {
      mt::ToRadio h;
      h.kind = mt::ToKind::Heartbeat;
      write_msg(h);
      last_heartbeat_ = t;
      ++heartbeats_;
    }
    if (device_free_known_ && device_free_ == 0 && t >= queue_wait_since_ + kQueueWaitMs) device_free_known_ = false;
    pump(t);
  }

  std::string status_json() const override {
    Json j = base_status();
    j.set("framing", ble_ ? "ble" : "stream");
    if (my_num_) j.set("node_id", node_id(my_num_)).set("node_num", my_num_);
    if (!long_name_.empty()) j.set("long_name", long_name_);
    if (!short_name_.empty()) j.set("short_name", short_name_);
    if (!firmware_.empty()) j.set("firmware_version", firmware_);
    j.set("nodes_known", nodes_known_);
    const PresetInfo* p = effective_preset();
    j.set("modem_preset", p->id);
    const RegionInfo* r = effective_region();
    j.set("region", r ? r->id : "unknown");
    if (has_lora_) j.set("device_region_code", dev_lora_.region).set("device_tx_enabled", dev_lora_.tx_enabled);
    if (has_rssi_) j.set("last_rssi_dbm", last_rssi_);
    if (has_snr_) j.set("last_snr_db", static_cast<double>(last_snr_));
    if (device_free_known_) j.set("device_queue_free", device_free_).set("device_queue_max", device_max_);
    j.set("handshakes", handshakes_).set("heartbeats", heartbeats_);
    j.set("framing_errors", framing_errors_).set("decode_errors", decode_errors_);
    j.set("ignored_packets", ignored_).set("encrypted_packets", encrypted_).set("own_packets", own_);
    if (!log_last_.empty()) j.set("device_log", log_last_);
    j.set("caps", caps_json(caps()));
    return j.dump();
  }

  bool reconfigure(const std::string& cfg, std::string* error) override {
    Json c;
    if (!parse_config(mt_schema(), cfg, &c, error)) return false;
    bool was_ble = ble_;
    if (!configure(c, error)) return false;
    if (connected_ && was_ble != ble_) {
      deframer_.reset();
      attempts_ = 0;
      start_handshake(now());
    }
    return true;
  }

 private:
  const PresetInfo* effective_preset() const {
    if (preset_cfg_ != "auto") {
      const PresetInfo* p = preset_by_id(preset_cfg_);
      if (p) return p;
    }
    if (has_lora_ && dev_lora_.use_preset) {
      const PresetInfo* p = preset_by_value(dev_lora_.modem_preset);
      if (p) return p;
    }
    return &kPresets[0];
  }
  const RegionInfo* effective_region() const {
    if (region_cfg_ != "auto") return region_by_id(region_cfg_);
    if (has_lora_) return region_by_value(dev_lora_.region);
    return nullptr;
  }
  uint16_t duty_permille() const {
    const RegionInfo* r = effective_region();
    return r ? r->duty_permille : 1000;
  }

  bool queue_packet(mt::MeshPacket& p, char tag, uint8_t priority) {
    p.id = random_u32() | 1u;
    mt::ToRadio t;
    t.kind = mt::ToKind::Packet;
    t.packet = p;
    Bytes b;
    b.push_back(static_cast<uint8_t>(tag));
    b.push_back(static_cast<uint8_t>(p.decoded.payload.size() >> 8));
    b.push_back(static_cast<uint8_t>(p.decoded.payload.size()));
    Bytes enc = mt::encode_to_radio(t);
    b.insert(b.end(), enc.begin(), enc.end());
    return q_.push(ByteView(b), priority);
  }

  void write_msg(const mt::ToRadio& t) { write_raw(ByteView(mt::encode_to_radio(t))); }
  void write_raw(ByteView pb) {
    if (ble_) io_write(pb);
    else io_write(ByteView(mt::frame(pb)));
  }

  void start_handshake(uint64_t t) {
    ++attempts_;
    ++handshakes_;
    set_state(LinkState::Connecting);
    do {
      nonce_ = random_u32();
    } while (nonce_ == 0 || nonce_ == 69420 || nonce_ == 69421);  // reserved "config/nodes only" nonces
    nodes_known_ = 0;
    if (!ble_) {
      Bytes wake(32, mt::kStart2);
      io_write(ByteView(wake));
      wake_pending_ = true;
      wake_at_ = t + kWakeDelayMs;
    } else {
      wake_pending_ = false;
      mt::ToRadio w;
      w.kind = mt::ToKind::WantConfig;
      w.want_config_id = nonce_;
      write_msg(w);
    }
    hs_last_ = t;
    hs_started_ = t;
    hs_progress_ = 0;
  }

  // Hard limit for one attempt even while config messages keep arriving (a device with a huge
  // node database on a slow link may take a while, but never forever).
  uint64_t attempt_deadline_ms() const {
    uint64_t d = static_cast<uint64_t>(timeout_ms_) * 6;
    return d < 120000 ? 120000 : d;
  }

  void pump(uint64_t t) {
    if (state_ != LinkState::Up) return;
    while (!q_.empty()) {
      if (!pacer_.ready(t)) return;
      if (device_free_known_ && device_free_ == 0) return;
      const Bytes* head = q_.peek();
      size_t payload = static_cast<size_t>((*head)[1]) << 8 | (*head)[2];
      const PresetInfo* p = effective_preset();
      uint32_t air = lora_airtime_ms(payload + kMtPacketOverhead, p->bw_khz, p->sf, p->cr);
      uint16_t duty = duty_permille();
      if (duty < 1000 && !duty_.allows(t, air, duty)) return;
      Bytes item;
      q_.pop(&item);
      char tag = static_cast<char>(item[0]);
      ByteView pb = ByteView(item).sub(3);
      write_raw(pb);
      if (tag == kTagMesh) {
        ++st_.tx_frames;
        st_.tx_bytes += payload;
      }
      st_.airtime_ms += air;
      if (duty < 1000) duty_.add(t, air);
      pacer_.spend(t, 0, 0, air);
      if (device_free_known_ && device_free_ > 0) {
        --device_free_;
        if (device_free_ == 0) queue_wait_since_ = t;
      }
    }
  }

  void log_char(char c) {
    if (c == '\n' || c == '\r') {
      if (!log_line_.empty()) log_last_ = printable_text(ByteView(std::string_view(log_line_)), 120);
      log_line_.clear();
      return;
    }
    if (log_line_.size() < 200) log_line_.push_back(c);
  }

  void handle(ByteView pb, uint64_t t) {
    mt::FromRadio f;
    if (!mt::decode_from_radio(pb, &f)) {
      ++decode_errors_;
      ++st_.rx_errors;
      return;
    }
    hs_progress_ = t;
    switch (f.kind) {
      case mt::FromKind::MyInfo: my_num_ = f.my_node_num; break;
      case mt::FromKind::NodeInfo:
        if (nodes_known_ < 100000) ++nodes_known_;
        if (my_num_ && f.node.num == my_num_) {
          // Device-supplied strings end up in status / UI: no control characters, bounded (L9).
          long_name_ = printable_text(ByteView(std::string_view(f.node.long_name)), kMaxNameBytes);
          short_name_ = printable_text(ByteView(std::string_view(f.node.short_name)), kMaxNameBytes);
        }
        break;
      case mt::FromKind::Config:
        if (f.has_lora) { dev_lora_ = f.lora; has_lora_ = true; }
        break;
      case mt::FromKind::Metadata:
        firmware_ = printable_text(ByteView(std::string_view(f.firmware_version)), kMaxNameBytes);
        break;
      case mt::FromKind::ConfigComplete:
        if (state_ != LinkState::Up && f.config_complete_id == nonce_ && !wake_pending_) {
          set_state(LinkState::Up);
          last_error_.clear();
          attempts_ = 0;
          last_heartbeat_ = t;
          pump(t);
        }
        break;
      case mt::FromKind::Rebooted:
        if (connected_) { attempts_ = 0; start_handshake(t); }
        break;
      case mt::FromKind::QueueStatus:
        device_free_ = f.queue.free;
        device_max_ = f.queue.maxlen;
        device_free_known_ = f.queue.maxlen > 0;
        if (device_free_ == 0) queue_wait_since_ = t;
        if (f.queue.res != 0) set_error("device refused a packet (queue status " + std::to_string(f.queue.res) + ")");
        break;
      case mt::FromKind::Packet: on_packet(f.packet); break;
      default: break;
    }
  }

  void on_packet(const mt::MeshPacket& p) {
    if (my_num_ && p.from == my_num_) { ++own_; return; }
    if (!p.has_decoded) { ++encrypted_; return; }
    if (p.rx_rssi != 0) { has_rssi_ = true; last_rssi_ = p.rx_rssi; }
    if (p.has_snr) { has_snr_ = true; last_snr_ = p.rx_snr; }
    bool to_me = my_num_ && p.to == my_num_;
    if (p.decoded.portnum == mt::kPortPrivate) {
      if (p.channel != channel_ || p.decoded.payload.empty()) { ++ignored_; return; }
      RxMeta m;
      if (p.rx_rssi != 0) { m.has_rssi = true; m.rssi_dbm = static_cast<float>(p.rx_rssi); }
      if (p.has_snr) { m.has_snr = true; m.snr_db = p.rx_snr; }
      deliver(ByteView(p.decoded.payload), m);
      return;
    }
    if (p.decoded.portnum == mt::kPortText && text_bridge_ && (p.channel == channel_ || to_me)) {
      InteropText it;
      it.from = node_id(p.from);
      it.to = p.to == mt::kBroadcast ? "" : node_id(p.to);
      it.text = printable_text(ByteView(p.decoded.payload), 512);
      it.network = "meshtastic";
      if (it.text.empty()) { ++ignored_; return; }
      interop(it);
      return;
    }
    ++ignored_;
  }

  // config
  bool ble_ = false;
  uint32_t channel_ = 0, hop_limit_ = 3;
  std::string preset_cfg_ = "auto", region_cfg_ = "auto";
  bool licensed_ = false, text_bridge_ = true;
  uint32_t heartbeat_ms_ = 60000, timeout_ms_ = 10000;
  uint8_t cost_ = 20;
  // session
  bool connected_ = false;
  mt::Deframer deframer_;
  uint32_t nonce_ = 0, attempts_ = 0;
  bool wake_pending_ = false;
  uint64_t wake_at_ = 0, hs_last_ = 0, retry_at_ = 0, last_rx_io_ = 0, last_heartbeat_ = 0;
  uint64_t hs_started_ = 0, hs_progress_ = 0;
  uint32_t my_num_ = 0;
  std::string long_name_, short_name_, firmware_;
  uint64_t nodes_known_ = 0;
  bool has_lora_ = false;
  mt::LoraConfig dev_lora_;
  bool device_free_known_ = false;
  uint32_t device_free_ = 0, device_max_ = 0;
  uint64_t queue_wait_since_ = 0;
  Pacer pacer_;
  DutyCycle duty_;
  bool has_rssi_ = false, has_snr_ = false;
  int32_t last_rssi_ = 0;
  float last_snr_ = 0;
  uint64_t handshakes_ = 0, heartbeats_ = 0, framing_errors_ = 0, decode_errors_ = 0;
  uint64_t ignored_ = 0, encrypted_ = 0, own_ = 0;
  std::string log_line_, log_last_;
};

std::unique_ptr<Link> create_mt(const Json& c, LinkHost& h, std::string* error) {
  auto l = std::make_unique<MeshtasticLink>(h);
  if (!l->configure(c, error)) return nullptr;
  return l;
}

}  // namespace

const LinkTypeInfo& meshtastic_type() {
  static const LinkTypeInfo t = {
      "meshtastic",
      "Meshtastic device (LoRa)",
      "Uses a stock Meshtastic radio as a long-range LoRa modem over USB, Wi-Fi (TCP) or Bluetooth. "
      "Ember traffic rides in private-app packets on one Meshtastic channel; Meshtastic text chat on "
      "that channel is bridged into Ember.",
      {"serial", "tcp", "ble"},
      mt_schema,
      create_mt,
  };
  return t;
}

}  // namespace link
}  // namespace ember
