// EmberMesh core — the mesh engine: routing, store-carry-forward, reliability, fragmentation,
// airtime budgeting and end-to-end security for one node. Wire format: docs/PROTOCOL.md.
//
// Ownership & wiring (the `node` facade does this):
//
//   MeshEngine eng(identity, entropy);
//   eng.set_delegate(&my_delegate);           // app-level deliveries / status / peers
//   eng.set_persistence(&my_store);           // optional: persist the DTN store
//   eng.attach_link(id, link_ptr);            // node owns Link objects; engine calls send()/caps()/state()
//   ... LinkHost::link_frame_received(link, frame, meta)  ->  eng.on_frame(id, frame, meta)
//   eng.tick(now_unix_ms);                    // every 20-50 ms and after input; next_wakeup_ms() for idle
//
// Everything is single-threaded, deterministic (time and randomness injected) and bounded: every
// table has a hard cap and hostile input can neither crash the engine nor grow memory unbounded.
//
// Message ids (MsgId) are origin node id + 32-bit packet id; MsgId::to_hex() gives the 24-char
// lowercase id used by the RPC layer for messages and bulletins.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/json.h"
#include "base/log.h"
#include "crypto/identity.h"
#include "ember/bytes.h"
#include "ember/link.h"
#include "proto/packet.h"
#include "proto/payload.h"
#include "proto/secure.h"

