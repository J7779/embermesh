// SPDX-License-Identifier: MIT
// Minimal multicast DNS (RFC 6762) responder + DNS-SD (RFC 6763) browser for "Nearby devices"
// (docs/CONNECTIVITY.md §3). Advertises exactly one instance `EmberMesh-<token>._embermesh._tcp.local`
// with TXT `ember=1 v=1 link=stream t=<token>` and the host name `EmberMesh-<token>.local` (never the
// computer's name), and reports other instances with a valid TXT record.
//
// Two layers:
//   * a pure DNS message codec (parse / build) — everything on the LAN is hostile: bounded names
//     (labels <= 63, names <= 255, <= 16 compression jumps, pointers only backwards), bounded
//     record counts, TXT <= 1300 bytes, valid UTF-8, no allocation sized by input beyond the caps;
//   * MdnsAgent on the Loop: one IPv4 socket on 224.0.0.251:5353 and one IPv6 socket on
//     ff02::fb:5353 (address/port reuse, so it coexists with the OS responder), per-interface
//     joins, announcements, answers, queries with backoff, a bounded cache with TTL expiry, and a
//     per-second packet budget.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "host/loop.h"
#include "host/net.h"

namespace ember_host {
namespace mdns {

enum : uint16_t { kA = 1, kPTR = 12, kTXT = 16, kAAAA = 28, kSRV = 33, kANY = 255, kIN = 1 };

constexpr size_t kMaxPacket = 9000;     // larger datagrams are ignored
constexpr size_t kMaxEntries = 64;      // per section
constexpr size_t kMaxName = 255;        // wire bytes of a decoded name
constexpr size_t kMaxJumps = 16;        // compression pointers followed per name
constexpr size_t kMaxTxt = 1300;        // TXT rdata bytes
constexpr size_t kMaxTxtStrings = 32;

struct Question {
  std::string name;  // dotted, without the trailing dot, case preserved
  uint16_t type = 0, cls = kIN;
  bool unicast_response = false;  // QU bit
};

struct Record {
  std::string name;
  uint16_t type = 0, cls = kIN;
  bool cache_flush = false;
  uint32_t ttl = 0;
  std::string target;              // PTR target, SRV target
  uint16_t priority = 0, weight = 0, port = 0;  // SRV
  std::vector<std::string> txt;    // TXT strings (only if all are valid UTF-8)
  bool txt_valid = false;
  uint8_t addr[16] = {0};          // A (4 bytes) / AAAA (16 bytes)
  size_t addr_len = 0;
};

struct Message {
  uint16_t id = 0, flags = 0;
  bool response() const { return (flags & 0x8000) != 0; }
  std::vector<Question> questions;
  std::vector<Record> records;  // answers, authority and additional, in order
};

// Parses a DNS message. Returns false (and why) for anything malformed; never reads out of
// bounds, never loops, never allocates more than the caps above.
bool parse(const uint8_t* data, size_t len, Message* out, std::string* err = nullptr);

// Case-insensitive DNS name comparison (ASCII only, as RFC 6762 §16).
bool name_equal(const std::string& a, const std::string& b);

// Builds a message (no name compression: our packets are small).
class Builder {
 public:
  explicit Builder(uint16_t id = 0, uint16_t flags = 0) : id_(id), flags_(flags) {}
  void question(const std::string& name, uint16_t type, bool unicast = false);
  void ptr(const std::string& name, const std::string& target, uint32_t ttl);
  void srv(const std::string& name, const std::string& target, uint16_t port, uint32_t ttl);
  void txt(const std::string& name, const std::vector<std::string>& strings, uint32_t ttl);
  void a(const std::string& name, const uint8_t ip[4], uint32_t ttl);
  void aaaa(const std::string& name, const uint8_t ip[16], uint32_t ttl);
  // Records added after this call go to the additional section.
  void begin_additional() { additional_ = true; }
  std::string finish() const;
  bool ok() const { return ok_; }

