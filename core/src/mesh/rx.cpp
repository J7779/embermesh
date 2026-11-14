// Mesh engine: receive path — frames, reassembly, packet acceptance, authentication, delivery.
#include <algorithm>
#include <cstring>

#include "mesh/impl.h"

namespace ember::mesh {

using namespace proto;

namespace {
constexpr size_t kMaxNeighboursPerLink = 64;
constexpr size_t kMaxReasmPerLink = 16;
constexpr size_t kMaxReasmBytesPerLink = 128 * 1024;
constexpr size_t kMaxPending = 64;
constexpr uint32_t kMaxFramesPerSecond = 400;
constexpr uint64_t kClockUnsetWire = 126230400;  // 2024-01-01 in wire seconds
constexpr uint64_t kPendingMs = 5 * 60000;
constexpr uint64_t kSkewToleranceMs = 86400000ull;
constexpr size_t kMaxFilesRx = 8;

uint64_t commit_u64(const AckCommit& c) {
  uint64_t v = 0;
  for (uint8_t x : c) v = v << 8 | x;
  return v;
}

float ewma(float old, float v, bool had) { return had ? old * 0.7f + v * 0.3f : v; }
}  // namespace

// ---------------------------------------------------------------------------------------------
void MeshEngine::Impl::on_frame(uint32_t link_id, ByteView frame, const RxMeta& meta) {
  auto it = links.find(link_id);
  if (it == links.end()) return;
  LinkSlot& L = it->second;
  st.rx_frames++;
  L.st.rx_frames++;
  L.st.rx_bytes += frame.size();
  // Per-link frame rate cap: a jammer or a broken driver cannot monopolise the CPU.
  if (host_now - L.rx_window_start >= 1000) { L.rx_window_start = host_now; L.rx_window_frames = 0; }
  if (++L.rx_window_frames > kMaxFramesPerSecond) { L.st.rx_bad++; st.rx_bad++; return; }
  FrameHeader fh;
  ByteView body;
  if (frame.size() > 65535 || !parse_frame(frame, fh, body)) { L.st.rx_bad++; st.rx_bad++; return; }
  if (fh.hop_src == my_hop) return;  // our own transmission echoed back (hub, loopback)
  L.st.last_rx_ms = mesh_now();
  L.heard_ms = host_now;  // channel was busy just now (listen-before-talk, tx.cpp holdoff_until)
  auto nit = L.nbrs.find(fh.hop_src);
  if (nit == L.nbrs.end()) {
    if (L.nbrs.size() >= kMaxNeighboursPerLink) {
      auto victim = L.nbrs.begin();
      for (auto i = L.nbrs.begin(); i != L.nbrs.end(); ++i)
        if (i->second.last_rx_ms < victim->second.last_rx_ms) victim = i;
      L.nbrs.erase(victim);
    }
    Neighbour n;
    n.hop = fh.hop_src;
    n.first_ms = host_now;
    nit = L.nbrs.emplace(fh.hop_src, n).first;
    L.beacon_soon = true;  // say hello so the newcomer learns our store digest
    L.new_nbr_ms = host_now;
    if (contended(L)) {
      // The newcomer may have started after our start-up announcement: answer once with our
      // announcement (keys, name) after a random delay, coalescing several newcomers.
      L.disc_for_ms = host_now;
      if (!L.disc_due_ms) {
        uint32_t a = airtime_ms(L, std::min<size_t>(L.caps.mtu, 128));
        L.disc_due_ms = host_now + a + rng.uniform(4 * a + 1);
      }
    }
  }
  Neighbour& n = nit->second;
  // Per transmitter share of the link budget (security review L6): one hostile client of a shared
  // link (hub) cannot take all of it under one hop id.
  if (host_now - n.win_start >= 1000) { n.win_start = host_now; n.win_frames = 0; }
  if (++n.win_frames > kMaxFramesPerSecond / 2) { L.st.rx_bad++; st.rx_bad++; return; }
  n.last_rx_ms = host_now;
  if (meta.has_rssi) { n.rssi = ewma(n.rssi, meta.rssi_dbm, n.has_rssi); n.has_rssi = true; }
  if (meta.has_snr) { n.snr = ewma(n.snr, meta.snr_db, n.has_snr); n.has_snr = true; }
  if (!fh.station.empty()) n.station = fh.station;
  switch (fh.type) {
    case FrameType::Packet: {
      int r = handle_packet(body, link_id, meta, false, false, fh.hop_src);
      if (r < 0) L.st.rx_bad++;
      break;
    }
    case FrameType::Fragment: handle_fragment(L, fh.hop_src, body, meta); break;
    case FrameType::Control: handle_control(L, fh.hop_src, body); break;
  }
}

void MeshEngine::Impl::handle_unit(LinkSlot& L, uint32_t hop, ByteView unit, const RxMeta& meta) {
  if (unit.size() < 2) { L.st.rx_bad++; st.rx_bad++; return; }
  uint8_t t = unit[0];
  if (t == static_cast<uint8_t>(FrameType::Packet)) {
    if (handle_packet(unit.sub(1), L.id, meta, false, false, hop) < 0) L.st.rx_bad++;
  } else if (t == static_cast<uint8_t>(FrameType::Control)) {
    handle_control(L, hop, unit.sub(1));
  } else {
    L.st.rx_bad++;
    st.rx_bad++;
  }
}

void MeshEngine::Impl::handle_fragment(LinkSlot& L, uint32_t hop, ByteView body, const RxMeta& meta) {
  Fragment f;
  if (!parse_fragment(body, f)) { L.st.rx_bad++; st.rx_bad++; return; }
  Reassembly* ra = nullptr;
  for (auto& r : L.reasm)
    if (r.hop == hop && r.frag_id == f.frag_id) { ra = &r; break; }
  if (!ra) {
    size_t total_bytes = 0;
    for (auto& r : L.reasm) total_bytes += r.bytes;
    while (!L.reasm.empty() && (L.reasm.size() >= kMaxReasmPerLink || total_bytes > kMaxReasmBytesPerLink)) {
      total_bytes -= L.reasm.front().bytes;
      L.reasm.pop_front();
    }
    Reassembly r;
    r.hop = hop;
    r.frag_id = f.frag_id;
    r.count = f.count;
    r.parts.resize(f.count);
    r.first_ms = host_now;
    r.meta = meta;
    L.reasm.push_back(std::move(r));
    ra = &L.reasm.back();
  }
  if (ra->count != f.count) { L.st.rx_bad++; st.rx_bad++; return; }
  ra->last_ms = host_now;
  if (!ra->parts[f.index].empty()) return;  // duplicate fragment
  if (ra->bytes + f.data.size() > kMaxUnit) {
    // Inconsistent / hostile: drop the whole reassembly.
    for (auto i = L.reasm.begin(); i != L.reasm.end(); ++i)
      if (&*i == ra) { L.reasm.erase(i); break; }
    L.st.rx_bad++;
    st.rx_bad++;
    return;
  }
  if (f.index == 0) ra->total = f.total_len;
  ra->parts[f.index] = f.data.to_bytes();
  ra->bytes += f.data.size();
  ra->got++;
  if (meta.has_snr || meta.has_rssi) ra->meta = meta;
  if (ra->got < ra->count) return;
  Bytes unit;
  unit.reserve(ra->bytes);
  for (auto& p : ra->parts) unit.insert(unit.end(), p.begin(), p.end());
  bool ok = ra->total == unit.size();
  RxMeta m = ra->meta;
  for (auto i = L.reasm.begin(); i != L.reasm.end(); ++i)
    if (&*i == ra) { L.reasm.erase(i); break; }
  if (!ok) { L.st.rx_bad++; st.rx_bad++; return; }
  handle_unit(L, hop, unit, m);
}

void MeshEngine::Impl::handle_control(LinkSlot& L, uint32_t hop, ByteView body) {
  if (body.empty()) { L.st.rx_bad++; st.rx_bad++; return; }
  uint8_t t = body[0];
  ByteView rest = body.sub(1);
  switch (static_cast<Ctrl>(t)) {
    case Ctrl::Beacon: {
      Beacon b;
      if (!decode_beacon(rest, b)) { L.st.rx_bad++; st.rx_bad++; return; }
      handle_beacon(L, hop, b);
      break;
    }
    case Ctrl::Sync: {
      SyncMsg m;
      if (!decode_sync(rest, m)) { L.st.rx_bad++; st.rx_bad++; return; }
      if (m.target == my_hop && cfg.sync_enabled) handle_sync(L, hop, m);
      break;
    }
    case Ctrl::FragNack: {
      FragNack n;
      if (!decode_frag_nack(rest, n)) { L.st.rx_bad++; st.rx_bad++; return; }
      if (n.target == my_hop) handle_frag_nack(L, n);
      break;
    }
    default:
      L.st.rx_bad++;
      st.rx_bad++;
  }
}

// ---------------------------------------------------------------------------------------------
uint64_t MeshEngine::Impl::compute_expiry(const Header& h) const {
  uint64_t now = mesh_now();
  uint64_t created = wire_to_unix_ms(h.created);
  if (h.created < kClockUnsetWire || created > now + 3600000ull) created = now;  // sender's clock unset / ahead
  uint64_t exp = created + uint64_t(h.lifetime_min) * 60000ull;
  uint64_t cap = now + uint64_t(cfg.max_lifetime_s) * 1000ull;
  return exp > cap ? cap : exp;
}

bool MeshEngine::Impl::rate_ok(const NodeId& origin) {
  auto it = rate.find(origin);
  double cap = cfg.origin_burst;
  if (it == rate.end()) {
    if (rate.size() >= 1024) {
      auto victim = rate.begin();
      for (auto i = rate.begin(); i != rate.end(); ++i)
        if (i->second.second < victim->second.second) victim = i;
      rate.erase(victim);
    }
    it = rate.emplace(origin, std::make_pair(cap, host_now)).first;
  }
  auto& b = it->second;
  double refill = (host_now - b.second) * (cfg.origin_rate_per_min / 60000.0);
  b.first = std::min(cap, b.first + refill);
  b.second = host_now;
  if (b.first < 1.0) return false;
  b.first -= 1.0;
  return true;
}

void MeshEngine::Impl::note_heard(const PacketView& pv, uint32_t link_id, const RxMeta& meta) {
  if (pv.h.origin == me) return;
  bool is_new = false;
  PeerRec& p = peer_rec(pv.h.origin, &is_new);
  uint64_t now = mesh_now();
  uint64_t prev_seen = p.info.last_seen_ms;
  p.info.last_seen_ms = now;
  uint8_t prev_hops = p.info.hops;
  if (p.info.hops == 0xFF || pv.h.hops <= p.info.hops || now - p.hops_ms > 10 * 60000ull) {
    p.info.hops = pv.h.hops;
    p.hops_ms = now;
  }
  if (pv.h.hops == 0 && link_id) {
    PeerLink* pl = nullptr;
    for (auto& x : p.info.links)
      if (x.link_id == link_id) pl = &x;
    if (!pl) {
      if (p.info.links.size() >= 8) p.info.links.erase(p.info.links.begin());
      p.info.links.push_back(PeerLink{});
      pl = &p.info.links.back();
      pl->link_id = link_id;
    }
    pl->last_rx_ms = now;
    if (meta.has_rssi) { pl->rssi_dbm = ewma(pl->rssi_dbm, meta.rssi_dbm, pl->has_rssi); pl->has_rssi = true; }
    if (meta.has_snr) { pl->snr_db = ewma(pl->snr_db, meta.snr_db, pl->has_snr); pl->has_snr = true; }
    p.info.direct = true;
  }
  if (is_new || prev_hops != p.info.hops || now - prev_seen > 10000) notify_peer(p, is_new);
}

// Returns 1 accepted, 0 duplicate, -1 rejected.
int MeshEngine::Impl::handle_packet(ByteView raw, uint32_t link_id, const RxMeta& meta, bool from_bundle, bool via_sync,
                                    uint32_t hop_src) {
  PacketView pv;
  if (!parse_packet(raw, pv)) { st.rx_bad++; return -1; }
  const Header& h = pv.h;
  uint64_t wire = wire_id(raw);
  MsgId mid{h.origin, h.pkt_id};
  if (seen.contains(wire) || store.contains(wire)) {
    st.dropped_duplicates++;
    heard_duplicate(wire, link_id);
    {
      auto lit = links.find(link_id);
      if (lit != links.end()) evidence_of(lit->second, wire);  // someone relayed / re-sent our packet
    }
    // TTL upgrade: the first copy may have come the long way round. A copy with more hops left
    // replaces our stored hop budget (and is relayed if we could not relay before).
    if (StoreEntry* e = store.get_mut(wire)) {
      if (!e->own() && h.ttl > 0) {
        PacketView sv;
        uint8_t cur_ttl = 0;
        if (e->transmittable() && parse_packet(e->raw, sv)) cur_ttl = static_cast<uint8_t>(sv.h.ttl + 1);
        if (h.ttl > cur_ttl) {
          bool was_tx = e->transmittable();
          set_hop_byte(e->raw, static_cast<uint8_t>(h.ttl - 1), static_cast<uint8_t>(h.hops + 1));
          e->flags |= kStoredTransmittable;
          if (per) per->store_put(wire, e->raw, StoredMeta{e->received_ms, e->expires_ms, e->flags});
          bool to_me_only = h.dst_type == DestType::Node && h.dst_node == me;
          if (cfg.relay && !to_me_only && (!was_tx || h.ttl > cur_ttl + 1)) {
            PacketView tv;
            if (parse_packet(e->raw, tv)) schedule_flood(tv, wire, link_id, meta);
          }
        }
      }
    }
    if (h.origin == me) set_status(mid, MsgStatus::Relayed);
    // The sender retried: our ack was probably lost. Re-acknowledge (rate limited) with a fresh
    // ack packet so it is not swallowed by duplicate suppression on the way back.
    if (h.dst_type == DestType::Node && h.dst_node == me && h.want_ack && delivered.check(mid) == ReplayGuard::kAuth) {
      const uint64_t* lp = reack_ms.find(mid);
      uint64_t last = lp ? *lp : 0;
      if (host_now - last > 15000 || last == 0) {
        reack_ms.put(mid, host_now);
        auto it = peers.find(h.origin);
        if (it != peers.end() && peer_pair_key(it->second))
          send_ack(h.origin, h.pkt_id, ack_secret(it->second.pair, h.origin, h.pkt_id), compute_expiry(h));
      }
    }
    return 0;
  }
  if (h.origin == me) {
    if (seen.insert(wire)) seen_sids[sync_id(wire)]++;
    return 0;
  }
  uint64_t expires = compute_expiry(h);
  uint64_t now = mesh_now();
  {
    // Clock disagreement tolerance (security review C1): a packet that looks expired by up to a
    // day may just come from a node whose clock is behind ours. Keep it for a short grace period
    // instead of dropping fresh traffic; replays of genuinely old packets are still caught by
    // the replay memory, which outlives expiry by the same day.
    uint64_t max_age = uint64_t(cfg.max_lifetime_s) * 1000 + kSkewToleranceMs;
    bool too_old = h.created >= kClockUnsetWire && wire_to_unix_ms(h.created) + max_age < now;
    if (!too_old && expires <= now && now - expires <= kSkewToleranceMs && !from_bundle)
      expires = now + std::min<uint64_t>(uint64_t(h.lifetime_min) * 60000ull, 3600000ull);
    if (too_old || expires <= now) {
      st.dropped_expired++;
      if (seen.insert(wire)) seen_sids[sync_id(wire)]++;
      return -1;
    }
  }
  if (h.has_ack_commit()) {
    if (ack_known(mid, commit_u64(h.ack_commit))) {
      if (seen.insert(wire)) seen_sids[sync_id(wire)]++;
      return 0;  // already delivered end to end
    }
  }
  if (!rate_ok(h.origin)) { st.dropped_rate++; return -1; }

  // Authentication that relays can do: announcements are self-certifying; signed packets from
  // origins whose key we know are verified before we spend airtime or storage on them.
  auto pit = peers.find(h.origin);
  bool had_keys = pit != peers.end() && pit->second.info.has_keys;
  uint8_t trust = kTrustUnknown;
  if (h.kind == Kind::Announce) {
    // New identities are free to make (sybils): their rate is budgeted (security review M3).
    if (!had_keys && !admit_new_identity(link_id, from_bundle)) { st.dropped_rate++; return -1; }
    bool ok = false;
    handle_announce(pv, link_id, meta, &ok, from_bundle ? 0 : hop_src);
    if (!ok) { st.dropped_auth++; return -1; }
    trust = kTrustVerified;
  } else if (h.sec == Sec::Signed) {
    if (had_keys) {
      if (!verify_signed(pv, pit->second.info.keys.sign_pk)) { st.dropped_auth++; return -1; }
      trust = kTrustVerified;
    }
  } else if (h.kind == Kind::Ack) {
    if (ack_verifiable(pv)) trust = kTrustVerified;
  }
  Opened o;
  bool need_key = false;
  if (!open_packet(pv, o, &need_key)) { st.dropped_auth++; return -1; }
  if (o.authenticated) trust = kTrustVerified;
  else if (trust == kTrustUnknown && had_keys && h.sec != Sec::Signed) trust = kTrustKnown;
  // A pairwise-authenticated packet to us straight from its origin proves that neighbour already
  // has our keys: it needs no discovery announcement from us.
  if (o.authenticated && o.for_me && (h.sec == Sec::PairMac || h.sec == Sec::PairAead) && h.hops == 0 &&
      link_id && hop_src && hop_src == short_id(h.origin)) {
    auto lit = links.find(link_id);
    if (lit != links.end()) {
      auto nb = lit->second.nbrs.find(hop_src);
      if (nb != lit->second.nbrs.end()) nb->second.knows_us = true;
    }
  }

  // Admission budgets for what we cannot verify (security review C2): per neighbour, per link and
  // global for unknown origins; per link for known-but-unverifiable traffic.
  if (!admit(trust, link_id, hop_src, from_bundle)) { st.dropped_rate++; return -1; }

  if (seen.insert(wire)) seen_sids[sync_id(wire)]++;
  // Unknown origins do not create peer-table entries (they would evict real peers).
  if (trust != kTrustUnknown || peers.count(h.origin)) note_heard(pv, link_id, meta);

  if (h.kind == Kind::Ack) handle_ack_packet(pv);
  if (h.kind == Kind::Whois) handle_whois(pv, link_id);

  bool transmittable = h.ttl > 0;
  Bytes txraw = raw.to_bytes();
  if (transmittable) {
    uint8_t ttl = static_cast<uint8_t>(h.ttl - 1);
    // Unknown origins travel at most two more hops (SOS) or one (anything else).
    if (trust == kTrustUnknown) ttl = std::min<uint8_t>(ttl, h.kind == Kind::Sos ? 2 : 1);
    set_hop_byte(txraw, ttl, static_cast<uint8_t>(h.hops + 1));
  }
  uint8_t flags = transmittable ? kStoredTransmittable : 0;
  bool to_me_only = h.dst_type == DestType::Node && h.dst_node == me;
  bool keep = !(to_me_only && h.want_ack);  // acknowledged right away; the ack replaces it
  if (h.kind == Kind::Bulletin && o.authenticated) {
    BulletinBody bb;
    if (o.for_me && decode(o.body, bb) && bb.is_delete)
      for (uint64_t w : store.find_msg(MsgId{h.origin, bb.delete_pkt_id})) store_remove(w);
  }
  if (keep) store_put(pv, txraw, now, expires, flags, trust);

  if (transmittable && cfg.relay && !to_me_only) {
    PacketView tv;
    if (parse_packet(txraw, tv)) schedule_flood(tv, wire, link_id, meta);
  } else if (!transmittable && !to_me_only) {
    st.dropped_ttl++;
  }

  if (o.for_me) {
    bool sos_alert = false;
    if (h.kind == Kind::Sos) {
      SosBody sb;
      sos_alert = decode(o.body, sb) && sb.active;
    }
    if (need_key && sos_alert) {
      // A stranger in trouble is heard at once, flagged unverified and rate-limited; a cancel
      // waits for verification like everything else (security review C2/H1).
      if (from_bundle || admit_unverified_sos(link_id)) deliver(pv, o, link_id, meta, from_bundle, now, expires);
      send_whois(h.origin);
    } else if (need_key) {
      // Hold for the sender's announcement (asked for with WHOIS).
      if (pending.size() >= kMaxPending) pending.pop_front();
      PendingRx pr;
      pr.raw = raw.to_bytes();
      pr.link = link_id;
      pr.meta = meta;
      pr.received_ms = now;
      pr.from_bundle = from_bundle;
      pending.push_back(std::move(pr));
      send_whois(h.origin);
    } else {
      deliver(pv, o, link_id, meta, from_bundle, now, expires);
    }
  }
  (void)via_sync;
  return 1;
}

bool MeshEngine::Impl::open_packet(const PacketView& pv, Opened& o, bool* need_key) {
  const Header& h = pv.h;
  *need_key = false;
  o = Opened();
  auto known = peers.find(h.origin);
  bool has_keys = known != peers.end() && known->second.info.has_keys;
  switch (h.dst_type) {
    case DestType::Broadcast:
      o.for_me = h.kind != Kind::Ack && h.kind != Kind::Whois;
      o.body = pv.body.to_bytes();
      if (h.sec == Sec::Signed) {
        if (has_keys) o.authenticated = true;  // verified by the caller
        else *need_key = true;
      }
      return true;
    case DestType::Node: {
      if (h.dst_node != me) { o.for_me = false; return true; }
      o.for_me = h.kind != Kind::Ack;
      if (h.sec == Sec::None) { o.body = pv.body.to_bytes(); return true; }
      if (h.sec == Sec::Signed) {
        o.body = pv.body.to_bytes();
        if (has_keys) o.authenticated = true;
        else *need_key = true;
        return true;
      }
      if (!has_keys || !peer_pair_key(known->second)) { *need_key = true; return true; }
      const crypto::Key32& k = known->second.pair;
      if (h.sec == Sec::PairMac) {
        if (!verify_pair_mac(pv, k)) return false;
        o.body = pv.body.to_bytes();
        o.authenticated = true;
        return true;
      }
      if (h.sec == Sec::PairAead) {
        if (!open_pair_aead(pv, k, o.body)) return false;
        o.authenticated = true;
        o.encrypted = true;
        return true;
      }
      return false;
    }
    case DestType::Channel: {
      auto ch = channels.find(h.channel);
      if (ch == channels.end()) { o.for_me = false; return true; }
      o.channel_name = ch->second.name;
      if (ch->second.encrypted) {
        if (h.sec != Sec::ChanAead) { o.for_me = false; return true; }
        crypto::Sig64 sig;
        if (!open_chan(pv, ch->second, o.body, sig)) { o.for_me = false; return true; }  // id collision
        o.for_me = true;
        o.encrypted = true;
        if (has_keys) {
          if (!verify_chan_sig(pv, o.body, sig, known->second.info.keys.sign_pk)) return false;  // member forgery
          o.authenticated = true;
        } else {
          *need_key = true;
        }
        return true;
      }
      if (h.sec != Sec::Signed) { o.for_me = false; return true; }
      o.for_me = true;
      o.body = pv.body.to_bytes();
      if (has_keys) o.authenticated = true;
      else *need_key = true;
      return true;
    }
  }
  return false;
}

void MeshEngine::Impl::deliver(const PacketView& pv, const Opened& o, uint32_t link_id, const RxMeta& meta,
                               bool from_bundle, uint64_t received_ms, uint64_t expires_ms) {
  const Header& h = pv.h;
  MsgId mid{h.origin, h.pkt_id};
  // Replay of something already delivered (an authenticated copy supersedes an unverified one).
  ReplayGuard::Seen seen_as = delivered.check(mid);
  if (seen_as == ReplayGuard::kAuth || (seen_as == ReplayGuard::kUnauth && !o.authenticated)) return;
  remember_delivery(mid, expires_ms, o.authenticated);
  RxInfo rx;
  rx.id = mid;
  rx.kind = h.kind;
  rx.from = h.origin;
  rx.dest.type = static_cast<Dest::Type>(h.dst_type);
  rx.dest.node = h.dst_node;
  rx.dest.channel = h.channel;
  rx.prio = h.prio;
  rx.created_ms = h.created >= kClockUnsetWire ? wire_to_unix_ms(h.created) : received_ms;
  rx.received_ms = received_ms;
  rx.expires_ms = expires_ms;
  rx.hops = h.hops;
  rx.via_link = link_id;
  rx.encrypted = o.encrypted;
  rx.authenticated = o.authenticated;
  rx.from_bundle = from_bundle;
  rx.has_rssi = meta.has_rssi;
  rx.has_snr = meta.has_snr;
  rx.rssi_dbm = meta.rssi_dbm;
  rx.snr_db = meta.snr_db;
  rx.channel_name = o.channel_name;
  bool to_me = h.dst_type == DestType::Node && h.dst_node == me;
  bool ok = false;
  auto ack_if_wanted = [&]() {
    if (!to_me || !h.want_ack) return;
    auto it = peers.find(h.origin);
    if (it == peers.end() || !peer_pair_key(it->second)) return;
    AckSecret s = ack_secret(it->second.pair, h.origin, h.pkt_id);
    if (ack_commit(s) != h.ack_commit) return;  // sender used a different key; cannot ack
    reack_ms.put(mid, host_now);
    send_ack(h.origin, h.pkt_id, s, expires_ms);
  };
  switch (h.kind) {
    case Kind::Text: {
      TextBody b;
      if ((ok = decode(o.body, b)) && del) del->on_text(rx, b);
      if (ok) ack_if_wanted();
      break;
    }
    case Kind::Position: {
      PositionBody b;
      if ((ok = decode(o.body, b))) {
        auto it = peers.find(h.origin);
        if (it != peers.end() && o.authenticated) {
          it->second.info.position = b.pos;
          it->second.info.position_ms = rx.created_ms;
        }
        if (del) del->on_position(rx, b.pos);
        ack_if_wanted();
      }
      break;
    }
    case Kind::Sos: {
      SosBody b;
      if ((ok = decode(o.body, b))) {
        if (b.pos && o.authenticated) {
          auto it = peers.find(h.origin);
          if (it != peers.end()) { it->second.info.position = b.pos; it->second.info.position_ms = rx.created_ms; }
        }
        if (del) del->on_sos(rx, b);
      }
      break;
    }
    case Kind::SosAck: {
      SosAckBody b;
      if ((ok = decode(o.body, b)) && del) del->on_sos_ack(rx, b);
      break;
    }
    case Kind::Checkin: {
      CheckinBody b;
      if ((ok = decode(o.body, b)) && del) del->on_checkin(rx, b);
      break;
    }
    case Kind::Bulletin: {
      BulletinBody b;
      if ((ok = decode(o.body, b)) && del) del->on_bulletin(rx, b);
      break;
    }
    case Kind::NetPlan: {
      NetPlanBody b;
      if ((ok = decode(o.body, b)) && del) del->on_netplan(rx, b);
      break;
    }
    case Kind::Announce: {
      AnnounceBody b;
      if ((ok = decode(o.body, b)) && del) del->on_announce(rx, b);
      break;
    }
    case Kind::File: {
      FileBody b;
      if ((ok = decode(o.body, b))) file_chunk(rx, b, h);
      break;
    }
    case Kind::App: {
      AppBody b;
      if ((ok = decode(o.body, b))) {
        if (del) del->on_app(rx, b.app_type, b.data);
        ack_if_wanted();
      }
      break;
    }
    case Kind::Read: {
      ReadBody b;
      if ((ok = decode(o.body, b)) && o.authenticated) {
        for (uint32_t pid : b.pkt_ids) {
          MsgId m{me, pid};
          // Only messages we actually sent to the reader (security review L5).
          auto oit = out.find(m);
          if (oit == out.end() || oit->second.dest != h.origin) continue;
          set_status(m, MsgStatus::Read);
          if (del) del->on_read(h.origin, m);
        }
      }
      break;
    }
    case Kind::Ping: {
      PingBody b;
      if ((ok = decode(o.body, b)) && o.authenticated) {
        SendOptions so;
        so.prio = Prio::High;
        so.want_ack = false;
        originate(Kind::Pong, Dest::to_node(h.origin), encode(PongBody{b.nonce, h.hops}), so, 600);
      }
      break;
    }
    case Kind::Pong: {
      PongBody b;
      if ((ok = decode(o.body, b)) && o.authenticated) {
        auto it = pings.find(b.nonce);
        if (it != pings.end() && it->second.first == h.origin) {
          uint64_t rtt = host_now - it->second.second;
          pings.erase(it);
          if (del) del->on_pong(h.origin, static_cast<uint32_t>(rtt > 0xFFFFFFFFull ? 0xFFFFFFFFu : rtt), b.ping_hops);
        }
      }
      break;
    }
    default:
      break;  // Ack/Whois handled elsewhere; unknown kinds are relayed only
  }
  if (ok) st.delivered++;
}

void MeshEngine::Impl::file_chunk(const RxInfo& rx, const FileBody& fb, const Header& h) {
  uint32_t expect = fb.file_id + fb.index;  // chunk i is sent with pkt_id file_id + i
  if (expect == 0) expect = 1;
  if (rx.id.pkt_id != expect) return;
  auto key = std::make_pair(node_id_u64(rx.from), fb.file_id);
  auto it = files.find(key);
  if (it == files.end()) {
    if (files.size() >= kMaxFilesRx) {
      auto victim = files.begin();
      for (auto i = files.begin(); i != files.end(); ++i)
        if (i->second.last_ms < victim->second.last_ms) victim = i;
      files.erase(victim);
    }
    FileRx f;
    f.origin = rx.from;
    f.file_id = fb.file_id;
    f.count = fb.count;
    f.chunks.resize(fb.count);
    f.have.resize(fb.count, false);
    it = files.emplace(key, std::move(f)).first;
  }
  FileRx& f = it->second;
  f.last_ms = host_now;
  if (f.count != fb.count || f.have[fb.index]) return;
  if (f.bytes + fb.data.size() > cfg.max_file_bytes) { files.erase(it); return; }
  if (fb.index == 0) {
    if (fb.total_size > cfg.max_file_bytes) { files.erase(it); return; }
    f.has_meta = true;
    f.total = fb.total_size;
    f.name = fb.name;
    f.mime = fb.mime;
    f.hash = fb.hash;
    f.info0 = rx;
    f.want_ack = h.want_ack;
    f.ack_commit = h.ack_commit;
  }
  f.chunks[fb.index] = fb.data;
  f.have[fb.index] = true;
  f.got++;
  f.bytes += fb.data.size();
  MsgId file_msg{rx.from, fb.file_id};
  if (del) del->on_file_progress(file_msg, rx.from, f.got, f.count);
  if (f.got < f.count || !f.has_meta) return;
  Bytes data;
  data.reserve(f.bytes);
  for (auto& c : f.chunks) data.insert(data.end(), c.begin(), c.end());
  std::array<uint8_t, 16> hash;
  crypto::blake2b(hash.data(), 16, data);
  FileRx done = std::move(f);
  files.erase(it);
  if (data.size() != done.total || hash != done.hash) return;
  if (del) del->on_file(done.info0, done.name, done.mime, data);
  // Acknowledge the file (chunk 0) only once it is complete.
  const RxInfo& r0 = done.info0;
  if (r0.dest.type == Dest::Node && r0.dest.node == me && done.want_ack) {
    auto pit = peers.find(r0.from);
    if (pit != peers.end() && peer_pair_key(pit->second)) {
      AckSecret s = ack_secret(pit->second.pair, r0.from, r0.id.pkt_id);
      if (ack_commit(s) == done.ack_commit) {
        reack_ms.put(r0.id, host_now);
        send_ack(r0.from, r0.id.pkt_id, s, r0.expires_ms);
      }
    }
  }
}

bool MeshEngine::Impl::ack_verifiable(const PacketView& pv) const {
  AckBody b;
  if (!decode(pv.body, b) || pv.h.dst_type != DestType::Node) return false;
  const NodeId& sender = pv.h.dst_node;
  for (const AckEntry& a : b.acks) {
    MsgId m{sender, a.pkt_id};
    AckCommit c = ack_commit(a.secret);
    for (uint64_t w : store.find_msg(m)) {
      const StoreEntry* e = store.get(w);
      if (e && e->h.has_ack_commit() && e->h.ack_commit == c) return true;
    }
    if (sender == me) {
      auto it = out.find(m);
      if (it != out.end() && ack_commit(it->second.secret) == c) return true;
    }
  }
  return false;
}

bool MeshEngine::Impl::handle_ack_packet(const PacketView& pv) {
  AckBody b;
  bool any = false;
  if (!decode(pv.body, b) || pv.h.dst_type != DestType::Node) return false;
  const NodeId& sender = pv.h.dst_node;  // the original sender of the acknowledged packets
  for (const AckEntry& a : b.acks) {
    MsgId m{sender, a.pkt_id};
    AckCommit c = ack_commit(a.secret);
    uint64_t cv = commit_u64(c);
    bool verified = false;
    for (uint64_t w : store.find_msg(m)) {
      const StoreEntry* e = store.get(w);
      if (e && e->h.has_ack_commit() && e->h.ack_commit == c) {
        verified = true;
        store_remove(w);
      }
    }
    if (sender == me) {
      auto it = out.find(m);
      if (it != out.end() && it->second.reliable && ack_commit(it->second.secret) == c) {
        verified = true;
        st.acks_received++;
        it->second.reliable = false;  // stop retrying
        set_status(m, MsgStatus::Delivered);
      }
    }
    // Unverifiable acks (we never held the packet) are remembered separately: they only stop us
    // from re-accepting a packet whose commitment matches, which a forger cannot produce, and
    // junk acks cannot push verified ones out (security review H4).
    if (verified) acked_v.put(m, cv);
    else acked_u.put(m, cv);
    any = any || verified;
  }
  return any;
}

void MeshEngine::Impl::handle_announce(const PacketView& pv, uint32_t link_id, const RxMeta& meta, bool* ok,
                                       uint32_t hop_src) {
  *ok = false;
  AnnounceBody a;
  if (pv.h.sec != Sec::Signed || pv.h.dst_type != DestType::Broadcast || !decode(pv.body, a)) return;
  if (a.keys.node_id() != pv.h.origin) return;
  if (!verify_signed(pv, a.keys.sign_pk)) return;
  bool is_new = false;
  PeerRec& p = peer_rec(pv.h.origin, &is_new);
  if (p.info.has_keys && p.info.keys != a.keys) return;  // cannot happen without a 64-bit collision
  *ok = true;
  bool got_keys = !p.info.has_keys;
  p.info.has_keys = true;
  p.info.keys = a.keys;
  if (pv.h.created >= p.announce_created) {
    p.announce_created = pv.h.created;
    p.info.name = a.name;
    p.info.callsign = a.callsign;
    p.info.status = a.status;
    p.info.relay = a.relay;
  }
  // Authenticated time evidence: only an announcement heard straight from its origin.
  if (link_id && hop_src && hop_src == short_id(pv.h.origin) && pv.h.hops == 0 && pv.h.created >= kClockUnsetWire)
    time_sample(pv.h.origin, wire_to_unix_ms(pv.h.created) + 500, a.time_quality, p.info.pinned);
  if (link_id) notify_peer(p, is_new);
  if (got_keys) {
    upgrade_origin(pv.h.origin);
    flush_await_key(pv.h.origin);
    process_pending(pv.h.origin);
  }
  (void)meta;
}

void MeshEngine::Impl::handle_whois(const PacketView& pv, uint32_t link_id) {
  WhoisBody w;
  if (!decode(pv.body, w)) return;
  bool self_resend = false;
  if (w.target == me) {
    if (host_now - last_announce_ms > 30000 || last_announce_ms == 0) {
      announce();
      return;
    }
    // We announced moments ago but the asker evidently missed it (a collision on a slow shared
    // channel): re-send that same announcement on this link, at most every 10 s per link. Never a
    // new packet, so WHOIS floods cannot make us originate more than one per 30 s.
    auto lit = links.find(link_id);
    if (lit == links.end() || !contended(lit->second) ||
        (lit->second.self_ann_ms && host_now - lit->second.self_ann_ms < 10000))
      return;
    lit->second.self_ann_ms = host_now;
    self_resend = true;
  }
  Header probe;
  probe.kind = Kind::Announce;
  probe.origin = self_resend ? me : w.target;
  const StoreEntry* e = store.find_replace(replace_key(probe));
  if (!e || !e->transmittable()) return;
  auto it = links.find(link_id);
  if (it == links.end()) return;
  LinkSlot& L = it->second;
  for (const auto& f : L.floods)
    if (f.wire == e->wire) return;
  if (L.floods.size() >= 512) return;
  PendingFlood pf;
  pf.raw = e->raw;
  pf.wire = e->wire;
  pf.prio = 4;
  pf.due_ms = host_now + 50 + rng.uniform(std::max<uint32_t>(200, 4 * airtime_ms(L, e->raw.size())));
  // One copy suffices: suppress it as soon as anyone else answers (threshold is 1 on contended links).
  pf.heard = static_cast<uint8_t>(!contended(L) && cfg.suppress_threshold > 0 ? cfg.suppress_threshold - 1 : 0);
  L.floods.push_back(pf);
}

void MeshEngine::Impl::process_pending(const NodeId& origin) {
  std::deque<PendingRx> keep;
  std::vector<PendingRx> ready;
  for (auto& p : pending) {
    PacketView pv;
    if (parse_packet(p.raw, pv) && pv.h.origin == origin) ready.push_back(std::move(p));
    else keep.push_back(std::move(p));
  }
  pending.swap(keep);
  for (auto& p : ready) {
    PacketView pv;
    if (!parse_packet(p.raw, pv)) continue;
    if (pv.h.sec == Sec::Signed) {
      auto it = peers.find(origin);
      if (it == peers.end() || !verify_signed(pv, it->second.info.keys.sign_pk)) { st.dropped_auth++; continue; }
    }
    Opened o;
    bool need = false;
    if (!open_packet(pv, o, &need) || need) { st.dropped_auth++; continue; }
    if (o.for_me) deliver(pv, o, p.link, p.meta, p.from_bundle, p.received_ms, compute_expiry(pv.h));
  }
}

void MeshEngine::Impl::expire_pending(uint64_t now) {
  (void)now;
  uint64_t mnow = mesh_now();
  while (!pending.empty() && mnow - pending.front().received_ms > kPendingMs) {
    PendingRx p = std::move(pending.front());
    pending.pop_front();
    PacketView pv;
    if (!parse_packet(p.raw, pv)) continue;
    // The key may have arrived some other way meanwhile: verify then.
    auto kit = peers.find(pv.h.origin);
    if (kit != peers.end() && kit->second.info.has_keys) {
      Opened o;
      bool need = false;
      bool sig_ok = pv.h.sec != Sec::Signed || verify_signed(pv, kit->second.info.keys.sign_pk);
      if (sig_ok && open_packet(pv, o, &need) && !need && o.for_me)
        deliver(pv, o, p.link, p.meta, p.from_bundle, p.received_ms, compute_expiry(pv.h));
      else
        st.dropped_auth++;
      continue;
    }
    // Signed cleartext can be shown unverified; ciphertext without the key is lost. An unverified
    // packet never cancels or withdraws anything (security review H1): SOS cancels, bulletin
    // withdrawals and read receipts are dropped.
    bool withdraws = false;
    if (pv.h.kind == Kind::Sos) {
      SosBody sb;
      withdraws = !decode(pv.body, sb) || !sb.active;
    } else if (pv.h.kind == Kind::Bulletin) {
      BulletinBody bb;
      withdraws = !decode(pv.body, bb) || bb.is_delete;
    } else if (pv.h.kind == Kind::Read) {
      withdraws = true;
    }
    if (pv.h.sec == Sec::Signed && !withdraws) {
      Opened o;
      o.for_me = true;
      o.body = pv.body.to_bytes();
      if (pv.h.dst_type == DestType::Channel) {
        auto ch = channels.find(pv.h.channel);
        if (ch != channels.end()) o.channel_name = ch->second.name;
      }
      deliver(pv, o, p.link, p.meta, p.from_bundle, p.received_ms, compute_expiry(pv.h));
    } else {
      st.dropped_auth++;
    }
  }
}

}  // namespace ember::mesh
