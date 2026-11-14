// EmberMesh node — RPC method table and the node / identity / time / settings / kv / security
// methods (docs/RPC_API.md §2).
#include <algorithm>
#include <cstring>

#include "base/codec.h"
#include "node/node.h"
#include "node/node_util.h"

#ifndef EMBER_VERSION
#define EMBER_VERSION "0.0.0"
#endif

namespace ember::node {

namespace {
constexpr uint8_t L = kAllowLocked;
constexpr uint8_t E = kNeedEngine;
constexpr size_t kMaxPassphrase = 1024;
constexpr size_t kMaxKvValue = 256 * 1024;
constexpr uint64_t kMaxKvTotal = 16u << 20;
}  // namespace

const Node::MethodDef* Node::find_method(std::string_view name) {
  static const MethodDef kMethods[] = {
      {"node.info", &Node::m_node_info, L},
      {"node.set_profile", &Node::m_node_set_profile, 0},
      {"node.set_time", &Node::m_node_set_time, L},
      {"identity.card", &Node::m_identity_card, 0},
      {"identity.export", &Node::m_identity_export, 0},
      {"identity.import", &Node::m_identity_import, 0},
      {"contacts.list", &Node::m_contacts_list, 0},
      {"contacts.add", &Node::m_contacts_add, 0},
      {"contacts.update", &Node::m_contacts_update, 0},
      {"contacts.remove", &Node::m_contacts_remove, 0},
      {"channels.list", &Node::m_channels_list, 0},
      {"channels.create", &Node::m_channels_create, 0},
      {"channels.invite", &Node::m_channels_invite, 0},
      {"channels.join", &Node::m_channels_join, 0},
      {"channels.update", &Node::m_channels_update, 0},
      {"channels.leave", &Node::m_channels_leave, 0},
      {"conversations.list", &Node::m_conversations_list, 0},
      {"conversations.clear", &Node::m_conversations_clear, 0},
      {"messages.list", &Node::m_messages_list, 0},
      {"messages.send", &Node::m_messages_send, E},
      {"messages.retry", &Node::m_messages_retry, E},
      {"messages.mark_read", &Node::m_messages_mark_read, 0},
      {"messages.delete", &Node::m_messages_delete, 0},
      {"messages.search", &Node::m_messages_search, 0},
      {"files.send", &Node::m_files_send, E},
      {"files.get", &Node::m_files_get, 0},
      {"interop.send", &Node::m_interop_send, 0},
      {"sos.start", &Node::m_sos_start, E},
      {"sos.stop", &Node::m_sos_stop, E},
      {"sos.status", &Node::m_sos_status, 0},
      {"sos.list", &Node::m_sos_list, 0},
      {"sos.ack", &Node::m_sos_ack, E},
      {"checkin.send", &Node::m_checkin_send, E},
      {"checkin.list", &Node::m_checkin_list, 0},
      {"position.set", &Node::m_position_set, L},
      {"position.get", &Node::m_position_get, 0},
      {"position.share", &Node::m_position_share, E},
      {"bulletins.list", &Node::m_bulletins_list, 0},
      {"bulletins.post", &Node::m_bulletins_post, E},
      {"bulletins.delete", &Node::m_bulletins_delete, E},
      {"mesh.peers", &Node::m_mesh_peers, 0},
      {"mesh.stats", &Node::m_mesh_stats, 0},
      {"mesh.ping", &Node::m_mesh_ping, E},
      {"mesh.announce", &Node::m_mesh_announce, E},
      {"netplan.get", &Node::m_netplan_get, 0},
      {"netplan.set", &Node::m_netplan_set, E},
      {"links.types", &Node::m_links_types, L},
      {"links.list", &Node::m_links_list, L},
      {"links.add", &Node::m_links_add, 0},
      {"links.configure", &Node::m_links_configure, 0},
      {"links.remove", &Node::m_links_remove, 0},
      {"links.endpoint_state", &Node::m_links_endpoint_state, L},
      {"guides.categories", &Node::m_guides_categories, L},
      {"guides.list", &Node::m_guides_list, L},
      {"guides.get", &Node::m_guides_get, L},
      {"guides.search", &Node::m_guides_search, L},
      {"guides.related", &Node::m_guides_related, L},
      {"guides.ask", &Node::m_guides_ask, L},
      {"guides.info", &Node::m_guides_info, L},
      {"tools.list", &Node::m_tools_list, L},
      {"tools.run", &Node::m_tools_run, L},
      {"rf.bands", &Node::m_rf_bands, L},
      {"rf.radios", &Node::m_rf_radios, L},
      {"rf.hardware", &Node::m_rf_hardware, L},
      {"morse.rx.start", &Node::m_morse_rx_start, L},
      {"morse.rx.feed", &Node::m_morse_rx_feed, L},
      {"morse.rx.status", &Node::m_morse_rx_status, L},
      {"morse.rx.reset", &Node::m_morse_rx_reset, L},
      {"morse.rx.stop", &Node::m_morse_rx_stop, L},
      {"tools.qr", &Node::m_tools_qr, L},
      {"bundle.export", &Node::m_bundle_export, E},
      {"bundle.import", &Node::m_bundle_import, E},
      {"settings.get", &Node::m_settings_get, 0},
      {"settings.set", &Node::m_settings_set, 0},
      {"kv.get", &Node::m_kv_get, 0},
      {"kv.set", &Node::m_kv_set, 0},
      {"kv.delete", &Node::m_kv_delete, 0},
      {"kv.list", &Node::m_kv_list, 0},
      {"security.status", &Node::m_security_status, L},
      {"security.set_passphrase", &Node::m_security_set_passphrase, 0},
      {"security.unlock", &Node::m_security_unlock, L},
      {"security.lock", &Node::m_security_lock, 0},
      {"security.set_duress", &Node::m_security_set_duress, 0},
      {"security.wipe", &Node::m_security_wipe, L},
  };
  for (const MethodDef& m : kMethods)
    if (name == m.name) return &m;
  return nullptr;
}

// --- node / identity -----------------------------------------------------------------------------

Json Node::node_info_json() {
  bool hide = lock_ != Lock::Open || !id_.valid();
  Json j = Json::object();
  j.set("version", EMBER_VERSION);
  j.set("node_id", hide ? std::string() : node_id_hex(id_.node_id()));
  j.set("name", hide ? std::string() : profile_.get_string("name"));
  j.set("callsign", hide ? std::string() : profile_.get_string("callsign"));
  j.set("status", hide ? std::string() : profile_.get_string("status"));
  Json fp = Json::array();
  if (!hide)
    for (auto& w : id_.fingerprint()) fp.push(w);
  j.set("fingerprint", std::move(fp));
  j.set("card", hide ? std::string() : id_.card(profile_.get_string("name"), profile_.get_string("callsign")).to_uri());
  j.set("platform", cfg_.platform);
  j.set("app_version", cfg_.app_version);
  j.set("device_name", cfg_.device_name);
  j.set("time_ms", mesh_now());
  j.set("time_source", eng_ ? mesh::time_source_name(eng_->time_info().source) : "system");
  j.set("uptime_ms", mono_ - created_mono_);
  j.set("locked", lock_ != Lock::Open);
  j.set("features", Json::array({"dm", "channels", "sos", "checkin", "position", "bulletins", "guides", "tools",
                                 "bundles", "files", "interop", "netplan", "kv", "security"}));
  j.set("storage", cfg_.storage_dir.empty() ? "memory" : "disk");
  if (!storage_error_.empty()) j.set("storage_error", storage_error_);
  return j;
}

RpcResult Node::m_node_info(Params&) { return rpc_ok(node_info_json()); }

RpcResult Node::m_node_set_profile(Params& p) {
  std::string name, cs, status;
  bool hn = p.has("name"), hc = p.has("callsign"), hs = p.has("status");
  if (!p.str("name", &name, proto::kMaxName, false) || !p.str("callsign", &cs, proto::kMaxCallsign, false) ||
      !p.str("status", &status, proto::kMaxStatus, false))
    return p.fail();
  if (!utf8_valid(name) || !utf8_valid(status)) return rpc_err("bad_params", "name/status must be valid UTF-8");
  for (char& c : cs)
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  if (hc && !cs.empty() && !proto::valid_callsign(cs))
    return rpc_err("bad_params", "callsign: letters, digits, '/' and '-' only (max 16)");
  if (hn) profile_.set("name", name);
  if (hc) profile_.set("callsign", cs);
  if (hs) profile_.set("status", status);
  db_put_json("meta", B("profile"), profile_);
  refresh_decoy();
  if (eng_) eng_->set_profile(profile_.get_string("name"), profile_.get_string("callsign"), profile_.get_string("status"));
  if (hc) reapply_link_callsigns();
  return rpc_ok(node_info_json());
}

RpcResult Node::m_node_set_time(Params& p) {
  int64_t ts = 0;
  std::string src;
  if (!p.integer("ts", &ts, 1, 32503680000000ll, true) || !p.str("source", &src, 16, true)) return p.fail();
  mesh::TimeSource s;
  if (src == "gps") s = mesh::TimeSource::Gps;
  else if (src == "manual") s = mesh::TimeSource::Manual;
  else if (src == "system") s = mesh::TimeSource::System;
  else return rpc_err("bad_params", "source must be one of gps, manual, system");
  if (eng_) {
    eng_->set_time(static_cast<uint64_t>(ts), s);
  } else {
    pending_time_ = std::make_pair(static_cast<uint64_t>(ts), s);
  }
  uint64_t t = eng_ ? eng_->now_ms() : static_cast<uint64_t>(ts);
  emit("time.updated", Json::object({{"time_ms", t}, {"source", src}}));
  return rpc_ok(Json::object({{"time_ms", t}}));
}

RpcResult Node::m_identity_card(Params&) {
  return rpc_ok(Json::object(
      {{"card", id_.card(profile_.get_string("name"), profile_.get_string("callsign")).to_uri()}}));
}

RpcResult Node::m_identity_export(Params& p) {
  std::string pass;
  if (!p.str("passphrase", &pass, kMaxPassphrase, true, false)) return p.fail();
  std::string extra = profile_.dump();
  std::string backup = id_.export_backup(pass, rng_, ByteView(std::string_view(extra)), cfg_.kdf);
  crypto::wipe(pass.data(), pass.size());
  if (backup.empty()) return rpc_err("internal", "backup could not be created");
  return rpc_ok(Json::object({{"backup", backup}}));
}

RpcResult Node::m_identity_import(Params& p) {
  std::string backup, pass;
  if (!p.str("backup", &backup, 4096, true, false) || !p.str("passphrase", &pass, kMaxPassphrase, true)) return p.fail();
  Identity idn;
  Bytes extra;
  std::string err;
  bool ok = Identity::import_backup(backup, pass, idn, &extra, &err);
  crypto::wipe(pass.data(), pass.size());
  if (!ok) {
    if (err.find("passphrase") != std::string::npos) return rpc_err("denied", "wrong passphrase or corrupt backup");
    return rpc_err("bad_params", "not a valid identity backup: " + err);
  }
  stop_runtime(true);
  id_ = idn;
  Bytes s = id_.serialize_secret();
  store::WriteBatch b;
  b.put("meta", B("identity"), s);
  b.del("meta", B("engine"));
  b.drop_namespace("dtn");
  b.drop_namespace("replay");
  crypto::wipe(s);
  if (kv_) kv_->write(b, store::WriteOptions{true});
  mark_dirty();
  std::optional<Json> prof = parse_json_bytes(extra);
  if (prof && prof->is_object()) {
    for (const char* k : {"name", "callsign", "status"})
      if (prof->get(k).is_string()) profile_.set(k, prof->get(k));
    db_put_json("meta", B("profile"), profile_);
  }
  contacts_.erase(id_.node_id());
  db_del("contacts", nid_view(id_.node_id()));
  start_engine();
  return rpc_ok(node_info_json());
}

// --- settings ------------------------------------------------------------------------------------

RpcResult Node::m_settings_get(Params&) { return rpc_ok(Json::object({{"settings", settings_}})); }

RpcResult Node::m_settings_set(Params& p) {
  const Json* in = nullptr;
  if (!p.object("settings", &in, true)) return p.fail();
  Json next = settings_;
  for (const auto& kv : in->members()) {
    const std::string& k = kv.first;
    const Json& v = kv.second;
    auto bad = [&](const std::string& why) { return rpc_err("bad_params", "settings." + k + ": " + why); };
    auto one_of = [&](std::initializer_list<const char*> opts) {
      if (!v.is_string()) return false;
      for (const char* o : opts)
        if (v.as_string() == o) return true;
      return false;
    };
    if (k.empty() || k.size() > 64) return rpc_err("bad_params", "setting names must be 1..64 bytes");
    if (k == "region") {
      if (!v.is_string() || v.as_string().empty() || v.as_string().size() > 8) return bad("region code expected");
      std::string r = v.as_string();
      for (char& c : r) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return bad("letters/digits only");
      }
      next.set(k, r);
    } else if (k == "relay_enabled" || k == "low_power" || k == "read_receipts") {
      if (!v.is_bool()) return bad("must be true or false");
      next.set(k, v);
    } else if (k == "store_forward_days") {
      int64_t d = 0;
      if (!v.get_int_if(&d) || d < 1 || d > 30) return bad("whole number of days 1..30");
      next.set(k, d);
    } else if (k == "beacon_interval_s") {
      int64_t s = 0;
      if (!v.get_int_if(&s) || s < 0 || s > 86400 || (s > 0 && s < 60)) return bad("0 (never) or 60..86400 seconds");
      next.set(k, s);
    } else if (k == "default_ttl_hops") {
      int64_t h = 0;
      if (!v.get_int_if(&h) || h < 1 || h > proto::kMaxTtl) return bad("1..15");
      next.set(k, h);
    } else if (k == "position_sharing") {
      if (!one_of({"off", "manual", "auto"})) return bad("off, manual or auto");
      next.set(k, v);
    } else if (k == "units") {
      if (!one_of({"metric", "imperial"})) return bad("metric or imperial");
      next.set(k, v);
    } else if (k == "theme") {
      if (!one_of({"dark", "light", "night"})) return bad("dark, light or night");
      next.set(k, v);
    } else if (k == "links_autostart") {
      if (!v.is_array()) return bad("must be an array");
      next.set(k, v);
    } else {
      next.set(k, v);  // UI-defined preference: stored and echoed
    }
  }
  if (next.dump().size() > 64 * 1024) return rpc_err("too_large", "settings exceed 64 KiB");
  bool engine_changed = false;
  for (const char* k : {"relay_enabled", "store_forward_days", "beacon_interval_s", "low_power", "default_ttl_hops"})
    if (next.get(k) != settings_.get(k)) engine_changed = true;
  settings_ = std::move(next);
  if (!db_put_json("meta", B("settings"), settings_)) return rpc_err("io_error", "settings could not be saved");
  refresh_decoy();
  if (engine_changed) apply_settings_to_engine();
  return rpc_ok(Json::object({{"settings", settings_}}));
}

