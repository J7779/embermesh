// SPDX-License-Identifier: MIT
// mDNS / DNS-SD for Nearby devices (see mdns.h).
#include "host/mdns.h"

#include <algorithm>
#include <cstring>

#include "host/util.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ember_host {
namespace mdns {

namespace {

struct Reader {
  const uint8_t* p;
  size_t n, pos = 0;
  bool u16(uint16_t* v) {
    if (n - pos < 2 || pos > n) return false;
    *v = uint16_t((p[pos] << 8) | p[pos + 1]);
    pos += 2;
    return true;
  }
  bool u32(uint32_t* v) {
    if (pos > n || n - pos < 4) return false;
    *v = (uint32_t(p[pos]) << 24) | (uint32_t(p[pos + 1]) << 16) | (uint32_t(p[pos + 2]) << 8) | uint32_t(p[pos + 3]);
    pos += 4;
    return true;
  }
};

// Decodes a (possibly compressed) name starting at *pos; advances *pos past the name in place.
// Pointers must point strictly before the position they were read at (no loops, no forward
// references), at most kMaxJumps, decoded length <= kMaxName.
bool read_name(const uint8_t* p, size_t n, size_t* pos, std::string* out, const char** why) {
  out->clear();
  size_t cur = *pos, wire = 0, jumps = 0;
  bool jumped = false;
  size_t limit = cur;  // pointers must go below the start of the label sequence being read
  while (true) {
    if (cur >= n) {
      *why = "name runs past the packet";
      return false;
    }
    uint8_t len = p[cur];
    if (len == 0) {
      if (!jumped) *pos = cur + 1;
      ++wire;
      break;
    }
    if ((len & 0xC0) == 0xC0) {
      if (cur + 1 >= n) {
        *why = "truncated compression pointer";
        return false;
      }
      size_t target = (size_t(len & 0x3F) << 8) | p[cur + 1];
      if (target >= limit) {
        *why = "compression pointer does not point backwards";
        return false;
      }
      if (++jumps > kMaxJumps) {
        *why = "too many compression pointers";
        return false;
      }
      if (!jumped) *pos = cur + 2;
      jumped = true;
      limit = target;
      cur = target;
      continue;
    }
    if (len & 0xC0) {
      *why = "reserved label type";
      return false;
    }
    if (cur + 1 + len > n) {
      *why = "label runs past the packet";
      return false;
    }
    wire += 1 + len;
    if (wire > kMaxName) {
      *why = "name too long";
      return false;
    }
    if (!out->empty()) out->push_back('.');
    out->append(reinterpret_cast<const char*>(p + cur + 1), len);
    cur += 1 + len;
  }
  if (!utf8_valid(*out)) {
    *why = "name is not valid UTF-8";
    return false;
  }
  return true;
}

void put16(std::string& o, uint16_t v) {
  o.push_back(char(v >> 8));
  o.push_back(char(v & 0xff));
}
void put32(std::string& o, uint32_t v) {
  put16(o, uint16_t(v >> 16));
  put16(o, uint16_t(v & 0xffff));
}

}  // namespace

bool name_equal(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    char x = a[i], y = b[i];
    if (x >= 'A' && x <= 'Z') x = char(x - 'A' + 'a');
    if (y >= 'A' && y <= 'Z') y = char(y - 'A' + 'a');
    if (x != y) return false;
  }
  return true;
}

