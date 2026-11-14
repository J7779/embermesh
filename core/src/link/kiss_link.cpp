// EmberMesh core / link — "kiss": any KISS TNC (hardware TNCs, Direwolf over TCP, Mobilinkd,
// Kenwood/Yaesu radios with a built-in TNC, NinoTNC, soundmodem, LoRa KISS firmwares).
//
// payload = "ax25" (default): mesh frames ride in AX.25 UI frames (link/ax25_carrier.h):
//   dst EMBER, src = callsign-SSID, optional digipeater path, control 0x03, PID 0xF0, info = frame.
//   Other UI frames are decoded as APRS (messages with acks, bulletins, positions, status).
// payload = "raw": the mesh frame is the KISS data payload as-is (for licence-free LoRa KISS
//   modems that do not need AX.25 addressing).
#include <memory>

#include "framing/kiss.h"
#include "link/ax25_carrier.h"
#include "link/drivers.h"

namespace ember {
namespace link {
namespace {

constexpr size_t kKissRxMax = 4096;

Schema kiss_schema() {
  return {
      text("callsign", "Your callsign", "N0CALL",
           "Your amateur radio callsign with optional -SSID (e.g. N0CALL-7). Sent with every packet as the law requires.",
           12),
      select("licence", "Band / licence", "amateur",
             {{"amateur", "Amateur radio (callsign, no encryption)"},
              {"licence_free", "Licence-free or emergency use (encryption allowed)"}},
             "Amateur bands forbid encryption and need your callsign on every packet. Choose licence-free only "
             "if the radio works on a band where that is legal."),
      select("payload", "Packet format", "ax25",
             {{"ax25", "AX.25 (standard packet radio, works with APRS gear)"},
              {"raw", "Raw (LoRa KISS modems; no AX.25 header)"}},
             "Keep AX.25 for normal packet radio TNCs. Raw saves 16 bytes per packet on licence-free LoRa KISS modems."),
      num("port", "TNC port", 0, 0, 15, "Radio port on multi-port TNCs (0 for almost all devices)."),
      num("bitrate_bps", "Radio data rate", 1200, 50, 1000000,
          "Speed of the TNC's modem: 1200 for VHF packet/APRS, 9600 for fast UHF packet, 300 for HF.", "bit/s"),
      text("path", "Digipeater path", "",
           "Comma-separated digipeaters for mesh packets (normally empty: the mesh relays by itself).", 80).adv(),
      boolean("aprs", "APRS messaging", true,
              "Show APRS messages, positions and status heard on the channel, and allow sending APRS messages."),
      text("aprs_path", "APRS path", "WIDE1-1,WIDE2-1", "Digipeater path for APRS messages you send.", 80).adv(),
      boolean("aprs_auto_ack", "Acknowledge APRS messages", true,
              "Automatically confirm APRS messages addressed to your callsign.").adv(),
      boolean("set_params", "Configure the TNC", true,
              "Send the timing settings below to the TNC when it connects. Turn off for TNCs that ignore or dislike them."),
      num("txdelay_ms", "TX delay", 300, 0, 2550,
          "Time the transmitter is keyed before data starts. Increase if the start of packets is lost.", "ms").adv(),
      num("persistence", "Persistence", 63, 0, 255,
          "Chance (out of 255) of transmitting at each free slot. 63 suits busy channels.").adv(),
      num("slottime_ms", "Slot time", 100, 0, 2550, "Wait between channel checks.", "ms").adv(),
      num("txtail_ms", "TX tail", 30, 0, 2550, "Time the transmitter stays keyed after data.", "ms").adv(),
      boolean("full_duplex", "Full duplex", false, "Only for full-duplex radios/satellite setups.").adv(),
      select("init", "Enter KISS mode", "none",
             {{"none", "Already in KISS mode (Direwolf, Mobilinkd, most)"},
              {"kiss_on", "Send KISS ON + RESTART (TNC-2, Kantronics, Kenwood)"},
              {"custom", "Send custom text (below)"}},
             "Some TNCs and radios start in a text command mode and must be switched to KISS first.").adv(),
      text("init_custom", "Custom init text", "",
           "Text sent before KISS starts; use \\r for Enter (e.g. TC 0\\r for some Kenwood radios).", 200).adv(),
      boolean("exit_kiss", "Leave KISS on close", false,
              "Send the 'exit KISS' command when this link is removed, returning the TNC to its command mode.").adv(),
      num("mtu", "Largest frame", 256, 32, 2048,
          "Largest mesh frame per packet. 256 is the AX.25 standard; some TNCs accept more.", "bytes").adv(),
      num("duty_cycle_permille", "Duty-cycle limit", 1000, 1, 1000,
          "Legal transmit time limit in thousandths (1000 = none).", "‰").adv(),
      num("cost", "Routing cost", 0, 0, 255, "Lower is preferred. 0 = automatic from the data rate.").adv(),
      num("queue_frames", "Queue size", 32, 1, 256, "Frames waiting to be sent before new ones are refused.", "frames").adv(),
  };
}

uint8_t ten_ms(int64_t ms) { return static_cast<uint8_t>((ms + 5) / 10 > 255 ? 255 : (ms + 5) / 10); }

class KissLink final : public LinkBase {
 public:
  explicit KissLink(LinkHost& h) : LinkBase(h), dec_(kKissRxMax, true) { ax_.set_pseudo_seed(random_u32()); }
  ~KissLink() override {
    if (exit_kiss_ && state_ == LinkState::Up) io_write(ByteView(kiss::encode_return()));
  }