// --- kv ------------------------------------------------------------------------------------------

namespace {
bool kv_key(Params& p, Bytes* out, bool need_key) {
  std::string ns, key;
  if (!p.str("ns", &ns, 64, true, false)) return false;
  if (need_key && !p.str("key", &key, 512, true, false)) return false;
  if (ns.find('\0') != std::string::npos || key.find('\0') != std::string::npos)
    return p.error("ns and key must not contain NUL");
  out->assign(ns.begin(), ns.end());
  out->push_back(0);
  out->insert(out->end(), key.begin(), key.end());
  return true;
}
}  // namespace

RpcResult Node::m_kv_get(Params& p) {
  Bytes k;
  if (!kv_key(p, &k, true)) return p.fail();
  Json v;
  if (!db_get_json("ukv", k, &v)) v = Json();
  return rpc_ok(Json::object({{"value", v}}));
}

RpcResult Node::m_kv_set(Params& p) {
  Bytes k;
  if (!kv_key(p, &k, true)) return p.fail();
  const Json* v = p.raw().find("value");
  if (!v) return rpc_err("bad_params", "value is required");
  std::string s = v->dump();
  if (s.size() > kMaxKvValue) return rpc_err("too_large", "value exceeds 256 KiB");
  if (kv_ && kv_->namespace_stats("ukv").bytes() + s.size() > kMaxKvTotal)
    return rpc_err("too_large", "app storage is full (16 MiB)");
  if (!db_put("ukv", k, ByteView(std::string_view(s)))) return rpc_err("io_error", "could not store value");
  return rpc_ok();
}

