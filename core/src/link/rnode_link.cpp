// EmberMesh core / link — "rnode": LoRa boards running the RNode firmware (markqvist/RNode_Firmware:
// LilyGO T-Beam / LoRa32, Heltec, RAK4631, homebrew RNodes) over serial, USB, Bluetooth or TCP.
//
// Host protocol (verified against RNode_Firmware Framing.h and Reticulum RNodeInterface.py):
//   KISS framing: FEND(0xC0) command escaped(args) FEND, FESC 0xDB / TFEND 0xDC / TFESC 0xDD; the
//   whole command byte is used (no KISS port nibble). Multi-byte arguments are big-endian.
//   0x00 DATA      payload = one LoRa packet (firmware splits > 255-byte packets in two, max 508)
//   0x01 FREQUENCY u32 Hz    0x02 BANDWIDTH u32 Hz    0x03 TXPOWER u8 dBm    0x04 SF u8    0x05 CR u8 (5..8)
//   0x06 RADIO_STATE u8 (0 off, 1 on, 0xFF ask)   0x08 DETECT 0x73 -> device answers 0x08 0x46
//   0x0A LEAVE 0xFF   0x0B ST_ALOCK / 0x0C LT_ALOCK u16 = percent * 100   0x0F READY (flow control)
//   0x21 STAT_RX u32  0x22 STAT_TX u32  0x23 STAT_RSSI u8 (dBm = v - 157)  0x24 STAT_SNR s8 (dB = v / 4)
//   0x25 STAT_CHTM (airtime/channel load u16 % * 100 x4, current RSSI, noise floor, interference)
//   0x26 STAT_PHYPRM   0x27 STAT_BAT (state, percent)   0x29 STAT_TEMP (v - 120 C)
//   0x48 PLATFORM (0x90 AVR, 0x80 ESP32, 0x70 nRF52)   0x49 MCU   0x47 BOARD   0x50 FW_VERSION (major, minor)
//   0x55 RESET 0xF8 (device announces a reboot)   0x90 ERROR u8 (1 init radio, 2 TX failed, 3 EEPROM locked,
//   4 queue full, 5 memory low, 6 modem timeout)
// Every configuration command is echoed by the device with the value it actually applied; like
// Reticulum we only go Up when frequency (+-100 Hz), bandwidth, TX power, SF, CR and radio state ON
// all match what we asked for.
//
// On air: one mesh frame per RNode DATA packet, unchanged. RNode adds no addressing, so this shares
// the channel with Reticulum peers on the same settings, but the packets are not Reticulum packets
// (each side ignores the other's traffic as malformed).
#include <cmath>
#include <cstdio>
#include <memory>

#include "framing/slip.h"
#include "link/drivers.h"
#include "link/lora_common.h"

