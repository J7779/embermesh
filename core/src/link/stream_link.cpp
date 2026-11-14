// EmberMesh core / link — "stream": byte-stream endpoints (TCP, serial cables, BLE UART, BLE peer
// service, Bluetooth SPP, transparent serial radio modems such as Ebyte E22/E32, HC-12, SiK, XBee).
//
// On-air / on-wire formats (the receiver accepts plain always and fec only when fec is enabled;
// the sender uses the configured one). Frames longer than the link mtu are not accepted.
//   plain: framing/stream.h frame, type 0, payload = mesh frame (10 bytes overhead, CRC-32).
//   fec:   4-byte sync word 0x7E 0x1B 0xA5 0xD2 followed by a fec/packet.h envelope of the mesh
//          frame (RS-protected header + RS blocks + CRC-32). The sync is matched with up to 3 bit
//          errors, so a noisy radio link still finds frame starts; RS repairs byte errors inside.
#include <memory>

#include "fec/packet.h"
#include "framing/stream.h"
#include "link/drivers.h"

namespace ember {
namespace link {
namespace {

constexpr size_t kRxMax = 8192;
const uint8_t kFecSync[4] = {0x7E, 0x1B, 0xA5, 0xD2};
constexpr int kSyncMaxBitErrors = 3;

struct StreamPreset {
  const char* id;
  uint16_t mtu;
  uint32_t bitrate;   // nominal throughput
  uint32_t pace_bps;  // 0 = write immediately
  uint32_t gap_ms;    // pause after each paced frame / after received bytes
  bool fec, half_duplex, broadcast, reliable;
  uint8_t cost;
};

const StreamPreset kPresets[] = {
    {"cable", 2048, 115200, 0, 0, false, false, false, false, 2},
    {"tcp", 4096, 1000000, 0, 0, false, false, false, true, 1},
    {"nearby", 4096, 1000000, 0, 0, false, false, false, true, 1},  // automatic LAN links: hello first
    {"ble", 512, 20000, 0, 0, false, false, false, false, 3},
    {"bt_spp", 2048, 200000, 0, 0, false, false, false, true, 3},
    {"ebyte", 200, 2400, 2400, 150, true, true, true, false, 25},
    {"hc12", 200, 4800, 4800, 80, true, true, true, false, 20},
    {"sik", 240, 32000, 32000, 20, false, false, false, false, 10},
    {"xbee", 200, 9600, 9600, 20, false, true, true, false, 8},
    {"custom", 256, 9600, 0, 0, false, false, false, false, 10},
};

const StreamPreset& preset(const std::string& id) {
  for (const auto& p : kPresets)
    if (id == p.id) return p;
  return kPresets[0];
}

Schema stream_schema() {
  std::vector<FieldOption> ayn = {{"auto", "Preset default"}, {"yes", "Yes"}, {"no", "No"}};
  return {
      select("preset", "Device / cable", "cable",
             {{"cable", "USB or serial cable between two devices"},
              {"tcp", "TCP connection (also Wi-Fi Direct)"},
              {"nearby", "Nearby device (automatic; starts with the EmberMesh hello)"},
              {"ble", "Bluetooth LE (UART adapter or phone-to-phone)"},
              {"bt_spp", "Classic Bluetooth serial (SPP)"},
              {"ebyte", "Ebyte E22 / E32 LoRa module (transparent mode)"},
              {"hc12", "HC-12 433 MHz serial radio"},
              {"sik", "SiK / 3DR telemetry radio"},
              {"xbee", "XBee in transparent mode"},
              {"custom", "Custom (set the values below)"}},
             "What the byte stream runs over. Both ends should use the same choice."),
      num("mtu", "Largest frame", 0, 0, 4096, "Biggest mesh frame per packet. 0 = preset default.", "bytes").adv(),
      num("bitrate_bps", "Speed estimate", 0, 0, 100000000,
          "Real throughput, used for scheduling. 0 = preset default.", "bit/s").adv(),
      num("pace_bps", "Write pacing", 0, 0, 10000000,
          "Slow writes to this rate so cheap radio modems are not overrun (set it to the module's air "
          "data rate). 0 = preset default; the cable/TCP/Bluetooth presets do not pace.", "bit/s").adv(),
      num("gap_ms", "Gap between packets", -1, -1, 10000,
          "Pause after each packet and after receiving, so a half-duplex radio can switch over. -1 = preset default.", "ms").adv(),
      select("fec", "Error correction", "auto", ayn,
             "Add Reed-Solomon error correction (recommended for radio modules, useless on cables).").adv(),
      num("fec_roots", "Correction strength", 16, 2, 64,
          "Parity bytes per block: corrects half as many byte errors. Higher = more robust, more overhead.").adv(),
      select("half_duplex", "Half duplex", "auto", ayn, "The medium cannot send and receive at the same time.").adv(),
      select("broadcast", "Reaches everyone", "auto", ayn, "One transmission reaches every listener (radio modules).").adv(),
      num("write_chunk", "Write size limit", 0, 0, 4096,
          "Split writes into pieces of at most this many bytes (for endpoints with small buffers). 0 = no limit.", "bytes").adv(),
      boolean("amateur", "Amateur radio band", false,
              "Turn on if the radio transmits on an amateur band (e.g. 433 MHz in the Americas): traffic is then "
              "sent unencrypted and must carry your callsign."),
      num("duty_cycle_permille", "Duty-cycle limit", 1000, 1, 1000,
          "Legal transmit time limit in thousandths (1000 = none, 100 = 10 %, 10 = 1 %).", "‰").adv(),
      num("cost", "Routing cost", 0, 0, 255, "Lower is preferred. 0 = preset default.").adv(),
      num("queue_frames", "Queue size", 32, 1, 512, "Frames waiting to be sent before new ones are refused.", "frames").adv(),
  };
}

int popcount8(uint8_t v) {
  v = static_cast<uint8_t>(v - ((v >> 1) & 0x55));
  v = static_cast<uint8_t>((v & 0x33) + ((v >> 2) & 0x33));
  return (v + (v >> 4)) & 0x0F;
}

// Resynchronising receiver for the FEC format. Only instantiated work when FEC is enabled.
//
// Security (review H5): the buffer is consumed through a head index (no erase(begin) per byte);
// an announced envelope that could never fit the buffer is rejected at once instead of pinning
// the head; and RS work is paid from a credit earned per input byte (kWorkPerByte units, banked up
// to a cap). A candidate that cannot be afforded is skipped unchecked, so hostile input (a valid
// sync + header at every offset) costs linear CPU. Honest traffic needs well under the budget.
class FecReceiver {
 public:
  static constexpr size_t kWorkPerByte = 64;
  static constexpr size_t kHeaderCost = 64;

