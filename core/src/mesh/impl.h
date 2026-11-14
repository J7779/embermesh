// EmberMesh core — mesh engine internals (not part of the public API; include engine.h instead).
#pragma once

#include <algorithm>
#include <array>
#include <deque>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "crypto/rng.h"
#include "mesh/engine.h"

namespace ember::mesh {

// ---------------------------------------------------------------------------------------------
// Bounded FIFO set of 64-bit keys (duplicate suppression, delivered-message memory).
class RecentSet {
 public:
  explicit RecentSet(size_t cap = 8192) : cap_(cap ? cap : 1) {}
  bool contains(uint64_t k) const { return set_.count(k) != 0; }
  // Returns true if newly inserted.
  bool insert(uint64_t k);
  void set_capacity(size_t cap);
  size_t size() const { return set_.size(); }
  const std::deque<uint64_t>& order() const { return order_; }

 private:
  size_t cap_;
  std::unordered_set<uint64_t> set_;
  std::deque<uint64_t> order_;
};

// ---------------------------------------------------------------------------------------------
// SipHash-2-4 with a per-engine random key: hash tables indexed by attacker-chosen ids cannot be
// collision-flooded, and no table is keyed by a lossy hash of a MsgId (security review H2).
uint64_t siphash24(uint64_t k0, uint64_t k1, const uint8_t* p, size_t n);
struct KeyedMsgIdHash {
  uint64_t k0 = 0, k1 = 0;
  size_t operator()(const MsgId& m) const {
    uint8_t b[12];
    for (int i = 0; i < 8; ++i) b[i] = m.origin[static_cast<size_t>(i)];
    b[8] = static_cast<uint8_t>(m.pkt_id >> 24);
    b[9] = static_cast<uint8_t>(m.pkt_id >> 16);
    b[10] = static_cast<uint8_t>(m.pkt_id >> 8);
    b[11] = static_cast<uint8_t>(m.pkt_id);
    return static_cast<size_t>(siphash24(k0, k1, b, 12));
  }
};

// Bounded FIFO map with exact MsgId keys.
template <class V>
class MsgFifo {
 public:
  MsgFifo(size_t cap = 4096, KeyedMsgIdHash h = KeyedMsgIdHash()) : cap_(cap ? cap : 1), map_(16, h) {}
  const V* find(const MsgId& m) const {
    auto it = map_.find(m);
    return it == map_.end() ? nullptr : &it->second;
  }
  void put(const MsgId& m, const V& v) {
    auto it = map_.find(m);
    if (it != map_.end()) { it->second = v; return; }
    map_.emplace(m, v);
    order_.push_back(m);
    while (order_.size() > cap_) {
      map_.erase(order_.front());
      order_.pop_front();
    }
  }
  template <class Pred>
  void erase_if(Pred pred) {
    for (auto it = map_.begin(); it != map_.end();) {
      if (pred(it->second)) it = map_.erase(it);
      else ++it;
    }
    std::deque<MsgId> keep;
    for (const MsgId& m : order_)
      if (map_.count(m)) keep.push_back(m);
    order_.swap(keep);
  }
  size_t size() const { return map_.size(); }

 private:
  size_t cap_;
  std::unordered_map<MsgId, V, KeyedMsgIdHash> map_;
  std::deque<MsgId> order_;
};

// Memory of delivered message ids until they expire (security review H4/M4). Authenticated
// deliveries are kept until expiry (bounded; evicting the soonest to expire) and persisted by the
// node, so a captured packet cannot be replayed into the app after junk flushed other caches.
// Unauthenticated deliveries live in a separate, smaller FIFO and never displace authenticated
// ones; a later authenticated copy of the same id is still delivered (it supersedes the
// unverified one).
class ReplayGuard {
 public:
  enum Seen { kNone = 0, kUnauth = 1, kAuth = 2 };
  explicit ReplayGuard(KeyedMsgIdHash h = KeyedMsgIdHash(), size_t cap_auth = 32768, size_t cap_unauth = 4096)
      : cap_auth_(cap_auth), map_(64, h), unauth_(cap_unauth, h) {}
  Seen check(const MsgId& m) const {
    if (map_.count(m)) return kAuth;
    return unauth_.find(m) ? kUnauth : kNone;
  }
  // Returns ids evicted from the authenticated set (for persistence).
  void put(const MsgId& m, uint64_t expires_ms, bool auth, std::vector<MsgId>* evicted);
  void prune(uint64_t now_ms, std::vector<MsgId>* removed);
  size_t size_auth() const { return map_.size(); }