namespace ember::mesh {

using proto::Kind;
using proto::MsgId;
using proto::Position;
using proto::Prio;

enum class MsgStatus : uint8_t { Unknown = 0, Queued, Sent, Relayed, Delivered, Read, Failed };
const char* msg_status_name(MsgStatus s);  // "queued","sent","relayed","delivered","read","failed"

enum class TimeSource : uint8_t { System = 0, Gps = 1, Mesh = 2, Manual = 3 };
const char* time_source_name(TimeSource s);  // "system","gps","mesh","manual"

struct Dest {
  enum Type : uint8_t { Broadcast = 0, Node = 1, Channel = 2 } type = Broadcast;
  NodeId node{};
  uint32_t channel = 0;
  static Dest broadcast() { return Dest(); }
  static Dest to_node(const NodeId& n) { Dest d; d.type = Node; d.node = n; return d; }
  static Dest to_channel(uint32_t id) { Dest d; d.type = Channel; d.channel = id; return d; }
  bool operator==(const Dest& o) const {
    return type == o.type && (type != Node || node == o.node) && (type != Channel || channel == o.channel);
  }
};

struct SendOptions {
  Prio prio = Prio::Normal;
  bool want_ack = true;        // only meaningful for Dest::Node
  uint8_t ttl_hops = 0;        // 0 = config default; 1 = direct neighbours only; max 15
  uint32_t lifetime_s = 0;     // 0 = per-kind default; capped by MeshConfig::max_lifetime_s
};

struct SendResult {
  bool ok = false;
  MsgId id;
  MsgStatus status = MsgStatus::Unknown;
  // ok && waiting_for_key: the recipient's public keys are unknown; the message is held (status
  // Queued) and a WHOIS is broadcast; it is sealed and sent as soon as the keys arrive.
  bool waiting_for_key = false;
  std::string error;  // "unknown_channel", "too_large", "bad_params", "no_identity", "busy",
                      // "not_started" (call tick() once before sending: it sets the clock)
};

// Information about a delivered (authenticated or explicitly unauthenticated) payload.
struct RxInfo {
  MsgId id;
  Kind kind = Kind::Text;
  NodeId from{};
  Dest dest;
  Prio prio = Prio::Normal;
  uint64_t created_ms = 0;   // sender's mesh time (Unix ms)
  uint64_t received_ms = 0;  // our mesh time at reception
  uint64_t expires_ms = 0;
  uint8_t hops = 0;          // relays between sender and us (0 = heard directly)
  uint32_t via_link = 0;     // 0 = bundle import / restored
  bool encrypted = false;    // was end-to-end encrypted (DM or private channel)
  bool authenticated = false;  // signature / MAC / AEAD verified with the sender's key
  bool from_bundle = false;
  bool has_rssi = false, has_snr = false;
  float rssi_dbm = 0, snr_db = 0;
  std::string channel_name;  // for channel messages
};

struct PeerLink {
  uint32_t link_id = 0;
  bool has_rssi = false, has_snr = false;
  float rssi_dbm = 0, snr_db = 0;
  uint64_t last_rx_ms = 0;
};

// What may cross a link (internet relay sessions, docs/CONNECTIVITY.md §4.7). An unrestricted link
// (the default) carries everything the other rules allow. A restricted link carries, in both
// directions, only: direct packets between us and `peer`; packets of `channel`; ANNOUNCE and SOS
// broadcasts originated by us or by `peer`. Reconciliation on it covers only those packets, and
// received packets outside the scope are dropped. Hop-local control frames are not affected.
struct LinkScope {
  bool restricted = false;
  // An internet relay session (docs/CONNECTIVITY.md §4): sends nothing while its contact is heard
  // directly on another link (fallback), beacons every 30 min, small reconciliation pushes, and
  // nothing received on it is ever forwarded to amateur-radio links.
  bool relay_session = false;
  bool has_peer = false;
  NodeId peer{};
  bool has_channel = false;
  uint32_t channel = 0;
};

struct PeerInfo {
  NodeId id{};
  bool has_keys = false;
  PublicKeys keys;
  std::string name, callsign, status;
  bool relay = true;
  bool direct = false;   // heard on a link within the last 15 minutes
  uint8_t hops = 0xFF;   // fewest relays seen recently (0xFF = unknown)
  uint64_t last_seen_ms = 0;
  std::vector<PeerLink> links;
  std::optional<Position> position;
  uint64_t position_ms = 0;
  bool pinned = false;   // keys added by the app (contact); never evicted
};

struct LinkStats {
  uint32_t link_id = 0;
  uint64_t tx_frames = 0, rx_frames = 0, tx_bytes = 0, rx_bytes = 0, rx_bad = 0;
  uint64_t airtime_ms = 0;
  uint32_t duty_used_permille = 0;  // over the last 10 minutes
  uint32_t queue = 0;               // frames waiting in the engine's priority queues
  uint32_t queue_bytes = 0;
  uint64_t dropped_queue_full = 0, blocked_legal = 0, blocked_no_callsign = 0;
  uint64_t last_rx_ms = 0;
  uint32_t neighbours = 0;
  uint64_t repairs = 0;             // own fresh packets re-sent for lack of evidence (slow links)
};

struct MeshStats {
  uint32_t stored_messages = 0;
  uint64_t stored_bytes = 0;
  uint64_t relayed = 0;             // packets we rebroadcast / bridged
  uint64_t dropped_duplicates = 0;
  uint64_t dropped_ttl = 0;         // could not be relayed further (hop limit reached)
  uint64_t dropped_expired = 0;
  uint64_t dropped_rate = 0;
  uint64_t dropped_auth = 0;        // forged / failed verification / undecryptable
  uint64_t dropped_scope = 0;       // arrived on a restricted link but outside its scope
  uint64_t suppressed = 0;          // rebroadcasts cancelled because neighbours already relayed
  uint64_t tx_frames = 0, rx_frames = 0, rx_bad = 0;
  uint64_t delivered = 0;           // payloads handed to the delegate
  uint64_t acks_received = 0, retries = 0;
  uint64_t sync_rounds = 0, sync_pushed = 0;
  uint64_t store_evicted = 0;
  uint64_t uptime_ms = 0;
  std::vector<LinkStats> per_link;
};

struct TimeInfo {
  uint64_t mesh_ms = 0;
  TimeSource source = TimeSource::System;
  uint8_t quality = 8;  // 0 best .. 15 unknown
};

// Metadata persisted with every stored packet.
struct StoredMeta {
  uint64_t received_ms = 0;
  uint64_t expires_ms = 0;
  uint8_t flags = 0;  // kStoredOwn | kStoredTransmittable
};
constexpr uint8_t kStoredOwn = 1;
constexpr uint8_t kStoredTransmittable = 2;
constexpr uint8_t kStoredUnverified = 4;  // origin unknown when stored (lowest storage class)
constexpr uint8_t kStoredViaRelay = 8;    // received over an internet relay: never put on amateur links

class MeshDelegate {
 public:
  virtual ~MeshDelegate() = default;
  virtual void on_text(const RxInfo& rx, const proto::TextBody& body) {}
  virtual void on_position(const RxInfo& rx, const Position& pos) {}
  virtual void on_sos(const RxInfo& rx, const proto::SosBody& body) {}
  virtual void on_sos_ack(const RxInfo& rx, const proto::SosAckBody& body) {}  // someone heard our SOS
  virtual void on_checkin(const RxInfo& rx, const proto::CheckinBody& body) {}
  // Includes withdrawals (body.is_delete; delete_pkt_id refers to MsgId{rx.from, id}).
  virtual void on_bulletin(const RxInfo& rx, const proto::BulletinBody& body) {}
  virtual void on_netplan(const RxInfo& rx, const proto::NetPlanBody& body) {}
  virtual void on_announce(const RxInfo& rx, const proto::AnnounceBody& body) {}
  // file = id of chunk 0 (the message id of the file).
  virtual void on_file_progress(const MsgId& file, const NodeId& from, uint32_t received, uint32_t total) {}
  virtual void on_file(const RxInfo& rx, const std::string& name, const std::string& mime, const Bytes& data) {}
  virtual void on_app(const RxInfo& rx, uint16_t app_type, ByteView data) {}
  virtual void on_read(const NodeId& reader, const MsgId& msg) {}
  virtual void on_status(const MsgId& msg, MsgStatus status) {}
  virtual void on_peer(const PeerInfo& peer, bool is_new) {}
  virtual void on_pong(const NodeId& from, uint32_t rtt_ms, uint8_t hops) {}
  virtual void on_time(uint64_t mesh_ms, TimeSource source) {}
};

// Optional persistence hook for the DTN store. The engine calls these synchronously; a storage
// layer writes them out (append log, files ...). On start-up feed stored packets back with
// MeshEngine::restore_packet() and the opaque state with load_state().
class MeshPersistence {
 public:
  virtual ~MeshPersistence() = default;
  virtual void store_put(uint64_t wire_id, ByteView packet, const StoredMeta& meta) = 0;
  virtual void store_remove(uint64_t wire_id) = 0;
  // Engine state (time offset, outgoing reliability state, SOS ...) changed: call save_state().
  virtual void state_dirty() {}
  // Replay memory: an authenticated delivery of `msg` must never be delivered again before
  // `keep_until_ms` (mesh time). Persist and feed back with MeshEngine::restore_replay().
  virtual void replay_put(const MsgId& msg, uint64_t keep_until_ms) { (void)msg; (void)keep_until_ms; }
  virtual void replay_remove(const MsgId& msg) { (void)msg; }
};

struct MeshConfig {
  uint8_t default_ttl = 7;
  bool relay = true;                       // forward other nodes' packets (settings.relay_enabled)
  size_t store_max_bytes = 4u << 20;       // DTN storage quota
  size_t store_max_packets = 5000;
  uint32_t default_lifetime_s = 3 * 86400; // store_forward_days
  uint32_t max_lifetime_s = 30 * 86400;
  uint32_t announce_interval_s = 1800;     // 0 = never re-announce automatically
  uint32_t ack_retry_base_ms = 20000;
  uint32_t ack_retry_max_ms = 30 * 60000;
  uint8_t ack_max_retries = 6;
  size_t max_file_bytes = 128 * 1024;
  size_t file_chunk_bytes = 480;
  size_t dedup_capacity = 8192;
  uint32_t origin_rate_per_min = 120;      // per-origin packet budget (relaying & storing)
  uint32_t origin_burst = 300;
  uint8_t suppress_threshold = 2;          // cancel a pending rebroadcast after hearing this many copies
  size_t link_queue_max_bytes = 64 * 1024;
  bool sync_enabled = true;                // store reconciliation with neighbours
  bool beacons_enabled = true;
  uint32_t sync_push_limit = 48;           // packets pushed per sync message
  // Shared slow half-duplex channel etiquette (docs/PROTOCOL.md "Slow shared channels"): listen-
  // before-talk holdoff after hearing a frame, announcement piggy-backed on a first message, fast
  // bounded repair of own fresh packets, no WHOIS on a stranger's beacon. Off = previous behaviour
  // (kept for comparison tests).
  bool slow_channel_etiquette = true;
};

struct BundleFilter {
  enum Scope : uint8_t { All = 0, ToDest = 1, ForNode = 2 } scope = All;
  Dest dest;           // ToDest: packets addressed to this destination (conversation)
  NodeId node{};       // ForNode: packets addressed to, or of interest to, this node (DMs to it + broadcasts)
  uint64_t since_ms = 0;  // created at/after (mesh time)
  size_t max_bytes = 0;   // 0 = unlimited
};

struct BundleImportResult {
  uint32_t imported = 0;
  uint32_t duplicates = 0;
  uint32_t for_me = 0;
  uint32_t rejected = 0;
  bool ok = false;  // container was valid
};

class MeshEngine {
 public:
  // `entropy` seeds the engine's private CSPRNG (>= 32 bytes recommended).
  MeshEngine(const Identity& identity, ByteView entropy, const MeshConfig& config = MeshConfig());
  ~MeshEngine();
  MeshEngine(const MeshEngine&) = delete;
  MeshEngine& operator=(const MeshEngine&) = delete;

