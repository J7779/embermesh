#include "host/net.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#if defined(__ANDROID__) && __ANDROID_API__ < 24
#include <dlfcn.h>  // getifaddrs is only in Bionic from API 24: looked up at run time
#endif
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace ember_host {

#ifdef _WIN32
static inline SOCKET S(sock_t s) { return static_cast<SOCKET>(s); }
#else
static inline int S(sock_t s) { return static_cast<int>(s); }
#endif

bool net_init() {
  static std::once_flag once;
  static bool ok = false;
  std::call_once(once, [] {
#ifdef _WIN32
    WSADATA d;
    ok = WSAStartup(MAKEWORD(2, 2), &d) == 0;
#else
    ::signal(SIGPIPE, SIG_IGN);
    ok = true;
#endif
  });
  return ok;
}

void sock_close(sock_t s) {
  if (s == kInvalidSock) return;
#ifdef _WIN32
  ::closesocket(S(s));
#else
  ::close(S(s));
#endif
}

bool sock_set_nonblock(sock_t s) {
#ifdef _WIN32
  u_long on = 1;
  return ::ioctlsocket(S(s), FIONBIO, &on) == 0;
#else
  int fl = ::fcntl(S(s), F_GETFL, 0);
  if (fl < 0) return false;
  if (::fcntl(S(s), F_SETFL, fl | O_NONBLOCK) != 0) return false;
  ::fcntl(S(s), F_SETFD, FD_CLOEXEC);
  return true;
#endif
}

int sock_error() {
#ifdef _WIN32
  return WSAGetLastError();
#else
  return errno;
#endif
}

bool sock_would_block(int e) {
#ifdef _WIN32
  return e == WSAEWOULDBLOCK;
#else
  return e == EAGAIN || e == EWOULDBLOCK;
#endif
}

bool sock_in_progress(int e) {
#ifdef _WIN32
  return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS;
#else
  return e == EINPROGRESS;
#endif
}

bool sock_interrupted(int e) {
#ifdef _WIN32
  return e == WSAEINTR;
#else
  return e == EINTR;
#endif
}

std::string sock_strerror(int e) {
#ifdef _WIN32
  char buf[256] = {0};
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, DWORD(e), 0, buf, sizeof buf, nullptr);
  std::string s = buf;
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
  return s.empty() ? ("winsock error " + std::to_string(e)) : s;
#else
  return std::strerror(e);
#endif
}

int sock_pending_error(sock_t s) {
  int err = 0;
#ifdef _WIN32
  int len = sizeof err;
  if (::getsockopt(S(s), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len) != 0) return sock_error();
#else
  socklen_t len = sizeof err;
  if (::getsockopt(S(s), SOL_SOCKET, SO_ERROR, &err, &len) != 0) return sock_error();
#endif
  return err;
}

long sock_send(sock_t s, const void* data, size_t len) {
#ifdef _WIN32
  int n = ::send(S(s), static_cast<const char*>(data), int(len > 0x7fffffff ? 0x7fffffff : len), 0);
  return n == SOCKET_ERROR ? -1 : long(n);
#else
  int flags = 0;
#ifdef MSG_NOSIGNAL
  flags |= MSG_NOSIGNAL;
#endif
  ssize_t n = ::send(S(s), data, len, flags);
  return long(n);
#endif
}

long sock_recv(sock_t s, void* buf, size_t len) {
#ifdef _WIN32
  int n = ::recv(S(s), static_cast<char*>(buf), int(len > 0x7fffffff ? 0x7fffffff : len), 0);
  return n == SOCKET_ERROR ? -1 : long(n);
#else
  return long(::recv(S(s), buf, len, 0));
#endif
}

// ---- addresses -----------------------------------------------------------------------------------

std::string SockAddr::ip() const {
  char buf[INET6_ADDRSTRLEN] = {0};
  const sockaddr* sa = reinterpret_cast<const sockaddr*>(storage);
  if (sa->sa_family == AF_INET) {
    ::inet_ntop(AF_INET, &reinterpret_cast<const sockaddr_in*>(sa)->sin_addr, buf, sizeof buf);
  } else if (sa->sa_family == AF_INET6) {
    ::inet_ntop(AF_INET6, &reinterpret_cast<const sockaddr_in6*>(sa)->sin6_addr, buf, sizeof buf);
    std::string s = buf;
    if (s.rfind("::ffff:", 0) == 0 && s.find('.') != std::string::npos) return s.substr(7);
  }
  return buf;
}

