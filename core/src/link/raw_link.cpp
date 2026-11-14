// EmberMesh core / link — "raw": datagram endpoints, one datagram == one mesh frame, no header.
// Used for UDP multicast (239.83.77.69:47474), the hub WebSocket bus (/ws), WebRTC data channels,
// MultipeerConnectivity sessions and BroadcastChannel loopback. (Android Wi-Fi Direct is a TCP
// byte stream and uses the "stream" driver.)
#include <memory>

#include "link/drivers.h"

namespace ember {
namespace link {
namespace {

constexpr size_t kRawRxMax = 8192;  // accept frames from peers with larger MTUs than ours

struct RawPreset {
  const char* id;
  uint16_t mtu;
  uint32_t bitrate;
  bool broadcast, reliable;
  uint8_t cost;
};

const RawPreset kPresets[] = {
    {"udp_lan", 1400, 10000000, true, false, 1},
    {"websocket", 4096, 1000000, true, true, 1},
    {"webrtc", 4096, 1000000, false, true, 2},
    {"multipeer", 4096, 1000000, true, true, 2},
    {"loopback", 4096, 100000000, true, true, 1},
    {"custom", 1400, 1000000, false, false, 10},
};

const RawPreset& preset(const std::string& id) {
  for (const auto& p : kPresets)
    if (id == p.id) return p;
  return kPresets[0];
}

Schema raw_schema() {
  std::vector<FieldOption> auto_yes_no = {{"auto", "Preset default"}, {"yes", "Yes"}, {"no", "No"}};
  return {
      select("preset", "Connection kind", "udp_lan",
             {{"udp_lan", "Wi-Fi / LAN (UDP multicast)"},
              {"websocket", "Hub WebSocket (browser to hub)"},
              {"webrtc", "WebRTC data channel (paired by QR)"},
              {"multipeer", "Apple MultipeerConnectivity"},
              {"loopback", "Same-device loopback (testing)"},
              {"custom", "Custom (set the values below)"}},
             "Pick what the connection runs over; the other settings default to sensible values for it."),
      num("mtu", "Largest frame", 0, 0, 8192, "Biggest frame sent in one datagram. 0 = preset default.", "bytes").adv(),
      num("bitrate_bps", "Speed estimate", 0, 0, 1000000000, "Used for scheduling only. 0 = preset default.", "bit/s").adv(),
      select("broadcast", "Reaches everyone", "auto", auto_yes_no,
             "Whether one datagram reaches all peers (multicast, hub bus) or only one (WebRTC).").adv(),
      select("reliable", "Reliable delivery", "auto", auto_yes_no,
             "Whether the medium itself guarantees delivery and order (WebSocket, reliable WebRTC).").adv(),
      num("cost", "Routing cost", 0, 0, 255, "Lower is preferred when several links reach a node. 0 = preset default.").adv(),
      num("queue_frames", "Queue size", 64, 1, 1024, "Frames held while the connection is down before refusing more.", "frames").adv(),
  };
}

class RawLink final : public LinkBase {
 public:
  explicit RawLink(LinkHost& h) : LinkBase(h) {}

  bool configure(const Json& c, std::string* error) {
    const RawPreset& p = preset(c.get_string("preset"));
    LinkCaps k;
    int64_t mtu = c.get_int("mtu");
    if (mtu != 0 && mtu < 32) {
      if (error) *error = "mtu: must be 0 (preset) or at least 32";
      return false;
    }
    k.mtu = static_cast<uint16_t>(mtu ? mtu : p.mtu);
    int64_t br = c.get_int("bitrate_bps");
    k.bitrate_bps = static_cast<uint32_t>(br ? br : p.bitrate);
    std::string b = c.get_string("broadcast"), r = c.get_string("reliable");
    k.broadcast = b == "auto" ? p.broadcast : b == "yes";
    k.reliable = r == "auto" ? p.reliable : r == "yes";
    k.half_duplex = false;
    k.encryption_allowed = true;
    k.requires_station_id = false;
    k.duty_cycle_permille = 1000;
    int64_t cost = c.get_int("cost");
    k.cost = static_cast<uint8_t>(cost ? cost : p.cost);
    caps_ = k;
    preset_ = p.id;
    size_t qf = static_cast<size_t>(c.get_int("queue_frames", 64));
    q_.set_limits(qf, qf * k.mtu);
    return true;
  }

  const char* type() const override { return "raw"; }
  LinkCaps caps() const override { return caps_; }

  bool send(ByteView frame, uint8_t priority) override {
    if (!enqueue(frame, priority, caps_.mtu)) return false;
    flush();
    return true;
  }

  void on_io_state(bool connected) override {
    set_state(connected ? LinkState::Up : LinkState::Down);
    if (connected) flush();
  }

  void on_io_bytes(ByteView d) override {
    st_.io_rx_bytes += d.size();
    if (d.empty() || d.size() > kRawRxMax) {
      ++st_.rx_errors;
      return;
    }
    // A datagram arriving proves the endpoint is open even if the host forgot to tell us.
    if (state_ != LinkState::Up) set_state(LinkState::Up);
    deliver(d, RxMeta());
  }

  void tick(uint64_t) override { flush(); }

  std::string status_json() const override {
    Json j = base_status();
    j.set("preset", preset_).set("caps", caps_json(caps_));
    return j.dump();
  }

  bool reconfigure(const std::string& cfg, std::string* error) override {
    Json c;
    if (!parse_config(raw_schema(), cfg, &c, error)) return false;
    RawLink tmp(host_);
    if (!tmp.configure(c, error)) return false;
    return configure(c, error);
  }

 private:
  void flush() {
    if (state_ != LinkState::Up) return;
    Bytes f;
    while (q_.pop(&f)) {
      ++st_.tx_frames;
      st_.tx_bytes += f.size();
      io_write(ByteView(f));
    }
  }

  LinkCaps caps_;
  std::string preset_;
};

std::unique_ptr<Link> create_raw(const Json& c, LinkHost& h, std::string* error) {
  auto l = std::make_unique<RawLink>(h);
  if (!l->configure(c, error)) return nullptr;
  return l;
}

}  // namespace

const LinkTypeInfo& raw_type() {
  static const LinkTypeInfo t = {
      "raw",
      "Direct datagrams (Wi-Fi, hub, WebRTC)",
      "Carries one mesh frame per datagram with no extra framing. Use it for Wi-Fi/LAN multicast, the "
      "hub WebSocket, WebRTC data channels, MultipeerConnectivity and loopback.",
      {"udp", "websocket", "ws_server", "webrtc", "multipeer", "loopback"},
      raw_schema,
      create_raw,
  };
  return t;
}

}  // namespace link
}  // namespace ember
