// Deterministic multi-node mesh simulator for tests.
//
//   Sim sim(seed);
//   auto& a = sim.add_node("a"); auto& b = sim.add_node("b");
//   Medium& radio = sim.add_medium(caps);           // a broadcast domain (radio channel, LAN ...)
//   sim.join(a, radio); sim.join(b, radio);         // every member hears every other by default
//   radio.set_loss(0.3); radio.connect(x, y, false); // per-pair connectivity / asymmetry
//   sim.run(60000);                                 // advance virtual time (ms)
//
// Model: each SimLink transmits one frame at a time (airtime from LinkCaps bitrate + key-up),
// receivers get it after `delay_ms`. Loss is per receiver. With half-duplex collisions enabled, a
// receiver that is transmitting, or that hears two overlapping frames, loses them.
#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "crypto/identity.h"
#include "crypto/rng.h"
#include "ember/link.h"
#include "mesh/engine.h"

namespace sim {

using namespace ember;
using namespace ember::mesh;

struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed ? seed : 0x1234567) {}
  uint64_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
  double unit() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
  uint32_t below(uint32_t n) { return n ? static_cast<uint32_t>(next() % n) : 0; }
};

class Medium;
struct Node;

class SimLink : public Link {
 public:
  SimLink(Medium* m, Node* n, uint32_t id) : medium_(m), node_(n), id_(id) {}
  const char* type() const override { return "sim"; }
  LinkCaps caps() const override;
  LinkState state() const override { return up_ ? LinkState::Up : LinkState::Down; }
  bool send(ByteView frame, uint8_t priority) override;
  size_t tx_queue_depth() const override { return txq_.size() + (transmitting_ ? 1 : 0); }
  bool channel_busy() const override;

  void set_up(bool up) { up_ = up; if (!up) txq_.clear(); }
  bool state_up() const { return up_; }
  // True if any of our recent transmissions overlaps [start, end).
  bool overlaps(uint64_t start, uint64_t end) const {
    for (auto& iv : tx_intervals_)
      if (iv.first < end && start < iv.second) return true;
    return false;
  }
  bool last_tx_overlaps(uint64_t start, uint64_t end) const { return overlaps(start, end); }
  std::deque<std::pair<uint64_t, uint64_t>> tx_intervals_;
  uint32_t id() const { return id_; }
  Node* node() const { return node_; }
  Medium* medium() const { return medium_; }

  // simulator internals
  std::deque<Bytes> txq_;
  bool transmitting_ = false;
  uint64_t tx_end_ = 0;
  uint64_t tx_start_ = 0;
  Bytes tx_frame_;
  uint64_t sent_frames = 0;
  std::vector<Bytes> sent_log;  // every frame put on the air (for inspection)
  bool log_frames = false;

 private:
  Medium* medium_;
  Node* node_;
  uint32_t id_;
  bool up_ = true;
};

struct Delivery {
  uint64_t at_ms;
  SimLink* to;
  Bytes frame;
  uint64_t tx_start, tx_end;
  SimLink* from;
  bool corrupted = false;
};

class Medium {
 public:
  Medium(uint64_t seed, const LinkCaps& caps) : rng_(seed), caps_(caps) {}
  LinkCaps caps_value() const { return caps_; }
  void set_caps(const LinkCaps& c) { caps_ = c; }
  void set_loss(double p) { loss_ = p; }
  void set_delay(uint32_t ms) { delay_ms_ = ms; }
  void set_collisions(bool on) { collisions_ = on; }
  bool collisions_enabled() const { return collisions_; }
  // Connectivity between members (directional). Default: everyone hears everyone.
  void connect(const Node& a, const Node& b, bool on, bool both_ways = true);
  void set_pair_loss(const Node& from, const Node& to, double p);
  bool hears(const SimLink* from, const SimLink* to) const;

  std::vector<SimLink*> members;
  uint64_t frames_on_air = 0, frames_lost = 0, frames_collided = 0;

  void step(uint64_t now, std::vector<Delivery>& out);
  uint32_t airtime(size_t bytes) const;

 private:
  Rng rng_;
  LinkCaps caps_;
  double loss_ = 0;
  uint32_t delay_ms_ = 5;
  bool collisions_ = false;
  std::set<std::pair<const Node*, const Node*>> blocked_;
  std::map<std::pair<const Node*, const Node*>, double> pair_loss_;
  friend class SimLink;
};

struct Recorder : public MeshDelegate {
  struct Text { RxInfo rx; proto::TextBody body; };
  std::vector<Text> texts;
  std::vector<std::pair<RxInfo, proto::SosBody>> sos;
  std::vector<std::pair<RxInfo, proto::SosAckBody>> sos_acks;
  std::vector<std::pair<RxInfo, proto::CheckinBody>> checkins;
  std::vector<std::pair<RxInfo, proto::BulletinBody>> bulletins;
  std::vector<std::pair<RxInfo, proto::NetPlanBody>> netplans;
  std::vector<std::pair<RxInfo, Position>> positions;
  struct File { RxInfo rx; std::string name, mime; Bytes data; };
  std::vector<File> files;
  std::vector<std::pair<MsgId, MsgStatus>> statuses;
  std::vector<std::pair<NodeId, uint32_t>> pongs;
  std::vector<std::pair<NodeId, MsgId>> reads;
  std::map<MsgId, MsgStatus> last_status;
  uint32_t file_progress = 0;
  uint32_t peers_new = 0;
  std::vector<std::pair<uint64_t, TimeSource>> times;