uint16_t SockAddr::port() const {
  const sockaddr* sa = reinterpret_cast<const sockaddr*>(storage);
  if (sa->sa_family == AF_INET) return ntohs(reinterpret_cast<const sockaddr_in*>(sa)->sin_port);
  if (sa->sa_family == AF_INET6) return ntohs(reinterpret_cast<const sockaddr_in6*>(sa)->sin6_port);
  return 0;
}

bool SockAddr::is_v4() const { return reinterpret_cast<const sockaddr*>(storage)->sa_family == AF_INET; }

std::string SockAddr::to_string() const {
  std::string i = ip();
  if (i.find(':') != std::string::npos) return "[" + i + "]:" + std::to_string(port());
  return i + ":" + std::to_string(port());
}

bool SockAddr::is_loopback() const {
  const sockaddr* sa = reinterpret_cast<const sockaddr*>(storage);
  if (sa->sa_family == AF_INET) {
    uint32_t a = ntohl(reinterpret_cast<const sockaddr_in*>(sa)->sin_addr.s_addr);
    return (a >> 24) == 127;
  }
  if (sa->sa_family == AF_INET6) {
    const in6_addr& a = reinterpret_cast<const sockaddr_in6*>(sa)->sin6_addr;
    static const uint8_t lo[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    if (std::memcmp(&a, lo, 16) == 0) return true;
    static const uint8_t mapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
    const uint8_t* b = reinterpret_cast<const uint8_t*>(&a);
    if (std::memcmp(b, mapped, 12) == 0) return b[12] == 127;
  }
  return false;
}

static bool resolve_impl(const std::string& host, uint16_t port, SockAddr* out, int family, bool numeric,
                         std::string* err) {
  addrinfo hints;
  std::memset(&hints, 0, sizeof hints);
  hints.ai_family = family == 4 ? AF_INET : family == 6 ? AF_INET6 : AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = numeric ? AI_NUMERICHOST : 0;
  hints.ai_flags |= AI_NUMERICSERV;
  addrinfo* res = nullptr;
  std::string svc = std::to_string(port);
  int rc = ::getaddrinfo(host.c_str(), svc.c_str(), &hints, &res);
  if (rc != 0 || !res) {
    if (err) {
#ifdef _WIN32
      *err = "cannot resolve '" + host + "'";
#else
      *err = "cannot resolve '" + host + "': " + gai_strerror(rc);
#endif
    }
    return false;
  }
  // Prefer IPv4 (most hub/radio software is IPv4-only).
  addrinfo* pick = res;
  for (addrinfo* a = res; a; a = a->ai_next)
    if (a->ai_family == AF_INET) {
      pick = a;
      break;
    }
  if (size_t(pick->ai_addrlen) > sizeof out->storage) {
    ::freeaddrinfo(res);
    return false;
  }
  std::memset(out->storage, 0, sizeof out->storage);
  std::memcpy(out->storage, pick->ai_addr, pick->ai_addrlen);
  out->len = uint32_t(pick->ai_addrlen);
  ::freeaddrinfo(res);
  return true;
}

bool resolve_numeric(const std::string& host, uint16_t port, SockAddr* out, int family_hint) {
  std::string h = host;
  if (h.size() >= 2 && h.front() == '[' && h.back() == ']') h = h.substr(1, h.size() - 2);
  if (h.empty()) h = "0.0.0.0";
  return resolve_impl(h, port, out, family_hint, true, nullptr);
}

bool resolve_blocking(const std::string& host, uint16_t port, SockAddr* out, std::string* err) {
  if (resolve_numeric(host, port, out)) return true;
  if (host == "localhost") return resolve_numeric("127.0.0.1", port, out);
  return resolve_impl(host, port, out, 0, false, err);
}

SockAddr sock_local_addr(sock_t s) {
  SockAddr a;
#ifdef _WIN32
  int len = int(sizeof a.storage);
#else
  socklen_t len = sizeof a.storage;
#endif
  if (::getsockname(S(s), reinterpret_cast<sockaddr*>(a.storage), &len) == 0) a.len = uint32_t(len);
  return a;
}

// ---- TCP -----------------------------------------------------------------------------------------

static void set_int_opt(sock_t s, int level, int name, int v) {
#ifdef _WIN32
  ::setsockopt(S(s), level, name, reinterpret_cast<const char*>(&v), sizeof v);
#else
  ::setsockopt(S(s), level, name, &v, sizeof v);
#endif
}

sock_t tcp_listen(const std::string& bind_addr, uint16_t port, std::string* err, uint16_t* bound_port) {
  net_init();
  SockAddr a;
  std::string b = bind_addr.empty() ? "0.0.0.0" : bind_addr;
  if (!resolve_numeric(b, port, &a)) {
    if (err) *err = "bad bind address '" + bind_addr + "'";
    return kInvalidSock;
  }
  const sockaddr* sa = reinterpret_cast<const sockaddr*>(a.storage);
  sock_t s = sock_t(::socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP));
  if (s == kInvalidSock) {
    if (err) *err = "socket: " + sock_strerror(sock_error());
    return kInvalidSock;
  }
#ifdef _WIN32
  set_int_opt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, 1);