namespace ember {
namespace link {
namespace {

namespace cmd {
constexpr uint8_t DATA = 0x00, FREQUENCY = 0x01, BANDWIDTH = 0x02, TXPOWER = 0x03, SF = 0x04, CR = 0x05,
                  RADIO_STATE = 0x06, DETECT = 0x08, LEAVE = 0x0A, ST_ALOCK = 0x0B,
                  LT_ALOCK = 0x0C, READY = 0x0F, STAT_RX = 0x21, STAT_TX = 0x22, STAT_RSSI = 0x23,
                  STAT_SNR = 0x24, STAT_CHTM = 0x25, STAT_BAT = 0x27,
                  STAT_TEMP = 0x29, BOARD = 0x47, PLATFORM = 0x48, MCU = 0x49, FW_VERSION = 0x50,
                  RESET = 0x55, ERROR = 0x90;
constexpr uint8_t DETECT_REQ = 0x73, DETECT_RESP = 0x46;
}  // namespace cmd

constexpr int kRssiOffset = 157;
constexpr size_t kHwMtu = 508;
constexpr uint32_t kConfigTimeoutMs = 2500;
constexpr uint32_t kRetryIdleMs = 15000;  // after a failed handshake, try again this often
constexpr uint8_t kDetectTries = 3;

const char* const kBandwidths[] = {"7800", "10400", "15600", "20800", "31250", "41700",
                                   "62500", "125000", "250000", "500000"};

Schema rnode_schema() {
  std::vector<FieldOption> bw;
  for (const char* b : kBandwidths) {
    int64_t v = 0;
    parse_int(b, &v);
    char label[32];
    std::snprintf(label, sizeof label, "%.1f kHz", static_cast<double>(v) / 1000.0);
    bw.push_back({b, label});
  }
  return {
      select("region", "Region", "eu868", lora_region_options(),
             "Where you are. Sets a legal default frequency, power and duty-cycle limit."),
      num("frequency_hz", "Frequency", 0, 0, 3000000000LL,
          "Exact centre frequency. 0 = the region's default channel. All stations must match.", "Hz"),
      select("bandwidth_hz", "Bandwidth", "125000", bw,
             "LoRa bandwidth. Narrower reaches further but is slower. All stations must match."),
      num("sf", "Spreading factor", 9, 5, 12,
          "7 = fast/short range ... 12 = slow/long range. All stations must match."),
      num("cr", "Coding rate", 5, 5, 8, "Error-correction overhead 4/5 ... 4/8. All stations must match.").adv(),
      num("tx_power_dbm", "Transmit power", -1, -1, 30,
          "-1 = the region's conservative default. Check your local legal limit and your board's maximum.", "dBm"),
      real("st_alock_pct", "Short-term airtime limit", 0, 0, 100, 0.01,
           "Firmware-enforced airtime cap over 15 s, in percent. 0 = not set.", "%").adv(),
      real("lt_alock_pct", "Long-term airtime limit", 0, 0, 100, 0.01,
           "Firmware-enforced airtime cap over 1 hour, in percent. 0 = use the region's duty-cycle limit.", "%").adv(),
      num("duty_cycle_permille", "Duty-cycle limit", 0, 0, 1000,
          "Transmit time limit in thousandths (100 = 10 %). 0 = the region's legal limit.", "‰").adv(),
      num("mtu", "Largest frame", 254, 32, 508,
          "254 fits one LoRa packet; up to 508 lets the firmware split a frame over two packets.", "bytes").adv(),
      boolean("amateur", "Amateur radio operation", false,
              "Turn on when operating under an amateur licence: traffic is then unencrypted and must carry your callsign."),
      num("cost", "Routing cost", 20, 1, 255, "Lower is preferred when several links reach a node.").adv(),
      num("queue_frames", "Queue size", 32, 1, 256, "Frames waiting to be sent before new ones are refused.", "frames").adv(),
      num("detect_timeout_ms", "Detect timeout", 2000, 200, 20000,
          "How long to wait for the board to answer before retrying (Bluetooth may need more).", "ms").adv(),
  };
}

const char* error_text(uint8_t e) {
  switch (e) {
    case 0x01: return "radio initialisation failed";
    case 0x02: return "transmit failed";
    case 0x03: return "EEPROM locked (device not provisioned: run rnodeconf)";
    case 0x04: return "device queue full";
    case 0x05: return "device memory low";
    case 0x06: return "modem communication timed out";
    default: return "unknown device error";
  }
}

const char* platform_name(int p) {
  switch (p) {
    case 0x90: return "avr";
    case 0x80: return "esp32";
    case 0x70: return "nrf52";
    default: return p < 0 ? "" : "unknown";
  }
}

uint32_t be32(ByteView v) {
  return static_cast<uint32_t>(v[0]) << 24 | static_cast<uint32_t>(v[1]) << 16 | static_cast<uint32_t>(v[2]) << 8 | v[3];
}
uint16_t be16(ByteView v, size_t off = 0) { return static_cast<uint16_t>(v[off] << 8 | v[off + 1]); }

struct RadioParams {
  uint32_t freq = 0, bw = 125000;
  uint8_t sf = 9, cr = 5, txp = 10;
  uint16_t st_alock = 0, lt_alock = 0;  // percent * 100, 0 = not sent
  bool operator==(const RadioParams& o) const {
    return freq == o.freq && bw == o.bw && sf == o.sf && cr == o.cr && txp == o.txp && st_alock == o.st_alock &&
           lt_alock == o.lt_alock;
  }
};

class RNodeLink final : public LinkBase {
 public:
  explicit RNodeLink(LinkHost& h) : LinkBase(h), dec_(kHwMtu + 16, true) {}
  ~RNodeLink() override {
    if (connected_ && state_ == LinkState::Up) write_cmd(cmd::LEAVE, Bytes{0xFF});
  }