 private:
  size_t cap_auth_;
  std::unordered_map<MsgId, uint64_t, KeyedMsgIdHash> map_;  // -> expiry
  std::set<std::pair<uint64_t, MsgId>> by_exp_;
  MsgFifo<uint8_t> unauth_;
};

// ---------------------------------------------------------------------------------------------
// Token bucket (admission control, docs/PROTOCOL.md §11).
struct Bucket {
  double tokens = -1;  // < 0: not initialised (starts full)
  uint64_t ms = 0;
  bool take(uint64_t now, double per_min, double burst) {
    if (tokens < 0) { tokens = burst; ms = now; }
    if (now > ms) tokens = std::min(burst, tokens + double(now - ms) * per_min / 60000.0);
    ms = now;
    if (tokens < 1.0) return false;
    tokens -= 1.0;
    return true;
  }
};

// How much we know about a packet's origin. Unknown = no key (cannot verify anything);
// Known = origin key known but we cannot verify this packet (someone else's DM / channel);
// Verified = signature / MAC / AEAD / self-certifying announce / matching ack checked by us.
enum Trust : uint8_t { kTrustUnknown = 0, kTrustKnown = 1, kTrustVerified = 2 };

// ---------------------------------------------------------------------------------------------
struct StoreEntry {
  uint64_t wire = 0;
  uint32_t sid = 0;
  Bytes raw;  // hop byte holds the values for our next transmission
  proto::Header h;
  uint64_t received_ms = 0;
  uint64_t expires_ms = 0;     // mesh time (persisted, exported)
  uint64_t expires_local = 0;  // engine monotonic clock: what expiry decisions use, so clock
                               // jumps can never purge the store (security review C1)
  uint8_t flags = 0;  // kStoredOwn | kStoredTransmittable
  uint8_t trust = kTrustVerified;
  bool upgraded = false;  // its hop budget was raised once by a better copy
  uint64_t replace_key = 0;
  bool own() const { return flags & kStoredOwn; }
  bool transmittable() const { return flags & kStoredTransmittable; }
};

// Store-carry-forward packet store with quota and priority-aware eviction.
class DtnStore {
 public:
  void set_limits(size_t max_bytes, size_t max_packets) { max_bytes_ = max_bytes; max_packets_ = max_packets; }
  bool contains(uint64_t wire) const { return map_.count(wire) != 0; }
  const StoreEntry* get(uint64_t wire) const;
  StoreEntry* get_mut(uint64_t wire);
  // Inserts (evicting lower-value entries if needed). Returns false if the entry itself was the
  // least valuable and does not fit. `evicted` receives removed wire ids.
  bool put(StoreEntry&& e, uint64_t now, std::vector<uint64_t>* evicted);
  bool remove(uint64_t wire);
  std::vector<uint64_t> find_msg(const MsgId& m) const;
  const StoreEntry* find_replace(uint64_t replace_key) const;
  void expired(uint64_t now, std::vector<uint64_t>& out) const;
  size_t count() const { return map_.size(); }
  size_t bytes() const { return bytes_; }
  size_t unknown_count() const { return u_count_; }
  void set_trust(uint64_t wire, uint8_t t);
  std::vector<uint64_t> by_sid(uint32_t sid) const;
  uint64_t generation() const { return gen_; }
  const std::unordered_map<uint64_t, StoreEntry>& all() const { return map_; }

