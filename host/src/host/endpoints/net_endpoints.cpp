// Network endpoints: tcp (client), tcp_server, udp (LAN multicast per ARCHITECTURE §6), websocket
// (client, hub-to-hub bridging) and loopback. Byte pipes only.
#include <set>

#include "host/endpoint.h"
#include "host/stream.h"
#include "host/util.h"
#include "host/ws_client.h"

namespace ember_host {

namespace {

constexpr size_t kMaxPending = 1 << 20;  // per-endpoint output cap; beyond it frames are dropped

// Defers a state report so open() never calls back synchronously.
template <class F>
void defer(Loop& loop, const std::shared_ptr<bool>& alive, F fn) {
  std::weak_ptr<bool> w = alive;
  loop.timer(0, [w, fn] {
    auto a = w.lock();
    if (a && *a) fn();
  });
}

// ---- tcp client ----------------------------------------------------------------------------------

class TcpEndpoint : public Endpoint {
 public:
  TcpEndpoint(Loop& l, EndpointSink& s, const Json& c) : Endpoint(l, s, c), stream_(l), alive_(std::make_shared<bool>(true)) {
    host_ = cfg_str(c, "host", "127.0.0.1");
    port_ = uint16_t(cfg_int(c, "port", 8001));
    stream_.on_connect = [this](const std::string& err) {
      if (!err.empty()) {
        set_state(false, err);
        return;
      }
      EH_LOGI("tcp: connected to %s:%u", host_.c_str(), port_);
      set_state(true);
    };
    stream_.on_data = [this](const uint8_t* d, size_t n) { deliver(d, n); };
    stream_.on_close = [this](const std::string& why) { set_state(false, why); };
  }
  ~TcpEndpoint() override { *alive_ = false; }
  const char* kind() const override { return "tcp"; }
  void open() override { stream_.connect(host_, port_, 10000); }
  void close() override { stream_.close(); }
  void write(const uint8_t* d, size_t n) override {
    if (!stream_.connected()) return;
    if (stream_.pending() > kMaxPending) {
      ++dropped_;
      return;
    }
    stream_.write(d, n);
  }
  Json status() const override {
    Json s = Json::object();
    s.set("host", host_);
    s.set("port", port_);
    s.set("rx_bytes", double(stream_.bytes_in()));
    s.set("tx_bytes", double(stream_.bytes_out()));
    s.set("tx_pending", double(stream_.pending()));
    if (dropped_) s.set("tx_dropped", double(dropped_));
    return s;
  }

 private:
  std::string host_;
  uint16_t port_;
  TcpStream stream_;
  uint64_t dropped_ = 0;
  std::shared_ptr<bool> alive_;
};

// ---- tcp server ----------------------------------------------------------------------------------
// Accepts one client at a time (a stream link cannot interleave several byte streams); a new client
// replaces the old one (typical after a client reconnects while the old socket is half-dead).

class TcpServerEndpoint : public Endpoint {
 public:
  TcpServerEndpoint(Loop& l, EndpointSink& s, const Json& c) : Endpoint(l, s, c), alive_(std::make_shared<bool>(true)) {
    bind_ = cfg_str(c, "bind", "0.0.0.0");
    port_ = uint16_t(cfg_int(c, "port", 8001));
  }
  ~TcpServerEndpoint() override {
    *alive_ = false;
    close();
  }
  const char* kind() const override { return "tcp_server"; }
  bool auto_reconnect() const override { return false; }  // retries listening itself
  void open() override {
    close();
    want_open_ = true;
    try_listen();
  }
  void close() override {
    want_open_ = false;
    if (retry_timer_) {
      loop_.cancel(retry_timer_);
      retry_timer_ = 0;
    }
    client_.reset();
    if (watch_) {
      loop_.unwatch(watch_);
      watch_ = 0;
    }
    if (listener_ != kInvalidSock) {
      sock_close(listener_);
      listener_ = kInvalidSock;
    }
  }
  void write(const uint8_t* d, size_t n) override {
    if (!client_ || !client_->connected()) return;
    if (client_->pending() > kMaxPending) return;
    client_->write(d, n);
  }
  Json status() const override {
    Json s = Json::object();
    s.set("bind", bind_);
    s.set("port", bound_port_ ? bound_port_ : port_);
    s.set("listening", listener_ != kInvalidSock);
    s.set("client", client_ && client_->connected() ? client_->peer().to_string() : std::string());
    return s;
  }
  uint16_t bound_port() const { return bound_port_; }

