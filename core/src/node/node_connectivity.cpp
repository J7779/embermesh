// EmberMesh node — Nearby devices and the internet relay (docs/RPC_API.md §11,
// docs/CONNECTIVITY.md): connectivity.get / set, consent, the sessions of every `relay` link
// (contacts and private channels, each attached to the engine as its own scoped link), and the
// bookkeeping of automatic nearby links (duplicate removal).
#include <algorithm>
#include <cstdio>
#include <cstring>

#include "link/relay_link.h"
#include "node/node.h"
#include "node/node_util.h"
#include "proto/relay.h"
#include "proto/secure.h"

namespace ember::node {

namespace {

constexpr size_t kMaxRelays = 8;
constexpr size_t kMaxNearbyLinks = 16;
constexpr uint64_t kConnectivityTickMs = 5000;
constexpr uint64_t kMinSaneUnixMs = 1704067200000ull;  // 2024-01-01

const char* const kNearbyNotice =
    "Nearby devices: find other EmberMesh users on this Wi-Fi or local network and connect automatically. "
    "Devices on the same network can see your name and your EmberMesh identity.";

// The internet relay notice names the relays actually enabled (CONNECTIVITY §4.10); every word is true.
std::string internet_notice(const Json& relays) {
  std::vector<std::string> hosts;
  if (relays.is_array())
    for (const Json& r : relays.items())
      if (r.get_bool("enabled", true)) {
        std::string h = proto::relay::canonical_host(r.get_string("url"));
        hosts.push_back(h.substr(0, h.rfind(':')));
      }
  std::string list;
  for (size_t i = 0; i < hosts.size(); ++i) list += (i ? ", " : "") + hosts[i];
  std::string n = "Also send messages to your contacts over the internet when they cannot be reached nearby. They go through ";
  n += hosts.size() == 1 ? std::string("1 relay server") : std::to_string(hosts.size()) + " relay servers";
  n += " run by others (" + (list.empty() ? std::string("none enabled") : list) +
       ") — free services with no guarantee unless you added your own. The relays, and anyone watching them, cannot "
       "read your direct messages. The relays can see your IP address (and so roughly where you are), when you are "
       "online, roughly how many contacts you have and which other users you exchange messages with, and can follow "
       "this across days and networks. Your contacts can see when you come online. Private channels go over the relay "
       "only if you turn it on for a channel with a strong key.";
  if (hosts.size() == 1)
    n += " With one relay, reaching your contacts over the internet depends on that one free service; your own hub "
         "is the robust option, and more relays can be added under Settings > Connections.";
  return n;
}

std::string hex_id(const uint8_t* p, size_t n) {
  static const char kHex[] = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < n; ++i) { s.push_back(kHex[p[i] >> 4]); s.push_back(kHex[p[i] & 15]); }
  return s;
}

// wss://host[:port][/path] (or ws:// when plaintext is allowed). Host: letters, digits, '.', '-',
// or a bracketed IPv6 literal; no user info, query or fragment.
bool valid_relay_url(const std::string& u, bool allow_plain, std::string* why) {
  auto fail = [&](const char* m) { if (why) *why = m; return false; };
  if (u.size() > 256) return fail("relay address too long (256)");
  std::string rest;
  if (u.rfind("wss://", 0) == 0) rest = u.substr(6);
  else if (u.rfind("ws://", 0) == 0) {
    if (!allow_plain) return fail("relay addresses must start with wss:// (plain ws:// needs allow_plaintext)");
    rest = u.substr(5);
  } else return fail("relay addresses must start with wss://");
  size_t slash = rest.find('/');
  std::string hostport = rest.substr(0, slash);
  std::string path = slash == std::string::npos ? "" : rest.substr(slash);
  if (hostport.empty()) return fail("relay address has no host");
  for (char c : path)
    if (c < 0x21 || c > 0x7e || c == '?' || c == '#') return fail("relay path: printable characters, no query");
  std::string host = hostport, port;
  if (hostport[0] == '[') {
    size_t e = hostport.find(']');
    if (e == std::string::npos) return fail("bad IPv6 literal");
    host = hostport.substr(1, e - 1);
    if (e + 1 < hostport.size()) {
      if (hostport[e + 1] != ':') return fail("bad host");
      port = hostport.substr(e + 2);
    }
    for (char c : host)
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == ':')) return fail("bad IPv6 literal");
  } else {
    size_t colon = hostport.rfind(':');
    if (colon != std::string::npos) { host = hostport.substr(0, colon); port = hostport.substr(colon + 1); }
    for (char c : host)
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-'))
        return fail("relay host: letters, digits, dots and hyphens");
  }
  if (host.empty()) return fail("relay address has no host");
  if (!port.empty()) {
    if (port.size() > 5) return fail("bad port");
    int v = 0;
    for (char c : port) {
      if (c < '0' || c > '9') return fail("bad port");
      v = v * 10 + (c - '0');
    }
    if (v < 1 || v > 65535) return fail("bad port");
  }
  return true;
}

