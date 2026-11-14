// EmberMesh node — the application layer: turns the core modules into the app.
//
// One Node owns: identity, CSPRNG, the (optionally encrypted) KvStore, the MeshEngine, every Link
// driver (it is their LinkHost), the guide library, settings and all app state (contacts, channels,
// conversations, messages, files, SOS, check-ins, bulletins, net plans, kv). It implements every
// core method and event of docs/RPC_API.md. The C API (core/src/capi) wraps one Node per
// ember_node and adds the lock; the Node itself is single-threaded and exception-free.
//
// Time comes only from tick(now_ms). The node keeps a monotonic clock for its own timers (host
// clock jumps backward are ignored for those) and hands the raw host clock to the engine, which
// tracks mesh time.
//
// Storage layout (KvStore namespaces; binary keys, JSON values unless noted):
//   meta      identity (secret, binary), profile, settings, engine (save_state), next_link_id,
//             sos (own beacon), sos_acks, position, netplan (own), security
//   contacts  node_id(8)            → contact record (incl. public keys when known)
//   channels  "<id>"                → channel record (incl. key for passphrase channels)
//   conv      "<conv>"              → conversation meta (unread, last_ts, pending read receipts…)
//   msg       conv \0 be64(ts) be64(seq) id12 → Message (exact RPC shape)
//   mid       id12                  → msg key        (message id index, also retry aliases)
//   mexp      be64(expires) id12    → ""             (expiring messages)
//   files     id12                  → file bytes (binary);  fage: be64(ts) id12 → "" (eviction)
//   dtn       be64(wire_id)         → u64 received | u64 expires | u8 flags | packet  (binary)
//   links     be32(id)              → {id,type,name,config,endpoint}
//   bulletins id12, sos node_id(8), checkins node_id(8), netplans node_id(8)
//   ukv       ns \0 key             → JSON value (kv.* methods)
#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/json.h"
#include "crypto/identity.h"
#include "crypto/rng.h"
#include "ember/bytes.h"
#include "ember/link.h"
#include "guides/library.h"
#include "mesh/engine.h"
#include "morse/rx.h"
#include "node/rpc_util.h"
#include "node/store_crypto.h"
#include "store/kv_store.h"
#include "store/vfs.h"

namespace ember::node {

struct NodeConfig {
  std::string storage_dir;  // "" = in memory
  std::string platform = "desktop";
  std::string app_version;
  std::string device_name;
  uint64_t now_ms = 0;
  // Internal knobs (config JSON keys of the same name; for tests and small devices).
  crypto::KdfParams kdf;                     // storage passphrase / identity backup KDF
  size_t max_messages = 20000;               // message history cap (all conversations)
  size_t max_conversations = 5000;           // DM / interop conversation table
  uint64_t max_message_bytes = 24u << 20;    // keys + values in "msg"
  uint64_t max_file_bytes_stored = 16u << 20;
  size_t max_outbox_items = 8192;
  size_t max_outbox_bytes = 16u << 20;

  static NodeConfig from_json(const Json& j);
};

enum : int { kOutEvent = 1, kOutLinkTx = 2 };

struct OutItem {
  uint64_t seq = 0;  // global order across the event and TX queues
  int kind = 0;
  uint32_t link_id = 0;
  std::string data;  // event JSON (NUL terminated by std::string) or link bytes
};

class Node final : public LinkHost, public mesh::MeshDelegate, public mesh::MeshPersistence {
 public:
  Node(const NodeConfig& cfg, ByteView entropy);
  ~Node() override;
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  // --- C API surface ----------------------------------------------------------------------------
  std::string call(std::string_view method, std::string_view params_json);  // JSON envelope
  RpcResult call_json(std::string_view method, const Json& params);
  void tick(uint64_t now_ms);
  uint32_t next_wakeup_ms();
  void link_rx(uint32_t link_id, ByteView data);
  void audio_rx(uint32_t link_id, const int16_t* samples, size_t count, uint32_t rate);
  size_t audio_tx(uint32_t link_id, int16_t* out, size_t max, uint32_t rate);
  void add_entropy(ByteView e);
  int guides_load(const uint8_t* pack, size_t len, bool copy);
  // Pops the next outbox item; *out stays valid until the next poll() or destruction.
  bool poll(const OutItem** out);
  size_t outbox_size() const { return out_.size() + out_tx_.size(); }