 private:
  void try_listen() {
    std::string err;
    listener_ = tcp_listen(bind_, port_, &err, &bound_port_);
    if (listener_ == kInvalidSock) {
      EH_LOGW("tcp_server: %s (retrying in 5 s)", err.c_str());
      defer(loop_, alive_, [this, err] { set_state(false, err); });
      retry_timer_ = loop_.timer(5000, [this] {
        retry_timer_ = 0;
        if (want_open_) try_listen();
      });
      return;
    }
    EH_LOGI("tcp_server: listening on %s:%u", bind_.c_str(), bound_port_);
    watch_ = loop_.watch(listener_, kPollIn, [this](short) { accept(); });
    sink().ep_status_changed(*this);
  }
  void accept() {
    SockAddr peer;
    sock_t c = tcp_accept(listener_, &peer);
    if (c == kInvalidSock) return;
    bool had = client_ && client_->connected();
    client_ = std::make_unique<TcpStream>(loop_);
    client_->on_data = [this](const uint8_t* d, size_t n) { deliver(d, n); };
    client_->on_close = [this](const std::string& why) {
      EH_LOGI("tcp_server: client left (%s)", why.c_str());
      std::weak_ptr<bool> w = alive_;
      // Destroy the stream outside its own callback.
      loop_.timer(0, [this, w] {
        auto a = w.lock();
        if (!a || !*a) return;
        if (client_ && !client_->is_open()) client_.reset();
      });
      set_state(false, "");
    };
    client_->adopt(c, peer);
    EH_LOGI("tcp_server: client %s%s", peer.to_string().c_str(), had ? " (replaced previous client)" : "");
    set_state(true);
  }