  void set_delegate(MeshDelegate* d);
  void set_persistence(MeshPersistence* p);
  void set_log(const Log& log);
  void add_entropy(ByteView entropy);
  const MeshConfig& config() const;
  void set_config(const MeshConfig& c);
  const Identity& identity() const;
  NodeId node_id() const;

  // Profile carried in announcements. Changing it triggers an announcement.
  void set_profile(std::string_view name, std::string_view callsign, std::string_view status);
  const std::string& callsign() const;

  // --- time --------------------------------------------------------------------------------------
  // now_ms: host wall clock (may be wrong; the engine tolerates jumps).
  void tick(uint64_t now_ms);
  uint64_t now_ms() const;  // mesh time (Unix ms)
  TimeInfo time_info() const;
  void set_time(uint64_t unix_ms, TimeSource source);  // gps / manual / system fix
  uint32_t next_wakeup_ms() const;                       // 0..60000

  // --- links -------------------------------------------------------------------------------------
  // link_id must be non-zero and unique. The engine never deletes the Link.
  bool attach_link(uint32_t link_id, Link* link);
  // Sets the scope of an attached link (no effect for unknown ids; reset on detach).
  void set_link_scope(uint32_t link_id, const LinkScope& scope);
  // Neighbours of a link whose identity is proven (verified ANNOUNCE with hops 0 from that hop id, or a
  // direct pairwise-authenticated packet); never from unauthenticated beacons. Used for nearby duplicates.
  std::vector<NodeId> proven_neighbours(uint32_t link_id) const;
  void detach_link(uint32_t link_id);
  void on_frame(uint32_t link_id, ByteView frame, const RxMeta& meta);