RpcResult Node::m_kv_delete(Params& p) {
  Bytes k;
  if (!kv_key(p, &k, true)) return p.fail();
  if (!db_del("ukv", k)) return rpc_err("io_error", "could not delete value");
  return rpc_ok();
}

RpcResult Node::m_kv_list(Params& p) {
  Bytes k;
  std::string prefix;
  if (!kv_key(p, &k, false) || !p.str("prefix", &prefix, 512, false)) return p.fail();
  size_t ns_len = k.size();
  k.insert(k.end(), prefix.begin(), prefix.end());
  Json keys = Json::array();
  if (kv_) {
    store::ScanSpec s;
    s.set_prefix(k);
    s.keys_only = true;
    s.limit = 10000;
    kv_->scan("ukv", s, [&](ByteView key, ByteView, uint64_t) {
      keys.push(utf8_sanitize(key.sub(ns_len).to_string()));
      return true;
    });
  }
  return rpc_ok(Json::object({{"keys", std::move(keys)}}));
}

// --- security ------------------------------------------------------------------------------------

RpcResult Node::m_security_status(Params&) {
  Json j = Json::object({{"locked", lock_ != Lock::Open},
                         {"has_passphrase", has_passphrase_},
                         {"has_duress", lock_ == Lock::Open && has_duress_}});
  if (tampered_) j.set("tampered", true).set("tamper_message", tamper_msg_);
  return rpc_ok(j);
}

