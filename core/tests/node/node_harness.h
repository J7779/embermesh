// Test harness for the node / C API suites: drives nodes ONLY through ember.h and shuttles
// EMBER_OUT_LINK_TX bytes between them over a small simulated network (broadcast "media" with
// per-medium loss, delay, partitions and an optional tx hook), in virtual time.
#pragma once

#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/json.h"
#include "doctest.h"
#include "ember/ember.h"

namespace nt {

using ember::Json;

constexpr uint64_t kStart = 1767225600000ull;  // 2026-01-01

struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed ? seed : 0x9e3779b97f4a7c15ull) {}
  uint64_t next() {
    uint64_t z = (s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }
  double unit() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
  uint32_t below(uint32_t n) { return n ? static_cast<uint32_t>(next() % n) : 0; }
};

inline Json J(const char* text) {
  std::optional<Json> j = Json::parse(text);
  REQUIRE_MESSAGE(j.has_value(), "bad JSON in test: " << text);
  return *j;
}

// Directory for on-disk stores: inside this build tree, never anywhere else.
inline std::string temp_dir(const std::string& name) {
  const char* env = std::getenv("EMBER_TEST_TMP");
  std::string base = env && *env ? env : std::string(EMBER_REPO_ROOT) + "/build/node/test-tmp";
  std::string d = base + "/" + name;
  std::error_code ec;
  std::filesystem::remove_all(d, ec);
  std::filesystem::create_directories(d, ec);
  return d;
}

struct Ev {
  std::string name;
  Json data;
};

class TNode {
 public:
  explicit TNode(const std::string& storage_dir = "", uint64_t seed = 1, uint64_t now = kStart,
                 const std::string& extra_cfg = "") {
    seed_ = seed;
    std::string cfg = "{\"storage_dir\":" + Json::quote(storage_dir) + ",\"platform\":\"desktop\",\"now_ms\":" +
                      std::to_string(now) + ",\"kdf_mem_kib\":64,\"kdf_passes\":1" + extra_cfg + "}";
    uint8_t ent[48];
    Rng r(seed * 7919 + 17);
    for (auto& b : ent) b = static_cast<uint8_t>(r.next());
    n = ember_create(cfg.c_str(), ent, sizeof ent);
    REQUIRE(n != nullptr);
    drain();
    Json info = call("node.info");
    if (info.get_bool("ok")) id = info.get("result").get_string("node_id");
  }
  ~TNode() {
    if (n) ember_destroy(n);
  }
  TNode(const TNode&) = delete;
  TNode& operator=(const TNode&) = delete;

  Json call(const std::string& method, const Json& params = Json::object()) {
    std::string ps = params.dump();
    char* r = ember_call(n, method.c_str(), ps.c_str());
    REQUIRE(r != nullptr);
    std::optional<Json> j = Json::parse(r);
    ember_free(r);
    REQUIRE_MESSAGE(j.has_value(), "envelope is not JSON for " << method);
    REQUIRE(j->is_object());
    return *j;
  }
  // Calls and requires success; returns the result object.
  Json ok(const std::string& method, const Json& params = Json::object()) {
    Json e = call(method, params);
    INFO(method << " " << params.dump() << " -> " << e.dump());
    REQUIRE(e.get_bool("ok"));
    REQUIRE(e.get("result").is_object());
    return e.get("result");
  }
  // Copies the array result[key] (safe to iterate, unlike ok(...).get(...).items()).
  std::vector<Json> items(const std::string& method, const std::string& key, const Json& params = Json::object()) {
    Json r = ok(method, params);
    return r.get(key).items();
  }
  // Calls and requires failure; returns the error code.
  std::string err(const std::string& method, const Json& params = Json::object()) {
    Json e = call(method, params);
    INFO(method << " " << params.dump() << " -> " << e.dump());
    CHECK_FALSE(e.get_bool("ok"));
    CHECK(e.get("error").get("code").is_string());
    CHECK(e.get("error").get("message").is_string());
    return e.get("error").get_string("code");
  }