  bool configure(const Json& c, std::string* error) {
    const LoraRegion& reg = lora_region(c.get_string("region"));
    RadioParams p;
    int64_t f = c.get_int("frequency_hz");
    p.freq = static_cast<uint32_t>(f ? f : reg.freq_hz);
    if (f && f < 137000000) {
      if (error) *error = "frequency_hz: RNode supports 137 MHz .. 3 GHz (or 0 for the region default)";
      return false;
    }
    if (f && reg.min_hz && (static_cast<uint32_t>(f) < reg.min_hz || static_cast<uint32_t>(f) > reg.max_hz)) {
      if (error) *error = "frequency_hz: outside the selected region's band (choose region 'custom' to override)";
      return false;
    }
    int64_t bw = 0;
    parse_int(c.get_string("bandwidth_hz"), &bw);
    p.bw = static_cast<uint32_t>(bw);
    p.sf = static_cast<uint8_t>(c.get_int("sf", 9));
    p.cr = static_cast<uint8_t>(c.get_int("cr", 5));
    int64_t txp = c.get_int("tx_power_dbm", -1);
    p.txp = static_cast<uint8_t>(txp >= 0 ? txp : reg.power_dbm);
    int64_t duty = c.get_int("duty_cycle_permille");
    uint16_t dutyp = static_cast<uint16_t>(duty ? duty : reg.duty_permille);
    p.st_alock = static_cast<uint16_t>(std::lround(c.get_double("st_alock_pct") * 100));
    double lt = c.get_double("lt_alock_pct");
    p.lt_alock = static_cast<uint16_t>(lt > 0 ? std::lround(lt * 100) : (dutyp < 1000 ? dutyp * 10 : 0));
    LinkCaps k;
    k.mtu = static_cast<uint16_t>(c.get_int("mtu", 254));
    k.bitrate_bps = lora_bitrate(p.sf, p.bw, p.cr);
    k.broadcast = true;
    k.half_duplex = true;
    k.reliable = false;
    bool amateur = c.get_bool("amateur");
    k.encryption_allowed = !amateur;
    k.requires_station_id = amateur;
    k.duty_cycle_permille = dutyp;
    k.cost = static_cast<uint8_t>(c.get_int("cost", 20));
    bool radio_changed = !(p == want_);
    want_ = p;
    caps_ = k;
    region_ = reg.id;
    detect_timeout_ms_ = static_cast<uint32_t>(c.get_int("detect_timeout_ms", 2000));
    size_t qf = static_cast<size_t>(c.get_int("queue_frames", 32));
    q_.set_limits(qf, qf * k.mtu);
    if (connected_ && radio_changed && phase_ != Phase::Detect) start_config(now());
    return true;
  }

  const char* type() const override { return "rnode"; }
  LinkCaps caps() const override { return caps_; }

  bool send(ByteView frame, uint8_t priority) override {
    if (!enqueue(frame, priority, caps_.mtu)) return false;
    pump(now());
    return true;
  }
  bool channel_busy() const override { return inflight_; }

  void on_io_state(bool connected) override {
    connected_ = connected;
    dec_.reset();
    inflight_ = false;
    if (connected) {
      detected_ = false;
      start_detect(now());
    } else {
      phase_ = Phase::Idle;
      set_state(LinkState::Down);
    }
  }

