#include "host/host.h"

#include <algorithm>
#include <future>
#include <map>
#include <set>

#include "host/util.h"
#include "host/ws.h"

#if !defined(_WIN32)
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
#endif

namespace ember_host {

constexpr uint64_t kRelayStableMs = 120000;  // RPC_API §11: relay backoff resets after 2 min up

uint32_t relay_backoff_step(uint32_t* backoff_ms, uint16_t rnd) {
  // RPC_API §11: 2 s ... 5 min, +-20 % so many nodes behind one outage do not return in step.
  uint32_t base = std::min<uint32_t>(std::max<uint32_t>(*backoff_ms, 2000), 300000);
  *backoff_ms = std::min<uint32_t>(base * 2, 300000);
  return uint32_t(double(base) * (0.8 + 0.4 * (double(rnd) / 65535.0)));
}

Json rpc_ok(Json result) {
  Json e = Json::object();
  e.set("ok", true);
  e.set("result", result.is_null() ? Json::object() : std::move(result));
  return e;
}

Json rpc_error(const std::string& code, const std::string& message) {
  Json e = Json::object();
  e.set("ok", false);
  Json err = Json::object();
  err.set("code", code);
  err.set("message", message);
  e.set("error", err);
  return e;
}

CoreApi CoreApi::real() {
  CoreApi a;
  a.call = &ember_call;
  a.free_ = &ember_free;
  a.tick = &ember_tick;
  a.next_wakeup_ms = &ember_next_wakeup_ms;
  a.link_rx = &ember_link_rx;
  a.audio_rx = &ember_audio_rx;
  a.audio_tx = &ember_audio_tx;
  a.poll = &ember_poll;
  return a;
}

HostConfig HostConfig::from_json(const Json& j) {
  HostConfig c;
  c.data_dir = j.get("data_dir").as_string(c.data_dir);
  c.platform = j.get("platform").as_string(c.platform);
  c.app_version = j.get("app_version").as_string(c.app_version);
  c.restore_links = cfg_bool(j, "restore_links", c.restore_links);
  c.web_root = j.get("web_root").as_string(c.web_root);
  for (const auto& d : j.get("download_dirs").elements())
    if (d.is_string()) c.download_dirs.push_back(d.as_string());
  for (const auto& d : j.get("download_files").elements())
    if (d.is_string()) c.download_files.push_back(d.as_string());
  if (j.get("download_dir").is_string()) c.download_dirs.push_back(j.get("download_dir").as_string());
  c.admin_token = j.get("admin_token").as_string(c.admin_token);
  c.local_no_token = cfg_bool(j, "local_no_token", c.local_no_token);
  c.captive_portal = cfg_bool(j, "captive_portal", c.captive_portal);
  c.max_tick_interval_ms = uint32_t(cfg_int(j, "max_tick_interval_ms", c.max_tick_interval_ms));
  c.nearby = cfg_bool(j, "nearby", c.nearby);
  c.nearby_service = cfg_str(j, "nearby_service", c.nearby_service);
  for (const auto& l : j.get("links").elements())
    if (l.is_object()) c.startup_links.push(l);
  const Json& h = j.get("hub");
  if (h.is_bool()) {
    c.hub_autostart = h.as_bool();
    c.hub_restore = h.as_bool();
  }
  if (h.is_object()) {
    c.hub_autostart = cfg_bool(h, "autostart", true);
    c.hub_restore = cfg_bool(h, "restore", c.hub_restore);
    c.hub_port = uint16_t(cfg_int(h, "port", c.hub_port));
    c.hub_bind = cfg_str(h, "bind", c.hub_bind);
    c.hub_max_guests = size_t(cfg_int(h, "max_guests", int64_t(c.hub_max_guests)));
    c.hub_mqtt = cfg_bool(h, "mqtt", c.hub_mqtt);
    c.hub_mqtt_port = uint16_t(cfg_int(h, "mqtt_port", c.hub_mqtt_port));
    if (h.get("token").is_string()) c.admin_token = h.get("token").as_string();
    c.local_no_token = cfg_bool(h, "local_no_token", c.local_no_token);
    c.captive_portal = cfg_bool(h, "captive_portal", c.captive_portal);
  }
  return c;
}

Host::Host(ember_node* node, HostConfig cfg, CoreApi core, EndpointRegistry registry)
    : node_(node),
      cfg_(std::move(cfg)),
      core_(core),
      registry_(std::move(registry)),
      hub_(loop_, *this),
      alive_(std::make_shared<bool>(true)) {
  loop_.set_prepare([this] { return prepare(); });
  next_tick_due_ = 0;
}

Host::~Host() {
  stop_thread();
  if (!shut_down_) {
    loop_.bind_thread();
    shutdown();
  }
  dying_.clear();
  *alive_ = false;
}

// ---- threading -----------------------------------------------------------------------------------

void Host::start_thread() {
  if (thread_.joinable()) return;
  {
    std::lock_guard<std::mutex> lk(call_mu_);
    accepting_ = true;
  }
  loop_.reset_stop();
  shut_down_ = false;
  // Until the new thread binds itself in run(), no thread may run loop work inline: otherwise the
  // caller's next start()/call() would execute on its own thread, racing the loop thread.
  loop_.unbind_thread();
  thread_ = std::thread([this] { loop_.run(); });
}

void Host::stop_thread() {
  if (!thread_.joinable()) return;
  {
    std::lock_guard<std::mutex> lk(call_mu_);
    accepting_ = false;
    loop_.post([this] { shutdown(); });
    loop_.stop();
  }
  thread_.join();
  thread_ = std::thread();
  loop_.bind_thread();
}

void Host::run_once(int64_t max_wait_ms) {
  loop_.bind_thread();
  loop_.run_once(max_wait_ms);
}

void Host::start() {
  if (thread_.joinable() && !loop_.in_loop_thread()) {
    std::promise<void> p;
    auto f = p.get_future();
    loop_.post([this, &p] {
      start();
      p.set_value();
    });
    f.get();
    return;
  }
  if (started_) return;
  started_ = true;
  shut_down_ = false;
  if (cfg_.hub_autostart && !hub_.running()) {
    std::string err;
    if (!hub_server_start(cfg_.hub_port, cfg_.hub_bind, &err)) EH_LOGE("hub: %s", err.c_str());
  }
#ifdef EMBER_HOST_HAVE_MDNS
  if (cfg_.nearby && !nearby_) nearby_.reset(new Nearby(loop_, *this, *this, cfg_.nearby_service));
#endif
  // Also run on every node.ready (the first one is usually still in the outbox: sync is idempotent).
  sync_links();
}

void Host::shutdown() {
  if (thread_.joinable() && !loop_.in_loop_thread()) {
    std::promise<void> p;
    auto f = p.get_future();
    {
      std::lock_guard<std::mutex> lk(call_mu_);
      if (!accepting_) return;
      loop_.post([this, &p] {
        shutdown();
        p.set_value();
      });
    }
    f.get();
    return;
  }
  if (shut_down_) return;
  shut_down_ = true;
  nearby_.reset();  // goodbye on mDNS, closes the listener; its links go with links_ below
  hub_.stop();
  if (capture_) {
    if (capture_->timer) loop_.cancel(capture_->timer);
    capture_.reset();
  }
  if (hub_link_) {
    Json p = Json::object();
    p.set("id", hub_link_);
    p.set("connected", false);
    core_call("links.endpoint_state", p);
    hub_link_ = 0;
  }
  for (auto& kv : links_) {
    LinkRec& r = kv.second;
    if (r.retry_timer) {
      loop_.cancel(r.retry_timer);
      r.retry_timer = 0;
    }
    if (r.ep) {
      r.ep->close();
      Json p = Json::object();
      p.set("id", r.id);
      p.set("connected", false);
      core_call("links.endpoint_state", p);
    }
  }
  links_.clear();
  dying_.clear();
  started_ = false;
  drain();
}

Json Host::call(const std::string& method, const Json& params, CallOrigin origin) {
  if (thread_.joinable() && !loop_.in_loop_thread()) {
    std::promise<Json> p;
    auto f = p.get_future();
    {
      std::lock_guard<std::mutex> lk(call_mu_);
      if (!accepting_) return rpc_error("busy", "host is shutting down");
      loop_.post([this, &p, &method, &params, origin] { p.set_value(call_on_loop(method, params, origin)); });
    }
    return f.get();
  }
  return call_on_loop(method, params, origin);
}

std::string Host::call_json(const std::string& method, const std::string& params_json, CallOrigin origin) {
  Json p = Json::object();
  if (!params_json.empty()) {
    std::string err;
    if (!Json::parse(params_json, &p, &err)) return rpc_error("bad_params", "params are not valid JSON: " + err).dump();
    if (p.is_null()) p = Json::object();
  }
  return call(method, p, origin).dump();
}

int Host::add_event_listener(std::function<void(const std::string&)> fn) {
  int id = next_listener_++;
  listeners_[id] = std::move(fn);
  return id;
}

void Host::remove_event_listener(int id) { listeners_.erase(id); }

// ---- node runner ---------------------------------------------------------------------------------

int64_t Host::prepare() {
  uint64_t now = steady_ms();
  if (now >= next_tick_due_) {
    core_.tick(node_, unix_ms());
    need_drain_ = true;
    uint32_t w = core_.next_wakeup_ms(node_);
    if (w < 2) w = 2;  // never spin
    if (w > cfg_.max_tick_interval_ms) w = cfg_.max_tick_interval_ms;
    if (audio_endpoints_ > 0 && w > 20) w = 20;
    next_tick_due_ = now + w;
  }
  if (need_drain_) drain();
  now = steady_ms();
  return next_tick_due_ > now ? int64_t(next_tick_due_ - now) : 0;
}

void Host::drain() {
  if (draining_) return;
  draining_ = true;
  need_drain_ = false;
  ember_out out;
  for (int i = 0; i < 20000; ++i) {
    if (!core_.poll(node_, &out)) break;
    if (out.kind == EMBER_OUT_LINK_TX) {
      if (out.link_id != 0 && out.link_id == hub_link_) {
        hub_.broadcast_frame(out.data, out.len);
        continue;
      }
      auto it = links_.find(out.link_id);
      if (it != links_.end() && it->second.ep) {
        it->second.ep->write(out.data, out.len);
      } else {
        EH_LOGD("node: dropping %zu bytes for link %u (no endpoint)", out.len, out.link_id);
      }
    } else if (out.kind == EMBER_OUT_EVENT) {
      handle_event(reinterpret_cast<const char*>(out.data), out.len);
    }
    if (i == 19999) need_drain_ = true;  // yield to I/O; continue next iteration
  }
  draining_ = false;
}

void Host::handle_event(const char* json, size_t len) {
  std::string s(json, len);
  Json ev = Json::parse_or_null(s);
  const std::string& name = ev.get("event").as_string();
  if (name == "link.ptt") {
    const Json& d = ev.get("data");
    uint32_t id = uint32_t(d.get("link_id").as_int(0));
    auto it = links_.find(id);
    if (it != links_.end() && it->second.ep) it->second.ep->set_ptt(d.get("on").as_bool(false));
  } else if (name == "connectivity.changed") {
    if (started_) {
      refresh_nearby(&ev.get("data"));
      refresh_relays(&ev.get("data"));
    }
  } else if (name == "link.removed") {
    // The core removed a link by itself (duplicate nearby link, relay consent withdrawn).
    uint32_t id = uint32_t(ev.get("data").get("id").as_int(0));
    bool ours = nearby_ && nearby_->owns(id);
    close_link_rec(id);
    if (ours) nearby_->link_gone(id);
  } else if (name == "node.ready") {
    // RPC_API.md: hosts re-run their startup step on every node.ready (after create, unlock,
    // duress unlock and wipe).
    if (started_) sync_links();
  }
  auto ls = listeners_;  // a listener may (un)register listeners
  for (auto& kv : ls) kv.second(s);
  hub_.broadcast_event(s);
}

void Host::emit(const std::string& event, const Json& data) {
  Json e = Json::object();
  e.set("event", event);
  e.set("data", data);
  std::string s = e.dump();
  auto ls = listeners_;
  for (auto& kv : ls) kv.second(s);
  hub_.broadcast_event(s);
}

// ---- calls ---------------------------------------------------------------------------------------

Json Host::core_call(const std::string& method, const Json& params) {
  std::string pj = params.is_object() ? params.dump() : std::string("{}");
  char* r = core_.call(node_, method.c_str(), pj.c_str());
  need_drain_ = true;
  if (!r) return rpc_error("internal", "core returned NULL");
  Json env;
  bool ok = Json::parse(r, &env);
  core_.free_(r);
  if (!ok || !env.is_object()) return rpc_error("internal", "core returned invalid JSON");
  return env;
}

Json Host::call_on_loop(const std::string& method, const Json& params, CallOrigin origin) {
  Json p = params.is_object() ? params : Json::object();
  if (starts_with(method, "host.")) {
    Json r = route_host(method, p, origin);
    if (need_drain_) loop_.wake();
    return r;
  }
  Json env = core_call(method, p);
  if (env.get("ok").as_bool(false) && method == "links.list") {
    // Additive: each link gains `host` = the endpoint's own status (connected, device names,
    // FIFO reader, levels, PTT ...), as host.link.* results already carry.
    Json res = env.get("result");
    Json out = Json::array();
    for (const auto& l : res.get("links").elements()) {
      Json lj = l;
      uint32_t id = uint32_t(l.get("id").as_int(0));
      auto it = links_.find(id);
      if (it != links_.end() && it->second.ep) {
        Json hs = Json::object();
        hs.set("connected", it->second.connected);
        if (!it->second.error.empty()) hs.set("error", it->second.error);
        hs.set("status", it->second.ep->status());
        lj.set("host", hs);
      } else if (id && id == hub_link_) {
        Json hs = Json::object();
        hs.set("connected", hub_.running());
        hs.set("status", hub_.stats());
        lj.set("host", hs);
      }
      out.push(lj);
    }
    res.set("links", out);
    env.set("result", res);
  }
  if (env.get("ok").as_bool(false)) {
    if (method == "links.remove") {
      uint32_t id = uint32_t(p.get("id").as_int(0));
      close_link_rec(id);
      if (id && id == hub_link_) {
        hub_link_ = 0;
        hub_.stop();
        hub_wanted_ = false;
      }
    } else if (method == "links.add" || method == "security.wipe" || method == "security.unlock" ||
               method == "identity.import") {
      // links.add with an endpoint we can provide gets it opened (same as at startup); a wipe /
      // unlock changes the link list (node.ready follows too; sync is idempotent).
      if (started_) sync_links();
    }
  }
  if (need_drain_) loop_.wake();
  return env;
}

Json Host::route_host(const std::string& m, const Json& p, CallOrigin origin) {
  if (m == "host.info") return m_info(origin);
  if (m == "host.endpoints.scan") return m_scan(p);
  if (m == "host.link.create") return m_link_create(p);
  if (m == "host.link.remove") return m_link_remove(p);
  if (m == "host.link.reconnect") return m_link_reconnect(p);
  if (m == "host.hub.start") return m_hub_start(p);
  if (m == "host.hub.stop") return m_hub_stop();
  if (m == "host.hub.status") return m_hub_status();
  if (m == "host.file.save") return m_file_save(p);
  if (m == "host.file.pick") return m_file_pick(p);
  if (m == "host.notify") return m_notify(p);
  if (m == "host.capture.start") return m_capture_start(p);
  if (m == "host.capture.stop") return m_capture_stop(p);
  if (m == "host.capture.update") {
    // Nothing to update for audio (rect/roi are camera settings; camera capture is in the browser).
    if (p.get("kind").as_string() != "mic_pcm") return rpc_error("unsupported", "camera capture is done by the web page (getUserMedia)");
    return rpc_ok(Json::object());
  }
  return rpc_error("unsupported", m + " is not available on this platform");
}

// ---- host.info / scan ----------------------------------------------------------------------------

Json Host::core_link_types() {
  if (link_types_cache_.is_array()) return link_types_cache_;
  Json env = core_call("links.types", Json::object());
  if (env.get("ok").as_bool(false) && env.get("result").get("types").is_array()) {
    link_types_cache_ = env.get("result").get("types");
    return link_types_cache_;
  }
  return Json();
}

std::vector<std::string> Host::compatible_link_types(const EndpointKind* k, const std::string& kind) {
  std::vector<std::string> from_core;
  Json types = core_link_types();
  for (const auto& t : types.elements())
    for (const auto& ek : t.get("endpoint_kinds").elements())
      if (ek.as_string() == kind) from_core.push_back(t.get("type").as_string());
  if (!from_core.empty()) return from_core;
  // A kind the core does not list (e.g. audio_pipe): the endpoint's own list, restricted to types
  // the core actually has.
  std::vector<std::string> own;
  if (!k) return own;
  for (const auto& lt : k->link_types) {
    bool known = !types.is_array() || types.size() == 0;
    for (const auto& t : types.elements())
      if (t.get("type").as_string() == lt) known = true;
    if (known) own.push_back(lt);
  }
  return own;
}

bool Host::link_type_ok(const std::string& kind, const std::string& type, std::string* err) {
  std::vector<std::string> lt = compatible_link_types(registry_.find(kind), kind);
  if (lt.empty() || std::find(lt.begin(), lt.end(), type) != lt.end()) return true;
  std::string list;
  for (auto& s : lt) list += (list.empty() ? "" : ", ") + s;
  *err = "link type '" + type + "' cannot run over a '" + kind + "' endpoint (compatible: " + list + ")";
  return false;
}

Json Host::m_info(CallOrigin origin) {
  auto kind_json = [&](const EndpointKind& k) {
    Json j = Json::object();
    j.set("kind", k.kind);
    j.set("title", k.title);
    j.set("description", k.description);
    j.set("available", k.available);
    if (!k.available) j.set("unavailable_reason", k.unavailable_reason);
    j.set("scannable", k.scannable && bool(k.scan));
    j.set("config", k.config.is_array() ? k.config : Json::array());
    std::vector<std::string> lt = compatible_link_types(&k, k.kind);
    Json lts = Json::array();
    for (auto& s : lt) lts.push(s);
    j.set("link_types", lts);
    std::string def = k.default_link_type;
    if (!lt.empty() && std::find(lt.begin(), lt.end(), def) == lt.end()) def = lt.front();
    j.set("default_link_type", def);
    return j;
  };
  Json kinds = Json::array();
  {
    EndpointKind hubk;
    hubk.kind = "ws_server";
    hubk.title = "Hub (Wi-Fi guests)";
    hubk.description = "Serve the web app and the mesh bus on port 8437 so phones on this network join from a browser.";
    hubk.config.push(Field("port", "Port", "number").def(8437).min(1).max(65535));
    hubk.config.push(Field("bind", "Bind address", "text").def("0.0.0.0").advanced());
    hubk.link_types = {"raw"};
    hubk.default_link_type = "raw";
    kinds.push(kind_json(hubk));
  }
  for (const auto& k : registry_.kinds()) kinds.push(kind_json(k));
  Json r = Json::object();
  r.set("platform", cfg_.platform);
  r.set("app_version", cfg_.app_version);
  r.set("backend", origin == CallOrigin::Remote ? "remote" : "native");
  r.set("host_version", EMBER_HOST_VERSION);
  r.set("endpoint_kinds", kinds);
  Json feats = Json::array();
  feats.push("hub");
  if (!cfg_.data_dir.empty()) feats.push("file");
  feats.push("notify");
  feats.push("capture_mic_pcm");  // the daemon's own sound input (or a PCM pipe) can feed morse.rx
  if (nearby_) feats.push("nearby");
  if (ws_tls_available()) feats.push("relay");  // RPC_API §11: only with TLS in the websocket endpoint
  r.set("features", feats);
  r.set("websocket_tls", ws_tls_available());
  if (nearby_) r.set("nearby", nearby_->status());
  // Morse reading: the camera is always captured by the page itself (getUserMedia in the operator's
  // browser); the microphone either by the page (the operator's device) or natively here.
  Json cap = Json::object();
  cap.set("camera_luma", "browser");
#ifdef EMBER_HOST_HAVE_AUDIO
  cap.set("mic_pcm", "native: host.capture.start {kind:\"mic_pcm\", session, device?|input_path?} uses this computer's sound input");
#else
  cap.set("mic_pcm", "native: host.capture.start {kind:\"mic_pcm\", session, input_path} (PCM pipe; no sound-card support in this build)");
#endif
  r.set("capture", cap);
  return rpc_ok(r);
}

Json Host::m_scan(const Json& p) {
  std::string kind = p.get("kind").as_string();
  const EndpointKind* k = registry_.find(kind);
  if (!k) {
    if (kind == "ws_server") {
      Json r = Json::object();
      r.set("devices", Json::array());
      return rpc_ok(r);
    }
    return rpc_error("bad_params", "unknown endpoint kind '" + kind + "'");
  }
  if (!k->available) return rpc_error("unsupported", k->unavailable_reason);
  if (!k->scan) {
    Json r = Json::object();
    r.set("devices", Json::array());
    return rpc_ok(r);
  }
  Json r = k->scan(p);
  if (!r.get("devices").is_array()) r.set("devices", Json::array());
  return rpc_ok(r);
}

// ---- links ---------------------------------------------------------------------------------------

Json Host::link_json(uint32_t id) {
  Json link;
  Json env = core_call("links.list", Json::object());
  if (env.get("ok").as_bool(false)) {
    for (const auto& l : env.get("result").get("links").elements())
      if (uint32_t(l.get("id").as_int(0)) == id) link = l;
  }
  auto it = links_.find(id);
  if (!link.is_object()) {
    link = Json::object();
    link.set("id", id);
    if (it != links_.end()) {
      link.set("type", it->second.link.get("type"));
      link.set("name", it->second.name);
      link.set("state", it->second.connected ? "up" : (it->second.error.empty() ? "connecting" : "error"));
      if (!it->second.error.empty()) link.set("error", it->second.error);
      link.set("endpoint", it->second.endpoint);
      link.set("config", it->second.link.get("config").is_object() ? it->second.link.get("config") : Json::object());
    } else if (id == hub_link_) {
      link.set("type", "raw");
      link.set("name", "Hub guests");
      link.set("state", hub_.running() ? "up" : "down");
    }
  }
  if (it != links_.end() && it->second.ep) {
    Json hs = Json::object();
    hs.set("connected", it->second.connected);
    if (!it->second.error.empty()) hs.set("error", it->second.error);
    hs.set("status", it->second.ep->status());
    hs.set("autostart", it->second.autostart);
    link.set("host", hs);
  } else if (id != 0 && id == hub_link_) {
    Json hs = Json::object();
    hs.set("connected", hub_.running());
    hs.set("status", hub_.stats());
    link.set("host", hs);
  }
  return link;
}

namespace {
// Sensible driver presets for an endpoint kind when the caller did not choose one, so the core's
// caps (mtu, reliable, cost) match the medium. Only fills `preset` when absent.
Json with_default_preset(const std::string& kind, const std::string& type, Json cfg) {
  if (!cfg.is_object()) cfg = Json::object();
  if (cfg.has("preset")) return cfg;
  const char* preset = nullptr;
  if (type == "raw") {
    if (kind == "ws_server" || kind == "websocket") preset = "websocket";
    else if (kind == "udp") preset = "udp_lan";
    else if (kind == "loopback") preset = "loopback";
  } else if (type == "stream") {
    if (kind == "tcp" || kind == "tcp_server") preset = "tcp";
    else if (kind == "serial") preset = "cable";
  }
  if (preset) cfg.set("preset", preset);
  return cfg;
}

std::string spec_host_key(const Json& spec) {
  std::string hk = spec.get("host_key").as_string();
  if (!hk.empty()) return hk;
  return "cfg-" + sha1_hex(spec.get("endpoint").dump() + spec.get("link").dump()).substr(0, 12);
}

bool is_locked_error(const Json& env) { return env.get("error").get("code").as_string() == "locked"; }
}  // namespace

void Host::count_audio_endpoints() {
  audio_endpoints_ = 0;
  for (auto& kv : links_)
    if (kv.second.ep && kv.second.ep->is_audio()) ++audio_endpoints_;
}

void Host::close_link_rec(uint32_t id) {
  auto it = links_.find(id);
  if (it == links_.end()) return;
  if (it->second.retry_timer) loop_.cancel(it->second.retry_timer);
  if (it->second.ep) {
    it->second.ep->close();
    destroy_endpoint_later(std::move(it->second.ep));
  }
  links_.erase(it);
  count_audio_endpoints();
}

Host::LinkRec& Host::install_link(uint32_t id, const std::string& name, const Json& endpoint, const Json& lspec,
                                  const std::string& host_key, bool persist, std::unique_ptr<Endpoint> ep) {
  close_link_rec(id);  // never leak an old endpoint for the same id
  LinkRec& r = links_[id];
  r.id = id;
  r.host_key = host_key;
  r.name = name;
  r.endpoint = endpoint;
  r.link = lspec;
  r.autostart = persist;
  r.persist = persist;
  r.ep = std::move(ep);
  r.ep->link_id = id;
  count_audio_endpoints();
  EH_LOGI("link %u: %s (%s over %s)", id, name.c_str(), lspec.get("type").as_string().c_str(),
          endpoint.get("kind").as_string().c_str());
  open_endpoint(r);
  return r;
}

Json Host::m_link_create(const Json& p) {
  const Json& epj = p.get("endpoint");
  std::string kind = epj.get("kind").as_string();
  if (kind.empty()) return rpc_error("bad_params", "endpoint.kind is required");
  Json ep_cfg_in = epj.get("config").is_object() ? epj.get("config") : Json::object();
  const Json& lj = p.get("link");
  // autostart:false (RPC_API) = do not bring it back at the next start = not persisted.
  bool persist = cfg_bool(p, "persist", cfg_bool(p, "autostart", true));
  std::string host_key = p.get("host_key").as_string();

  if (kind == "ws_server") {
    uint16_t port = uint16_t(cfg_int(ep_cfg_in, "port", cfg_.hub_port));
    std::string bind = cfg_str(ep_cfg_in, "bind", cfg_.hub_bind);
    std::string err;
    if (!hub_start_internal(port, bind, &err)) return rpc_error("io_error", err);
    if (!hub_link_) return rpc_error("locked", "the hub is serving the app, but the node is locked: unlock to connect guests");
    Json r = Json::object();
    r.set("link", link_json(hub_link_));
    return rpc_ok(r);
  }

  const EndpointKind* k = registry_.find(kind);
  if (!k) return rpc_error("bad_params", "unknown endpoint kind '" + kind + "'");
  if (!k->available || !k->create) return rpc_error("unsupported", k->unavailable_reason.empty() ? kind + " is not available" : k->unavailable_reason);
  std::vector<std::string> compat = compatible_link_types(k, kind);
  std::string def_type = k->default_link_type;
  if (!compat.empty() && std::find(compat.begin(), compat.end(), def_type) == compat.end()) def_type = compat.front();
  std::string type = lj.get("type").as_string(def_type);
  if (type.empty()) return rpc_error("bad_params", "link.type is required");
  std::string err;
  if (!link_type_ok(kind, type, &err)) return rpc_error("bad_params", err);
  Json link_cfg = with_default_preset(kind, type, lj.get("config"));

  // Idempotent create (optional host_key): an endpoint we already run, or a core link we are not
  // running yet that carries the same key, is reused instead of adding a duplicate.
  if (!host_key.empty()) {
    for (auto& kv : links_)
      if (kv.second.host_key == host_key && kv.second.ep) {
        Json r = Json::object();
        r.set("link", link_json(kv.first));
        return rpc_ok(r);
      }
    Json env = core_call("links.list", Json::object());
    for (const auto& l : env.get("result").get("links").elements())
      if (l.get("endpoint").get("host_key").as_string() == host_key && open_core_link(l)) {
        Json r = Json::object();
        r.set("link", link_json(uint32_t(l.get("id").as_int(0))));
        return rpc_ok(r);
      }
  }

  Json ep_cfg = cfg_with_defaults(k->config, ep_cfg_in);
  std::unique_ptr<Endpoint> ep = k->create(loop_, *this, ep_cfg, &err);
  if (!ep) return rpc_error("bad_params", err.empty() ? "bad endpoint config" : err);

  std::string name = p.get("name").as_string(lj.get("name").as_string(""));
  if (name.empty()) {
    std::string detail = cfg_str(ep_cfg, "port", "");
    if (kind == "tcp" || kind == "rtl_tcp") detail = cfg_str(ep_cfg, "host") + ":" + cfg_str(ep_cfg, "port");
    if (kind == "websocket") detail = cfg_str(ep_cfg, "url");
    if (kind == "udp") detail = cfg_str(ep_cfg, "group") + ":" + cfg_str(ep_cfg, "port");
    if (kind == "audio") detail = cfg_str(ep_cfg, "input_device", "default");
    name = k->title + (detail.empty() ? "" : " " + detail);
  }
  Json endpoint = Json::object();
  endpoint.set("kind", kind);
  endpoint.set("config", ep_cfg);
  if (!host_key.empty()) endpoint.set("host_key", host_key);

  Json add = Json::object();
  add.set("type", type);
  add.set("name", name);
  add.set("config", link_cfg);
  add.set("endpoint", endpoint);
  add.set("persist", persist);
  Json env = core_call("links.add", add);
  if (!env.get("ok").as_bool(false)) return env;
  uint32_t id = uint32_t(env.get("result").get("link").get("id").as_int(0));
  if (!id) return rpc_error("internal", "links.add returned no link id");
  Json lspec = Json::object();
  lspec.set("type", type);
  lspec.set("config", link_cfg);
  install_link(id, name, endpoint, lspec, host_key, persist, std::move(ep));
  Json res = Json::object();
  res.set("link", link_json(id));
  return rpc_ok(res);
}

bool Host::open_core_link(const Json& l) {
  uint32_t id = uint32_t(l.get("id").as_int(0));
  if (!id) return false;
  if (links_.count(id)) return true;
  const Json& endpoint = l.get("endpoint");
  std::string kind = endpoint.get("kind").as_string();
  const EndpointKind* k = registry_.find(kind);
  if (!k || !k->available || !k->create) return false;
  Json ep_cfg = cfg_with_defaults(k->config, endpoint.get("config"));
  std::string err;
  std::unique_ptr<Endpoint> ep = k->create(loop_, *this, ep_cfg, &err);
  if (!ep) {
    EH_LOGW("link %u: cannot reopen its %s endpoint: %s", id, kind.c_str(), err.c_str());
    Json sp = Json::object();
    sp.set("id", id);
    sp.set("connected", false);
    sp.set("error", "endpoint: " + err);
    core_call("links.endpoint_state", sp);
    return false;
  }
  Json lspec = Json::object();
  lspec.set("type", l.get("type"));
  lspec.set("config", l.get("config").is_object() ? l.get("config") : Json::object());
  install_link(id, l.get("name").as_string(), endpoint, lspec, endpoint.get("host_key").as_string(),
               l.get("persist").as_bool(true), std::move(ep));
  return true;
}

void Host::sync_links() {
  if (shut_down_ || syncing_) return;
  syncing_ = true;
  Json env = core_call("links.list", Json::object());
  if (!env.get("ok").as_bool(false)) EH_LOGW("links.list failed: %s", env.dump().c_str());
  if (env.get("ok").as_bool(false)) {
    const Json& list = env.get("result").get("links");
    std::set<uint32_t> ids;
    for (const auto& l : list.elements()) ids.insert(uint32_t(l.get("id").as_int(0)));
    // Links the core no longer has (removed, wiped, duress identity): close their endpoints.
    std::vector<uint32_t> gone;
    for (auto& kv : links_)
      if (!ids.count(kv.first)) gone.push_back(kv.first);
    for (uint32_t id : gone) close_link_rec(id);
    if (hub_link_ && !ids.count(hub_link_)) hub_link_ = 0;
    for (const auto& l : list.elements()) {
      uint32_t id = uint32_t(l.get("id").as_int(0));
      if (!id || links_.count(id) || id == hub_link_) continue;
      const Json& ep = l.get("endpoint");
      std::string kind = ep.get("kind").as_string();
      if (kind == "ws_server") {
        if (hub_link_) continue;  // only one hub per host; extra ws_server links stay down
        if (!hub_.running()) {
          if (!cfg_.hub_restore || !cfg_.restore_links) continue;
          uint16_t port = uint16_t(cfg_int(ep.get("config"), "port", cfg_.hub_port));
          std::string bind = cfg_str(ep.get("config"), "bind", cfg_.hub_bind);
          std::string err;
          if (!hub_server_start(port, bind, &err)) {
            EH_LOGE("hub: %s", err.c_str());
            Json sp = Json::object();
            sp.set("id", id);
            sp.set("connected", false);
            sp.set("error", err);
            core_call("links.endpoint_state", sp);
            continue;
          }
        }
        adopt_hub_link(id);
        continue;
      }
      if (!cfg_.restore_links) continue;
      // Auto links (nearby / relay) belong to the component that created them; never reopen them here.
      if (l.get("auto").is_string() || ep.get("auto").is_string()) continue;
      open_core_link(l);  // kinds this host cannot provide are left alone (state down)
    }
    if (hub_.running() && !hub_link_) ensure_hub_link();
    ensure_startup_links();
  }
  syncing_ = false;
  refresh_nearby();
  refresh_relays();
}

void Host::refresh_relays(const Json* connectivity) {
  if (shut_down_ || relays_refreshing_) return;
  relays_refreshing_ = true;
  Json c;
  if (connectivity && connectivity->is_object()) {
    c = *connectivity;
  } else {
    Json env = core_call("connectivity.get", Json::object());
    c = env.get("ok").as_bool(false) ? env.get("result") : Json::object();
  }
  // Only while `ready` and `internet.active`, and never without TLS (a host without TLS creates no
  // relay links). ready:false (lock, wipe, duress) closes them all.
  bool on = ws_tls_available() && c.get("ready").as_bool(false) && c.get("internet").get("active").as_bool(false);
  std::map<std::string, bool> want;  // url -> allow_plaintext
  if (on)
    for (const auto& rl : c.get("internet").get("relays").elements()) {
      std::string url = rl.get("url").as_string();
      if (!url.empty() && rl.get("enabled").as_bool(false)) want[url] = rl.get("allow_plaintext").as_bool(false);
    }
  std::vector<uint32_t> drop;
  std::set<std::string> have;
  for (const auto& kv : links_)
    if (kv.second.auto_relay) {
      if (want.count(kv.second.auto_key))
        have.insert(kv.second.auto_key);
      else
        drop.push_back(kv.first);
    }
  for (uint32_t id : drop) relay_remove(id);
  for (const auto& w : want)
    if (!have.count(w.first)) relay_install(w.first, w.second);
  relays_refreshing_ = false;
}

void Host::relay_install(const std::string& url, bool allow_plaintext) {
  const EndpointKind* k = registry_.find("websocket");
  if (!k || !k->create) return;
  Json ecfg = Json::object();
  ecfg.set("url", url);
  Json protos = Json::array();
  protos.push("mqtt");
  ecfg.set("protocols", protos);
  if (allow_plaintext) ecfg.set("allow_plaintext", true);
  std::string err;
  std::unique_ptr<Endpoint> ep = k->create(loop_, *this, cfg_with_defaults(k->config, ecfg), &err);
  if (!ep) {
    EH_LOGW("relay: not connecting (%s)", err.c_str());
    return;
  }
  Json endpoint = Json::object();
  endpoint.set("kind", "websocket");
  endpoint.set("config", ecfg);
  endpoint.set("auto", "relay");  // marker for hosts reading links.list (sync_links skips these)
  Json lcfg = Json::object();
  lcfg.set("url", url);
  Json add = Json::object();
  add.set("type", "relay");
  add.set("config", lcfg);
  add.set("endpoint", endpoint);
  add.set("persist", false);
  add.set("auto", "relay");
  add.set("auto_key", url);
  Json env = core_call("links.add", add);
  uint32_t id = uint32_t(env.get("result").get("link").get("id").as_int(0));
  if (!env.get("ok").as_bool(false) || !id) {
    EH_LOGW("relay: the core refused the link (%s)", env.get("error").get("message").as_string().c_str());
    return;
  }
  Json lspec = Json::object();
  lspec.set("type", "relay");
  lspec.set("config", lcfg);
  LinkRec& r = install_link(id, env.get("result").get("link").get("name").as_string("Internet relay"), endpoint, lspec,
                            std::string(), false, std::move(ep));
  r.auto_relay = true;
  r.auto_key = url;
}

void Host::relay_remove(uint32_t id) {
  close_link_rec(id);
  Json rp = Json::object();
  rp.set("id", id);
  core_call("links.remove", rp);  // may already be gone (the core removes relay links itself)
}

void Host::refresh_nearby(const Json* connectivity) {
  if (!nearby_ || shut_down_) return;
  Json c;
  if (connectivity && connectivity->is_object()) {
    c = *connectivity;
  } else {
    Json env = core_call("connectivity.get", Json::object());
    // Anything but a valid answer (locked, a core without connectivity.*) means: start nothing.
    c = env.get("ok").as_bool(false) ? env.get("result") : Json::object();
  }
  // RPC_API §11: only while `ready` (setup done, unlocked) and `nearby.active`; stop on ready:false.
  bool on = c.get("ready").as_bool(false) && c.get("nearby").get("active").as_bool(false);
  std::string token = c.get("nearby").get("token").as_string();
  if (token.size() != 8) on = false;
  nearby_->set_enabled(on, on ? token : std::string());
}

uint32_t Host::nearby_install(std::unique_ptr<Endpoint> ep, const Json& p) {
  if (shut_down_) return 0;
  Json endpoint = p.get("endpoint");
  endpoint.set("auto", "nearby");  // marker for hosts reading links.list (sync_links skips these)
  Json cfg = Json::object();
  cfg.set("preset", "nearby");     // the core sends and checks the 8-byte hello (CONNECTIVITY §3.3)
  Json add = Json::object();
  add.set("type", "stream");       // the core names it "Nearby device" / "Nearby: <name>"
  add.set("config", cfg);
  add.set("endpoint", endpoint);
  add.set("persist", false);
  add.set("auto", "nearby");
  add.set("auto_role", p.get("auto_role"));
  add.set("auto_key", p.get("auto_key"));
  Json env = core_call("links.add", add);
  if (!env.get("ok").as_bool(false)) {
    EH_LOGD("nearby: core refused the link (%s)", env.get("error").get("message").as_string().c_str());
    return 0;
  }
  uint32_t id = uint32_t(env.get("result").get("link").get("id").as_int(0));
  if (!id) return 0;
  Json lspec = Json::object();
  lspec.set("type", "stream");
  lspec.set("config", cfg);
  LinkRec& r = install_link(id, env.get("result").get("link").get("name").as_string("Nearby device"), endpoint, lspec,
                            std::string(), false, std::move(ep));
  r.auto_nearby = true;
  return id;
}

std::string Host::nearby_network_changed() {
  Json env = core_call("connectivity.network_changed", Json::object());
  return env.get("result").get("token").as_string();
}

void Host::nearby_remove(uint32_t id) {
  auto remove_now = [this, id] {
    bool known = links_.count(id) != 0;
    close_link_rec(id);
    if (known || !shut_down_) {
      Json rp = Json::object();
      rp.set("id", id);
      core_call("links.remove", rp);
    }
  };
  if (shut_down_) {
    remove_now();
    return;
  }
  // Deferred: this is often called from inside the endpoint's own callbacks.
  auto alive = alive_;
  loop_.timer(0, [alive, remove_now] {
    if (*alive) remove_now();
  });
}

void Host::ensure_startup_links() {
  for (const auto& spec_in : cfg_.startup_links.elements()) {
    std::string hk = spec_host_key(spec_in);
    bool have = false;
    for (auto& kv : links_)
      if (kv.second.host_key == hk) have = true;
    if (have) continue;
    Json spec = spec_in;
    spec.set("host_key", hk);
    if (!spec.has("persist")) spec.set("persist", false);
    Json r = m_link_create(spec);
    if (r.get("ok").as_bool(false)) continue;
    std::string what = spec.get("name").as_string(spec.get("endpoint").get("kind").as_string());
    if (is_locked_error(r)) {
      EH_LOGI("link %s: waiting for the node to be unlocked", what.c_str());
    } else {
      EH_LOGE("link %s: %s", what.c_str(), r.get("error").get("message").as_string().c_str());
    }
  }
}

void Host::open_endpoint(LinkRec& r) {
  r.opened_ms = steady_ms();
  r.error.clear();
  r.ep->open();
}

Json Host::m_link_remove(const Json& p) {
  uint32_t id = uint32_t(cfg_int(p, "id", 0));
  if (!id) return rpc_error("bad_params", "id is required");
  bool known = links_.count(id) != 0;
  if (id == hub_link_) {
    hub_.stop();
    hub_wanted_ = false;
    known = true;
  }
  close_link_rec(id);
  Json rp = Json::object();
  rp.set("id", id);
  Json env = core_call("links.remove", rp);
  if (id == hub_link_) hub_link_ = 0;
  if (!env.get("ok").as_bool(false) && !known) return env;
  return rpc_ok(Json::object());
}

Json Host::m_link_reconnect(const Json& p) {
  uint32_t id = uint32_t(cfg_int(p, "id", 0));
  if (id && id == hub_link_) {
    uint16_t port = hub_.port();
    std::string bind = hub_.options().bind;
    hub_.stop();
    std::string err;
    if (!hub_start_internal(port ? port : cfg_.hub_port, bind, &err)) return rpc_error("io_error", err);
    Json r = Json::object();
    r.set("link", link_json(id));
    return rpc_ok(r);
  }
  auto it = links_.find(id);
  if (it == links_.end() || !it->second.ep) return rpc_error("not_found", "no endpoint for link " + std::to_string(id));
  LinkRec& r = it->second;
  if (r.retry_timer) {
    loop_.cancel(r.retry_timer);
    r.retry_timer = 0;
  }
  r.backoff_ms = 1000;
  r.ep->close();
  if (r.connected) {
    r.connected = false;
    Json sp = Json::object();
    sp.set("id", id);
    sp.set("connected", false);
    core_call("links.endpoint_state", sp);
  }
  open_endpoint(r);
  Json res = Json::object();
  res.set("link", link_json(id));
  return rpc_ok(res);
}

void Host::schedule_reconnect(uint32_t id) {
  auto it = links_.find(id);
  if (it == links_.end() || it->second.retry_timer || shut_down_) return;
  LinkRec& r = it->second;
  uint32_t delay = r.backoff_ms;
  if (r.ep && r.ep->relay_backoff()) {
    uint16_t rnd = 0;
    random_bytes(&rnd, sizeof rnd);
    delay = relay_backoff_step(&r.backoff_ms, rnd);
  } else {
    r.backoff_ms = std::min<uint32_t>(r.backoff_ms * 2, 30000);
  }
  EH_LOGD("link %u: reconnecting in %u ms", id, delay);
  r.retry_timer = loop_.timer(delay, [this, id] {
    auto it2 = links_.find(id);
    if (it2 == links_.end() || !it2->second.ep) return;
    it2->second.retry_timer = 0;
    it2->second.ep->close();
    open_endpoint(it2->second);
  });
}

void Host::destroy_endpoint_later(std::unique_ptr<Endpoint> ep) {
  if (!ep) return;
  dying_.push_back(std::move(ep));
  if (dying_timer_) return;
  auto alive = alive_;
  dying_timer_ = loop_.timer(0, [this, alive] {
    if (!*alive) return;
    dying_timer_ = 0;
    dying_.clear();
  });
}

// ---- EndpointSink --------------------------------------------------------------------------------

void Host::ep_rx(Endpoint& ep, const uint8_t* data, size_t len) {
  if (!ep.link_id || len == 0) return;
  core_.link_rx(node_, ep.link_id, data, len);
  need_drain_ = true;
  next_tick_due_ = 0;  // tick after feeding input
}

void Host::ep_state(Endpoint& ep, bool connected, const std::string& error) {
  auto it = links_.find(ep.link_id);
  if (it == links_.end() || it->second.ep.get() != &ep) return;
  LinkRec& r = it->second;
  bool changed = r.connected != connected || r.error != error;
  r.connected = connected;
  r.error = error;
  if (changed) {
    if (connected)
      EH_LOGI("link %u: endpoint up", r.id);
    else
      EH_LOGW("link %u: endpoint down%s%s", r.id, error.empty() ? "" : ": ", error.c_str());
    Json sp = Json::object();
    sp.set("id", r.id);
    sp.set("connected", connected);
    if (!error.empty()) sp.set("error", error);
    core_call("links.endpoint_state", sp);
    Json ev = Json::object();
    ev.set("link_id", r.id);
    ev.set("connected", connected);
    if (!error.empty()) ev.set("error", error);
    emit("host.endpoint.state", ev);
  }
  if (connected) {
    // Only reset the backoff once a connection has proven stable-ish, so a flapping peer that
    // accepts and immediately drops us does not get hammered.
    if (r.retry_timer) {
      loop_.cancel(r.retry_timer);
      r.retry_timer = 0;
    }
    if (ep.relay_backoff())
      r.up_since_ms = steady_ms();  // the backoff resets only after 2 min connected (below)
    else
      r.backoff_ms = 1000;
  } else if (r.auto_nearby) {
    // A nearby connection that ended (or a dial that failed) is not reconnected: the link goes and
    // rediscovery makes a new one.
    if (nearby_) nearby_->link_gone(r.id);
    nearby_remove(r.id);
  } else if (ep.auto_reconnect()) {
    if (ep.relay_backoff() && r.up_since_ms) {
      if (steady_ms() - r.up_since_ms >= kRelayStableMs) r.backoff_ms = 2000;
      r.up_since_ms = 0;
    }
    schedule_reconnect(r.id);
  }
}

void Host::ep_audio_rx(Endpoint& ep, const int16_t* s, size_t n, uint32_t rate) {
  if (ep.link_id) core_.audio_rx(node_, ep.link_id, s, n, rate);
}

size_t Host::ep_audio_tx(Endpoint& ep, int16_t* out, size_t max, uint32_t rate) {
  if (!ep.link_id) return 0;
  return core_.audio_tx(node_, ep.link_id, out, max, rate);
}

void Host::ep_status_changed(Endpoint& ep) {}

// ---- hub -----------------------------------------------------------------------------------------

bool Host::hub_start_internal(uint16_t port, const std::string& bind, std::string* err) {
  if (!hub_server_start(port, bind, err)) return false;
  ensure_hub_link();
  return true;
}

void Host::adopt_hub_link(uint32_t id) {
  hub_link_ = id;
  Json sp = Json::object();
  sp.set("id", id);
  sp.set("connected", hub_.running());
  core_call("links.endpoint_state", sp);
  EH_LOGI("link %u: hub guests (ws_server on port %u)", id, unsigned(hub_.port()));
}

void Host::ensure_hub_link() {
  if (!hub_.running()) return;
  if (hub_link_) {
    adopt_hub_link(hub_link_);
    return;
  }
  // Reuse the core's persisted hub link (any ws_server link), else add one.
  Json env = core_call("links.list", Json::object());
  if (!env.get("ok").as_bool(false)) return;
  for (const auto& l : env.get("result").get("links").elements())
    if (l.get("endpoint").get("kind").as_string() == "ws_server") {
      adopt_hub_link(uint32_t(l.get("id").as_int(0)));
      return;
    }
  Json add = Json::object();
  add.set("type", "raw");
  add.set("name", "Hub guests (Wi-Fi)");
  add.set("config", with_default_preset("ws_server", "raw", Json::object()));
  Json ep = Json::object();
  ep.set("kind", "ws_server");
  Json c = Json::object();
  c.set("port", hub_.options().port);  // as requested (0 = any free port, e.g. tests)
  c.set("bind", hub_.options().bind);
  ep.set("config", c);
  add.set("endpoint", ep);
  Json r = core_call("links.add", add);
  if (r.get("ok").as_bool(false)) {
    locked_warned_ = false;
    adopt_hub_link(uint32_t(r.get("result").get("link").get("id").as_int(0)));
  } else if (is_locked_error(r)) {
    if (!locked_warned_)
      EH_LOGW("hub: the node is locked; the app is served but guests join the mesh only after unlocking");
    locked_warned_ = true;
  } else {
    EH_LOGW("hub: core refused the hub link (%s): the web app is served but guest frames cannot reach the mesh",
            r.get("error").get("message").as_string().c_str());
  }
}

bool Host::hub_server_start(uint16_t port, const std::string& bind, std::string* err) {
  if (hub_.running() && (port == 0 || port == hub_.port() || port == hub_.options().port) && hub_.options().bind == bind)
    return true;
  if (hub_.running()) hub_.stop();
  HubOptions o;
  o.bind = bind;
  o.port = port;
  o.web_root = cfg_.web_root;
  o.download_dirs = cfg_.download_dirs;
  o.download_files = cfg_.download_files;
  o.admin_token = cfg_.admin_token;
  o.local_no_token = cfg_.local_no_token;
  o.captive_portal = cfg_.captive_portal;
  o.max_guests = cfg_.hub_max_guests;
  o.mqtt = cfg_.hub_mqtt;
  o.mqtt_port = cfg_.hub_mqtt_port;
  o.version = cfg_.app_version;
  if (!hub_.start(o, err)) return false;
  hub_wanted_ = true;
  if (hub_link_) adopt_hub_link(hub_link_);  // restarted on another port: still connected
  return true;
}

Json Host::m_hub_start(const Json& p) {
  uint16_t port = uint16_t(cfg_int(p, "port", hub_.running() ? hub_.port() : cfg_.hub_port));
  std::string bind = cfg_str(p, "bind", cfg_.hub_bind);
  std::string err;
  if (!hub_start_internal(port, bind, &err)) return rpc_error("io_error", err);
  return m_hub_status();
}

Json Host::m_hub_stop() {
  // Stopping the hub removes its (persisted) core link so it does not come back at the next start.
  hub_.stop();
  hub_wanted_ = false;
  if (hub_link_) {
    Json sp = Json::object();
    sp.set("id", hub_link_);
    core_call("links.remove", sp);
    hub_link_ = 0;
  }
  return m_hub_status();
}

Json Host::m_hub_status() {
  Json r = Json::object();
  r.set("running", hub_.running());
  Json urls = Json::array();
  for (auto& u : hub_.urls()) urls.push(u);
  r.set("urls", urls);
  r.set("clients", double(hub_.guest_count()));
  if (hub_.running()) r.set("port", hub_.port());
  r.set("link_id", hub_link_);
  r.set("remote_sessions", double(hub_.rpc_count()));
  if (hub_.mqtt_port()) r.set("relay_broker_port", hub_.mqtt_port());
  r.set("stats", hub_.stats());
  return rpc_ok(r);
}

void Host::hub_bus_rx(const uint8_t* frame, size_t len) {
  if (!hub_link_) return;
  core_.link_rx(node_, hub_link_, frame, len);
  need_drain_ = true;
  next_tick_due_ = 0;
}

Json Host::hub_rpc(const std::string& method, const Json& params) { return call_on_loop(method, params, CallOrigin::Remote); }

void Host::hub_clients_changed(size_t guests) {
  Json d = Json::object();
  d.set("clients", double(guests));
  emit("host.hub.clients", d);
}

Json Host::hub_probe_info() {
  uint64_t now = steady_ms();
  if (!probe_cache_.is_object() || now - probe_cache_ms_ > 5000) {
    Json env = core_call("node.info", Json::object());
    Json j = Json::object();
    if (env.get("ok").as_bool(false)) {
      const Json& r = env.get("result");
      if (r.get("name").is_string()) j.set("name", r.get("name"));
      if (r.get("node_id").is_string()) j.set("node_id", r.get("node_id"));
    }
    probe_cache_ = j;
    probe_cache_ms_ = now;
  }
  return probe_cache_;
}

// ---- files / notify ------------------------------------------------------------------------------

static std::string sanitize_filename(const std::string& in) {
  std::string base = in;
  size_t s = base.find_last_of("/\\");
  if (s != std::string::npos) base = base.substr(s + 1);
  std::string out;
  for (unsigned char c : base) {
    if (c < 0x20 || c == 0x7f || std::string("<>:\"/\\|?*").find(char(c)) != std::string::npos)
      out += '_';
    else
      out += char(c);
  }
  while (!out.empty() && (out[0] == '.' || out[0] == ' ')) out.erase(0, 1);
  while (!out.empty() && (out.back() == '.' || out.back() == ' ')) out.pop_back();
  if (out.size() > 120) out = out.substr(out.size() - 120);
  if (out.empty()) out = "file";
  return out;
}

Json Host::m_file_save(const Json& p) {
  if (cfg_.data_dir.empty()) return rpc_error("unsupported", "no data directory configured");
  std::string name = sanitize_filename(p.get("name").as_string("file"));
  std::string data;
  if (!base64_decode(p.get("data_b64").as_string(), &data, true)) return rpc_error("bad_params", "data_b64 is not valid base64");
  if (data.size() > (size_t(64) << 20)) return rpc_error("too_large", "file larger than 64 MiB");
  std::string dir = path_join(cfg_.data_dir, "saved");
  if (!make_dirs(dir)) return rpc_error("io_error", "cannot create " + dir);
  std::string stem = name, ext;
  size_t dot = name.rfind('.');
  if (dot != std::string::npos && dot > 0) {
    stem = name.substr(0, dot);
    ext = name.substr(dot);
  }
  std::string path = path_join(dir, name);
  for (int i = 2; is_file(path) && i < 1000; ++i) path = path_join(dir, stem + " (" + std::to_string(i) + ")" + ext);
  if (!write_file_atomic(path, data)) return rpc_error("io_error", "cannot write " + path);
  EH_LOGI("saved %s (%zu bytes)", path.c_str(), data.size());
  Json r = Json::object();
  r.set("path", path);
  r.set("bytes", double(data.size()));
  return rpc_ok(r);
}

Json Host::m_file_pick(const Json& p) {
  if (cfg_.data_dir.empty()) return rpc_error("unsupported", "no data directory configured");
  std::string dir = path_join(cfg_.data_dir, "inbox");
  make_dirs(dir);
  std::vector<std::string> exts;
  for (const auto& a : split(p.get("accept").as_string(), ',')) {
    std::string t = to_lower(trim(a));
    if (starts_with(t, ".")) exts.push_back(t);
  }
  std::string chosen;
  if (p.get("name").is_string()) {
    chosen = sanitize_filename(p.get("name").as_string());
    if (!is_file(path_join(dir, chosen))) return rpc_error("not_found", "no file '" + chosen + "' in " + dir);
  } else {
    int64_t best = -1;
    for (const auto& n : list_dir(dir)) {
      if (n.empty() || n[0] == '.') continue;
      uint64_t sz;
      int64_t mt;
      if (!is_file(path_join(dir, n), &sz, &mt)) continue;
      if (!exts.empty()) {
        bool ok = false;
        for (auto& e : exts)
          if (ends_with(to_lower(n), e)) ok = true;
        if (!ok) continue;
      }
      if (mt > best) {
        best = mt;
        chosen = n;
      }
    }
    if (chosen.empty()) return rpc_error("not_found", "no file to pick: copy it into " + dir);
  }
  std::string data;
  if (!read_file(path_join(dir, chosen), &data, size_t(64) << 20)) return rpc_error("io_error", "cannot read " + chosen);
  Json r = Json::object();
  r.set("name", chosen);
  std::string mime = mime_type_for(chosen);
  size_t semi = mime.find(';');
  if (semi != std::string::npos) mime = mime.substr(0, semi);
  r.set("mime", mime);
  r.set("data_b64", base64_encode(data.data(), data.size()));
  return rpc_ok(r);
}

Json Host::m_notify(const Json& p) {
  std::string title = p.get("title").as_string("EmberMesh"), body = p.get("body").as_string();
  EH_LOGI("notify: %s: %s", title.c_str(), body.c_str());
#if defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__))
  if (cfg_.platform == "desktop") {
    std::vector<std::string> args;
#if defined(__APPLE__)
    auto as_str = [](const std::string& s) {
      std::string o = "\"";
      for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        if (static_cast<unsigned char>(c) >= 0x20) o += c;
      }
      return o + "\"";
    };
    args = {"osascript", "-e", "display notification " + as_str(body) + " with title " + as_str(title)};
#else
    if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY")) return rpc_ok(Json::object());
    args = {"notify-send", "--", title, body};
#endif
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(&a[0]);
    argv.push_back(nullptr);
    pid_t pid;
    if (posix_spawnp(&pid, argv[0], nullptr, nullptr, argv.data(), environ) == 0) {
      std::thread([pid] {
        int st;
        waitpid(pid, &st, 0);
      }).detach();
    }
  }
#endif
  return rpc_ok(Json::object());
}

