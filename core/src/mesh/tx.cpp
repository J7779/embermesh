// Mesh engine: transmit path — legal-mode filter, priority queues, fragmentation, airtime and
// duty-cycle budgeting, managed flooding with randomised / SNR-weighted delay and suppression.
#include <algorithm>
#include <cmath>

#include "mesh/impl.h"

namespace ember::mesh {

using namespace proto;

namespace {
constexpr size_t kMaxTxFrags = 16;
constexpr size_t kMaxFloodsPerLink = 512;
constexpr uint64_t kTxFragKeepMs = 120000;
}  // namespace

// Strict priority classes (0 = highest): SOS > acks > direct > channel > beacons/announce > bulk.
uint8_t MeshEngine::Impl::link_prio(const Header& h) {
  if (h.prio == Prio::Emergency || h.kind == Kind::Sos || h.kind == Kind::SosAck) return 0;
  switch (h.kind) {
    case Kind::Ack: case Kind::Read: case Kind::Pong: return 1;
    case Kind::File: return 6;
    default: break;
  }
  if (h.prio == Prio::High) return 2;
  if (h.dst_type == DestType::Node && (h.kind == Kind::Text || h.kind == Kind::Ping || h.kind == Kind::App))
    return h.prio == Prio::Low ? 5 : 2;
  if (h.kind == Kind::Text || h.kind == Kind::Checkin || h.kind == Kind::App) return h.prio == Prio::Low ? 5 : 3;
  return 4;  // announce, position, bulletin, netplan, whois
}

// THE legal-mode rule: links where encryption is not allowed only ever carry cleartext packets
// (signed, MAC'd or self-authenticating). Ciphertext is never queued for them, whoever made it.
bool MeshEngine::Impl::allowed_on(const LinkSlot& L, const Header& h) const {
  if (!L.caps.encryption_allowed && !legal_cleartext(h.sec)) return false;
  if (L.scope.restricted && !in_scope(L, h)) return false;
  return true;
}

// Scope of a restricted link (LinkScope in engine.h): symmetric, so both ends of a relay session
// agree on what is in it and their reconciliation digests match.
bool MeshEngine::Impl::in_scope(const LinkSlot& L, const Header& h) const {
  const LinkScope& s = L.scope;
  if (!s.restricted) return true;
  switch (h.dst_type) {
    case DestType::Node:
      return s.has_peer && ((h.origin == me && h.dst_node == s.peer) || (h.origin == s.peer && h.dst_node == me));
    case DestType::Channel:
      return s.has_channel && h.channel == s.channel;
    case DestType::Broadcast:
      // Channel sessions: members who are not contacts learn each other's keys (review M11).
      if (s.has_channel) return h.kind == Kind::Announce || h.kind == Kind::Whois;
      return s.has_peer && (h.kind == Kind::Announce || h.kind == Kind::Sos) && (h.origin == me || h.origin == s.peer);
  }
  return false;
}

// Fallback rule of relay sessions (CONNECTIVITY §4.9): the contact was heard directly (hops 0) on
// another link that is not a relay session within the last 3 minutes.
bool MeshEngine::Impl::reachable_elsewhere(const LinkSlot& L) const {
  if (!L.scope.relay_session || !L.scope.has_peer) return false;
  auto pit = peers.find(L.scope.peer);
  if (pit == peers.end()) return false;
  uint64_t now = mesh_now();  // PeerLink times are mesh time
  for (const PeerLink& pl : pit->second.info.links) {
    if (pl.link_id == L.id || now > pl.last_rx_ms + 180000) continue;
    auto lit = links.find(pl.link_id);
    if (lit != links.end() && !lit->second.scope.relay_session && lit->second.state == LinkState::Up) return true;
  }
  return false;
}

bool MeshEngine::Impl::can_transmit(LinkSlot& L) {
  if (!L.link || L.state != LinkState::Up || L.caps.mtu < kMinMtu) return false;
  if (reachable_elsewhere(L)) return false;
  if (L.caps.requires_station_id && callsign.empty()) { L.st.blocked_no_callsign++; return false; }
  return true;
}

uint32_t MeshEngine::Impl::airtime_ms(const LinkSlot& L, size_t bytes) const {
  uint32_t bps = L.caps.bitrate_bps ? L.caps.bitrate_bps : 1;
  uint64_t bits = uint64_t(bytes) * 8ull;
  uint64_t keyup = 0;
  if (L.caps.half_duplex && bps <= 20000) {
    // Slow radio links carry link-layer framing around every mesh frame (AX.25 addresses + FCS +
    // flags, LoRa / Meshtastic headers: ~20 bytes) and HDLC bit stuffing (~5 %), and key up with a
    // preamble (sound-card modems: txdelay 300 ms + tail 50 ms by default). Estimating without
    // them undercounted real AFSK1200 airtime by ~18 %, so the duty-cycle token bucket let the real
    // transmit time exceed the configured budget; the estimate is now conservative.
    bits = (bits + 20 * 8) * 105 / 100;
    keyup = bps <= 2400 ? 350 : 60;
  } else if (L.caps.half_duplex) {
    keyup = 2;
  }
  uint64_t ms = (bits * 1000ull + bps - 1) / bps + keyup;
  return static_cast<uint32_t>(ms > 0xFFFFFFFFull ? 0xFFFFFFFFu : ms);
}

void MeshEngine::Impl::account_airtime(LinkSlot& L, uint32_t ms, uint64_t now) {
  L.st.airtime_ms += ms;
  uint64_t minute = now / 60000;
  if (minute != L.air_bucket_minute) {
    uint64_t gap = minute > L.air_bucket_minute ? minute - L.air_bucket_minute : 10;
    for (uint64_t i = 1; i <= std::min<uint64_t>(gap, 10); ++i) L.air_bucket[(L.air_bucket_minute + i) % 10] = 0;
    L.air_bucket_minute = minute;
  }
  L.air_bucket[minute % 10] += ms;
}

bool MeshEngine::Impl::enqueue_frame(LinkSlot& L, Bytes&& frame, uint8_t prio, uint64_t wire, uint32_t own_pkt,
                                     uint64_t expire_ms) {
  if (prio > 7) prio = 7;
  size_t sz = frame.size();
  size_t cap = cfg.link_queue_max_bytes;
  // Make room by dropping strictly lower-priority frames (newest first).
  while (L.qbytes + sz > cap) {
    int victim = -1;
    for (int p = 7; p > prio; --p)
      if (!L.q[static_cast<size_t>(p)].empty()) { victim = p; break; }
    if (victim < 0) { L.st.dropped_queue_full++; return false; }
    auto& dq = L.q[static_cast<size_t>(victim)];
    L.qbytes -= dq.back().frame.size();
    L.qframes--;
    dq.pop_back();
    L.st.dropped_queue_full++;
  }
  QItem it;
  it.frame = std::move(frame);
  it.prio = prio;
  it.enq_ms = host_now;
  it.expire_ms = expire_ms;
  it.wire = wire;
  it.own_pkt = own_pkt;
  L.qbytes += sz;
  L.qframes++;
  L.q[prio].push_back(std::move(it));
  return true;
}

bool MeshEngine::Impl::enqueue_unit(LinkSlot& L, uint8_t inner_type, ByteView content, uint8_t prio, uint64_t wire,
                                    uint32_t own_pkt, uint64_t expire_ms) {
  FrameHeader fh;
  fh.type = static_cast<FrameType>(inner_type);
  if (L.caps.requires_station_id) fh.station = callsign;
  fh.hop_src = my_hop;
  size_t ovh = frame_overhead(fh.station.size());
  if (ovh + content.size() <= L.caps.mtu) {
    ByteWriter w(ovh + content.size());
    write_frame_header(w, fh);
    w.bytes(content);
    return enqueue_frame(L, w.take(), prio, wire, own_pkt, expire_ms);
  }
  Bytes unit;
  unit.reserve(content.size() + 1);
  unit.push_back(inner_type);
  unit.insert(unit.end(), content.begin(), content.end());
  uint16_t fid = L.next_frag_id++;
  if (L.next_frag_id == 0) L.next_frag_id = 1;
  std::vector<Bytes> frames = fragment_unit(unit, L.caps.mtu, fh, fid);
  if (frames.empty()) { L.st.dropped_queue_full++; return false; }
  TxFrag tf;
  tf.frag_id = fid;
  tf.frames = frames;
  tf.prio = prio;
  tf.ms = host_now;
  tf.wire = wire;
  while (L.txfrags.size() >= kMaxTxFrags) L.txfrags.pop_front();
  L.txfrags.push_back(std::move(tf));
  bool ok = true;
  for (size_t i = 0; i < frames.size(); ++i)
    ok = enqueue_frame(L, std::move(frames[i]), prio, wire, i == 0 ? own_pkt : 0, expire_ms) && ok;
  return ok;
}

bool MeshEngine::Impl::send_raw_packet_on(LinkSlot& L, ByteView raw, const Header& h, uint8_t prio, uint64_t wire,
                                          bool own) {
  if (!allowed_on(L, h)) { L.st.blocked_legal++; return false; }

  if (!can_transmit(L)) return false;
  // Frames that wait too long in a queue are stale for flooding purposes (DTN sync will resend).
  uint64_t keep = prio == 0 ? 30 * 60000ull : (prio >= 6 ? 10 * 60000ull : 15 * 60000ull);
  return enqueue_unit(L, static_cast<uint8_t>(FrameType::Packet), raw, prio, wire, own ? h.pkt_id : 0, host_now + keep);
}

void MeshEngine::Impl::send_packet_on(LinkSlot& L, const StoreEntry& e, uint8_t prio, bool own) {
  send_raw_packet_on(L, e.raw, e.h, prio, e.wire, own);
}

void MeshEngine::Impl::flood_own(const StoreEntry& e) {
  uint8_t prio = force_prio >= 0 ? static_cast<uint8_t>(force_prio) : link_prio(e.h);
  for (auto& kv : links) send_raw_packet_on(kv.second, e.raw, e.h, prio, e.wire, true);
}

void MeshEngine::Impl::send_control(LinkSlot& L, ByteView body, uint8_t prio) {
  if (!can_transmit(L)) return;
  enqueue_unit(L, static_cast<uint8_t>(FrameType::Control), body, prio, 0, 0, host_now + 60000);
}

void MeshEngine::Impl::schedule_flood(const PacketView& pv, uint64_t wire, uint32_t from_link, const RxMeta& meta) {
  for (auto& kv : links) {
    LinkSlot& L = kv.second;
    if (!L.link || L.state != LinkState::Up) continue;
    bool same = L.id == from_link;
    if (same && !L.caps.broadcast) continue;  // point-to-point: never echo back to the sender
    // Slow shared channel with a single neighbour: it is the one we just heard it from.
    if (same && contended(L) && L.nbrs.size() <= 1) continue;
    if (!allowed_on(L, pv.h)) { L.st.blocked_legal++; continue; }
    // Nothing that arrived over an internet relay goes onto amateur radio (review M12).
    if (L.caps.requires_station_id && from_relay_session(from_link)) { L.st.blocked_legal++; continue; }
    if (L.floods.size() >= kMaxFloodsPerLink) continue;
    uint32_t air = airtime_ms(L, pv.raw.size() + 8);
    uint64_t delay;
    if (same) {
      // Listen-before-relay window. Nodes that heard the packet weakly (far from the sender)
      // relay first; close nodes wait longer and are usually suppressed.
      uint32_t W = std::max<uint32_t>(40, 3 * air);
      double f = 0.5;
      if (meta.has_snr) f = std::min(1.0, std::max(0.0, (meta.snr_db + 5.0) / 20.0));
      else if (meta.has_rssi) f = std::min(1.0, std::max(0.0, (meta.rssi_dbm + 110.0) / 60.0));
      delay = static_cast<uint64_t>(f * W) + rng.uniform(W);
    } else {
      delay = rng.uniform(L.caps.broadcast ? std::max<uint32_t>(20, air) : 10);  // bridge
    }
    PendingFlood pf;
    pf.raw = pv.raw.to_bytes();
    pf.wire = wire;
    pf.due_ms = host_now + delay;
    pf.prio = link_prio(pv.h);
    L.floods.push_back(std::move(pf));
  }
}

void MeshEngine::Impl::heard_duplicate(uint64_t wire, uint32_t link_id) {
  auto it = links.find(link_id);
  if (it == links.end()) return;
  for (auto& f : it->second.floods)
    if (f.wire == wire && f.heard < 255) f.heard++;
}

void MeshEngine::Impl::run_floods(uint64_t now) {
  for (auto& kv : links) {
    LinkSlot& L = kv.second;
    if (L.floods.empty()) continue;
    std::vector<PendingFlood> due;
    for (size_t i = 0; i < L.floods.size();) {
      if (L.floods[i].due_ms <= now) {
        due.push_back(std::move(L.floods[i]));
        L.floods[i] = std::move(L.floods.back());
        L.floods.pop_back();
      } else {
        ++i;
      }
    }
    for (auto& f : due) {
      // On a slow shared channel one relayed copy already reached everyone who heard it: airtime
      // there is the scarce resource, so suppress after the first copy (reconciliation covers the
      // rare partial mesh).
      uint8_t thr = contended(L) && cfg.suppress_threshold ? 1 : cfg.suppress_threshold;
      if (L.caps.broadcast && thr && f.heard >= thr) {
        st.suppressed++;
        continue;
      }
      PacketView pv;
      if (!parse_packet(f.raw, pv)) continue;
      if (send_raw_packet_on(L, f.raw, pv.h, f.prio, f.wire, false)) st.relayed++;
    }
  }
}

// Listen-before-talk etiquette for a shared slow half-duplex channel. After a frame is heard,
// listeners do not key up at once: the station that just transmitted may still be continuing (its
// next burst starts within its own CSMA slots), and several listeners reacting to the same frame
// would otherwise start together inside the modems' carrier-detect latency and collide. Each frame
// heard draws one random backoff; the wait is a quiet window plus that backoff, both scaled to the
// airtime of a typical frame, shortest for emergencies and user traffic, longest for beacons,
// announcements and reconciliation (those are never reactive in a hurry).
uint64_t MeshEngine::Impl::holdoff_until(LinkSlot& L, uint8_t prio) {
  if (!contended(L) || !L.heard_ms) return 0;
  if (L.holdoff_for != L.heard_ms) {
    L.holdoff_for = L.heard_ms;
    L.holdoff_r = rng.uniform(1000000) / 1000000.0;
  }
  double a = airtime_ms(L, std::min<size_t>(L.caps.mtu, 128));
  double base, span;
  if (prio == 0) { base = 0.25 * a; span = 0.5 * a; }
  else if (prio <= 3) { base = a; span = 2 * a; }
  else { base = 3 * a; span = 4 * a; }
  return L.heard_ms + static_cast<uint64_t>(base + L.holdoff_r * span);
}

void MeshEngine::Impl::pump(LinkSlot& L, uint64_t now) {
  if (!L.link || L.state != LinkState::Up || L.caps.mtu < kMinMtu) return;
  if (L.caps.requires_station_id && callsign.empty()) return;
  // Duty-cycle token bucket (ms of airtime). Unlimited links skip budgeting but are still paced.
  uint32_t duty = L.caps.duty_cycle_permille;
  bool unlimited = duty >= 1000;
  if (!unlimited) {
    double per_ms = duty / 1000.0;
    double cap = std::max(2.0 * airtime_ms(L, L.caps.mtu), per_ms * 600000.0);
    if (now > L.token_ms) L.tokens_ms += (now - L.token_ms) * per_ms;
    if (L.tokens_ms > cap) L.tokens_ms = cap;
  }
  L.token_ms = now;
  size_t driver_depth = L.caps.bitrate_bps >= 1000000 ? 16 : 2;
  int guard = 0;
  while (L.qframes && guard++ < 256) {
    if (now < L.busy_until) break;
    if (L.link->channel_busy()) break;
    if (L.link->tx_queue_depth() >= driver_depth) break;
    std::deque<QItem>* q = nullptr;
    for (auto& dq : L.q)
      if (!dq.empty()) { q = &dq; break; }
    if (!q) break;
    QItem& it = q->front();
    if (it.expire_ms && it.expire_ms < now) {
      L.qbytes -= it.frame.size();
      L.qframes--;
      q->pop_front();
      continue;
    }
    if (now < holdoff_until(L, it.prio)) break;
    uint32_t air = airtime_ms(L, it.frame.size());
    if (!unlimited && L.tokens_ms < air) break;  // wait for the duty-cycle budget
    if (!L.link->send(it.frame, it.prio)) break;  // driver full; retry on a later tick
    if (!unlimited) L.tokens_ms -= air;
    // Pace slow / half-duplex media ourselves so strict priority holds.
    if (L.caps.half_duplex || L.caps.bitrate_bps < 1000000) L.busy_until = now + air;
    L.last_tx_end_ms = now + air;
    account_airtime(L, air, now);
    L.st.tx_frames++;
    L.st.tx_bytes += it.frame.size();
    st.tx_frames++;
    uint32_t own = it.own_pkt;
    uint64_t it_wire = it.wire;
    L.qbytes -= it.frame.size();
    L.qframes--;
    q->pop_front();
    if (own) set_status(MsgId{me, own}, MsgStatus::Sent);
    // Start the repair clock of an own fresh packet when it actually goes on air.
    if (it_wire)
      for (Repair& r : L.repairs)
        if (r.wire == it_wire && !r.due_ms) {
          double a = airtime_ms(L, std::min<size_t>(L.caps.mtu, 128));
          r.due_ms = now + air + static_cast<uint64_t>(3 * a + rng.uniform(static_cast<uint32_t>(3 * a) + 1));
        }
  }
}

// Discovery on a slow shared channel (stations started in any order): one announcement in reply to
// newly heard neighbours, at most one per minute per link, skipped if we announced after the newest
// of them was first heard (they heard that one, or will ask with WHOIS when they need our key) or
// if every new neighbour has already proven it holds our keys (a direct pairwise-authenticated
// packet to us).
// The listen-before-talk holdoff (prio 4 = housekeeping) still applies when it is queued.
void MeshEngine::Impl::discovery_timers(LinkSlot& L, uint64_t now) {
  if (!L.disc_due_ms || now < L.disc_due_ms) return;
  if (!contended(L) || L.state != LinkState::Up) { L.disc_due_ms = 0; return; }
  if (last_announce_ms && last_announce_ms >= L.disc_for_ms) { L.disc_due_ms = 0; return; }
  // Skip it if every neighbour heard since our last announcement already has our keys.
  bool someone_needs_it = false;
  for (const auto& kv : L.nbrs)
    if (kv.second.first_ms > last_announce_ms && !kv.second.knows_us) someone_needs_it = true;
  if (!someone_needs_it) { L.disc_due_ms = 0; return; }
  if (L.disc_last_ms && now - L.disc_last_ms < 60000) { L.disc_due_ms = L.disc_last_ms + 60000; return; }
  L.disc_due_ms = 0;
  L.disc_last_ms = now;
  announce();
}

// Evidence that a neighbour holds one of our packets: stop repairing it on this link.
void MeshEngine::Impl::evidence_of(LinkSlot& L, uint64_t wire) {
  for (size_t i = 0; i < L.repairs.size(); ++i)
    if (L.repairs[i].wire == wire) {
      L.repairs[i] = L.repairs.back();
      L.repairs.pop_back();
      return;
    }
}

void MeshEngine::Impl::repair_timers(LinkSlot& L, uint64_t now) {
  for (size_t i = 0; i < L.repairs.size();) {
    Repair& r = L.repairs[i];
    const StoreEntry* e = store.get(r.wire);
    bool gone = !e || now - r.created_ms > 10 * 60000ull;
    if (!gone && r.due_ms && now >= r.due_ms && L.nbrs.empty()) {
      // Nobody heard on this link yet: keep the re-send for the first neighbour that shows up.
      r.due_ms = now + airtime_ms(L, std::min<size_t>(L.caps.mtu, 128));
    } else if (!gone && r.due_ms && now >= r.due_ms) {
      if (r.left == 0) gone = true;
      else {
        --r.left;
        r.due_ms = 0;  // re-armed when this copy is transmitted
        L.st.repairs++;
        // The burst that carried it may also have carried our piggy-backed announcement.
        if (const StoreEntry* ae = r.announce_wire ? store.get(r.announce_wire) : nullptr)
          send_raw_packet_on(L, ae->raw, ae->h, r.prio, ae->wire, false);
        r.announce_wire = 0;
        send_raw_packet_on(L, e->raw, e->h, r.prio, r.wire, false);
      }
    }
    if (gone) {
      L.repairs[i] = L.repairs.back();
      L.repairs.pop_back();
      continue;
    }
    ++i;
  }
}

void MeshEngine::Impl::handle_frag_nack(LinkSlot& L, const FragNack& n) {
  for (auto& tf : L.txfrags) {
    if (tf.frag_id != n.frag_id || tf.frames.size() != n.count) continue;
    if (tf.resends >= 3 || host_now - tf.ms > kTxFragKeepMs) return;
    tf.resends++;
    for (uint8_t i : n.missing)
      if (i < tf.frames.size()) {
        Bytes copy = tf.frames[i];
        enqueue_frame(L, std::move(copy), tf.prio, tf.wire, 0, host_now + 60000);
      }
    return;
  }
}

void MeshEngine::Impl::reasm_timers(LinkSlot& L, uint64_t now) {
  uint32_t frame_air = airtime_ms(L, L.caps.mtu);
  uint64_t idle = std::max<uint64_t>(1500, 3ull * frame_air);
  for (auto it = L.reasm.begin(); it != L.reasm.end();) {
    Reassembly& r = *it;
    uint64_t overall = 30000 + uint64_t(r.count) * frame_air * 6;
    if (now - r.first_ms > overall) { it = L.reasm.erase(it); continue; }
    if (r.got < r.count && now - r.last_ms > idle && r.nacks < 2) {
      FragNack n;
      n.target = r.hop;
      n.frag_id = r.frag_id;
      n.count = r.count;
      for (uint8_t i = 0; i < r.count; ++i)
        if (r.parts[i].empty()) n.missing.push_back(i);
      send_control(L, encode_frag_nack(n), 1);
      r.nacks++;
      r.last_ms = now;
    }
    ++it;
  }
  while (!L.txfrags.empty() && now - L.txfrags.front().ms > kTxFragKeepMs) L.txfrags.pop_front();
}

}  // namespace ember::mesh
