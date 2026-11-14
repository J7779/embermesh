// Thin portable socket layer: BSD sockets on POSIX, Winsock2 on Windows.
// Everything above this file is platform-neutral. Windows paths are written carefully but are
// UNTESTED (no Windows toolchain was available when this was written).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ember_host {

using sock_t = intptr_t;  // int fd on POSIX, SOCKET on Windows (INVALID_SOCKET == ~0 -> -1)
constexpr sock_t kInvalidSock = -1;

// Poll event bits (mapped to POLLIN/POLLOUT/... by the loop).
enum : short { kPollIn = 1, kPollOut = 2, kPollErr = 4, kPollHup = 8 };

bool net_init();  // WSAStartup once on Windows; ignores SIGPIPE on POSIX. Idempotent.

void sock_close(sock_t s);
bool sock_set_nonblock(sock_t s);
int sock_error();                    // errno / WSAGetLastError()
bool sock_would_block(int err);      // EAGAIN/EWOULDBLOCK/WSAEWOULDBLOCK
bool sock_in_progress(int err);      // EINPROGRESS/WSAEWOULDBLOCK (connect)
bool sock_interrupted(int err);      // EINTR
std::string sock_strerror(int err);
int sock_pending_error(sock_t s);    // SO_ERROR
// Return >0 bytes, 0 = orderly close (recv only), -1 = error (check sock_error()).
long sock_send(sock_t s, const void* data, size_t len);
long sock_recv(sock_t s, void* buf, size_t len);

struct SockAddr {
  uint8_t storage[128] = {0};  // sockaddr_storage
  uint32_t len = 0;
  std::string to_string() const;  // "1.2.3.4:5678" / "[::1]:5678"
  std::string ip() const;
  uint16_t port() const;
  bool is_loopback() const;
  bool is_v4() const;
};

// Numeric-only resolution (no DNS, never blocks). family_hint: 0 any, 4, 6.
bool resolve_numeric(const std::string& host, uint16_t port, SockAddr* out, int family_hint = 0);
// Full getaddrinfo (may block on DNS: call from a worker thread for hostnames).
bool resolve_blocking(const std::string& host, uint16_t port, SockAddr* out, std::string* err);

// Listening TCP socket (non-blocking, SO_REUSEADDR, backlog 128). bind "" / "0.0.0.0" / "::".
sock_t tcp_listen(const std::string& bind_addr, uint16_t port, std::string* err, uint16_t* bound_port = nullptr);
sock_t tcp_accept(sock_t listener, SockAddr* peer);  // non-blocking accepted socket or kInvalidSock
// Starts a non-blocking connect; *in_progress=true means wait for writability then check SO_ERROR.
sock_t tcp_connect_start(const SockAddr& addr, bool* in_progress, std::string* err);
void tcp_tune(sock_t s);  // TCP_NODELAY + SO_KEEPALIVE + SO_NOSIGPIPE where available

// UDP.
sock_t udp_socket(int family, std::string* err);
bool udp_bind(sock_t s, const std::string& addr, uint16_t port, bool reuse, std::string* err);
bool udp_join_multicast_v4(sock_t s, const std::string& group, const std::string& iface_ip, std::string* err);
bool udp_set_multicast_if_v4(sock_t s, const std::string& iface_ip);
bool udp_set_multicast_ttl_loop(sock_t s, int ttl, bool loop);
long udp_sendto(sock_t s, const void* data, size_t len, const SockAddr& to);
long udp_recvfrom(sock_t s, void* buf, size_t len, SockAddr* from);
SockAddr sock_local_addr(sock_t s);

struct Interface {
  std::string name;
  std::string ip;  // IPv4 dotted
  bool loopback = false;
  bool up = true;
  bool multicast = true;
};
std::vector<Interface> list_ipv4_interfaces();
#if defined(__linux__)
// SIOCGIFCONF-based enumeration (fallback where getifaddrs is missing, e.g. Android < API 24).
std::vector<Interface> list_ipv4_interfaces_ioctl();
#endif


// poll() wrapper. fds/events/revents use kPoll* bits. Returns number ready, 0 timeout, -1 error.
struct PollItem {
  sock_t fd;
  short events;
  short revents;
};
int poll_sockets(PollItem* items, size_t n, int timeout_ms);

// Cross-thread wakeup primitive for the poll loop (pipe on POSIX, UDP loopback socket on Windows).
class Waker {
 public:
  Waker();
  ~Waker();
  Waker(const Waker&) = delete;
  Waker& operator=(const Waker&) = delete;
  bool ok() const { return read_fd_ != kInvalidSock; }
  sock_t fd() const { return read_fd_; }
  void wake();   // thread-safe, async-signal-safe on POSIX
  void drain();

 private:
  sock_t read_fd_ = kInvalidSock;
  sock_t write_fd_ = kInvalidSock;
};

}  // namespace ember_host