  void on_io_bytes(ByteView d) override {
    st_.io_rx_bytes += d.size();
    uint64_t before = dec_.stats().oversize + dec_.stats().bad_escape;
    dec_.feed(d, [&](ByteView f) { handle(f); });
    st_.rx_errors += dec_.stats().oversize + dec_.stats().bad_escape - before;
  }

  void tick(uint64_t t) override {
    if (!connected_) return;
    if (deadline_ && t >= deadline_) on_timeout(t);
    if (inflight_ && t >= inflight_until_) inflight_ = false;  // firmware without READY flow control
    pump(t);
  }

  std::string status_json() const override {
    Json j = base_status();
    j.set("region", region_).set("phase", phase_name());
    Json dev = Json::object();
    dev.set("detected", detected_);
    if (fw_major_ >= 0) dev.set("firmware", std::to_string(fw_major_) + "." + (fw_minor_ < 10 ? "0" : "") + std::to_string(fw_minor_));
    if (platform_ >= 0) dev.set("platform", platform_name(platform_)).set("platform_id", platform_);
    if (mcu_ >= 0) dev.set("mcu_id", mcu_);
    if (board_ >= 0) dev.set("board_id", board_);
    j.set("device", std::move(dev));
    Json radio = Json::object();
    radio.set("frequency_hz", want_.freq).set("bandwidth_hz", want_.bw).set("sf", want_.sf).set("cr", want_.cr);
    radio.set("tx_power_dbm", want_.txp);
    if (r_.have_state) radio.set("radio_on", r_.state == 1);
    j.set("radio", std::move(radio));
    if (have_rssi_) j.set("last_rssi_dbm", last_rssi_);
    if (have_snr_) j.set("last_snr_db", last_snr_);
    if (have_chtm_) {
      j.set("airtime_short_pct", chtm_[0] / 100.0).set("airtime_long_pct", chtm_[1] / 100.0);
      j.set("channel_load_short_pct", chtm_[2] / 100.0).set("channel_load_long_pct", chtm_[3] / 100.0);
      j.set("current_rssi_dbm", chtm_rssi_).set("noise_floor_dbm", chtm_noise_);
    }
    if (bat_pct_ >= 0) j.set("battery_pct", bat_pct_).set("battery_state", bat_state_);
    if (temp_c_ > -100) j.set("temperature_c", temp_c_);
    if (dev_rx_ >= 0) j.set("device_rx_packets", dev_rx_);
    if (dev_tx_ >= 0) j.set("device_tx_packets", dev_tx_);
    j.set("device_errors", device_errors_).set("tx_failed", tx_failed_);
    j.set("caps", caps_json(caps_));
    return j.dump();
  }

  bool reconfigure(const std::string& cfg, std::string* error) override {
    Json c;
    if (!parse_config(rnode_schema(), cfg, &c, error)) return false;
    RNodeLink tmp(host_);
    if (!tmp.configure(c, error)) return false;
    return configure(c, error);
  }

 private:
  enum class Phase : uint8_t { Idle, Detect, Config, Running, Failed };
  struct Reported {
    bool have_freq = false, have_bw = false, have_txp = false, have_sf = false, have_cr = false, have_state = false;
    uint32_t freq = 0, bw = 0;
    uint8_t txp = 0, sf = 0, cr = 0, state = 0;
  };

  const char* phase_name() const {
    switch (phase_) {
      case Phase::Idle: return "idle";
      case Phase::Detect: return "detecting";
      case Phase::Config: return "configuring";
      case Phase::Running: return "running";
      case Phase::Failed: return "failed";
    }
    return "idle";
  }

  void write_cmd(uint8_t c, const Bytes& args) {
    Bytes body;
    body.push_back(c);
    body.insert(body.end(), args.begin(), args.end());
    Bytes out;
    out.push_back(slip::END);
    slip::escape_append(ByteView(body), out);
    out.push_back(slip::END);
    io_write(ByteView(out));
  }
  static Bytes u32(uint32_t v) {
    return Bytes{static_cast<uint8_t>(v >> 24), static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v)};
  }