 private:
  static int value_class(const StoreEntry& e);
  std::unordered_map<uint64_t, StoreEntry> map_;
  std::unordered_multimap<MsgId, uint64_t, proto::MsgIdHash> by_msg_;
  std::unordered_map<uint64_t, uint64_t> by_replace_;
  std::unordered_multimap<uint32_t, uint64_t> by_sid_;  // sync id -> wire (security review H3)
  size_t bytes_ = 0;
  size_t u_count_ = 0, u_bytes_ = 0;  // unknown-origin share (capped at 10 % of the quota)
  uint64_t gen_ = 0;
  bool evict_one(uint64_t now, int newv, bool unknown_only);
  std::vector<uint64_t> evicted_tmp_;
  size_t max_bytes_ = 4u << 20;
  size_t max_packets_ = 5000;
};

// ---------------------------------------------------------------------------------------------
struct QItem {
  Bytes frame;
  uint8_t prio = 4;
  uint64_t enq_ms = 0;
  uint64_t expire_ms = 0;
  uint64_t wire = 0;      // packet carried (0 for control frames)
  uint32_t own_pkt = 0;   // pkt_id of one of our own packets (status "sent" once transmitted)
};

struct Neighbour {
  uint32_t hop = 0;
  NodeId node{};
  bool has_node = false;
  uint64_t first_ms = 0, last_rx_ms = 0;
  bool has_rssi = false, has_snr = false;
  float rssi = 0, snr = 0;
  std::string station;
  bool has_digest = false;
  uint32_t digest_count = 0, digest_sum = 0;
  uint64_t digest_ms = 0;
  uint64_t last_sync_ms = 0;     // last sync round we started or answered
  uint64_t mismatch_since = 0;
  uint32_t sync_backoff_ms = 0;  // grows when rounds transfer nothing
  uint32_t round_pushed = 0;
  uint32_t round_wanted = 0;
  uint32_t round_rx = 0;         // sync messages received from them this round
  uint32_t round_bytes = 0;      // bytes we pushed to them this round
  bool relay = true;
  bool knows_us = false;         // sent us a direct pairwise-authenticated packet: has our keys
  bool proven = false;           // identity proven: verified ANNOUNCE (hops 0) or direct pairwise packet
  NodeId proven_node{};
  Bucket unknown_b, sync_b;      // per-neighbour admission (unknown origins, SYNC messages)
  uint64_t win_start = 0;
  uint32_t win_frames = 0;
};

struct Reassembly {
  uint32_t hop = 0;
  uint16_t frag_id = 0;
  uint8_t count = 0;
  uint16_t total = 0;  // from fragment 0 (0 = not yet seen)
  std::vector<Bytes> parts;
  uint8_t got = 0;
  size_t bytes = 0;
  uint64_t first_ms = 0, last_ms = 0;
  uint8_t nacks = 0;
  RxMeta meta;
};

struct TxFrag {
  uint16_t frag_id = 0;
  std::vector<Bytes> frames;
  uint8_t prio = 4;
  uint64_t ms = 0;
  uint8_t resends = 0;
  uint64_t wire = 0;
};

struct PendingFlood {
  Bytes raw;  // packet in transmit form (hop byte already advanced)
  uint64_t wire = 0;
  uint64_t due_ms = 0;
  uint8_t heard = 0;
  uint8_t prio = 4;
};

// Fast repair of one of our own fresh user packets on a contended (slow, half-duplex) link: if no
// evidence that a neighbour got it arrives (a relayed copy, or a digest beacon that matches ours),
// the packet is sent once or twice more. Bounded per packet and per link.
struct Repair {
  uint64_t wire = 0;
  uint64_t due_ms = 0;   // 0 = not yet transmitted
  uint8_t left = 2;      // re-sends still allowed
  uint8_t prio = 3;
  uint64_t announce_wire = 0;  // our announcement piggy-backed on it (re-sent first if still held)
  uint64_t created_ms = 0;
};

struct LinkSlot {
  uint32_t id = 0;
  Link* link = nullptr;
  LinkScope scope;
  LinkCaps caps;
  LinkState state = LinkState::Down;
  std::array<std::deque<QItem>, 8> q;
  size_t qbytes = 0;
  size_t qframes = 0;
  double tokens_ms = 0;
  uint64_t token_ms = 0;  // last refill time
  uint64_t busy_until = 0;
  LinkStats st;
  std::array<uint64_t, 10> air_bucket{};  // airtime per minute, ring
  uint64_t air_bucket_minute = 0;
  uint64_t next_beacon_ms = 0;
  uint64_t last_beacon_ms = 0;
  bool beacon_soon = false;
  std::unordered_map<uint32_t, Neighbour> nbrs;
  std::deque<Reassembly> reasm;
  std::deque<TxFrag> txfrags;
  std::vector<PendingFlood> floods;
  uint16_t next_frag_id = 1;
  uint64_t rx_window_start = 0;
  uint32_t rx_window_frames = 0;
  Bucket unknown_b, known_b, sos_unv_b, newid_b;  // per-link admission budgets
  uint64_t own_new_ms = 0;                        // we originated something new (early beacon)
  uint64_t last_tx_end_ms = 0;                    // end of the airtime of our last frame
  uint64_t digest_gen = ~0ull;                    // cached root digest
  uint32_t digest_count = 0, digest_sum = 0;
  // Shared slow channel etiquette (contended(): half duplex below 20 kbit/s).
  uint64_t heard_ms = 0;          // last frame received from someone else on this link
  uint64_t holdoff_for = 0;       // heard_ms the current random backoff was drawn for
  double holdoff_r = 0;           // that backoff, 0..1
  uint64_t new_nbr_ms = 0;        // a neighbour we had not heard before appeared
  uint64_t self_ann_ms = 0;       // last re-send of our own announcement answering a WHOIS
  uint64_t disc_due_ms = 0;       // discovery announcement for new neighbours (0 = none pending)
  uint64_t disc_for_ms = 0;       // first_ms of the newest neighbour it is for
  uint64_t disc_last_ms = 0;      // last discovery announcement on this link
  std::vector<Repair> repairs;
};

struct PeerRec {
  PeerInfo info;
  bool pair_ok = false;
  crypto::Key32 pair{};
  uint64_t hops_ms = 0;
  uint64_t last_whois_ms = 0;
  uint64_t announce_created = 0;
  uint64_t time_sample_ms = 0;
};

struct PendingRx {
  Bytes raw;
  uint32_t link = 0;
  RxMeta meta;
  uint64_t received_ms = 0;
  bool from_bundle = false;
};

struct OutMsg {
  uint64_t wire = 0;
  NodeId dest{};
  bool reliable = false;
  proto::AckSecret secret{};
  uint64_t next_retry_ms = 0;
  uint32_t interval_ms = 0;
  uint8_t attempts = 0;
  MsgStatus status = MsgStatus::Queued;
  uint64_t expires_ms = 0;
  uint64_t created_ms = 0;
};

struct AwaitKey {
  MsgId id;
  NodeId to{};
  Kind kind = Kind::Text;
  Bytes body;
  SendOptions o;
  uint64_t created_ms = 0;
  uint64_t expires_ms = 0;
};

struct FileRx {
  NodeId origin{};
  uint32_t file_id = 0;
  uint16_t count = 0;
  bool has_meta = false;
  uint32_t total = 0;
  std::string name, mime;
  std::array<uint8_t, 16> hash{};
  std::vector<Bytes> chunks;
  std::vector<bool> have;
  uint16_t got = 0;
  size_t bytes = 0;
  RxInfo info0;
  bool want_ack = false;
  proto::AckCommit ack_commit{};
  uint64_t last_ms = 0;
};

struct TimeSample {
  NodeId from{};
  int64_t offset_ms = 0;  // their mesh time - our host clock
  uint8_t quality = 15;
  uint64_t at_ms = 0;
  bool pinned = false;    // from a contact whose keys the user added
};

// Sane range for any clock value we accept from the network or from saved state: 2024 .. 2100.
constexpr uint64_t kMinSaneMs = 1704067200000ull;
constexpr uint64_t kMaxSaneMs = 4102444800000ull;

// Opened (authenticated / decrypted) view of a packet ready for delivery.
struct Opened {
  Bytes body;
  bool encrypted = false;
  bool authenticated = false;
  bool for_me = false;
  std::string channel_name;
};

struct MeshEngine::Impl {
  Impl(const Identity& id, ByteView entropy, const MeshConfig& c);