// Plain ws:// is allowed only to a LAN, link-local or .local host (CONNECTIVITY §4.11).
bool lan_host(const std::string& url) {
  std::string h = proto::relay::canonical_host(url);
  if (h.empty()) return false;
  h = h.substr(0, h.rfind(':'));
  auto ends = [&](const char* suf) {
    size_t n = std::strlen(suf);
    return h.size() > n && h.compare(h.size() - n, n, suf) == 0;
  };
  if (ends(".local")) return true;
  if (h.find(':') != std::string::npos) {  // IPv6: link-local fe80::/10 or ULA fc00::/7
    return h.rfind("fe8", 0) == 0 || h.rfind("fe9", 0) == 0 || h.rfind("fea", 0) == 0 || h.rfind("feb", 0) == 0 ||
           h.rfind("fc", 0) == 0 || h.rfind("fd", 0) == 0;
  }
  unsigned a = 0, b = 0, c = 0, d = 0;
  char tail = 0;
  if (std::sscanf(h.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4 || a > 255 || b > 255 || c > 255 || d > 255)
    return false;
  return a == 10 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168) || (a == 169 && b == 254);
}

}  // namespace

// Defaults (verified 2026-11-14, CONNECTIVITY §4.1): only a broker that refuses both `#` and `+`
// subscriptions is enabled; brokers that let anyone watch topics are listed but off.
const char* const kWildcardNote =
    "Off by default: this public broker lets anyone subscribe to wildcards, so strangers can watch topic "
    "names, sizes and timing (never content).";

Json default_relays() {
  return Json::array({Json::object({{"url", "wss://broker.hivemq.com:8884/mqtt"}, {"enabled", true}}),
                      Json::object({{"url", "wss://broker.emqx.io:8084/mqtt"}, {"enabled", false}}),
                      Json::object({{"url", "wss://test.mosquitto.org:8081/mqtt"}, {"enabled", false}})});
}

static const char* relay_note(const std::string& url) {
  if (url == "wss://broker.emqx.io:8084/mqtt" || url == "wss://test.mosquitto.org:8081/mqtt") return kWildcardNote;
  if (url == "wss://broker.hivemq.com:8884/mqtt")
    return "Free public test service with no guarantee; refuses wildcard subscriptions (checked 2026-11-14).";
  return nullptr;
}

// Relays without secrets (passwords are never returned).
static Json public_relays(const Json& relays) {
  Json out = Json::array();
  if (!relays.is_array()) return out;
  for (const Json& r : relays.items()) {
    Json x = Json::object({{"url", r.get_string("url")}, {"enabled", r.get_bool("enabled", true)}});
    if (r.get_bool("allow_plaintext")) x.set("allow_plaintext", true);
    if (!r.get_string("username").empty()) x.set("username", r.get_string("username"));
    if (const char* n = relay_note(r.get_string("url"))) x.set("note", n);
    out.push(std::move(x));
  }
  return out;
}

Json strip_connectivity_secrets(const Json& settings) {
  Json s = settings;
  if (s.has("connectivity.relays")) s.set("connectivity.relays", public_relays(s.get("connectivity.relays")));
  return s;
}

bool Node::connectivity_ready() const {
  return settings_.get_bool("connectivity.setup_done") && !locked() && id_.valid();
}

// Lock, wipe, duress: every automatic link (nearby and relay) goes before anything else happens.
void Node::connectivity_shutdown(const char* reason) {
  std::vector<uint32_t> ids;
  for (auto& kv : links_)
    if (!kv.second.auto_kind.empty() || kv.second.type == "relay") ids.push_back(kv.first);
  for (uint32_t id : ids) remove_link_internal(id, reason);
}

