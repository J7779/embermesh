// Nearby devices and the internet relay at the node level (docs/CONNECTIVITY.md, RPC_API.md §11),
// driven only through ember.h: consent and readiness gating, channel opt-in (H3), endpoint refusal
// (M9), lock / wipe / duress (M15), secrets never returned, and a direct message between two
// contacts through a hostile in-test MQTT broker that drops, reorders, replays, injects and echoes.
#include <map>
#include <set>

#include "framing/mqtt.h"
#include "node_harness.h"

using namespace nt;
using ember::Bytes;
using ember::ByteView;
namespace mqtt = ember::mqtt;

namespace {

const char* kRelayUrl = "wss://broker.example.org:8084/mqtt";

void consent(TNode& n, bool internet = true) {
  n.ok("connectivity.set", Json::object({{"setup_done", true}, {"internet", internet}}));
}

Json relay_link_params(const std::string& url = kRelayUrl) {
  return Json::object({{"type", "relay"},
                       {"config", Json::object({{"url", url}})},
                       {"endpoint", Json::object({{"kind", "loopback"}})},
                       {"persist", false},
                       {"auto", "relay"},
                       {"auto_key", url}});
}

// An MQTT 3.1.1 broker, as hostile as asked: every subscriber gets every publish on its topics
// (the sender included), and with `hostile` set some are dropped, delayed out of order, replayed
// later, duplicated, and garbage is injected on live topics.
class Broker {
 public:
  struct Conn {
    TNode* node;
    uint32_t link;
    mqtt::Decoder dec{1 << 16};
    std::set<std::string> subs;
  };
  struct Pending {
    uint64_t at;
    Conn* to;
    Bytes bytes;
  };

  explicit Broker(uint64_t seed) : rng_(seed) {}
  bool hostile = false;
  uint64_t now = kStart;
  uint64_t published = 0, dropped = 0, replayed = 0, injected = 0;
  std::vector<std::string> topics_seen;
  std::vector<std::string> client_ids;

  Conn& conn(TNode& n, uint32_t link) {
    for (auto& c : conns_)
      if (c->node == &n && c->link == link) return *c;
    conns_.push_back(std::make_unique<Conn>());
    conns_.back()->node = &n;
    conns_.back()->link = link;
    return *conns_.back();
  }

  void from_client(TNode& n, uint32_t link, const std::string& bytes) {
    Conn& c = conn(n, link);
    c.dec.feed(ByteView(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()));
    mqtt::Packet p;
    while (c.dec.next(&p)) handle(c, p);
    REQUIRE_FALSE(c.dec.failed());
  }

  void step() {
    std::vector<Pending> due, later;
    for (auto& x : q_) (x.at <= now ? due : later).push_back(x);
    q_ = later;
    for (auto& d : due) ember_link_rx(d.to->node->n, d.to->link, d.bytes.data(), d.bytes.size());
  }

 private:
  void send(Conn& c, const Bytes& b, uint64_t delay = 5) { q_.push_back({now + delay, &c, b}); }

  void handle(Conn& c, const mqtt::Packet& p) {
    switch (p.type) {
      case mqtt::Type::Connect:
        client_ids.push_back(p.client_id);
        send(c, mqtt::connack(false, 0));
        break;
      case mqtt::Type::Subscribe: {
        for (auto& f : p.filters) c.subs.insert(f.first);
        send(c, mqtt::suback(p.packet_id, std::vector<uint8_t>(p.filters.size(), 0)));
        break;
      }
      case mqtt::Type::Unsubscribe:
        for (auto& f : p.filters) c.subs.erase(f.first);
        send(c, mqtt::unsuback(p.packet_id));
        break;
      case mqtt::Type::Pingreq:
        send(c, mqtt::pingresp());
        break;
      case mqtt::Type::Publish: {
        ++published;
        topics_seen.push_back(p.topic);
        Bytes wire = mqtt::publish(p.topic, ByteView(p.payload));
        for (auto& o : conns_) {
          if (!o->subs.count(p.topic)) continue;
          if (hostile && rng_.below(100) < 20) { ++dropped; continue; }
          uint64_t delay = hostile ? 5 + rng_.below(3000) : 5;  // reorders
          send(*o, wire, delay);
          if (hostile && rng_.below(100) < 30) {  // replayed much later
            ++replayed;
            send(*o, wire, delay + 60000 + rng_.below(600000));
          }
          if (hostile && rng_.below(100) < 15) {  // garbage injected on the same topic
            ++injected;
            Bytes junk = p.payload;
            junk[1 + rng_.below(static_cast<uint32_t>(junk.size() - 1))] ^= 0x40;
            send(*o, mqtt::publish(p.topic, ByteView(junk)), delay + 1);
          }
        }
        break;
      }
      case mqtt::Type::Disconnect:
        c.subs.clear();
        break;
      default:
        FAIL("client sent an unexpected packet");
    }
  }

