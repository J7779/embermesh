// The hub (ARCHITECTURE §5.2, §6): one HTTP server on port 8437 that
//   /            serves the web app from the web root (or a built-in fallback page),
//   /ws          is the mesh bus: binary WebSocket, one mesh frame per message; every guest shares ONE
//                core link of type "raw" (frames from the core go to every guest, frames from a guest
//                go to the core — the mesh engine does the relaying),
//   /rpc         is the token-protected remote-control JSON-RPC WebSocket for the operator's browser
//                (RPC_API §5) — also POST /rpc for curl/scripts,
//   /download/   serves files for spreading the app (single-file HTML, APK, source archive),
//   /hub.json    is the hub probe: {"embermesh_hub":true,"name":…,"ws":"/ws",…}.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "host/json.h"
#include "host/mqtt_broker.h"
#include "host/loop.h"
#include "host/server.h"

namespace ember_host {

class HubDelegate {
 public:
  virtual ~HubDelegate() = default;
  virtual void hub_bus_rx(const uint8_t* frame, size_t len) = 0;  // a guest sent a mesh frame
  // Remote-control RPC; returns the reply envelope {"ok":…,"result"|"error":…}.
  virtual Json hub_rpc(const std::string& method, const Json& params) = 0;
  virtual void hub_clients_changed(size_t guests) = 0;
  virtual Json hub_probe_info() = 0;  // extra fields for /hub.json (name, node_id)
};

struct HubOptions {
  std::string bind = "0.0.0.0";
  uint16_t port = 8437;
  std::string web_root;                     // "" -> built-in fallback page
  std::vector<std::string> download_dirs;   // first match wins
  std::vector<std::string> download_files;  // single files offered at /download/<basename>
  std::string admin_token;                  // "" -> /rpc refused (unless local_no_token)
  bool local_no_token = false;              // loopback clients may use /rpc without the token
  bool captive_portal = false;              // answer OS connectivity probes with a redirect to us
  size_t max_guests = 200;
  // Largest WebSocket message accepted on /ws: the hub link's raw `websocket` preset mtu. Bigger
  // messages close the guest (1009) before the payload is buffered.
  size_t max_frame = 4096;
  // All guests share ONE core link, and the core rate-limits frames per link (400/s, counted before
  // parsing). The hub therefore feeds the core at most `bus_frames_per_s` in total, served
  // round-robin from bounded per-guest queues (fair queuing), so one hostile guest cannot starve
  // the others; a guest that keeps overflowing its own budget is disconnected.
  double bus_frames_per_s = 300;
  double guest_frames_per_s = 100;  // per-guest sustained rate (burst = 2 s worth)
  double guest_bytes_per_s = 256 * 1024;
  size_t guest_queue_frames = 64;
  uint64_t guest_abuse_drops = 2000;  // frames dropped within 10 s before the guest is evicted
  std::string version;
  // /mqtt: minimal MQTT broker so the hub can be an internet relay (CONNECTIVITY §4.12).
  bool mqtt = false;
  uint16_t mqtt_port = 8438;  // its own listener: never the hub port (0 = any free port)
};

class Hub {
 public:
  Hub(Loop& loop, HubDelegate& d);
  ~Hub();
  Hub(const Hub&) = delete;
  Hub& operator=(const Hub&) = delete;

  bool start(const HubOptions& opt, std::string* err);
  void stop();
  bool running() const { return server_ && server_->running(); }
  uint16_t port() const { return server_ ? server_->port() : 0; }
  uint16_t mqtt_port() const { return mqtt_server_ ? mqtt_server_->port() : 0; }
  const HubOptions& options() const { return opt_; }
  std::vector<std::string> urls() const;  // http://<lan-ip>:<port>/ for every usable interface
  size_t guest_count() const;
  size_t rpc_count() const;
  void set_admin_token(const std::string& t) { opt_.admin_token = t; }

  // Core -> every guest. Slow guests drop frames instead of buffering without bound.
  void broadcast_frame(const uint8_t* data, size_t len);
  // Event JSON -> every remote-control session.
  void broadcast_event(const std::string& event_json);

  Json stats() const;

 private:
  struct Guest;
  void handle_http(const HttpRequest& req, const SockAddr& peer, HttpResponse& resp);
  void serve_download(const HttpRequest& req, HttpResponse& resp);
  int authorize_rpc(const HttpRequest& req, const SockAddr& peer) const;
  void on_rpc_message(WsConn& c, std::string& text);
  std::string fallback_page() const;
  void pump_bus();  // round-robin from guest queues into the core, within the shared budget

  Loop& loop_;
  HubDelegate& d_;
  HubOptions opt_;
  std::unique_ptr<HttpServer> server_;
  std::unique_ptr<MqttBroker> mqtt_;
  std::unique_ptr<HttpServer> mqtt_server_;  // only /mqtt, nothing else
  uint64_t frames_in_ = 0, frames_out_ = 0, frames_dropped_ = 0, guests_total_ = 0, guests_evicted_ = 0;
  uint64_t frames_rate_limited_ = 0, guests_abusive_ = 0;
  double bus_tokens_ = 0;
  uint64_t bus_tokens_ms_ = 0;
  uint64_t pump_timer_ = 0;
  size_t rr_next_ = 0;
};

// Authorization helper (exposed for tests): token from ?token= or "Authorization: Bearer <tok>".
std::string request_token(const HttpRequest& req);

}  // namespace ember_host