RpcResult Node::m_security_set_passphrase(Params& p) {
  std::string old, nw;
  if (!p.str("old", &old, kMaxPassphrase, false) || !p.str("new", &nw, kMaxPassphrase, true)) return p.fail();
  if (!kv_) return rpc_err("io_error", "storage unavailable");
  if (has_passphrase_) {
    crypto::Key32 k{};
    bool ok = derive_storage_key(old, ByteView(sparams_.salt.data(), 16), sparams_.kdf, &k) &&
              ct_equal(ByteView(k.data(), 32), ByteView(skey_.data(), 32));
    crypto::wipe(k);
    if (!ok) return rpc_err("denied", "current passphrase is wrong");
  }
  if (nw.empty()) {
    if (!has_passphrase_) return rpc_ok();
    // Marker first: a crash between the two leaves an encrypted store without a marker (still
    // protected by the store itself), never a plaintext store that looks tampered.
    set_encrypted_marker(false);
    if (kv_->rekey(nullptr, ByteView()) != store::Status::Ok) {
      set_encrypted_marker(true);
      return rpc_err("io_error", "re-encryption failed");
    }
    has_passphrase_ = false;
    has_duress_ = false;
    crypto::wipe(skey_);
    db_put_json("meta", B("security"), Json::object({{"has_duress", false}}));
    db_sync();
    return rpc_ok();
  }
  StorageParams np = StorageParams::fresh(has_duress_ ? sparams_.kdf : cfg_.kdf, rng_);
  if (has_duress_) {
    np.duress_salt = sparams_.duress_salt;
    np.duress_hash = sparams_.duress_hash;
    np.decoy = sparams_.decoy;
    np.version = sparams_.version;
    crypto::Key32 d{};
    bool same = derive_storage_key(nw, ByteView(np.duress_salt.data(), 16), np.kdf, &d) && duress_matches(np, d);
    crypto::wipe(d);
    if (same) return rpc_err("bad_params", "the passphrase must differ from the duress passphrase");
  }
  crypto::Key32 k{};
  if (!derive_storage_key(nw, ByteView(np.salt.data(), 16), np.kdf, &k)) return rpc_err("internal", "key derivation failed");
  if (kv_->rekey(std::make_unique<AeadRecordCipher>(k), np.encode()) != store::Status::Ok) {
    crypto::wipe(k);
    return rpc_err("io_error", "re-encryption failed");
  }
  sparams_ = np;
  skey_ = k;
  crypto::wipe(k);
  has_passphrase_ = true;
  set_encrypted_marker(true);
  mark_dirty();
  return rpc_ok();
}