  // --- LinkHost ---------------------------------------------------------------------------------
  void link_frame_received(Link& link, ByteView frame, const RxMeta& meta) override;
  void link_io_write(Link& link, ByteView bytes) override;
  void link_interop_text(Link& link, const InteropText& msg) override;
  void link_event(Link& link, const char* name, const std::string& json_object) override;
  // Drivers (and the engine) get the node's monotonic clock: see tick().
  uint64_t now_ms() const override { return clock_base_ + mono_; }
  void random_bytes(uint8_t* out, size_t n) override { rng_.fill(out, n); }

  // --- MeshDelegate -----------------------------------------------------------------------------
  void on_text(const mesh::RxInfo& rx, const proto::TextBody& body) override;
  void on_position(const mesh::RxInfo& rx, const proto::Position& pos) override;
  void on_sos(const mesh::RxInfo& rx, const proto::SosBody& body) override;
  void on_sos_ack(const mesh::RxInfo& rx, const proto::SosAckBody& body) override;
  void on_checkin(const mesh::RxInfo& rx, const proto::CheckinBody& body) override;
  void on_bulletin(const mesh::RxInfo& rx, const proto::BulletinBody& body) override;
  void on_netplan(const mesh::RxInfo& rx, const proto::NetPlanBody& body) override;
  void on_announce(const mesh::RxInfo& rx, const proto::AnnounceBody& body) override;
  void on_file_progress(const proto::MsgId& file, const NodeId& from, uint32_t received, uint32_t total) override;
  void on_file(const mesh::RxInfo& rx, const std::string& name, const std::string& mime, const Bytes& data) override;
  void on_read(const NodeId& reader, const proto::MsgId& msg) override;
  void on_status(const proto::MsgId& msg, mesh::MsgStatus status) override;
  void on_peer(const mesh::PeerInfo& peer, bool is_new) override;
  void on_pong(const NodeId& from, uint32_t rtt_ms, uint8_t hops) override;
  void on_time(uint64_t mesh_ms, mesh::TimeSource source) override;

  // --- MeshPersistence --------------------------------------------------------------------------
  void store_put(uint64_t wire_id, ByteView packet, const mesh::StoredMeta& meta) override;
  void store_remove(uint64_t wire_id) override;
  void state_dirty() override { eng_state_dirty_ = true; }
  void replay_put(const mesh::MsgId& msg, uint64_t keep_until_ms) override;
  void replay_remove(const mesh::MsgId& msg) override;

  // Introspection for tests.
  bool locked() const { return lock_ != Lock::Open; }
  bool engine_running() const { return eng_ != nullptr; }
  mesh::MeshEngine* engine() { return eng_.get(); }
  store::KvStore* kv() { return kv_.get(); }
  uint64_t outbox_dropped() const { return out_dropped_; }

 private:
  enum class Lock : uint8_t { Open, NoStore, Runtime };

  struct LinkRec {
    uint32_t id = 0;
    std::string type, name;
    Json config;    // as given by the caller (merged on configure)
    Json endpoint;  // opaque, echoed
    bool persist = true;
    std::unique_ptr<Link> link;  // null when the driver could not be created (state "error")
    std::string create_error;
    std::string endpoint_error;
    bool endpoint_connected = false;
    int last_state = -1;
    bool last_blocked = false;  // last link.state reported blocked:"no_callsign"
    std::string last_interop_network;
    std::deque<std::pair<std::string, std::string>> interop_pending;  // (to, message id) awaiting aprs_ack
    uint64_t act_tx = 0, act_rx = 0;  // bytes not yet reported in link.activity
    uint64_t act_last = 0;            // mono ms of the last link.activity
    uint64_t io_tx_bytes = 0, io_rx_bytes = 0;
  };

  struct ChannelRec {
    Json rec;  // {id,name,encrypted,is_default,muted,key?,members:[..]}
    proto::ChannelKey key;
  };

  using Handler = RpcResult (Node::*)(Params&);
  struct MethodDef {
    const char* name;
    Handler fn;
    uint8_t flags;
  };
  static const MethodDef* find_method(std::string_view name);