bool parse(const uint8_t* data, size_t len, Message* out, std::string* err) {
  const char* why = "";
  auto fail = [&](const char* w) {
    if (err) *err = w;
    return false;
  };
  *out = Message();
  if (!data || len < 12) return fail("shorter than a DNS header");
  if (len > kMaxPacket) return fail("datagram too large");
  Reader r{data, len};
  uint16_t qd, an, ns, ar;
  r.u16(&out->id);
  r.u16(&out->flags);
  r.u16(&qd);
  r.u16(&an);
  r.u16(&ns);
  r.u16(&ar);
  if (qd > kMaxEntries || an > kMaxEntries || ns > kMaxEntries || ar > kMaxEntries) return fail("too many entries");
  out->questions.reserve(qd);
  for (uint16_t i = 0; i < qd; ++i) {
    Question q;
    if (!read_name(data, len, &r.pos, &q.name, &why)) return fail(why);
    uint16_t cls;
    if (!r.u16(&q.type) || !r.u16(&cls)) return fail("truncated question");
    q.unicast_response = (cls & 0x8000) != 0;
    q.cls = uint16_t(cls & 0x7fff);
    out->questions.push_back(std::move(q));
  }
  size_t total = size_t(an) + ns + ar;
  out->records.reserve(total);
  for (size_t i = 0; i < total; ++i) {
    Record rr;
    if (!read_name(data, len, &r.pos, &rr.name, &why)) return fail(why);
    uint16_t cls, rdlen;
    if (!r.u16(&rr.type) || !r.u16(&cls) || !r.u32(&rr.ttl) || !r.u16(&rdlen)) return fail("truncated record header");
    rr.cache_flush = (cls & 0x8000) != 0;
    rr.cls = uint16_t(cls & 0x7fff);
    if (r.pos > len || len - r.pos < rdlen) return fail("record data runs past the packet");
    size_t rd = r.pos, rd_end = r.pos + rdlen;
    switch (rr.type) {
      case kA:
        if (rdlen != 4) return fail("A record is not 4 bytes");
        std::memcpy(rr.addr, data + rd, 4);
        rr.addr_len = 4;
        break;
      case kAAAA:
        if (rdlen != 16) return fail("AAAA record is not 16 bytes");
        std::memcpy(rr.addr, data + rd, 16);
        rr.addr_len = 16;
        break;
      case kPTR: {
        size_t p = rd;
        if (!read_name(data, rd_end, &p, &rr.target, &why)) return fail(why);
        if (p != rd_end) return fail("PTR data has trailing bytes");
        break;
      }
      case kSRV: {
        if (rdlen < 7) return fail("SRV record too short");
        Reader s{data, rd_end, rd};
        s.u16(&rr.priority);
        s.u16(&rr.weight);
        s.u16(&rr.port);
        size_t p = s.pos;
        if (!read_name(data, rd_end, &p, &rr.target, &why)) return fail(why);
        if (p != rd_end) return fail("SRV data has trailing bytes");
        break;
      }
      case kTXT: {
        if (rdlen > kMaxTxt) return fail("TXT record too large");
        size_t p = rd;
        bool valid = true;
        while (p < rd_end) {
          uint8_t l = data[p];
          if (p + 1 + l > rd_end) return fail("TXT string runs past its record");
          if (rr.txt.size() >= kMaxTxtStrings) return fail("too many TXT strings");
          std::string s(reinterpret_cast<const char*>(data + p + 1), l);
          if (!utf8_valid(s)) valid = false;
          rr.txt.push_back(std::move(s));
          p += 1 + l;
        }
        rr.txt_valid = valid;
        if (!valid) rr.txt.clear();
        break;
      }
      default:
        break;  // unknown types are skipped by their length
    }
    r.pos = rd_end;
    out->records.push_back(std::move(rr));
  }
  return true;
}

void Builder::question(const std::string& name, uint16_t type, bool unicast) {
  if (!put_name(name, q_)) return;
  put16(q_, type);
  put16(q_, uint16_t(kIN | (unicast ? 0x8000 : 0)));
  ++nq_;
}

bool Builder::put_name(const std::string& name, std::string& out) {
  size_t total = 0;
  for (const auto& label : split(name, '.')) {
    if (label.empty()) continue;
    if (label.size() > 63) {
      ok_ = false;
      return false;
    }
    total += 1 + label.size();
    out.push_back(char(label.size()));
    out += label;
  }
  out.push_back('\0');
  if (total + 1 > kMaxName) ok_ = false;
  return ok_;
}

void Builder::rr_header(const std::string& name, uint16_t type, uint32_t ttl, bool flush, std::string& out) {
  put_name(name, out);
  put16(out, type);
  put16(out, uint16_t(kIN | (flush ? 0x8000 : 0)));
  put32(out, ttl);
}

void Builder::ptr(const std::string& name, const std::string& target, uint32_t ttl) {
  std::string& o = additional_ ? ar_ : an_;
  rr_header(name, kPTR, ttl, false, o);
  std::string rd;
  put_name(target, rd);
  put16(o, uint16_t(rd.size()));
  o += rd;
  (additional_ ? nar_ : nan_)++;
}

void Builder::srv(const std::string& name, const std::string& target, uint16_t port, uint32_t ttl) {
  std::string& o = additional_ ? ar_ : an_;
  rr_header(name, kSRV, ttl, true, o);
  std::string rd;
  put16(rd, 0);
  put16(rd, 0);
  put16(rd, port);
  put_name(target, rd);
  put16(o, uint16_t(rd.size()));
  o += rd;
  (additional_ ? nar_ : nan_)++;
}

void Builder::txt(const std::string& name, const std::vector<std::string>& strings, uint32_t ttl) {
  std::string& o = additional_ ? ar_ : an_;
  rr_header(name, kTXT, ttl, true, o);
  std::string rd;
  for (const auto& s : strings) {
    if (s.size() > 255) {
      ok_ = false;
      return;
    }
    rd.push_back(char(s.size()));
    rd += s;
  }
  put16(o, uint16_t(rd.size()));
  o += rd;
  (additional_ ? nar_ : nan_)++;
}

void Builder::a(const std::string& name, const uint8_t ip[4], uint32_t ttl) {
  std::string& o = additional_ ? ar_ : an_;
  rr_header(name, kA, ttl, true, o);
  put16(o, 4);
  o.append(reinterpret_cast<const char*>(ip), 4);
  (additional_ ? nar_ : nan_)++;
}

void Builder::aaaa(const std::string& name, const uint8_t ip[16], uint32_t ttl) {
  std::string& o = additional_ ? ar_ : an_;
  rr_header(name, kAAAA, ttl, true, o);
  put16(o, 16);
  o.append(reinterpret_cast<const char*>(ip), 16);
  (additional_ ? nar_ : nan_)++;
}