#else
  set_int_opt(s, SOL_SOCKET, SO_REUSEADDR, 1);
#endif
  if (sa->sa_family == AF_INET6) set_int_opt(s, IPPROTO_IPV6, IPV6_V6ONLY, 0);
  if (::bind(S(s), sa, a.len) != 0) {
    int e = sock_error();
    if (err) *err = "bind " + a.to_string() + ": " + sock_strerror(e);
    sock_close(s);
    return kInvalidSock;
  }
  if (::listen(S(s), 128) != 0) {
    if (err) *err = "listen: " + sock_strerror(sock_error());
    sock_close(s);
    return kInvalidSock;
  }
  sock_set_nonblock(s);
  if (bound_port) *bound_port = sock_local_addr(s).port();
  return s;
}

sock_t tcp_accept(sock_t listener, SockAddr* peer) {
  SockAddr a;
#ifdef _WIN32
  int len = int(sizeof a.storage);
#else
  socklen_t len = sizeof a.storage;
#endif
  sock_t c = sock_t(::accept(S(listener), reinterpret_cast<sockaddr*>(a.storage), &len));
  if (c == kInvalidSock) return kInvalidSock;
  a.len = uint32_t(len);
  sock_set_nonblock(c);
  tcp_tune(c);
  if (peer) *peer = a;
  return c;
}

void tcp_tune(sock_t s) {
  set_int_opt(s, IPPROTO_TCP, TCP_NODELAY, 1);
  set_int_opt(s, SOL_SOCKET, SO_KEEPALIVE, 1);
#ifdef SO_NOSIGPIPE
  set_int_opt(s, SOL_SOCKET, SO_NOSIGPIPE, 1);
#endif
}

sock_t tcp_connect_start(const SockAddr& addr, bool* in_progress, std::string* err) {
  net_init();
  *in_progress = false;
  const sockaddr* sa = reinterpret_cast<const sockaddr*>(addr.storage);
  sock_t s = sock_t(::socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP));
  if (s == kInvalidSock) {
    if (err) *err = "socket: " + sock_strerror(sock_error());
    return kInvalidSock;
  }
  sock_set_nonblock(s);
  tcp_tune(s);
  if (::connect(S(s), sa, addr.len) != 0) {
    int e = sock_error();
    if (sock_in_progress(e)) {
      *in_progress = true;
      return s;
    }
    if (err) *err = "connect " + addr.to_string() + ": " + sock_strerror(e);
    sock_close(s);
    return kInvalidSock;
  }
  return s;
}

// ---- UDP -----------------------------------------------------------------------------------------

sock_t udp_socket(int family, std::string* err) {
  net_init();
  sock_t s = sock_t(::socket(family == 6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP));
  if (s == kInvalidSock) {
    if (err) *err = "socket: " + sock_strerror(sock_error());
    return kInvalidSock;
  }
  sock_set_nonblock(s);
  return s;
}

bool udp_bind(sock_t s, const std::string& addr, uint16_t port, bool reuse, std::string* err) {
  if (reuse) {
    set_int_opt(s, SOL_SOCKET, SO_REUSEADDR, 1);
#ifdef SO_REUSEPORT
    set_int_opt(s, SOL_SOCKET, SO_REUSEPORT, 1);
#endif
  }
  SockAddr a;
  if (!resolve_numeric(addr.empty() ? "0.0.0.0" : addr, port, &a, 4)) {
    if (err) *err = "bad address " + addr;
    return false;
  }
  if (::bind(S(s), reinterpret_cast<const sockaddr*>(a.storage), a.len) != 0) {
    if (err) *err = "bind " + a.to_string() + ": " + sock_strerror(sock_error());
    return false;
  }
  return true;
}

