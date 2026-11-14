// Real-audio shared half-duplex channel for mesh tests: every node runs a MeshEngine on top of a
// real `audio` link driver (soft modem, CSMA with p-persistence, PTT sequencing); the samples every
// transmitting node pulls are summed onto one channel and fed to every node that is not itself
// transmitting (a radio is deaf while keyed). Overlapping transmissions therefore collide exactly
// as they would on air, and carrier sense has the modem's real DCD latency.
//
// Deterministic: fixed seeds for identities, engine entropy, link randomness and channel noise.
#pragma once

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "crypto/identity.h"
#include "ember/link.h"
#include "mesh/engine.h"
#include "mesh/sim.h"
#include "proto/packet.h"

namespace air {

using namespace ember;
using namespace ember::mesh;

struct AirNode;

// Names what a mesh frame is ("beacon", "sync", "announce", "text", "frag"...), for traces.
inline std::string frame_kind(ByteView f, const NodeId* self = nullptr) {
  proto::FrameHeader fh;
  ByteView body;
  if (!proto::parse_frame(f, fh, body)) return "?";
  if (fh.type == proto::FrameType::Fragment) return "frag";
  if (fh.type == proto::FrameType::Control) {
    if (body.empty()) return "ctl?";
    switch (body[0]) {
      case 1: return "beacon";
      case 2: return "sync";
      case 3: return "fragnack";
      default: return "ctl?";
    }
  }
  proto::PacketView pv;
  if (!proto::parse_packet(body, pv)) return "pkt?";
  switch (pv.h.kind) {
    case proto::Kind::Text: return "text";
    case proto::Kind::Ack: return "ack";
    case proto::Kind::Announce:  // own: "announce#<pkt_id>" (same packet re-sent = same name)
      return self && pv.h.origin == *self ? "announce#" + std::to_string(pv.h.pkt_id) : "announce(relay)";
    case proto::Kind::Whois: return "whois";
    default: return "pkt" + std::to_string(static_cast<int>(pv.h.kind));
  }
}

// Forwards everything to the real driver and records what the engine handed it.
class TraceLink : public Link {
 public:
  TraceLink(Link* l, const uint64_t* clock, NodeId self) : l_(l), clock_(clock), self_(self) {}
  struct Sent { uint64_t ms; std::string kind; size_t bytes; };
  std::vector<Sent> sent;
  const char* type() const override { return l_->type(); }
  LinkCaps caps() const override { return l_->caps(); }
  LinkState state() const override { return l_->state(); }
  bool send(ByteView frame, uint8_t prio) override {
    bool ok = l_->send(frame, prio);
    if (ok) sent.push_back({*clock_, frame_kind(frame, &self_), frame.size()});
    return ok;
  }
  size_t tx_queue_depth() const override { return l_->tx_queue_depth(); }
  bool channel_busy() const override { return l_->channel_busy(); }

 private:
  Link* l_;
  const uint64_t* clock_;
  NodeId self_;
};

class AirHost : public LinkHost {
 public:
  AirHost(AirNode* n, uint64_t seed) : node_(n), s_(seed ? seed : 1) {}
  void link_frame_received(Link& link, ByteView frame, const RxMeta& meta) override;
  void link_io_write(Link&, ByteView) override {}
  void link_interop_text(Link&, const InteropText&) override {}
  void link_event(Link&, const char*, const std::string&) override {}
  uint64_t now_ms() const override;
  void random_bytes(uint8_t* out, size_t n) override {
    for (size_t i = 0; i < n; ++i) {
      s_ ^= s_ << 13; s_ ^= s_ >> 7; s_ ^= s_ << 17;
      out[i] = static_cast<uint8_t>(s_);
    }
  }

 private:
  AirNode* node_;
  uint64_t s_;
};

struct AirNode {
  std::string name;
  Identity id;
  std::unique_ptr<MeshEngine> eng;
  sim::Recorder rec;
  std::unique_ptr<AirHost> host;
  std::unique_ptr<Link> link;
  std::unique_ptr<TraceLink> trace;
  const uint64_t* clock = nullptr;
  uint32_t link_id = 1;
  // channel accounting
  uint64_t tx_samples = 0;
  bool keyed = false;  // pulled samples this step
  bool busy = false;   // channel_busy() at the start of this step
  std::vector<std::pair<uint64_t, std::string>> events;  // key-up / key-down / busy transitions
};

inline void AirHost::link_frame_received(Link&, ByteView frame, const RxMeta& meta) {
  node_->events.push_back({*node_->clock, "rx " + frame_kind(frame)});
  node_->eng->on_frame(node_->link_id, frame, meta);
}
inline uint64_t AirHost::now_ms() const { return *node_->clock; }

class AirNet {
 public:
  // `link_cfg`: audio link config JSON (modem, licence...). Rate: channel sample rate.
  AirNet(uint64_t seed, std::string link_cfg, uint32_t rate = 8000)
      : seed_(seed), cfg_(std::move(link_cfg)), rate_(rate), noise_s_(seed * 7919 + 1) {}