std::string Builder::finish() const {
  std::string o;
  put16(o, id_);
  put16(o, flags_);
  put16(o, nq_);
  put16(o, nan_);
  put16(o, 0);
  put16(o, nar_);
  return o + q_ + an_ + ar_;
}

}  // namespace mdns

// ---- interfaces ----------------------------------------------------------------------------------

namespace {

constexpr uint16_t kMdnsPort = 5353;
const char* kGroup4 = "224.0.0.251";
constexpr uint32_t kTtl = 120;             // seconds, our records
constexpr uint32_t kMaxTtl = 4500;          // cap for cached records
constexpr size_t kMaxInstances = 64;
constexpr size_t kMaxHosts = 32;
constexpr size_t kMaxAddrsPerHost = 8;
constexpr uint32_t kPacketsPerSecond = 500;        // all senders together
constexpr uint32_t kPacketsPerSourcePerSecond = 40; // one sender (a flooder cannot starve the others)
constexpr size_t kMaxSources = 64;

struct If6 {
  uint32_t index = 0;
  std::string name;
  uint8_t ll[16] = {0};
  bool have_ll = false;
};

std::vector<If6> list_v6() {
  std::vector<If6> r;
#ifdef _WIN32
  ULONG sz = 16 * 1024;
  std::vector<uint8_t> buf(sz);
  ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
  ULONG rc = GetAdaptersAddresses(AF_INET6, flags, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()), &sz);
  if (rc == ERROR_BUFFER_OVERFLOW) {
    buf.resize(sz);
    rc = GetAdaptersAddresses(AF_INET6, flags, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()), &sz);
  }
  if (rc != NO_ERROR) return r;
  for (auto* a = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()); a; a = a->Next) {
    if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK || (a->Flags & IP_ADAPTER_NO_MULTICAST)) continue;
    If6 it;
    it.index = a->Ipv6IfIndex;
    it.name = std::to_string(a->Ipv6IfIndex);
    for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
      if (u->Address.lpSockaddr->sa_family != AF_INET6) continue;
      auto* s6 = reinterpret_cast<sockaddr_in6*>(u->Address.lpSockaddr);
      if (s6->sin6_addr.s6_addr[0] == 0xfe && (s6->sin6_addr.s6_addr[1] & 0xc0) == 0x80) {
        std::memcpy(it.ll, s6->sin6_addr.s6_addr, 16);
        it.have_ll = true;
        break;
      }
    }
    if (it.index) r.push_back(it);
  }
#else
  ifaddrs* ifa = nullptr;
  if (::getifaddrs(&ifa) != 0) return r;
  for (ifaddrs* p = ifa; p; p = p->ifa_next) {
    if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET6 || !p->ifa_name) continue;
    if (!(p->ifa_flags & IFF_UP) || (p->ifa_flags & IFF_LOOPBACK) || !(p->ifa_flags & IFF_MULTICAST)) continue;
    auto* s6 = reinterpret_cast<sockaddr_in6*>(p->ifa_addr);
    if (!(s6->sin6_addr.s6_addr[0] == 0xfe && (s6->sin6_addr.s6_addr[1] & 0xc0) == 0x80)) continue;
    uint32_t idx = ::if_nametoindex(p->ifa_name);
    if (!idx) continue;
    bool dup = false;
    for (auto& e : r) dup |= e.index == idx;
    if (dup) continue;
    If6 it;
    it.index = idx;
    it.name = p->ifa_name;
    std::memcpy(it.ll, s6->sin6_addr.s6_addr, 16);
    it.have_ll = true;
    r.push_back(it);
  }
  ::freeifaddrs(ifa);
#endif
  return r;
}

std::string scope_name(uint32_t idx) {
#ifdef _WIN32
  return std::to_string(idx);
#else
  char b[IF_NAMESIZE + 1] = {0};
  if (::if_indextoname(idx, b)) return b;
  return std::to_string(idx);
#endif
}

void set_opt(sock_t s, int level, int name, int v) {
#ifdef _WIN32
  ::setsockopt(SOCKET(s), level, name, reinterpret_cast<const char*>(&v), sizeof v);
#else
  ::setsockopt(int(s), level, name, &v, sizeof v);
#endif
}

std::string lower(std::string s) { return to_lower(s); }

bool valid_token(const std::string& t) {
  if (t.size() != 8) return false;
  for (char c : t)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

bool v4_private(const uint8_t* a) {
  return a[0] == 10 || (a[0] == 172 && (a[1] & 0xf0) == 16) || (a[0] == 192 && a[1] == 168) || (a[0] == 169 && a[1] == 254);
}

}  // namespace