  void start_detect(uint64_t t) {
    phase_ = Phase::Detect;
    set_state(LinkState::Connecting);
    tries_ = 0;
    send_detect(t);
  }
  void send_detect(uint64_t t) {
    ++tries_;
    // Same request burst as Reticulum: detect + firmware version + platform + MCU queries.
    static const uint8_t kDetect[] = {0xC0, cmd::DETECT, cmd::DETECT_REQ, 0xC0, cmd::FW_VERSION, 0x00, 0xC0,
                                      cmd::PLATFORM, 0x00, 0xC0, cmd::MCU, 0x00, 0xC0};
    io_write(ByteView(kDetect, sizeof kDetect));
    deadline_ = t + detect_timeout_ms_;
  }

  void start_config(uint64_t t) {
    phase_ = Phase::Config;
    set_state(LinkState::Connecting);
    inflight_ = false;
    r_ = Reported();
    write_cmd(cmd::FREQUENCY, u32(want_.freq));
    write_cmd(cmd::BANDWIDTH, u32(want_.bw));
    write_cmd(cmd::TXPOWER, Bytes{want_.txp});
    write_cmd(cmd::SF, Bytes{want_.sf});
    write_cmd(cmd::CR, Bytes{want_.cr});
    if (want_.st_alock) write_cmd(cmd::ST_ALOCK, Bytes{static_cast<uint8_t>(want_.st_alock >> 8), static_cast<uint8_t>(want_.st_alock)});
    if (want_.lt_alock) write_cmd(cmd::LT_ALOCK, Bytes{static_cast<uint8_t>(want_.lt_alock >> 8), static_cast<uint8_t>(want_.lt_alock)});
    write_cmd(cmd::RADIO_STATE, Bytes{0x01});
    deadline_ = t + kConfigTimeoutMs;
    ++config_attempts_;
  }

  void fail(const std::string& why, uint64_t t) {
    phase_ = Phase::Failed;
    set_state(LinkState::Error, why.c_str());
    inflight_ = false;
    deadline_ = t + kRetryIdleMs;  // recovery: start over later while the endpoint stays open
  }

  void on_timeout(uint64_t t) {
    deadline_ = 0;
    switch (phase_) {
      case Phase::Detect:
        if (tries_ < kDetectTries) send_detect(t);
        else fail("no RNode answered (check port, baud 115200 and that the board runs RNode firmware)", t);
        break;
      case Phase::Config:
        if (config_attempts_ < 2) start_config(t);
        else check_config(t, true);
        break;
      case Phase::Failed:
        detected_ = false;
        config_attempts_ = 0;
        start_detect(t);
        break;
      default: break;
    }
  }

  // Validates the echoed configuration; with `final`, missing echoes are an error.
  void check_config(uint64_t t, bool final) {
    if (!(r_.have_freq && r_.have_bw && r_.have_txp && r_.have_sf && r_.have_cr && r_.have_state)) {
      if (final) fail("RNode did not confirm the radio settings", t);
      return;
    }
    std::string bad;
    uint32_t df = r_.freq > want_.freq ? r_.freq - want_.freq : want_.freq - r_.freq;
    if (df > 100) bad = "frequency";
    else if (r_.bw != want_.bw) bad = "bandwidth";
    else if (r_.txp != want_.txp) bad = "TX power";
    else if (r_.sf != want_.sf) bad = "spreading factor";
    else if (r_.cr != want_.cr) bad = "coding rate";
    else if (r_.state != 1) bad = "radio state";
    if (!bad.empty()) {
      fail("RNode rejected the " + bad + " setting (unsupported by this hardware?)", t);
      return;
    }
    phase_ = Phase::Running;
    deadline_ = 0;
    config_attempts_ = 0;
    last_error_.clear();
    set_state(LinkState::Up);
    pump(t);
  }