Json Node::connectivity_json() {
  if (nearby_token_.empty()) {
    uint8_t r[4];
    rng_.fill(r, sizeof r);
    nearby_token_ = hex_id(r, 4);
  }
  uint64_t nearby_up = 0;
  for (auto& kv : links_)
    if (kv.second.auto_kind == "nearby" && kv.second.link && kv.second.link->state() == LinkState::Up) ++nearby_up;
  bool ready = connectivity_ready();
  bool internet_on = settings_.get_bool("connectivity.internet");
  Json relays = public_relays(settings_.get("connectivity.relays"));
  Json out_relays = Json::array();
  for (const Json& r : relays.items()) {
    Json x = r;
    x.set("state", internet_on && r.get_bool("enabled", true) ? "down" : "off");
    for (auto& kv : links_) {
      if (kv.second.auto_kind != "relay" || kv.second.auto_key != r.get_string("url")) continue;
      x.set("link_id", kv.first);
      LinkState s = kv.second.link ? kv.second.link->state() : LinkState::Error;
      x.set("state", s == LinkState::Up ? "up" : s == LinkState::Connecting ? "connecting" : s == LinkState::Error ? "error" : "down");
    }
    out_relays.push(std::move(x));
  }
  Json internet = Json::object({{"enabled", internet_on},
                                {"active", internet_on && ready},
                                {"notice", internet_notice(settings_.get("connectivity.relays"))},
                                {"relays", std::move(out_relays)}});
  if (settings_.has("connectivity.internet_consent_ts"))
    internet.set("consent_ts", settings_.get_int("connectivity.internet_consent_ts"));
  bool nearby_on = settings_.get_bool("connectivity.nearby", true);
  return Json::object({{"ready", ready},
                       {"setup_done", settings_.get_bool("connectivity.setup_done")},
                       {"nearby", Json::object({{"enabled", nearby_on},
                                                {"active", nearby_on && ready},
                                                {"token", nearby_token_},
                                                {"notice", kNearbyNotice},
                                                {"links", nearby_up}})},
                       {"internet", std::move(internet)},
                       {"available_link_types", Json::array({"stream", "relay"})}});
}

RpcResult Node::m_connectivity_get(Params&) { return rpc_ok(connectivity_json()); }

RpcResult Node::m_connectivity_network_changed(Params&) {
  uint8_t r[4];
  rng_.fill(r, sizeof r);
  nearby_token_ = hex_id(r, 4);
  Json c = connectivity_json();
  last_connectivity_dump_ = c.dump();
  emit("connectivity.changed", c);
  return rpc_ok(Json::object({{"token", nearby_token_}}));
}