std::vector<LocalNet> local_networks() {
  std::vector<LocalNet> r;
#ifdef _WIN32
  ULONG sz = 32 * 1024;
  std::vector<uint8_t> buf(sz);
  ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
  ULONG rc = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()), &sz);
  if (rc == ERROR_BUFFER_OVERFLOW) {
    buf.resize(sz);
    rc = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()), &sz);
  }
  if (rc != NO_ERROR) return r;
  for (auto* a = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()); a; a = a->Next) {
    if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
    for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
      LocalNet n;
      unsigned plen = u->OnLinkPrefixLength;
      char ip[64] = {0};
      if (u->Address.lpSockaddr->sa_family == AF_INET) {
        auto* s4 = reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr);
        std::memcpy(n.addr, &s4->sin_addr, 4);
        if (!v4_private(n.addr) || plen > 32) continue;
        for (unsigned b = 0; b < plen; ++b) n.mask[b / 8] |= uint8_t(0x80 >> (b % 8));
        ::inet_ntop(AF_INET, n.addr, ip, sizeof ip);
        n.bind = ip;
      } else if (u->Address.lpSockaddr->sa_family == AF_INET6) {
        auto* s6 = reinterpret_cast<sockaddr_in6*>(u->Address.lpSockaddr);
        std::memcpy(n.addr, s6->sin6_addr.s6_addr, 16);
        if (!(n.addr[0] == 0xfe && (n.addr[1] & 0xc0) == 0x80)) continue;
        n.v6 = true;
        n.scope = a->Ipv6IfIndex;
        for (int b = 0; b < 8; ++b) n.mask[b] = 0xff;
        ::inet_ntop(AF_INET6, n.addr, ip, sizeof ip);
        n.bind = std::string(ip) + "%" + std::to_string(n.scope);
      } else {
        continue;
      }
      r.push_back(n);
    }
  }
#else
  ifaddrs* ifa = nullptr;
  if (::getifaddrs(&ifa) != 0) return r;
  for (ifaddrs* p = ifa; p; p = p->ifa_next) {
    if (!p->ifa_addr || !p->ifa_name || !(p->ifa_flags & IFF_UP) || (p->ifa_flags & IFF_LOOPBACK)) continue;
    LocalNet n;
    char ip[64] = {0};
    if (p->ifa_addr->sa_family == AF_INET) {
      std::memcpy(n.addr, &reinterpret_cast<sockaddr_in*>(p->ifa_addr)->sin_addr, 4);
      if (!v4_private(n.addr) || !p->ifa_netmask) continue;
      std::memcpy(n.mask, &reinterpret_cast<sockaddr_in*>(p->ifa_netmask)->sin_addr, 4);
      ::inet_ntop(AF_INET, n.addr, ip, sizeof ip);
      n.bind = ip;
    } else if (p->ifa_addr->sa_family == AF_INET6) {
      std::memcpy(n.addr, reinterpret_cast<sockaddr_in6*>(p->ifa_addr)->sin6_addr.s6_addr, 16);
      if (!(n.addr[0] == 0xfe && (n.addr[1] & 0xc0) == 0x80) || !(p->ifa_flags & IFF_MULTICAST)) continue;
      n.v6 = true;
      n.scope = ::if_nametoindex(p->ifa_name);
      if (!n.scope) continue;
      for (int b = 0; b < 8; ++b) n.mask[b] = 0xff;
      ::inet_ntop(AF_INET6, n.addr, ip, sizeof ip);
      n.bind = std::string(ip) + "%" + p->ifa_name;
    } else {
      continue;
    }
    r.push_back(n);
  }
  ::freeifaddrs(ifa);
#endif
  return r;
}

bool on_local_network(const std::string& ip_in, const std::vector<LocalNet>& nets) {
  std::string ip = ip_in.substr(0, ip_in.find('%'));
  if (ip.compare(0, 7, "::ffff:") == 0) ip = ip.substr(7);
  uint8_t a[16] = {0};
  if (::inet_pton(AF_INET, ip.c_str(), a) == 1) {
    for (const auto& n : nets) {
      if (n.v6) continue;
      bool in = true;
      for (int i = 0; i < 4; ++i) in &= (a[i] & n.mask[i]) == (n.addr[i] & n.mask[i]);
      if (in) return true;
    }
    return false;
  }
  if (::inet_pton(AF_INET6, ip.c_str(), a) != 1) return false;
  if (a[0] == 0xfe && (a[1] & 0xc0) == 0x80) return true;  // link-local
  if ((a[0] & 0xfe) == 0xfc)                               // ULA: only inside an interface prefix
    for (const auto& n : nets) {
      if (!n.v6) continue;
      bool in = true;
      for (int i = 0; i < 16; ++i) in &= (a[i] & n.mask[i]) == (n.addr[i] & n.mask[i]);
      if (in) return true;
    }
  return false;
}

std::string source_key(const std::string& ip_in) {
  std::string ip = ip_in.substr(0, ip_in.find('%'));
  if (ip.compare(0, 7, "::ffff:") == 0) ip = ip.substr(7);
  uint8_t a[16] = {0};
  if (::inet_pton(AF_INET6, ip.c_str(), a) == 1) return "v6/64:" + hex_encode(a, 8);
  return ip;
}

namespace {

}  // namespace

MdnsAgent::MdnsAgent(Loop& loop) : loop_(loop), alive_(std::make_shared<bool>(true)) {}

MdnsAgent::~MdnsAgent() {
  stop();
  *alive_ = false;
}