  std::string bind_;
  uint16_t port_;
  uint16_t bound_port_ = 0;
  sock_t listener_ = kInvalidSock;
  uint64_t watch_ = 0;
  uint64_t retry_timer_ = 0;
  bool want_open_ = false;
  std::unique_ptr<TcpStream> client_;
  std::shared_ptr<bool> alive_;
};

// ---- udp multicast -------------------------------------------------------------------------------

class UdpEndpoint : public Endpoint {
 public:
  UdpEndpoint(Loop& l, EndpointSink& s, const Json& c) : Endpoint(l, s, c), alive_(std::make_shared<bool>(true)) {
    group_ = cfg_str(c, "group", "239.83.77.69");
    port_ = uint16_t(cfg_int(c, "port", 47474));
    iface_ = cfg_str(c, "interface", "");
    ttl_ = int(cfg_int(c, "ttl", 1));
    loop_back_ = cfg_bool(c, "loopback", true);
    for (const auto& p : split(cfg_str(c, "peers", ""), ',')) {
      std::string t = trim(p);
      if (t.empty()) continue;
      std::string h = t;
      uint64_t pp = port_;
      size_t colon = t.rfind(':');
      if (colon != std::string::npos && t.find(']') == std::string::npos) {
        h = t.substr(0, colon);
        if (!parse_u64(t.substr(colon + 1), &pp) || pp == 0 || pp > 65535) continue;
      }
      SockAddr a;
      if (resolve_numeric(h, uint16_t(pp), &a, 4)) peers_.push_back(a);
    }
  }
  ~UdpEndpoint() override {
    *alive_ = false;
    close();
  }
  const char* kind() const override { return "udp"; }
  bool datagram() const override { return true; }
  void open() override {
    close();
    std::string err;
    rx_ = udp_socket(4, &err);
    tx_ = udp_socket(4, &err);
    if (rx_ == kInvalidSock || tx_ == kInvalidSock || !udp_bind(rx_, "0.0.0.0", port_, true, &err) ||
        !udp_bind(tx_, "0.0.0.0", 0, false, &err)) {
      close();
      defer(loop_, alive_, [this, err] { set_state(false, err); });
      return;
    }
    udp_set_multicast_ttl_loop(tx_, ttl_, loop_back_);
    tx_port_ = sock_local_addr(tx_).port();
    if (!resolve_numeric(group_, port_, &group_addr_, 4)) {
      close();
      defer(loop_, alive_, [this] { set_state(false, "bad multicast group " + group_); });
      return;
    }
    join_interfaces();
    rx_watch_ = loop_.watch(rx_, kPollIn, [this](short) { on_readable(rx_); });
    tx_watch_ = loop_.watch(tx_, kPollIn, [this](short) { on_readable(tx_); });  // unicast replies
    rejoin_timer_ = loop_.timer(30000, [this] { rejoin(); });
    EH_LOGI("udp: multicast %s:%u on %zu interface(s), tx port %u", group_.c_str(), port_, joined_.size(), tx_port_);
    defer(loop_, alive_, [this] { set_state(true); });
  }
  void close() override {
    if (rejoin_timer_) {
      loop_.cancel(rejoin_timer_);
      rejoin_timer_ = 0;
    }
    if (rx_watch_) loop_.unwatch(rx_watch_);
    if (tx_watch_) loop_.unwatch(tx_watch_);
    rx_watch_ = tx_watch_ = 0;
    sock_close(rx_);
    sock_close(tx_);
    rx_ = tx_ = kInvalidSock;
    joined_.clear();
    local_ips_.clear();
  }
  void write(const uint8_t* d, size_t n) override {
    if (tx_ == kInvalidSock || n == 0 || n > 65507) return;
    if (joined_.empty()) {
      if (udp_sendto(tx_, d, n, group_addr_) < 0) ++tx_errors_;
    } else {
      for (const auto& ip : joined_) {
        udp_set_multicast_if_v4(tx_, ip == "*" ? std::string() : ip);
        if (udp_sendto(tx_, d, n, group_addr_) < 0) ++tx_errors_;
      }
    }
    for (const auto& p : peers_)
      if (udp_sendto(tx_, d, n, p) < 0) ++tx_errors_;
    ++tx_frames_;
  }
  Json status() const override {
    Json s = Json::object();
    s.set("group", group_);
    s.set("port", port_);
    Json ifs = Json::array();
    for (const auto& ip : joined_) ifs.push(ip);
    s.set("interfaces", ifs);
    s.set("tx_frames", double(tx_frames_));
    s.set("rx_frames", double(rx_frames_));
    if (tx_errors_) s.set("tx_errors", double(tx_errors_));
    return s;
  }

 private:
  void join_interfaces() {
    local_ips_.clear();
    for (const auto& i : list_ipv4_interfaces()) local_ips_.insert(i.ip);
    if (!iface_.empty()) {
      std::string err;
      if (joined_.count(iface_) == 0 && udp_join_multicast_v4(rx_, group_, iface_, &err)) joined_.insert(iface_);
      else if (!err.empty()) EH_LOGW("udp: %s", err.c_str());
      return;
    }
    for (const auto& i : list_ipv4_interfaces()) {
      if (i.loopback || !i.up || !i.multicast) continue;
      if (joined_.count(i.ip)) continue;
      std::string err;
      if (udp_join_multicast_v4(rx_, group_, i.ip, &err)) {
        joined_.insert(i.ip);
      } else {
        EH_LOGD("udp: %s", err.c_str());
      }
    }
    if (joined_.empty()) {
      // No usable interface (e.g. no network yet): join on the default route so we hear loopback.
      std::string err;
      if (udp_join_multicast_v4(rx_, group_, "", &err)) joined_.insert("*");
    }
  }
  void rejoin() {
    rejoin_timer_ = 0;
    if (rx_ == kInvalidSock) return;
    size_t before = joined_.size();
    if (joined_.count("*") && joined_.size() == 1) {
      // Retry real interfaces once one shows up (hotspot started after us).
      bool any = false;
      for (const auto& i : list_ipv4_interfaces())
        if (!i.loopback && i.up && i.multicast) any = true;
      if (any) joined_.clear();
    }
    join_interfaces();
    if (joined_.size() != before) {
      EH_LOGI("udp: now on %zu interface(s)", joined_.size());
      sink().ep_status_changed(*this);
    }
    rejoin_timer_ = loop_.timer(30000, [this] { rejoin(); });
  }
  void on_readable(sock_t s) {
    uint8_t buf[65536];
    for (int i = 0; i < 64; ++i) {
      SockAddr from;
      long n = udp_recvfrom(s, buf, sizeof buf, &from);
      if (n < 0) return;
      // Our own multicast looped back: same tx port and one of our addresses.
      if (from.port() == tx_port_ && (local_ips_.count(from.ip()) || from.is_loopback())) continue;
      if (n == 0) continue;
      ++rx_frames_;
      deliver(buf, size_t(n));
      if (rx_ == kInvalidSock) return;  // closed from the callback
    }
  }