  // --- state ---------------------------------------------------------------------------------
  Identity id;
  NodeId me{};
  uint32_t my_hop = 0;
  MeshConfig cfg;
  crypto::Csprng rng;
  MeshDelegate* del = nullptr;
  MeshPersistence* per = nullptr;
  Log log;
  std::string name, callsign, status_text;

  uint64_t host_now = 0;   // monotonic timer clock: advances by the host clock's forward steps only
  uint64_t host_raw = 0;   // last host clock value passed to tick() (non-zero once known)
  bool started = false;
  uint64_t start_host_ms = 0;
  int64_t offset_ms = 0;  // mesh = host + offset
  TimeSource tsource = TimeSource::System;
  uint8_t tquality = 8;
  std::vector<TimeSample> tsamples;
  uint64_t last_time_adjust = 0;  // host_now of the last mesh-time correction

  std::map<uint32_t, LinkSlot> links;
  DtnStore store;
  KeyedMsgIdHash mid_hash;    // per-engine random SipHash key
  RecentSet seen{8192};       // wire ids (already a BLAKE2b hash of the whole packet)
  ReplayGuard delivered;      // MsgIds handed to the delegate
  std::unordered_map<uint32_t, uint16_t> seen_sids;  // sync ids of `seen` (multiset counts)
  std::unordered_map<NodeId, PeerRec, NodeIdHash> peers;
  std::unordered_map<uint32_t, proto::ChannelKey> channels;  // by id (encrypted or open)
  std::unordered_map<NodeId, std::pair<double, uint64_t>, NodeIdHash> rate;  // tokens, last ms
  std::deque<PendingRx> pending;
  std::map<MsgId, OutMsg> out;
  std::deque<MsgId> out_order;
  std::vector<AwaitKey> await_key;
  std::map<uint32_t, std::pair<NodeId, uint64_t>> pings;
  std::map<std::pair<uint64_t, uint32_t>, FileRx> files;
  MsgFifo<uint64_t> acked_v;   // MsgId -> ack commit, verified (matched a packet we held / sent / acked)
  MsgFifo<uint64_t> acked_u;   // unverified acks (cannot displace verified ones)
  MsgFifo<uint64_t> reack_ms;  // MsgId -> last re-ack time