// ---- native Morse capture (RPC_API.md §6 host.capture.*) -----------------------------------------

Json Host::m_capture_start(const Json& p) {
  std::string kind = p.get("kind").as_string();
  if (kind == "camera_luma")
    return rpc_error("unsupported", "camera capture is done by the web page itself (getUserMedia); this host has no native camera");
  if (kind != "mic_pcm") return rpc_error("bad_params", "kind must be mic_pcm");
  std::string session = p.get("session").as_string();
  if (session.empty() && p.get("session").is_number()) session = cfg_str(p, "session");
  if (session.empty()) return rpc_error("bad_params", "session is required (from morse.rx.start)");
  int64_t rate = cfg_int(p, "sample_rate", 8000);
  if (rate < 8000 || rate > 48000) return rpc_error("bad_params", "sample_rate must be 8000..48000");
  if (capture_) m_capture_stop(Json::object());
  auto cap = std::make_unique<Capture>();
  cap->session = session;
  cap->rate = uint32_t(rate);
  cap->buf = std::make_shared<CaptureBuf>();
  std::shared_ptr<CaptureBuf> buf = cap->buf;
  size_t cap_samples = size_t(rate);  // at most 1 s waits for the loop
  PcmFn on_pcm = [buf, cap_samples](const int16_t* s, size_t n) {
    std::lock_guard<std::mutex> lk(buf->mu);
    if (buf->pcm.size() + n > cap_samples) {
      size_t drop = std::min(buf->pcm.size(), buf->pcm.size() + n - cap_samples);
      buf->pcm.erase(buf->pcm.begin(), buf->pcm.begin() + long(drop));
      buf->dropped += drop;
    }
    buf->pcm.insert(buf->pcm.end(), s, s + n);
  };
  PcmErrFn on_err = [buf](const std::string& e) {
    std::lock_guard<std::mutex> lk(buf->mu);
    if (buf->error.empty()) buf->error = e.empty() ? "capture stopped" : e;
  };
  std::string path = p.get("input_path").as_string();
  std::string err;
  if (!path.empty()) {
    cap->source = path;
    cap->src = open_pcm_file_source(path, uint32_t(rate), on_pcm, on_err, &err);
    if (!cap->src) return rpc_error("io_error", err);
  } else {
#ifdef EMBER_HOST_HAVE_AUDIO
    cap->source = p.get("device").as_string();
    cap->src = open_mic_source(cap->source, uint32_t(rate), uint32_t(cfg_int(p, "open_timeout_ms", 8000)), on_pcm, on_err);
#else
    return rpc_error("unsupported", "no sound-card support in this build; give input_path (a PCM file or FIFO)");
#endif
  }
  capture_ = std::move(cap);
  EH_LOGI("capture: mic_pcm from %s @ %lld Hz -> morse session %s",
          capture_->source.empty() ? "default input" : capture_->source.c_str(), static_cast<long long>(rate), session.c_str());
  capture_->timer = loop_.timer(33, [this] { capture_flush(); });
  Json ev = Json::object();
  ev.set("kind", "mic_pcm");
  ev.set("session", session);
  ev.set("running", true);
  emit("host.capture.state", ev);
  return rpc_ok(Json::object());
}