std::string MdnsAgent::instance_name() const { return "EmberMesh-" + opt_.token + "." + service_name(); }

bool MdnsAgent::open_v4(std::string* err) {
  sock_t s = udp_socket(4, err);
  if (s == kInvalidSock) return false;
  if (!udp_bind(s, "0.0.0.0", kMdnsPort, true, err)) {
    sock_close(s);
    return false;
  }
  udp_set_multicast_ttl_loop(s, 255, true);
  v4_.fd = s;
  v4_.ifaddr.clear();
  for (const auto& i : list_ipv4_interfaces()) {
    if (i.loopback || !i.up || !i.multicast) continue;
    std::string e;
    if (udp_join_multicast_v4(s, kGroup4, i.ip, &e)) v4_.ifaddr.push_back(i.ip);
  }
  if (v4_.ifaddr.empty()) {
    std::string e;
    if (udp_join_multicast_v4(s, kGroup4, "", &e)) v4_.ifaddr.push_back("");  // default route only
  }
  if (v4_.ifaddr.empty()) {
    *err = "could not join 224.0.0.251 on any interface";
    sock_close(s);
    v4_.fd = kInvalidSock;
    return false;
  }
  v4_.watch = loop_.watch(s, kPollIn, [this](short) { on_readable(v4_); });
  return true;
}

bool MdnsAgent::open_v6(std::string* err) {
  sock_t s = udp_socket(6, err);
  if (s == kInvalidSock) return false;
  set_opt(s, IPPROTO_IPV6, IPV6_V6ONLY, 1);
  set_opt(s, SOL_SOCKET, SO_REUSEADDR, 1);
#ifdef SO_REUSEPORT
  set_opt(s, SOL_SOCKET, SO_REUSEPORT, 1);
#endif
  sockaddr_in6 a;
  std::memset(&a, 0, sizeof a);
  a.sin6_family = AF_INET6;
  a.sin6_port = htons(kMdnsPort);
  a.sin6_addr = in6addr_any;
#ifdef _WIN32
  int rc = ::bind(SOCKET(s), reinterpret_cast<sockaddr*>(&a), sizeof a);
#else
  int rc = ::bind(int(s), reinterpret_cast<sockaddr*>(&a), sizeof a);
#endif
  if (rc != 0) {
    *err = "bind [::]:5353: " + sock_strerror(sock_error());
    sock_close(s);
    return false;
  }
  set_opt(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, 255);
  set_opt(s, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, 1);
  v6_.fd = s;
  v6_.ifindex.clear();
  ipv6_mreq m;
  std::memset(&m, 0, sizeof m);
  ::inet_pton(AF_INET6, "ff02::fb", &m.ipv6mr_multiaddr);
  for (const auto& i : list_v6()) {
    m.ipv6mr_interface = i.index;
#ifdef _WIN32
    int jr = ::setsockopt(SOCKET(s), IPPROTO_IPV6, IPV6_JOIN_GROUP, reinterpret_cast<const char*>(&m), sizeof m);
#else
    int jr = ::setsockopt(int(s), IPPROTO_IPV6, IPV6_JOIN_GROUP, &m, sizeof m);
#endif
    if (jr == 0) v6_.ifindex.push_back(i.index);
  }
  if (v6_.ifindex.empty()) {
    *err = "no IPv6 multicast interface";
    sock_close(s);
    v6_.fd = kInvalidSock;
    return false;
  }
  v6_.v6 = true;
  v6_.watch = loop_.watch(s, kPollIn, [this](short) { on_readable(v6_); });
  return true;
}

bool MdnsAgent::start(const Options& o, std::string* err) {
  stop();
  opt_ = o;
  if (!valid_token(opt_.token) || opt_.port == 0) {
    *err = "bad token or port";
    return false;
  }
  std::string e4, e6;
  bool ok4 = open_v4(&e4);
  bool ok6 = opt_.ipv6 && open_v6(&e6);
  if (!ok4 && !ok6) {
    *err = "mDNS unavailable: " + e4 + (e6.empty() ? "" : "; " + e6);
    return false;
  }
  if (!ok6 && opt_.ipv6) EH_LOGD("mdns: IPv6 not used: %s", e6.c_str());
  running_ = true;
  EH_LOGI("mdns: advertising %s on port %u (%s%s)", instance_name().c_str(), unsigned(opt_.port), ok4 ? "IPv4" : "",
          ok6 ? (ok4 ? " + IPv6" : "IPv6") : "");
  // Announce three times (RFC 6762 §8.3: at least twice, one second apart) and browse.
  announces_left_ = 3;
  std::weak_ptr<bool> w = alive_;
  announce_timer_ = loop_.timer(20, [this, w] {
    auto a = w.lock();
    if (!a || !*a || !running_) return;
    announce_timer_ = 0;
    send_all(build_records(0, false, false, true, false));
    if (--announces_left_ > 0) {
      announce_timer_ = loop_.timer(1000, [this, w] {
        auto a2 = w.lock();
        if (!a2 || !*a2 || !running_) return;
        announce_timer_ = 0;
        send_all(build_records(0, false, false, true, false));
        if (--announces_left_ > 0)
          announce_timer_ = loop_.timer(1000, [this] {
            announce_timer_ = 0;
            if (running_) send_all(build_records(0, false, false, true, false));
          });
      });
    }
  });
  query_delay_ = 1000;
  schedule_query(50);
  tick_timer_ = loop_.timer(1000, [this] { tick(); });
  return true;
}

