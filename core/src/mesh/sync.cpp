// Mesh engine: neighbour beacons, mesh-time estimation and store reconciliation.
//
// Reconciliation is a symmetric, prefix-tree set comparison over 32-bit sync ids (high half of
// the 64-bit wire id). A beacon carries (count, sum) of everything we hold; when a neighbour's
// differs, one side sends 16 child digests of the root, the other answers each differing child
// with either its id list (small subtrees) or the next level of digests, and so on. Leaves end
// in exact id lists from which each side pushes what the other lacks and asks (WANT) for what
// it lacks itself. In steady state the cost is one ~30-byte beacon per interval.
#include <algorithm>
#include <unordered_set>

#include "mesh/impl.h"

namespace ember::mesh {

using namespace proto;

namespace {
constexpr size_t kIdsThreshold = 16;  // send id lists for subtrees at most this big
constexpr size_t kMaxSyncReply = 900;  // bytes per sync message (fragmented on small MTUs)

// Three speed classes: fast (LAN, BLE, Wi-Fi), medium (LoRa, 9600 bd packet), slow (1200 bd).
uint32_t beacon_interval_ms(const LinkCaps& c) {
  if (c.bitrate_bps >= 100000) return 30000;
  if (c.bitrate_bps >= 2400) return 120000;
  return 600000;
}
uint32_t beacon_gap_ms(const LinkCaps& c) {
  if (c.bitrate_bps >= 100000) return 2000;
  if (c.bitrate_bps >= 2400) return 10000;
  return 60000;
}
uint32_t sync_gap_ms(const LinkCaps& c) {
  if (c.bitrate_bps >= 100000) return 4000;
  if (c.bitrate_bps >= 2400) return 20000;
  return 120000;
}
uint32_t prefix_mask(uint8_t bits) { return bits == 0 ? 0u : ~0u << (32 - bits); }
}  // namespace

void MeshEngine::Impl::beacon_timers(LinkSlot& L, uint64_t now) {
  if (!cfg.beacons_enabled) return;
  uint32_t iv = beacon_interval_ms(L.caps);
  if (L.next_beacon_ms == 0) L.next_beacon_ms = now + 300 + rng.uniform(1200);
  bool soon = L.beacon_soon && now - L.last_beacon_ms >= beacon_gap_ms(L.caps) && !L.nbrs.empty();
  // Our own fresh message may have been lost to a collision (broadcasts are not acknowledged):
  // advertise the new digest soon after a short post-transmission quiet time, so neighbours that
  // missed it reconcile within seconds rather than a full beacon gap.
  if (!soon && L.beacon_soon && !L.nbrs.empty() && L.own_new_ms > L.last_beacon_ms) {
    // Quiet time counted from the end of our last transmission (others get the channel first).
    uint64_t quiet = std::max<uint64_t>(2000, 4ull * airtime_ms(L, L.caps.mtu));
    uint64_t since = std::max(L.own_new_ms, L.last_tx_end_ms);
    soon = now >= since + quiet && now - L.last_beacon_ms >= std::min<uint64_t>(beacon_gap_ms(L.caps), 15000) &&
           L.qframes == 0;
  }
  if (soon || now >= L.next_beacon_ms) {
    send_beacon(L);
    L.last_beacon_ms = now;
    L.next_beacon_ms = now + iv + rng.uniform(iv / 5 + 1);
    L.beacon_soon = false;
  }
  // Keep reconciling while a neighbour's last digest disagreed with ours (lost sync messages are
  // retried without waiting for its next beacon).
  if (!cfg.sync_enabled) return;
  uint64_t gap = sync_gap_ms(L.caps);
  for (auto& kv : L.nbrs) {
    Neighbour& n = kv.second;
    if (!n.has_digest || !n.mismatch_since) continue;
    if (now - n.last_sync_ms < gap + n.sync_backoff_ms) continue;
    bool my_turn = my_hop < n.hop || now - n.mismatch_since >= 2 * (gap + n.sync_backoff_ms);
    if (my_turn) start_sync(L, n);
  }
}

void MeshEngine::Impl::send_beacon(LinkSlot& L) {
  Beacon b;
  b.node = me;
  b.relay = cfg.relay;
  b.has_time = tquality < 15;
  b.time_ms = mesh_now();
  b.time_quality = tquality;
  root_digest(L, b.store_count, b.store_sum);
  send_control(L, encode_beacon(b), 4);
}

void MeshEngine::Impl::handle_beacon(LinkSlot& L, uint32_t hop, const Beacon& b) {
  if (short_id(b.node) != hop || b.node == me) { L.st.rx_bad++; st.rx_bad++; return; }
  auto it = L.nbrs.find(hop);
  if (it == L.nbrs.end()) return;
  Neighbour& n = it->second;
  bool first = !n.has_node;
  n.node = b.node;
  n.has_node = true;
  n.relay = b.relay;
  n.has_digest = true;
  n.digest_count = b.store_count;
  n.digest_sum = b.store_sum;
  n.digest_ms = host_now;
  // Peer table: a beacon proves a direct neighbour on this link.
  bool is_new = false;
  PeerRec& p = peer_rec(b.node, &is_new);
  uint64_t mnow = mesh_now();
  uint64_t prev_seen = p.info.last_seen_ms;
  p.info.last_seen_ms = mnow;
  p.info.hops = 0;
  p.hops_ms = mnow;
  p.info.direct = true;
  // The frame's station id is unauthenticated: it is not copied into the peer's callsign (only
  // the signed announcement sets that; security review L6).
  PeerLink* pl = nullptr;
  for (auto& x : p.info.links)
    if (x.link_id == L.id) pl = &x;
  if (!pl) {
    if (p.info.links.size() >= 8) p.info.links.erase(p.info.links.begin());
    p.info.links.push_back(PeerLink{});
    pl = &p.info.links.back();
    pl->link_id = L.id;
  }
  pl->last_rx_ms = mnow;
  pl->has_rssi = n.has_rssi;
  pl->rssi_dbm = n.rssi;
  pl->has_snr = n.has_snr;
  pl->snr_db = n.snr;
  if (is_new || first || mnow - prev_seen > 10000) notify_peer(p, is_new);
  // No WHOIS just because a stranger beaconed on a slow shared channel: that is a reactive
  // transmission (and its answer another). Keys are asked for when something of theirs actually
  // needs verifying (rx.cpp), and senders piggy-back their announcement on a first message.
  if (!p.info.has_keys && !contended(L)) send_whois(b.node);  // cheap on fast links: names for the UI
  if (first) L.beacon_soon = true;

  // Beacon time is unauthenticated and self-declared: informational only, never used to set the
  // clock (security review C1). Time evidence comes from signed announcements (time_sample).

  if (!cfg.sync_enabled) return;
  uint32_t mc, ms;
  root_digest(L, mc, ms);
  if (mc == b.store_count && ms == b.store_sum) {
    if (L.nbrs.size() == 1) L.repairs.clear();  // the only neighbour holds everything we do
    n.mismatch_since = 0;
    n.sync_backoff_ms = 0;
    return;
  }
  if (!n.mismatch_since) n.mismatch_since = host_now;
  uint64_t gap = uint64_t(sync_gap_ms(L.caps)) + n.sync_backoff_ms;
  // The side that holds less starts at once (it wants what the other advertises).
  bool my_turn = my_hop < hop || host_now - n.mismatch_since >= 2 * gap || b.store_count > mc;
  if ((n.last_sync_ms == 0 || host_now - n.last_sync_ms >= gap) && my_turn) start_sync(L, n);
}

// Mesh time (security review C1/M1; docs/PROTOCOL.md §9):
//  * evidence is the `created` time of signed ANNOUNCEs heard directly from their origin (origin's
//    short id == the frame's hop_src, hops 0), with the time quality the origin signed into it;
//  * GPS / manual time and a sane system clock (quality < 15) are never moved by the mesh;
//  * a node without a usable clock (quality 15) adopts the median of the largest cluster (±60 s)
//    of the best-quality samples: from pinned contacts if any (one suffices), otherwise at least two
//    distinct origins that are a strict majority of that quality class;
//  * after that, further corrections are slewed (at most 1 s per minute of uptime, at least 1 s).
void MeshEngine::Impl::time_sample(const NodeId& from, uint64_t their_ms, uint8_t quality, bool pinned) {
  if (quality >= 15 || their_ms < kMinSaneMs || their_ms > kMaxSaneMs) return;
  if (tsource == TimeSource::Gps || tsource == TimeSource::Manual) return;
  if (tsource == TimeSource::System && tquality < 15) return;  // a sane local clock always wins
  // Both values are < 2^62, so the difference cannot overflow.
  int64_t off = static_cast<int64_t>(their_ms) - static_cast<int64_t>(host_now);
  bool replaced = false;
  for (auto& s : tsamples)
    if (s.from == from) { s.offset_ms = off; s.quality = quality; s.at_ms = host_now; s.pinned = pinned; replaced = true; }
  if (!replaced) {
    if (tsamples.size() >= 32) tsamples.erase(tsamples.begin());
    tsamples.push_back(TimeSample{from, off, quality, host_now, pinned});
  }
  std::vector<const TimeSample*> use;
  bool any_pinned = false;
  for (auto& s : tsamples)
    if (host_now - s.at_ms < 30 * 60000ull && s.quality < tquality) {
      use.push_back(&s);
      any_pinned = any_pinned || s.pinned;
    }
  if (any_pinned) {
    std::vector<const TimeSample*> p;
    for (auto* s : use) if (s->pinned) p.push_back(s);
    use.swap(p);
  }
  if (use.empty()) return;
  uint8_t best = 15;
  for (auto* s : use) best = std::min(best, s->quality);
  std::vector<int64_t> offs;
  for (auto* s : use) if (s->quality == best) offs.push_back(s->offset_ms);
  std::sort(offs.begin(), offs.end());
  size_t bi = 0, bn = 0;  // largest window of offsets within 60 s
  for (size_t i = 0, j = 0; i < offs.size(); ++i) {
    while (offs[i] - offs[j] > 60000) ++j;
    if (i - j + 1 > bn) { bn = i - j + 1; bi = j; }
  }
  size_t need = any_pinned ? 1 : 2;
  if (bn < need || bn * 2 <= offs.size()) return;
  int64_t target = offs[bi + bn / 2];
  uint8_t q = static_cast<uint8_t>(std::max<int>(9, best + 1));
  if (q > 14) q = 14;
  bool changed;
  if (tquality >= 15) {
    changed = true;  // no clock at all: adopt
    offset_ms = target;
  } else {
    uint64_t since = last_time_adjust ? host_now - last_time_adjust : 60000;
    int64_t max_step = static_cast<int64_t>(std::max<uint64_t>(1000, std::min<uint64_t>(since / 60, 600000)));
    int64_t d = target - offset_ms;
    if (d > max_step) d = max_step;
    if (d < -max_step) d = -max_step;
    changed = d > 1000 || d < -1000 || tsource != TimeSource::Mesh;
    offset_ms += d;
  }
  last_time_adjust = host_now;
  tquality = std::min(tquality, q);
  tsource = TimeSource::Mesh;
  if (changed) {
    if (del) del->on_time(mesh_now(), TimeSource::Mesh);
    dirty();
  }
}

bool MeshEngine::Impl::sync_eligible(const LinkSlot& L, const StoreEntry& e) const {
  if (e.expires_local <= host_now) return false;
  if (!L.caps.encryption_allowed && !legal_cleartext(e.h.sec)) return false;
  return true;
}

void MeshEngine::Impl::root_digest(const LinkSlot& L, uint32_t& count, uint32_t& sum) const {
  // Cached per link until the store changes (beacons arrive far more often than it does). Expiry
  // only removes entries at gc, which also changes the store generation.
  if (L.digest_gen == store.generation()) {
    count = L.digest_count;
    sum = L.digest_sum;
    return;
  }
  count = 0;
  sum = 0;
  for (const auto& kv : store.all()) {
    if (!sync_eligible(L, kv.second)) continue;
    count++;
    sum += kv.second.sid;
  }
  LinkSlot& m = const_cast<LinkSlot&>(L);
  m.digest_gen = store.generation();
  m.digest_count = count;
  m.digest_sum = sum;
}

void MeshEngine::Impl::digest_children(const LinkSlot& L, uint8_t bits, uint32_t prefix,
                                       std::array<SyncDigest, 16>& res) const {
  res = {};
  uint32_t mask = prefix_mask(bits);
  int shift = 28 - bits;
  for (const auto& kv : store.all()) {
    const StoreEntry& e = kv.second;
    if ((e.sid & mask) != prefix || !sync_eligible(L, e)) continue;
    SyncDigest& d = res[(e.sid >> shift) & 15];
    if (d.count < 0xFFFF) d.count++;
    d.sum += e.sid;
  }
}

void MeshEngine::Impl::ids_in(const LinkSlot& L, uint8_t bits, uint32_t prefix, std::vector<uint32_t>& res) const {
  res.clear();
  uint32_t mask = prefix_mask(bits);
  for (const auto& kv : store.all()) {
    const StoreEntry& e = kv.second;
    if ((e.sid & mask) == prefix && sync_eligible(L, e)) res.push_back(e.sid);
  }
  std::sort(res.begin(), res.end());
  res.erase(std::unique(res.begin(), res.end()), res.end());
  if (res.size() > 255) res.resize(255);
}

void MeshEngine::Impl::start_sync(LinkSlot& L, Neighbour& n) {
  // A round that moved nothing in either direction backs off (sync-id collisions, packets we
  // hold but may not forward, or a neighbour that ignores us).
  if (n.last_sync_ms) {
    // Only a round the neighbour answered counts; lost messages just mean "try again".
    if (n.round_rx > 0 && n.round_pushed == 0 && n.round_wanted == 0) {
      uint32_t g = sync_gap_ms(L.caps);
      n.sync_backoff_ms = std::min<uint32_t>(n.sync_backoff_ms ? n.sync_backoff_ms * 2 : g, 30 * 60000);
    } else {
      n.sync_backoff_ms = 0;
    }
  }
  n.round_pushed = 0;
  n.round_wanted = 0;
  n.round_rx = 0;
  n.last_sync_ms = host_now;
  SyncMsg m;
  m.target = n.hop;
  SyncItem it;
  it.type = SyncItem::Dig;
  it.bits = 0;
  it.prefix = 0;
  digest_children(L, 0, 0, it.children);
  m.items.push_back(std::move(it));
  st.sync_rounds++;
  send_control(L, encode_sync(m), 6);
}

void MeshEngine::Impl::push_sid(LinkSlot& L, uint32_t sid, uint32_t& budget) {
  // Indexed by sync id: a WANT for ids we do not hold costs one hash lookup (security review H3).
  for (uint64_t w : store.by_sid(sid)) {
    if (!budget) return;
    const StoreEntry* ep = store.get(w);
    if (!ep) continue;
    const StoreEntry& e = *ep;
    if (!e.transmittable() || !sync_eligible(L, e)) continue;
    if (push_bytes_left_ < e.raw.size()) { budget = 0; return; }
    uint8_t prio = link_prio(e.h);
    if (prio > 0) prio = 6;  // reconciliation is bulk traffic, except emergencies
    if (send_raw_packet_on(L, e.raw, e.h, prio, e.wire, false)) {
      budget--;
      push_bytes_left_ -= e.raw.size();
      st.sync_pushed++;
    }
  }
}

void MeshEngine::Impl::handle_sync(LinkSlot& L, uint32_t hop, const SyncMsg& m) {
  auto nit = L.nbrs.find(hop);
  if (nit == L.nbrs.end()) return;
  Neighbour& n = nit->second;
  // Only neighbours that introduced themselves with a beacon, at a bounded message rate
  // (security review H3).
  if (!n.has_node) return;
  bool fast = L.caps.bitrate_bps >= 100000;
  if (!n.sync_b.take(host_now, fast ? 240 : 30, fast ? 60 : 16)) return;
  if (n.last_sync_ms == 0 || host_now - n.last_sync_ms > sync_gap_ms(L.caps)) {
    n.round_pushed = 0;
    n.round_wanted = 0;
    n.round_rx = 0;
    n.round_bytes = 0;
  }
  n.round_rx++;
  n.last_sync_ms = host_now;
  uint32_t budget = cfg.sync_push_limit;
  // Bytes pushed per neighbour per round, by link speed.
  uint32_t round_cap = fast ? 64 * 1024 : (L.caps.bitrate_bps >= 2400 ? 16 * 1024 : 4 * 1024);
  push_bytes_left_ = n.round_bytes < round_cap ? round_cap - n.round_bytes : 0;
  size_t ids_left = 512, digs_left = 16;  // work per message
  SyncMsg reply;
  reply.target = hop;
  size_t reply_size = 6;
  std::vector<uint32_t> wants;
  auto add_item = [&](SyncItem&& it) {
    size_t sz = sync_item_size(it);
    if (reply_size + sz > kMaxSyncReply || reply.items.size() >= kMaxSyncItems - 1) return;
    reply_size += sz;
    reply.items.push_back(std::move(it));
  };
  auto handle_ids = [&](uint8_t bits, uint32_t prefix, const std::vector<uint32_t>& theirs) {
    std::vector<uint32_t> mine;
    ids_in(L, bits, prefix, mine);
    std::unordered_set<uint32_t> ts(theirs.begin(), theirs.end());
    std::unordered_set<uint32_t> ms(mine.begin(), mine.end());
    uint32_t before = budget;
    for (uint32_t sid : mine)
      if (!ts.count(sid)) push_sid(L, sid, budget);
    n.round_pushed += before - budget;
    for (uint32_t sid : theirs)
      if (!ms.count(sid) && !seen_sids.count(sid) && wants.size() < 255) wants.push_back(sid);
  };
  for (const SyncItem& it : m.items) {
    if (it.type == SyncItem::Dig) {
      if (digs_left == 0) continue;
      --digs_left;
    } else {
      if (ids_left < it.ids.size()) continue;
      ids_left -= it.ids.size();
    }
    switch (it.type) {
      case SyncItem::Dig: {
        if (it.bits >= 28) {  // cannot split further: answer with the exact ids
          SyncItem r;
          r.type = SyncItem::Ids;
          r.bits = 28;
          r.prefix = it.prefix;
          ids_in(L, 28, it.prefix, r.ids);
          add_item(std::move(r));
          break;
        }
        std::array<SyncDigest, 16> mine;
        digest_children(L, it.bits, it.prefix, mine);
        for (uint32_t c = 0; c < 16; ++c) {
          if (mine[c] == it.children[c]) continue;
          uint8_t cbits = static_cast<uint8_t>(it.bits + 4);
          uint32_t cpre = it.prefix | (c << (28 - it.bits));
          SyncItem r;
          if (mine[c].count <= kIdsThreshold || cbits >= 28) {
            r.type = SyncItem::Ids;
            r.bits = cbits;
            r.prefix = cpre;
            ids_in(L, cbits, cpre, r.ids);
          } else {
            r.type = SyncItem::Dig;
            r.bits = cbits;
            r.prefix = cpre;
            digest_children(L, cbits, cpre, r.children);
          }
          add_item(std::move(r));
        }
        break;
      }
      case SyncItem::Ids:
        handle_ids(it.bits, it.prefix, it.ids);
        break;
      case SyncItem::Want: {
        uint32_t before = budget;
        for (uint32_t sid : it.ids) push_sid(L, sid, budget);
        n.round_pushed += before - budget;
        break;
      }
    }
  }
  n.round_bytes = static_cast<uint32_t>(round_cap - std::min<size_t>(push_bytes_left_, round_cap));
  if (!wants.empty()) {
    SyncItem w;
    w.type = SyncItem::Want;
    w.ids = wants;
    add_item(std::move(w));
    n.round_wanted += static_cast<uint32_t>(wants.size());
  }
  bool asked_root = false;
  for (const SyncItem& it : m.items)
    if (it.type == SyncItem::Dig && it.bits == 0) asked_root = true;
  if (m.items.empty()) {
    // "Nothing differs" from the neighbour: we are in sync with it.
    n.mismatch_since = 0;
    n.sync_backoff_ms = 0;
    return;
  }
  if (!reply.items.empty() || asked_root) send_control(L, encode_sync(reply), 6);
}

}  // namespace ember::mesh