  // periodic work
  uint64_t next_announce_ms = 0;
  bool first_announce_scheduled = false;
  uint64_t last_announce_ms = 0;
  uint64_t next_gc_ms = 0;
  uint64_t whois_window_ms = 0;
  uint32_t whois_window_count = 0;
  bool sos_on = false;
  uint64_t sos_since = 0, sos_next = 0;
  uint32_t sos_interval_ms = 60000;
  uint16_t sos_seq = 0;
  uint32_t sos_count = 0;
  std::string sos_text;
  std::optional<Position> sos_pos;
  proto::NetPlanBody netplan;
  uint32_t netplan_interval_ms = 0;
  uint64_t netplan_next = 0;

  MeshStats st;
  Bucket g_unknown, g_sos_unv, g_newid;  // global admission budgets
  Bucket g_known;                        // known-but-unverifiable, across all links (review L8)
  size_t push_bytes_left_ = 0;            // sync push budget of the message being handled

  // --- helpers (engine.cpp) -------------------------------------------------------------------
  uint64_t mesh_now() const {
    int64_t m = static_cast<int64_t>(host_now) + offset_ms;
    return m > 0 ? static_cast<uint64_t>(m) : 0;
  }
  uint32_t new_pkt_id() { uint32_t v; do { v = rng.u32(); } while (v == 0); return v; }
  uint32_t lifetime_for(Kind k, uint32_t requested_s) const;
  PeerRec& peer_rec(const NodeId& n, bool* is_new = nullptr);
  void evict_peers();
  bool peer_pair_key(PeerRec& p);
  void notify_peer(const PeerRec& p, bool is_new);
  void set_status(const MsgId& m, MsgStatus s);
  void track_out(const MsgId& m, const OutMsg& o);
  bool ack_known(const MsgId& m, uint64_t commit) const;
  void remember_delivery(const MsgId& m, uint64_t expires_ms, bool auth);
  void dirty();

