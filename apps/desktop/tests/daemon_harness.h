// Test harness for multi-process mesh tests: spawns real emberd processes (real core, real
// sockets), talks to each over POST /rpc, and provides tiny in-test "media": a TCP relay that
// behaves like two KISS TNCs on one frequency, a pty pair relay (a null-modem cable), and a fake
// rtl_tcp server. POSIX only.
#pragma once

#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <termios.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "host/json.h"
#include "host/util.h"

extern char** environ;

namespace mesh {

using ember_host::Json;

inline Json J(const std::string& s) { return Json::parse_or_null(s); }

inline uint16_t free_tcp_port() {
  int s = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = 0;
  ::bind(s, reinterpret_cast<sockaddr*>(&a), sizeof a);
  socklen_t l = sizeof a;
  ::getsockname(s, reinterpret_cast<sockaddr*>(&a), &l);
  uint16_t p = ntohs(a.sin_port);
  ::close(s);
  return p;
}

inline bool wait_until(const std::function<bool()>& pred, int timeout_ms, int step_ms = 100) {
  uint64_t end = ember_host::steady_ms() + uint64_t(timeout_ms);
  while (true) {
    if (pred()) return true;
    if (ember_host::steady_ms() > end) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(step_ms));
  }
}

// Blocking HTTP/1.1 request to 127.0.0.1:port. Returns the body; *status = HTTP status (0 = I/O error).
inline std::string http(uint16_t port, const std::string& method, const std::string& path, const std::string& body,
                        const std::string& token, int* status, int timeout_ms = 10000) {
  *status = 0;
  int s = ::socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return "";
  timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
#ifdef SO_NOSIGPIPE
  int one = 1;
  setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = htons(port);
  if (::connect(s, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0) {
    ::close(s);
    return "";
  }
  std::string req = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n";
  if (!token.empty()) req += "Authorization: Bearer " + token + "\r\n";
  if (method == "POST") req += "Content-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\n";
  req += "\r\n" + body;
  size_t off = 0;
  while (off < req.size()) {
    ssize_t w = ::send(s, req.data() + off, req.size() - off, 0);
    if (w <= 0) {
      ::close(s);
      return "";
    }
    off += size_t(w);
  }
  std::string resp;
  char buf[65536];
  size_t want = std::string::npos;
  while (true) {
    ssize_t r = ::recv(s, buf, sizeof buf, 0);
    if (r <= 0) break;
    resp.append(buf, size_t(r));
    size_t he = resp.find("\r\n\r\n");
    if (he != std::string::npos && want == std::string::npos) {
      std::string head = ember_host::to_lower(resp.substr(0, he));
      size_t cl = head.find("content-length:");
      if (cl != std::string::npos) want = he + 4 + size_t(std::strtoull(head.c_str() + cl + 15, nullptr, 10));
    }
    if (want != std::string::npos && resp.size() >= want) break;
  }
  ::close(s);
  size_t he = resp.find("\r\n\r\n");
  if (resp.size() < 12 || he == std::string::npos) return "";
  *status = std::atoi(resp.c_str() + 9);
  return resp.substr(he + 4);
}

inline std::string repo_dir() { return EMBER_REPO_DIR; }

// Per-test directory; kept (and its path printed) when EMBER_KEEP_TEST_DIRS is set.
struct TestDir {
  std::string path;
  TestDir() {
    const char* t = std::getenv("TMPDIR");
    std::string base = t && *t ? t : "/tmp";
    path = ember_host::path_join(base, "emberd_mesh_" + ember_host::random_hex(5));
    ember_host::make_dirs(path);
  }
  ~TestDir() {
    if (std::getenv("EMBER_KEEP_TEST_DIRS")) {
      std::fprintf(stderr, "kept test dir %s\n", path.c_str());
      return;
    }
    rmtree(path);
  }
  static void rmtree(const std::string& p) {
    if (ember_host::is_dir(p) && !ember_host::is_symlink(p)) {
      for (const auto& n : ember_host::list_dir(p)) rmtree(ember_host::path_join(p, n));
      ::rmdir(p.c_str());
    } else {
      ::unlink(p.c_str());
    }
  }
};

// One emberd process with its own data directory, hub port and admin token.
struct Daemon {
  std::string name, dir, log_path;
  uint16_t port = 0;
  std::string token;
  pid_t pid = -1;
  std::vector<std::string> extra;
  std::string stdin_path = "/dev/null";
  bool hub = true;
  bool set_name = true;  // pass --name (false: test the anonymous default)
  // Nearby (mDNS auto links) is off in tests unless asked for, with a private service type so
  // parallel tests and real devices on the LAN never see each other.
  std::string nearby_service;

  Daemon(const std::string& n, const std::string& base_dir) : name(n) {
    dir = ember_host::path_join(base_dir, n);
    ember_host::make_dirs(dir);
    log_path = dir + ".log";
    port = free_tcp_port();
    token = "tok-" + ember_host::random_hex(12);
  }
  ~Daemon() {
    stop();
    if (std::getenv("EMBER_TEST_LOGS")) {
      std::string l = log();
      if (l.size() > 6000) l = l.substr(l.size() - 6000);
      std::fprintf(stderr, "----- %s log (tail) -----\n%s\n", name.c_str(), l.c_str());
    }
  }

  bool running() const { return pid > 0; }

  bool start(const std::vector<std::string>& more = {}, bool wait = true) {
    std::vector<std::string> args = {EMBERD_PATH, "--data-dir", dir, "--port", std::to_string(port), "--bind", "127.0.0.1",
                                     "--token", token, "--no-udp", "--no-qr", "--log-level", std::getenv("EMBER_TEST_DEBUG") ? "debug" : "info"};
    std::string web = ember_host::path_join(repo_dir(), "dist/web");
    if (ember_host::is_file(ember_host::path_join(web, "index.html"))) {
      args.push_back("--web-root");
      args.push_back(web);
    }
    std::string guides = ember_host::path_join(repo_dir(), "build/guides.pack");
    if (ember_host::is_file(guides)) {
      args.push_back("--guides");
      args.push_back(guides);
    }
    if (set_name) {
      args.push_back("--name");
      args.push_back(name);
    }
    if (!hub) args.push_back("--no-hub");
    if (nearby_service.empty()) {
      args.push_back("--no-nearby");
    } else {
      args.push_back("--nearby-service");
      args.push_back(nearby_service);
    }
    for (auto& a : extra) args.push_back(a);
    for (auto& a : more) args.push_back(a);
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(&a[0]);
    argv.push_back(nullptr);
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, 0, stdin_path.c_str(), O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&fa, 1, log_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    posix_spawn_file_actions_adddup2(&fa, 1, 2);
    int rc = posix_spawn(&pid, argv[0], &fa, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&fa);
    if (rc != 0) {
      pid = -1;
      return false;
    }
    if (!wait) return true;
    if (!hub) {
      std::this_thread::sleep_for(std::chrono::milliseconds(800));
      return alive();
    }
    return wait_until([&] {
      int st;
      http(port, "GET", "/hub.json", "", "", &st, 1000);
      return st == 200 || !alive();
    }, 15000) && alive();
  }

  bool alive() {
    if (pid <= 0) return false;
    int st;
    pid_t r = ::waitpid(pid, &st, WNOHANG);
    if (r == pid) {
      exit_status = st;
      pid = -1;
      return false;
    }
    return true;
  }

  // SIGTERM, wait up to `ms` for a clean exit, then SIGKILL. Returns the exit code (-1 if killed).
  int stop(int ms = 15000) {
    if (pid <= 0) return exit_code();
    ::kill(pid, SIGTERM);
    uint64_t end = ember_host::steady_ms() + uint64_t(ms);
    while (ember_host::steady_ms() < end) {
      int st;
      pid_t r = ::waitpid(pid, &st, WNOHANG);
      if (r == pid) {
        exit_status = st;
        pid = -1;
        return exit_code();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ::kill(pid, SIGKILL);
    int st;
    ::waitpid(pid, &st, 0);
    pid = -1;
    exit_status = -1;
    return -1;
  }
  int exit_code() const { return exit_status >= 0 && WIFEXITED(exit_status) ? WEXITSTATUS(exit_status) : -1; }
  int exit_status = 0;

  Json rpc(const std::string& method, const Json& params = Json::object(), int timeout_ms = 10000) {
    Json body = Json::object();
    body.set("method", method);
    body.set("params", params);
    int st;
    std::string r = http(port, "POST", "/rpc", body.dump(), token, &st, timeout_ms);
    Json j = Json::parse_or_null(r);
    if (!j.is_object()) {
      j = Json::object();
      j.set("ok", false);
      Json e = Json::object();
      e.set("code", "io_error");
      e.set("message", "HTTP " + std::to_string(st));
      j.set("error", e);
    }
    return j;
  }
  Json result(const std::string& method, const Json& params = Json::object()) { return rpc(method, params).get("result"); }

  std::string node_id() {
    if (id_.empty()) id_ = result("node.info").get("node_id").as_string();
    return id_;
  }
  void forget_id() { id_.clear(); }

  // Message lookup helpers.
  Json find_message(const std::string& conv, const std::string& text) {
    Json p = Json::object();
    p.set("conv", conv);
    p.set("limit", 500);
    Json res_messages = result("messages.list", p);

    for (const auto& m : res_messages.get("messages").elements())
      if (m.get("text").as_string() == text) return m;
    return Json();
  }
  bool has_message(const std::string& conv, const std::string& text) { return find_message(conv, text).is_object(); }
  Json send(const std::string& conv, const std::string& text) {
    Json p = Json::object();
    p.set("conv", conv);
    p.set("text", text);
    return rpc("messages.send", p);
  }
  Json link_by_name(const std::string& n) {
    Json res_links = result("links.list");

    for (const auto& l : res_links.get("links").elements())
      if (l.get("name").as_string() == n) return l;
    return Json();
  }
  std::string log() {
    std::string s;
    ember_host::read_file(log_path, &s, 4 << 20);
    return s;
  }
  // Resident memory (KiB) and accumulated CPU seconds of the process, from ps.
  bool usage(long* rss_kib, double* cpu_s) {
    if (pid <= 0) return false;
    std::string cmd = "ps -o rss=,time= -p " + std::to_string(pid);
    FILE* f = ::popen(cmd.c_str(), "r");
    if (!f) return false;
    char line[256] = {0};
    bool ok = std::fgets(line, sizeof line, f) != nullptr;
    ::pclose(f);
    if (!ok) return false;
    long rss = 0;
    char t[64] = {0};
    if (std::sscanf(line, "%ld %63s", &rss, t) != 2) return false;
    // time: [[dd-]hh:]mm:ss[.cc]
    double secs = 0, part = 0;
    std::string ts = t;
    size_t dash = ts.find('-');
    if (dash != std::string::npos) {
      secs += std::atof(ts.substr(0, dash).c_str()) * 86400;
      ts = ts.substr(dash + 1);
    }
    std::vector<std::string> parts = ember_host::split(ts, ':');
    for (auto& p : parts) {
      part = std::atof(p.c_str());
      secs = secs * 60 + part;
    }
    *rss_kib = rss;
    *cpu_s = secs;
    return true;
  }

 private:
  std::string id_;
};

// Waits until `a` and `b` have exchanged keys (each lists the other as a peer with a name).
inline bool wait_peer(Daemon& a, const std::string& other_id, int timeout_ms = 20000) {
  return wait_until([&] {
    Json res_peers = a.result("mesh.peers");

    for (const auto& p : res_peers.get("peers").elements())
      if (p.get("node_id").as_string() == other_id) return true;
    return false;
  }, timeout_ms, 200);
}

// ---- in-test media ------------------------------------------------------------------------------

// TCP "shared channel": every byte a client sends goes to all other clients (two KISS TNCs on one
// frequency, or a serial-to-network bridge). Records everything that crossed.
struct TcpRelay {
  int lfd = -1;
  uint16_t port = 0;
  std::atomic<bool> stop{false};
  std::thread th;
  std::mutex mu;
  std::string seen;
  std::vector<int> clients;
  std::atomic<size_t> bytes{0};

  TcpRelay() {
    lfd = ::socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ::bind(lfd, reinterpret_cast<sockaddr*>(&a), sizeof a);
    ::listen(lfd, 8);
    socklen_t l = sizeof a;
    ::getsockname(lfd, reinterpret_cast<sockaddr*>(&a), &l);
    port = ntohs(a.sin_port);
    th = std::thread([this] { run(); });
  }
  ~TcpRelay() {
    stop = true;
    th.join();
    for (int c : clients) ::close(c);
    ::close(lfd);
  }
  size_t client_count() {
    std::lock_guard<std::mutex> lk(mu);
    return clients.size();
  }
  void run() {
    while (!stop) {
      std::vector<pollfd> p;
      p.push_back({lfd, POLLIN, 0});
      {
        std::lock_guard<std::mutex> lk(mu);
        for (int c : clients) p.push_back({c, POLLIN, 0});
      }
      if (::poll(p.data(), nfds_t(p.size()), 50) <= 0) continue;
      if (p[0].revents & POLLIN) {
        int c = ::accept(lfd, nullptr, nullptr);
        if (c >= 0) {
#ifdef SO_NOSIGPIPE
          int one = 1;
          setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
          std::lock_guard<std::mutex> lk(mu);
          clients.push_back(c);
        }
      }
      for (size_t i = 1; i < p.size(); ++i) {
        if (!(p[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;
        char buf[8192];
        ssize_t r = ::recv(p[i].fd, buf, sizeof buf, 0);
        std::lock_guard<std::mutex> lk(mu);
        if (r <= 0) {
          ::close(p[i].fd);
          clients.erase(std::remove(clients.begin(), clients.end(), p[i].fd), clients.end());
          continue;
        }
        seen.append(buf, size_t(r));
        bytes += size_t(r);
        for (int c : clients)
          if (c != p[i].fd) ::send(c, buf, size_t(r), 0);
      }
    }
  }
};

// Two pseudo-terminals whose master sides are cross-connected: the slave paths behave like the two
// ends of a null-modem cable for two serial endpoints.
struct PtyCable {
  int m1 = -1, s1 = -1, m2 = -1, s2 = -1;
  std::string path1, path2;
  std::atomic<bool> stop{false};
  std::atomic<size_t> bytes{0};
  std::thread th;
  bool ok = false;

  PtyCable() {
    char n1[128], n2[128];
    if (::openpty(&m1, &s1, n1, nullptr, nullptr) != 0 || ::openpty(&m2, &s2, n2, nullptr, nullptr) != 0) return;
    path1 = n1;
    path2 = n2;
    for (int s : {s1, s2}) {  // raw, no echo: bytes must pass untouched before emberd opens the port
      termios t;
      tcgetattr(s, &t);
      cfmakeraw(&t);
      tcsetattr(s, TCSANOW, &t);
    }
    for (int m : {m1, m2}) ::fcntl(m, F_SETFL, ::fcntl(m, F_GETFL) | O_NONBLOCK);
    ok = true;
    th = std::thread([this] { run(); });
  }
  ~PtyCable() {
    stop = true;
    if (th.joinable()) th.join();
    for (int f : {m1, s1, m2, s2})
      if (f >= 0) ::close(f);
  }
  void run() {
    while (!stop) {
      pollfd p[2] = {{m1, POLLIN, 0}, {m2, POLLIN, 0}};
      if (::poll(p, 2, 50) <= 0) continue;
      for (int i = 0; i < 2; ++i) {
        if (!(p[i].revents & POLLIN)) continue;
        char buf[4096];
        ssize_t r = ::read(p[i].fd, buf, sizeof buf);
        if (r <= 0) continue;
        bytes += size_t(r);
        int to = i == 0 ? m2 : m1;
        ssize_t off = 0;
        uint64_t end = ember_host::steady_ms() + 500;
        while (off < r && ember_host::steady_ms() < end) {
          ssize_t w = ::write(to, buf + off, size_t(r - off));
          if (w > 0) off += w;
          else std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      }
    }
  }
};

// Minimal rtl_tcp server: sends the 12-byte header, swallows tuner commands and streams the given
// u8 I/Q buffer at `rate_x` times real time (then unmodulated noise), like a dongle. Pacing is in
// sample time against a deadline: a stretched sleep makes the next send bigger (a burst), never
// the stream slower. `iq_done` is set once the whole buffer is on the wire, so tests wait for
// that instead of guessing how long a loaded machine needs.
// Test knobs: EMBER_TEST_SDR_RATE_X (default 2; 0 = the buffer as fast as the socket takes it) and
// EMBER_TEST_SDR_JITTER_MS (random stalls up to that long, then the backlog in one burst: what a
// busy USB host or a remote rtl_tcp over Wi-Fi delivers).
struct FakeRtlTcp {
  int lfd = -1;
  uint16_t port = 0;
  std::string iq;
  uint32_t sample_rate = 240000;
  double rate_x = 2.0;
  int jitter_ms = 0;
  std::atomic<bool> stop{false};
  std::atomic<size_t> sent{0};
  std::atomic<bool> iq_done{false};
  std::atomic<size_t> commands{0};
  std::thread th;

  explicit FakeRtlTcp(std::string iq_bytes) : iq(std::move(iq_bytes)) {
    if (const char* e = std::getenv("EMBER_TEST_SDR_RATE_X")) rate_x = std::atof(e);
    if (const char* e = std::getenv("EMBER_TEST_SDR_JITTER_MS")) jitter_ms = std::atoi(e);
    lfd = ::socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ::bind(lfd, reinterpret_cast<sockaddr*>(&a), sizeof a);
    ::listen(lfd, 2);
    socklen_t l = sizeof a;
    ::getsockname(lfd, reinterpret_cast<sockaddr*>(&a), &l);
    port = ntohs(a.sin_port);
    th = std::thread([this] { run(); });
  }
  ~FakeRtlTcp() {
    stop = true;
    th.join();
    ::close(lfd);
  }
  void run() {
    int c = -1;
    while (!stop && c < 0) {
      pollfd p{lfd, POLLIN, 0};
      if (::poll(&p, 1, 50) > 0) c = ::accept(lfd, nullptr, nullptr);
    }
    if (c < 0) return;
#ifdef SO_NOSIGPIPE
    int one = 1;
    setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
    const uint8_t hdr[12] = {'R', 'T', 'L', '0', 0, 0, 0, 5, 0, 0, 0, 29};
    ::send(c, hdr, sizeof hdr, 0);
    const double bytes_per_ms = double(sample_rate) * 2.0 / 1000.0;  // u8 I + u8 Q per sample
    const size_t kMaxBurst = 1 << 20;
    uint32_t rng = 12345;
    std::string noise;
    size_t off = 0;           // bytes of `iq` sent
    size_t noise_sent = 0;    // bytes of noise sent after it
    uint64_t t0 = ember_host::steady_ms(), t_noise = 0;
    while (!stop) {
      // Drain commands.
      pollfd p{c, POLLIN, 0};
      while (::poll(&p, 1, 0) > 0 && (p.revents & POLLIN)) {
        char b[256];
        ssize_t r = ::recv(c, b, sizeof b, 0);
        if (r <= 0) {
          ::close(c);
          return;
        }
        commands += size_t(r) / 5;
      }
      // Everything due by now in sample time: the buffer at rate_x (all of it at once with 0),
      // then noise at real time.
      uint64_t now = ember_host::steady_ms();
      size_t n = 0;
      const char* data = nullptr;
      if (off < iq.size()) {
        size_t due = rate_x <= 0 ? iq.size() : size_t(double(now - t0) * bytes_per_ms * rate_x);
        if (due > iq.size()) due = iq.size();
        n = due > off ? std::min(due - off, kMaxBurst) & ~size_t(1) : 0;
        data = iq.data() + off;
        off += n;
        if (off >= iq.size()) t_noise = now;
      } else {
        size_t due = size_t(double(now - t_noise) * bytes_per_ms);
        n = due > noise_sent ? std::min(due - noise_sent, kMaxBurst) & ~size_t(1) : 0;
        noise.resize(n);
        for (auto& ch : noise) {
          rng = rng * 1103515245u + 12345u;
          ch = char(127 + int((rng >> 16) % 5) - 2);
        }
        data = noise.data();
        noise_sent += n;
      }
      size_t w = 0;
      while (w < n && !stop) {
        ssize_t r = ::send(c, data + w, n - w, 0);
        if (r <= 0) {
          ::close(c);
          return;
        }
        w += size_t(r);
      }
      sent += n;
      if (off >= iq.size()) iq_done = true;
      int pause = 10;
      if (jitter_ms > 0) {
        rng = rng * 1103515245u + 12345u;
        pause = 1 + int((rng >> 8) % uint32_t(jitter_ms));
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(pause));
    }
    ::close(c);
  }
};

// NBFM-modulates s16 mono PCM (rate `in_rate`) onto a carrier `offset_hz` above the tuned centre,
// as unsigned 8-bit I/Q at `iq_rate` (what an RTL-SDR dongle would deliver).
inline std::string fm_modulate(const std::vector<int16_t>& pcm, uint32_t in_rate, uint32_t iq_rate, double offset_hz,
                               double deviation_hz) {
  std::string out;
  double ratio = double(iq_rate) / double(in_rate);
  size_t n = size_t(double(pcm.size()) * ratio);
  out.resize(n * 2);
  double phase = 0;
  const double two_pi = 6.283185307179586;
  for (size_t i = 0; i < n; ++i) {
    double pos = double(i) / ratio;
    size_t k = size_t(pos);
    double fr = pos - double(k);
    double a0 = k < pcm.size() ? pcm[k] / 32768.0 : 0;
    double a1 = k + 1 < pcm.size() ? pcm[k + 1] / 32768.0 : a0;
    double a = a0 + (a1 - a0) * fr;
    phase += two_pi * (offset_hz + deviation_hz * a) / double(iq_rate);
    if (phase > two_pi) phase -= two_pi;
    double I = 127.5 + 100.0 * std::cos(phase), Q = 127.5 + 100.0 * std::sin(phase);
    out[2 * i] = char(uint8_t(I < 0 ? 0 : I > 255 ? 255 : std::lround(I)));
    out[2 * i + 1] = char(uint8_t(Q < 0 ? 0 : Q > 255 ? 255 : std::lround(Q)));
  }
  return out;
}

}  // namespace mesh

namespace mesh {

// Runs a process to completion (stdin from a file, stdout+stderr to `log`). Returns the exit code,
// or -1 if it had to be killed after `timeout_ms`.
inline int run_process(std::vector<std::string> args, const std::string& stdin_path, const std::string& log, int timeout_ms) {
  std::vector<char*> argv;
  for (auto& a : args) argv.push_back(&a[0]);
  argv.push_back(nullptr);
  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init(&fa);
  posix_spawn_file_actions_addopen(&fa, 0, stdin_path.c_str(), O_RDONLY, 0);
  posix_spawn_file_actions_addopen(&fa, 1, log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  posix_spawn_file_actions_adddup2(&fa, 1, 2);
  pid_t pid;
  int rc = posix_spawn(&pid, argv[0], &fa, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&fa);
  if (rc != 0) return -2;
  uint64_t end = ember_host::steady_ms() + uint64_t(timeout_ms);
  while (ember_host::steady_ms() < end) {
    int st;
    if (::waitpid(pid, &st, WNOHANG) == pid) return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  ::kill(pid, SIGKILL);
  int st;
  ::waitpid(pid, &st, 0);
  return -1;
}

// Raw TCP connection with a hand-made WebSocket upgrade; optionally never reads (a frozen guest).
inline int raw_ws_connect(uint16_t port, const std::string& path, int rcvbuf = 0) {
  int s = ::socket(AF_INET, SOCK_STREAM, 0);
  if (rcvbuf) setsockopt(s, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof rcvbuf);
#ifdef SO_NOSIGPIPE
  int one = 1;
  setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = htons(port);
  if (::connect(s, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0) {
    ::close(s);
    return -1;
  }
  std::string req = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
  ::send(s, req.data(), req.size(), 0);
  return s;
}

// Masked client frame (RFC 6455).
inline std::string ws_client_frame(uint8_t op, const std::string& payload) {
  std::string f;
  f += char(0x80 | op);
  size_t n = payload.size();
  if (n < 126) {
    f += char(0x80 | n);
  } else if (n < 65536) {
    f += char(0x80 | 126);
    f += char(n >> 8);
    f += char(n & 0xff);
  } else {
    f += char(0x80 | 127);
    for (int i = 7; i >= 0; --i) f += char((uint64_t(n) >> (8 * i)) & 0xff);
  }
  const char mask[4] = {0x12, 0x34, 0x56, 0x78};
  f.append(mask, 4);
  for (size_t i = 0; i < n; ++i) f += char(payload[i] ^ mask[i % 4]);
  return f;
}

inline std::string random_bytes_str(size_t n) {
  std::string s(n, '\0');
  ember_host::random_bytes(&s[0], n);
  return s;
}

}  // namespace mesh

namespace mesh {

// N raw WebSocket guests on /ws, read by one thread that parses server frames and counts binary
// messages per guest (what a browser tab running the WASM node would receive).
struct GuestSwarm {
  struct G {
    int fd = -1;
    std::string buf;
    bool upgraded = false, closed = false;
    size_t frames = 0, bytes = 0;
    uint16_t close_code = 0;
  };
  std::vector<G> g;
  std::mutex mu;
  std::atomic<bool> stop{false};
  std::thread th;

  GuestSwarm(uint16_t port, size_t n) {
    g.resize(n);
    for (auto& x : g) {
      x.fd = raw_ws_connect(port, "/ws");
      if (x.fd >= 0) ::fcntl(x.fd, F_SETFL, ::fcntl(x.fd, F_GETFL) | O_NONBLOCK);
    }
    th = std::thread([this] { run(); });
  }
  ~GuestSwarm() {
    stop = true;
    th.join();
    for (auto& x : g)
      if (x.fd >= 0) ::close(x.fd);
  }
  size_t upgraded() {
    std::lock_guard<std::mutex> lk(mu);
    size_t n = 0;
    for (auto& x : g) n += x.upgraded && !x.closed;
    return n;
  }
  size_t min_frames() {
    std::lock_guard<std::mutex> lk(mu);
    size_t m = SIZE_MAX;
    for (auto& x : g) m = std::min(m, x.frames);
    return g.empty() ? 0 : m;
  }
  void send(size_t i, const std::string& payload) {
    std::string f = ws_client_frame(0x2, payload);
    std::lock_guard<std::mutex> lk(mu);
    if (g[i].fd >= 0) ::send(g[i].fd, f.data(), f.size(), 0);
  }
  void run() {
    while (!stop) {
      std::vector<pollfd> p;
      {
        std::lock_guard<std::mutex> lk(mu);
        for (auto& x : g) p.push_back({x.fd, short(x.fd >= 0 && !x.closed ? POLLIN : 0), 0});
      }
      if (::poll(p.data(), nfds_t(p.size()), 50) <= 0) continue;
      std::lock_guard<std::mutex> lk(mu);
      for (size_t i = 0; i < p.size(); ++i) {
        if (!(p[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;
        G& x = g[i];
        char b[65536];
        ssize_t r = ::recv(x.fd, b, sizeof b, 0);
        if (r <= 0) {
          if (r == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) x.closed = true;
          continue;
        }
        x.buf.append(b, size_t(r));
        if (!x.upgraded) {
          size_t he = x.buf.find("\r\n\r\n");
          if (he == std::string::npos) continue;
          x.upgraded = x.buf.compare(0, 12, "HTTP/1.1 101") == 0;
          if (!x.upgraded) x.closed = true;
          x.buf.erase(0, he + 4);
        }
        while (x.buf.size() >= 2) {
          uint8_t op = uint8_t(x.buf[0]) & 0x0f;
          uint64_t len = uint8_t(x.buf[1]) & 0x7f;
          size_t h = 2;
          if (len == 126) {
            if (x.buf.size() < 4) break;
            len = (uint64_t(uint8_t(x.buf[2])) << 8) | uint8_t(x.buf[3]);
            h = 4;
          } else if (len == 127) {
            if (x.buf.size() < 10) break;
            len = 0;
            for (int k = 0; k < 8; ++k) len = (len << 8) | uint8_t(x.buf[2 + k]);
            h = 10;
          }
          if (x.buf.size() < h + len) break;
          if (op == 0x2) {
            ++x.frames;
            x.bytes += len;
          } else if (op == 0x8) {
            x.close_code = len >= 2 ? uint16_t((uint8_t(x.buf[h]) << 8) | uint8_t(x.buf[h + 1])) : 1005;
            x.closed = true;
          }
          x.buf.erase(0, h + len);
        }
      }
    }
  }
};

}  // namespace mesh
