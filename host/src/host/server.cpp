#include "host/server.h"

#include <cstdio>

#include "host/stream.h"
#include "host/util.h"

namespace ember_host {

struct HttpServer::Conn : WsConn {
  enum Mode { Http, Ws, Dead };

  HttpServer& srv;
  uint64_t cid;
  TcpStream stream;
  SockAddr peer_addr;
  Mode mode = Http;
  std::string inbuf;
  HttpParser parser;
  uint64_t request_started_ms = 0;  // first byte of the current (incomplete) request
  uint64_t last_out_bytes = 0;
  uint64_t last_out_progress_ms = 0;
  bool closing = false;

  // file streaming
  std::FILE* file = nullptr;
  uint64_t file_remaining = 0;
  bool busy = false;
  bool keep_alive_after = true;

  // websocket
  HttpRequest ws_req;
  WsRoute* route = nullptr;
  WsDecoder dec;
  bool close_sent = false;
  bool close_received = false;
  bool on_close_called = false;
  uint64_t close_deadline = 0;
  uint64_t last_input_ms = 0;
  uint64_t last_ping_ms = 0;

  Conn(HttpServer& s, uint64_t id, sock_t sock, const SockAddr& peer)
      : srv(s), cid(id), stream(s.loop_), peer_addr(peer), parser(s.opt_.limits) {
    stream.on_data = [this](const uint8_t* d, size_t n) { on_data(d, n); };
    stream.on_drain = [this] { on_drain(); };
    stream.on_close = [this](const std::string& why) { teardown(why); };
    stream.adopt(sock, peer);
    last_input_ms = last_out_progress_ms = steady_ms();
  }
  ~Conn() override {
    if (file) std::fclose(file);
  }

  // ---- WsConn ------------------------------------------------------------------------------------
  uint64_t id() const override { return cid; }
  const HttpRequest& request() const override { return ws_req; }
  const SockAddr& peer() const override { return peer_addr; }
  size_t pending() const override { return stream.pending(); }
  bool open() const override { return mode == Ws && !close_sent && stream.is_open(); }

  bool ws_send(uint8_t op, const void* data, size_t len, bool droppable) {
    if (!open()) return false;
    size_t cap = route ? route->max_pending : (1u << 20);
    if (stream.pending() > cap) {
      if (droppable) return false;
      if (stream.pending() > cap * 4) {
        close(kWsClosePolicy, "client too slow");
        return false;
      }
    }
    std::string f = ws_encode_frame(op, data, len);
    stream.write(f);
    return true;
  }
  bool send_text(std::string_view t, bool droppable) override { return ws_send(kWsText, t.data(), t.size(), droppable); }
  bool send_binary(const void* d, size_t n, bool droppable) override { return ws_send(kWsBinary, d, n, droppable); }

  void close(uint16_t code, std::string_view reason) override {
    if (mode != Ws || close_sent) return;
    close_sent = true;
    std::string p = ws_close_payload(code, reason);
    stream.write(ws_encode_frame(kWsClose, p.data(), p.size()));
    if (close_received) {
      stream.close_after_flush();
    } else {
      close_deadline = steady_ms() + 3000;
    }
  }

  // ---- HTTP --------------------------------------------------------------------------------------
  size_t max_inbuf() const { return srv.opt_.limits.max_header_bytes + srv.opt_.limits.max_body + 4096; }

  void on_data(const uint8_t* d, size_t n) {
    last_input_ms = steady_ms();
    if (mode == Ws) {
      ws_bytes(d, n);
      return;
    }
    if (mode != Http || closing) return;
    if (inbuf.empty() && !busy) request_started_ms = steady_ms();
    inbuf.append(reinterpret_cast<const char*>(d), n);
    if (inbuf.size() > max_inbuf()) stream.set_read_paused(true);
    process();
  }

  void on_drain() {
    last_out_progress_ms = steady_ms();
    if (busy && file) pump_file();
  }