  void handle(ByteView f) {
    if (f.empty()) return;
    uint8_t c = f[0];
    ByteView a = f.sub(1);
    uint64_t t = now();
    switch (c) {
      case cmd::DATA:
        if (a.empty() || a.size() > kHwMtu) { ++st_.rx_errors; return; }
        if (phase_ != Phase::Running) return;  // not configured yet: ignore stale traffic
        {
          RxMeta m;
          if (have_rssi_ && t <= rssi_ms_ + 1000) { m.has_rssi = true; m.rssi_dbm = static_cast<float>(last_rssi_); }
          if (have_snr_ && t <= snr_ms_ + 1000) { m.has_snr = true; m.snr_db = static_cast<float>(last_snr_); }
          deliver(a, m);
        }
        return;
      case cmd::DETECT:
        if (a.size() == 1 && a[0] == cmd::DETECT_RESP && phase_ == Phase::Detect) {
          detected_ = true;
          config_attempts_ = 0;
          start_config(t);
        }
        return;
      case cmd::FREQUENCY:
        if (a.size() == 4) { r_.freq = be32(a); r_.have_freq = true; }
        break;
      case cmd::BANDWIDTH:
        if (a.size() == 4) { r_.bw = be32(a); r_.have_bw = true; }
        break;
      case cmd::TXPOWER:
        if (a.size() == 1) { r_.txp = a[0]; r_.have_txp = true; }
        break;
      case cmd::SF:
        if (a.size() == 1) { r_.sf = a[0]; r_.have_sf = true; }
        break;
      case cmd::CR:
        if (a.size() == 1) { r_.cr = a[0]; r_.have_cr = true; }
        break;
      case cmd::RADIO_STATE:
        if (a.size() == 1) {
          r_.state = a[0];
          r_.have_state = true;
          if (phase_ == Phase::Running && a[0] != 1) start_config(t);  // radio went off underneath us
        }
        break;
      case cmd::READY:
        inflight_ = false;
        pump(t);
        return;
      case cmd::STAT_RSSI:
        if (a.size() == 1) { last_rssi_ = static_cast<int>(a[0]) - kRssiOffset; have_rssi_ = true; rssi_ms_ = t; }
        return;
      case cmd::STAT_SNR:
        if (a.size() == 1) { last_snr_ = static_cast<int8_t>(a[0]) * 0.25; have_snr_ = true; snr_ms_ = t; }
        return;
      case cmd::STAT_RX:
        if (a.size() == 4) dev_rx_ = be32(a);
        return;
      case cmd::STAT_TX:
        if (a.size() == 4) dev_tx_ = be32(a);
        return;
      case cmd::STAT_CHTM:
        if (a.size() >= 10) {
          for (int i = 0; i < 4; ++i) chtm_[i] = be16(a, static_cast<size_t>(2 * i));
          chtm_rssi_ = static_cast<int>(a[8]) - kRssiOffset;
          chtm_noise_ = static_cast<int>(a[9]) - kRssiOffset;
          have_chtm_ = true;
        }
        return;
      case cmd::STAT_BAT:
        if (a.size() == 2) { bat_state_ = a[0]; bat_pct_ = a[1] > 100 ? 100 : a[1]; }
        return;
      case cmd::STAT_TEMP:
        if (a.size() == 1) {
          int tc = static_cast<int>(a[0]) - 120;
          if (tc >= -30 && tc <= 90) temp_c_ = tc;
        }
        return;
      case cmd::FW_VERSION:
        if (a.size() == 2) { fw_major_ = a[0]; fw_minor_ = a[1]; }
        return;
      case cmd::PLATFORM:
        if (a.size() == 1) platform_ = a[0];
        return;
      case cmd::MCU:
        if (a.size() == 1) mcu_ = a[0];
        return;
      case cmd::BOARD:
        if (a.size() == 1) board_ = a[0];
        return;
      case cmd::RESET:
        if (a.size() == 1 && a[0] == 0xF8 && connected_) {  // device rebooted: handshake again
          set_error("RNode rebooted");
          detected_ = false;
          start_detect(t);
        }
        return;
      case cmd::ERROR:
        if (a.size() >= 1) {
          ++device_errors_;
          set_error(error_text(a[0]));
          if (a[0] == 0x02 || a[0] == 0x04) {  // TX failed / queue full: frame lost, carry on
            ++tx_failed_;
            inflight_ = false;
          } else if (a[0] == 0x01 || a[0] == 0x03) {
            fail(std::string("RNode error: ") + error_text(a[0]), t);
          }
        }
        return;
      default:
        return;  // other reports (PHYPRM, CSMA, RANDOM, LOG, display ...) are not needed
    }
    if (phase_ == Phase::Config) check_config(t, false);
  }