void Host::capture_flush() {
  if (!capture_) return;
  capture_->timer = 0;
  std::vector<int16_t> pcm;
  std::string error;
  {
    std::lock_guard<std::mutex> lk(capture_->buf->mu);
    pcm.swap(capture_->buf->pcm);
    error = capture_->buf->error;
  }
  if (!pcm.empty()) {
    std::string bytes(pcm.size() * 2, '\0');
    for (size_t i = 0; i < pcm.size(); ++i) {
      uint16_t v = uint16_t(pcm[i]);
      bytes[2 * i] = char(v & 0xff);
      bytes[2 * i + 1] = char(v >> 8);
    }
    Json f = Json::object();
    f.set("session", capture_->session);
    f.set("pcm_b64", base64_encode(bytes));
    Json env = core_call("morse.rx.feed", f);
    capture_->fed_samples += pcm.size();
    if (!env.get("ok").as_bool(false)) {
      std::string code = env.get("error").get("code").as_string();
      if (code == "not_found" || code == "not_implemented" || code == "bad_params")
        error = "morse.rx.feed: " + env.get("error").get("message").as_string(code);
    }
  }
  if (!error.empty()) {
    EH_LOGW("capture: %s", error.c_str());
    Json ev = Json::object();
    ev.set("kind", "mic_pcm");
    ev.set("session", capture_->session);
    ev.set("running", false);
    ev.set("error", error);
    capture_.reset();
    emit("host.capture.state", ev);
    return;
  }
  capture_->timer = loop_.timer(33, [this] { capture_flush(); });
}

Json Host::m_capture_stop(const Json& p) {
  std::string kind = p.get("kind").as_string("mic_pcm");
  if (kind != "mic_pcm") return rpc_ok(Json::object());
  if (!capture_) return rpc_ok(Json::object());
  if (capture_->timer) loop_.cancel(capture_->timer);
  capture_->src.reset();  // joins / uninits; no more samples
  capture_->timer = 0;
  capture_flush();        // deliver what is left
  if (!capture_) return rpc_ok(Json::object());
  if (capture_->timer) loop_.cancel(capture_->timer);
  Json ev = Json::object();
  ev.set("kind", "mic_pcm");
  ev.set("session", capture_->session);
  ev.set("running", false);
  Json r = Json::object();
  r.set("fed_samples", double(capture_->fed_samples));
  capture_.reset();
  emit("host.capture.state", ev);
  return rpc_ok(r);
}

}  // namespace ember_host
