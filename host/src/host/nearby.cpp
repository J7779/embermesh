// SPDX-License-Identifier: MIT
// Nearby devices: listener + mDNS + dialing rule + limits (see nearby.h).
#include "host/nearby.h"

#include <algorithm>

#include "host/stream.h"
#include "host/util.h"

namespace ember_host {

namespace {

constexpr size_t kMaxAddrsPerInstance = 4;
constexpr size_t kMaxPerAddress = 2;
constexpr size_t kAcceptsPerAddressPerMinute = 4;
constexpr size_t kDialsPerMinute = 4;
constexpr uint64_t kWaitBeforeDialMs = 8000;
constexpr uint64_t kEyeballsStepMs = 250;
constexpr uint32_t kConnectTimeoutMs = 5000;
constexpr uint16_t kPreferredPort = 47475;
constexpr size_t kMaxPendingOut = 1 << 20;

std::string strip_scope(const std::string& a) {
  size_t p = a.find('%');
  return p == std::string::npos ? a : a.substr(0, p);
}

// A TCP connection under one nearby link: either an accepted socket, or a Happy Eyeballs dial over
// several addresses (the first to connect wins). Never reconnects: a nearby link whose connection
// ends is removed and rediscovery makes a new one.
class NearbyTcpEndpoint : public Endpoint {
 public:
  // Accepted connection.
  NearbyTcpEndpoint(Loop& l, EndpointSink& s, const Json& cfg, sock_t accepted, const SockAddr& peer)
      : Endpoint(l, s, cfg), alive_(std::make_shared<bool>(true)) {
    winner_.reset(new TcpStream(l));
    wire(*winner_);
    winner_->adopt(accepted, peer);
  }
  // Outgoing connection.
  NearbyTcpEndpoint(Loop& l, EndpointSink& s, const Json& cfg, std::vector<std::string> addrs, uint16_t port)
      : Endpoint(l, s, cfg), addrs_(std::move(addrs)), port_(port), alive_(std::make_shared<bool>(true)) {}
  ~NearbyTcpEndpoint() override {
    *alive_ = false;
    close();
  }
  const char* kind() const override { return "tcp"; }
  bool auto_reconnect() const override { return false; }

  void open() override {
    std::weak_ptr<bool> w = alive_;
    if (winner_) {  // accepted: already connected
      loop_.timer(0, [this, w] {
        auto a = w.lock();
        if (a && *a && winner_) set_state(true);
      });
      return;
    }
    next_attempt();
  }
  void close() override {
    for (auto t : timers_) loop_.cancel(t);
    timers_.clear();
    attempts_.clear();
    if (winner_) winner_->close();
    winner_.reset();
  }
  void write(const uint8_t* d, size_t n) override {
    if (!winner_ || !winner_->connected() || winner_->pending() > kMaxPendingOut) return;
    winner_->write(d, n);
  }
  Json status() const override {
    Json s = Json::object();
    s.set("nearby", true);
    if (winner_) s.set("peer", winner_->peer().to_string());
    s.set("rx_bytes", double(winner_ ? winner_->bytes_in() : 0));
    s.set("tx_bytes", double(winner_ ? winner_->bytes_out() : 0));
    return s;
  }