bool udp_join_multicast_v4(sock_t s, const std::string& group, const std::string& iface_ip, std::string* err) {
  ip_mreq m;
  std::memset(&m, 0, sizeof m);
  if (::inet_pton(AF_INET, group.c_str(), &m.imr_multiaddr) != 1) {
    if (err) *err = "bad multicast group " + group;
    return false;
  }
  if (iface_ip.empty()) {
    m.imr_interface.s_addr = htonl(INADDR_ANY);
  } else if (::inet_pton(AF_INET, iface_ip.c_str(), &m.imr_interface) != 1) {
    if (err) *err = "bad interface " + iface_ip;
    return false;
  }
#ifdef _WIN32
  int rc = ::setsockopt(S(s), IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&m), sizeof m);
#else
  int rc = ::setsockopt(S(s), IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof m);
#endif
  if (rc != 0) {
    if (err) *err = "join " + group + " on " + (iface_ip.empty() ? "default" : iface_ip) + ": " + sock_strerror(sock_error());
    return false;
  }
  return true;
}

bool udp_set_multicast_if_v4(sock_t s, const std::string& iface_ip) {
  in_addr a;
  if (iface_ip.empty()) {
    a.s_addr = htonl(INADDR_ANY);
  } else if (::inet_pton(AF_INET, iface_ip.c_str(), &a) != 1) {
    return false;
  }
#ifdef _WIN32
  return ::setsockopt(S(s), IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&a), sizeof a) == 0;
#else
  return ::setsockopt(S(s), IPPROTO_IP, IP_MULTICAST_IF, &a, sizeof a) == 0;
#endif
}

bool udp_set_multicast_ttl_loop(sock_t s, int ttl, bool loop) {
#ifdef _WIN32
  DWORD t = DWORD(ttl), l = loop ? 1 : 0;
  bool a = ::setsockopt(S(s), IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&t), sizeof t) == 0;
  bool b = ::setsockopt(S(s), IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>(&l), sizeof l) == 0;
#else
  unsigned char t = static_cast<unsigned char>(ttl), l = loop ? 1 : 0;
  bool a = ::setsockopt(S(s), IPPROTO_IP, IP_MULTICAST_TTL, &t, sizeof t) == 0;
  bool b = ::setsockopt(S(s), IPPROTO_IP, IP_MULTICAST_LOOP, &l, sizeof l) == 0;
#endif
  return a && b;
}

long udp_sendto(sock_t s, const void* data, size_t len, const SockAddr& to) {
#ifdef _WIN32
  int n = ::sendto(S(s), static_cast<const char*>(data), int(len), 0, reinterpret_cast<const sockaddr*>(to.storage), int(to.len));
  return n == SOCKET_ERROR ? -1 : long(n);
#else
  return long(::sendto(S(s), data, len, 0, reinterpret_cast<const sockaddr*>(to.storage), to.len));
#endif
}

long udp_recvfrom(sock_t s, void* buf, size_t len, SockAddr* from) {
  SockAddr a;
#ifdef _WIN32
  int alen = int(sizeof a.storage);
  int n = ::recvfrom(S(s), static_cast<char*>(buf), int(len), 0, reinterpret_cast<sockaddr*>(a.storage), &alen);
  if (n == SOCKET_ERROR) return -1;
#else
  socklen_t alen = sizeof a.storage;
  ssize_t n = ::recvfrom(S(s), buf, len, 0, reinterpret_cast<sockaddr*>(a.storage), &alen);
  if (n < 0) return -1;
#endif
  a.len = uint32_t(alen);
  if (from) *from = a;
  return long(n);
}

// ---- interfaces ----------------------------------------------------------------------------------