  // Drains the outbox: events are recorded, link bytes handed to `tx`.
  void drain(const std::function<void(uint32_t, const std::string&)>& tx = nullptr) {
    ember_out o;
    while (ember_poll(n, &o)) {
      if (o.kind == EMBER_OUT_EVENT) {
        REQUIRE(o.data != nullptr);
        REQUIRE(o.data[o.len] == 0);
        std::optional<Json> j = Json::parse(std::string_view(reinterpret_cast<const char*>(o.data), o.len));
        REQUIRE(j.has_value());
        REQUIRE(j->get("event").is_string());
        REQUIRE(j->get("data").is_object());
        events.push_back({j->get_string("event"), j->get("data")});
        if (events.size() > 20000) events.erase(events.begin(), events.begin() + 10000);
      } else {
        REQUIRE(o.kind == EMBER_OUT_LINK_TX);
        tx_items++;
        if (tx) tx(o.link_id, std::string(reinterpret_cast<const char*>(o.data), o.len));
      }
    }
  }
  size_t count(const std::string& ev) const {
    size_t c = 0;
    for (auto& e : events) c += e.name == ev;
    return c;
  }
  const Ev* last(const std::string& ev) const {
    for (auto it = events.rbegin(); it != events.rend(); ++it)
      if (it->name == ev) return &*it;
    return nullptr;
  }
  bool saw(const std::string& ev, const std::function<bool(const Json&)>& pred = nullptr) const {
    for (auto& e : events)
      if (e.name == ev && (!pred || pred(e.data))) return true;
    return false;
  }
  // Texts of messages in a conversation (oldest first).
  std::vector<Json> messages(const std::string& conv, int limit = 500) {
    Json r = ok("messages.list", Json::object({{"conv", conv}, {"limit", limit}}));
    std::vector<Json> v;
    for (const Json& m : r.get("messages").items()) v.push_back(m);
    return v;
  }
  bool has_text(const std::string& conv, const std::string& text) {
    Json r = call("messages.list", Json::object({{"conv", conv}, {"limit", 1000}}));
    if (!r.get_bool("ok")) return false;
    for (const Json& m : r.get("result").get("messages").items())
      if (m.get_string("text") == text) return true;
    return false;
  }
  Json find_message(const std::string& conv, const std::string& id) {
    for (auto& m : messages(conv, 1000))
      if (m.get_string("id") == id) return m;
    return Json();
  }
  std::string card() { return ok("identity.card").get_string("card"); }

  ember_node* n = nullptr;
  std::string id;
  std::vector<Ev> events;
  uint64_t tx_items = 0;
  uint64_t seed_ = 0;
};

// Everyone on one medium hears everyone else (a radio channel, a LAN, a hub bus).
class Net {
 public:
  struct Port {
    TNode* node;
    uint32_t link;
    int medium;
  };
  struct Medium {
    double loss = 0;
    uint32_t delay_ms = 5;
    bool stream = false;  // byte-stream links: never drop (framing would desync)
  };

  explicit Net(uint64_t seed = 1) : rng_(seed) {}

