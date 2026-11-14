// EmberMesh node — links: the LinkHost side (driver I/O, events, state polling, activity) and the
// links.* RPC methods, including persistence with stable ids (docs/RPC_API.md "Link persistence").
#include <algorithm>

#include "link/link_util.h"
#include "link/lora_common.h"
#include "node/node.h"
#include "node/node_util.h"

namespace ember::node {

namespace {
constexpr size_t kMaxLinks = 64;
constexpr size_t kMaxLinkJson = 16 * 1024;

const char* state_name(int s) {
  switch (s) {
    case 1: return "connecting";
    case 2: return "up";
    case 3: return "error";
    default: return "down";
  }
}

Bytes link_key(uint32_t id) {
  ByteWriter w(4);
  w.u32be(id);
  return w.take();
}

Json caps_to_json(const LinkCaps& c) {
  return Json::object({{"mtu", static_cast<int>(c.mtu)},
                       {"bitrate_bps", c.bitrate_bps},
                       {"broadcast", c.broadcast},
                       {"half_duplex", c.half_duplex},
                       {"reliable", c.reliable},
                       {"encryption_allowed", c.encryption_allowed},
                       {"requires_station_id", c.requires_station_id},
                       {"duty_cycle_permille", static_cast<int>(c.duty_cycle_permille)},
                       {"cost", static_cast<int>(c.cost)}});
}

bool valid_event_name(const char* n) {
  if (!n || !*n) return false;
  size_t len = 0;
  for (const char* p = n; *p; ++p, ++len) {
    char c = *p;
    if (len >= 32 || !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  }
  return true;
}

std::string upper(std::string s) {
  for (char& c : s)
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  return s;
}
}  // namespace

const Json& Node::link_types_cached() {
  if (!link_types_.is_array()) {
    std::optional<Json> j = Json::parse(link_types_json());
    link_types_ = j && j->is_array() ? *j : Json::array();
  }
  return link_types_;
}

std::string Node::regional_link_default(const Json& t) {
  const char* want = link::lora_region_for_setting(settings_.get_string("region", "US"));
  if (!want) return std::string();
  for (const Json& f : t.get("config").items()) {
    if (f.get_string("key") != "region" || f.get_string("type") != "select") continue;
    for (const Json& o : f.get("options").items())
      if (o.get_string("value") == want) return want;
  }
  return std::string();
}

Json Node::link_types_for_region() {
  Json out = Json::array();
  for (const Json& t : link_types_cached().items()) {
    std::string def = regional_link_default(t);
    if (def.empty()) {
      out.push(t);
      continue;
    }
    Json tt = t;
    Json fields = Json::array();
    for (const Json& f : t.get("config").items()) {
      if (f.get_string("key") == "region") {
        Json ff = f;
        ff.set("default", def);
        fields.push(std::move(ff));
      } else {
        fields.push(f);
      }
    }
    tt.set("config", std::move(fields));
    out.push(std::move(tt));
  }
  return out;
}

Node::LinkRec* Node::link_by_ptr(const Link* l) {
  for (auto& kv : links_)
    if (kv.second.link.get() == l) return &kv.second;
  return nullptr;
}

Node::LinkRec* Node::link_by_id(uint32_t id) {
  auto it = links_.find(id);
  return it == links_.end() ? nullptr : &it->second;
}

Json Node::effective_link_config(const LinkRec& r) {
  Json c = r.config.is_object() ? r.config : Json::object();
  std::string cs = profile_.get_string("callsign");
  if (cs.empty()) return c;
  std::string cur = c.get_string("callsign");
  if (!cur.empty() && upper(cur) != "N0CALL") return c;
  for (const Json& t : link_types_cached().items()) {
    if (t.get_string("type") != r.type) continue;
    for (const Json& f : t.get("config").items())
      if (f.get_string("key") == "callsign") c.set("callsign", cs);
  }
  return c;
}

void Node::reapply_link_callsigns() {
  for (auto& kv : links_) {
    LinkRec& r = kv.second;
    if (!r.link || r.config.get("callsign").is_string()) continue;
    Json eff = effective_link_config(r);
    if (!eff.get("callsign").is_string()) continue;
    std::string err;
    r.link->reconfigure(eff.dump(), &err);
  }
}

bool Node::instantiate_link(LinkRec& r, std::string* error) {
  std::string err;
  std::unique_ptr<Link> l = create_link(r.type, effective_link_config(r).dump(), *this, &err);
  if (!l) {
    if (error) *error = err.empty() ? "invalid configuration" : err;
    r.create_error = err.empty() ? "invalid configuration" : err;
    return false;
  }
  if (eng_ && r.link) eng_->detach_link(r.id);
  r.link = std::move(l);
  r.create_error.clear();
  if (r.endpoint_connected) r.link->on_io_state(true);
  if (eng_) eng_->attach_link(r.id, r.link.get());
  return true;
}

namespace {
// The mesh engine sends nothing on a link that requires a station id (amateur band) while the
// profile has no callsign (mesh/tx.cpp can_transmit). Surfaced as Link.blocked = "no_callsign".
bool blocked_no_callsign(const Link* l, const mesh::MeshEngine* eng) {
  if (!l || !eng || l->state() != LinkState::Up) return false;
  LinkCaps c = l->caps();
  return c.requires_station_id && c.mtu > 0 && eng->callsign().empty();
}
constexpr const char* kNoCallsignError = "Set your callsign to transmit on this link (amateur band)";
}  // namespace

Json Node::link_json(const LinkRec& r) {
  Json j = Json::object();
  j.set("id", r.id).set("type", r.type).set("name", r.name);
  int st = r.link ? static_cast<int>(r.link->state()) : 3;
  Json status = Json::object();
  if (r.link) {
    std::optional<Json> s = Json::parse(r.link->status_json());
    if (s && s->is_object()) status = std::move(*s);
  }
  std::string error;
  if (!r.link) {
    error = r.create_error;
  } else if (st == 3) {
    error = status.get_string("last_error");
  } else if (st == 0 && !r.endpoint_error.empty()) {
    st = 3;
    error = r.endpoint_error;
  }
  j.set("state", state_name(st));
  if (!error.empty()) j.set("error", error);
  j.set("endpoint", r.endpoint.is_object() ? r.endpoint : Json::object());
  if (!r.auto_kind.empty()) {
    j.set("auto", r.auto_kind);
    if (!r.auto_role.empty()) j.set("auto_role", r.auto_role);
    j.set("auto_key", r.auto_key);
    if (!r.auto_neighbour.empty()) j.set("neighbour", r.auto_neighbour);
  }
  // Config: what the driver actually runs with — schema defaults, then the driver preset for keys
  // the user did not set, then the user's keys and the profile callsign the node copies in place of
  // N0CALL; the callsign the driver really transmits with (e.g. a licence-free pseudo id) wins.
  Json eff = effective_link_config(r);
  Json cfg = Json::object();
  std::string cerr;
  if (!link::effective_config(r.type, eff.dump(), &cfg, &cerr)) {
    cfg = Json::object();
    for (const Json& t : link_types_cached().items()) {
      if (t.get_string("type") != r.type) continue;
      for (const Json& f : t.get("config").items())
        if (f.has("default")) cfg.set(f.get_string("key"), f.get("default"));
    }
    for (const auto& kv : eff.members()) cfg.set(kv.first, kv.second);
  }
  const Json& ax = status.get("ax25").is_object() ? status.get("ax25") : status.get("modem_status").get("ax25");
  if (cfg.has("callsign") && ax.get("callsign").is_string() && !ax.get_string("callsign").empty())
    cfg.set("callsign", ax.get_string("callsign"));
  if (cfg.get("preset").is_string()) j.set("preset", cfg.get_string("preset"));
  Json overrides = Json::object();  // only the keys the user set explicitly
  if (r.config.is_object())
    for (const auto& kv : r.config.members())
      if (cfg.has(kv.first) && !kv.second.is_null()) overrides.set(kv.first, kv.second);
  if (r.type == "relay") {  // relay credentials are never echoed (they come from connectivity.relays)
    if (overrides.has("password")) overrides.set("password", "");
    if (cfg.is_object() && cfg.has("password")) cfg.set("password", "");
    Json ep = j.get("endpoint");
    if (ep.get("config").is_object() && ep.get("config").has("password")) {
      Json ec = ep.get("config");
      ec.set("password", "");
      ep.set("config", std::move(ec));
      j.set("endpoint", std::move(ep));
    }
  }
  j.set("overrides", std::move(overrides));
  j.set("config", std::move(cfg));
  j.set("caps", caps_to_json(r.link ? r.link->caps() : LinkCaps()));
  Json stats = Json::object();
  stats.set("tx_frames", status.get_int("tx_frames")).set("rx_frames", status.get_int("rx_frames"));
  stats.set("tx_bytes", status.get_int("tx_bytes")).set("rx_bytes", status.get_int("rx_bytes"));
  stats.set("rx_errors", status.get_int("rx_errors")).set("airtime_ms", status.get_int("airtime_ms"));
  stats.set("last_rx_ts", status.get_int("last_rx_ms"));
  stats.set("queue", status.get_int("queue", r.link ? static_cast<int64_t>(r.link->tx_queue_depth()) : 0));
  stats.set("io_tx_bytes", r.io_tx_bytes).set("io_rx_bytes", r.io_rx_bytes);
  j.set("stats", std::move(stats));
  if (blocked_no_callsign(r.link.get(), eng_.get())) {
    j.set("blocked", "no_callsign");
    if (status.get_string("last_error").empty()) status.set("last_error", kNoCallsignError);
  }
  if (r.type == "relay") {
    Json ids = Json::array();
    for (const auto& kv : relay_children_)
      if (kv.second.parent == r.id) ids.push(kv.first);
    status.set("session_link_ids", std::move(ids));
  }
  j.set("status", std::move(status));
  j.set("persist", r.persist);
  return j;
}

void Node::emit_link_state(LinkRec& r) {
  Json lj = link_json(r);
  std::string s = lj.get_string("state");
  r.last_state = s == "up" ? 2 : s == "connecting" ? 1 : s == "error" ? 3 : 0;
  r.last_blocked = lj.get_string("blocked") == "no_callsign";
  emit("link.state", Json::object({{"link", std::move(lj)}}));
}

void Node::poll_link_states() {
  for (auto& kv : links_) {
    LinkRec& r = kv.second;
    int st = r.link ? static_cast<int>(r.link->state()) : 3;
    if (st == 0 && !r.endpoint_error.empty()) st = 3;
    // Also when the "cannot transmit without a callsign" condition starts or clears.
    if (st != r.last_state || blocked_no_callsign(r.link.get(), eng_.get()) != r.last_blocked) emit_link_state(r);
  }
}

void Node::flush_activity(bool force) {
  for (auto& kv : links_) {
    LinkRec& r = kv.second;
    if (!r.act_tx && !r.act_rx) continue;
    if (!force && mono_ - r.act_last < 1000 && r.act_last != 0) continue;
    if (r.act_tx) emit("link.activity", Json::object({{"link_id", r.id}, {"dir", "tx"}, {"bytes", r.act_tx}}));
    if (r.act_rx) emit("link.activity", Json::object({{"link_id", r.id}, {"dir", "rx"}, {"bytes", r.act_rx}}));
    r.act_tx = r.act_rx = 0;
    r.act_last = mono_ ? mono_ : 1;
  }
}

void Node::persist_link(const LinkRec& r) {
  if (!r.persist) return;
  Json j = Json::object({{"id", r.id}, {"type", r.type}, {"name", r.name}, {"config", r.config}, {"endpoint", r.endpoint}});
  db_put_json("links", link_key(r.id), j);
  db_put_json("meta", B("next_link_id"), static_cast<int64_t>(next_link_id_));
}

void Node::load_links() {
  if (!kv_) return;
  std::vector<store::KvEntry> rows;
  kv_->scan("links", store::ScanSpec(), &rows);
  for (auto& row : rows) {
    std::optional<Json> j = parse_json_bytes(row.value);
    if (!j || !j->is_object()) continue;
    int64_t id = j->get_int("id");
    if (id < 1 || id > 0x7fffffff || links_.count(static_cast<uint32_t>(id))) continue;
    LinkRec& r = links_[static_cast<uint32_t>(id)];
    r.id = static_cast<uint32_t>(id);
    r.type = j->get_string("type");
    r.name = j->get_string("name");
    r.config = j->get("config").is_object() ? j->get("config") : Json::object();
    r.endpoint = j->get("endpoint").is_object() ? j->get("endpoint") : Json::object();
    r.persist = true;
    instantiate_link(r, nullptr);  // failures keep the definition visible in state "error"
    if (r.id >= next_link_id_) next_link_id_ = r.id + 1;
  }
}

// --- LinkHost ------------------------------------------------------------------------------------

void Node::link_frame_received(Link& link, ByteView frame, const RxMeta& meta) {
  LinkRec* r = link_by_ptr(&link);
  if (!r) {  // a session of a relay link (attached to the engine as its own link)
    uint32_t cid = relay_child_id(&link);
    if (cid && eng_) eng_->on_frame(cid, frame, meta);
    return;
  }
  if (!eng_) return;
  eng_->on_frame(r->id, frame, meta);
}

void Node::link_io_write(Link& link, ByteView bytes) {
  LinkRec* r = link_by_ptr(&link);
  if (!r || bytes.empty()) return;
  r->io_tx_bytes += bytes.size();
  r->act_tx += bytes.size();
  push_out(kOutLinkTx, r->id, bytes.to_string());
}

void Node::link_event(Link& link, const char* name, const std::string& json_object) {
  LinkRec* r = link_by_ptr(&link);
  if (!r || !valid_event_name(name)) return;
  std::optional<Json> j = Json::parse(json_object);
  Json d = j && j->is_object() ? *j : Json::object();
  d.set("link_id", r->id);
  if (std::string(name) == "aprs_ack") {
    std::string to = upper(d.get_string("to"));
    for (auto it = r->interop_pending.begin(); it != r->interop_pending.end(); ++it) {
      if (upper(it->first) != to) continue;
      set_message_status(it->second, d.get_bool("acked") ? "delivered" : "failed");
      r->interop_pending.erase(it);
      break;
    }
  }
  std::string ev = std::string("link.") + name;
  emit(ev.c_str(), d);
}

// --- links.* -------------------------------------------------------------------------------------

RpcResult Node::m_links_types(Params&) { return rpc_ok(Json::object({{"types", link_types_for_region()}})); }

RpcResult Node::m_links_list(Params&) {
  Json a = Json::array();
  for (auto& kv : links_) a.push(link_json(kv.second));
  return rpc_ok(Json::object({{"links", std::move(a)}}));
}

RpcResult Node::m_links_add(Params& p) {
  std::string type, name;
  const Json* cfg = nullptr;
  const Json* ep = nullptr;
  bool persist = true;
  std::string auto_kind, auto_role, auto_key;
  if (!p.str("type", &type, 32, true, false) || !p.str("name", &name, 64, false) || !p.object("config", &cfg, false) ||
      !p.object("endpoint", &ep, false) || !p.boolean("persist", &persist) || !p.str("auto", &auto_kind, 16, false) ||
      !p.str("auto_role", &auto_role, 16, false) || !p.str("auto_key", &auto_key, 64, false))
    return p.fail();
  {
    RpcResult aerr;
    if (!check_auto_link(auto_kind, auto_role, auto_key, type, &aerr)) return aerr;
    if (type == "relay" && !check_relay_endpoint(ep ? *ep : Json::object(), &aerr)) return aerr;
    if (!auto_kind.empty()) persist = false;  // automatic links are never persisted
    if (!utf8_valid(auto_key)) return rpc_err("bad_params", "auto_key must be valid UTF-8");
  }
  if (!utf8_valid(name)) return rpc_err("bad_params", "name must be valid UTF-8");
  if ((cfg && cfg->dump().size() > kMaxLinkJson) || (ep && ep->dump().size() > kMaxLinkJson))
    return rpc_err("too_large", "config / endpoint too large (16 KiB)");
  if (links_.size() >= kMaxLinks) return rpc_err("busy", "too many links (64)");
  std::string title, region_default;
  bool known = false;
  for (const Json& t : link_types_cached().items())
    if (t.get_string("type") == type) {
      known = true;
      title = t.get_string("title");
      region_default = regional_link_default(t);
    }
  if (!known) return rpc_err("bad_params", "unknown link type '" + utf8_sanitize(type) + "'");
  LinkRec r;
  r.id = next_link_id_;
  r.type = type;
  r.name = name.empty() ? title : name;
  r.config = cfg ? *cfg : Json::object();
  r.endpoint = ep ? *ep : Json::object();
  r.persist = persist;
  r.auto_kind = auto_kind;
  r.auto_role = auto_kind == "nearby" ? auto_role : "";
  r.auto_key = auto_key;
  if (auto_kind == "nearby") r.name = "Nearby device";
  if (type == "relay") {
    // The host never sees relay credentials (connectivity.get does not return passwords): the node
    // fills them in from the relay list entry with this address.
    const Json& rl = settings_.get("connectivity.relays");
    if (rl.is_array())
      for (const Json& e : rl.items())
        if (e.get_string("url") == (auto_key.empty() ? r.config.get_string("url") : auto_key)) {
          if (!e.get_string("username").empty()) r.config.set("username", e.get_string("username"));
          if (!e.get_string("password").empty()) r.config.set("password", e.get_string("password"));
        }
  }
  std::string err;
  // An unset LoRa "region" follows the node's region setting (stored explicitly, so a later change
  // of the setting never retunes an existing radio). If that makes an otherwise valid config fail
  // (e.g. an explicit frequency for another region), the driver default is kept as before.
  bool filled = false;
  if (!region_default.empty() && !r.config.has("region")) {
    r.config.set("region", region_default);
    filled = true;
  }
  if (!instantiate_link(r, &err)) {
    if (!filled) return rpc_err("bad_params", "link config: " + err);
    r.config = cfg ? *cfg : Json::object();
    err.clear();
    if (!instantiate_link(r, &err)) return rpc_err("bad_params", "link config: " + err);
  }
  ++next_link_id_;
  if (!persist) db_put_json("meta", B("next_link_id"), static_cast<int64_t>(next_link_id_));
  uint32_t id = r.id;
  links_[id] = std::move(r);
  LinkRec& rr = links_[id];
  persist_link(rr);
  emit_link_state(rr);
  return rpc_ok(Json::object({{"link", link_json(rr)}}));
}

RpcResult Node::m_links_configure(Params& p) {
  int64_t id = 0;
  std::string name;
  const Json* cfg = nullptr;
  const Json* ep = nullptr;
  if (!p.integer("id", &id, 1, 0x7fffffff, true) || !p.str("name", &name, 64, false) || !p.object("config", &cfg, false) ||
      !p.object("endpoint", &ep, false))
    return p.fail();
  LinkRec* r = link_by_id(static_cast<uint32_t>(id));
  if (!r) return rpc_err("not_found", "no such link");
  if (!utf8_valid(name)) return rpc_err("bad_params", "name must be valid UTF-8");
  if (ep && ep->dump().size() > kMaxLinkJson) return rpc_err("too_large", "endpoint too large (16 KiB)");
  if (cfg) {
    Json merged = r->config.is_object() ? r->config : Json::object();
    for (const auto& kv : cfg->members()) merged.set(kv.first, kv.second);
    if (merged.dump().size() > kMaxLinkJson) return rpc_err("too_large", "config too large (16 KiB)");
    Json old = r->config;
    r->config = merged;
    std::string err;
    bool ok = false;
    if (r->link) {
      ok = r->link->reconfigure(effective_link_config(*r).dump(), &err);
      if (ok && eng_) {
        // Caps (mtu, encryption_allowed ...) may have changed: re-attach so the engine re-reads them.
        eng_->detach_link(r->id);
        eng_->attach_link(r->id, r->link.get());
      }
    }
    if (!ok && err.empty()) ok = instantiate_link(*r, &err);  // driver cannot reconfigure in place
    if (!ok) {
      r->config = old;
      return rpc_err("bad_params", "link config: " + (err.empty() ? std::string("invalid configuration") : err));
    }
  }
  std::string nm = name;
  while (!nm.empty() && nm.back() == ' ') nm.pop_back();
  if (!nm.empty()) r->name = nm;
  // The endpoint descriptor is opaque to the core: replaced as a whole, same link id.
  if (ep) r->endpoint = *ep;
  persist_link(*r);
  emit_link_state(*r);
  return rpc_ok(Json::object({{"link", link_json(*r)}}));
}

RpcResult Node::m_links_remove(Params& p) {
  int64_t id = 0;
  if (!p.integer("id", &id, 1, 0x7fffffff, true)) return p.fail();
  auto it = links_.find(static_cast<uint32_t>(id));
  if (it == links_.end()) return rpc_err("not_found", "no such link");
  relay_detach_children(it->first);
  if (eng_) eng_->detach_link(it->first);
  Json lj = link_json(it->second);
  lj.set("state", "down").set("removed", true);
  db_del("links", link_key(it->first));
  links_.erase(it);
  emit("link.state", Json::object({{"link", std::move(lj)}}));
  return rpc_ok();
}

RpcResult Node::m_links_endpoint_state(Params& p) {
  int64_t id = 0;
  bool connected = false;
  std::string error;
  if (!p.integer("id", &id, 1, 0x7fffffff, true) || !p.str("error", &error, 512, false)) return p.fail();
  const Json* c = p.raw().find("connected");
  if (!c || !c->is_bool()) return rpc_err("bad_params", "connected must be true or false");
  connected = c->as_bool();
  LinkRec* r = link_by_id(static_cast<uint32_t>(id));
  if (!r) return rpc_err("not_found", "no such link");
  r->endpoint_connected = connected;
  r->endpoint_error = connected ? std::string() : utf8_sanitize(error);
  if (r->link) r->link->on_io_state(connected);
  emit_link_state(*r);
  return rpc_ok();
}

}  // namespace ember::node
