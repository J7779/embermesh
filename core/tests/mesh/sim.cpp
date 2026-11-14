#include "mesh/sim.h"

#include <algorithm>

namespace sim {

LinkCaps SimLink::caps() const { return medium_->caps_; }

bool SimLink::send(ByteView frame, uint8_t) {
  if (!up_ || frame.size() > medium_->caps_.mtu || txq_.size() >= 8) return false;
  txq_.push_back(frame.to_bytes());
  return true;
}

bool SimLink::channel_busy() const {
  if (transmitting_) return true;
  if (!medium_->collisions_) return false;
  for (SimLink* o : medium_->members)
    if (o != this && o->transmitting_ && medium_->hears(o, this)) return true;
  return false;
}

void Medium::connect(const Node& a, const Node& b, bool on, bool both_ways) {
  auto set = [&](const Node* x, const Node* y) {
    if (on) blocked_.erase({x, y});
    else blocked_.insert({x, y});
  };
  set(&a, &b);
  if (both_ways) set(&b, &a);
}

void Medium::set_pair_loss(const Node& from, const Node& to, double p) { pair_loss_[{&from, &to}] = p; }

bool Medium::hears(const SimLink* from, const SimLink* to) const {
  if (from == to) return false;
  return !blocked_.count({from->node(), to->node()});
}

uint32_t Medium::airtime(size_t bytes) const {
  uint32_t bps = caps_.bitrate_bps ? caps_.bitrate_bps : 1;
  uint64_t ms = (uint64_t(bytes) * 8000ull + bps - 1) / bps;
  if (caps_.half_duplex) ms += bps <= 2400 ? 250 : (bps <= 20000 ? 50 : 2);
  return static_cast<uint32_t>(ms);
}

void Medium::step(uint64_t now, std::vector<Delivery>& out) {
  for (SimLink* l : members) {
    if (l->transmitting_ && now >= l->tx_end_) {
      // Transmission complete: deliver to every listener.
      l->transmitting_ = false;
      frames_on_air++;
      for (SimLink* r : members) {
        if (r == l || !r->state_up() || !hears(l, r)) continue;
        double p = loss_;
        auto pl = pair_loss_.find({l->node(), r->node()});
        if (pl != pair_loss_.end()) p = pl->second;
        if (p > 0 && rng_.unit() < p) { frames_lost++; continue; }
        Delivery d;
        d.at_ms = now + delay_ms_;
        d.to = r;
        d.from = l;
        d.frame = l->tx_frame_;
        d.tx_start = l->tx_start_;
        d.tx_end = l->tx_end_;
        out.push_back(std::move(d));
      }
    }
    // p-persistent CSMA like a KISS TNC / LoRa modem: wait for a clear channel, then key up
    // with probability 1/4 per 10 ms slot.
    bool can_start = !l->transmitting_ && !l->txq_.empty() && l->state_up();
    if (can_start && collisions_) {
      for (SimLink* o : members)
        if (o != l && o->transmitting_ && hears(o, l)) { can_start = false; break; }
      if (can_start && rng_.below(4) != 0) can_start = false;
    }
    if (can_start) {
      l->tx_frame_ = std::move(l->txq_.front());
      l->txq_.pop_front();
      l->transmitting_ = true;
      l->tx_start_ = now;
      l->tx_end_ = now + std::max<uint32_t>(1, airtime(l->tx_frame_.size()));
      l->sent_frames++;
      l->tx_intervals_.push_back({l->tx_start_, l->tx_end_});
      while (l->tx_intervals_.size() > 16) l->tx_intervals_.pop_front();
      if (l->log_frames) l->sent_log.push_back(l->tx_frame_);
    }
  }
}

Node& Sim::add_node(const std::string& name, const MeshConfig& cfg) {
  auto n = std::make_unique<Node>();
  n->name = name;
  std::string seed = "sim-node-" + name + "-" + std::to_string(seed_) + "-padding-to-32-bytes";
  crypto::Csprng r{ByteView(std::string_view(seed))};
  n->id = Identity::generate(r);
  Bytes ent = r.bytes(32);
  n->eng = std::make_unique<MeshEngine>(n->id, ent, cfg);
  n->eng->set_delegate(&n->rec);
  n->eng->set_profile(name, "", "");
  nodes.push_back(std::move(n));
  return *nodes.back();
}

Medium& Sim::add_medium(const LinkCaps& caps, uint32_t delay_ms) {
  media.push_back(std::make_unique<Medium>(rng_.next(), caps));
  media.back()->set_delay(delay_ms);
  return *media.back();
}

SimLink& Sim::join(Node& n, Medium& m) {
  uint32_t id = next_link_id_++;
  n.links.push_back(std::make_unique<SimLink>(&m, &n, id));
  SimLink* l = n.links.back().get();
  m.members.push_back(l);
  n.eng->attach_link(id, l);
  return *l;
}

void Sim::introduce(Node& a, Node& b) {
  a.eng->add_known_keys(b.id.public_keys(), b.name);
  b.eng->add_known_keys(a.id.public_keys(), a.name);
}

void Sim::introduce_all() {
  for (auto& a : nodes)
    for (auto& b : nodes)
      if (a != b) a->eng->add_known_keys(b->id.public_keys(), b->name);
}

void Sim::step(uint32_t dt) {
  now_ += dt;
  std::vector<Delivery> fresh;
  for (auto& m : media) m->step(now_, fresh);
  for (auto& d : fresh) {
    if (tx_hook) tx_hook(*d.from, d.frame);
    pending_.push_back(std::move(d));
  }
  // Collisions: a receiver loses frames that overlap another frame it heard, or its own TX.
  std::vector<Delivery> ready;
  for (size_t i = 0; i < pending_.size();) {
    if (pending_[i].at_ms <= now_) {
      ready.push_back(std::move(pending_[i]));
      pending_[i] = std::move(pending_.back());
      pending_.pop_back();
    } else {
      ++i;
    }
  }
  for (auto& d : ready) {
    Medium* m = d.to->medium();
    if (m->collisions_enabled()) {
      bool collided = false;
      if (d.to->last_tx_overlaps(d.tx_start, d.tx_end)) collided = true;
      for (SimLink* o : m->members) {
        if (o == d.from || o == d.to || !m->hears(o, d.to)) continue;
        if (o->overlaps(d.tx_start, d.tx_end)) { collided = true; break; }
      }
      if (collided) { m->frames_collided++; continue; }
    }
    Bytes f = std::move(d.frame);
    if (rx_hook && !rx_hook(*d.to, f)) continue;
    if (!d.to->state_up()) continue;
    RxMeta meta;
    meta.has_snr = true;
    meta.snr_db = 5.0f;
    d.to->node()->eng->on_frame(d.to->id(), f, meta);
  }
  for (auto& n : nodes) n->eng->tick(static_cast<uint64_t>(static_cast<int64_t>(now_) + n->clock_skew_ms));
}

void Sim::run(uint64_t ms, uint32_t dt) {
  uint64_t end = now_ + ms;
  while (now_ < end) step(dt);
}

uint64_t Sim::run_until(const std::function<bool()>& pred, uint64_t max_ms, uint32_t dt) {
  uint64_t start = now_;
  while (now_ - start < max_ms) {
    if (pred()) return now_ - start;
    step(dt);
  }
  return pred() ? now_ - start : UINT64_MAX;
}

}  // namespace sim