  // --- lifecycle (node.cpp) ---------------------------------------------------------------------
  void open_storage();
  bool open_store_with(std::unique_ptr<store::RecordCipher> cipher, store::Status* st);
  // "This store is encrypted" marker in storage_dir, outside the store (downgrade protection).
  bool encrypted_marker() const;
  void set_encrypted_marker(bool on);
  void set_tampered(const std::string& why);
  void reset_storage(const std::string* duress_passphrase);  // wipe + fresh store
  // Wipe + fresh store encrypted with an already derived key (duress unlock).
  void reset_storage_with_key(const crypto::Key32& key, const std::array<uint8_t, 16>& salt);
  Bytes presentation_blob() const;               // non-secret state carried by a duress unlock
  void apply_presentation(ByteView blob);
  void refresh_decoy();                          // re-seal the duress decoy after profile changes
  void load_all();          // after the store is open: identity, profile, settings, collections
  void start_engine();      // needs identity + time
  void stop_runtime(bool keep_links);
  void clear_memory_state();
  void persist_engine_state();
  void apply_settings_to_engine();
  mesh::MeshConfig mesh_config() const;
  void flush_periodic(bool force);
  void sweep();
  uint64_t mesh_now() const;

  // --- storage helpers --------------------------------------------------------------------------
  bool db_put(std::string_view ns, ByteView key, ByteView value);
  bool db_put_json(std::string_view ns, ByteView key, const Json& v);
  bool db_get(std::string_view ns, ByteView key, Bytes* out);
  bool db_get_json(std::string_view ns, ByteView key, Json* out);
  bool db_del(std::string_view ns, ByteView key);
  bool db_write(const store::WriteBatch& b);
  void mark_dirty();
  void db_sync();

  // --- outbox / events --------------------------------------------------------------------------
  void push_out(int kind, uint32_t link_id, std::string data);
  void emit(const char* name, const Json& data);
  bool event_allowed_while_locked(std::string_view name) const;

  // --- messages (node_msg.cpp) ------------------------------------------------------------------
  static Bytes msg_key(std::string_view conv, uint64_t ts, uint64_t seq, const Bytes& id12);
  static bool parse_conv(std::string_view conv, std::string* kind, std::string* rest);
  RpcResult check_conv(const std::string& conv, bool for_send);
  Json new_message(const std::string& conv, const std::string& id_hex, bool outgoing, const std::string& kind,
                   const std::string& text);
  bool store_message(const Json& msg, bool count_unread, bool emit_events);
  bool load_message(const std::string& id_hex, Json* msg, Bytes* key);
  bool save_message(const Bytes& key, const Json& msg);
  void set_message_status(const std::string& id_hex, const char* status, bool force = false);
  Json conversation_json(const std::string& conv);
  Json& conv_meta(const std::string& conv);
  bool conv_admit(const std::string& conv);  // room for a new conversation? (never deletes history)
  bool replace_unverified(const Json& msg);  // an authenticated copy supersedes an unverified one
  void save_conv_meta(const std::string& conv);
  void emit_conversation(const std::string& conv);
  std::string conv_title(const std::string& conv) const;
  std::string peer_name(const NodeId& id) const;
  void evict_messages();
  void delete_conversation_messages(const std::string& conv);
  std::string local_message_id();
  std::string conv_for_rx(const mesh::RxInfo& rx);
  bool is_blocked(const NodeId& id) const;
  void add_system_message(const std::string& conv, const std::string& text);

  // contacts / channels
  Json contact_json(const Json& rec) const;
  void emit_contact(const NodeId& id);
  Json channel_json(const ChannelRec& c);
  void emit_channel(const std::string& id);
  bool add_channel(const proto::ChannelKey& key, const std::string& display_name, Json* out);
  void ensure_public_channel();

  // --- links (node_links.cpp) -------------------------------------------------------------------
  LinkRec* link_by_ptr(const Link* l);
  LinkRec* link_by_id(uint32_t id);
  Json link_json(const LinkRec& r);
  void emit_link_state(LinkRec& r);
  void poll_link_states();
  void flush_activity(bool force);
  void persist_link(const LinkRec& r);
  void load_links();
  bool instantiate_link(LinkRec& r, std::string* error);
  const Json& link_types_cached();
  Json effective_link_config(const LinkRec& r);  // stored config + node callsign for AX.25 drivers
  // links.types with defaults adjusted to the node's `region` setting (LoRa drivers' "region").
  Json link_types_for_region();
  // The value a type's "region" select should default to for this node, or "" (keep driver default).
  std::string regional_link_default(const Json& type_entry);
  void reapply_link_callsigns();