 private:
  void rr_header(const std::string& name, uint16_t type, uint32_t ttl, bool flush, std::string& out);
  bool put_name(const std::string& name, std::string& out);
  uint16_t id_, flags_;
  std::string q_, an_, ar_;
  uint16_t nq_ = 0, nan_ = 0, nar_ = 0;
  bool additional_ = false;
  bool ok_ = true;
};

}  // namespace mdns

// The local-network interfaces Nearby may use (CONNECTIVITY §3.4): IPv4 addresses that are private
// (RFC 1918) or link-local (169.254/16) with their prefix, and IPv6 link-local addresses with their
// zone. Never loopback, never public or global addresses (so never a cellular uplink).
struct LocalNet {
  bool v6 = false;
  uint8_t addr[16] = {0};
  uint8_t mask[16] = {0};
  uint32_t scope = 0;    // IPv6 zone (interface index)
  std::string bind;      // "192.168.1.5" / "fe80::1%en0": usable for bind() and as a dial string
};
std::vector<LocalNet> local_networks();
// True if `ip` (numeric, IPv6 with or without "%zone") lies on one of `nets` (IPv4: inside an
// interface prefix; IPv6: fe80::/10, or a ULA inside an interface prefix).
bool on_local_network(const std::string& ip, const std::vector<LocalNet>& nets);
// Limit key for a source address: the IPv4 address itself, or the /64 prefix of an IPv6 address.
std::string source_key(const std::string& ip);

// A discovered EmberMesh instance with a valid TXT record and at least one address.
struct NearbyPeer {
  std::string instance;            // full instance name
  std::string token;               // from TXT `t` (8 lowercase hex)
  uint16_t port = 0;
  std::vector<std::string> addrs;  // dialable host strings: IPv6 link-local first ("fe80::1%en0"), then IPv4
};

class MdnsAgent {
 public:
  struct Options {
    std::string service = "_embermesh._tcp";  // tests isolate themselves with a private service type
    std::string token;                         // our instance token (8 lowercase hex)
    uint16_t port = 0;                         // our listener port
    bool ipv6 = true;
    // IPv4 addresses to advertise (the local-network interfaces; never all of them). IPv6: only
    // link-local addresses of the joined interfaces are advertised.
    std::vector<std::string> ipv4;
  };
  explicit MdnsAgent(Loop& loop);
  ~MdnsAgent();
  MdnsAgent(const MdnsAgent&) = delete;
  MdnsAgent& operator=(const MdnsAgent&) = delete;

  // Opens the sockets, announces and starts browsing. False (with *err) if no socket could be
  // bound (e.g. port 5353 held exclusively by another program, or no multicast).
  bool start(const Options& o, std::string* err);
  // Sends goodbyes (TTL 0) and closes everything. Idempotent.
  void stop();
  bool running() const { return running_; }

  std::function<void(const NearbyPeer&)> on_found;     // new peer, or its addresses/port changed
  std::function<void(const std::string& token)> on_lost;

  // Counters for status / tests.
  struct Stats {
    uint64_t rx_packets = 0, rx_bad = 0, rx_dropped_budget = 0, answers_sent = 0, queries_sent = 0;
  };
  const Stats& stats() const { return stats_; }
  std::vector<NearbyPeer> peers() const;
  std::string instance_name() const;

 private:
  struct Sock {
    sock_t fd = kInvalidSock;
    uint64_t watch = 0;
    bool v6 = false;
    std::vector<uint32_t> ifindex;     // v6: joined interfaces
    std::vector<std::string> ifaddr;   // v4: joined interface addresses
  };
  struct Instance {
    std::string name, token, target;
    uint16_t port = 0;
    bool have_srv = false, have_txt = false, valid_txt = false;
    uint64_t expires = 0;
    bool reported = false;
    std::string reported_key;
  };
  struct HostAddrs {
    std::vector<std::string> v4, v6;  // dialable strings
    uint64_t expires = 0;
  };
  bool open_v4(std::string* err);
  bool open_v6(std::string* err);
  void on_readable(Sock& s);
  void handle(const mdns::Message& m, const SockAddr& from, Sock& s);
  void answer(const mdns::Message& q, const SockAddr& from, Sock& s);
  std::string build_records(uint16_t id, bool response_to_legacy, bool goodbye, bool include_ptr,
                            bool include_service_enum);
  void send_all(const std::string& pkt);
  void send_query();
  void schedule_query(uint64_t delay);
  void tick();
  void report();
  std::string host_name() const { return "EmberMesh-" + opt_.token + ".local"; }  // never the system host name
  std::string service_name() const { return opt_.service + ".local"; }

  Loop& loop_;
  Options opt_;
  bool running_ = false;
  Sock v4_, v6_;
  uint64_t query_timer_ = 0, tick_timer_ = 0, announce_timer_ = 0;
  uint64_t query_delay_ = 1000;
  int announces_left_ = 0;
  std::map<std::string, Instance> instances_;  // lowercased instance name
  std::map<std::string, HostAddrs> hosts_;     // lowercased host name
  std::map<std::string, uint64_t> last_answer_;  // per question key: rate limit (1/s)
  uint64_t budget_window_ = 0;
  uint32_t budget_used_ = 0;
  std::map<std::string, uint32_t> per_source_;  // packets per source address in this second
  uint32_t overflow_source_ = 0;                // shared by sources beyond the table
  Stats stats_;
  std::shared_ptr<bool> alive_;
};

}  // namespace ember_host
