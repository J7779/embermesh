// EmberMesh node — lifecycle: configuration, storage (open / lock / wipe), engine start/stop,
// clock, timers, outbox, persistence delegate and the RPC entry point.
#include "node/node.h"

#include <algorithm>
#include <cstring>

#include "base/codec.h"
#include "node/node_util.h"
#include "store/mem_vfs.h"

namespace ember::node {

namespace {
constexpr uint64_t kFallbackEpochMs = 1767225600000ull;  // 2026-01-01, used when the host has no clock
constexpr uint64_t kSyncIntervalMs = 1000;
constexpr uint64_t kDirtyEventIntervalMs = 1000;
constexpr uint64_t kStateSaveIntervalMs = 5000;
constexpr uint64_t kSweepIntervalMs = 60000;
}  // namespace

NodeConfig NodeConfig::from_json(const Json& j) {
  NodeConfig c;
  c.storage_dir = j.get_string("storage_dir");
  std::string pl = j.get_string("platform", "desktop");
  if (pl.size() <= 32 && !pl.empty()) c.platform = utf8_sanitize(pl);
  c.app_version = utf8_sanitize(utf8_truncate(j.get_string("app_version"), 64));
  c.device_name = utf8_sanitize(utf8_truncate(j.get_string("device_name"), proto::kMaxName));
  int64_t now = 0;
  if (j.get_int_if("now_ms", &now) && now > 0) c.now_ms = static_cast<uint64_t>(now);
  int64_t v = 0;
  // Capped like the store header (security review L8): a store we write must be one we can open.
  if (j.get_int_if("kdf_mem_kib", &v) && v >= crypto::KdfParams::kMinMemKib && v <= kMaxHeaderMemKib)
    c.kdf.mem_kib = static_cast<uint32_t>(v);
  if (j.get_int_if("kdf_passes", &v) && v >= 1 && v <= kMaxHeaderPasses) c.kdf.passes = static_cast<uint32_t>(v);
  if (j.get_int_if("max_conversations", &v) && v >= 4 && v <= 100000) c.max_conversations = static_cast<size_t>(v);
  if (j.get_int_if("max_messages", &v) && v >= 10 && v <= 2000000) c.max_messages = static_cast<size_t>(v);
  if (j.get_int_if("max_message_bytes", &v) && v >= 4096 && v <= (int64_t(1) << 32)) c.max_message_bytes = static_cast<uint64_t>(v);
  if (j.get_int_if("max_file_bytes_stored", &v) && v >= 4096 && v <= (int64_t(1) << 32))
    c.max_file_bytes_stored = static_cast<uint64_t>(v);
  if (j.get_int_if("max_outbox_items", &v) && v >= 16 && v <= 1000000) c.max_outbox_items = static_cast<size_t>(v);
  return c;
}

Node::Node(const NodeConfig& cfg, ByteView entropy) : cfg_(cfg) {
  rng_.seed(entropy);
  settings_ = default_settings();
  profile_ = Json::object({{"name", ""}, {"callsign", ""}, {"status", ""}});
  if (cfg_.now_ms) {
    have_time_ = true;
    host_now_ = cfg_.now_ms;
    clock_base_ = cfg_.now_ms;
  }
  open_storage();
  emit("node.ready", Json::object());
}

Node::~Node() {
  if (kv_) {
    persist_engine_state();
    db_sync();
  }
  // Links reference *this as their host and the engine references the links: engine first.
  eng_.reset();
  links_.clear();
  if (kv_) kv_->close();
  crypto::wipe(skey_);
}

uint64_t Node::mesh_now() const { return eng_ ? eng_->now_ms() : host_now_; }

// --- storage -------------------------------------------------------------------------------------

namespace {
constexpr const char* kEncMarker = "ember-encrypted.marker";
}  // namespace

bool Node::encrypted_marker() const {
  uint64_t sz = 0;
  return vfs_ && vfs_->file_size(store::path_join(dir_, kEncMarker), &sz) == store::Status::Ok;
}

void Node::set_encrypted_marker(bool on) {
  if (!vfs_) return;
  std::string path = store::path_join(dir_, kEncMarker);
  if (!on) {
    vfs_->remove(path);
    vfs_->sync_dir(dir_);
    return;
  }
  if (encrypted_marker()) return;
  std::unique_ptr<store::VfsFile> f;
  if (vfs_->open(path, store::OpenMode::CreateTruncate, &f) == store::Status::Ok) {
    f->append(ByteView(std::string_view("EMBER-ENCRYPTED-STORE v1\n")));
    f->sync();
  }
  vfs_->sync_dir(dir_);
}

void Node::set_tampered(const std::string& why) {
  tampered_ = true;
  tamper_msg_ = why;
  storage_error_ = why;
}

bool Node::open_store_with(std::unique_ptr<store::RecordCipher> cipher, store::Status* st) {
  store::Options o;
  // With a cipher, or when the marker says so, only an encrypted generation may ever be opened:
  // a planted plaintext generation yields Tampered instead of a silent downgrade.
  o.require_encryption = cipher != nullptr || encrypted_marker();
  o.cipher = std::move(cipher);
  o.entropy = [this](uint8_t* b, size_t n) { rng_.fill(b, n); };
  std::unique_ptr<store::KvStore> kv;
  store::RecoveryReport rep;
  *st = store::KvStore::open(vfs_.get(), dir_, std::move(o), &kv, &rep);
  if (*st != store::Status::Ok) return false;
  if (rep.mixed_encryption)
    set_tampered("a plaintext copy of the store was found next to the encrypted one and was removed; "
                 "your data was opened from the encrypted store only");
  kv_ = std::move(kv);
  return true;
}

void Node::open_storage() {
  if (cfg_.storage_dir.empty()) {
    vfs_ = std::make_unique<store::MemVfs>();
    dir_ = "ember";
  } else {
    vfs_ = store::new_os_vfs();
    dir_ = cfg_.storage_dir;
    while (dir_.size() > 1 && dir_.back() == '/') dir_.pop_back();
  }
  vfs_->mkdir(dir_);
  store::ProbeInfo pi;
  store::Status st = store::KvStore::probe(*vfs_, dir_, &pi);
  bool must_encrypt = encrypted_marker();
  // Never wipe or reset anything that is, or should be, encrypted: stay locked and say why.
  auto stay_locked = [&](const std::string& why, bool blocks) {
    set_tampered(why);
    tamper_blocks_ = blocks;
    lock_ = Lock::NoStore;
    has_passphrase_ = true;
  };
  if (st == store::Status::Ok && pi.exists && pi.encrypted) {
    if (pi.mixed_encryption)
      set_tampered("an unexpected plaintext copy of the store was found next to the encrypted one");
    if (StorageParams::decode(pi.cipher_params, &sparams_)) {
      lock_ = Lock::NoStore;
      has_passphrase_ = true;
      return;
    }
    stay_locked("the encrypted store's header has unsupported or hostile key-derivation parameters", true);
    return;
  }
  if (must_encrypt) {
    stay_locked(st == store::Status::Ok
                    ? "the encrypted store is missing or was replaced by an unencrypted one"
                    : std::string("the encrypted store could not be read (") + store::status_name(st) + ")",
                true);
    return;
  }
  if (st != store::Status::Ok) {
    storage_error_ = std::string("store unreadable (") + store::status_name(st) + "), started fresh";
    reset_storage(nullptr);
  } else if (!open_store_with(nullptr, &st)) {
    if (st == store::Status::Locked || st == store::Status::Tampered) {
      stay_locked(std::string("the store could not be opened safely (") + store::status_name(st) + ")", true);
      return;
    }
    storage_error_ = std::string("store could not be opened (") + store::status_name(st) + "), started fresh";
    reset_storage(nullptr);
  }
  load_all();
  start_engine();
}

void Node::reset_storage(const std::string* duress_passphrase) {
  if (kv_) {
    kv_->wipe();
    kv_.reset();
  } else {
    store::KvStore::wipe(*vfs_, dir_);
  }
  if (cfg_.storage_dir.empty()) vfs_ = std::make_unique<store::MemVfs>();
  vfs_->mkdir(dir_);
  has_passphrase_ = false;
  has_duress_ = false;
  crypto::wipe(skey_);
  store::Status st;
  if (!open_store_with(nullptr, &st)) {
    // The disk refuses a fresh store: keep running from memory rather than not at all.
    storage_error_ = std::string("storage unavailable (") + store::status_name(st) + "), running in memory";
    vfs_ = std::make_unique<store::MemVfs>();
    vfs_->mkdir(dir_);
    if (!open_store_with(nullptr, &st)) return;
  }
  tampered_ = false;
  tamper_blocks_ = false;
  tamper_msg_.clear();
  if (duress_passphrase) {
    StorageParams sp = StorageParams::fresh(cfg_.kdf, rng_);
    crypto::Key32 k{};
    if (derive_storage_key(*duress_passphrase, ByteView(sp.salt.data(), sp.salt.size()), sp.kdf, &k) &&
        kv_->rekey(std::make_unique<AeadRecordCipher>(k), sp.encode()) == store::Status::Ok) {
      sparams_ = sp;
      skey_ = k;
      has_passphrase_ = true;
      set_encrypted_marker(true);
    }
    crypto::wipe(k);
  }
}

void Node::reset_storage_with_key(const crypto::Key32& key, const std::array<uint8_t, 16>& salt) {
  StorageParams sp = StorageParams::fresh(sparams_.kdf, rng_);
  sp.salt = salt;
  reset_storage(nullptr);
  if (kv_ && kv_->rekey(std::make_unique<AeadRecordCipher>(key), sp.encode()) == store::Status::Ok) {
    sparams_ = sp;
    skey_ = key;
    has_passphrase_ = true;
    set_encrypted_marker(true);
  }
}

// Presentation state (docs/RPC_API.md "Duress unlock"): profile and the presentation settings, in
// at most kDecoyPlain bytes.
Bytes Node::presentation_blob() const {
  ByteWriter w(kDecoyPlain);
  auto str8 = [&](const std::string& v, size_t max) {
    std::string_view t = utf8_truncate(v, max);
    w.u8(static_cast<uint8_t>(t.size())).str(t);
  };
  w.u8(1);
  str8(profile_.get_string("name"), 32);
  str8(profile_.get_string("callsign"), 16);
  str8(profile_.get_string("status"), 64);
  str8(settings_.get_string("region"), 8);
  auto pick = [](const std::string& v, std::initializer_list<const char*> opts) {
    uint8_t i = 0;
    for (const char* o : opts) {
      if (v == o) return i;
      ++i;
    }
    return uint8_t(0xFF);
  };
  w.u8(pick(settings_.get_string("units"), {"metric", "imperial"}));
  w.u8(pick(settings_.get_string("theme"), {"dark", "light", "night"}));
  w.u8(pick(settings_.get_string("position_sharing"), {"off", "manual", "auto"}));
  w.u8(static_cast<uint8_t>((settings_.get_bool("relay_enabled", true) ? 1 : 0) | (settings_.get_bool("low_power") ? 2 : 0) |
                            (settings_.get_bool("read_receipts", true) ? 4 : 0)));
  w.u8(static_cast<uint8_t>(std::clamp<int64_t>(settings_.get_int("store_forward_days", 3), 1, 30)));
  w.u8(static_cast<uint8_t>(std::clamp<int64_t>(settings_.get_int("default_ttl_hops", 7), 1, 15)));
  w.u32be(static_cast<uint32_t>(std::clamp<int64_t>(settings_.get_int("beacon_interval_s", 1800), 0, 86400)));
  return w.take();
}

void Node::apply_presentation(ByteView blob) {
  ByteReader r(blob);
  if (r.u8() != 1) return;
  auto str8 = [&]() {
    uint8_t n = r.u8();
    ByteView v = r.bytes(n);
    std::string t = r.ok() ? v.to_string() : std::string();
    return utf8_valid(t) ? t : std::string();
  };
  std::string name = str8(), cs = str8(), status = str8(), region = str8();
  uint8_t units = r.u8(), theme = r.u8(), pos = r.u8(), flags = r.u8(), days = r.u8(), ttl = r.u8();
  uint32_t beacon = r.u32be();
  if (!r.ok()) return;
  profile_.set("name", name).set("callsign", proto::valid_callsign(cs) ? cs : std::string()).set("status", status);
  const char* u[] = {"metric", "imperial"};
  const char* t[] = {"dark", "light", "night"};
  const char* ps[] = {"off", "manual", "auto"};
  if (!region.empty()) settings_.set("region", region);
  if (units < 2) settings_.set("units", u[units]);
  if (theme < 3) settings_.set("theme", t[theme]);
  if (pos < 3) settings_.set("position_sharing", ps[pos]);
  settings_.set("relay_enabled", (flags & 1) != 0).set("low_power", (flags & 2) != 0).set("read_receipts", (flags & 4) != 0);
  if (days >= 1 && days <= 30) settings_.set("store_forward_days", days);
  if (ttl >= 1 && ttl <= 15) settings_.set("default_ttl_hops", ttl);
  if (beacon <= 86400) settings_.set("beacon_interval_s", static_cast<int64_t>(beacon));
  db_put_json("meta", B("profile"), profile_);
  db_put_json("meta", B("settings"), settings_);
}

void Node::refresh_decoy() {
  if (!has_duress_ || !has_decoy_key_ || !kv_ || !has_passphrase_) return;
  StorageParams np = sparams_;
  Bytes pb = presentation_blob();
  reseal_decoy(np, decoy_key_, pb, rng_);
  if (kv_->rekey(std::make_unique<AeadRecordCipher>(skey_), np.encode()) == store::Status::Ok) sparams_ = np;
}

void Node::clear_memory_state() {
  id_ = Identity();
  crypto::wipe(decoy_key_);
  has_decoy_key_ = false;
  profile_ = Json::object({{"name", ""}, {"callsign", ""}, {"status", ""}});
  settings_ = default_settings();
  contacts_.clear();
  channels_.clear();
  conv_meta_.clear();
  bulletins_.clear();
  alerts_.clear();
  checkins_.clear();
  netplans_.clear();
  own_sos_ = Json();
  own_netplan_ = Json();
  own_position_ = Json();
  peer_event_mono_.clear();
  pending_time_.reset();
}

void Node::load_all() {
  if (!kv_) return;
  // identity
  Bytes sec;
  Identity idn;
  if (db_get("meta", B("identity"), &sec) && Identity::deserialize_secret(sec, idn)) {
    id_ = idn;
  } else {
    id_ = Identity::generate(rng_);
    Bytes s = id_.serialize_secret();
    kv_->put("meta", B("identity"), s, store::WriteOptions{true});
    crypto::wipe(s);
    mark_dirty();
  }
  crypto::wipe(sec);

  Json j;
  if (db_get_json("meta", B("profile"), &j) && j.is_object()) {
    for (const char* k : {"name", "callsign", "status"})
      if (j.get(k).is_string()) profile_.set(k, j.get(k));
  }
  if (db_get_json("meta", B("settings"), &j) && j.is_object()) {
    for (const auto& kv : j.members()) settings_.set(kv.first, kv.second);
  }
  if (db_get_json("meta", B("security"), &j)) {
    has_duress_ = has_passphrase_ && j.get_bool("has_duress");
    Bytes dk;
    has_decoy_key_ = false;
    if (has_duress_ && from_hex(j.get_string("decoy_key"), dk) && dk.size() == 32) {
      std::copy(dk.begin(), dk.end(), decoy_key_.begin());
      has_decoy_key_ = true;
    }
    crypto::wipe(dk);
  }
  if (db_get_json("meta", B("next_link_id"), &j)) {
    int64_t v = j.as_int(1);
    if (v >= 1 && v < 0x7fffffff) next_link_id_ = std::max(next_link_id_, static_cast<uint32_t>(v));
  }
  if (db_get_json("meta", B("sos"), &j) && j.is_object()) own_sos_ = j;
  if (db_get_json("meta", B("netplan"), &j) && j.is_object()) own_netplan_ = j;
  if (db_get_json("meta", B("position"), &j) && j.is_object()) own_position_ = j;

  std::vector<store::KvEntry> rows;
  kv_->scan("contacts", store::ScanSpec(), &rows);
  for (auto& r : rows) {
    if (r.key.size() != 8) continue;
    std::optional<Json> c = parse_json_bytes(r.value);
    if (!c || !c->is_object()) continue;
    NodeId id;
    std::copy(r.key.begin(), r.key.end(), id.begin());
    contacts_[id] = *c;
  }
  kv_->scan("channels", store::ScanSpec(), &rows);
  for (auto& r : rows) {
    std::optional<Json> c = parse_json_bytes(r.value);
    if (!c || !c->is_object()) continue;
    ChannelRec ch;
    ch.rec = *c;
    std::string id = c->get_string("id");
    if (id == "public" || id.empty()) continue;
    std::string norm = c->get_string("norm");
    if (c->get_bool("encrypted")) {
      Bytes key;
      if (!base64_decode(c->get_string("key"), key) || key.size() != 32) continue;
      crypto::Key32 k;
      std::copy(key.begin(), key.end(), k.begin());
      ch.key = proto::channel_from_key(norm, k);
      crypto::wipe(key);
    } else {
      ch.key = proto::open_channel(norm);
    }
    if (hex32(ch.key.id) != id) continue;
    channels_[id] = std::move(ch);
  }
  ensure_public_channel();
  kv_->scan("conv", store::ScanSpec(), &rows);
  for (auto& r : rows) {
    std::optional<Json> c = parse_json_bytes(r.value);
    if (c && c->is_object()) conv_meta_[std::string(r.key.begin(), r.key.end())] = *c;
  }
  kv_->scan("bulletins", store::ScanSpec(), &rows);
  for (auto& r : rows) {
    std::optional<Json> c = parse_json_bytes(r.value);
    if (c && c->is_object()) bulletins_[c->get_string("id")] = *c;
  }
  auto load_by_node = [&](const char* ns, std::map<NodeId, Json>& into) {
    kv_->scan(ns, store::ScanSpec(), &rows);
    for (auto& r : rows) {
      if (r.key.size() != 8) continue;
      std::optional<Json> c = parse_json_bytes(r.value);
      if (!c || !c->is_object()) continue;
      NodeId id;
      std::copy(r.key.begin(), r.key.end(), id.begin());
      into[id] = *c;
    }
  };
  load_by_node("sos", alerts_);
  load_by_node("checkins", checkins_);
  load_by_node("netplans", netplans_);
  load_links();
}

mesh::MeshConfig Node::mesh_config() const {
  mesh::MeshConfig c;
  c.default_ttl = static_cast<uint8_t>(std::clamp<int64_t>(settings_.get_int("default_ttl_hops", 7), 1, proto::kMaxTtl));
  c.relay = settings_.get_bool("relay_enabled", true);
  int64_t days = std::clamp<int64_t>(settings_.get_int("store_forward_days", 3), 1, 30);
  c.default_lifetime_s = static_cast<uint32_t>(days * 86400);
  int64_t beacon = std::clamp<int64_t>(settings_.get_int("beacon_interval_s", 1800), 0, 86400);
  c.announce_interval_s = static_cast<uint32_t>(beacon);
  if (settings_.get_bool("low_power", false)) {
    if (c.announce_interval_s && c.announce_interval_s < 3600) c.announce_interval_s = 3600;
    c.sync_push_limit = 16;
  }
  return c;
}

void Node::apply_settings_to_engine() {
  if (eng_) eng_->set_config(mesh_config());
}

void Node::start_engine() {
  if (eng_ || !kv_ || !id_.valid() || !have_time_ || lock_ == Lock::NoStore) return;
  eng_ = std::make_unique<mesh::MeshEngine>(id_, ByteView(rng_.bytes(32)), mesh_config());
  eng_->set_delegate(this);
  eng_->tick(now_ms());
  // Restore the DTN store (collect first: no writes during a scan).
  std::vector<store::KvEntry> rows;
  kv_->scan("dtn", store::ScanSpec(), &rows);
  std::vector<Bytes> bad;
  for (auto& r : rows) {
    ByteReader rd{ByteView(r.value)};
    mesh::StoredMeta m;
    m.received_ms = rd.u64be();
    m.expires_ms = rd.u64be();
    m.flags = rd.u8();
    ByteView pkt = rd.rest();
    if (!rd.ok() || !eng_->restore_packet(pkt, m)) bad.push_back(r.key);
  }
  rows.clear();
  for (auto& k : bad) db_del("dtn", k);
  {
    bad.clear();
    kv_->scan("replay", store::ScanSpec(), &rows);
    uint64_t now = eng_->now_ms();
    for (auto& r : rows) {
      ByteReader rd{ByteView(r.value)};
      uint64_t until = rd.u64be();
      if (r.key.size() != 12 || !rd.ok() || until <= now) { bad.push_back(r.key); continue; }
      mesh::MsgId m;
      std::copy(r.key.begin(), r.key.begin() + 8, m.origin.begin());
      m.pkt_id = uint32_t(r.key[8]) << 24 | uint32_t(r.key[9]) << 16 | uint32_t(r.key[10]) << 8 | r.key[11];
      eng_->restore_replay(m, until);
    }
    rows.clear();
    for (auto& k : bad) db_del("replay", k);
  }
  Json st;
  if (db_get_json("meta", B("engine"), &st)) eng_->load_state(st);
  eng_->set_persistence(this);
  for (auto& kv : contacts_) {
    PublicKeys keys;
    if (contact_keys(kv.second, &keys))
      eng_->add_known_keys(keys, kv.second.get_string("name"), kv.second.get_string("callsign"));
  }
  for (auto& kv : channels_) eng_->join_channel(kv.second.key);
  eng_->set_profile(profile_.get_string("name"), profile_.get_string("callsign"), profile_.get_string("status"));
  if (pending_time_) {
    eng_->set_time(pending_time_->first, pending_time_->second);
    pending_time_.reset();
  }
  for (auto& kv : links_)
    if (kv.second.link) eng_->attach_link(kv.first, kv.second.link.get());
  if (own_sos_.is_object() && own_sos_.get_bool("active") && !eng_->sos_active()) {
    std::optional<proto::Position> pos;
    proto::Position pp;
    if (json_to_position(own_sos_.get("position"), &pp)) pos = pp;
    eng_->sos_start(own_sos_.get_string("text"), pos,
                    static_cast<uint32_t>(std::clamp<int64_t>(own_sos_.get_int("interval_s", 60), 10, 3600)));
  }
  if (own_netplan_.is_object()) {
    proto::NetPlanBody np;
    if (json_to_netplan(own_netplan_, &np, nullptr) && !np.entries.empty()) eng_->set_netplan(np, 1800);
  }
  last_state_save_ = mono_;
}

void Node::stop_runtime(bool keep_links) {
  persist_engine_state();
  if (eng_) {
    for (auto& kv : links_) eng_->detach_link(kv.first);
    eng_.reset();
  }
  if (!keep_links) links_.clear();
}

void Node::persist_engine_state() {
  eng_state_dirty_ = false;
  last_state_save_ = mono_;
  if (!eng_ || !kv_) return;
  db_put_json("meta", B("engine"), eng_->save_state());
}

// --- storage helpers -----------------------------------------------------------------------------

void Node::mark_dirty() {
  db_dirty_ = true;
  dirty_event_ = true;
}

bool Node::db_put(std::string_view ns, ByteView key, ByteView value) {
  if (!kv_) return false;
  store::Status s = kv_->put(ns, key, value, store::WriteOptions{false});
  if (s != store::Status::Ok) return false;
  mark_dirty();
  return true;
}

bool Node::db_put_json(std::string_view ns, ByteView key, const Json& v) {
  std::string s = v.dump();
  return db_put(ns, key, ByteView(std::string_view(s)));
}

bool Node::db_get(std::string_view ns, ByteView key, Bytes* out) {
  if (!kv_) return false;
  return kv_->get(ns, key, out) == store::Status::Ok;
}

bool Node::db_get_json(std::string_view ns, ByteView key, Json* out) {
  Bytes b;
  if (!db_get(ns, key, &b)) return false;
  std::optional<Json> j = parse_json_bytes(b);
  if (!j) return false;
  *out = std::move(*j);
  return true;
}

bool Node::db_del(std::string_view ns, ByteView key) {
  if (!kv_) return false;
  if (!kv_->contains(ns, key)) return true;
  store::Status s = kv_->del(ns, key, store::WriteOptions{false});
  if (s != store::Status::Ok) return false;
  mark_dirty();
  return true;
}

bool Node::db_write(const store::WriteBatch& b) {
  if (!kv_) return false;
  if (b.empty()) return true;
  store::Status s = kv_->write(b, store::WriteOptions{false});
  if (s != store::Status::Ok) return false;
  mark_dirty();
  return true;
}

void Node::db_sync() {
  if (kv_ && db_dirty_) kv_->sync();
  db_dirty_ = false;
  last_sync_ = mono_;
}

// --- MeshPersistence -----------------------------------------------------------------------------

void Node::store_put(uint64_t wire_id, ByteView packet, const mesh::StoredMeta& meta) {
  ByteWriter k(8);
  k.u64be(wire_id);
  ByteWriter v(17 + packet.size());
  v.u64be(meta.received_ms).u64be(meta.expires_ms).u8(meta.flags).bytes(packet);
  db_put("dtn", k.data(), v.data());
}

// Replay memory of authenticated deliveries (security review H4): survives restarts and the
// deletion of message history, until the packet's expiry (+1 day).
namespace {
Bytes mid_key(const mesh::MsgId& m) {
  ByteWriter k(12);
  k.bytes(m.origin.data(), 8).u32be(m.pkt_id);
  return k.take();
}
}  // namespace

void Node::replay_put(const mesh::MsgId& msg, uint64_t keep_until_ms) {
  ByteWriter v(8);
  v.u64be(keep_until_ms);
  db_put("replay", mid_key(msg), v.data());
}

void Node::replay_remove(const mesh::MsgId& msg) { db_del("replay", mid_key(msg)); }

void Node::store_remove(uint64_t wire_id) {
  ByteWriter k(8);
  k.u64be(wire_id);
  db_del("dtn", k.data());
}

// --- outbox --------------------------------------------------------------------------------------

void Node::push_out(int kind, uint32_t link_id, std::string data) {
  // Events and link TX are bounded separately: a flood of events (e.g. hostile traffic) evicts
  // only older events, never frames waiting for the radio; TX is bounded on its own (oldest first).
  bool tx = kind == kOutLinkTx;
  std::deque<OutItem>& q = tx ? out_tx_ : out_;
  size_t& qb = tx ? out_tx_bytes_ : out_bytes_;
  size_t max_items = tx ? std::max<size_t>(16, cfg_.max_outbox_items / 2) : cfg_.max_outbox_items;
  size_t max_bytes = tx ? cfg_.max_outbox_bytes / 2 : cfg_.max_outbox_bytes;
  if (data.size() > max_bytes) {
    ++out_dropped_;
    return;
  }
  while (!q.empty() && (q.size() >= max_items || qb + data.size() > max_bytes)) {
    qb -= q.front().data.size();
    q.pop_front();
    ++out_dropped_;
  }
  qb += data.size();
  OutItem it;
  it.seq = ++out_seq_;
  it.kind = kind;
  it.link_id = link_id;
  it.data = std::move(data);
  q.push_back(std::move(it));
}

bool Node::event_allowed_while_locked(std::string_view name) const {
  return name == "node.ready" || name == "security.locked" || name == "storage.dirty" || name == "time.updated" ||
         name.substr(0, 5) == "link.";
}

void Node::emit(const char* name, const Json& data) {
  if (lock_ != Lock::Open && !event_allowed_while_locked(name)) return;
  std::string s = "{\"event\":";
  Json::quote(name, s);
  s += ",\"data\":";
  if (data.is_object())
    data.dump_to(s);
  else
    s += "{}";
  s += "}";
  push_out(kOutEvent, 0, std::move(s));
}

bool Node::poll(const OutItem** out) {
  if (out_.empty() && out_tx_.empty()) return false;
  bool tx = out_.empty() || (!out_tx_.empty() && out_tx_.front().seq < out_.front().seq);
  std::deque<OutItem>& q = tx ? out_tx_ : out_;
  current_ = std::move(q.front());
  q.pop_front();
  (tx ? out_tx_bytes_ : out_bytes_) -= current_.data.size();
  *out = &current_;
  return true;
}

// --- clock / timers ------------------------------------------------------------------------------

void Node::tick(uint64_t now) {
  if (now == 0) now = have_time_ ? host_now_ : kFallbackEpochMs;
  uint64_t jumped_back = have_time_ && now < host_now_ ? host_now_ - now : 0;
  if (have_time_ && now > host_now_) mono_ += now - host_now_;
  if (!have_time_) clock_base_ = now - mono_;
  host_now_ = now;
  have_time_ = true;
  if (!eng_) start_engine();
  // The engine and the drivers only ever see a monotonic clock; mesh time following a corrected
  // system clock is applied once here. (MeshEngine::tick now also handles backward host steps
  // correctly by itself — tests/mesh/mesh_clock_test.cpp — but the drivers still need monotonic
  // time, and keeping one clock path for both is simpler.)
  uint64_t t = now_ms();
  for (auto& kv : links_)
    if (kv.second.link) kv.second.link->tick(t);
  if (eng_) {
    eng_->tick(t);
    if (jumped_back && eng_->time_info().source == mesh::TimeSource::System) {
      uint64_t m = eng_->now_ms();
      eng_->set_time(m > jumped_back ? m - jumped_back : 0, mesh::TimeSource::System);
    }
  }
  poll_link_states();
  morse_.expire(mono_);
  flush_activity(false);
  if (mono_ - last_sweep_ >= kSweepIntervalMs) {
    last_sweep_ = mono_;
    sweep();
  }
  flush_periodic(false);
}

void Node::flush_periodic(bool force) {
  if (eng_state_dirty_ && (force || mono_ - last_state_save_ >= kStateSaveIntervalMs)) persist_engine_state();
  if (db_dirty_ && (force || mono_ - last_sync_ >= kSyncIntervalMs)) db_sync();
  if (dirty_event_ && (force || mono_ >= dirty_defer_until_) &&
      (force || mono_ - last_dirty_emit_ >= kDirtyEventIntervalMs || last_dirty_emit_ == 0)) {
    dirty_event_ = false;
    last_dirty_emit_ = mono_ ? mono_ : 1;
    emit("storage.dirty", Json::object());
  }
}

uint32_t Node::next_wakeup_ms() {
  if (!out_.empty() || !out_tx_.empty()) return 0;
  uint64_t w = 60000;
  auto until = [&](uint64_t due) {
    uint64_t d = due > mono_ ? due - mono_ : 0;
    if (d < w) w = d;
  };
  if (eng_) w = std::min<uint64_t>(w, eng_->next_wakeup_ms());
  if (db_dirty_) until(last_sync_ + kSyncIntervalMs);
  if (dirty_event_) until(std::max(last_dirty_emit_ + kDirtyEventIntervalMs, dirty_defer_until_));
  if (eng_state_dirty_) until(last_state_save_ + kStateSaveIntervalMs);
  until(last_sweep_ + kSweepIntervalMs);
  for (auto& kv : links_) {
    const LinkRec& r = kv.second;
    if (r.act_tx || r.act_rx) until(r.act_last + 1000);
    if (r.link && r.link->tx_queue_depth() > 0) w = std::min<uint64_t>(w, 20);
    // Device handshakes / retries run from tick(): keep ticking while a link is connecting.
    if (r.link && r.link->state() == LinkState::Connecting) w = std::min<uint64_t>(w, 50);
  }
  return static_cast<uint32_t>(w);
}

void Node::add_entropy(ByteView e) {
  rng_.seed(e);
  if (eng_) eng_->add_entropy(e);
}

int Node::guides_load(const uint8_t* pack, size_t len, bool copy) {
  if (!pack || len == 0) {
    lib_.clear();
    return -1;
  }
  guides::LoadStatus s = lib_.load(ByteView(pack, len), copy);
  return s == guides::LoadStatus::kOk ? 0 : (static_cast<int>(s) < 0 ? static_cast<int>(s) : -1);
}

// --- I/O entry points ----------------------------------------------------------------------------

void Node::link_rx(uint32_t link_id, ByteView data) {
  LinkRec* r = link_by_id(link_id);
  if (!r || !r->link || data.empty()) return;
  r->io_rx_bytes += data.size();
  r->act_rx += data.size();
  r->link->on_io_bytes(data);
}

void Node::audio_rx(uint32_t link_id, const int16_t* samples, size_t count, uint32_t rate) {
  LinkRec* r = link_by_id(link_id);
  if (!r || !r->link || !samples || count == 0 || rate < 8000 || rate > 384000) return;
  r->link->on_audio(samples, count, rate);
}

size_t Node::audio_tx(uint32_t link_id, int16_t* out, size_t max, uint32_t rate) {
  LinkRec* r = link_by_id(link_id);
  if (!r || !r->link || !out || max == 0 || rate < 8000 || rate > 384000) return 0;
  size_t n = r->link->pull_audio(out, max, rate);
  return n > max ? max : n;
}

// --- RPC entry -----------------------------------------------------------------------------------

std::string Node::call(std::string_view method, std::string_view params_json) {
  RpcResult r;
  Json params = Json::object();
  bool blank = true;
  for (char c : params_json)
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
      blank = false;
      break;
    }
  if (!blank) {
    std::string perr;
    std::optional<Json> pj = Json::parse(params_json, &perr);
    if (!pj)
      r = rpc_err("bad_params", "params are not valid JSON: " + perr);
    else if (!pj->is_object())
      r = rpc_err("bad_params", "params must be a JSON object");
    else
      params = std::move(*pj);
  }
  if (r.ok) r = call_json(method, params);
  Json env = Json::object();
  if (r.ok) {
    env.set("ok", true).set("result", std::move(r.result));
  } else {
    env.set("ok", false).set("error", Json::object({{"code", r.code}, {"message", utf8_sanitize(r.message)}}));
  }
  return env.dump();
}

RpcResult Node::call_json(std::string_view method, const Json& params) {
  if (method.empty() || method.size() > 64 || !utf8_valid(method)) return rpc_err("bad_params", "invalid method name");
  const MethodDef* m = find_method(method);
  if (!m) {
    std::string name(method);
    if (name.compare(0, 5, "host.") == 0)
      return rpc_err("unsupported", name + " is handled by the platform host, not the core");
    return rpc_err("not_implemented", "unknown method '" + name + "'");
  }
  if (lock_ != Lock::Open && !(m->flags & kAllowLocked))
    return rpc_err("locked", "storage is locked: unlock with security.unlock");
  if ((m->flags & kNeedEngine) && !eng_)
    return rpc_err("busy", "the node clock is not set yet: call ember_tick first");
  Params p(params);
  RpcResult r = (this->*(m->fn))(p);
  flush_periodic(false);
  return r;
}

}  // namespace ember::node