  void pump(uint64_t t) {
    if (phase_ != Phase::Running || state_ != LinkState::Up) return;
    while (!inflight_ && !q_.empty()) {
      const Bytes* head = q_.peek();
      size_t n = head->size();
      // Firmware adds a 1-byte header per LoRa packet and splits frames above 254 bytes in two.
      uint32_t air = n > 254 ? lora_airtime_ms(255, want_.sf, want_.bw, want_.cr) +
                                   lora_airtime_ms(n - 254 + 1, want_.sf, want_.bw, want_.cr)
                             : lora_airtime_ms(n + 1, want_.sf, want_.bw, want_.cr);
      if (caps_.duty_cycle_permille < 1000 && !duty_.allows(t, air, caps_.duty_cycle_permille)) return;
      Bytes f;
      q_.pop(&f);
      write_cmd(cmd::DATA, f);
      ++st_.tx_frames;
      st_.tx_bytes += f.size();
      st_.airtime_ms += air;
      duty_.add(t, air);
      inflight_ = true;
      inflight_until_ = t + air + 300;  // fallback when the firmware does not send READY
    }
  }

  LinkCaps caps_;
  RadioParams want_;
  Reported r_;
  std::string region_;
  slip::Decoder dec_;
  DutyCycle duty_;
  Phase phase_ = Phase::Idle;
  bool connected_ = false, detected_ = false, inflight_ = false;
  uint64_t deadline_ = 0, inflight_until_ = 0;
  uint8_t tries_ = 0, config_attempts_ = 0;
  uint32_t detect_timeout_ms_ = 2000;
  int fw_major_ = -1, fw_minor_ = -1, platform_ = -1, mcu_ = -1, board_ = -1;
  bool have_rssi_ = false, have_snr_ = false, have_chtm_ = false;
  int last_rssi_ = 0;
  double last_snr_ = 0;
  uint64_t rssi_ms_ = 0, snr_ms_ = 0;
  uint16_t chtm_[4] = {0, 0, 0, 0};
  int chtm_rssi_ = 0, chtm_noise_ = 0;
  int bat_pct_ = -1, bat_state_ = 0, temp_c_ = -1000;
  int64_t dev_rx_ = -1, dev_tx_ = -1;
  uint64_t device_errors_ = 0, tx_failed_ = 0;
};

std::unique_ptr<Link> create_rnode(const Json& c, LinkHost& h, std::string* error) {
  auto l = std::make_unique<RNodeLink>(h);
  if (!l->configure(c, error)) return nullptr;
  return l;
}

}  // namespace

const LinkTypeInfo& rnode_type() {
  static const LinkTypeInfo t = {
      "rnode",
      "RNode LoRa radio",
      "Uses a LoRa board running the open RNode firmware (LilyGO T-Beam/LoRa32, Heltec, RAK4631 ...) as a "
      "long-range radio. The app sets frequency, bandwidth, spreading factor and power on the board.",
      {"serial", "ble", "bt_spp", "tcp"},
      rnode_schema,
      create_rnode,
  };
  return t;
}

}  // namespace link
}  // namespace ember