  void on_text(const RxInfo& rx, const proto::TextBody& b) override { texts.push_back({rx, b}); }
  void on_sos(const RxInfo& rx, const proto::SosBody& b) override { sos.push_back({rx, b}); }
  void on_sos_ack(const RxInfo& rx, const proto::SosAckBody& b) override { sos_acks.push_back({rx, b}); }
  void on_checkin(const RxInfo& rx, const proto::CheckinBody& b) override { checkins.push_back({rx, b}); }
  void on_bulletin(const RxInfo& rx, const proto::BulletinBody& b) override { bulletins.push_back({rx, b}); }
  void on_netplan(const RxInfo& rx, const proto::NetPlanBody& b) override { netplans.push_back({rx, b}); }
  void on_position(const RxInfo& rx, const Position& p) override { positions.push_back({rx, p}); }
  void on_file(const RxInfo& rx, const std::string& n, const std::string& m, const Bytes& d) override {
    files.push_back({rx, n, m, d});
  }
  void on_file_progress(const MsgId&, const NodeId&, uint32_t, uint32_t) override { file_progress++; }
  void on_status(const MsgId& m, MsgStatus s) override { statuses.push_back({m, s}); last_status[m] = s; }
  void on_pong(const NodeId& from, uint32_t rtt, uint8_t) override { pongs.push_back({from, rtt}); }
  void on_read(const NodeId& r, const MsgId& m) override { reads.push_back({r, m}); }
  void on_peer(const PeerInfo&, bool is_new) override { if (is_new) peers_new++; }
  void on_time(uint64_t ms, TimeSource s) override { times.push_back({ms, s}); }

  bool got_text(const std::string& t) const {
    for (auto& x : texts) if (x.body.text == t) return true;
    return false;
  }
  size_t count_text(const std::string& t) const {
    size_t n = 0;
    for (auto& x : texts) if (x.body.text == t) n++;
    return n;
  }
  MsgStatus status_of(const MsgId& m) const {
    auto it = last_status.find(m);
    return it == last_status.end() ? MsgStatus::Unknown : it->second;
  }
};

struct Node {
  std::string name;
  Identity id;
  std::unique_ptr<MeshEngine> eng;
  Recorder rec;
  std::vector<std::unique_ptr<SimLink>> links;
  int64_t clock_skew_ms = 0;  // host clock error
  NodeId nid() const { return id.node_id(); }
  SimLink* link_on(const Medium* m) {
    for (auto& l : links) if (l->medium() == m) return l.get();
    return nullptr;
  }
};

class Sim {
 public:
  explicit Sim(uint64_t seed, uint64_t start_ms = 1767225600000ull /* 2026-01-01 */)
      : seed_(seed), now_(start_ms), start_(start_ms), rng_(seed) {}

  Node& add_node(const std::string& name, const MeshConfig& cfg = default_config());
  Medium& add_medium(const LinkCaps& caps, uint32_t delay_ms = 5);
  SimLink& join(Node& n, Medium& m);
  // Exchange contact cards (keys) between all nodes, as if everyone scanned everyone's QR.
  void introduce_all();
  void introduce(Node& a, Node& b);

  void step(uint32_t dt_ms = 10);
  void run(uint64_t ms, uint32_t dt_ms = 10);
  // Runs until pred() or max_ms elapsed; returns elapsed ms or UINT64_MAX on timeout.
  uint64_t run_until(const std::function<bool()>& pred, uint64_t max_ms, uint32_t dt_ms = 10);

  uint64_t now() const { return now_; }
  uint64_t elapsed() const { return now_ - start_; }
  std::vector<std::unique_ptr<Node>> nodes;
  std::vector<std::unique_ptr<Medium>> media;
  // Called for every frame delivered to a link, before the engine sees it; may mutate / drop.
  std::function<bool(SimLink& to, Bytes& frame)> rx_hook;
  // Observes every transmitted frame.
  std::function<void(SimLink& from, const Bytes& frame)> tx_hook;

  static MeshConfig default_config() {
    MeshConfig c;
    c.announce_interval_s = 600;
    return c;
  }

 private:
  uint64_t seed_;
  uint64_t now_;
  uint64_t start_;
  Rng rng_;
  uint32_t next_link_id_ = 1;
  std::vector<Delivery> pending_;
  friend class SimLink;
};

// --- caps presets --------------------------------------------------------------------------------
inline LinkCaps lan_caps() {
  LinkCaps c;
  c.mtu = 1400;
  c.bitrate_bps = 10000000;
  c.broadcast = true;
  c.half_duplex = false;
  c.reliable = false;
  c.cost = 1;
  return c;
}
inline LinkCaps lora_caps() {
  LinkCaps c;
  c.mtu = 200;
  c.bitrate_bps = 5000;
  c.broadcast = true;
  c.half_duplex = true;
  c.cost = 20;
  return c;
}
inline LinkCaps afsk_caps() {
  LinkCaps c;
  c.mtu = 255;
  c.bitrate_bps = 1200;
  c.broadcast = true;
  c.half_duplex = true;
  c.cost = 40;
  return c;
}

}  // namespace sim