  void process() {
    while (mode == Http && !busy && !closing && !inbuf.empty()) {
      size_t used = 0;
      HttpParser::Result r = parser.parse(inbuf, &used);
      if (r == HttpParser::Result::NeedMore) {
        // Bounded: the parser errors out (431/413) before inbuf exceeds max_inbuf.
        stream.set_read_paused(inbuf.size() > max_inbuf());
        return;
      }
      if (r == HttpParser::Result::Error) {
        int st = parser.error_status();
        EH_LOGD("http %s: %d %s", peer_addr.to_string().c_str(), st, parser.error_message().c_str());
        send_error(st, parser.error_message(), false);
        return;
      }
      HttpRequest req = std::move(parser.request());
      inbuf.erase(0, used);
      request_started_ms = inbuf.empty() ? 0 : steady_ms();
      handle(req);
    }
    if (mode == Http && !busy && !closing) stream.set_read_paused(false);
  }

  void send_error(int status, const std::string& msg, bool keep) {
    std::vector<std::pair<std::string, std::string>> extra;
    std::string body = format("%d %s\n%s\n", status, http_reason(status), msg.c_str());
    stream.write(http_simple_response(status, "text/plain; charset=utf-8", body, keep, extra));
    if (!keep) {
      closing = true;
      stream.close_after_flush();
    }
  }

  void handle(HttpRequest& req) {
    // WebSocket routes.
    auto rit = srv.ws_routes_.find(req.path);
    if (rit != srv.ws_routes_.end() && (req.method == "GET" || ws_is_upgrade_request(req))) {
      if (!ws_is_upgrade_request(req)) {
        std::string body = "426 Upgrade Required\nThis is a WebSocket endpoint.\n";
        stream.write(http_simple_response(426, "text/plain; charset=utf-8", body, req.keep_alive,
                                          {{"Upgrade", "websocket"}, {"Sec-WebSocket-Version", "13"}}));
        finish(req.keep_alive);
        return;
      }
      std::string why;
      int st = ws_validate_upgrade(req, &why);
      if (st != 0) {
        std::vector<std::pair<std::string, std::string>> extra;
        if (st == 426) extra.emplace_back("Sec-WebSocket-Version", "13");
        stream.write(http_simple_response(st, "text/plain; charset=utf-8", why + "\n", false, extra));
        closing = true;
        stream.close_after_flush();
        return;
      }
      WsRoute& route_ref = rit->second;
      if (route_ref.authorize) {
        int a = route_ref.authorize(req, peer_addr);
        if (a != 0) {
          stream.write(http_simple_response(a, "text/plain; charset=utf-8", std::string(http_reason(a)) + "\n", false));
          closing = true;
          stream.close_after_flush();
          return;
        }
      }
      stream.write(ws_server_handshake_response(req));
      mode = Ws;
      route = &route_ref;
      ws_req = std::move(req);
      WsDecoder::Options o;
      o.expect_masked = true;
      o.max_message = route->max_message;
      dec = WsDecoder(o);
      last_ping_ms = steady_ms();
      stream.set_read_paused(false);
      std::string rest;
      rest.swap(inbuf);
      if (route->on_open) route->on_open(*this);
      if (mode == Ws && !rest.empty()) ws_bytes(reinterpret_cast<const uint8_t*>(rest.data()), rest.size());
      return;
    }

    static const char* known[] = {"GET", "HEAD", "POST", "OPTIONS", "PUT", "DELETE", "PATCH"};
    bool is_known = false;
    for (const char* m : known) is_known |= req.method == m;
    if (!is_known) {
      send_error(501, "method not implemented", false);
      return;
    }

    HttpResponse resp;
    if (srv.handler_) {
      srv.handler_(req, peer_addr, resp);
    } else {
      resp.status = 404;
      resp.body = "not found\n";
    }
    bool keep = req.keep_alive && !resp.close;
    bool head = req.method == "HEAD";
    if (!resp.file_path.empty()) {
      serve_file(req, resp, keep, head);
      return;
    }
    std::vector<std::pair<std::string, std::string>> hdrs = resp.headers;
    if (!resp.content_type.empty()) hdrs.emplace_back("Content-Type", resp.content_type);
    bool has_cc = false;
    for (auto& h : hdrs)
      if (iequals(h.first, "cache-control")) has_cc = true;
    if (!has_cc) hdrs.emplace_back("Cache-Control", "no-store");
    bool no_body = resp.status == 304 || resp.status == 204 || (resp.status >= 100 && resp.status < 200);
    std::string out = http_response_head(resp.status, hdrs, no_body ? -1 : int64_t(resp.body.size()), keep);
    if (!head && !no_body) out += resp.body;
    stream.write(out);
    finish(keep);
  }