 private:
  void wire(TcpStream& t) {
    t.on_data = [this](const uint8_t* d, size_t n) { deliver(d, n); };
    t.on_close = [this](const std::string& why) { lost(why); };
  }
  void lost(const std::string& why) {
    std::weak_ptr<bool> w = alive_;
    loop_.timer(0, [this, w, why] {  // never tear down inside the stream's own callback
      auto a = w.lock();
      if (!a || !*a) return;
      if (winner_) winner_->close();
      set_state(false, why.empty() ? "connection closed" : why);
    });
  }
  void next_attempt() {
    if (winner_ || next_ >= addrs_.size()) return;
    size_t i = next_++;
    auto s = std::make_unique<TcpStream>(loop_);
    TcpStream* raw = s.get();
    raw->on_connect = [this, raw](const std::string& err) { connected(raw, err); };
    raw->connect(addrs_[i], port_, kConnectTimeoutMs);
    attempts_.push_back(std::move(s));
    ++outstanding_;
    if (next_ < addrs_.size()) timers_.push_back(loop_.timer(kEyeballsStepMs, [this] { next_attempt(); }));
  }
  void connected(TcpStream* s, const std::string& err) {
    --outstanding_;
    if (winner_) return;
    if (!err.empty()) {
      last_error_ = err;
      if (next_ < addrs_.size()) {
        next_attempt();  // a failure starts the next address at once
      } else if (outstanding_ == 0) {
        std::weak_ptr<bool> w = alive_;
        std::string e = last_error_;
        loop_.timer(0, [this, w, e] {
          auto a = w.lock();
          if (a && *a) set_state(false, e);
        });
      }
      return;
    }
    for (auto& a : attempts_)
      if (a.get() == s) winner_ = std::move(a);
    // Losers are closed (and destroyed outside this callback).
    std::vector<std::unique_ptr<TcpStream>> losers;
    for (auto& a : attempts_)
      if (a) {
        a->close();
        losers.push_back(std::move(a));
      }
    attempts_.clear();
    std::shared_ptr<std::vector<std::unique_ptr<TcpStream>>> keep(new std::vector<std::unique_ptr<TcpStream>>(std::move(losers)));
    loop_.timer(0, [keep] {});
    wire(*winner_);
    set_state(true);
  }

