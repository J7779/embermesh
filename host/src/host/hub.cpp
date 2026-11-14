#include "host/hub.h"

#include <algorithm>
#include <deque>

#include "host/net.h"
#include "host/util.h"

namespace ember_host {

namespace {

// Per-guest token bucket so one misbehaving browser cannot flood the core.
struct Bucket {
  double tokens, cap, rate;  // rate per second
  uint64_t last;
  Bucket(double c, double r) : tokens(c), cap(c), rate(r), last(steady_ms()) {}
  bool take(double n) {
    uint64_t now = steady_ms();
    tokens = std::min(cap, tokens + double(now - last) * rate / 1000.0);
    last = now;
    if (tokens < n) return false;
    tokens -= n;
    return true;
  }
};

bool host_is_local_name(const std::string& origin) {
  // Origin: "http://localhost:8437" / "http://127.0.0.1:8437" / "http://[::1]:8437"
  std::string o = to_lower(origin);
  size_t p = o.find("://");
  if (p == std::string::npos) return false;
  std::string h = o.substr(p + 3);
  size_t slash = h.find('/');
  if (slash != std::string::npos) h = h.substr(0, slash);
  if (!h.empty() && h[0] == '[') {
    size_t rb = h.find(']');
    h = rb == std::string::npos ? h : h.substr(1, rb - 1);
  } else {
    size_t c = h.rfind(':');
    if (c != std::string::npos) h = h.substr(0, c);
  }
  return h == "localhost" || h == "::1" || starts_with(h, "127.");
}

}  // namespace

struct Hub::Guest {
  Guest(const HubOptions& o)
      : msgs(o.guest_frames_per_s * 2, o.guest_frames_per_s), bytes(o.guest_bytes_per_s * 2, o.guest_bytes_per_s) {}
  Bucket msgs;
  Bucket bytes;
  std::deque<std::string> inq;      // frames waiting for the shared budget (bounded)
  uint64_t dropped = 0;
  uint64_t drop_window_ms = 0, drops_in_window = 0;
  uint64_t out_dropping_since = 0;  // first outbound frame dropped since the last successful send
};

// A guest whose outbound queue stays full (it stopped reading: frozen tab, dead Wi-Fi) has its
// frames dropped (bounded memory) and is evicted after this long, so it reconnects and resyncs.
constexpr uint64_t kSlowGuestEvictMs = 15000;

std::string request_token(const HttpRequest& req) {
  bool present = false;
  std::string t = query_param(req.query, "token", &present);
  if (present && !t.empty()) return t;
  std::string auth = req.header_or("authorization");
  if (starts_with(to_lower(auth), "bearer ")) return trim(std::string_view(auth).substr(7));
  return std::string();
}

Hub::Hub(Loop& loop, HubDelegate& d) : loop_(loop), d_(d) {}

Hub::~Hub() {
  if (pump_timer_) loop_.cancel(pump_timer_);
  if (server_) {
    server_->stop();
    server_.reset();
  }
}

bool Hub::start(const HubOptions& opt, std::string* err) {
  if (running()) return true;
  opt_ = opt;
  HttpServer::Options so;
  so.bind = opt.bind;
  so.port = opt.port;
  so.max_conns = opt.max_guests + 56;
  so.limits.max_body = 8 << 20;  // POST /rpc with files.send payloads
  server_ = std::make_unique<HttpServer>(loop_, so);
  server_->set_handler([this](const HttpRequest& r, const SockAddr& p, HttpResponse& resp) { handle_http(r, p, resp); });

  HttpServer::WsRoute bus;
  bus.max_message = opt.max_frame;
  bus_tokens_ = opt.bus_frames_per_s;
  bus_tokens_ms_ = steady_ms();
  bus.max_pending = 512 * 1024;
  bus.authorize = [this](const HttpRequest&, const SockAddr&) {
    return guest_count() >= opt_.max_guests ? 503 : 0;
  };
  bus.on_open = [this](WsConn& c) {
    c.user = new Guest(opt_);
    ++guests_total_;
    EH_LOGI("hub: guest %s joined (%zu on the bus)", c.peer().to_string().c_str(), guest_count());
    d_.hub_clients_changed(guest_count());
  };
  bus.on_message = [this](WsConn& c, uint8_t op, std::string& data) {
    if (op != kWsBinary || data.empty()) return;  // mesh bus is binary only
    Guest* g = static_cast<Guest*>(c.user);
    if (!g) return;
    if (!g->msgs.take(1) || !g->bytes.take(double(data.size())) || g->inq.size() >= opt_.guest_queue_frames) {
      ++g->dropped;
      ++frames_rate_limited_;
      uint64_t now = steady_ms();
      if (now - g->drop_window_ms > 10000) {
        g->drop_window_ms = now;
        g->drops_in_window = 0;
      }
      if (++g->drops_in_window > opt_.guest_abuse_drops) {
        ++guests_abusive_;
        EH_LOGW("hub: guest %s floods the bus (%llu frames over its budget in 10 s): disconnecting it",
                c.peer().to_string().c_str(), static_cast<unsigned long long>(g->drops_in_window));
        g->inq.clear();
        c.close(kWsClosePolicy, "rate limit");
      }
      return;
    }
    g->inq.push_back(std::move(data));
    pump_bus();
  };
  bus.on_close = [this](WsConn& c) {
    delete static_cast<Guest*>(c.user);
    c.user = nullptr;
    EH_LOGI("hub: guest %s left", c.peer().to_string().c_str());
    d_.hub_clients_changed(guest_count());  // the closing connection is no longer counted
  };
  server_->add_ws_route("/ws", bus);

  HttpServer::WsRoute rpc;
  rpc.max_message = 8 << 20;
  rpc.max_pending = 8 << 20;
  rpc.authorize = [this](const HttpRequest& r, const SockAddr& p) { return authorize_rpc(r, p); };
  rpc.on_open = [](WsConn& c) { EH_LOGI("hub: remote control session from %s", c.peer().to_string().c_str()); };
  rpc.on_message = [this](WsConn& c, uint8_t op, std::string& data) {
    if (op != kWsText) {
      c.close(kWsCloseUnsupported, "JSON text frames only");
      return;
    }
    on_rpc_message(c, data);
  };
  server_->add_ws_route("/rpc", rpc);

  if (!server_->start(err)) {
    server_.reset();
    return false;
  }
  EH_LOGI("hub: listening on %s:%u", opt.bind.c_str(), server_->port());
  return true;
}

void Hub::pump_bus() {
  if (!server_) return;
  uint64_t now = steady_ms();
  bus_tokens_ = std::min(opt_.bus_frames_per_s, bus_tokens_ + double(now - bus_tokens_ms_) * opt_.bus_frames_per_s / 1000.0);
  bus_tokens_ms_ = now;
  std::vector<WsConn*> conns = server_->ws_connections("/ws");
  bool waiting = false;
  if (!conns.empty()) {
    // Round-robin: one frame per guest per turn, starting after the guest served last.
    size_t n = conns.size();
    bool progress = true;
    while (bus_tokens_ >= 1 && progress) {
      progress = false;
      for (size_t k = 0; k < n && bus_tokens_ >= 1; ++k) {
        WsConn* c = conns[(rr_next_ + k) % n];
        Guest* g = static_cast<Guest*>(c->user);
        if (!g || g->inq.empty() || !c->open()) continue;
        std::string f = std::move(g->inq.front());
        g->inq.pop_front();
        bus_tokens_ -= 1;
        ++frames_in_;
        progress = true;
        rr_next_ = (rr_next_ + k + 1) % n;
        d_.hub_bus_rx(reinterpret_cast<const uint8_t*>(f.data()), f.size());
        if (!server_) return;
        break;  // re-scan from the new rr position so every guest gets a turn
      }
    }
    for (WsConn* c : server_->ws_connections("/ws")) {
      Guest* g = static_cast<Guest*>(c->user);
      if (g && !g->inq.empty()) waiting = true;
    }
  }
  if (waiting && !pump_timer_) {
    double need_ms = (1.0 - bus_tokens_) * 1000.0 / opt_.bus_frames_per_s;
    pump_timer_ = loop_.timer(uint64_t(need_ms < 2 ? 2 : need_ms), [this] {
      pump_timer_ = 0;
      pump_bus();
    });
  }
}

void Hub::stop() {
  if (pump_timer_) {
    loop_.cancel(pump_timer_);
    pump_timer_ = 0;
  }
  if (!server_) return;
  server_->stop();
  // Keep the server object alive until its connections are reaped (they may be mid-callback).
  std::shared_ptr<HttpServer> dying(server_.release());
  loop_.timer(10, [dying] {});
}

size_t Hub::guest_count() const { return server_ ? server_->ws_count("/ws") : 0; }
size_t Hub::rpc_count() const { return server_ ? server_->ws_count("/rpc") : 0; }

std::vector<std::string> Hub::urls() const {
  std::vector<std::string> r;
  if (!running()) return r;
  uint16_t p = port();
  std::string b = opt_.bind;
  if (!b.empty() && b != "0.0.0.0" && b != "::") {
    r.push_back(format("http://%s:%u/", b.find(':') != std::string::npos ? ("[" + b + "]").c_str() : b.c_str(), p));
    return r;
  }
  for (const auto& i : list_ipv4_interfaces()) {
    if (i.loopback || !i.up) continue;
    if (starts_with(i.ip, "169.254.")) continue;  // link-local without DHCP; still list last
    r.push_back(format("http://%s:%u/", i.ip.c_str(), p));
  }
  for (const auto& i : list_ipv4_interfaces())
    if (!i.loopback && i.up && starts_with(i.ip, "169.254.")) r.push_back(format("http://%s:%u/", i.ip.c_str(), p));
  if (r.empty()) r.push_back(format("http://127.0.0.1:%u/", p));
  return r;
}

void Hub::broadcast_frame(const uint8_t* data, size_t len) {
  if (!server_) return;
  uint64_t now = steady_ms();
  for (WsConn* c : server_->ws_connections("/ws")) {
    if (!c->open()) continue;
    Guest* g = static_cast<Guest*>(c->user);
    if (c->send_binary(data, len, true)) {
      ++frames_out_;
      if (g) g->out_dropping_since = 0;
      continue;
    }
    ++frames_dropped_;
    if (!g) continue;
    if (!g->out_dropping_since) g->out_dropping_since = now;
    if (now - g->out_dropping_since > kSlowGuestEvictMs) {
      ++guests_evicted_;
      EH_LOGW("hub: guest %s is not reading (queue full for %llu s): disconnecting it", c->peer().to_string().c_str(),
              static_cast<unsigned long long>((now - g->out_dropping_since) / 1000));
      c->close(kWsClosePolicy, "too slow");
    }
  }
}

void Hub::broadcast_event(const std::string& ev) {
  if (!server_) return;
  for (WsConn* c : server_->ws_connections("/rpc"))
    if (c->open()) c->send_text(ev, false);
}

Json Hub::stats() const {
  Json s = Json::object();
  s.set("guests", double(guest_count()));
  s.set("remote_sessions", double(rpc_count()));
  s.set("frames_in", double(frames_in_));
  s.set("frames_out", double(frames_out_));
  s.set("frames_dropped", double(frames_dropped_));
  s.set("guests_total", double(guests_total_));
  s.set("guests_evicted", double(guests_evicted_));
  s.set("frames_rate_limited", double(frames_rate_limited_));
  s.set("guests_abusive", double(guests_abusive_));
  return s;
}

int Hub::authorize_rpc(const HttpRequest& req, const SockAddr& peer) const {
  std::string tok = request_token(req);
  if (!opt_.admin_token.empty() && !tok.empty() && constant_time_equals(tok, opt_.admin_token)) return 0;
  if (opt_.local_no_token && peer.is_loopback()) {
    // A web page from elsewhere open in the operator's browser could otherwise reach the loopback
    // RPC (WebSockets are not subject to CORS): require no Origin or a local one.
    const std::string* origin = req.header("origin");
    if (!origin || host_is_local_name(*origin)) return 0;
    return 403;
  }
  return 401;
}

void Hub::on_rpc_message(WsConn& c, std::string& text) {
  Json req;
  std::string perr;
  Json reply = Json::object();
  if (!Json::parse(text, &req, &perr) || !req.is_object()) {
    reply.set("id", Json());
    reply.set("ok", false);
    Json e = Json::object();
    e.set("code", "bad_params");
    e.set("message", "request must be a JSON object: " + perr);
    reply.set("error", e);
    c.send_text(reply.dump());
    return;
  }
  reply.set("id", req.get("id"));
  const std::string& method = req.get("method").as_string();
  if (method.empty()) {
    reply.set("ok", false);
    Json e = Json::object();
    e.set("code", "bad_params");
    e.set("message", "missing method");
    reply.set("error", e);
    c.send_text(reply.dump());
    return;
  }
  Json env = d_.hub_rpc(method, req.get("params"));
  reply.set("ok", env.get("ok").as_bool(false));
  if (env.get("ok").as_bool(false))
    reply.set("result", env.get("result").is_null() ? Json::object() : env.get("result"));
  else
    reply.set("error", env.get("error"));
  c.send_text(reply.dump());
}

std::string Hub::fallback_page() const {
  return R"(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>EmberMesh hub</title>
<style>body{font:16px/1.5 system-ui,sans-serif;max-width:40em;margin:2em auto;padding:0 1em;background:#111;color:#eee}
a{color:#f93}code{background:#222;padding:.1em .3em}</style></head>
<body><h1>EmberMesh hub</h1>
<p>This hub is running, but the web app files are not installed on it
(start <code>emberd --web-root DIR</code> with the <code>ui/</code> directory).</p>
<ul><li><a href="/download/">Downloads</a> &mdash; the offline app for phones and computers</li>
<li><a href="/hub.json">Hub status</a></li></ul>
<p id="s"></p>
<script>fetch('/hub.json').then(r=>r.json()).then(j=>{document.getElementById('s').textContent=
'Hub '+(j.name||'')+' version '+j.version+', '+j.guests+' device(s) connected.'}).catch(()=>{});</script>
</body></html>
)";
}

void Hub::serve_download(const HttpRequest& req, HttpResponse& resp) {
  std::string rel = req.path.substr(std::string("/download").size());  // "/..." or ""
  if (rel.empty()) {
    resp.status = 301;
    resp.headers.emplace_back("Location", "/download/");
    return;
  }
  if (rel != "/") {
    std::string base = rel.substr(1);
    for (const auto& f : opt_.download_files) {
      if (base.find('/') != std::string::npos || f.substr(f.find_last_of("/\\") + 1) != base || !is_file(f)) continue;
      resp.file_path = f;
      return;
    }
    for (const auto& d : opt_.download_dirs) {
      bool isdir = false;
      std::string f = resolve_static_path(d, rel, &isdir);
      if (f.empty() || isdir) continue;
      resp.file_path = f;
      std::string name = rel.substr(rel.find_last_of('/') + 1);
      if (!ends_with(to_lower(name), ".html")) {
        std::string safe;
        for (char ch : name)
          if (ch != '"' && ch != '\\' && static_cast<unsigned char>(ch) >= 0x20) safe += ch;
        resp.headers.emplace_back("Content-Disposition", "attachment; filename=\"" + safe + "\"");
      }
      return;
    }
    resp.status = 404;
    resp.content_type = "text/plain; charset=utf-8";
    resp.body = "not found\n";
    return;
  }
  // Listing.
  struct Item {
    std::string name;
    uint64_t size;
  };
  std::vector<Item> items;
  for (const auto& f : opt_.download_files) {
    uint64_t sz = 0;
    if (is_file(f, &sz)) items.push_back({f.substr(f.find_last_of("/\\") + 1), sz});
  }
  for (const auto& d : opt_.download_dirs) {
    for (const auto& n : list_dir(d)) {
      if (n.empty() || n[0] == '.') continue;
      uint64_t sz = 0;
      if (!is_file(path_join(d, n), &sz)) continue;
      bool dup = false;
      for (auto& it : items)
        if (it.name == n) dup = true;
      if (!dup) items.push_back({n, sz});
    }
  }
  std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.name < b.name; });
  std::string accept = req.header_or("accept");
  if (query_param(req.query, "format") == "json" || (accept.find("application/json") != std::string::npos && accept.find("text/html") == std::string::npos)) {
    Json files = Json::array();
    for (auto& it : items) {
      Json f = Json::object();
      f.set("name", it.name);
      f.set("size", double(it.size));
      f.set("url", "/download/" + it.name);
      f.set("mime", mime_type_for(it.name));
      files.push(f);
    }
    Json r = Json::object();
    r.set("files", files);
    resp.content_type = "application/json";
    resp.body = r.dump();
    return;
  }
  std::string h = "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                  "<title>EmberMesh downloads</title><style>body{font:16px/1.5 system-ui,sans-serif;max-width:40em;margin:2em auto;padding:0 1em;background:#111;color:#eee}a{color:#f93}</style>"
                  "</head><body><h1>EmberMesh downloads</h1><p>Copy the app to more devices. The single HTML file works offline in any browser.</p><ul>";
  for (auto& it : items) {
    std::string esc = html_escape(it.name);
    std::string href;
    for (unsigned char ch : it.name) {
      if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~')
        href += char(ch);
      else
        href += format("%%%02X", ch);
    }
    h += "<li><a href=\"/download/" + href + "\">" + esc + "</a> (" + format("%.1f", double(it.size) / 1024.0) + " KiB)</li>";
  }
  if (items.empty()) h += "<li>No files yet. Put the single-file build, APK or source archive in the hub's download directory.</li>";
  h += "</ul><p><a href=\"/\">Back to the app</a></p></body></html>";
  resp.content_type = "text/html; charset=utf-8";
  resp.body = h;
}

void Hub::handle_http(const HttpRequest& req, const SockAddr& peer, HttpResponse& resp) {
  const std::string& p = req.path;

  if (p == "/rpc" && (req.method == "POST" || req.method == "OPTIONS")) {
    if (req.method == "OPTIONS") {
      resp.status = 204;
      resp.headers.emplace_back("Allow", "POST, OPTIONS");
      return;
    }
    int a = authorize_rpc(req, peer);
    if (a != 0) {
      resp.status = a;
      resp.content_type = "application/json";
      resp.body = R"({"ok":false,"error":{"code":"denied","message":"admin token required"}})";
      return;
    }
    Json body;
    std::string perr;
    resp.content_type = "application/json";
    if (!Json::parse(req.body, &body, &perr) || !body.is_object() || body.get("method").as_string().empty()) {
      resp.status = 400;
      resp.body = R"({"ok":false,"error":{"code":"bad_params","message":"body must be {\"method\":…,\"params\":{…}}"}})";
      return;
    }
    Json env = d_.hub_rpc(body.get("method").as_string(), body.get("params"));
    if (body.has("id")) env.set("id", body.get("id"));
    resp.body = env.dump();
    return;
  }

  if (req.method != "GET" && req.method != "HEAD") {
    resp.status = 405;
    resp.headers.emplace_back("Allow", "GET, HEAD");
    resp.content_type = "text/plain; charset=utf-8";
    resp.body = "method not allowed\n";
    return;
  }

  if (p == "/hub.json") {
    Json j = d_.hub_probe_info();
    if (!j.is_object()) j = Json::object();
    j.set("embermesh_hub", true);  // key the web app checks (convention with apps/web)
    j.set("ember_hub", true);      // legacy alias
    if (!j.has("name")) j.set("name", "EmberMesh hub");
    j.set("version", opt_.version);
    j.set("ws", "/ws");
    j.set("rpc", "/rpc");
    j.set("download", "/download/");
    j.set("guests", double(guest_count()));
    resp.content_type = "application/json";
    resp.headers.emplace_back("Cache-Control", "no-store");
    resp.headers.emplace_back("Access-Control-Allow-Origin", "*");
    resp.body = j.dump();
    return;
  }

  if (p == "/download" || starts_with(p, "/download/")) {
    serve_download(req, resp);
    return;
  }

  if (opt_.captive_portal) {
    static const char* probes[] = {"/generate_204", "/gen_204", "/hotspot-detect.html", "/library/test/success.html",
                                   "/ncsi.txt", "/connecttest.txt", "/redirect", "/success.txt", "/canonical.html"};
    for (const char* pr : probes) {
      if (p != pr) continue;
      std::vector<std::string> u = urls();
      std::string target = u.empty() ? std::string("/") : u.front();
      // Prefer the URL on the guest's own subnet.
      std::string pip = peer.ip();
      size_t dot = pip.rfind('.');
      if (dot != std::string::npos)
        for (const auto& x : u)
          if (x.find("//" + pip.substr(0, dot + 1)) != std::string::npos) target = x;
      resp.status = 302;
      resp.headers.emplace_back("Location", target);
      resp.content_type = "text/html; charset=utf-8";
      resp.body = "<a href=\"" + html_escape(target) + "\">EmberMesh</a>\n";
      return;
    }
  }

  if (!opt_.web_root.empty()) {
    bool isdir = false;
    std::string f = resolve_static_path(opt_.web_root, p, &isdir);
    if (!f.empty()) {
      if (isdir && !p.empty() && p.back() != '/') {
        resp.status = 301;
        resp.headers.emplace_back("Location", req.target.substr(0, req.target.find('?')) + "/" +
                                                  (req.query.empty() ? "" : "?" + req.query));
        return;
      }
      resp.file_path = f;
      return;
    }
  }
  if (p == "/" || p == "/index.html") {
    resp.content_type = "text/html; charset=utf-8";
    resp.body = fallback_page();
    return;
  }
  resp.status = 404;
  resp.content_type = "text/plain; charset=utf-8";
  resp.body = "not found\n";
}

}  // namespace ember_host