  void serve_file(const HttpRequest& req, HttpResponse& resp, bool keep, bool head) {
    uint64_t size = 0;
    int64_t mtime = 0;
    if (!is_file(resp.file_path, &size, &mtime)) {
      send_simple(404, "not found\n", keep);
      return;
    }
    std::string etag = format("\"%llx-%llx\"", static_cast<unsigned long long>(size), static_cast<unsigned long long>(mtime));
    std::vector<std::pair<std::string, std::string>> hdrs = resp.headers;
    hdrs.emplace_back("Content-Type", resp.content_type.empty() ? mime_type_for(resp.file_path) : resp.content_type);
    hdrs.emplace_back("Accept-Ranges", "bytes");
    hdrs.emplace_back("ETag", etag);
    hdrs.emplace_back("Last-Modified", http_date(mtime));
    bool has_cc = false;
    for (auto& h : hdrs)
      if (iequals(h.first, "cache-control")) has_cc = true;
    if (!has_cc) hdrs.emplace_back("Cache-Control", "no-cache");
    hdrs.emplace_back("X-Content-Type-Options", "nosniff");

    const std::string* inm = req.header("if-none-match");
    if (inm && (*inm == etag || *inm == "*" || header_has_token(*inm, etag) || header_has_token(*inm, "W/" + etag))) {
      stream.write(http_response_head(304, hdrs, -1, keep));
      finish(keep);
      return;
    }
    uint64_t start = 0, len = size;
    int status = 200;
    const std::string* range = req.header("range");
    const std::string* if_range = req.header("if-range");
    if (range && (!if_range || *if_range == etag)) {
      int r = parse_range(*range, size, &start, &len);
      if (r < 0) {
        hdrs.emplace_back("Content-Range", format("bytes */%llu", static_cast<unsigned long long>(size)));
        stream.write(http_response_head(416, hdrs, 0, keep));
        finish(keep);
        return;
      }
      if (r > 0) {
        status = 206;
        hdrs.emplace_back("Content-Range", format("bytes %llu-%llu/%llu", static_cast<unsigned long long>(start),
                                                  static_cast<unsigned long long>(start + len - 1),
                                                  static_cast<unsigned long long>(size)));
      } else {
        start = 0;
        len = size;
      }
    }
    std::FILE* f = nullptr;
    if (!head && len > 0) {
      f = fopen_utf8(resp.file_path, "rb");
      if (!f || !fseek64(f, start)) {
        if (f) std::fclose(f);
        send_simple(500, "cannot read file\n", false);
        return;
      }
    }
    stream.write(http_response_head(status, hdrs, int64_t(len), keep));
    if (!f) {
      finish(keep);
      return;
    }
    file = f;
    file_remaining = len;
    busy = true;
    keep_alive_after = keep;
    pump_file();
  }

  void send_simple(int status, const std::string& body, bool keep) {
    stream.write(http_simple_response(status, "text/plain; charset=utf-8", body, keep));
    finish(keep);
  }

  void pump_file() {
    if (!file) return;
    char buf[64 * 1024];
    while (file_remaining > 0 && stream.pending() < srv.opt_.out_high_water && stream.is_open()) {
      size_t want = sizeof buf;
      if (want > srv.opt_.file_chunk) want = srv.opt_.file_chunk;
      if (uint64_t(want) > file_remaining) want = size_t(file_remaining);
      size_t got = std::fread(buf, 1, want, file);
      if (got == 0) {
        // File shrank underneath us: we cannot fix the Content-Length any more, so drop the conn.
        std::fclose(file);
        file = nullptr;
        busy = false;
        closing = true;
        stream.close_after_flush();
        return;
      }
      file_remaining -= got;
      stream.write(buf, got);
    }
    if (file_remaining == 0) {
      std::fclose(file);
      file = nullptr;
      busy = false;
      finish(keep_alive_after);
    }
  }