void MdnsAgent::stop() {
  if (!running_ && v4_.fd == kInvalidSock && v6_.fd == kInvalidSock) return;
  if (running_) send_all(build_records(0, false, true, true, false));  // goodbye: TTL 0
  running_ = false;
  for (uint64_t* t : {&query_timer_, &tick_timer_, &announce_timer_})
    if (*t) {
      loop_.cancel(*t);
      *t = 0;
    }
  for (Sock* s : {&v4_, &v6_}) {
    if (s->watch) loop_.unwatch(s->watch);
    s->watch = 0;
    if (s->fd != kInvalidSock) sock_close(s->fd);
    s->fd = kInvalidSock;
  }
  instances_.clear();
  hosts_.clear();
  last_answer_.clear();
}

std::string MdnsAgent::build_records(uint16_t id, bool legacy, bool goodbye, bool include_ptr, bool service_enum) {
  uint32_t ttl = goodbye ? 0 : kTtl;
  mdns::Builder b(id, 0x8400);  // response, authoritative
  std::string inst = instance_name();
  if (service_enum) b.ptr("_services._dns-sd._udp.local", service_name(), ttl);
  if (include_ptr) b.ptr(service_name(), inst, ttl);
  b.srv(inst, host_name(), opt_.port, ttl);
  b.txt(inst, {"ember=1", "v=1", "link=stream", "t=" + opt_.token}, ttl);
  b.begin_additional();
  for (const auto& a4 : opt_.ipv4) {
    uint8_t ip[4];
    if (::inet_pton(AF_INET, a4.c_str(), ip) == 1) b.a(host_name(), ip, ttl);
  }
  if (v6_.fd != kInvalidSock)
    for (const auto& i : list_v6())
      if (i.have_ll) b.aaaa(host_name(), i.ll, ttl);
  (void)legacy;
  return b.finish();
}

void MdnsAgent::send_all(const std::string& pkt) {
  if (pkt.size() > mdns::kMaxPacket) return;
  if (v4_.fd != kInvalidSock) {
    SockAddr g;
    resolve_numeric(kGroup4, kMdnsPort, &g, 4);
    for (const auto& ip : v4_.ifaddr) {
      udp_set_multicast_if_v4(v4_.fd, ip);
      udp_sendto(v4_.fd, pkt.data(), pkt.size(), g);
    }
  }
  if (v6_.fd != kInvalidSock) {
    SockAddr g;
    resolve_numeric("ff02::fb", kMdnsPort, &g, 6);
    for (uint32_t idx : v6_.ifindex) {
#ifdef _WIN32
      DWORD i = idx;
      ::setsockopt(SOCKET(v6_.fd), IPPROTO_IPV6, IPV6_MULTICAST_IF, reinterpret_cast<const char*>(&i), sizeof i);
#else
      unsigned int i = idx;
      ::setsockopt(int(v6_.fd), IPPROTO_IPV6, IPV6_MULTICAST_IF, &i, sizeof i);
#endif
      udp_sendto(v6_.fd, pkt.data(), pkt.size(), g);
    }
  }
}

void MdnsAgent::schedule_query(uint64_t delay) {
  if (query_timer_) loop_.cancel(query_timer_);
  query_timer_ = loop_.timer(delay, [this] {
    query_timer_ = 0;
    if (!running_) return;
    send_query();
    query_delay_ = std::min<uint64_t>(query_delay_ * 2, 60000);  // RFC 6762 §5.2 backoff, capped at 60 s
    schedule_query(query_delay_);
  });
}

void MdnsAgent::send_query() {
  mdns::Builder b(0, 0);
  b.question(service_name(), mdns::kPTR);
  // Follow-up questions for instances / hosts we know of but miss data for.
  size_t extra = 0;
  for (auto& kv : instances_) {
    if (extra >= 8) break;
    const Instance& in = kv.second;
    if (!in.have_srv || !in.have_txt) {
      b.question(in.name, mdns::kANY);
      ++extra;
    } else if (!hosts_.count(lower(in.target))) {
      b.question(in.target, mdns::kA);
      b.question(in.target, mdns::kAAAA);
      extra += 2;
    }
  }
  send_all(b.finish());
  ++stats_.queries_sent;
}