RpcResult Node::m_security_unlock(Params& p) {
  std::string pass;
  if (!p.str("passphrase", &pass, kMaxPassphrase, true)) return p.fail();
  if (lock_ == Lock::Open) return rpc_ok(Json::object({{"unlocked", true}}));
  // Tampered storage is never opened, reset or wiped by an unlock attempt: the node stays locked
  // (security.status explains why; security.wipe remains the user's explicit choice).
  if (tamper_blocks_) return rpc_err("denied", "storage was tampered with: " + tamper_msg_);
  // Both derivations always run, so a normal, a wrong and a duress unlock take the same time.
  crypto::Key32 k{}, d{};
  if (!derive_storage_key(pass, ByteView(sparams_.salt.data(), 16), sparams_.kdf, &k) ||
      !derive_storage_key(pass, ByteView(sparams_.duress_salt.data(), 16), sparams_.kdf, &d)) {
    crypto::wipe(k);
    crypto::wipe(d);
    return rpc_err("internal", "key derivation failed");
  }
  bool unlocked = false;
  if (lock_ == Lock::Runtime) {
    unlocked = ct_equal(ByteView(k.data(), 32), ByteView(skey_.data(), 32));
  } else {
    store::Status st;
    if (open_store_with(std::make_unique<AeadRecordCipher>(k), &st)) {
      unlocked = true;
      skey_ = k;
    } else if (st == store::Status::Tampered || st == store::Status::Locked) {
      crypto::wipe(k);
      crypto::wipe(d);
      set_tampered(std::string("the encrypted store could not be opened safely (") + store::status_name(st) + ")");
      return rpc_err("denied", "storage was tampered with: " + tamper_msg_);
    } else if (st != store::Status::WrongKey) {
      crypto::wipe(k);
      crypto::wipe(d);
      return rpc_err("io_error", std::string("storage could not be opened: ") + store::status_name(st));
    }
  }
  crypto::wipe(k);
  if (!unlocked) {
    bool duress = duress_matches(sparams_, d);
    if (!duress) {
      crypto::wipe(d);
      return rpc_err("denied", "wrong passphrase");
    }
    // Duress: silently destroy everything and continue as a lightly used device: fresh identity,
    // the same profile / presentation settings (from the header decoy), storage protected by the
    // passphrase just entered (D is the new storage key), and has_duress still reported.
    Bytes present;
    bool have_present = open_decoy(sparams_, d, &present);
    std::array<uint8_t, 16> new_salt = sparams_.duress_salt;
    bool runtime = lock_ == Lock::Runtime;
    stop_runtime(runtime);
    clear_memory_state();
    reset_storage_with_key(d, new_salt);
    crypto::wipe(d);
    lock_ = Lock::Open;
    load_all();
    if (have_present) apply_presentation(present);
    crypto::wipe(present);
    // A phantom duress slot (random) keeps security.status identical to the real device's.
    has_duress_ = has_passphrase_;
    db_put_json("meta", B("security"), Json::object({{"has_duress", has_duress_}}));
    for (auto& kv : links_) persist_link(kv.second);
    start_engine();
    crypto::wipe(pass.data(), pass.size());
    // storage.dirty (hosts flush IDBFS) still follows, but at the normal rate-limited time rather
    // than together with the unlock, so the event sequence matches a normal unlock.
    dirty_defer_until_ = mono_ + 1000;
    emit("node.ready", Json::object());
    return rpc_ok(Json::object({{"unlocked", true}}));
  }
  crypto::wipe(d);
  crypto::wipe(pass.data(), pass.size());
  set_encrypted_marker(true);  // also migrates stores encrypted before the marker existed
  bool was_nostore = lock_ == Lock::NoStore;
  lock_ = Lock::Open;
  if (was_nostore) {
    load_all();
    start_engine();
  }
  emit("node.ready", Json::object());
  return rpc_ok(Json::object({{"unlocked", true}}));
}

