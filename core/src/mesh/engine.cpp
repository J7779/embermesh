// Mesh engine: construction, origination, timers, public API and persistence.
#include <algorithm>
#include <cstdint>
#include <cstring>

#include "base/codec.h"
#include "mesh/impl.h"

namespace ember::mesh {

using namespace proto;

const char* msg_status_name(MsgStatus s) {
  switch (s) {
    case MsgStatus::Unknown: return "unknown";
    case MsgStatus::Queued: return "queued";
    case MsgStatus::Sent: return "sent";
    case MsgStatus::Relayed: return "relayed";
    case MsgStatus::Delivered: return "delivered";
    case MsgStatus::Read: return "read";
    case MsgStatus::Failed: return "failed";
  }
  return "unknown";
}

const char* time_source_name(TimeSource s) {
  switch (s) {
    case TimeSource::System: return "system";
    case TimeSource::Gps: return "gps";
    case TimeSource::Mesh: return "mesh";
    case TimeSource::Manual: return "manual";
  }
  return "system";
}

namespace {
constexpr uint64_t kClockSaneMs = 1704067200000ull;  // 2024-01-01: anything earlier is an unset clock
constexpr size_t kMaxPeers = 2048;
constexpr uint64_t kGcIntervalMs = 30000;  // expiry housekeeping; idle hosts wake for it
constexpr size_t kMaxOut = 2048;
constexpr size_t kMaxAwaitKey = 256;

uint32_t beacon_gap(const LinkCaps& c) {  // == sync.cpp beacon_gap_ms
  if (c.bitrate_bps >= 100000) return 2000;
  if (c.bitrate_bps >= 2400) return 10000;
  return 60000;
}

int status_rank(MsgStatus s) {
  switch (s) {
    case MsgStatus::Unknown: return 0;
    case MsgStatus::Queued: return 1;
    case MsgStatus::Sent: return 2;
    case MsgStatus::Relayed: return 3;
    case MsgStatus::Delivered: return 4;
    case MsgStatus::Read: return 5;
    case MsgStatus::Failed: return 1;
  }
  return 0;
}
}  // namespace

// ---------------------------------------------------------------------------------------------
MeshEngine::Impl::Impl(const Identity& ident, ByteView entropy, const MeshConfig& c)
    : id(ident), cfg(c), rng(entropy) {
  me = id.node_id();
  my_hop = short_id(me);
  rng.seed(ByteView(me.data(), 8));
  {
    // Derived from the entropy without drawing from rng (keeps the engine's sequence unchanged).
    crypto::Blake2b kb(16);
    kb.update(std::string_view("ember-mid-hash-key")).update(entropy).update(me.data(), 8);
    uint8_t k[16];
    kb.final(k);
    for (int i = 0; i < 8; ++i) {
      mid_hash.k0 = mid_hash.k0 << 8 | k[i];
      mid_hash.k1 = mid_hash.k1 << 8 | k[8 + i];
    }
  }
  delivered = ReplayGuard(mid_hash);
  acked_v = MsgFifo<uint64_t>(16384, mid_hash);
  acked_u = MsgFifo<uint64_t>(4096, mid_hash);
  reack_ms = MsgFifo<uint64_t>(4096, mid_hash);
  seen.set_capacity(cfg.dedup_capacity);
  store.set_limits(cfg.store_max_bytes, cfg.store_max_packets);
}

bool MeshEngine::Impl::ack_known(const MsgId& m, uint64_t commit) const {
  const uint64_t* v = acked_v.find(m);
  if (v && *v == commit) return true;
  const uint64_t* u = acked_u.find(m);
  return u && *u == commit;
}

void MeshEngine::Impl::remember_delivery(const MsgId& m, uint64_t expires_ms, bool auth) {
  std::vector<MsgId> ev;
  // Kept a day past expiry: packets up to a day "expired" by clock disagreement are still accepted.
  uint64_t keep = expires_ms + 86400000ull;
  bool was_auth = delivered.check(m) == ReplayGuard::kAuth;
  delivered.put(m, keep, auth, &ev);
  if (per) {
    if (auth && !was_auth) per->replay_put(m, keep);
    for (const MsgId& e : ev) per->replay_remove(e);
  }
}

void MeshEngine::Impl::dirty() {
  if (per) per->state_dirty();
}

uint32_t MeshEngine::Impl::lifetime_for(Kind k, uint32_t requested_s) const {
  uint32_t s = requested_s;
  if (s == 0) {
    switch (k) {
      case Kind::Announce: s = 7 * 86400; break;
      case Kind::Position: s = 86400; break;
      case Kind::Sos: s = 86400; break;
      case Kind::SosAck: s = 86400; break;
      case Kind::Checkin: s = 2 * 86400; break;
      case Kind::NetPlan: s = 2 * 86400; break;
      case Kind::Read: s = 86400; break;
      case Kind::Ping: case Kind::Pong: case Kind::Whois: s = 600; break;
      default: s = cfg.default_lifetime_s; break;
    }
  }
  if (s > cfg.max_lifetime_s) s = cfg.max_lifetime_s;
  if (s < 60) s = 60;
  return s;
}

PeerRec& MeshEngine::Impl::peer_rec(const NodeId& n, bool* is_new) {
  auto it = peers.find(n);
  if (it != peers.end()) {
    if (is_new) *is_new = false;
    return it->second;
  }
  if (peers.size() >= kMaxPeers) evict_peers();
  PeerRec& p = peers[n];
  p.info.id = n;
  if (is_new) *is_new = true;
  return p;
}

void MeshEngine::Impl::evict_peers() {
  // Remove the least recently seen non-pinned peer (keys of pinned contacts are never dropped).
  // Keyless entries go first (beacons / unverified traffic create those), then the least recently
  // seen keyed peer; pinned contacts never (security review M3).
  auto victim = peers.end();
  for (auto it = peers.begin(); it != peers.end(); ++it) {
    if (it->second.info.pinned) continue;
    if (victim == peers.end()) { victim = it; continue; }
    bool a = it->second.info.has_keys, b = victim->second.info.has_keys;
    if (a != b) { if (!a) victim = it; continue; }
    if (it->second.info.last_seen_ms < victim->second.info.last_seen_ms) victim = it;
  }
  if (victim != peers.end()) peers.erase(victim);
}

bool MeshEngine::Impl::peer_pair_key(PeerRec& p) {
  if (p.pair_ok) return true;
  if (!p.info.has_keys) return false;
  if (!pair_key(id, p.info.keys.dh_pk, p.pair)) return false;
  p.pair_ok = true;
  return true;
}

void MeshEngine::Impl::notify_peer(const PeerRec& p, bool is_new) {
  if (del) del->on_peer(p.info, is_new);
}

void MeshEngine::Impl::set_status(const MsgId& m, MsgStatus s) {
  auto it = out.find(m);
  if (it == out.end()) return;
  OutMsg& o = it->second;
  if (o.status == s) return;
  if (s != MsgStatus::Failed && status_rank(s) <= status_rank(o.status)) return;
  if (s == MsgStatus::Failed && status_rank(o.status) >= status_rank(MsgStatus::Delivered)) return;
  o.status = s;
  if (del) del->on_status(m, s);
  dirty();
}

void MeshEngine::Impl::track_out(const MsgId& m, const OutMsg& o) {
  if (out.size() >= kMaxOut) {
    // Drop the oldest entry that is finished (or, failing that, the oldest overall).
    bool dropped = false;
    for (auto it = out_order.begin(); it != out_order.end(); ++it) {
      auto f = out.find(*it);
      if (f == out.end()) { out_order.erase(it); dropped = true; break; }
      if (!f->second.reliable || status_rank(f->second.status) >= status_rank(MsgStatus::Delivered) ||
          f->second.status == MsgStatus::Failed) {
        out.erase(f);
        out_order.erase(it);
        dropped = true;
        break;
      }
    }
    if (!dropped && !out_order.empty()) {
      out.erase(out_order.front());
      out_order.pop_front();
    }
  }
  out[m] = o;
  out_order.push_back(m);
}

// ---------------------------------------------------------------------------------------------
Header MeshEngine::Impl::base_header(Kind k, const Dest& d, const SendOptions& o, uint32_t lifetime_s) {
  Header h;
  h.kind = k;
  h.dst_type = static_cast<DestType>(d.type);
  h.dst_node = d.node;
  h.channel = d.channel;
  h.prio = o.prio;
  uint8_t hops = o.ttl_hops ? o.ttl_hops : cfg.default_ttl;
  if (hops < 1) hops = 1;
  if (hops > kMaxTtl + 1) hops = kMaxTtl + 1;
  h.ttl = static_cast<uint8_t>(hops - 1);
  h.hop_limit = h.ttl;
  h.hops = 0;
  h.pkt_id = new_pkt_id();
  h.origin = me;
  h.created = unix_ms_to_wire(mesh_now());
  uint32_t mins = (lifetime_s + 59) / 60;
  h.lifetime_min = static_cast<uint16_t>(mins > 0xFFFF ? 0xFFFF : (mins < 1 ? 1 : mins));
  return h;
}

bool MeshEngine::Impl::seal_for(Header& h, ByteView body, Bytes& out_raw, std::string* err, bool* need_key) {
  *need_key = false;
  out_raw.clear();
  switch (h.dst_type) {
    case DestType::Broadcast:
      h.want_ack = false;
      out_raw = (h.kind == Kind::Whois) ? seal_none(h, body) : seal_signed(h, body, id);
      break;
    case DestType::Node: {
      if (h.kind == Kind::Ack) { h.want_ack = false; out_raw = seal_none(h, body); break; }
      if (h.kind == Kind::SosAck) { h.want_ack = false; out_raw = seal_signed(h, body, id); break; }
      auto it = peers.find(h.dst_node);
      if (it == peers.end() || !peer_pair_key(it->second)) { *need_key = true; return false; }
      const crypto::Key32& k = it->second.pair;
      if (h.want_ack) h.ack_commit = ack_commit(ack_secret(k, h.origin, h.pkt_id));
      if (h.kind == Kind::Read || h.kind == Kind::Ping || h.kind == Kind::Pong) {
        h.want_ack = false;
        out_raw = seal_pair_mac(h, body, k);
      } else {
        uint8_t ent[16];
        rng.fill(ent, sizeof ent);
        out_raw = seal_pair_aead(h, body, k, ByteView(ent, sizeof ent));
      }
      break;
    }
    case DestType::Channel: {
      h.want_ack = false;
      auto it = channels.find(h.channel);
      if (it == channels.end()) { if (err) *err = "unknown_channel"; return false; }
      uint8_t ent[16];
      rng.fill(ent, sizeof ent);
      out_raw = it->second.encrypted ? seal_chan(h, body, it->second, id, ByteView(ent, sizeof ent))
                                     : seal_signed(h, body, id);
      break;
    }
  }
  if (out_raw.empty()) { if (err) *err = "too_large"; return false; }
  return true;
}

SendResult MeshEngine::Impl::originate(Kind k, const Dest& d, ByteView body, const SendOptions& o,
                                       uint32_t lifetime_s, uint32_t fixed_pkt_id) {
  SendResult r;
  if (!id.valid()) { r.error = "no_identity"; return r; }
  if (!started) { r.error = "not_started"; return r; }  // tick() must run once first (sets the clock)
  if (d.type == Dest::Node && d.node == me) { r.error = "bad_params"; return r; }
  uint32_t life = lifetime_for(k, lifetime_s);
  Header h = base_header(k, d, o, life);
  if (fixed_pkt_id) h.pkt_id = fixed_pkt_id;
  h.want_ack = o.want_ack && d.type == Dest::Node && k != Kind::Ack && k != Kind::SosAck &&
               k != Kind::Read && k != Kind::Ping && k != Kind::Pong;
  r.id = MsgId{me, h.pkt_id};
  Bytes raw;
  bool need_key = false;
  if (!seal_for(h, body, raw, &r.error, &need_key)) {
    if (!need_key) return r;
    if (await_key.size() >= kMaxAwaitKey) { r.error = "busy"; return r; }
    AwaitKey a;
    a.id = r.id;
    a.to = d.node;
    a.kind = k;
    a.body = body.to_bytes();
    a.o = o;
    a.o.lifetime_s = life;
    a.created_ms = mesh_now();
    a.expires_ms = a.created_ms + uint64_t(life) * 1000;
    uint64_t a_expires = a.expires_ms;
    await_key.push_back(std::move(a));
    OutMsg om;
    om.dest = d.node;
    om.reliable = h.want_ack;
    om.status = MsgStatus::Queued;
    om.expires_ms = a_expires;
    om.created_ms = mesh_now();
    track_out(r.id, om);
    send_whois(d.node);
    r.ok = true;
    r.waiting_for_key = true;
    r.status = MsgStatus::Queued;
    return r;
  }
  PacketView pv;
  if (!parse_packet(raw, pv)) { r.error = "internal"; return r; }
  AckSecret secret{};
  if (h.want_ack) {
    auto it = peers.find(d.node);
    if (it != peers.end() && it->second.pair_ok) secret = ack_secret(it->second.pair, me, h.pkt_id);
  }
  uint64_t expires = mesh_now() + uint64_t(life) * 1000;
  inject_own(std::move(raw), pv, expires, h.want_ack, secret);
  r.ok = true;
  r.status = MsgStatus::Queued;
  return r;
}

void MeshEngine::Impl::inject_own(Bytes&& raw, const PacketView& pv_in, uint64_t expires_ms, bool reliable,
                                  const AckSecret& secret) {
  Header h = pv_in.h;
  uint64_t wire = wire_id(raw);
  if (seen.insert(wire)) seen_sids[sync_id(wire)]++;
  MsgId m{me, h.pkt_id};
  delivered.put(m, expires_ms + 86400000ull, true, nullptr);
  OutMsg om;
  om.wire = wire;
  om.dest = h.dst_node;
  om.reliable = reliable;
  om.secret = secret;
  om.status = MsgStatus::Queued;
  om.expires_ms = expires_ms;
  om.created_ms = mesh_now();
  om.interval_ms = cfg.ack_retry_base_ms;
  om.next_retry_ms = host_now + cfg.ack_retry_base_ms;
  bool track = h.kind != Kind::Ack && h.kind != Kind::Whois && h.kind != Kind::Announce;
  if (track) track_out(m, om);
  StoreEntry tmp;
  tmp.wire = wire;
  tmp.raw = raw;
  tmp.h = h;
  PacketView pv;
  parse_packet(raw, pv);
  bool user = h.kind != Kind::Ack && h.kind != Kind::Whois && h.kind != Kind::Announce && h.kind != Kind::Read &&
              h.kind != Kind::Pong;
  // Strangers on a slow shared channel verify our first message without a WHOIS round trip: our
  // (self-certifying, signed) announcement goes out right before it, at the message's priority,
  // so both share one key-up. Rate-limited by need_announce().
  uint64_t ann_wire = 0;
  if (user && need_announce()) {
    announce(link_prio(h));
    Header probe;
    probe.kind = Kind::Announce;
    probe.origin = me;
    if (const StoreEntry* ae = store.find_replace(replace_key(probe))) ann_wire = ae->wire;
  }
  store_put(pv, raw, mesh_now(), expires_ms, kStoredOwn | kStoredTransmittable);
  if (h.kind != Kind::Ack && h.kind != Kind::Whois)
    for (auto& kv : links) kv.second.own_new_ms = host_now;
  flood_own(tmp);
  // Fast, bounded repair of a fresh user packet lost to a collision on a contended link.
  uint8_t lp = link_prio(h);
  if (user && lp <= 3)
    for (auto& kv : links) {
      LinkSlot& L = kv.second;
      if (!contended(L) || L.repairs.size() >= 8 || !allowed_on(L, h)) continue;
      Repair r;
      r.created_ms = host_now;
      r.wire = wire;
      r.prio = lp;
      r.announce_wire = ann_wire;
      L.repairs.push_back(r);
    }
}

bool MeshEngine::Impl::need_announce() const {
  bool slow = false;
  uint64_t newest_nbr = 0;
  for (const auto& kv : links) {
    const LinkSlot& L = kv.second;
    if (!L.link || L.state != LinkState::Up || !contended(L)) continue;
    slow = true;
    newest_nbr = std::max(newest_nbr, L.new_nbr_ms);
  }
  if (!slow) return false;
  if (last_announce_ms == 0) return true;
  if (host_now - last_announce_ms < 10000) return false;
  return newest_nbr > last_announce_ms || host_now - last_announce_ms > 30 * 60000ull;
}

void MeshEngine::Impl::send_whois(const NodeId& target) {
  bool is_new = false;
  PeerRec& p = peer_rec(target, &is_new);
  if (p.info.has_keys) return;
  if (p.last_whois_ms && host_now - p.last_whois_ms < 60000) return;
  if (host_now - whois_window_ms > 60000) { whois_window_ms = host_now; whois_window_count = 0; }
  if (whois_window_count >= 10) return;
  ++whois_window_count;
  p.last_whois_ms = host_now;
  WhoisBody w{target};
  SendOptions o;
  o.ttl_hops = 4;
  o.prio = Prio::Normal;
  originate(Kind::Whois, Dest::broadcast(), encode(w), o, 600);
}

void MeshEngine::Impl::send_ack(const NodeId& to, uint32_t pkt_id, const AckSecret& s, uint64_t expires_ms) {
  AckBody b;
  b.acks.push_back({pkt_id, s});
  SendOptions o;
  o.prio = Prio::High;
  o.want_ack = false;
  uint64_t now = mesh_now();
  uint32_t life = expires_ms > now ? static_cast<uint32_t>(std::min<uint64_t>((expires_ms - now) / 1000 + 60, cfg.max_lifetime_s))
                                   : 3600;
  // Purge our own copies of the acknowledged packet and remember it is done.
  MsgId m{to, pkt_id};
  AckCommit c = ack_commit(s);
  uint64_t cv = 0;
  for (uint8_t x : c) cv = cv << 8 | x;
  acked_v.put(m, cv);
  for (uint64_t w : store.find_msg(m)) store_remove(w);
  originate(Kind::Ack, Dest::to_node(to), encode(b), o, life);
}

void MeshEngine::Impl::announce(int prio) {
  AnnounceBody a;
  a.keys = id.public_keys();
  a.relay = cfg.relay;
  a.name = name;
  a.callsign = callsign;
  a.status = status_text;
  a.time_quality = tquality;
  SendOptions o;
  o.prio = Prio::Normal;
  force_prio = prio;
  originate(Kind::Announce, Dest::broadcast(), encode(a), o, 0);
  force_prio = -1;
  last_announce_ms = host_now;
}

void MeshEngine::Impl::flush_await_key(const NodeId& n) {
  auto it = peers.find(n);
  if (it == peers.end() || !peer_pair_key(it->second)) return;
  std::vector<AwaitKey> ready;
  for (size_t i = 0; i < await_key.size();) {
    if (await_key[i].to == n) {
      ready.push_back(std::move(await_key[i]));
      await_key.erase(await_key.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      ++i;
    }
  }
  for (AwaitKey& a : ready) {
    uint64_t now = mesh_now();
    if (a.expires_ms <= now) { set_status(a.id, MsgStatus::Failed); continue; }
    uint32_t remaining = static_cast<uint32_t>((a.expires_ms - now) / 1000);
    // Keep the message id the app already knows.
    SendOptions o = a.o;
    Header h = base_header(a.kind, Dest::to_node(a.to), o, remaining);
    h.pkt_id = a.id.pkt_id;
    h.want_ack = o.want_ack;
    Bytes raw;
    bool need = false;
    std::string err;
    if (!seal_for(h, a.body, raw, &err, &need)) { set_status(a.id, MsgStatus::Failed); continue; }
    PacketView pv;
    if (!parse_packet(raw, pv)) continue;
    AckSecret secret{};
    auto pit = peers.find(n);
    if (h.want_ack && pit != peers.end()) secret = ack_secret(pit->second.pair, me, h.pkt_id);
    out.erase(a.id);
    inject_own(std::move(raw), pv, a.expires_ms, h.want_ack, secret);
  }
}

// ---------------------------------------------------------------------------------------------
uint64_t MeshEngine::Impl::replace_key(const Header& h) const {
  bool replaceable = h.kind == Kind::Announce || h.kind == Kind::Checkin || h.kind == Kind::NetPlan ||
                     h.kind == Kind::Sos || h.kind == Kind::Position;
  if (!replaceable) return 0;
  crypto::Blake2b b(8);
  b.update(std::string_view("ember-replace")).update(h.origin.data(), 8).u8(static_cast<uint8_t>(h.kind));
  if (h.kind == Kind::Position) {
    b.u8(static_cast<uint8_t>(h.dst_type)).update(h.dst_node.data(), 8);
    uint8_t c[4] = {static_cast<uint8_t>(h.channel >> 24), static_cast<uint8_t>(h.channel >> 16),
                    static_cast<uint8_t>(h.channel >> 8), static_cast<uint8_t>(h.channel)};
    b.update(c, 4);
  }
  uint8_t d[8];
  b.final(d);
  uint64_t v = 0;
  for (uint8_t x : d) v = v << 8 | x;
  return v ? v : 1;
}

bool MeshEngine::Impl::store_put(const PacketView& pv, Bytes raw, uint64_t received_ms, uint64_t expires_ms,
                                 uint8_t flags, uint8_t trust) {
  Kind k = pv.h.kind;
  if (k == Kind::Ping || k == Kind::Pong || k == Kind::Whois) return false;
  StoreEntry e;
  e.wire = wire_id(raw);
  if (store.contains(e.wire)) return true;
  e.sid = sync_id(e.wire);
  e.h = pv.h;
  e.received_ms = received_ms;
  e.expires_ms = expires_ms;
  {
    uint64_t mnow = mesh_now();
    uint64_t left = expires_ms > mnow ? expires_ms - mnow : 0;
    uint64_t cap = uint64_t(cfg.max_lifetime_s) * 1000ull;
    e.expires_local = host_now + (left > cap ? cap : left);
  }
  e.flags = flags;
  e.trust = (flags & kStoredOwn) ? static_cast<uint8_t>(kTrustVerified) : trust;
  e.replace_key = replace_key(pv.h);
  if (e.replace_key) {
    if (const StoreEntry* old = store.find_replace(e.replace_key)) {
      bool newer = pv.h.created > old->h.created ||
                   (pv.h.created == old->h.created && pv.h.pkt_id > old->h.pkt_id);
      // An unverified packet never replaces a verified one (a forged SOS cancel or announce
      // must not erase the genuine one), and a `created` far in the future cannot win
      // (security review H1).
      if (old->trust > e.trust) newer = false;
      if (wire_to_unix_ms(pv.h.created) > mesh_now() + 600000ull && !(flags & kStoredOwn)) newer = false;
      if (e.trust == kTrustUnknown && pv.h.kind == Kind::Sos) {
        SosBody sb;
        if (!decode(pv.body, sb) || !sb.active) newer = false;  // unverified cancels never replace
      }
      if (!newer) return false;
      store_remove(old->wire);
    }
  }
  e.raw = std::move(raw);
  std::vector<uint64_t> evicted;
  uint64_t w = e.wire;
  StoredMeta meta{received_ms, expires_ms,
                  static_cast<uint8_t>(flags | (e.trust == kTrustUnknown ? kStoredUnverified : 0))};
  Bytes copy_for_persist;
  if (per) copy_for_persist = e.raw;
  bool ok = store.put(std::move(e), host_now, &evicted);
  for (uint64_t ev : evicted) {
    st.store_evicted++;
    if (per) per->store_remove(ev);
  }
  if (!ok) return false;
  if (per) per->store_put(w, copy_for_persist, meta);
  for (auto& kv : links) kv.second.beacon_soon = true;
  return true;
}

// Trust of a packet re-inserted from persistence (keys may arrive later: upgrade_origin()).
uint8_t MeshEngine::Impl::classify_stored(const PacketView& pv) {
  if (pv.h.origin == me) return kTrustVerified;
  if (pv.h.kind == Kind::Announce) return kTrustVerified;  // verified by restore_packet
  auto it = peers.find(pv.h.origin);
  bool keys = it != peers.end() && it->second.info.has_keys;
  if (pv.h.sec == Sec::Signed)
    return keys && verify_signed(pv, it->second.info.keys.sign_pk) ? kTrustVerified : kTrustUnknown;
  if (pv.h.kind == Kind::Ack) return ack_verifiable(pv) ? kTrustVerified : kTrustUnknown;
  return keys ? kTrustKnown : kTrustUnknown;
}

// The origin's key just became known: verify what we hold from it; drop forgeries.
void MeshEngine::Impl::upgrade_origin(const NodeId& origin) {
  auto it = peers.find(origin);
  if (it == peers.end() || !it->second.info.has_keys) return;
  std::vector<std::pair<uint64_t, uint8_t>> change;
  std::vector<uint64_t> drop;
  for (const auto& kv : store.all()) {
    const StoreEntry& e = kv.second;
    if (e.h.origin != origin || e.trust == kTrustVerified) continue;
    if (e.h.sec == Sec::Signed) {
      PacketView pv;
      if (parse_packet(e.raw, pv) && verify_signed(pv, it->second.info.keys.sign_pk)) change.push_back({e.wire, kTrustVerified});
      else drop.push_back(e.wire);
    } else if (e.h.kind != Kind::Ack) {
      change.push_back({e.wire, kTrustKnown});
    }
  }
  for (auto& c : change) {
    store.set_trust(c.first, c.second);
    if (per) {
      if (const StoreEntry* e = store.get(c.first)) per->store_put(c.first, e->raw, StoredMeta{e->received_ms, e->expires_ms, e->flags});
    }
  }
  for (uint64_t w : drop) {
    st.dropped_auth++;
    store_remove(w);
  }
}

namespace {
bool fast_link(const LinkCaps& c) { return c.bitrate_bps >= 100000; }
}  // namespace

// Admission control (docs/PROTOCOL.md §11). Bundles are local, user-initiated input: only the
// storage share applies to them, not the rate budgets.
bool MeshEngine::Impl::admit(uint8_t trust, uint32_t link_id, uint32_t hop_src, bool from_bundle) {
  if (trust == kTrustVerified || from_bundle) return true;
  auto it = links.find(link_id);
  if (it == links.end()) return trust != kTrustUnknown;
  LinkSlot& L = it->second;
  bool fast = fast_link(L.caps);
  if (trust == kTrustKnown) return L.known_b.take(host_now, fast ? 600 : 120, fast ? 1200 : 240);
  auto nit = L.nbrs.find(hop_src);
  if (nit != L.nbrs.end() && !nit->second.unknown_b.take(host_now, fast ? 30 : 10, fast ? 30 : 10)) return false;
  if (!L.unknown_b.take(host_now, fast ? 60 : 20, fast ? 60 : 20)) return false;
  return g_unknown.take(host_now, 120, 120);
}

bool MeshEngine::Impl::admit_new_identity(uint32_t link_id, bool from_bundle) {
  if (from_bundle) return true;
  auto it = links.find(link_id);
  if (it != links.end() && !it->second.newid_b.take(host_now, 30, 100)) return false;
  return g_newid.take(host_now, 60, 200);
}

bool MeshEngine::Impl::admit_unverified_sos(uint32_t link_id) {
  auto it = links.find(link_id);
  if (it != links.end() && !it->second.sos_unv_b.take(host_now, 6, 6)) return false;
  return g_sos_unv.take(host_now, 12, 12);
}

void MeshEngine::Impl::store_remove(uint64_t wire) {
  if (store.remove(wire) && per) per->store_remove(wire);
}

void MeshEngine::Impl::gc(uint64_t now) {
  uint64_t mnow = mesh_now();
  std::vector<uint64_t> exp;
  store.expired(host_now, exp);
  for (uint64_t w : exp) store_remove(w);
  // Outgoing messages that expired without delivery.
  for (auto& kv : out) {
    OutMsg& o = kv.second;
    if (o.expires_ms && o.expires_ms <= mnow && status_rank(o.status) < status_rank(MsgStatus::Delivered) &&
        o.status != MsgStatus::Failed && o.reliable) {
      set_status(kv.first, MsgStatus::Failed);
    }
  }
  for (size_t i = 0; i < await_key.size();) {
    if (await_key[i].expires_ms <= mnow) {
      MsgId m = await_key[i].id;
      await_key.erase(await_key.begin() + static_cast<std::ptrdiff_t>(i));
      set_status(m, MsgStatus::Failed);
    } else {
      send_whois(await_key[i].to);  // rate limited internally
      ++i;
    }
  }
  expire_pending(now);
  for (auto it = files.begin(); it != files.end();) {
    if (now - it->second.last_ms > 30 * 60000ull) it = files.erase(it);
    else ++it;
  }
  for (auto it = pings.begin(); it != pings.end();) {
    if (now - it->second.second > 120000) it = pings.erase(it);
    else ++it;
  }
  for (auto it = rate.begin(); it != rate.end();) {
    if (now - it->second.second > 10 * 60000ull) it = rate.erase(it);
    else ++it;
  }
  reack_ms.erase_if([&](uint64_t t) { return now - t > 10 * 60000ull; });
  {
    std::vector<MsgId> gone;
    delivered.prune(mnow, &gone);
    if (per)
      for (const MsgId& m : gone) per->replay_remove(m);
  }
  for (auto& kv : links) {
    LinkSlot& L = kv.second;
    for (auto it = L.nbrs.begin(); it != L.nbrs.end();) {
      if (now - it->second.last_rx_ms > 30 * 60000ull) it = L.nbrs.erase(it);
      else ++it;
    }
  }
  for (auto& kv : peers) {
    PeerRec& p = kv.second;
    bool direct = false;
    for (auto& pl : p.info.links)
      if (mnow - pl.last_rx_ms < 15 * 60000ull) direct = true;
    p.info.direct = direct;
  }
}

// =============================================================================================
// Public API
// =============================================================================================

MeshEngine::MeshEngine(const Identity& identity, ByteView entropy, const MeshConfig& config)
    : d_(new Impl(identity, entropy, config)) {}
MeshEngine::~MeshEngine() = default;

void MeshEngine::set_delegate(MeshDelegate* d) { d_->del = d; }
void MeshEngine::set_persistence(MeshPersistence* p) { d_->per = p; }
void MeshEngine::set_log(const Log& log) { d_->log = log; }
void MeshEngine::add_entropy(ByteView e) { d_->rng.seed(e); }
const MeshConfig& MeshEngine::config() const { return d_->cfg; }
void MeshEngine::set_config(const MeshConfig& c) {
  d_->cfg = c;
  d_->seen.set_capacity(c.dedup_capacity);
  d_->store.set_limits(c.store_max_bytes, c.store_max_packets);
}
const Identity& MeshEngine::identity() const { return d_->id; }
NodeId MeshEngine::node_id() const { return d_->me; }

void MeshEngine::set_profile(std::string_view name, std::string_view callsign, std::string_view status) {
  d_->name = std::string(utf8_truncate(name, kMaxName));
  d_->callsign = valid_callsign(callsign) ? std::string(callsign) : std::string();
  d_->status_text = std::string(utf8_truncate(status, kMaxStatus));
  if (d_->started) d_->next_announce_ms = d_->host_now;  // announce on next tick
}
const std::string& MeshEngine::callsign() const { return d_->callsign; }

void MeshEngine::tick(uint64_t now) {
  Impl& d = *d_;
  if (!d.started) {
    d.started = true;
    d.host_now = now;
    d.host_raw = now;
    d.start_host_ms = now;
    if (now < kClockSaneMs) d.tquality = 15;
    d.next_announce_ms = now + 200 + d.rng.uniform(1500);  // refined below once links are known
    d.next_gc_ms = now + kGcIntervalMs;
  }
  // host_now (all timers) only ever moves forward, by the host clock's forward steps. A backward
  // step of the host clock is applied exactly once: mesh time follows it when it comes from the
  // system clock; GPS / manual / mesh time ignores it. now == 0 after start means "host has no
  // clock reading": nothing moves.
  if (now != 0 || d.host_raw == 0) {
    if (now >= d.host_raw) {
      d.host_now += now - d.host_raw;
    } else if (d.tsource == TimeSource::System) {
      uint64_t back = d.host_raw - now;
      d.offset_ms -= back > static_cast<uint64_t>(INT64_MAX / 2) ? INT64_MAX / 2 : static_cast<int64_t>(back);
      d.dirty();
    }
    d.host_raw = now;
  }
  now = d.host_now;
  // A saved / adjusted offset that puts a sane host clock outside 2024..2100 is corrupt state
  // (security review M1): fall back to the host clock.
  if (d.host_raw >= kClockSaneMs && (d.mesh_now() < kMinSaneMs || d.mesh_now() > kMaxSaneMs)) {
    d.offset_ms = 0;
    d.tsource = TimeSource::System;
    d.tquality = 8;
    d.dirty();
  }

  for (auto& kv : d.links) {
    LinkSlot& L = kv.second;
    if (!L.link) continue;
    L.caps = L.link->caps();
    L.state = L.link->state();
    L.link->tick(now);
  }
  if (!d.first_announce_scheduled && !d.links.empty()) {
    // Spread start-up announcements (a whole camp powering up together must not collide):
    // up to ~25 frame airtimes on the slowest link.
    d.first_announce_scheduled = true;
    uint32_t worst = 0;
    for (auto& kv : d.links) worst = std::max(worst, d.airtime_ms(kv.second, 200));
    d.next_announce_ms = now + 200 + d.rng.uniform(1500 + 25 * worst);
  }
  if (now >= d.next_announce_ms) {
    // On a slow shared channel, skip it if we announced moments ago (piggy-backed on a message,
    // or answering a WHOIS): airtime there is precious.
    bool slow = false;
    for (auto& kv : d.links) slow |= d.contended(kv.second);
    if (!slow || !d.last_announce_ms || now - d.last_announce_ms >= 60000) d.announce();
    uint32_t iv = d.cfg.announce_interval_s;
    d.next_announce_ms = iv ? now + uint64_t(iv) * 1000 + d.rng.uniform(iv * 100 + 1) : UINT64_MAX;
  }
  if (d.sos_on && now >= d.sos_next) {
    SosBody s;
    s.active = true;
    s.seq = ++d.sos_seq;
    s.pos = d.sos_pos;
    s.text = d.sos_text;
    SendOptions o;
    o.prio = Prio::Emergency;
    o.ttl_hops = kMaxTtl + 1;
    d.originate(Kind::Sos, Dest::broadcast(), encode(s), o, 0);
    d.sos_count++;
    d.sos_next = now + d.sos_interval_ms;
  }
  if (d.netplan_interval_ms && !d.netplan.entries.empty() && now >= d.netplan_next) {
    SendOptions o;
    d.originate(Kind::NetPlan, Dest::broadcast(), encode(d.netplan), o, 0);
    d.netplan_next = now + d.netplan_interval_ms;
  }
  // End-to-end retries.
  for (auto& kv : d.out) {
    OutMsg& o = kv.second;
    if (!o.reliable || !o.wire || status_rank(o.status) >= status_rank(MsgStatus::Delivered) ||
        o.status == MsgStatus::Failed)
      continue;
    if (now < o.next_retry_ms || o.attempts >= d.cfg.ack_max_retries) continue;
    o.attempts++;
    o.interval_ms = std::min<uint32_t>(o.interval_ms ? o.interval_ms * 2 : d.cfg.ack_retry_base_ms, d.cfg.ack_retry_max_ms);
    o.next_retry_ms = now + o.interval_ms + d.rng.uniform(o.interval_ms / 4 + 1);
    if (const StoreEntry* e = d.store.get(o.wire)) {
      d.st.retries++;
      d.flood_own(*e);
      for (auto& lk : d.links) lk.second.beacon_soon = true;
    }
  }
  if (now >= d.next_gc_ms) {
    d.gc(now);
    d.next_gc_ms = now + kGcIntervalMs;
  }
  d.run_floods(now);
  for (auto& kv : d.links) {
    LinkSlot& L = kv.second;
    if (!L.link || L.state != LinkState::Up) continue;
    d.beacon_timers(L, now);
    d.reasm_timers(L, now);
    d.repair_timers(L, now);
    d.discovery_timers(L, now);
    d.pump(L, now);
  }
}

uint64_t MeshEngine::now_ms() const { return d_->mesh_now(); }

TimeInfo MeshEngine::time_info() const {
  TimeInfo t;
  t.mesh_ms = d_->mesh_now();
  t.source = d_->tsource;
  t.quality = d_->tquality;
  return t;
}

void MeshEngine::set_time(uint64_t unix_ms, TimeSource source) {
  Impl& d = *d_;
  if (unix_ms > kMaxSaneMs) unix_ms = kMaxSaneMs;
  d.offset_ms = static_cast<int64_t>(unix_ms) - static_cast<int64_t>(d.host_now);
  d.tsamples.clear();
  d.last_time_adjust = 0;
  d.tsource = source;
  switch (source) {
    case TimeSource::Gps: case TimeSource::Manual:
      d.tquality = 0;
      if (d.started) d.next_announce_ms = std::min(d.next_announce_ms, d.host_now + 1000);  // share the fix
      break;
    case TimeSource::System: d.tquality = unix_ms < kClockSaneMs ? 15 : 8; break;
    case TimeSource::Mesh: d.tquality = 9; break;
  }
  if (d.del) d.del->on_time(d.mesh_now(), source);
  d.dirty();
}

uint32_t MeshEngine::next_wakeup_ms() const {
  const Impl& d = *d_;
  uint64_t now = d.host_now;
  uint64_t next = now + 60000;
  auto consider = [&](uint64_t t) { if (t < next) next = t; };
  consider(d.next_announce_ms);
  consider(d.next_gc_ms);
  if (d.sos_on) consider(d.sos_next);
  if (d.netplan_interval_ms && !d.netplan.entries.empty()) consider(d.netplan_next);
  // Only timers tick() actually acts on: a timer that is due but cannot fire would otherwise make
  // the host spin (battery).
  for (const auto& kv : d.out)
    if (kv.second.reliable && kv.second.wire && status_rank(kv.second.status) < status_rank(MsgStatus::Delivered) &&
        kv.second.status != MsgStatus::Failed && kv.second.attempts < d.cfg.ack_max_retries)
      consider(kv.second.next_retry_ms);
  for (const auto& kv : d.links) {
    const LinkSlot& L = kv.second;
    bool up = L.link && L.state == LinkState::Up && L.caps.mtu >= proto::kMinMtu &&
              !(L.caps.requires_station_id && d.callsign.empty());
    if (L.qframes && up) consider(std::max(L.busy_until, now + 10));
    for (const auto& r : L.repairs)
      if (r.due_ms) consider(r.due_ms);
    if (L.disc_due_ms) consider(L.disc_due_ms);
    for (const auto& f : L.floods) consider(f.due_ms);
    if (!up || !d.cfg.beacons_enabled) continue;
    if (L.next_beacon_ms) consider(L.next_beacon_ms);
    else consider(now + 300);  // first beacon gets scheduled by the next tick
    if (L.beacon_soon && !L.nbrs.empty()) {
      uint64_t at = L.last_beacon_ms + beacon_gap(L.caps);
      if (L.own_new_ms > L.last_beacon_ms)
        at = std::min(at, std::max(std::max(L.own_new_ms, L.last_tx_end_ms) + 2000,
                                   L.last_beacon_ms + std::min<uint64_t>(beacon_gap(L.caps), 15000)));
      consider(at);
    }
    if (!L.reasm.empty()) consider(now + 500);
  }
  if (next <= now) return 0;
  uint64_t dt = next - now;
  return dt > 60000 ? 60000 : static_cast<uint32_t>(dt);
}

bool MeshEngine::attach_link(uint32_t link_id, Link* link) {
  if (!link_id || !link || d_->links.count(link_id)) return false;
  LinkSlot& L = d_->links[link_id];
  L.id = link_id;
  L.link = link;
  L.caps = link->caps();
  L.state = link->state();
  L.st.link_id = link_id;
  L.tokens_ms = 1e18;  // start full; clamped to capacity at first refill
  L.token_ms = d_->host_now;
  L.next_frag_id = static_cast<uint16_t>(d_->rng.u32() | 1);
  L.beacon_soon = true;
  return true;
}

void MeshEngine::detach_link(uint32_t link_id) { d_->links.erase(link_id); }

void MeshEngine::on_frame(uint32_t link_id, ByteView frame, const RxMeta& meta) { d_->on_frame(link_id, frame, meta); }

bool MeshEngine::add_known_keys(const PublicKeys& keys, std::string_view name, std::string_view cs) {
  Impl& d = *d_;
  NodeId n = keys.node_id();
  if (n == d.me) return false;
  bool is_new = false;
  PeerRec& p = d.peer_rec(n, &is_new);
  if (p.info.has_keys && p.info.keys != keys) return false;
  p.info.has_keys = true;
  p.info.keys = keys;
  p.info.pinned = true;
  if (!name.empty() && p.info.name.empty()) p.info.name = std::string(utf8_truncate(name, kMaxName));
  if (!cs.empty() && p.info.callsign.empty() && valid_callsign(cs)) p.info.callsign = std::string(cs);
  d.notify_peer(p, is_new);
  d.upgrade_origin(n);
  d.flush_await_key(n);
  d.process_pending(n);
  return true;
}

bool MeshEngine::has_keys(const NodeId& n) const {
  auto it = d_->peers.find(n);
  return it != d_->peers.end() && it->second.info.has_keys;
}

void MeshEngine::forget_peer(const NodeId& n) { d_->peers.erase(n); }

void MeshEngine::join_channel(const ChannelKey& ch) { d_->channels[ch.id] = ch; }
void MeshEngine::leave_channel(uint32_t id) { d_->channels.erase(id); }
bool MeshEngine::in_channel(uint32_t id) const { return d_->channels.count(id) != 0; }

SendResult MeshEngine::send_text(const Dest& to, std::string_view text, const SendOptions& o,
                                 const std::optional<MsgId>& reply_to) {
  if (text.empty() || text.size() > kMaxText) { SendResult r; r.error = text.empty() ? "bad_params" : "too_large"; return r; }
  TextBody b;
  b.text = utf8_sanitize(text);
  b.reply_to = reply_to;
  return d_->originate(Kind::Text, to, encode(b), o, o.lifetime_s);
}

SendResult MeshEngine::send_position(const Dest& to, const Position& pos, const SendOptions& o) {
  if (!pos.valid()) { SendResult r; r.error = "bad_params"; return r; }
  SendOptions oo = o;
  oo.want_ack = false;
  return d_->originate(Kind::Position, to, encode(PositionBody{pos}), oo, o.lifetime_s);
}

SendResult MeshEngine::sos_start(std::string_view text, const std::optional<Position>& pos, uint32_t interval_s) {
  Impl& d = *d_;
  if (!d.sos_on) {
    d.sos_since = d.mesh_now();
    d.sos_count = 0;
  }
  d.sos_on = true;
  d.sos_text = std::string(utf8_truncate(utf8_sanitize(text), kMaxNote));
  d.sos_pos = pos;
  d.sos_interval_ms = (interval_s < 10 ? 10 : interval_s) * 1000;
  SosBody s;
  s.active = true;
  s.seq = ++d.sos_seq;
  s.pos = pos;
  s.text = d.sos_text;
  SendOptions o;
  o.prio = Prio::Emergency;
  o.ttl_hops = kMaxTtl + 1;
  SendResult r = d.originate(Kind::Sos, Dest::broadcast(), encode(s), o, 0);
  d.sos_count++;
  d.sos_next = d.host_now + d.sos_interval_ms;
  d.dirty();
  return r;
}

SendResult MeshEngine::sos_stop() {
  Impl& d = *d_;
  SendResult r;
  if (!d.sos_on) { r.ok = true; return r; }
  d.sos_on = false;
  SosBody s;
  s.active = false;
  s.seq = ++d.sos_seq;
  s.text = "";
  SendOptions o;
  o.prio = Prio::Emergency;
  o.ttl_hops = kMaxTtl + 1;
  r = d.originate(Kind::Sos, Dest::broadcast(), encode(s), o, 0);
  d.dirty();
  return r;
}

bool MeshEngine::sos_active() const { return d_->sos_on; }
uint64_t MeshEngine::sos_since_ms() const { return d_->sos_since; }
uint32_t MeshEngine::sos_sent() const { return d_->sos_count; }

SendResult MeshEngine::sos_ack(const NodeId& origin, std::string_view text, uint32_t sos_pkt_id) {
  SosAckBody b;
  b.sos_pkt_id = sos_pkt_id;
  b.text = utf8_sanitize(text);
  SendOptions o;
  o.prio = Prio::Emergency;
  o.want_ack = false;
  return d_->originate(Kind::SosAck, Dest::to_node(origin), encode(b), o, 0);
}

SendResult MeshEngine::send_checkin(const CheckinBody& body, const SendOptions& o) {
  CheckinBody b = body;
  b.note = utf8_sanitize(b.note);
  SendOptions oo = o;
  if (b.status == CheckinStatus::NeedHelp || b.status == CheckinStatus::Injured) oo.prio = Prio::High;
  return d_->originate(Kind::Checkin, Dest::broadcast(), encode(b), oo, o.lifetime_s);
}

SendResult MeshEngine::post_bulletin(const BulletinBody& body, uint32_t lifetime_s) {
  BulletinBody b = body;
  b.is_delete = false;
  b.title = utf8_sanitize(b.title);
  b.text = utf8_sanitize(b.text);
  SendOptions o;
  o.prio = b.category == BulletinCategory::Hazard ? Prio::High : Prio::Normal;
  return d_->originate(Kind::Bulletin, Dest::broadcast(), encode(b), o, lifetime_s);
}

SendResult MeshEngine::delete_bulletin(uint32_t pkt_id) {
  Impl& d = *d_;
  uint32_t life = d.cfg.default_lifetime_s;
  for (uint64_t w : d.store.find_msg(MsgId{d.me, pkt_id})) {
    if (const StoreEntry* e = d.store.get(w)) {
      uint64_t now = d.mesh_now();
      if (e->expires_ms > now) life = static_cast<uint32_t>((e->expires_ms - now) / 1000 + 60);
    }
    d.store_remove(w);
  }
  BulletinBody b;
  b.is_delete = true;
  b.delete_pkt_id = pkt_id;
  SendOptions o;
  return d.originate(Kind::Bulletin, Dest::broadcast(), encode(b), o, life);
}

SendResult MeshEngine::send_file(const Dest& to, std::string_view name, std::string_view mime, ByteView data,
                                 const SendOptions& o) {
  Impl& d = *d_;
  SendResult r;
  size_t chunk = d.cfg.file_chunk_bytes < 64 ? 64 : (d.cfg.file_chunk_bytes > 3000 ? 3000 : d.cfg.file_chunk_bytes);
  if (data.size() > d.cfg.max_file_bytes) { r.error = "too_large"; return r; }
  size_t count = data.empty() ? 1 : (data.size() + chunk - 1) / chunk;
  if (count > kMaxFileChunks) { r.error = "too_large"; return r; }
  uint32_t file_id = d.new_pkt_id();
  std::array<uint8_t, 16> hash;
  crypto::blake2b(hash.data(), 16, data);
  for (size_t i = 0; i < count; ++i) {
    FileBody f;
    f.file_id = file_id;
    f.index = static_cast<uint16_t>(i);
    f.count = static_cast<uint16_t>(count);
    if (i == 0) {
      f.total_size = static_cast<uint32_t>(data.size());
      f.name = std::string(utf8_truncate(utf8_sanitize(name), kMaxShortField));
      f.mime = std::string(utf8_truncate(mime, kMaxShortField));
      f.hash = hash;
    }
    f.data = data.sub(i * chunk, chunk).to_bytes();
    SendOptions co = o;
    co.want_ack = o.want_ack && i == 0;
    if (co.prio > Prio::Normal) co.prio = Prio::Normal;
    uint32_t pid = file_id + static_cast<uint32_t>(i);
    if (pid == 0) pid = 1;
    SendResult cr = d.originate(Kind::File, to, encode(f), co, o.lifetime_s, pid);
    if (i == 0) r = cr;
    if (!cr.ok) return cr;
  }
  return r;
}

SendResult MeshEngine::set_netplan(const NetPlanBody& plan, uint32_t interval_s) {
  Impl& d = *d_;
  d.netplan = plan;
  d.netplan_interval_ms = interval_s * 1000;
  d.netplan_next = d.host_now + d.netplan_interval_ms;
  SendResult r;
  if (plan.entries.empty()) { d.netplan_interval_ms = 0; r.ok = true; return r; }
  SendOptions o;
  return d.originate(Kind::NetPlan, Dest::broadcast(), encode(plan), o, 0);
}

SendResult MeshEngine::send_read_receipt(const NodeId& to, const std::vector<uint32_t>& ids) {
  if (ids.empty()) { SendResult r; r.error = "bad_params"; return r; }
  ReadBody b;
  b.pkt_ids = ids;
  SendOptions o;
  o.want_ack = false;
  return d_->originate(Kind::Read, Dest::to_node(to), encode(b), o, 0);
}

SendResult MeshEngine::send_app(const Dest& to, uint16_t app_type, ByteView data, const SendOptions& o) {
  AppBody b;
  b.app_type = app_type;
  b.data = data.to_bytes();
  return d_->originate(Kind::App, to, encode(b), o, o.lifetime_s);
}

bool MeshEngine::ping(const NodeId& node) {
  Impl& d = *d_;
  uint32_t nonce = d.rng.u32();
  SendOptions o;
  o.prio = Prio::High;
  o.want_ack = false;
  SendResult r = d.originate(Kind::Ping, Dest::to_node(node), encode(PingBody{nonce}), o, 600);
  if (!r.ok) return false;
  if (d.pings.size() > 64) d.pings.erase(d.pings.begin());
  d.pings[nonce] = {node, d.host_now};
  return true;
}

void MeshEngine::announce_now() { d_->announce(); }

bool MeshEngine::retry(const MsgId& m) {
  Impl& d = *d_;
  auto it = d.out.find(m);
  if (it == d.out.end()) return false;
  OutMsg& o = it->second;
  const StoreEntry* e = o.wire ? d.store.get(o.wire) : nullptr;
  if (!e) return false;
  if (o.status == MsgStatus::Failed) o.status = MsgStatus::Sent;
  o.attempts = 0;
  o.interval_ms = d.cfg.ack_retry_base_ms;
  o.next_retry_ms = d.host_now + o.interval_ms;
  d.flood_own(*e);
  for (auto& lk : d.links) lk.second.beacon_soon = true;
  return true;
}

MsgStatus MeshEngine::status(const MsgId& m) const {
  auto it = d_->out.find(m);
  return it == d_->out.end() ? MsgStatus::Unknown : it->second.status;
}

bool MeshEngine::cancel(const MsgId& m) {
  Impl& d = *d_;
  auto it = d.out.find(m);
  if (it == d.out.end()) return false;
  if (it->second.wire) d.store_remove(it->second.wire);
  it->second.reliable = false;
  for (size_t i = 0; i < d.await_key.size(); ++i)
    if (d.await_key[i].id == m) { d.await_key.erase(d.await_key.begin() + static_cast<std::ptrdiff_t>(i)); break; }
  return true;
}

std::vector<PeerInfo> MeshEngine::peers() const {
  std::vector<PeerInfo> v;
  v.reserve(d_->peers.size());
  for (const auto& kv : d_->peers) v.push_back(kv.second.info);
  std::sort(v.begin(), v.end(), [](const PeerInfo& a, const PeerInfo& b) { return a.last_seen_ms > b.last_seen_ms; });
  return v;
}

std::optional<PeerInfo> MeshEngine::peer(const NodeId& n) const {
  auto it = d_->peers.find(n);
  if (it == d_->peers.end()) return std::nullopt;
  return it->second.info;
}

MeshStats MeshEngine::stats() const {
  const Impl& d = *d_;
  MeshStats s = d.st;
  s.stored_messages = static_cast<uint32_t>(d.store.count());
  s.stored_bytes = d.store.bytes();
  s.uptime_ms = d.host_now - d.start_host_ms;
  for (const auto& kv : d.links) {
    LinkStats ls = kv.second.st;
    ls.queue = static_cast<uint32_t>(kv.second.qframes);
    ls.queue_bytes = static_cast<uint32_t>(kv.second.qbytes);
    ls.neighbours = static_cast<uint32_t>(kv.second.nbrs.size());
    uint64_t sum = 0;
    for (uint64_t b : kv.second.air_bucket) sum += b;
    ls.duty_used_permille = static_cast<uint32_t>(sum * 1000 / 600000);
    s.per_link.push_back(ls);
  }
  return s;
}

std::vector<uint32_t> MeshEngine::link_ids() const {
  std::vector<uint32_t> v;
  for (const auto& kv : d_->links) v.push_back(kv.first);
  return v;
}

Bytes MeshEngine::export_bundle(const BundleFilter& f, uint32_t* count) const {
  const Impl& d = *d_;
  std::vector<const StoreEntry*> sel;
  for (const auto& kv : d.store.all()) {
    const StoreEntry& e = kv.second;
    if (e.expires_local <= d.host_now) continue;
    if (f.since_ms && wire_to_unix_ms(e.h.created) < f.since_ms) continue;
    if (f.scope == BundleFilter::ToDest) {
      Dest ed;
      ed.type = static_cast<Dest::Type>(e.h.dst_type);
      ed.node = e.h.dst_node;
      ed.channel = e.h.channel;
      // A DM conversation includes both directions.
      bool dm_back = f.dest.type == Dest::Node && e.h.dst_type == DestType::Node && e.h.origin == f.dest.node &&
                     e.h.dst_node == d.me;
      if (!(ed == f.dest) && !dm_back) continue;
    } else if (f.scope == BundleFilter::ForNode) {
      bool ok = e.h.dst_type == DestType::Broadcast || (e.h.dst_type == DestType::Node && e.h.dst_node == f.node) ||
                e.h.kind == Kind::Announce || e.h.kind == Kind::Ack;
      if (!ok) continue;
    }
    sel.push_back(&e);
  }
  // Most valuable first: emergency, then oldest so a size cap keeps conversations coherent.
  std::sort(sel.begin(), sel.end(), [](const StoreEntry* a, const StoreEntry* b) {
    bool ea = a->h.prio == Prio::Emergency || a->h.kind == Kind::Sos;
    bool eb = b->h.prio == Prio::Emergency || b->h.kind == Kind::Sos;
    if (ea != eb) return ea;
    if (a->h.created != b->h.created) return a->h.created < b->h.created;
    return a->wire < b->wire;
  });
  std::vector<Bytes> pkts;
  size_t total = 16;
  for (const StoreEntry* e : sel) {
    if (f.max_bytes && total + e->raw.size() + 3 > f.max_bytes) continue;
    total += e->raw.size() + 3;
    pkts.push_back(e->raw);
  }
  if (count) *count = static_cast<uint32_t>(pkts.size());
  return encode_bundle(pkts);
}

BundleImportResult MeshEngine::import_bundle(ByteView data) {
  Impl& d = *d_;
  BundleImportResult r;
  std::vector<ByteView> pkts;
  if (!decode_bundle(data, pkts)) return r;
  r.ok = true;
  for (ByteView p : pkts) {
    PacketView pv;
    bool parsed = parse_packet(p, pv);
    int res = d.handle_packet(p, 0, RxMeta(), true, false);
    if (res > 0) {
      r.imported++;
      if (parsed && pv.h.dst_type == DestType::Node && pv.h.dst_node == d.me) r.for_me++;
    } else if (res == 0) {
      r.duplicates++;
    } else {
      r.rejected++;
    }
  }
  return r;
}

bool MeshEngine::restore_packet(ByteView packet, const StoredMeta& meta) {
  Impl& d = *d_;
  PacketView pv;
  if (!parse_packet(packet, pv)) return false;
  uint64_t wire = wire_id(packet);
  if (d.seen.insert(wire)) d.seen_sids[sync_id(wire)]++;
  if (pv.h.origin == d.me) d.delivered.put(MsgId{pv.h.origin, pv.h.pkt_id}, meta.expires_ms + 86400000ull, true, nullptr);
  // Rebuild key knowledge from stored announcements and ack knowledge from stored acks.
  if (pv.h.kind == Kind::Announce) {
    bool ok = false;
    MeshPersistence* saved = d.per;
    d.per = nullptr;
    d.handle_announce(pv, 0, RxMeta(), &ok);
    d.per = saved;
    if (!ok) return false;
  } else if (pv.h.kind == Kind::Ack) {
    d.handle_ack_packet(pv);
  }
  MeshPersistence* saved = d.per;
  d.per = nullptr;  // do not echo back to the persistence layer
  uint8_t flags = static_cast<uint8_t>(meta.flags & (kStoredOwn | kStoredTransmittable));
  bool ok = d.store_put(pv, packet.to_bytes(), meta.received_ms, meta.expires_ms, flags, d.classify_stored(pv));
  d.per = saved;
  return ok;
}

void MeshEngine::restore_replay(const MsgId& m, uint64_t keep_until_ms) {
  d_->delivered.put(m, keep_until_ms, true, nullptr);
}

Json MeshEngine::save_state() const {
  const Impl& d = *d_;
  Json j = Json::object();
  j.set("v", 1);
  Json t = Json::object();
  t.set("offset_ms", static_cast<long long>(d.offset_ms));
  t.set("source", time_source_name(d.tsource));
  t.set("quality", d.tquality);
  j.set("time", std::move(t));
  Json outs = Json::array();
  for (const auto& kv : d.out) {
    const OutMsg& o = kv.second;
    if (!o.reliable && status_rank(o.status) >= status_rank(MsgStatus::Delivered)) continue;
    Json e = Json::object();
    e.set("id", kv.first.to_hex());
    uint8_t wb[8];
    for (int i = 0; i < 8; ++i) wb[i] = static_cast<uint8_t>(o.wire >> (56 - 8 * i));
    e.set("wire", to_hex(ByteView(wb, 8)));
    e.set("dest", node_id_hex(o.dest));
    e.set("reliable", o.reliable);
    e.set("secret", to_hex(ByteView(o.secret.data(), 8)));
    e.set("attempts", o.attempts);
    e.set("status", msg_status_name(o.status));
    e.set("expires_ms", static_cast<long long>(o.expires_ms));
    outs.push(std::move(e));
  }
  j.set("out", std::move(outs));
  Json sos = Json::object();
  sos.set("on", d.sos_on).set("since", static_cast<long long>(d.sos_since)).set("seq", d.sos_seq);
  sos.set("text", d.sos_text).set("interval_ms", d.sos_interval_ms).set("count", d.sos_count);
  if (d.sos_pos) sos.set("lat_e7", d.sos_pos->lat_e7).set("lon_e7", d.sos_pos->lon_e7);
  j.set("sos", std::move(sos));
  return j;
}

bool MeshEngine::load_state(const Json& j) {
  Impl& d = *d_;
  if (!j.is_object() || j.get_int("v") != 1) return false;
  const Json& t = j.get("time");
  // Hostile or corrupt state must not overflow later arithmetic (security review M1).
  int64_t off = t.get_int("offset_ms", 0);
  const int64_t kLim = static_cast<int64_t>(kMaxSaneMs);
  d.offset_ms = off < -kLim ? -kLim : (off > kLim ? kLim : off);
  std::string src = t.get_string("source", "system");
  d.tsource = src == "gps" ? TimeSource::Gps : src == "mesh" ? TimeSource::Mesh : src == "manual" ? TimeSource::Manual : TimeSource::System;
  int64_t q = t.get_int("quality", 8);
  d.tquality = static_cast<uint8_t>(q < 0 ? 0 : (q > 15 ? 15 : q));
  // Restored clocks are less trustworthy than fresh fixes.
  if (d.tquality < 15) d.tquality = static_cast<uint8_t>(d.tquality + 1);
  for (const Json& e : j.get("out").items()) {
    MsgId m;
    if (!MsgId::parse(e.get_string("id"), m)) continue;
    OutMsg o;
    Bytes w;
    if (from_hex(e.get_string("wire"), w) && w.size() == 8)
      for (uint8_t b : w) o.wire = o.wire << 8 | b;
    parse_node_id(e.get_string("dest"), o.dest);
    o.reliable = e.get_bool("reliable");
    Bytes s;
    if (from_hex(e.get_string("secret"), s) && s.size() == 8) std::memcpy(o.secret.data(), s.data(), 8);
    o.attempts = static_cast<uint8_t>(std::min<int64_t>(e.get_int("attempts"), 255));
    std::string st = e.get_string("status");
    o.status = st == "sent" ? MsgStatus::Sent : st == "relayed" ? MsgStatus::Relayed : st == "delivered" ? MsgStatus::Delivered
             : st == "failed" ? MsgStatus::Failed : MsgStatus::Queued;
    o.expires_ms = static_cast<uint64_t>(e.get_int("expires_ms"));
    o.interval_ms = d.cfg.ack_retry_base_ms;
    o.next_retry_ms = d.host_now + d.cfg.ack_retry_base_ms;
    d.track_out(m, o);
  }
  const Json& sos = j.get("sos");
  d.sos_on = sos.get_bool("on");
  d.sos_since = static_cast<uint64_t>(sos.get_int("since"));
  d.sos_seq = static_cast<uint16_t>(sos.get_int("seq"));
  d.sos_text = sos.get_string("text");
  d.sos_interval_ms = static_cast<uint32_t>(std::max<int64_t>(10000, sos.get_int("interval_ms", 60000)));
  d.sos_count = static_cast<uint32_t>(sos.get_int("count"));
  if (sos.has("lat_e7")) {
    Position p;
    p.lat_e7 = static_cast<int32_t>(sos.get_int("lat_e7"));
    p.lon_e7 = static_cast<int32_t>(sos.get_int("lon_e7"));
    if (p.valid()) d.sos_pos = p;
  }
  d.sos_next = d.host_now;
  return true;
}

}  // namespace ember::mesh