  void finish(bool keep) {
    if (!keep) {
      closing = true;
      stream.close_after_flush();
      return;
    }
    // Continue with pipelined requests (deferred to avoid deep recursion on long pipelines).
    if (!inbuf.empty()) {
      request_started_ms = steady_ms();
      auto alive = srv.alive_;
      uint64_t id = cid;
      HttpServer* s = &srv;
      srv.loop_.timer(0, [alive, s, id] {
        if (!*alive) return;
        auto it = s->conns_.find(id);
        if (it != s->conns_.end() && it->second->mode == Http) it->second->process();
      });
    } else {
      stream.set_read_paused(false);
    }
  }

  // ---- WebSocket ---------------------------------------------------------------------------------
  void ws_bytes(const uint8_t* d, size_t n) {
    dec.feed(d, n);
    while (mode == Ws) {
      WsEvent ev = dec.next();
      switch (ev.kind) {
        case WsEvent::None:
          return;
        case WsEvent::Message:
          if (close_sent) break;
          if (route && route->on_message) route->on_message(*this, ev.opcode, ev.payload);
          break;
        case WsEvent::Ping:
          if (!close_sent) {
            std::string f = ws_encode_frame(kWsPong, ev.payload.data(), ev.payload.size());
            stream.write(f);
          }
          break;
        case WsEvent::Pong:
          break;
        case WsEvent::Close: {
          close_received = true;
          if (!close_sent) {
            close_sent = true;
            std::string p = ev.close_code == kWsCloseNoStatus ? std::string() : ws_close_payload(ev.close_code, {});
            stream.write(ws_encode_frame(kWsClose, p.data(), p.size()));
          }
          stream.set_read_paused(true);
          stream.close_after_flush();
          return;
        }
        case WsEvent::Error: {
          EH_LOGD("ws %s: protocol error: %s", peer_addr.to_string().c_str(), ev.error.c_str());
          if (!close_sent) {
            close_sent = true;
            std::string p = ws_close_payload(ev.close_code, ev.error);
            stream.write(ws_encode_frame(kWsClose, p.data(), p.size()));
          }
          stream.set_read_paused(true);
          stream.close_after_flush();
          return;
        }
      }
    }
  }

  void teardown(const std::string& why) {
    if (mode == Dead) return;
    Mode was = mode;
    mode = Dead;
    if (was == Ws && route && route->on_close && !on_close_called) {
      on_close_called = true;
      route->on_close(*this);
    }
    stream.close();
    if (file) {
      std::fclose(file);
      file = nullptr;
    }
    srv.destroy_later(cid);
  }