  void set_max_payload(size_t max_payload) {
    max_ = max_payload;
    // Largest envelope we accept: nroots 128 roughly doubles the body.
    limit_ = 4 + fec::kPacketHeaderSize + 2 * (max_ + 4) + 2 * fec::kMaxRoots;
    credit_cap_ = 4 * kWorkPerByte * limit_;
    reset();
  }

  template <class F>
  void feed(ByteView in, F&& on_frame, uint64_t* errors) {
    while (!in.empty()) {
      size_t take = in.size() < limit_ ? in.size() : limit_;
      if (head_ > 0 && head_ >= buf_.size() / 2) {
        buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(head_));
        head_ = 0;
      }
      buf_.insert(buf_.end(), in.data(), in.data() + take);
      in = in.sub(take);
      credit_ += take * kWorkPerByte;
      if (credit_ > credit_cap_) credit_ = credit_cap_;
      process(on_frame, errors);
      // A candidate waits for at most limit_ bytes, so this only trims pure garbage.
      if (buf_.size() - head_ > limit_) head_ = buf_.size() - limit_;
    }
  }
  void reset() {
    buf_.clear();
    head_ = 0;
    credit_ = credit_cap_;
  }
  uint64_t budget_skipped() const { return budget_skipped_; }

 private:
  template <class F>
  void process(F&& on_frame, uint64_t* errors) {
    for (;;) {
      size_t avail = buf_.size() - head_;
      if (avail < 4) return;
      const uint8_t* p = buf_.data() + head_;
      int e = 0;
      for (int i = 0; i < 4; ++i) e += popcount8(static_cast<uint8_t>(p[i] ^ kFecSync[i]));
      if (e > kSyncMaxBitErrors) { ++head_; continue; }
      if (avail < 4 + fec::kPacketHeaderSize) return;
      if (credit_ < kHeaderCost) { ++budget_skipped_; ++head_; continue; }
      credit_ -= kHeaderCost;
      size_t total = 0;
      uint8_t nroots = 0;
      if (!fec::peek_header(ByteView(p + 4, fec::kPacketHeaderSize), &total, max_, &nroots) ||
          4 + total > limit_) {
        ++head_;
        continue;
      }
      if (avail < 4 + total) return;
      size_t cost = total * (4 + nroots / 2u);
      if (credit_ < cost) { ++budget_skipped_; ++head_; continue; }
      credit_ -= cost;
      Bytes payload;
      fec::RecoverInfo info;
      if (fec::recover(ByteView(p + 4, total), &payload, &info, max_) && !payload.empty()) {
        size_t corrected = info.corrected + info.header_corrected + static_cast<size_t>(e);
        head_ += 4 + total;
        on_frame(ByteView(payload), corrected);
      } else {
        ++*errors;
        ++head_;
      }
    }
  }
  Bytes buf_;
  size_t head_ = 0;
  size_t max_ = kRxMax, limit_ = 0;
  size_t credit_ = 0, credit_cap_ = 0;
  uint64_t budget_skipped_ = 0;
};

class StreamLink final : public LinkBase {
 public:
  explicit StreamLink(LinkHost& h) : LinkBase(h), dec_(kRxMax) { fecrx_.set_max_payload(kRxMax); }