  // building and originating packets
  proto::Header base_header(Kind k, const Dest& d, const SendOptions& o, uint32_t lifetime_s);
  SendResult originate(Kind k, const Dest& d, ByteView body, const SendOptions& o, uint32_t lifetime_s = 0,
                       uint32_t fixed_pkt_id = 0);
  bool seal_for(proto::Header& h, ByteView body, Bytes& out, std::string* err, bool* need_key);
  void inject_own(Bytes&& raw, const proto::PacketView& pv, uint64_t expires_ms, bool reliable,
                  const proto::AckSecret& secret);
  void send_whois(const NodeId& target);
  void send_ack(const NodeId& to, uint32_t pkt_id, const proto::AckSecret& s, uint64_t expires_ms);
  void announce(int prio = -1);
  void flush_await_key(const NodeId& n);

  // store
  bool store_put(const proto::PacketView& pv, Bytes raw, uint64_t received_ms, uint64_t expires_ms, uint8_t flags,
                 uint8_t trust = kTrustVerified);
  uint8_t classify_stored(const proto::PacketView& pv);
  void upgrade_origin(const NodeId& origin);
  bool admit(uint8_t trust, uint32_t link_id, uint32_t hop_src, bool from_bundle);
  bool admit_new_identity(uint32_t link_id, bool from_bundle);
  bool admit_unverified_sos(uint32_t link_id);
  void store_remove(uint64_t wire);
  uint64_t replace_key(const proto::Header& h) const;
  void gc(uint64_t now);

  // receive path (rx.cpp)
  void on_frame(uint32_t link_id, ByteView frame, const RxMeta& meta);
  void handle_unit(LinkSlot& L, uint32_t hop, ByteView unit, const RxMeta& meta);
  void handle_fragment(LinkSlot& L, uint32_t hop, ByteView body, const RxMeta& meta);
  void handle_control(LinkSlot& L, uint32_t hop, ByteView body);
  // Returns 1 = accepted new, 0 = duplicate, -1 = rejected.
  int handle_packet(ByteView raw, uint32_t link_id, const RxMeta& meta, bool from_bundle, bool via_sync,
                    uint32_t hop_src = 0);
  bool open_packet(const proto::PacketView& pv, Opened& o, bool* need_key);
  void deliver(const proto::PacketView& pv, const Opened& o, uint32_t link_id, const RxMeta& meta,
               bool from_bundle, uint64_t received_ms, uint64_t expires_ms);
  bool handle_ack_packet(const proto::PacketView& pv);  // true: verified against a packet we know
  bool ack_verifiable(const proto::PacketView& pv) const;
  void handle_announce(const proto::PacketView& pv, uint32_t link_id, const RxMeta& meta, bool* ok, uint32_t hop_src = 0);
  void handle_whois(const proto::PacketView& pv, uint32_t link_id);
  void process_pending(const NodeId& origin);
  void expire_pending(uint64_t now);
  bool rate_ok(const NodeId& origin);
  void note_heard(const proto::PacketView& pv, uint32_t link_id, const RxMeta& meta);
  void file_chunk(const RxInfo& rx, const proto::FileBody& fb, const proto::Header& h);
  uint64_t compute_expiry(const proto::Header& h) const;