  Rng rng_;
  std::vector<std::unique_ptr<Conn>> conns_;
  std::vector<Pending> q_;
};

struct World {
  Broker broker;
  std::vector<TNode*> nodes;
  std::map<TNode*, uint32_t> relay_link;
  uint64_t now = kStart;

  explicit World(uint64_t seed) : broker(seed) {}

  uint32_t connect(TNode& n) {
    nodes.push_back(&n);
    Json r = n.ok("links.add", relay_link_params());
    uint32_t id = static_cast<uint32_t>(r.get("link").get_int("id"));
    n.ok("links.endpoint_state", Json::object({{"id", id}, {"connected", true}}));
    relay_link[&n] = id;
    return id;
  }
  void step(uint32_t dt = 50) {
    now += dt;
    broker.now = now;
    broker.step();
    for (TNode* x : nodes) ember_tick(x->n, now);
    for (TNode* x : nodes)
      x->drain([&](uint32_t link, const std::string& bytes) {
        if (relay_link.count(x) && relay_link[x] == link) broker.from_client(*x, link, bytes);
      });
  }
  bool run_until(const std::function<bool()>& pred, uint64_t max_ms, uint32_t dt = 50) {
    for (uint64_t t = 0; t <= max_ms; t += dt) {
      if (pred()) return true;
      step(dt);
    }
    return pred();
  }
  void run(uint64_t ms) {
    for (uint64_t t = 0; t < ms; t += 50) step();
  }
};

}  // namespace