  std::string group_, iface_;
  uint16_t port_;
  int ttl_;
  bool loop_back_;
  std::vector<SockAddr> peers_;
  SockAddr group_addr_;
  sock_t rx_ = kInvalidSock, tx_ = kInvalidSock;
  uint16_t tx_port_ = 0;
  uint64_t rx_watch_ = 0, tx_watch_ = 0, rejoin_timer_ = 0;
  std::set<std::string> joined_, local_ips_;
  uint64_t tx_frames_ = 0, rx_frames_ = 0, tx_errors_ = 0;
  std::shared_ptr<bool> alive_;
};

// ---- websocket client ----------------------------------------------------------------------------

class WebSocketEndpoint : public Endpoint {
 public:
  WebSocketEndpoint(Loop& l, EndpointSink& s, const Json& c) : Endpoint(l, s, c), ws_(l), alive_(std::make_shared<bool>(true)) {
    url_ = cfg_str(c, "url", "");
    ws_.on_open = [this] {
      EH_LOGI("websocket: connected to %s", url_.c_str());
      set_state(true);
    };
    ws_.on_message = [this](uint8_t op, std::string& data) {
      if (op != kWsBinary) return;  // the mesh bus is binary; ignore text
      ++rx_frames_;
      deliver(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    };
    ws_.on_close = [this](uint16_t code, const std::string& reason) {
      set_state(false, reason.empty() ? format("closed (%u)", code) : reason);
    };
  }
  ~WebSocketEndpoint() override { *alive_ = false; }
  const char* kind() const override { return "websocket"; }
  bool datagram() const override { return true; }
  void open() override {
    std::string err;
    if (!ws_.connect(url_, &err)) defer(loop_, alive_, [this, err] { set_state(false, err); });
  }
  void close() override { ws_.abort(); }
  void write(const uint8_t* d, size_t n) override {
    if (!ws_.is_open()) return;
    if (ws_.pending() > kMaxPending) return;
    ws_.send_binary(d, n);
    ++tx_frames_;
  }
  Json status() const override {
    Json s = Json::object();
    s.set("url", url_);
    s.set("tx_frames", double(tx_frames_));
    s.set("rx_frames", double(rx_frames_));
    return s;
  }

 private:
  std::string url_;
  WsClient ws_;
  uint64_t tx_frames_ = 0, rx_frames_ = 0;
  std::shared_ptr<bool> alive_;
};

// ---- loopback ------------------------------------------------------------------------------------

class LoopbackEndpoint : public Endpoint {
 public:
  LoopbackEndpoint(Loop& l, EndpointSink& s, const Json& c) : Endpoint(l, s, c), alive_(std::make_shared<bool>(true)) {}
  ~LoopbackEndpoint() override { *alive_ = false; }
  const char* kind() const override { return "loopback"; }
  bool datagram() const override { return true; }
  bool auto_reconnect() const override { return false; }
  void open() override {
    open_ = true;
    defer(loop_, alive_, [this] { set_state(true); });
  }
  void close() override { open_ = false; }
  void write(const uint8_t* d, size_t n) override {
    if (!open_) return;
    std::string copy(reinterpret_cast<const char*>(d), n);
    defer(loop_, alive_, [this, copy] {
      if (open_) deliver(reinterpret_cast<const uint8_t*>(copy.data()), copy.size());
    });
  }