  // transmit path (tx.cpp)
  static uint8_t link_prio(const proto::Header& h);
  bool allowed_on(const LinkSlot& L, const proto::Header& h) const;
  bool in_scope(const LinkSlot& L, const proto::Header& h) const;
  bool reachable_elsewhere(const LinkSlot& L) const;
  void mark_proven(uint32_t link_id, uint32_t hop_src, const NodeId& node) {
    auto lit = links.find(link_id);
    if (lit == links.end()) return;
    auto nb = lit->second.nbrs.find(hop_src);
    if (nb == lit->second.nbrs.end()) return;
    nb->second.proven = true;
    nb->second.proven_node = node;
  }
  bool from_relay_session(uint32_t link_id) const {
    auto it = links.find(link_id);
    return it != links.end() && it->second.scope.relay_session;
  }
  bool can_transmit(LinkSlot& L);
  bool contended(const LinkSlot& L) const {
    return cfg.slow_channel_etiquette && L.caps.half_duplex && L.caps.bitrate_bps < 20000;
  }
  // Earliest time a frame of link priority `prio` may be handed to the driver after hearing the
  // channel busy (0 = no restriction).
  uint64_t holdoff_until(LinkSlot& L, uint8_t prio);
  void repair_timers(LinkSlot& L, uint64_t now);
  void discovery_timers(LinkSlot& L, uint64_t now);
  void evidence_of(LinkSlot& L, uint64_t wire);
  bool need_announce() const;
  int force_prio = -1;  // flood_own: override the link priority (announce piggy-backed on a message)
  bool enqueue_unit(LinkSlot& L, uint8_t inner_type, ByteView content, uint8_t prio, uint64_t wire, uint32_t own_pkt,
                    uint64_t expire_ms);
  bool enqueue_frame(LinkSlot& L, Bytes&& frame, uint8_t prio, uint64_t wire, uint32_t own_pkt, uint64_t expire_ms);
  void send_packet_on(LinkSlot& L, const StoreEntry& e, uint8_t prio, bool own);
  bool send_raw_packet_on(LinkSlot& L, ByteView raw, const proto::Header& h, uint8_t prio, uint64_t wire, bool own);
  void flood_own(const StoreEntry& e);
  void schedule_flood(const proto::PacketView& pv, uint64_t wire, uint32_t from_link, const RxMeta& meta);
  void heard_duplicate(uint64_t wire, uint32_t link_id);
  void run_floods(uint64_t now);
  void pump(LinkSlot& L, uint64_t now);
  uint32_t airtime_ms(const LinkSlot& L, size_t bytes) const;
  void account_airtime(LinkSlot& L, uint32_t ms, uint64_t now);
  void send_control(LinkSlot& L, ByteView body, uint8_t prio);
  void handle_frag_nack(LinkSlot& L, const proto::FragNack& n);
  void reasm_timers(LinkSlot& L, uint64_t now);

  // beacons, time and sync (sync.cpp)
  void beacon_timers(LinkSlot& L, uint64_t now);
  void send_beacon(LinkSlot& L);
  void handle_beacon(LinkSlot& L, uint32_t hop, const proto::Beacon& b);
  void time_sample(const NodeId& from, uint64_t their_ms, uint8_t quality, bool pinned);
  bool sync_eligible(const LinkSlot& L, const StoreEntry& e) const;
  void digest_children(const LinkSlot& L, uint8_t bits, uint32_t prefix, std::array<proto::SyncDigest, 16>& res) const;
  void root_digest(const LinkSlot& L, uint32_t& count, uint32_t& sum) const;
  void ids_in(const LinkSlot& L, uint8_t bits, uint32_t prefix, std::vector<uint32_t>& res) const;
  void start_sync(LinkSlot& L, Neighbour& n);
  void handle_sync(LinkSlot& L, uint32_t hop, const proto::SyncMsg& m);
  void push_sid(LinkSlot& L, uint32_t sid, uint32_t& budget);
};

}  // namespace ember::mesh
