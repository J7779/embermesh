// SPDX-License-Identifier: MIT
// "Nearby devices" on the desktop host (docs/CONNECTIVITY.md §3, RPC_API.md §11 "Host side"),
// active only while the core reports `nearby.active`:
//   * TCP listeners bound to the local-network interface addresses only (private IPv4, IPv6
//     link-local; one port for all, 47475 preferred) — never all interfaces;
//   * the mDNS responder/browser (mdns.h), advertising those addresses only;
//   * the dialing rule (smaller token dials; the other side dials too after 8 s without an inbound
//     connection) and Happy Eyeballs (IPv6 link-local first, then IPv4, 250 ms apart);
//   * normative limits: dial only on-link addresses, at most 4 per instance, at most 4 new dials
//     per minute; accept only on-link sources, at most 2 connections per source IPv4 address or
//     IPv6 /64 and 4 new per minute per source; at most 16 nearby links;
//   * network changes (interface addresses differ): `connectivity.network_changed` gives a new
//     token and everything restarts with it.
// Every connection becomes `links.add {type:"stream", config:{preset:"nearby"}, auto:"nearby",
// auto_role, auto_key}` (`"<token>@<address>"` dialing, `"@<address>"` accepting); the core sends and
// checks the 8-byte hello. A nearby link whose connection ends is removed.
#pragma once

#include <deque>
#include <map>
#include <memory>
#include <string>

#include "host/endpoint.h"
#include "host/json.h"
#include "host/mdns.h"

namespace ember_host {

// What Nearby needs from the Host (implemented by Host).
class NearbyHostApi {
 public:
  virtual ~NearbyHostApi() = default;
  // links.add with these params, then runs the endpoint under the new link. Returns the link id
  // (0 if the core refused; the endpoint is then destroyed).
  virtual uint32_t nearby_install(std::unique_ptr<Endpoint> ep, const Json& add_params) = 0;
  // links.remove + closes the endpoint (deferred: safe from endpoint callbacks).
  virtual void nearby_remove(uint32_t id) = 0;
  // connectivity.network_changed: the new token ("" if the core refused).
  virtual std::string nearby_network_changed() = 0;
};

class Nearby {
 public:
  Nearby(Loop& loop, EndpointSink& sink, NearbyHostApi& host, std::string service);
  ~Nearby();
  Nearby(const Nearby&) = delete;
  Nearby& operator=(const Nearby&) = delete;

  // Follows connectivity.nearby: starts (or restarts with a new token) / stops everything.
  void set_enabled(bool on, const std::string& token);
  bool enabled() const { return on_; }
  // The host reports that one of our links is gone (connection closed, removed, duplicate).
  void link_gone(uint32_t id);
  bool owns(uint32_t id) const { return links_.count(id) != 0; }
  Json status() const;

 private:
  struct LinkInfo {
    std::string token;  // peer token (dial side) or "" (accept side)
    std::string ip;
    std::string src;    // limit key: IPv4 address or IPv6 /64
    bool dial = false;
  };
  struct Peer {
    NearbyPeer p;
    uint64_t found_ms = 0;
    uint64_t wait_timer = 0;
  };
  bool open_listener(std::string* err);
  void watch_network();
  std::string net_signature() const;
  void close_all();
  void on_accept(sock_t listener);
  void on_found(const NearbyPeer& p);
  void on_lost(const std::string& token);
  void consider(const std::string& token, bool waited);
  void dial(const Peer& p);
  bool linked_to(const Peer& p) const;
  bool rate_ok(std::deque<uint64_t>& q, size_t max_per_minute);

  Loop& loop_;
  EndpointSink& sink_;
  NearbyHostApi& host_;
  std::string service_;
  bool on_ = false;
  std::string token_;
  struct Listener {
    sock_t fd;
    uint64_t watch;
  };
  std::vector<Listener> listeners_;
  std::vector<LocalNet> nets_;
  std::string net_sig_;
  uint64_t net_timer_ = 0;
  uint16_t port_ = 0;
  MdnsAgent mdns_;
  std::string error_;
  std::map<std::string, Peer> peers_;  // by token
  std::map<uint32_t, LinkInfo> links_;
  std::deque<uint64_t> dials_;
  std::map<std::string, std::deque<uint64_t>> accepts_;  // per remote address
  uint64_t refused_ = 0;
  std::shared_ptr<bool> alive_;
};

}  // namespace ember_host