  bool configure(const Json& c, std::string* error) {
    const StreamPreset& p = preset(c.get_string("preset"));
    int64_t mtu = c.get_int("mtu");
    if (mtu != 0 && mtu < 32) {
      if (error) *error = "mtu: must be 0 (preset) or at least 32";
      return false;
    }
    auto pick = [](const std::string& v, bool d) { return v == "auto" ? d : v == "yes"; };
    LinkCaps k;
    k.mtu = static_cast<uint16_t>(mtu ? mtu : p.mtu);
    int64_t br = c.get_int("bitrate_bps");
    k.bitrate_bps = static_cast<uint32_t>(br ? br : p.bitrate);
    k.half_duplex = pick(c.get_string("half_duplex"), p.half_duplex);
    k.broadcast = pick(c.get_string("broadcast"), p.broadcast);
    k.reliable = p.reliable;
    bool amateur = c.get_bool("amateur");
    k.encryption_allowed = !amateur;
    k.requires_station_id = amateur;
    k.duty_cycle_permille = static_cast<uint16_t>(c.get_int("duty_cycle_permille", 1000));
    int64_t cost = c.get_int("cost");
    k.cost = static_cast<uint8_t>(cost ? cost : p.cost);
    int64_t pace = c.get_int("pace_bps");
    pace_bps_ = static_cast<uint32_t>(pace ? pace : p.pace_bps);
    int64_t gap = c.get_int("gap_ms", -1);
    gap_ms_ = static_cast<uint32_t>(gap >= 0 ? gap : p.gap_ms);
    fec_ = pick(c.get_string("fec"), p.fec);
    roots_ = static_cast<uint8_t>(c.get_int("fec_roots", 16));
    write_chunk_ = static_cast<size_t>(c.get_int("write_chunk"));
    caps_ = k;
    preset_ = p.id;
    hello_ = preset_ == "nearby";
    // Receive limit = our mtu (both ends must use the same preset/mtu); bounds per-candidate work.
    if (rx_max_ != k.mtu) {
      rx_max_ = k.mtu;
      dec_ = stream::Decoder(rx_max_);
      fecrx_.set_max_payload(rx_max_);
    }
    size_t qf = static_cast<size_t>(c.get_int("queue_frames", 32));
    q_.set_limits(qf, qf * k.mtu);
    return true;
  }