RpcResult Node::m_connectivity_set(Params& p) {
  bool nearby = settings_.get_bool("connectivity.nearby", true), internet = settings_.get_bool("connectivity.internet");
  bool setup = settings_.get_bool("connectivity.setup_done");
  bool hn = false, hi = false, hs = false;
  const Json* relays = nullptr;
  if (!p.boolean("nearby", &nearby, &hn) || !p.boolean("internet", &internet, &hi) ||
      !p.boolean("setup_done", &setup, &hs) || !p.array("relays", &relays, false))
    return p.fail();
  if (hs && !setup) return rpc_err("bad_params", "setup_done can only be set to true");
  Json next = settings_;
  if (relays) {
    if (relays->size() > kMaxRelays) return rpc_err("bad_params", "at most 8 relays");
    Json list = Json::array();
    const Json& old = settings_.get("connectivity.relays");
    for (const Json& r : relays->items()) {
      if (!r.is_object()) return rpc_err("bad_params", "relays: objects {url, enabled?, allow_plaintext?, username?, password?}");
      std::string url = r.get_string("url"), why;
      bool plain = r.get_bool("allow_plaintext");
      if (!valid_relay_url(url, plain, &why)) return rpc_err("bad_params", "relays: " + why);
      if (url.rfind("ws://", 0) == 0 && !lan_host(url))
        return rpc_err("bad_params", "relays: plain ws:// only for a LAN, link-local or .local address");
      const Json* en = r.find("enabled");
      if (en && !en->is_bool()) return rpc_err("bad_params", "relays: enabled must be true or false");
      std::string user = r.get_string("username"), pass = r.get_string("password");
      if (user.size() > 128 || pass.size() > 128 || !utf8_valid(user)) return rpc_err("bad_params", "relays: user name / password too long");
      if (!r.has("password") && old.is_array())
        for (const Json& o : old.items())
          if (o.get_string("url") == url && o.get_string("username") == user) pass = o.get_string("password");
      for (const Json& x : list.items())
        if (x.get_string("url") == url) return rpc_err("bad_params", "relays: duplicate address");
      Json e = Json::object({{"url", url}, {"enabled", en ? en->as_bool() : true}});
      if (plain) e.set("allow_plaintext", true);
      if (!user.empty()) e.set("username", user);
      if (!pass.empty()) e.set("password", pass);
      list.push(std::move(e));
    }
    next.set("connectivity.relays", std::move(list));
  }
  if (hn) next.set("connectivity.nearby", nearby);
  if (hs) next.set("connectivity.setup_done", true);
  if (hi) {
    next.set("connectivity.internet", internet);
    if (internet && !settings_.get_bool("connectivity.internet")) next.set("connectivity.internet_consent_ts", static_cast<int64_t>(mesh_now()));
  }
  settings_ = next;
  if (!db_put_json("meta", B("settings"), settings_)) return rpc_err("io_error", "settings could not be saved");
  if (!settings_.get_bool("connectivity.nearby", true)) {
    std::vector<uint32_t> ids;
    for (auto& kv : links_)
      if (kv.second.auto_kind == "nearby") ids.push_back(kv.first);
    for (uint32_t id : ids) remove_link_internal(id, "nearby_off");
  }
  if (!settings_.get_bool("connectivity.internet")) {
    std::vector<uint32_t> ids;
    for (auto& kv : links_)
      if (kv.second.type == "relay") ids.push_back(kv.first);
    for (uint32_t id : ids) remove_link_internal(id, "relay_off");
  } else if (relays) {
    // Relay links whose address is no longer in the enabled list go away.
    std::vector<uint32_t> ids;
    for (auto& kv : links_) {
      if (kv.second.auto_kind != "relay") continue;
      bool keep = false;
      for (const Json& r : settings_.get("connectivity.relays").items())
        keep = keep || (r.get_string("url") == kv.second.auto_key && r.get_bool("enabled", true));
      if (!keep) ids.push_back(kv.first);
    }
    for (uint32_t id : ids) remove_link_internal(id, "relay_removed");
  }
  connectivity_tick(true);
  Json c = connectivity_json();
  last_connectivity_dump_ = c.dump();
  emit("connectivity.changed", c);
  return rpc_ok(c);
}

bool Node::check_relay_endpoint(const Json& ep, RpcResult* err) {
  // Relay-ness is decided here, not trusted from the host (review M9).
  const Json& c = ep.get("config");
  if (c.get_bool("insecure")) { *err = rpc_err("denied", "relay links never skip certificate checks (insecure)"); return false; }
  std::string url = c.get_string("url");
  if (url.rfind("ws://", 0) == 0 && !(c.get_bool("allow_plaintext") && lan_host(url))) {
    *err = rpc_err("denied", "relay links use wss:// (plain ws:// only with allow_plaintext to a LAN address)");
    return false;
  }
  return true;
}

bool Node::check_auto_link(const std::string& kind, const std::string& role, const std::string& key,
                           const std::string& type, RpcResult* err) {
  if (type == "relay" && !(settings_.get_bool("connectivity.internet") && connectivity_ready())) {
    *err = rpc_err("denied", "the internet relay is off (connectivity.internet): nothing is sent before consent");
    return false;
  }
  if (kind.empty()) return true;
  if (kind != "nearby" && kind != "relay") { *err = rpc_err("bad_params", "auto must be \"nearby\" or \"relay\""); return false; }
  if (kind == "relay" && type != "relay") { *err = rpc_err("bad_params", "auto:\"relay\" needs link type relay"); return false; }
  if (kind == "nearby") {
    if (!settings_.get_bool("connectivity.nearby", true) || !connectivity_ready()) {
      *err = rpc_err("denied", "Nearby devices is off or setup is not complete");
      return false;
    }
    if (role != "dial" && role != "accept") { *err = rpc_err("bad_params", "auto_role must be \"dial\" or \"accept\""); return false; }
    size_t n = 0;
    uint32_t victim = 0;
    for (auto& kv : links_) {
      if (kv.second.auto_kind != "nearby") continue;
      ++n;
      if (kv.second.auto_neighbour.empty() && (!victim || kv.first < victim)) victim = kv.first;  // oldest unproven
    }
    if (n >= kMaxNearbyLinks) {
      if (!victim) { *err = rpc_err("busy", "16 nearby devices with proven identities are already linked"); return false; }
      remove_link_internal(victim, "evicted");
    }
  }
  if (!key.empty())
    for (auto& kv : links_) {
      const LinkRec& r = kv.second;
      if (r.auto_kind != kind || r.auto_key != key) continue;
      if (kind == "nearby" && role != "dial") continue;
      if (r.link && r.link->state() != LinkState::Down) { *err = rpc_err("busy", "an automatic link with this key exists"); return false; }
    }
  return true;
}