RpcResult Node::m_security_lock(Params&) {
  if (!has_passphrase_) return rpc_err("bad_params", "set a storage passphrase first (security.set_passphrase)");
  db_sync();
  lock_ = Lock::Runtime;
  emit("security.locked", Json::object());
  return rpc_ok();
}

RpcResult Node::m_security_set_duress(Params& p) {
  std::string pass;
  if (!p.str("passphrase", &pass, kMaxPassphrase, true)) return p.fail();
  if (!has_passphrase_ || !kv_) return rpc_err("bad_params", "set a storage passphrase first");
  StorageParams np = sparams_;
  if (pass.empty()) {
    rng_.fill(np.duress_salt);
    rng_.fill(np.duress_hash);
    rng_.fill(np.decoy);
  } else {
    crypto::Key32 k{};
    bool same = derive_storage_key(pass, ByteView(sparams_.salt.data(), 16), sparams_.kdf, &k) &&
                ct_equal(ByteView(k.data(), 32), ByteView(skey_.data(), 32));
    crypto::wipe(k);
    if (same) return rpc_err("bad_params", "the duress passphrase must differ from the real one");
    rng_.fill(np.duress_salt);
    crypto::Key32 d{};
    if (!derive_storage_key(pass, ByteView(np.duress_salt.data(), 16), np.kdf, &d))
      return rpc_err("internal", "key derivation failed");
    Bytes pb = presentation_blob();
    set_duress_slot(np, d, pb, rng_);
    decoy_key_ = decoy_key_for(d);
    crypto::wipe(d);
  }
  if (kv_->rekey(std::make_unique<AeadRecordCipher>(skey_), np.encode()) != store::Status::Ok)
    return rpc_err("io_error", "could not update the storage header");
  sparams_ = np;
  has_duress_ = !pass.empty();
  has_decoy_key_ = has_duress_;
  Json sec = Json::object({{"has_duress", has_duress_}});
  if (has_duress_) sec.set("decoy_key", to_hex(ByteView(decoy_key_.data(), 32)));
  else crypto::wipe(decoy_key_);
  db_put_json("meta", B("security"), sec);
  db_sync();
  return rpc_ok();
}

RpcResult Node::m_security_wipe(Params& p) {
  std::string c;
  if (!p.str("confirm", &c, 16, true)) return p.fail();
  if (c != "ERASE") return rpc_err("bad_params", "confirm must be \"ERASE\"");
  stop_runtime(false);
  clear_memory_state();
  reset_storage(nullptr);
  lock_ = Lock::Open;
  load_all();
  start_engine();
  emit("node.ready", Json::object());
  return rpc_ok();
}

}  // namespace ember::node