  const char* type() const override { return "stream"; }
  LinkCaps caps() const override { return caps_; }

  bool send(ByteView frame, uint8_t priority) override {
    if (!enqueue(frame, priority, caps_.mtu)) return false;
    pump(now());
    return true;
  }

  bool channel_busy() const override {
    uint64_t t = host_.now_ms();
    return !pacer_.ready(t) || (caps_.half_duplex && last_rx_ms_ && t < last_rx_ms_ + gap_ms_);
  }

  void on_io_state(bool connected) override {
    hello_got_ = 0;
    hello_failed_ = false;
    if (connected && hello_) {
      // CONNECTIVITY §3.3: "EMNB", version 1, three reserved zero bytes, before anything else.
      io_write(ByteView(kHello, sizeof kHello));
      hello_deadline_ = now() + 2000;
    }
    dec_.reset();
    fecrx_.reset();
    pacer_.reset();
    set_state(connected ? LinkState::Up : LinkState::Down);
    if (connected) pump(now());
  }

  void on_io_bytes(ByteView d) override {
    if (d.empty()) return;
    st_.io_rx_bytes += d.size();
    if (hello_ && hello_failed_) return;
    if (hello_ && hello_got_ < sizeof kHello) {
      size_t n = 0;
      while (n < d.size() && hello_got_ < sizeof kHello) {
        if (d[n] != kHello[hello_got_]) { fail_hello("the other side is not an EmberMesh nearby device"); return; }
        ++hello_got_;
        ++n;
      }
      d = d.sub(n);
      if (d.empty()) return;
    }
    last_rx_ms_ = now();
    if (state_ != LinkState::Up) set_state(LinkState::Up);
    uint64_t before = dec_.stats().bad_crc + dec_.stats().bad_header + dec_.stats().oversize;
    dec_.feed(d, [&](uint8_t type, ByteView payload) {
      if (type != 0 || payload.empty()) { ++st_.rx_errors; return; }
      deliver(payload, RxMeta());
    });
    st_.rx_errors += dec_.stats().bad_crc + dec_.stats().bad_header + dec_.stats().oversize - before;
    if (!fec_) return;  // plain-only link: never pay for FEC candidate decoding
    fecrx_.feed(d, [&](ByteView payload, size_t corrected) {
      RxMeta m;
      m.corrected_errors = static_cast<uint8_t>(corrected > 255 ? 255 : corrected);
      fec_corrected_ += corrected;
      deliver(payload, m);
    }, &st_.rx_errors);
  }

  void tick(uint64_t now_ms) override {
    if (hello_ && !hello_failed_ && hello_got_ < sizeof kHello && state_ == LinkState::Up && now_ms >= hello_deadline_)
      fail_hello("no EmberMesh hello within 2 s");
    pump(now_ms);
  }

  std::string status_json() const override {
    Json j = base_status();
    j.set("preset", preset_).set("fec", fec_).set("fec_roots", roots_).set("pace_bps", pace_bps_);
    j.set("gap_ms", gap_ms_).set("fec_corrected", fec_corrected_);
    j.set("framing_discarded_bytes", dec_.stats().discarded_bytes);
    if (hello_) j.set("hello", hello_failed_ ? "failed" : hello_got_ >= sizeof kHello ? "ok" : "pending");
    j.set("rx_budget_skipped", dec_.stats().budget_skipped + fecrx_.budget_skipped());
    j.set("caps", caps_json(caps_));
    return j.dump();
  }