  int add_medium(double loss = 0, uint32_t delay_ms = 5, bool stream = false) {
    Medium m;
    m.loss = loss;
    m.delay_ms = delay_ms;
    m.stream = stream;
    media.push_back(m);
    return static_cast<int>(media.size() - 1);
  }
  void add(TNode& n) {
    for (TNode* x : nodes)
      if (x == &n) return;
    nodes.push_back(&n);
  }
  // Adds a link of `type` on node n, opens its endpoint and joins it to `medium`.
  uint32_t join(TNode& n, int medium, const std::string& type = "raw", const Json& config = Json::object(),
                bool persist = true) {
    add(n);
    Json r = n.ok("links.add", Json::object({{"type", type},
                                             {"name", type + " test"},
                                             {"config", config},
                                             {"endpoint", Json::object({{"kind", "loopback"}})},
                                             {"persist", persist}}));
    uint32_t id = static_cast<uint32_t>(r.get("link").get_int("id"));
    REQUIRE(id > 0);
    n.ok("links.endpoint_state", Json::object({{"id", id}, {"connected", true}}));
    ports.push_back({&n, id, medium});
    return id;
  }
  // Re-attaches an existing link id (after a node restart) to a medium.
  void attach(TNode& n, uint32_t link, int medium) {
    add(n);
    for (auto& p : ports)
      if (p.node == &n && p.link == link) {
        p.medium = medium;
        return;
      }
    ports.push_back({&n, link, medium});
  }
  void forget(TNode& n) {
    for (size_t i = 0; i < nodes.size(); ++i)
      if (nodes[i] == &n) nodes.erase(nodes.begin() + static_cast<long>(i));
    std::vector<Port> keep;
    for (auto& p : ports)
      if (p.node != &n) keep.push_back(p);
    ports = keep;
    std::deque<Pending> q2;
    for (auto& x : q_)
      if (x.to != &n) q2.push_back(x);
    q_ = q2;
  }
  // Directional connectivity between two nodes (default: connected).
  void cut(TNode& a, TNode& b, bool cut_it = true) {
    if (cut_it) {
      blocked_.insert({&a, &b});
      blocked_.insert({&b, &a});
    } else {
      blocked_.erase({&a, &b});
      blocked_.erase({&b, &a});
    }
  }
  void isolate(TNode& a, bool on = true) {
    if (on)
      isolated_.insert(&a);
    else
      isolated_.erase(&a);
  }

  void step(uint32_t dt = 20) {
    now += dt;
    std::deque<Pending> later;
    std::vector<Pending> due;
    for (auto& x : q_) {
      if (x.at <= now)
        due.push_back(x);
      else
        later.push_back(x);
    }
    q_ = later;
    for (auto& d : due) {
      bool alive = false;
      for (TNode* x : nodes) alive = alive || x == d.to;
      if (alive) ember_link_rx(d.to->n, d.link, reinterpret_cast<const uint8_t*>(d.bytes.data()), d.bytes.size());
    }
    for (TNode* x : nodes) ember_tick(x->n, now);
    for (TNode* x : nodes) {
      x->drain([&](uint32_t link, const std::string& bytes) { route(*x, link, bytes); });
    }
  }
  void run(uint64_t ms, uint32_t dt = 20) {
    for (uint64_t t = 0; t < ms; t += dt) step(dt);
  }
  bool run_until(const std::function<bool()>& pred, uint64_t max_ms, uint32_t dt = 20) {
    for (uint64_t t = 0; t <= max_ms; t += dt) {
      if (pred()) return true;
      step(dt);
    }
    return pred();
  }

  std::vector<TNode*> nodes;
  std::vector<Port> ports;
  std::vector<Medium> media;
  uint64_t now = kStart;
  uint64_t delivered = 0, dropped = 0;
  // Observes every transmission (port it left from, bytes).
  std::function<void(const Port&, const std::string&)> tx_hook;

 private:
  struct Pending {
    uint64_t at;
    TNode* to;
    uint32_t link;
    std::string bytes;
  };
  void route(TNode& from, uint32_t link, const std::string& bytes) {
    const Port* src = nullptr;
    for (auto& p : ports)
      if (p.node == &from && p.link == link) src = &p;
    if (!src) return;
    if (tx_hook) tx_hook(*src, bytes);
    if (isolated_.count(&from)) return;
    const Medium& m = media[static_cast<size_t>(src->medium)];
    for (auto& p : ports) {
      if (p.medium != src->medium || p.node == &from) continue;
      if (blocked_.count({&from, p.node}) || isolated_.count(p.node)) continue;
      if (!m.stream && m.loss > 0 && rng_.unit() < m.loss) {
        ++dropped;
        continue;
      }
      ++delivered;
      q_.push_back({now + m.delay_ms, p.node, p.link, bytes});
    }
  }

  Rng rng_;
  std::deque<Pending> q_;
  std::set<std::pair<TNode*, TNode*>> blocked_;
  std::set<TNode*> isolated_;
};

// Makes a and b contacts of each other (as if they scanned each other's QR cards).
inline void introduce(TNode& a, TNode& b) {
  a.ok("contacts.add", Json::object({{"card", b.card()}}));
  b.ok("contacts.add", Json::object({{"card", a.card()}}));
}

}  // namespace nt