void MdnsAgent::on_readable(Sock& s) {
  uint8_t buf[mdns::kMaxPacket + 1];
  for (int i = 0; i < 32 && s.fd != kInvalidSock; ++i) {
    SockAddr from;
    long n = udp_recvfrom(s.fd, buf, sizeof buf, &from);
    if (n < 0) return;
    uint64_t now = steady_ms();
    if (now - budget_window_ >= 1000) {
      budget_window_ = now;
      budget_used_ = 0;
      per_source_.clear();
    }
    std::string src = from.ip();
    uint32_t& used_by_src = per_source_.size() < kMaxSources || per_source_.count(src) ? per_source_[src] : overflow_source_;
    if (++budget_used_ > kPacketsPerSecond || ++used_by_src > kPacketsPerSourcePerSecond) {
      ++stats_.rx_dropped_budget;
      continue;
    }
    ++stats_.rx_packets;
    if (size_t(n) > mdns::kMaxPacket) {
      ++stats_.rx_bad;
      continue;
    }
    mdns::Message m;
    if (!mdns::parse(buf, size_t(n), &m)) {
      ++stats_.rx_bad;
      continue;
    }
    handle(m, from, s);
  }
}

void MdnsAgent::handle(const mdns::Message& m, const SockAddr& from, Sock& s) {
  if (!m.response()) {
    answer(m, from, s);
    return;
  }
  if (from.port() != kMdnsPort) return;  // RFC 6762 §11: responses must come from port 5353
  uint64_t now = steady_ms();
  bool changed = false;
  std::string svc = service_name();
  // Scope id for IPv6 link-local addresses learned from this packet: the interface it came in on,
  // which the kernel puts into the source address of a link-local sender.
  uint32_t scope = 0;
  if (s.v6 && from.len >= sizeof(sockaddr_in6)) {
    sockaddr_in6 f;
    std::memcpy(&f, from.storage, sizeof f);
    scope = f.sin6_scope_id;
  }
  for (const auto& rr : m.records) {
    uint64_t ttl_ms = uint64_t(std::min<uint32_t>(rr.ttl, kMaxTtl)) * 1000;
    if (rr.type == mdns::kPTR && mdns::name_equal(rr.name, svc)) {
      if (rr.target.size() <= svc.size() + 1 ||
          !mdns::name_equal(rr.target.substr(rr.target.size() - svc.size()), svc))
        continue;
      std::string key = lower(rr.target);
      if (rr.ttl == 0) {
        auto it = instances_.find(key);
        if (it != instances_.end()) {
          if (it->second.reported && on_lost) on_lost(it->second.token);
          instances_.erase(it);
        }
        continue;
      }
      if (!instances_.count(key) && instances_.size() >= kMaxInstances) continue;
      Instance& in = instances_[key];
      in.name = rr.target;
      in.expires = std::max(in.expires, now + ttl_ms);
      changed = true;
      continue;
    }
    auto it = instances_.find(lower(rr.name));
    if (rr.type == mdns::kSRV || rr.type == mdns::kTXT) {
      // Accept SRV/TXT for instances of our service type even without a PTR in this packet.
      std::string n = rr.name;
      if (it == instances_.end()) {
        if (n.size() <= svc.size() + 1 || !mdns::name_equal(n.substr(n.size() - svc.size()), svc)) continue;
        if (instances_.size() >= kMaxInstances || rr.ttl == 0) continue;
        Instance& in = instances_[lower(n)];
        in.name = n;
        in.expires = now + ttl_ms;
        it = instances_.find(lower(n));
      }
      Instance& in = it->second;
      if (rr.ttl == 0) {
        if (in.reported && on_lost) on_lost(in.token);
        instances_.erase(it);
        continue;
      }
      if (rr.type == mdns::kSRV) {
        in.port = rr.port;
        in.target = rr.target;
        in.have_srv = rr.port != 0 && !rr.target.empty();
      } else {
        in.have_txt = true;
        // Exactly what CONNECTIVITY §3.2 requires: ember=1, v=1, link=stream, t=<8 hex>.
        bool ember = false, v1 = false, stream = false;
        std::string tok;
        if (rr.txt_valid)
          for (const auto& t : rr.txt) {
            if (t == "ember=1") ember = true;
            else if (t == "v=1") v1 = true;
            else if (t == "link=stream") stream = true;
            else if (t.compare(0, 2, "t=") == 0) tok = t.substr(2);
          }
        in.valid_txt = ember && v1 && stream && valid_token(tok);
        in.token = in.valid_txt ? tok : std::string();
      }
      in.expires = std::max(in.expires, now + ttl_ms);
      changed = true;
      continue;
    }
    if (rr.type == mdns::kA || rr.type == mdns::kAAAA) {
      std::string key = lower(rr.name);
      // Only hosts some instance points at (or might, within this packet) are kept.
      bool wanted = false;
      for (auto& kv : instances_) wanted |= mdns::name_equal(kv.second.target, rr.name);
      for (const auto& r2 : m.records) wanted |= r2.type == mdns::kSRV && mdns::name_equal(r2.target, rr.name);
      if (!wanted) continue;
      if (!hosts_.count(key) && hosts_.size() >= kMaxHosts) continue;
      HostAddrs& h = hosts_[key];
      if (rr.ttl == 0) {
        hosts_.erase(key);
        changed = true;
        continue;
      }
      char ip[64] = {0};
      std::string a;
      if (rr.addr_len == 4) {
        ::inet_ntop(AF_INET, rr.addr, ip, sizeof ip);
        a = ip;
        if (a.compare(0, 4, "127.") == 0 || a.compare(0, 8, "169.254.") == 0) a.clear();
        if (!a.empty() && std::find(h.v4.begin(), h.v4.end(), a) == h.v4.end() && h.v4.size() < kMaxAddrsPerHost)
          h.v4.push_back(a);
      } else if (rr.addr_len == 16) {
        bool ll = rr.addr[0] == 0xfe && (rr.addr[1] & 0xc0) == 0x80;
        if (!ll || !scope) continue;  // only link-local, and only with a known interface
        ::inet_ntop(AF_INET6, rr.addr, ip, sizeof ip);
        a = std::string(ip) + "%" + scope_name(scope);
        if (std::find(h.v6.begin(), h.v6.end(), a) == h.v6.end() && h.v6.size() < kMaxAddrsPerHost) h.v6.push_back(a);
      }
      h.expires = std::max(h.expires, now + ttl_ms);
      changed = true;
    }
  }
  if (changed) report();
}