  std::vector<std::string> addrs_;
  uint16_t port_ = 0;
  size_t next_ = 0;
  int outstanding_ = 0;
  std::string last_error_;
  std::vector<std::unique_ptr<TcpStream>> attempts_;
  std::unique_ptr<TcpStream> winner_;
  std::vector<uint64_t> timers_;
  std::shared_ptr<bool> alive_;
};

}  // namespace

Nearby::Nearby(Loop& loop, EndpointSink& sink, NearbyHostApi& host, std::string service)
    : loop_(loop), sink_(sink), host_(host), service_(std::move(service)), mdns_(loop), alive_(std::make_shared<bool>(true)) {
  mdns_.on_found = [this](const NearbyPeer& p) { on_found(p); };
  mdns_.on_lost = [this](const std::string& t) { on_lost(t); };
}

Nearby::~Nearby() {
  *alive_ = false;
  close_all();
}

void Nearby::set_enabled(bool on, const std::string& token) {
  if (on == on_ && token == token_) return;
  close_all();
  on_ = on;
  token_ = token;
  error_.clear();
  if (!on_) {
    EH_LOGI("nearby: off");
    return;
  }
  nets_ = local_networks();
  net_sig_ = net_signature();
  watch_network();
  if (!open_listener(&error_)) {
    EH_LOGW("nearby: %s", error_.c_str());
    return;
  }
  MdnsAgent::Options o;
  o.service = service_;
  o.token = token_;
  o.port = port_;
  for (const auto& n : nets_)
    if (!n.v6) o.ipv4.push_back(n.bind);
  std::string err;
  if (!mdns_.start(o, &err)) {
    error_ = err;
    EH_LOGW("nearby: %s (other devices can still connect to port %u)", err.c_str(), unsigned(port_));
  }
}

std::string Nearby::net_signature() const {
  std::string sig;
  for (const auto& n : nets_) sig += n.bind + ";";
  return sig;
}

// Interface addresses are polled every few seconds; a change is a "network change"
// (CONNECTIVITY §3.2): a new token from the core, then everything restarts with it.
void Nearby::watch_network() {
  std::weak_ptr<bool> w = alive_;
  net_timer_ = loop_.timer(3000, [this, w] {
    auto a = w.lock();
    if (!a || !*a) return;
    net_timer_ = 0;
    if (!on_) return;
    std::vector<LocalNet> now = local_networks();
    std::string sig;
    for (const auto& n : now) sig += n.bind + ";";
    if (sig == net_sig_) {
      watch_network();
      return;
    }
    EH_LOGI("nearby: the network changed: new token, re-advertising");
    std::string tok = host_.nearby_network_changed();
    if (tok.size() != 8) tok = token_;  // keep running with the old token if the core refused
    token_.clear();                     // force a restart in set_enabled
    set_enabled(true, tok);
  });
}

bool Nearby::open_listener(std::string* err) {
  // One listener per local-network address, all on the same port (47475 if free): bound to the
  // Wi-Fi/Ethernet addresses only, never to every interface.
  std::string e;
  port_ = 0;
  for (const auto& n : nets_) {
    uint16_t want = port_ ? port_ : kPreferredPort, bound = 0;
    sock_t fd = tcp_listen(n.bind, want, &e, &bound);
    if (fd == kInvalidSock && !port_) fd = tcp_listen(n.bind, 0, &e, &bound);
    if (fd == kInvalidSock) continue;  // that address is busy on this port: skip it
    port_ = bound;
    Listener l{fd, 0};
    l.watch = loop_.watch(fd, kPollIn, [this, fd](short) { on_accept(fd); });
    listeners_.push_back(l);
  }
  if (listeners_.empty()) {
    *err = nets_.empty() ? "no local network (private IPv4 or IPv6 link-local address)" : "cannot listen: " + e;
    return false;
  }
  return true;
}

void Nearby::close_all() {
  mdns_.stop();
  for (auto& kv : peers_)
    if (kv.second.wait_timer) loop_.cancel(kv.second.wait_timer);
  peers_.clear();
  if (net_timer_) loop_.cancel(net_timer_);
  net_timer_ = 0;
  for (auto& l : listeners_) {
    if (l.watch) loop_.unwatch(l.watch);
    sock_close(l.fd);
  }
  listeners_.clear();
  port_ = 0;
  std::vector<uint32_t> ids;
  for (auto& kv : links_) ids.push_back(kv.first);
  links_.clear();
  for (uint32_t id : ids) host_.nearby_remove(id);
}

bool Nearby::rate_ok(std::deque<uint64_t>& q, size_t max_per_minute) {
  uint64_t now = steady_ms();
  while (!q.empty() && now - q.front() > 60000) q.pop_front();
  if (q.size() >= max_per_minute) return false;
  q.push_back(now);
  return true;
}

void Nearby::on_accept(sock_t listener) {
  for (int i = 0; i < 16; ++i) {
    SockAddr peer;
    sock_t c = tcp_accept(listener, &peer);
    if (c == kInvalidSock) return;
    std::string ip = strip_scope(peer.ip());
    if (ip.compare(0, 7, "::ffff:") == 0) ip = ip.substr(7);
    std::string src = source_key(ip);
    size_t same = 0;
    for (auto& kv : links_) same += kv.second.src == src;
    if (accepts_.size() >= 256) accepts_.clear();  // bounded memory; the per-minute caps restart
    if (!on_local_network(peer.ip(), nets_) || same >= kMaxPerAddress || !rate_ok(accepts_[src], kAcceptsPerAddressPerMinute)) {
      ++refused_;
      sock_close(c);
      continue;
    }
    tcp_tune(c);
    Json ep = Json::object();
    ep.set("host", ip);
    ep.set("port", peer.port());
    std::unique_ptr<Endpoint> e(new NearbyTcpEndpoint(loop_, sink_, ep, c, peer));
    Json add = Json::object();
    add.set("auto_role", "accept");
    add.set("auto_key", "@" + ip);
    Json epj = Json::object();
    epj.set("kind", "tcp");
    epj.set("config", ep);
    add.set("endpoint", epj);
    uint32_t id = host_.nearby_install(std::move(e), add);
    if (id) links_[id] = LinkInfo{std::string(), ip, src, false};
  }
}

void Nearby::on_found(const NearbyPeer& p) {
  if (!on_ || p.token == token_ || p.addrs.empty()) return;
  bool first = !peers_.count(p.token);
  if (first && peers_.size() >= 64) return;
  Peer& pe = peers_[p.token];
  pe.p = p;
  if (first) {
    pe.found_ms = steady_ms();
    EH_LOGI("nearby: found a device (%zu address(es))", p.addrs.size());
  }
  consider(p.token, false);
}

void Nearby::on_lost(const std::string& token) {
  auto it = peers_.find(token);
  if (it == peers_.end()) return;
  if (it->second.wait_timer) loop_.cancel(it->second.wait_timer);
  peers_.erase(it);
}

bool Nearby::linked_to(const Peer& p) const {
  for (const auto& kv : links_) {
    if (kv.second.dial && kv.second.token == p.p.token) return true;
    if (!kv.second.dial)
      for (const auto& a : p.p.addrs)
        if (strip_scope(a) == kv.second.ip) return true;
  }
  return false;
}

void Nearby::consider(const std::string& token, bool waited) {
  auto it = peers_.find(token);
  if (it == peers_.end() || !on_) return;
  Peer& pe = it->second;
  if (linked_to(pe)) return;
  // CONNECTIVITY §3.3: the smaller token dials; the other waits 8 s for the inbound connection.
  if (!waited && !(token_ < token)) {
    if (!pe.wait_timer) {
      std::weak_ptr<bool> w = alive_;
      pe.wait_timer = loop_.timer(kWaitBeforeDialMs, [this, w, token] {
        auto a = w.lock();
        if (!a || !*a) return;
        auto it2 = peers_.find(token);
        if (it2 == peers_.end()) return;
        it2->second.wait_timer = 0;
        consider(token, true);
      });
    }
    return;
  }
  dial(pe);
}

void Nearby::dial(const Peer& pe) {
  // Only on-link addresses from the answer, at most 4 (IPv6 link-local first, as mDNS gave them).
  std::vector<std::string> addrs;
  for (const auto& a : pe.p.addrs) {
    if (addrs.size() >= kMaxAddrsPerInstance) break;
    bool v6 = a.find(':') != std::string::npos;
    if (v6 && a.find('%') == std::string::npos) continue;  // link-local needs its zone
    if (on_local_network(a, nets_)) addrs.push_back(a);
  }
  if (addrs.empty()) return;
  if (!rate_ok(dials_, kDialsPerMinute)) {
    // Try again when the budget allows (a hostile network advertising many instances is bounded).
    std::weak_ptr<bool> w = alive_;
    std::string token = pe.p.token;
    loop_.timer(15000, [this, w, token] {
      auto a = w.lock();
      if (a && *a) consider(token, true);
    });
    return;
  }
  Json ep = Json::object();
  ep.set("host", addrs.front());
  ep.set("port", pe.p.port);
  std::unique_ptr<Endpoint> e(new NearbyTcpEndpoint(loop_, sink_, ep, addrs, pe.p.port));
  Json add = Json::object();
  add.set("auto_role", "dial");
  add.set("auto_key", pe.p.token + "@" + strip_scope(addrs.front()));
  Json epj = Json::object();
  epj.set("kind", "tcp");
  epj.set("config", ep);
  add.set("endpoint", epj);
  uint32_t id = host_.nearby_install(std::move(e), add);
  if (id) {
    std::string ip = strip_scope(addrs.front());
    links_[id] = LinkInfo{pe.p.token, ip, source_key(ip), true};
  }
}

void Nearby::link_gone(uint32_t id) {
  auto it = links_.find(id);
  if (it == links_.end()) return;
  std::string token = it->second.token;
  links_.erase(it);
  // A dial that failed or a connection that ended: try again later if the peer is still advertised.
  if (on_ && !token.empty() && peers_.count(token)) {
    std::weak_ptr<bool> w = alive_;
    loop_.timer(5000, [this, w, token] {
      auto a = w.lock();
      if (a && *a) consider(token, true);
    });
  }
}

Json Nearby::status() const {
  Json s = Json::object();
  s.set("enabled", on_);
  s.set("running", on_ && mdns_.running());
  if (!error_.empty()) s.set("error", error_);
  s.set("port", port_);
  s.set("service", service_ + ".local");
  s.set("links", double(links_.size()));
  s.set("devices_seen", double(peers_.size()));
  s.set("refused_connections", double(refused_));
  Json la = Json::array();
  for (const auto& n : nets_) la.push(n.bind.substr(0, n.bind.find('%')) + (n.v6 ? " (link-local)" : ""));
  s.set("listening_on", la);
  const auto& st = mdns_.stats();
  Json m = Json::object();
  m.set("rx_packets", double(st.rx_packets));
  m.set("rx_bad", double(st.rx_bad));
  m.set("rx_dropped_budget", double(st.rx_dropped_budget));
  m.set("answers_sent", double(st.answers_sent));
  m.set("queries_sent", double(st.queries_sent));
  s.set("mdns", m);
  return s;
}

}  // namespace ember_host