  AirNode& add(const std::string& name, MeshConfig mc = MeshConfig()) {
    auto n = std::make_unique<AirNode>();
    n->name = name;
    std::string sd = "air-node-" + name + "-" + std::to_string(seed_) + "-padding-to-32-bytes";
    crypto::Csprng r{ByteView(std::string_view(sd))};
    n->id = Identity::generate(r);
    Bytes ent = r.bytes(32);
    n->eng = std::make_unique<MeshEngine>(n->id, ent, mc);
    n->eng->set_delegate(&n->rec);
    n->eng->set_profile(name, "", "");
    n->clock = &now_;
    n->host = std::make_unique<AirHost>(n.get(), seed_ * 131 + nodes.size() + 1);
    std::string err;
    n->link = create_link("audio", cfg_, *n->host, &err);
    nodes.push_back(std::move(n));
    AirNode& x = *nodes.back();
    if (!x.link) { error = err; return x; }
    x.link->on_io_state(true);
    x.trace = std::make_unique<TraceLink>(x.link.get(), &now_, x.eng->node_id());
    x.eng->attach_link(x.link_id, x.trace.get());
    return x;
  }

  void introduce_all() {
    for (auto& a : nodes)
      for (auto& b : nodes)
        if (a != b) a->eng->add_known_keys(b->id.public_keys(), b->name);
  }

  // One 10 ms step: timers, transmit audio, mix, receive.
  void step() {
    now_ += kStepMs;
    for (auto& n : nodes) {
      n->link->tick(now_);
      n->eng->tick(now_);
    }
    const size_t k = rate_ * kStepMs / 1000;
    std::vector<float> mix(k, 0.0f);
    std::vector<int16_t> buf(k);
    int keyed = 0;
    for (auto& n : nodes) {
      bool busy = n->link->channel_busy();
      if (busy != n->busy && trace_events) n->events.push_back({now_, busy ? "busy" : "idle"});
      n->busy = busy;
      size_t got = n->link->pull_audio(buf.data(), k, rate_);
      if ((got > 0) != n->keyed && trace_events) n->events.push_back({now_, got > 0 ? "KEY" : "unkey"});
      n->keyed = got > 0;
      if (got) {
        ++keyed;
        n->tx_samples += got;
        for (size_t i = 0; i < got; ++i) mix[i] += buf[i] / 32768.0f;
      }
    }
    // Optional interferer (a collision the CSMA could not prevent, e.g. a hidden station much
    // stronger than the wanted signal): the wanted signal is lost entirely at every receiver
    // (adding noise is not enough: the robust modes decode far below the noise floor).
    if (jam && jam(*this)) {
      ++jam_steps;
      for (size_t i = 0; i < k; ++i) mix[i] = 0.4f * noise();
    }
    if (keyed >= 2) ++overlap_steps;
    if (keyed >= 1) ++busy_steps;
    for (auto& n : nodes) {
      std::vector<int16_t> rx(k);
      for (size_t i = 0; i < k; ++i) {
        // Own transmission is not heard (half duplex: keyed radio is deaf). Others: the sum, plus
        // a receiver noise floor (about -46 dBFS).
        float v = n->keyed ? 0.0f : mix[i];
        float x = v + noise() * 0.005f;
        if (x > 0.999f) x = 0.999f;
        if (x < -0.999f) x = -0.999f;
        rx[i] = static_cast<int16_t>(x * 32767.0f);
      }
      n->link->on_audio(rx.data(), k, rate_);
    }
  }
  void run(uint64_t ms) {
    for (uint64_t t = 0; t < ms; t += kStepMs) step();
  }
  // Returns elapsed ms until pred() or UINT64_MAX after max_ms.
  uint64_t run_until(const std::function<bool()>& pred, uint64_t max_ms) {
    uint64_t start = now_;
    while (now_ - start < max_ms) {
      if (pred()) return now_ - start;
      step();
    }
    return pred() ? now_ - start : UINT64_MAX;
  }
  uint64_t now() const { return now_; }
  double airtime_s(const AirNode& n) const { return static_cast<double>(n.tx_samples) / rate_; }

  std::vector<std::unique_ptr<AirNode>> nodes;
  uint64_t overlap_steps = 0, busy_steps = 0;
  bool trace_events = false;
  std::function<bool(AirNet&)> jam;
  uint64_t jam_steps = 0;
  std::string error;
  static constexpr uint32_t kStepMs = 10;

 private:
  float noise() {  // uniform-sum approximation of a Gaussian, unit variance
    float s = 0;
    for (int i = 0; i < 4; ++i) {
      noise_s_ ^= noise_s_ << 13; noise_s_ ^= noise_s_ >> 7; noise_s_ ^= noise_s_ << 17;
      s += static_cast<float>((noise_s_ >> 40) & 0xFFFFFF) / 16777216.0f - 0.5f;
    }
    return s * 1.7320508f;
  }
  uint64_t seed_;
  std::string cfg_;
  uint32_t rate_;
  uint64_t now_ = 1767225600000ull;  // 2026-01-01
  uint64_t noise_s_;
};

}  // namespace air