void MdnsAgent::report() {
  std::string own = lower(instance_name());
  for (auto& kv : instances_) {
    Instance& in = kv.second;
    if (kv.first == own || !in.valid_txt || !in.have_srv || in.token == opt_.token) continue;
    auto h = hosts_.find(lower(in.target));
    if (h == hosts_.end() || (h->second.v4.empty() && h->second.v6.empty())) continue;
    NearbyPeer p;
    p.instance = in.name;
    p.token = in.token;
    p.port = in.port;
    p.addrs = h->second.v6;  // IPv6 link-local first (CONNECTIVITY §3.3)
    for (auto& a : h->second.v4) p.addrs.push_back(a);
    std::string key = p.token + "|" + std::to_string(p.port);
    for (auto& a : p.addrs) key += "|" + a;
    if (in.reported && in.reported_key == key) continue;
    in.reported = true;
    in.reported_key = key;
    if (on_found) on_found(p);
  }
}

void MdnsAgent::answer(const mdns::Message& q, const SockAddr& from, Sock& s) {
  std::string svc = service_name(), inst = instance_name(), host = host_name();
  bool want_ptr = false, want_inst = false, want_host = false, want_enum = false;
  for (const auto& qq : q.questions) {
    bool any = qq.type == mdns::kANY;
    if (mdns::name_equal(qq.name, svc) && (any || qq.type == mdns::kPTR)) want_ptr = true;
    if (mdns::name_equal(qq.name, inst) && (any || qq.type == mdns::kSRV || qq.type == mdns::kTXT)) want_inst = true;
    if (mdns::name_equal(qq.name, host) && (any || qq.type == mdns::kA || qq.type == mdns::kAAAA)) want_host = true;
    if (mdns::name_equal(qq.name, "_services._dns-sd._udp.local") && (any || qq.type == mdns::kPTR)) want_enum = true;
  }
  if (!want_ptr && !want_inst && !want_host && !want_enum) return;
  // At most one multicast answer per second per kind of question (RFC 6762 §6).
  std::string key = std::string(want_ptr ? "p" : "") + (want_inst ? "i" : "") + (want_host ? "h" : "") + (want_enum ? "e" : "") +
                    (s.v6 ? "6" : "4");
  uint64_t now = steady_ms();
  bool legacy = from.port() != kMdnsPort;  // one-shot resolver: unicast reply with its id
  if (!legacy) {
    uint64_t& last = last_answer_[key];
    if (last && now - last < 1000) return;
    last = now;
    if (last_answer_.size() > 64) last_answer_.clear();
  }
  std::string pkt = build_records(legacy ? q.id : 0, legacy, false, want_ptr || want_enum, want_enum);
  if (legacy) {
    udp_sendto(s.fd, pkt.data(), pkt.size(), from);
  } else {
    send_all(pkt);
  }
  ++stats_.answers_sent;
}

void MdnsAgent::tick() {
  tick_timer_ = 0;
  if (!running_) return;
  uint64_t now = steady_ms();
  for (auto it = instances_.begin(); it != instances_.end();) {
    if (it->second.expires && it->second.expires < now) {
      if (it->second.reported && on_lost) on_lost(it->second.token);
      it = instances_.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = hosts_.begin(); it != hosts_.end();) {
    if (it->second.expires < now)
      it = hosts_.erase(it);
    else
      ++it;
  }
  tick_timer_ = loop_.timer(1000, [this] { tick(); });
}

std::vector<NearbyPeer> MdnsAgent::peers() const {
  std::vector<NearbyPeer> r;
  for (const auto& kv : instances_) {
    const Instance& in = kv.second;
    if (!in.reported) continue;
    auto h = hosts_.find(lower(in.target));
    NearbyPeer p;
    p.instance = in.name;
    p.token = in.token;
    p.port = in.port;
    if (h != hosts_.end()) {
      p.addrs = h->second.v6;
      for (auto& a : h->second.v4) p.addrs.push_back(a);
    }
    r.push_back(p);
  }
  return r;
}

}  // namespace ember_host