uint32_t Node::relay_child_id(const Link* l) const {
  for (const auto& kv : relay_children_)
    if (kv.second.link == l) return kv.first;
  return 0;
}

void Node::relay_detach_children(uint32_t parent) {
  for (auto it = relay_children_.begin(); it != relay_children_.end();) {
    if (it->second.parent == parent) {
      if (eng_) eng_->detach_link(it->first);
      it = relay_children_.erase(it);
    } else {
      ++it;
    }
  }
}

void Node::relay_sync(LinkRec& r) {
  link::RelayControl* rc = link::as_relay(r.link.get());
  if (!rc) return;
  bool consent = settings_.get_bool("connectivity.internet") && connectivity_ready();
  rc->set_consent(consent);
  NodeId mine = id_.node_id();
  rc->set_own_hop(mine.data());
  uint64_t wall = mesh_now();
  rc->set_wall_clock(have_time_ && wall >= kMinSaneUnixMs ? wall : 0);
  if (!eng_) return;
  // Sessions: every contact whose keys we hold (not blocked), every private channel.
  std::vector<link::RelaySession> want;
  if (consent) {
    std::map<NodeId, PublicKeys> keys;
    for (const mesh::PeerInfo& pi : eng_->peers())
      if (pi.has_keys) keys[pi.id] = pi.keys;
    for (const auto& kv : contacts_) {
      if (kv.second.get_bool("blocked")) continue;
      auto k = keys.find(kv.first);
      if (k == keys.end()) continue;
      auto ci = relay_root_cache_.find(kv.first);
      if (ci == relay_root_cache_.end() || ci->second.first != k->second.dh_pk) {
        crypto::Key32 kp;
        if (!proto::pair_key(id_, k->second.dh_pk, kp)) continue;
        relay_root_cache_[kv.first] = {k->second.dh_pk, proto::relay::pair_root(kp)};
        crypto::wipe(kp);
        ci = relay_root_cache_.find(kv.first);
      }
      link::RelaySession s;
      s.key = "c:" + hex_id(kv.first.data(), kv.first.size());
      s.root = ci->second.second;
      std::copy(kv.first.begin(), kv.first.begin() + 4, s.peer_hop);
      want.push_back(s);
    }
    for (const auto& kv : channels_) {
      // Only channels the user opted in, and only random-key or strong-passphrase ones (review H3).
      if (!kv.second.key.encrypted || !kv.second.rec.get_bool("relay") || !kv.second.rec.get_bool("relay_allowed")) continue;
      link::RelaySession s;
      char id[16];
      std::snprintf(id, sizeof id, "%08x", static_cast<unsigned>(kv.second.key.id));
      s.key = std::string("ch:") + id;
      s.channel = true;
      s.root = proto::relay::channel_root(kv.second.key.key);
      want.push_back(s);
    }
  }
  // Detach children whose session goes away or changes, before the link destroys them.
  for (auto it = relay_children_.begin(); it != relay_children_.end();) {
    if (it->second.parent != r.id) { ++it; continue; }
    bool keep = false;
    for (size_t i = 0; i < rc->child_count(); ++i)
      if (rc->child(i) == it->second.link)
        for (const auto& w : want)
          keep = keep || (w.key == it->second.key && w.root == rc->child_session(i).root);
    if (!keep) {
      eng_->detach_link(it->first);
      it = relay_children_.erase(it);
    } else {
      ++it;
    }
  }
  rc->set_sessions(want);
  for (size_t i = 0; i < rc->child_count(); ++i) {
    Link* c = rc->child(i);
    uint32_t cid = relay_child_id(c);
    if (!cid) {
      cid = next_relay_child_id_++;
      if (next_relay_child_id_ < 0x40000000u) next_relay_child_id_ = 0x40000000u;
      eng_->attach_link(cid, c);
      relay_children_[cid] = RelayChildRec{r.id, rc->child_session(i).key, c};
    }
    const link::RelaySession& s = rc->child_session(i);
    mesh::LinkScope sc;
    {
      sc.restricted = true;
      sc.relay_session = true;
      if (s.channel) {
        sc.has_channel = true;
        sc.channel = static_cast<uint32_t>(std::strtoul(s.key.c_str() + 3, nullptr, 16));
      } else {
        sc.has_peer = true;
        for (const auto& kv : contacts_)
          if (std::equal(kv.first.begin(), kv.first.begin() + 4, s.peer_hop) && s.key == "c:" + hex_id(kv.first.data(), kv.first.size()))
            sc.peer = kv.first;
      }
    }
    eng_->set_link_scope(cid, sc);
  }
}