  void sweep(uint64_t now) {
    if (mode == Dead) return;
    const Options& o = srv.opt_;
    if (stream.bytes_out() != last_out_bytes) {
      last_out_bytes = stream.bytes_out();
      last_out_progress_ms = now;
    }
    if (stream.pending() > 0 && now - last_out_progress_ms > o.write_stall_ms) {
      EH_LOGD("http %s: write stalled, dropping", peer_addr.to_string().c_str());
      teardown("write stall");
      return;
    }
    if (mode == Http) {
      if (closing) return;
      if (!inbuf.empty() && !busy && request_started_ms && now - request_started_ms > o.header_timeout_ms) {
        send_error(408, "request timeout", false);
        return;
      }
      if (inbuf.empty() && !busy && stream.pending() == 0 && now - stream.last_activity_ms() > o.idle_timeout_ms) {
        teardown("idle");
        return;
      }
    } else if (mode == Ws) {
      if (close_sent && close_deadline && now > close_deadline) {
        teardown("close handshake timeout");
        return;
      }
      if (now - last_input_ms > o.ws_timeout_ms) {
        teardown("ws timeout");
        return;
      }
      if (!close_sent && now - last_ping_ms >= o.ws_ping_interval_ms) {
        last_ping_ms = now;
        stream.write(ws_encode_frame(kWsPing, "ember", 5));
      }
    }
  }
};

HttpServer::HttpServer(Loop& loop, Options opt) : loop_(loop), opt_(std::move(opt)), alive_(std::make_shared<bool>(true)) {}

HttpServer::~HttpServer() {
  stop();
  *alive_ = false;
  if (reap_timer_) loop_.cancel(reap_timer_);
  graveyard_.clear();
}

bool HttpServer::start(std::string* err) {
  if (running()) return true;
  uint16_t bound = 0;
  listener_ = tcp_listen(opt_.bind, opt_.port, err, &bound);
  if (listener_ == kInvalidSock) return false;
  port_ = bound;
  listen_watch_ = loop_.watch(listener_, kPollIn, [this](short) { on_accept(); });
  auto alive = alive_;
  sweep_timer_ = loop_.timer(1000, [this, alive] {
    if (!*alive) return;
    sweep_timer_ = 0;
    sweep();
  });
  return true;
}

void HttpServer::stop() {
  if (listen_watch_) {
    loop_.unwatch(listen_watch_);
    listen_watch_ = 0;
  }
  if (listener_ != kInvalidSock) {
    sock_close(listener_);
    listener_ = kInvalidSock;
  }
  if (sweep_timer_) {
    loop_.cancel(sweep_timer_);
    sweep_timer_ = 0;
  }
  std::vector<uint64_t> ids;
  for (auto& kv : conns_) ids.push_back(kv.first);
  for (uint64_t id : ids) {
    auto it = conns_.find(id);
    if (it == conns_.end()) continue;
    Conn* c = it->second.get();
    if (c->mode == Conn::Ws && !c->close_sent) {
      c->close(kWsCloseGoingAway, "server shutting down");
    }
    c->teardown("server stopped");
  }
}

void HttpServer::on_accept() {
  for (int i = 0; i < 64; ++i) {
    SockAddr peer;
    sock_t c = tcp_accept(listener_, &peer);
    if (c == kInvalidSock) return;
    size_t same_ip = 0;
    std::string ip = peer.ip();
    for (auto& kv : conns_)
      if (kv.second->mode != Conn::Dead && kv.second->peer_addr.ip() == ip) ++same_ip;
    // The per-IP cap protects against one device on the Wi-Fi; processes on this machine (loopback)
    // are only bound by the global cap.
    if (conns_.size() >= opt_.max_conns || (same_ip >= opt_.max_conns_per_ip && !peer.is_loopback())) {
      std::string r = http_simple_response(503, "text/plain", "503 too many connections\n", false);
      sock_send(c, r.data(), r.size());
      sock_close(c);
      EH_LOGW("http: refusing connection from %s (limit reached)", peer.to_string().c_str());
      continue;
    }
    uint64_t id = next_conn_id_++;
    conns_[id] = std::make_unique<Conn>(*this, id, c, peer);
  }
}

void HttpServer::sweep() {
  uint64_t now = steady_ms();
  std::vector<uint64_t> ids;
  ids.reserve(conns_.size());
  for (auto& kv : conns_) ids.push_back(kv.first);
  for (uint64_t id : ids) {
    auto it = conns_.find(id);
    if (it != conns_.end()) it->second->sweep(now);
  }
  if (running()) {
    auto alive = alive_;
    sweep_timer_ = loop_.timer(1000, [this, alive] {
      if (!*alive) return;
      sweep_timer_ = 0;
      sweep();
    });
  }
}

void HttpServer::destroy_later(uint64_t id) {
  auto it = conns_.find(id);
  if (it == conns_.end()) return;
  graveyard_.push_back(std::move(it->second));
  conns_.erase(it);
  if (!reap_timer_) {
    auto alive = alive_;
    reap_timer_ = loop_.timer(0, [this, alive] {
      if (!*alive) return;
      reap_timer_ = 0;
      reap();
    });
  }
}

void HttpServer::reap() { graveyard_.clear(); }

size_t HttpServer::ws_count(const std::string& path) const {
  size_t n = 0;
  for (auto& kv : conns_)
    if (kv.second->mode == Conn::Ws && kv.second->ws_req.path == path) ++n;
  return n;
}

std::vector<WsConn*> HttpServer::ws_connections(const std::string& path) {
  std::vector<WsConn*> r;
  for (auto& kv : conns_)
    if (kv.second->mode == Conn::Ws && kv.second->ws_req.path == path) r.push_back(kv.second.get());
  return r;
}

}  // namespace ember_host