TEST_SUITE("node") {

TEST_CASE("connectivity: defaults, notices and the consent gate") {
  TNode a("", 1);
  Json c = a.ok("connectivity.get");
  CHECK_FALSE(c.get_bool("ready"));  // setup not done
  CHECK_FALSE(c.get_bool("setup_done"));
  CHECK(c.get("nearby").get_bool("enabled"));
  CHECK_FALSE(c.get("nearby").get_bool("active"));
  CHECK(c.get("nearby").get_string("notice").find("Bluetooth") == std::string::npos);  // v1: Wi-Fi/LAN only
  CHECK_FALSE(c.get("internet").get_bool("enabled"));
  CHECK_FALSE(c.get("internet").get_bool("active"));
  CHECK_FALSE(c.get("internet").has("consent_ts"));
  // the default relay list: only the broker verified to refuse both # and + is enabled
  std::vector<Json> relays = c.get("internet").get("relays").items();
  REQUIRE(relays.size() == 3);
  size_t enabled = 0;
  for (const Json& r : relays) {
    CHECK(r.get_string("state") == "off");
    if (r.get_bool("enabled")) {
      ++enabled;
      CHECK(r.get_string("url") == "wss://broker.hivemq.com:8884/mqtt");
    } else {
      CHECK(r.get_string("note").find("wildcard") != std::string::npos);
    }
  }
  CHECK(enabled == 1);
  CHECK(c.get("internet").get_string("notice").find("broker.hivemq.com") != std::string::npos);
  // nothing automatic before setup and consent
  CHECK(a.err("links.add", relay_link_params()) == "denied");
  Json nb = Json::object({{"type", "stream"},
                          {"config", Json::object({{"preset", "nearby"}})},
                          {"endpoint", Json::object({{"kind", "loopback"}})},
                          {"auto", "nearby"},
                          {"auto_role", "dial"},
                          {"auto_key", "0a0b0c0d@192.168.1.20"}});
  CHECK(a.err("links.add", nb) == "denied");
  CHECK(a.err("connectivity.set", J(R"({"setup_done":false})")) == "bad_params");
  a.ok("connectivity.set", J(R"({"setup_done":true})"));
  c = a.ok("connectivity.get");
  CHECK(c.get_bool("ready"));
  CHECK(c.get("nearby").get_bool("active"));
  CHECK(a.err("links.add", relay_link_params()) == "denied");  // still no internet consent
  Json l = a.ok("links.add", nb).get("link");
  CHECK(l.get_string("name") == "Nearby device");
  CHECK(l.get_string("auto") == "nearby");
  CHECK_FALSE(l.get_bool("persist"));
  a.ok("links.endpoint_state", Json::object({{"id", l.get_int("id")}, {"connected", true}}));
  CHECK(a.err("links.add", nb) == "busy");  // same dial key twice while the first is not down
  // consent: the relay may be added; turning it off removes relay links with relay_off
  consent(a);
  c = a.ok("connectivity.get");
  CHECK(c.get("internet").get_bool("active"));
  CHECK(c.get("internet").get_int("consent_ts") > 0);
  uint32_t rid = static_cast<uint32_t>(a.ok("links.add", relay_link_params()).get("link").get_int("id"));
  a.drain();
  consent(a, false);
  a.drain();
  CHECK(a.saw("link.removed", [&](const Json& d) { return d.get_int("id") == rid && d.get_string("reason") == "relay_off"; }));
  // Nearby off removes nearby links
  a.ok("connectivity.set", J(R"({"nearby":false})"));
  a.drain();
  CHECK(a.saw("link.removed", [&](const Json& d) { return d.get_string("reason") == "nearby_off"; }));
  // settings.set cannot reach around the consent
  CHECK(a.err("settings.set", J(R"({"connectivity.internet":true})")) != "");
  CHECK_FALSE(a.ok("connectivity.get").get("internet").get_bool("enabled"));
}

TEST_CASE("connectivity: at most 16 nearby links, the oldest unproven evicted") {
  TNode a("", 2);
  a.ok("connectivity.set", J(R"({"setup_done":true})"));
  std::vector<uint32_t> ids;
  for (int i = 0; i < 17; ++i) {
    Json nb = Json::object({{"type", "stream"},
                            {"config", Json::object({{"preset", "nearby"}})},
                            {"endpoint", Json::object({{"kind", "loopback"}})},
                            {"auto", "nearby"},
                            {"auto_role", "accept"},
                            {"auto_key", "@192.168.1." + std::to_string(10 + i)}});
    ids.push_back(static_cast<uint32_t>(a.ok("links.add", nb).get("link").get_int("id")));
  }
  a.drain();
  CHECK(a.saw("link.removed", [&](const Json& d) { return d.get_int("id") == ids[0] && d.get_string("reason") == "evicted"; }));
  size_t nearby = 0;
  for (const Json& l : a.items("links.list", "links")) nearby += l.get_string("auto") == "nearby";
  CHECK(nearby == 16);
}

TEST_CASE("connectivity M5: a nearby link whose hello fails is removed") {
  TNode a("", 3);
  a.ok("connectivity.set", J(R"({"setup_done":true})"));
  Json nb = Json::object({{"type", "stream"},
                          {"config", Json::object({{"preset", "nearby"}})},
                          {"endpoint", Json::object({{"kind", "loopback"}})},
                          {"auto", "nearby"},
                          {"auto_role", "accept"},
                          {"auto_key", "@192.168.1.30"}});
  uint32_t id = static_cast<uint32_t>(a.ok("links.add", nb).get("link").get_int("id"));
  a.ok("links.endpoint_state", Json::object({{"id", id}, {"connected", true}}));
  std::string http = "HTTP/1.1 200 OK\r\n";
  ember_link_rx(a.n, id, reinterpret_cast<const uint8_t*>(http.data()), http.size());
  for (int i = 0; i < 300; ++i) ember_tick(a.n, kStart + 50ull * i);
  a.drain();
  CHECK(a.saw("link.removed", [&](const Json& d) { return d.get_int("id") == id && d.get_string("reason") == "hello_failed"; }));
}

TEST_CASE("connectivity M9: relay endpoints are refused unless TLS (or plain to a LAN host)") {
  TNode a("", 4);
  consent(a);
  Json p = relay_link_params();
  p.set("endpoint", Json::object({{"kind", "websocket"}, {"config", Json::object({{"url", kRelayUrl}, {"insecure", true}})}}));
  CHECK(a.err("links.add", p) == "denied");
  p.set("endpoint", Json::object({{"kind", "websocket"}, {"config", Json::object({{"url", "ws://broker.example.org/mqtt"}})}}));
  CHECK(a.err("links.add", p) == "denied");
  p.set("endpoint", Json::object({{"kind", "websocket"},
                                  {"config", Json::object({{"url", "ws://broker.example.org/mqtt"}, {"allow_plaintext", true}})}}));
  CHECK(a.err("links.add", p) == "denied");  // plaintext only to a LAN host
  CHECK(a.err("connectivity.set", J(R"({"relays":[{"url":"ws://broker.example.org/mqtt","allow_plaintext":true}]})")) == "bad_params");
  CHECK(a.err("connectivity.set", J(R"({"relays":[{"url":"http://broker.example.org/mqtt"}]})")) == "bad_params");
  a.ok("connectivity.set", J(R"({"relays":[{"url":"ws://192.168.1.5:8437/mqtt","allow_plaintext":true}]})"));
}

TEST_CASE("connectivity: relay passwords are never returned") {
  TNode a("", 5);
  a.ok("connectivity.set", J(R"({"relays":[{"url":"wss://hub.example.org/mqtt","username":"u","password":"s3cret-pw"}]})"));
  CHECK(a.call("connectivity.get").dump().find("s3cret-pw") == std::string::npos);
  CHECK(a.call("settings.get").dump().find("s3cret-pw") == std::string::npos);
  // re-sending the list without the password keeps it
  a.ok("connectivity.set", J(R"({"relays":[{"url":"wss://hub.example.org/mqtt","username":"u"}]})"));
  consent(a);
  Json l = a.ok("links.add", relay_link_params("wss://hub.example.org/mqtt")).get("link");
  CHECK(l.dump().find("s3cret-pw") == std::string::npos);
  CHECK(a.call("links.list").dump().find("s3cret-pw") == std::string::npos);
}

TEST_CASE("connectivity H3: only random-key or strong-passphrase channels may use the relay") {
  TNode a("", 6);
  Json weak = a.ok("channels.create", J(R"({"name":"Weak","passphrase":"hunter2"})")).get("channel");
  CHECK_FALSE(weak.get_bool("relay_allowed"));
  CHECK(a.err("channels.update", Json::object({{"id", weak.get_string("id")}, {"relay", true}})) == "denied");
  Json strong = a.ok("channels.create", J(R"({"name":"Strong","passphrase":"correct horse battery staple"})")).get("channel");
  CHECK(strong.get_bool("relay_allowed"));
  Json rnd = a.ok("channels.create", J(R"({"name":"Family","random_key":true})")).get("channel");
  CHECK(rnd.get_bool("relay_allowed"));
  CHECK_FALSE(rnd.get_bool("relay"));  // opt-in, never by default
  CHECK(a.ok("channels.update", Json::object({{"id", rnd.get_string("id")}, {"relay", true}})).get("channel").get_bool("relay"));
  // an invite of a random-key channel carries the permission
  std::string inv = a.ok("channels.invite", Json::object({{"id", rnd.get_string("id")}})).get_string("invite");
  TNode b("", 7);
  Json joined = b.ok("channels.join", Json::object({{"invite", inv}})).get("channel");
  CHECK(joined.get_bool("relay_allowed"));
  CHECK_FALSE(joined.get_bool("relay"));
}

TEST_CASE("connectivity M15: lock removes automatic links; wipe resets consent; duress keeps the look") {
  std::string dir = temp_dir("conn-m15");
  TNode a(dir, 8);
  a.ok("security.set_passphrase", J(R"({"new":"real pass"})"));
  consent(a);
  uint32_t rid = static_cast<uint32_t>(a.ok("links.add", relay_link_params()).get("link").get_int("id"));
  a.ok("security.set_duress", J(R"({"passphrase":"duress pass"})"));
  a.drain();
  a.ok("security.lock");
  a.drain();
  CHECK(a.saw("link.removed", [&](const Json& d) { return d.get_int("id") == rid && d.get_string("reason") == "locked"; }));
  CHECK(a.err("links.add", relay_link_params()) != "");
  // a duress unlock shows the same connectivity settings (no contacts, so no real topics)
  CHECK(a.ok("security.unlock", J(R"({"passphrase":"duress pass"})")).get_bool("unlocked"));
  Json c = a.ok("connectivity.get");
  CHECK(c.get_bool("setup_done"));
  CHECK(c.get("internet").get_bool("enabled"));
  // wipe: every connectivity setting back to its default
  a.ok("security.wipe", J(R"({"confirm":"ERASE"})"));
  c = a.ok("connectivity.get");
  CHECK_FALSE(c.get_bool("setup_done"));
  CHECK_FALSE(c.get("internet").get_bool("enabled"));
  CHECK_FALSE(c.get("internet").has("consent_ts"));
}

TEST_CASE("connectivity e2e: two contacts talk through a hostile relay, nothing else crosses it") {
  TNode a("", 11), b("", 12), stranger("", 13);
  introduce(a, b);
  introduce(a, stranger);  // a's other contact, also on the broker, must never see a-b traffic
  for (TNode* n : {&a, &b, &stranger}) consent(*n);
  World w(99);
  w.connect(a);
  w.connect(b);
  w.connect(stranger);
  REQUIRE(w.run_until([&] {
    Json l = a.ok("links.list");
    return l.dump().find("\"session_link_ids\"") != std::string::npos;
  }, 30000));
  w.broker.hostile = true;
  std::vector<std::string> sent;
  for (int i = 0; i < 8; ++i) {
    std::string t = "relay message " + std::to_string(i);
    a.ok("messages.send", Json::object({{"conv", "dm:" + b.id}, {"text", t}}));
    sent.push_back(t);
    w.run(2000);
  }
  bool all = w.run_until([&] {
    for (auto& t : sent)
      if (!b.has_text("dm:" + a.id, t)) return false;
    return true;
  }, 20 * 60000);
  CHECK(all);
  b.ok("messages.send", Json::object({{"conv", "dm:" + a.id}, {"text", "and back"}}));
  CHECK(w.run_until([&] { return a.has_text("dm:" + b.id, "and back"); }, 20 * 60000));
  w.run(12 * 60000);  // let every delayed replay arrive
  // replays and injected garbage never produce a duplicate or a stray message
  size_t count = 0;
  for (const Json& m : b.messages("dm:" + a.id)) count += m.get_string("text").rfind("relay message ", 0) == 0;
  CHECK(count == sent.size());
  CHECK(w.broker.replayed > 0);
  CHECK(w.broker.injected > 0);
  CHECK_FALSE(stranger.has_text("dm:" + a.id, "relay message 0"));
  // the broker never learned a client id, and topic names are 32 hex digits
  for (const std::string& id : w.broker.client_ids) CHECK(id.empty());
  for (const std::string& t : w.broker.topics_seen) {
    CHECK(t.size() == 32);
    CHECK(t.find_first_not_of("0123456789abcdef") == std::string::npos);
  }
  // the injected garbage was seen and refused by the seal, and replays by the replay checks
  int64_t bad = 0, replay = 0;
  for (TNode* n : {&a, &b})
    for (const Json& l : n->items("links.list", "links"))
      if (l.get_string("type") == "relay") {
        bad += l.get("status").get_int("rx_bad_seal");
        replay += l.get("status").get_int("rx_replay");
      }
  CHECK(bad > 0);
  CHECK(replay > 0);
}

}  // TEST_SUITE