  bool configure(const Json& c, std::string* error) {
    Ax25Config ac;
    if (!parse_callsign(c.get_string("callsign"), &ac.mycall, error)) return false;
    if (!parse_path(c.get_string("path"), &ac.path, error)) return false;
    if (!parse_path(c.get_string("aprs_path"), &ac.aprs_path, error)) return false;
    Bytes init;
    std::string im = c.get_string("init");
    if (im == "kiss_on") {
      std::string s = "\rKISS ON\rRESTART\r";
      init.assign(s.begin(), s.end());
    } else if (im == "custom" && !unescape_text(c.get_string("init_custom"), &init)) {
      if (error) *error = "init_custom: invalid escape (use \\r \\n \\t \\\\ \\xHH)";
      return false;
    }
    raw_ = c.get_string("payload") == "raw";
    ac.aprs = c.get_bool("aprs") && !raw_;
    ac.auto_ack = c.get_bool("aprs_auto_ack");
    ac.max_info = static_cast<size_t>(c.get_int("mtu"));
    amateur_ = c.get_string("licence") == "amateur";
    ac.amateur = amateur_;
    init_ = init;
    port_ = static_cast<uint8_t>(c.get_int("port"));
    set_params_ = c.get_bool("set_params");
    txdelay_ = c.get_int("txdelay_ms");
    persist_ = c.get_int("persistence");
    slot_ = c.get_int("slottime_ms");
    tail_ = c.get_int("txtail_ms");
    full_ = c.get_bool("full_duplex");
    exit_kiss_ = c.get_bool("exit_kiss");
    ax_.configure(ac);

    LinkCaps k;
    k.mtu = static_cast<uint16_t>(c.get_int("mtu"));
    k.bitrate_bps = static_cast<uint32_t>(c.get_int("bitrate_bps"));
    k.broadcast = true;
    k.half_duplex = !full_;
    k.reliable = false;
    k.encryption_allowed = !amateur_;
    k.requires_station_id = amateur_;
    k.duty_cycle_permille = static_cast<uint16_t>(c.get_int("duty_cycle_permille"));
    int64_t cost = c.get_int("cost");
    k.cost = static_cast<uint8_t>(cost ? cost : k.bitrate_bps >= 9600 ? 25 : k.bitrate_bps >= 1200 ? 40 : 80);
    caps_ = k;
    size_t qf = static_cast<size_t>(c.get_int("queue_frames"));
    q_.set_limits(qf, qf * (k.mtu + 80));
    return true;
  }

  const char* type() const override { return "kiss"; }
  LinkCaps caps() const override { return caps_; }

  bool send(ByteView frame, uint8_t priority) override {
    if (amateur_ && !raw_ && ax_.config().mycall.call == "N0CALL") {
      last_error_ = "set your callsign before transmitting on amateur bands";
      ++st_.tx_refused;
      return false;
    }
    if (!enqueue(frame, priority, caps_.mtu)) return false;
    pump(now());
    return true;
  }

  bool channel_busy() const override { return !pacer_.ready(host_.now_ms()); }

  void on_io_state(bool connected) override {
    dec_.reset();
    pacer_.reset();
    if (!connected) {
      set_state(LinkState::Down);
      return;
    }
    set_state(LinkState::Up);
    if (!init_.empty()) io_write(ByteView(init_));
    if (set_params_) send_params();
    pump(now());
  }

  void on_io_bytes(ByteView d) override {
    if (d.empty()) return;
    st_.io_rx_bytes += d.size();
    if (state_ != LinkState::Up) set_state(LinkState::Up);
    uint64_t before = dec_.stats().oversize + dec_.stats().bad_escape;
    Ax25Carrier::Output out;
    dec_.feed(d, [&](const kiss::Frame& f) {
      if (f.command != kiss::Command::Data || f.port != port_) return;  // other ports / echoes
      if (f.payload.empty()) { ++st_.rx_errors; return; }
      if (raw_) {
        deliver(f.payload, RxMeta());
        return;
      }
      Bytes mesh;
      InteropText t;
      switch (ax_.on_frame(f.payload, now(), &mesh, &t, &out)) {
        case Ax25Carrier::Rx::Mesh: deliver(ByteView(mesh), RxMeta()); break;
        case Ax25Carrier::Rx::Interop: interop(t); break;
        case Ax25Carrier::Rx::Bad: ++st_.rx_errors; break;
        default: break;
      }
    });
    st_.rx_errors += dec_.stats().oversize + dec_.stats().bad_escape - before;
    apply(out);
  }

  void tick(uint64_t t) override {
    Ax25Carrier::Output out;
    ax_.tick(t, &out);
    apply(out);
    pump(t);
  }