  // --- keys, contacts, channels -------------------------------------------------------------------
  // Pins a contact's keys (from a contact card). Returns false if the keys do not match a node id
  // already known with different keys (impossible unless forged).
  bool add_known_keys(const PublicKeys& keys, std::string_view name = "", std::string_view callsign = "");
  bool has_keys(const NodeId& id) const;
  void forget_peer(const NodeId& id);
  void join_channel(const proto::ChannelKey& ch);
  void leave_channel(uint32_t channel_id);
  bool in_channel(uint32_t channel_id) const;

  // --- sending -----------------------------------------------------------------------------------
  SendResult send_text(const Dest& to, std::string_view text, const SendOptions& o = SendOptions(),
                       const std::optional<MsgId>& reply_to = std::nullopt);
  SendResult send_position(const Dest& to, const Position& pos, const SendOptions& o = SendOptions());
  // Starts a repeating emergency beacon (interval_s >= 10). Re-calling updates text/position.
  SendResult sos_start(std::string_view text, const std::optional<Position>& pos, uint32_t interval_s = 60);
  SendResult sos_stop();  // broadcasts a cancellation
  bool sos_active() const;
  uint64_t sos_since_ms() const;
  uint32_t sos_sent() const;
  SendResult sos_ack(const NodeId& sos_origin, std::string_view text = "", uint32_t sos_pkt_id = 0);
  SendResult send_checkin(const proto::CheckinBody& body, const SendOptions& o = SendOptions());
  SendResult post_bulletin(const proto::BulletinBody& body, uint32_t lifetime_s = 0);
  SendResult delete_bulletin(uint32_t pkt_id);
  SendResult send_file(const Dest& to, std::string_view name, std::string_view mime, ByteView data,
                       const SendOptions& o = SendOptions());
  // Announces the plan now and then every interval_s (0 = once). Empty plan stops re-announcing.
  SendResult set_netplan(const proto::NetPlanBody& plan, uint32_t interval_s = 1800);
  SendResult send_read_receipt(const NodeId& to, const std::vector<uint32_t>& pkt_ids);
  SendResult send_app(const Dest& to, uint16_t app_type, ByteView data, const SendOptions& o = SendOptions());
  bool ping(const NodeId& node);
  void announce_now();
  bool retry(const MsgId& id);  // re-flood now and restart the retry schedule
  MsgStatus status(const MsgId& id) const;
  bool cancel(const MsgId& id);  // stop retrying and drop our stored copy

  // --- queries -----------------------------------------------------------------------------------
  std::vector<PeerInfo> peers() const;
  std::optional<PeerInfo> peer(const NodeId& id) const;
  MeshStats stats() const;
  std::vector<uint32_t> link_ids() const;

  // --- bundles (sneakernet) ----------------------------------------------------------------------
  Bytes export_bundle(const BundleFilter& f, uint32_t* count = nullptr) const;
  BundleImportResult import_bundle(ByteView data);

  // --- persistence -------------------------------------------------------------------------------
  // Re-insert a persisted packet at start-up (no delivery, no flooding). Returns false if invalid.
  bool restore_packet(ByteView packet, const StoredMeta& meta);
  void restore_replay(const MsgId& msg, uint64_t keep_until_ms);
  Json save_state() const;
  bool load_state(const Json& state);

 private:
  struct Impl;
  std::unique_ptr<Impl> d_;
};

}  // namespace ember::mesh