#if defined(__linux__)
// Fallback (Linux/Android fixed-size ifreq layout): SIOCGIFCONF (IPv4 addresses) + SIOCGIFFLAGS. Used where getifaddrs is missing.
std::vector<Interface> list_ipv4_interfaces_ioctl() {
  std::vector<Interface> r;
  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) return r;
  std::vector<char> buf(64 * sizeof(ifreq));
  ifconf ifc;
  std::memset(&ifc, 0, sizeof ifc);
  for (int tries = 0; tries < 4; ++tries) {
    ifc.ifc_len = int(buf.size());
    ifc.ifc_buf = buf.data();
    if (::ioctl(fd, SIOCGIFCONF, &ifc) != 0) {
      ::close(fd);
      return r;
    }
    if (size_t(ifc.ifc_len) + sizeof(ifreq) < buf.size()) break;  // did not fill: complete
    buf.resize(buf.size() * 2);
  }
  // Entries are fixed-size ifreq on Linux/Android (sockaddr fits in ifr_addr for AF_INET).
  for (size_t off = 0; off + sizeof(ifreq) <= size_t(ifc.ifc_len); off += sizeof(ifreq)) {
    ifreq* q = reinterpret_cast<ifreq*>(buf.data() + off);
    if (q->ifr_addr.sa_family != AF_INET) continue;
    char ip[INET_ADDRSTRLEN] = {0};
    ::inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(&q->ifr_addr)->sin_addr, ip, sizeof ip);
    Interface it;
    it.name = std::string(q->ifr_name, strnlen(q->ifr_name, IFNAMSIZ));
    it.ip = ip;
    ifreq fq;
    std::memset(&fq, 0, sizeof fq);
    std::memcpy(fq.ifr_name, q->ifr_name, IFNAMSIZ);
    if (::ioctl(fd, SIOCGIFFLAGS, &fq) == 0) {
      it.loopback = (fq.ifr_flags & IFF_LOOPBACK) != 0;
      it.up = (fq.ifr_flags & IFF_UP) != 0;
      it.multicast = (fq.ifr_flags & IFF_MULTICAST) != 0;
    } else {
      it.loopback = std::strncmp(ip, "127.", 4) == 0;
    }
    r.push_back(it);
  }
  ::close(fd);
  return r;
}
#endif

std::vector<Interface> list_ipv4_interfaces() {
  std::vector<Interface> r;
#ifdef _WIN32
  ULONG sz = 16 * 1024;
  std::vector<uint8_t> buf(sz);
  ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
  ULONG rc = GetAdaptersAddresses(AF_INET, flags, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()), &sz);
  if (rc == ERROR_BUFFER_OVERFLOW) {
    buf.resize(sz);
    rc = GetAdaptersAddresses(AF_INET, flags, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()), &sz);
  }
  if (rc != NO_ERROR) return r;
  for (auto* a = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()); a; a = a->Next) {
    for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
      if (u->Address.lpSockaddr->sa_family != AF_INET) continue;
      char ip[INET_ADDRSTRLEN] = {0};
      ::inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr)->sin_addr, ip, sizeof ip);
      Interface it;
      char name[256] = {0};
      WideCharToMultiByte(CP_UTF8, 0, a->FriendlyName, -1, name, sizeof name - 1, nullptr, nullptr);
      it.name = name;
      it.ip = ip;
      it.loopback = a->IfType == IF_TYPE_SOFTWARE_LOOPBACK;
      it.up = a->OperStatus == IfOperStatusUp;
      it.multicast = !(a->Flags & IP_ADAPTER_NO_MULTICAST);
      r.push_back(it);
    }
  }
#else
  ifaddrs* ifa = nullptr;
#if defined(__ANDROID__) && __ANDROID_API__ < 24
  // Built for an API level without getifaddrs (minSdk 21): use it if the running OS has it (API 24+),
  // otherwise enumerate with SIOCGIFCONF (no multicast flag details on very old devices).
  using GetFn = int (*)(ifaddrs**);
  using FreeFn = void (*)(ifaddrs*);
  static GetFn get_fn = reinterpret_cast<GetFn>(::dlsym(RTLD_DEFAULT, "getifaddrs"));
  static FreeFn free_fn = reinterpret_cast<FreeFn>(::dlsym(RTLD_DEFAULT, "freeifaddrs"));
  if (!get_fn || !free_fn || get_fn(&ifa) != 0) return list_ipv4_interfaces_ioctl();
#else
  if (::getifaddrs(&ifa) != 0) return r;
#endif
  for (ifaddrs* p = ifa; p; p = p->ifa_next) {
    if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
    char ip[INET_ADDRSTRLEN] = {0};
    ::inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(p->ifa_addr)->sin_addr, ip, sizeof ip);
    Interface it;
    it.name = p->ifa_name ? p->ifa_name : "";
    it.ip = ip;
    it.loopback = (p->ifa_flags & IFF_LOOPBACK) != 0;
    it.up = (p->ifa_flags & IFF_UP) != 0;
    it.multicast = (p->ifa_flags & IFF_MULTICAST) != 0;
    r.push_back(it);
  }
