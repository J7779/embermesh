// HTTP/1.1 server with static files (Range, HEAD, ETag), keep-alive + pipelining and RFC 6455
// WebSocket routes. Slow-client safe: bounded input/output buffers, file streaming paced by socket
// writability, header/idle/write-stall timeouts, connection caps (global and per IP).
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "host/http.h"
#include "host/loop.h"
#include "host/net.h"
#include "host/ws.h"

namespace ember_host {

struct HttpResponse {
  int status = 200;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;
  std::string content_type;  // for body responses
  std::string file_path;     // if set: serve this file (Range/HEAD/ETag handled by the server)
  bool close = false;        // force Connection: close
};

// A live WebSocket connection (handle given to route callbacks). Loop thread only.
class WsConn {
 public:
  virtual ~WsConn() = default;
  virtual uint64_t id() const = 0;
  virtual const HttpRequest& request() const = 0;  // the upgrade request (path, query, headers)
  virtual const SockAddr& peer() const = 0;
  // Returns false if the message was dropped (droppable and the client is too slow) or the
  // connection is closing. Non-droppable sends close a hopelessly slow client (1008).
  virtual bool send_text(std::string_view text, bool droppable = false) = 0;
  virtual bool send_binary(const void* data, size_t len, bool droppable = false) = 0;
  virtual void close(uint16_t code = kWsCloseNormal, std::string_view reason = {}) = 0;
  virtual size_t pending() const = 0;
  virtual bool open() const = 0;
  void* user = nullptr;  // route-owned per-connection data
};

class HttpServer {
 public:
  struct Options {
    std::string bind = "0.0.0.0";
    uint16_t port = 8437;  // 0 = ephemeral (tests)
    size_t max_conns = 256;
    size_t max_conns_per_ip = 48;
    HttpLimits limits;
    uint64_t header_timeout_ms = 15000;   // whole request head must arrive within this
    uint64_t idle_timeout_ms = 60000;     // keep-alive idle
    uint64_t write_stall_ms = 60000;      // no output progress while data is pending
    uint64_t ws_ping_interval_ms = 30000;
    uint64_t ws_timeout_ms = 90000;       // no input at all (incl. pongs) for this long -> drop
    size_t file_chunk = 64 * 1024;
    size_t out_high_water = 256 * 1024;   // stop producing file data above this
  };
  struct WsRoute {
    size_t max_message = 1 << 20;
    size_t max_pending = 1 << 20;  // outbound buffer before droppable sends are dropped
    // Return 0 to accept, or an HTTP status (401/403/...) to refuse the upgrade.
    std::function<int(const HttpRequest&, const SockAddr& peer)> authorize;
    std::function<void(WsConn&)> on_open;
    std::function<void(WsConn&, uint8_t opcode, std::string& data)> on_message;
    std::function<void(WsConn&)> on_close;
    // Subprotocols this route speaks (Sec-WebSocket-Protocol). The first one the client offers is
    // echoed in the 101 response; a client that offers none is still accepted.
    std::vector<std::string> subprotocols;
  };
  using Handler = std::function<void(const HttpRequest&, const SockAddr& peer, HttpResponse&)>;

  HttpServer(Loop& loop, Options opt);
  ~HttpServer();
  HttpServer(const HttpServer&) = delete;
  HttpServer& operator=(const HttpServer&) = delete;

  void set_handler(Handler h) { handler_ = std::move(h); }
  void add_ws_route(const std::string& path, WsRoute route) { ws_routes_[path] = std::move(route); }

  bool start(std::string* err);
  void stop();  // closes listener and all connections (WS routes get on_close)
  bool running() const { return listener_ != kInvalidSock; }
  uint16_t port() const { return port_; }
  const Options& options() const { return opt_; }
  size_t connection_count() const { return conns_.size(); }
  size_t ws_count(const std::string& path) const;
  std::vector<WsConn*> ws_connections(const std::string& path);

 private:
  struct Conn;
  friend struct Conn;
  void on_accept();
  void sweep();
  void destroy_later(uint64_t id);
  void reap();

  Loop& loop_;
  Options opt_;
  Handler handler_;
  std::map<std::string, WsRoute> ws_routes_;
  sock_t listener_ = kInvalidSock;
  uint64_t listen_watch_ = 0;
  uint64_t sweep_timer_ = 0;
  uint16_t port_ = 0;
  uint64_t next_conn_id_ = 1;
  std::map<uint64_t, std::unique_ptr<Conn>> conns_;
  std::vector<std::unique_ptr<Conn>> graveyard_;
  uint64_t reap_timer_ = 0;
  std::shared_ptr<bool> alive_;
};

}  // namespace ember_host