 private:
  bool open_ = false;
  std::shared_ptr<bool> alive_;
};

// Probes TCP ports with parallel non-blocking connects; returns the ones that accepted.
std::vector<std::pair<std::string, uint16_t>> probe_tcp(const std::vector<std::pair<std::string, uint16_t>>& targets,
                                                        int timeout_ms) {
  std::vector<std::pair<std::string, uint16_t>> ok;
  std::vector<PollItem> items;
  std::vector<size_t> idx;
  for (size_t i = 0; i < targets.size(); ++i) {
    SockAddr a;
    if (!resolve_numeric(targets[i].first, targets[i].second, &a)) continue;
    bool prog = false;
    sock_t s = tcp_connect_start(a, &prog, nullptr);
    if (s == kInvalidSock) continue;
    if (!prog) {
      ok.push_back(targets[i]);
      sock_close(s);
      continue;
    }
    items.push_back(PollItem{s, kPollOut, 0});
    idx.push_back(i);
  }
  uint64_t deadline = steady_ms() + uint64_t(timeout_ms);
  std::vector<bool> done(items.size(), false);
  size_t remaining = items.size();
  while (remaining > 0) {
    uint64_t now = steady_ms();
    if (now >= deadline) break;
    std::vector<PollItem> live;
    std::vector<size_t> map;
    for (size_t i = 0; i < items.size(); ++i)
      if (!done[i]) {
        live.push_back(items[i]);
        map.push_back(i);
      }
    int rc = poll_sockets(live.data(), live.size(), int(deadline - now));
    if (rc <= 0) break;
    for (size_t k = 0; k < live.size(); ++k) {
      if (!live[k].revents) continue;
      size_t i = map[k];
      done[i] = true;
      --remaining;
      if (sock_pending_error(items[i].fd) == 0 && (live[k].revents & kPollOut)) ok.push_back(targets[idx[i]]);
    }
  }
  for (auto& it : items) sock_close(it.fd);
  return ok;
}

}  // namespace

void register_net_endpoints(EndpointRegistry& r) {
  {
    EndpointKind k;
    k.kind = "tcp";
    k.title = "TCP client";
    k.description = "Connect to a TCP server: Direwolf / soundmodem KISS (port 8001), Meshtastic (4403), serial-to-network bridges, another node.";
    k.scannable = true;
    k.config.push(Field("host", "Host", "text").def("127.0.0.1").required().help("IP address or hostname of the server."));
    k.config.push(Field("port", "Port", "number").def(8001).min(1).max(65535).required().help("Direwolf KISS 8001, Meshtastic 4403."));
    k.link_types = {"kiss", "stream", "meshtastic", "rnode", "lora_at", "raw"};
    k.default_link_type = "kiss";
    k.create = [](Loop& l, EndpointSink& s, const Json& c, std::string* err) -> std::unique_ptr<Endpoint> {
      if (cfg_str(c, "host").empty()) {
        *err = "host is required";
        return nullptr;
      }
      int64_t p = cfg_int(c, "port", 0);
      if (p < 1 || p > 65535) {
        *err = "port must be 1..65535";
        return nullptr;
      }
      return std::make_unique<TcpEndpoint>(l, s, c);
    };
    k.scan = [](const Json& params) {
      int t = int(cfg_int(params, "timeout_ms", 400));
      if (t < 50) t = 50;
      if (t > 1500) t = 1500;
      struct Known {
        uint16_t port;
        const char* name;
        const char* link;
      };
      static const Known known[] = {{8001, "Direwolf / soundmodem KISS", "kiss"}, {4403, "Meshtastic (TCP API)", "meshtastic"}};
      std::vector<std::pair<std::string, uint16_t>> targets;
      for (const auto& kn : known) targets.emplace_back("127.0.0.1", kn.port);
      Json devs = Json::array();
      for (const auto& hit : probe_tcp(targets, t)) {
        for (const auto& kn : known) {
          if (kn.port != hit.second) continue;
          Json d = Json::object();
          d.set("id", hit.first + ":" + std::to_string(hit.second));
          d.set("name", std::string(kn.name) + " on localhost:" + std::to_string(kn.port));
          d.set("detail", std::string("suggested link type: ") + kn.link);
          Json c = Json::object();
          c.set("host", hit.first);
          c.set("port", hit.second);
          d.set("config", c);
          d.set("link_type", kn.link);
          devs.push(d);
        }
      }
      Json res = Json::object();
      res.set("devices", devs);
      return res;
    };
    r.add(std::move(k));
  }
  {
    EndpointKind k;
    k.kind = "tcp_server";
    k.title = "TCP server";
    k.description = "Listen for one TCP client (e.g. a KISS client app or another node connecting to us). A new client replaces the previous one.";
    k.config.push(Field("bind", "Bind address", "text").def("0.0.0.0").advanced());
    k.config.push(Field("port", "Port", "number").def(8001).min(1).max(65535).required());
    k.link_types = {"stream", "kiss", "raw"};
    k.default_link_type = "stream";
    k.create = [](Loop& l, EndpointSink& s, const Json& c, std::string* err) -> std::unique_ptr<Endpoint> {
      int64_t p = cfg_int(c, "port", 0);
      if (p < 1 || p > 65535) {
        *err = "port must be 1..65535";
        return nullptr;
      }
      return std::make_unique<TcpServerEndpoint>(l, s, c);
    };
    r.add(std::move(k));
  }
  {
    EndpointKind k;
    k.kind = "udp";
    k.title = "LAN multicast (Wi-Fi / Ethernet)";
    k.description = "UDP multicast 239.83.77.69:47474 on the local network — talks to every EmberMesh phone and computer on the same Wi-Fi or hotspot.";
    k.scannable = true;
    k.config.push(Field("group", "Multicast group", "text").def("239.83.77.69").advanced());
    k.config.push(Field("port", "Port", "number").def(47474).min(1).max(65535).advanced());
    k.config.push(Field("interface", "Interface address", "text").def("").help("Empty = all interfaces.").advanced());
    k.config.push(Field("ttl", "Multicast TTL", "number").def(1).min(1).max(32).advanced());
    k.config.push(Field("loopback", "Hear other apps on this computer", "bool").def(true).advanced());
    k.config.push(Field("peers", "Extra unicast peers", "text").def("").help("Comma-separated ip[:port] for networks that block multicast.").advanced());
    k.link_types = {"raw"};
    k.default_link_type = "raw";
    k.create = [](Loop& l, EndpointSink& s, const Json& c, std::string* err) -> std::unique_ptr<Endpoint> {
      int64_t p = cfg_int(c, "port", 47474);
      if (p < 1 || p > 65535) {
        *err = "port must be 1..65535";
        return nullptr;
      }
      return std::make_unique<UdpEndpoint>(l, s, c);
    };
    k.scan = [](const Json&) {
      Json devs = Json::array();
      for (const auto& i : list_ipv4_interfaces()) {
        if (i.loopback || !i.up) continue;
        Json d = Json::object();
        d.set("id", i.ip);
        d.set("name", i.name + " (" + i.ip + ")");
        d.set("detail", i.multicast ? "multicast capable" : "no multicast");
        Json c = Json::object();
        c.set("interface", i.ip);
        d.set("config", c);
        devs.push(d);
      }
      Json res = Json::object();
      res.set("devices", devs);
      return res;
    };
    r.add(std::move(k));
  }
  {
    EndpointKind k;
    k.kind = "websocket";
    k.title = "WebSocket (hub-to-hub)";
    k.description = "Connect to another hub's mesh bus, e.g. ws://10.42.0.1:8437/ws, over any IP network.";
    k.config.push(Field("url", "Hub URL", "text").def("ws://10.42.0.1:8437/ws").required().help("ws:// only (no TLS)."));
    k.link_types = {"raw"};
    k.default_link_type = "raw";
    k.create = [](Loop& l, EndpointSink& s, const Json& c, std::string* err) -> std::unique_ptr<Endpoint> {
      std::string h, p;
      uint16_t port;
      if (!ws_parse_url(cfg_str(c, "url"), &h, &port, &p, err)) return nullptr;
      return std::make_unique<WebSocketEndpoint>(l, s, c);
    };
    r.add(std::move(k));
  }
  {
    EndpointKind k;
    k.kind = "loopback";
    k.title = "Loopback (testing)";
    k.description = "Everything sent is received back. For testing link drivers.";
    k.link_types = {"raw", "stream"};
    k.default_link_type = "raw";
    k.create = [](Loop& l, EndpointSink& s, const Json& c, std::string*) -> std::unique_ptr<Endpoint> {
      return std::make_unique<LoopbackEndpoint>(l, s, c);
    };
    r.add(std::move(k));
  }
}

}  // namespace ember_host