#if defined(__ANDROID__) && __ANDROID_API__ < 24
  free_fn(ifa);
#else
  ::freeifaddrs(ifa);
#endif
#endif
  return r;
}

// ---- poll ----------------------------------------------------------------------------------------

int poll_sockets(PollItem* items, size_t n, int timeout_ms) {
#ifdef _WIN32
  std::vector<WSAPOLLFD> p(n);
  for (size_t i = 0; i < n; ++i) {
    p[i].fd = S(items[i].fd);
    p[i].events = 0;
    if (items[i].events & kPollIn) p[i].events |= POLLRDNORM;
    if (items[i].events & kPollOut) p[i].events |= POLLWRNORM;
    p[i].revents = 0;
  }
  int rc = n ? ::WSAPoll(p.data(), ULONG(n), timeout_ms) : (::Sleep(DWORD(timeout_ms < 0 ? 0 : timeout_ms)), 0);
  if (rc < 0) return -1;
  for (size_t i = 0; i < n; ++i) {
    short r = 0;
    if (p[i].revents & (POLLRDNORM | POLLRDBAND | POLLIN)) r |= kPollIn;
    if (p[i].revents & (POLLWRNORM | POLLOUT)) r |= kPollOut;
    if (p[i].revents & (POLLERR | POLLNVAL)) r |= kPollErr;
    if (p[i].revents & POLLHUP) r |= kPollHup;
    items[i].revents = r;
  }
  return rc;
#else
  std::vector<pollfd> p(n);
  for (size_t i = 0; i < n; ++i) {
    p[i].fd = S(items[i].fd);
    p[i].events = 0;
    if (items[i].events & kPollIn) p[i].events |= POLLIN;
    if (items[i].events & kPollOut) p[i].events |= POLLOUT;
    p[i].revents = 0;
  }
  int rc = ::poll(p.data(), nfds_t(n), timeout_ms);
  if (rc < 0) return errno == EINTR ? 0 : -1;
  for (size_t i = 0; i < n; ++i) {
    short r = 0;
    if (p[i].revents & POLLIN) r |= kPollIn;
    if (p[i].revents & POLLOUT) r |= kPollOut;
    if (p[i].revents & (POLLERR | POLLNVAL)) r |= kPollErr;
    if (p[i].revents & POLLHUP) r |= kPollHup;
    items[i].revents = r;
  }
  return rc;
#endif
}

// ---- Waker ---------------------------------------------------------------------------------------

Waker::Waker() {
#ifdef _WIN32
  net_init();
  sock_t s = udp_socket(4, nullptr);
  if (s == kInvalidSock) return;
  SockAddr a;
  resolve_numeric("127.0.0.1", 0, &a, 4);
  if (::bind(S(s), reinterpret_cast<const sockaddr*>(a.storage), a.len) != 0) {
    sock_close(s);
    return;
  }
  SockAddr self = sock_local_addr(s);
  if (::connect(S(s), reinterpret_cast<const sockaddr*>(self.storage), self.len) != 0) {
    sock_close(s);
    return;
  }
  read_fd_ = write_fd_ = s;
#else
  int fds[2];
  if (::pipe(fds) != 0) return;
  for (int fd : fds) {
    ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    ::fcntl(fd, F_SETFD, FD_CLOEXEC);
  }
  read_fd_ = fds[0];
  write_fd_ = fds[1];
#endif
}

Waker::~Waker() {
#ifdef _WIN32
  sock_close(read_fd_);
#else
  if (read_fd_ != kInvalidSock) ::close(S(read_fd_));
  if (write_fd_ != kInvalidSock) ::close(S(write_fd_));
#endif
}

void Waker::wake() {
  if (write_fd_ == kInvalidSock) return;
  char c = 1;
#ifdef _WIN32
  ::send(S(write_fd_), &c, 1, 0);
#else
  ssize_t r = ::write(S(write_fd_), &c, 1);
  (void)r;
#endif
}

void Waker::drain() {
  if (read_fd_ == kInvalidSock) return;
  char buf[256];
#ifdef _WIN32
  while (::recv(S(read_fd_), buf, sizeof buf, 0) > 0) {
  }
#else
  while (::read(S(read_fd_), buf, sizeof buf) > 0) {
  }
#endif
}

}  // namespace ember_host