  bool reconfigure(const std::string& cfg, std::string* error) override {
    Json c;
    if (!parse_config(stream_schema(), cfg, &c, error)) return false;
    StreamLink tmp(host_);
    if (!tmp.configure(c, error)) return false;
    return configure(c, error);
  }

 private:
  static constexpr uint8_t kHello[8] = {0x45, 0x4D, 0x4E, 0x42, 0x01, 0x00, 0x00, 0x00};

  void fail_hello(const char* why) {
    hello_failed_ = true;
    dec_.reset();
    q_.clear();
    set_state(LinkState::Error, why);
  }

  void pump(uint64_t t) {
    if (state_ != LinkState::Up || (hello_ && (hello_failed_ || hello_got_ < sizeof kHello))) return;
    while (!q_.empty()) {
      if (pace_bps_ && !pacer_.ready(t)) return;
      if (caps_.half_duplex && gap_ms_ && last_rx_ms_ && t < last_rx_ms_ + gap_ms_) return;
      const Bytes* head = q_.peek();
      size_t wire = fec_ ? 4 + fec::protected_size(head->size(), params()) : head->size() + stream::kOverhead;
      uint32_t air = static_cast<uint32_t>(airtime_ms(wire, pace_bps_ ? pace_bps_ : caps_.bitrate_bps));
      if (caps_.duty_cycle_permille < 1000 && !duty_.allows(t, air, caps_.duty_cycle_permille)) return;
      Bytes f;
      q_.pop(&f);
      Bytes out;
      if (fec_) {
        out.assign(kFecSync, kFecSync + 4);
        Bytes env = fec::protect(ByteView(f), params());
        out.insert(out.end(), env.begin(), env.end());
      } else {
        stream::encode_append(ByteView(f), out, 0);
      }
      write_chunked(ByteView(out));
      ++st_.tx_frames;
      st_.tx_bytes += f.size();
      st_.airtime_ms += air;
      if (caps_.duty_cycle_permille < 1000) duty_.add(t, air);
      if (pace_bps_) pacer_.spend(t, out.size(), pace_bps_, gap_ms_);
    }
  }

  fec::ProtectParams params() const {
    fec::ProtectParams p;
    p.nroots = roots_;
    p.interleave = true;
    return p;
  }

  void write_chunked(ByteView b) {
    if (write_chunk_ == 0) { io_write(b); return; }
    for (size_t off = 0; off < b.size(); off += write_chunk_) io_write(b.sub(off, write_chunk_));
  }

  LinkCaps caps_;
  std::string preset_;
  uint32_t pace_bps_ = 0, gap_ms_ = 0;
  bool fec_ = false;
  uint8_t roots_ = 16;
  size_t write_chunk_ = 0;
  size_t rx_max_ = kRxMax;
  bool hello_ = false, hello_failed_ = false;
  size_t hello_got_ = 0;
  uint64_t hello_deadline_ = 0;
  stream::Decoder dec_;
  FecReceiver fecrx_;
  Pacer pacer_;
  DutyCycle duty_;
  uint64_t last_rx_ms_ = 0;
  uint64_t fec_corrected_ = 0;
};

std::unique_ptr<Link> create_stream(const Json& c, LinkHost& h, std::string* error) {
  auto l = std::make_unique<StreamLink>(h);
  if (!l->configure(c, error)) return nullptr;
  return l;
}

}  // namespace

const LinkTypeInfo& stream_type() {
  static const LinkTypeInfo t = {
      "stream",
      "Serial / TCP / Bluetooth byte stream",
      "Frames mesh traffic over any byte stream: USB or serial cables, TCP, Bluetooth LE and classic "
      "Bluetooth, and transparent serial radio modules (Ebyte, HC-12, SiK, XBee) with optional error "
      "correction and pacing.",
      {"serial", "tcp", "tcp_server", "ble", "ble_peer", "bt_spp", "wifi_direct"},
      stream_schema,
      create_stream,
  };
  return t;
}

}  // namespace link
}  // namespace ember