  // --- RPC handlers -----------------------------------------------------------------------------
  // node / identity / time (node_rpc.cpp)
  RpcResult m_node_info(Params& p);
  RpcResult m_node_set_profile(Params& p);
  RpcResult m_node_set_time(Params& p);
  RpcResult m_identity_card(Params& p);
  RpcResult m_identity_export(Params& p);
  RpcResult m_identity_import(Params& p);
  Json node_info_json();
  // settings / kv / security
  RpcResult m_settings_get(Params& p);
  RpcResult m_settings_set(Params& p);
  RpcResult m_kv_get(Params& p);
  RpcResult m_kv_set(Params& p);
  RpcResult m_kv_delete(Params& p);
  RpcResult m_kv_list(Params& p);
  RpcResult m_security_status(Params& p);
  RpcResult m_security_set_passphrase(Params& p);
  RpcResult m_security_unlock(Params& p);
  RpcResult m_security_lock(Params& p);
  RpcResult m_security_set_duress(Params& p);
  RpcResult m_security_wipe(Params& p);
  // contacts / channels / conversations / messages / files / interop (node_msg.cpp)
  RpcResult m_contacts_list(Params& p);
  RpcResult m_contacts_add(Params& p);
  RpcResult m_contacts_update(Params& p);
  RpcResult m_contacts_remove(Params& p);
  RpcResult m_channels_list(Params& p);
  RpcResult m_channels_create(Params& p);
  RpcResult m_channels_invite(Params& p);
  RpcResult m_channels_join(Params& p);
  RpcResult m_channels_update(Params& p);
  RpcResult m_channels_leave(Params& p);
  RpcResult m_conversations_list(Params& p);
  RpcResult m_conversations_clear(Params& p);
  RpcResult m_messages_list(Params& p);
  RpcResult m_messages_send(Params& p);
  RpcResult m_messages_retry(Params& p);
  RpcResult m_messages_mark_read(Params& p);
  RpcResult m_messages_delete(Params& p);
  RpcResult m_messages_search(Params& p);
  RpcResult m_files_send(Params& p);
  RpcResult m_files_get(Params& p);
  RpcResult m_interop_send(Params& p);
  // emergency / mesh / bundles (node_feat.cpp)
  RpcResult m_sos_start(Params& p);
  RpcResult m_sos_stop(Params& p);
  RpcResult m_sos_status(Params& p);
  RpcResult m_sos_list(Params& p);
  RpcResult m_sos_ack(Params& p);
  RpcResult m_checkin_send(Params& p);
  RpcResult m_checkin_list(Params& p);
  RpcResult m_position_set(Params& p);
  RpcResult m_position_get(Params& p);
  RpcResult m_position_share(Params& p);
  RpcResult m_bulletins_list(Params& p);
  RpcResult m_bulletins_post(Params& p);
  RpcResult m_bulletins_delete(Params& p);
  RpcResult m_mesh_peers(Params& p);
  RpcResult m_mesh_stats(Params& p);
  RpcResult m_mesh_ping(Params& p);
  RpcResult m_mesh_announce(Params& p);
  RpcResult m_netplan_get(Params& p);
  RpcResult m_netplan_set(Params& p);
  RpcResult m_bundle_export(Params& p);
  RpcResult m_bundle_import(Params& p);
  Json sos_status_json();
  // links (node_links.cpp)
  RpcResult m_links_types(Params& p);
  RpcResult m_links_list(Params& p);
  RpcResult m_links_add(Params& p);
  RpcResult m_links_configure(Params& p);
  RpcResult m_links_remove(Params& p);
  RpcResult m_links_endpoint_state(Params& p);
  // guides / tools / rf / qr (node_lib.cpp)
  RpcResult m_guides_categories(Params& p);
  RpcResult m_guides_list(Params& p);
  RpcResult m_guides_get(Params& p);
  RpcResult m_guides_search(Params& p);
  RpcResult m_guides_related(Params& p);
  RpcResult m_guides_ask(Params& p);
  RpcResult m_guides_info(Params& p);
  RpcResult m_tools_list(Params& p);
  RpcResult m_tools_run(Params& p);
  RpcResult m_rf_bands(Params& p);
  RpcResult m_rf_radios(Params& p);
  RpcResult m_rf_hardware(Params& p);
  // morse receive (node_morse.cpp)
  RpcResult m_morse_rx_start(Params& p);
  RpcResult m_morse_rx_feed(Params& p);
  RpcResult m_morse_rx_status(Params& p);
  RpcResult m_morse_rx_reset(Params& p);
  RpcResult m_morse_rx_stop(Params& p);
  morse_rx::Session* morse_session(Params& p, RpcResult* err);
  RpcResult m_tools_qr(Params& p);