  bool send_interop_text(const InteropText& msg) override {
    if (raw_ || (amateur_ && ax_.config().mycall.call == "N0CALL")) return false;
    Ax25Carrier::Output out;
    if (!ax_.send_text(msg, now(), &out)) return false;
    st_.interop_tx++;
    apply(out);
    return true;
  }

  std::string status_json() const override {
    Json j = base_status();
    j.set("payload", raw_ ? "raw" : "ax25").set("port", port_).set("ax25", ax_.status());
    j.set("kiss_discarded_bytes", dec_.stats().discarded_bytes);
    j.set("caps", caps_json(caps_));
    return j.dump();
  }

  bool reconfigure(const std::string& cfg, std::string* error) override {
    Json c;
    if (!parse_config(kiss_schema(), cfg, &c, error)) return false;
    KissLink tmp(host_);
    if (!tmp.configure(c, error)) return false;
    configure(c, error);
    if (state_ == LinkState::Up && set_params_) send_params();
    return true;
  }

 private:
  void send_params() {
    Bytes b;
    auto add = [&](kiss::Command cmd, uint8_t v) {
      Bytes f = kiss::encode_param(cmd, v, port_);
      b.insert(b.end(), f.begin(), f.end());
    };
    add(kiss::Command::TxDelay, ten_ms(txdelay_));
    add(kiss::Command::Persistence, static_cast<uint8_t>(persist_));
    add(kiss::Command::SlotTime, ten_ms(slot_));
    add(kiss::Command::TxTail, ten_ms(tail_));
    add(kiss::Command::FullDuplex, full_ ? 1 : 0);
    io_write(ByteView(b));
  }

  // Frames produced by the AX.25/APRS layer (acks, messages, retries) bypass the mesh queue limits
  // but are still paced; they are rare and bounded by the carrier.
  void apply(Ax25Carrier::Output& out) {
    for (auto& f : out.frames) aprs_out_.push_back(std::move(f));
    while (aprs_out_.size() > 16) aprs_out_.pop_front();
    for (auto& e : out.events) event(e.first.c_str(), e.second);
    pump(now());
  }

  void write_frame(ByteView ax25_or_raw, size_t mesh_bytes) {
    Bytes k;
    kiss::encode_append(ax25_or_raw, k, port_, kiss::Command::Data);
    io_write(ByteView(k));
    uint64_t air = airtime_ms(ax25_or_raw.size() + 4, caps_.bitrate_bps) +
                   static_cast<uint64_t>(full_ ? 0 : txdelay_ + tail_);
    st_.airtime_ms += air;
    if (caps_.duty_cycle_permille < 1000) duty_.add(now(), static_cast<uint32_t>(air));
    pacer_.spend(now(), 0, 0, static_cast<uint32_t>(air));
    (void)mesh_bytes;
  }

  void pump(uint64_t t) {
    if (state_ != LinkState::Up) return;
    while (pacer_.ready(t)) {
      uint32_t est = static_cast<uint32_t>(airtime_ms(caps_.mtu + 20, caps_.bitrate_bps) + txdelay_);
      if (caps_.duty_cycle_permille < 1000 && !duty_.allows(t, est, caps_.duty_cycle_permille)) return;
      if (!aprs_out_.empty()) {
        Bytes f = std::move(aprs_out_.front());
        aprs_out_.pop_front();
        write_frame(ByteView(f), 0);
        continue;
      }
      Bytes f;
      if (!q_.pop(&f)) return;
      ++st_.tx_frames;
      st_.tx_bytes += f.size();
      if (raw_) {
        write_frame(ByteView(f), f.size());
      } else {
        Bytes ax;
        if (!ax_.wrap(ByteView(f), &ax)) continue;
        write_frame(ByteView(ax), f.size());
      }
    }
  }

  LinkCaps caps_;
  Ax25Carrier ax_;
  kiss::Decoder dec_;
  std::deque<Bytes> aprs_out_;
  Bytes init_;
  bool raw_ = false, amateur_ = true, set_params_ = true, full_ = false, exit_kiss_ = false;
  uint8_t port_ = 0;
  int64_t txdelay_ = 300, persist_ = 63, slot_ = 100, tail_ = 30;
  Pacer pacer_;
  DutyCycle duty_;
};

std::unique_ptr<Link> create_kiss(const Json& c, LinkHost& h, std::string* error) {
  auto l = std::make_unique<KissLink>(h);
  if (!l->configure(c, error)) return nullptr;
  return l;
}

}  // namespace

const LinkTypeInfo& kiss_type() {
  static const LinkTypeInfo t = {
      "kiss",
      "KISS TNC (packet radio)",
      "Uses a packet-radio TNC through the KISS protocol: hardware TNCs, Direwolf, Mobilinkd, radios "
      "with a built-in TNC, NinoTNC, LoRa KISS modems. Mesh packets travel as AX.25 with your callsign, "
      "and APRS messages are bridged into chat.",
      {"serial", "tcp", "ble", "bt_spp"},
      kiss_schema,
      create_kiss,
  };
  return t;
}

}  // namespace link
}  // namespace ember