void Node::remove_link_internal(uint32_t id, const char* reason) {
  auto it = links_.find(id);
  if (it == links_.end()) return;
  relay_detach_children(id);
  if (eng_) eng_->detach_link(id);
  Json lj = link_json(it->second);
  lj.set("state", "down").set("removed", true);
  if (it->second.persist) {
    ByteWriter w(4);
    w.u32be(id);
    db_del("links", w.take());
  }
  links_.erase(it);
  emit("link.state", Json::object({{"link", std::move(lj)}}));
  emit("link.removed", Json::object({{"id", id}, {"reason", reason}}));
}

// Nearby links: names, and duplicates among links whose neighbour PROVED its identity (verified
// ANNOUNCE with hops 0 from that hop id, or a direct pairwise-authenticated packet; never a beacon
// claim, review H5). Among proven nearby links to one node keep the one dialed by the node with the
// smaller node id (both ends compute the same), or among links of different cost the cheaper one.
void Node::nearby_dedupe() {
  if (!eng_) return;
  std::map<NodeId, std::vector<uint32_t>> by_node;
  std::map<NodeId, std::string> names;
  for (const mesh::PeerInfo& pi : eng_->peers()) names[pi.id] = pi.name;
  for (auto& kv : links_) {
    LinkRec& r = kv.second;
    if (r.auto_kind != "nearby" || !r.link) continue;
    std::vector<NodeId> proven = eng_->proven_neighbours(kv.first);
    if (proven.size() != 1) {  // none yet, or a shared medium: not a single device
      if (r.auto_neighbour.empty()) r.name = "Nearby device";
      continue;
    }
    r.auto_neighbour = to_hex(ByteView(proven[0].data(), proven[0].size()));
    std::string nm = utf8_sanitize(std::string(utf8_truncate(names[proven[0]], 32)));
    std::string want = nm.empty() ? std::string("Nearby device") : "Nearby: " + nm;
    if (r.name != want) { r.name = want; emit_link_state(r); }
    if (r.link->state() == LinkState::Up) by_node[proven[0]].push_back(kv.first);
  }
  NodeId me = id_.node_id();
  for (auto& kv : by_node) {
    if (kv.second.size() < 2) continue;
    uint32_t keep = 0;
    uint8_t best_cost = 0;
    NodeId best{};
    bool have = false;
    for (uint32_t id : kv.second) {
      uint8_t cost = links_[id].link->caps().cost;
      const NodeId& dialer = links_[id].auto_role == "dial" ? me : kv.first;
      if (!have || cost < best_cost || (cost == best_cost && (dialer < best || (dialer == best && id < keep)))) {
        best_cost = cost;
        best = dialer;
        keep = id;
        have = true;
      }
    }
    for (uint32_t id : kv.second)
      if (id != keep) remove_link_internal(id, "duplicate");
  }
}

void Node::connectivity_tick(bool force) {
  if (!force && mono_ - last_connectivity_ms_ < kConnectivityTickMs) return;
  last_connectivity_ms_ = mono_;
  std::vector<uint32_t> relays;
  for (auto& kv : links_)
    if (kv.second.type == "relay") relays.push_back(kv.first);
  for (uint32_t id : relays) relay_sync(links_[id]);
  // Nearby links whose hello failed (CONNECTIVITY §3.3) go at once; the host closes the socket.
  std::vector<uint32_t> bad;
  for (auto& kv : links_)
    if (kv.second.auto_kind == "nearby" && kv.second.link && kv.second.link->state() == LinkState::Error) {
      std::optional<Json> st = Json::parse(kv.second.link->status_json());
      if (st && st->get_string("hello") == "failed") bad.push_back(kv.first);
    }
  for (uint32_t id : bad) remove_link_internal(id, "hello_failed");
  nearby_dedupe();
  Json c = connectivity_json();
  std::string d = c.dump();
  if (d != last_connectivity_dump_) {
    last_connectivity_dump_ = d;
    if (!force) emit("connectivity.changed", c);
  }
}

}  // namespace ember::node