  // --- state ------------------------------------------------------------------------------------
  NodeConfig cfg_;
  crypto::Csprng rng_;

  std::unique_ptr<store::Vfs> vfs_;
  std::string dir_;
  std::unique_ptr<store::KvStore> kv_;
  Lock lock_ = Lock::Open;
  bool has_passphrase_ = false;
  bool has_duress_ = false;
  bool has_decoy_key_ = false;
  bool tampered_ = false;          // storage was tampered with (security.status.tampered)
  bool tamper_blocks_ = false;     // ... so badly that it cannot be unlocked (stays locked)
  std::string tamper_msg_;
  uint64_t dirty_defer_until_ = 0;  // storage.dirty held back until this mono time
  crypto::Key32 decoy_key_{};  // kept inside the encrypted store; re-seals the header decoy
  StorageParams sparams_;
  crypto::Key32 skey_{};  // current storage key while the store is open with a passphrase
  std::string storage_error_;

  Identity id_;
  std::unique_ptr<mesh::MeshEngine> eng_;
  Json profile_;   // {name, callsign, status}
  Json settings_;

  // clock
  bool have_time_ = false;
  uint64_t host_now_ = 0;
  uint64_t mono_ = 0;          // monotonic ms (host clock jumps backward are ignored)
  uint64_t clock_base_ = 0;    // engine / driver clock = clock_base_ + mono_ (never goes backward)
  uint64_t created_mono_ = 0;
  std::optional<std::pair<uint64_t, mesh::TimeSource>> pending_time_;

  // outbox
  std::deque<OutItem> out_;     // events
  std::deque<OutItem> out_tx_;  // link TX: never evicted to make room for events (review L7)
  size_t out_tx_bytes_ = 0;
  uint64_t out_seq_ = 0;
  size_t out_bytes_ = 0;
  OutItem current_;
  uint64_t out_dropped_ = 0;

  // persistence bookkeeping
  bool db_dirty_ = false;        // unsynced writes
  bool dirty_event_ = false;     // storage.dirty not yet emitted
  bool eng_state_dirty_ = false;
  uint64_t last_sync_ = 0, last_dirty_emit_ = 0, last_state_save_ = 0, last_sweep_ = 0;
  bool in_scan_ = false;

  // collections mirrored in memory (bounded)
  std::map<NodeId, Json> contacts_;
  std::map<std::string, ChannelRec> channels_;
  std::map<std::string, Json> conv_meta_;
  std::map<std::string, Json> bulletins_;   // id hex → Bulletin (+internal fields)
  std::map<NodeId, Json> alerts_;           // SOS alerts heard
  std::map<NodeId, Json> checkins_;
  std::map<NodeId, Json> netplans_;
  Json own_sos_;        // {text, since_ts, acked_by:[...]} while active
  Json own_netplan_;    // NetPlan or null
  Json own_position_;   // Position or null
  uint64_t last_auto_position_ = 0;
  std::map<NodeId, uint64_t> peer_event_mono_;  // rate limit peer.seen / contact.updated

  // links
  std::map<uint32_t, LinkRec> links_;
  uint32_t next_link_id_ = 1;
  Json link_types_;

  guides::GuideLibrary lib_;
  morse_rx::Sessions morse_;  // morse.rx.* receive sessions (bounded, idle-expired)
};

}  // namespace ember::node
