// SPDX-License-Identifier: MIT
// The hub's /mqtt broker: relaying between clients, and every hostile client we can think of.
#include <mutex>
#include <random>

#include "doctest.h"
#include "framing/mqtt.h"
#include "host/host.h"
#include "host/mqtt_broker.h"
#include "host/server.h"
#include "host/util.h"
#include "host/ws_client.h"
#include "test_util.h"

using namespace ember_host;
namespace mq = ember::mqtt;

namespace {

struct Broker {
  Loop loop;
  std::unique_ptr<HttpServer> srv;
  std::unique_ptr<MqttBroker> broker;
  uint16_t port = 0;
  explicit Broker(MqttBroker::Options o = MqttBroker::Options()) {
    loop.bind_thread();
    HttpServer::Options so;
    so.bind = "127.0.0.1";
    so.port = 0;
    srv.reset(new HttpServer(loop, so));
    broker.reset(new MqttBroker(o));
    broker->attach(*srv);
    std::string err;
    REQUIRE(srv->start(&err));
    port = srv->port();
  }
  ~Broker() {
    srv->stop();
    test::run_for(loop, 20);
  }
};

struct Client {
  Loop& loop;
  WsClient ws;
  mq::Decoder dec{8192};
  std::vector<mq::Packet> got;
  bool open = false, closed = false;
  uint16_t close_code = 0;
  explicit Client(Loop& l, bool offer_mqtt = true) : loop(l), ws(l, opts(offer_mqtt)) {
    ws.on_open = [this] { open = true; };
    ws.on_close = [this](uint16_t c, const std::string&) {
      closed = true;
      close_code = c;
    };
    ws.on_message = [this](uint8_t op, std::string& d) {
      if (op != kWsBinary) return;
      dec.feed(ember::ByteView(reinterpret_cast<const uint8_t*>(d.data()), d.size()));
      mq::Packet p;
      while (dec.next(&p)) got.push_back(p);
    };
  }
  static WsClient::Options opts(bool mqtt) {
    WsClient::Options o;
    if (mqtt) o.protocols = {"mqtt"};
    return o;
  }
  bool connect(uint16_t port) {
    ws.connect("ws://127.0.0.1:" + std::to_string(port) + "/mqtt");
    if (!test::run_until(loop, [&] { return open || closed; }) || !open) return false;
    send(mq::connect("e" + random_hex(11), 240));
    return test::run_until(loop, [&] { return count(mq::Type::Connack) == 1; });
  }
  void send(const ember::Bytes& b) { ws.send_binary(b.data(), b.size()); }
  size_t count(mq::Type t) const {
    size_t n = 0;
    for (auto& p : got) n += p.type == t;
    return n;
  }
  const mq::Packet* last(mq::Type t) const {
    for (auto it = got.rbegin(); it != got.rend(); ++it)
      if (it->type == t) return &*it;
    return nullptr;
  }
  bool subscribe(const std::vector<std::string>& topics, std::vector<uint8_t>* codes = nullptr) {
    size_t before = count(mq::Type::Suback);
    send(mq::subscribe(uint16_t(7 + before), topics));
    if (!test::run_until(loop, [&] { return count(mq::Type::Suback) > before; })) return false;
    if (codes) *codes = last(mq::Type::Suback)->codes;
    return true;
  }
};

std::string topic() { return random_hex(16); }  // 32 hex characters, like a relay topic

}  // namespace

TEST_SUITE("mqtt_broker") {
  TEST_CASE("two clients relay through /mqtt; the subprotocol is negotiated") {
    Broker b;
    Client sub(b.loop), pub(b.loop), other(b.loop);
    REQUIRE(sub.connect(b.port));
    REQUIRE(pub.connect(b.port));
    REQUIRE(other.connect(b.port));
    CHECK(sub.ws.protocol() == "mqtt");
    std::string t = topic(), t2 = topic();
    std::vector<uint8_t> codes;
    REQUIRE(sub.subscribe({t}, &codes));
    CHECK(codes == std::vector<uint8_t>{0});
    REQUIRE(other.subscribe({t2}));
    std::string body(4137, 'x');  // the largest sealed relay frame
    pub.send(mq::publish(t, ember::ByteView(reinterpret_cast<const uint8_t*>(body.data()), body.size())));
    REQUIRE(test::run_until(b.loop, [&] { return sub.count(mq::Type::Publish) == 1; }));
    const mq::Packet* p = sub.last(mq::Type::Publish);
    CHECK(p->topic == t);
    CHECK(std::string(p->payload.begin(), p->payload.end()) == body);
    test::run_for(b.loop, 50);
    CHECK(other.count(mq::Type::Publish) == 0);  // other topic: nothing
    CHECK(pub.count(mq::Type::Publish) == 0);    // not subscribed: nothing
    // ping, unsubscribe
    sub.send(mq::pingreq());
    REQUIRE(test::run_until(b.loop, [&] { return sub.count(mq::Type::Pingresp) == 1; }));
    sub.send(mq::unsubscribe(9, {t}));
    REQUIRE(test::run_until(b.loop, [&] { return sub.count(mq::Type::Unsuback) == 1; }));
    pub.send(mq::publish(t, ember::ByteView(reinterpret_cast<const uint8_t*>("y"), 1)));
    test::run_for(b.loop, 50);
    CHECK(sub.count(mq::Type::Publish) == 1);
    // A client that offers no subprotocol is still served.
    Client plain(b.loop, false);
    REQUIRE(plain.connect(b.port));
    CHECK(plain.ws.protocol().empty());
    CHECK(b.broker->stats().get("clients").as_int() == 4);
  }

  TEST_CASE("wildcards, $ topics and the subscription cap are refused") {
    MqttBroker::Options o;
    o.max_subscriptions = 8;
    Broker b(o);
    Client spy(b.loop), pub(b.loop);
    REQUIRE(spy.connect(b.port));
    REQUIRE(pub.connect(b.port));
    std::vector<uint8_t> codes;
    REQUIRE(spy.subscribe({"#", "+", "a/+/b", "$SYS/#", "$share/x/y", "not-a-relay-topic",
                           "0123456789ABCDEF0123456789abcdef", "0123456789abcdef0123456789abcde"}, &codes));
    CHECK(codes == std::vector<uint8_t>(8, 0x80));
    pub.send(mq::publish(topic(), ember::ByteView(reinterpret_cast<const uint8_t*>("z"), 1)));
    test::run_for(b.loop, 50);
    CHECK(spy.count(mq::Type::Publish) == 0);
    std::vector<std::string> many;
    for (int i = 0; i < 12; ++i) many.push_back(topic());
    REQUIRE(spy.subscribe(many, &codes));
    size_t granted = 0;
    for (auto c : codes) granted += c == 0;
    CHECK(granted == 8);
    CHECK(b.broker->stats().get("refused_subscriptions").as_int() == 12);
  }

  TEST_CASE("protocol violations and oversized packets close only that client") {
    Broker b;
    auto expect_closed = [&](const std::function<void(Client&)>& act, const char* what) {
      std::string what_s = what;
      CAPTURE(what_s);
      Client c(b.loop);
      c.ws.connect("ws://127.0.0.1:" + std::to_string(b.port) + "/mqtt");
      REQUIRE(test::run_until(b.loop, [&] { return c.open; }));
      act(c);
      CHECK(test::run_until(b.loop, [&] { return c.closed; }));
    };
    std::string t = topic();
    expect_closed([&](Client& c) { c.send(mq::publish(t, ember::ByteView())); }, "PUBLISH before CONNECT");
    expect_closed([&](Client& c) { c.ws.send_text("hello"); }, "text frame");
    expect_closed([&](Client& c) {
      const char junk[] = {char(0xF0), 0x00};  // reserved packet type 15
      c.ws.send_binary(junk, sizeof junk);
    }, "garbage");
    expect_closed([&](Client& c) {
      c.send(mq::connect("e1", 60));
      c.send(mq::connect("e2", 60));
    }, "second CONNECT");
    expect_closed([&](Client& c) {
      c.send(mq::connect("e3", 60));
      std::string big(6000, 'b');
      c.send(mq::publish(t, ember::ByteView(reinterpret_cast<const uint8_t*>(big.data()), big.size())));
    }, "oversized PUBLISH");
    // A well-behaved client is unaffected.
    Client ok(b.loop);
    REQUIRE(ok.connect(b.port));
    REQUIRE(ok.subscribe({t}));
    // QoS 1 and PUBLISHes to anything but a 32-hex topic are denied (dropped, counted), not relayed.
    Client q(b.loop);
    REQUIRE(q.connect(b.port));
    std::string pk;
    pk.push_back(char(0x32));  // PUBLISH QoS 1
    std::string body;
    body.push_back(0);
    body.push_back(char(t.size()));
    body += t;
    body += std::string("\x00\x01", 2) + "p";
    pk.push_back(char(body.size()));
    pk += body;
    q.ws.send_binary(pk.data(), pk.size());
    q.send(mq::publish("$SYS/x", ember::ByteView(reinterpret_cast<const uint8_t*>("p"), 1)));
    q.send(mq::publish("other/topic", ember::ByteView(reinterpret_cast<const uint8_t*>("p"), 1)));
    test::run_for(b.loop, 100);
    CHECK(ok.count(mq::Type::Publish) == 0);
    CHECK(b.broker->stats().get("denied_publish").as_int() == 3);
    // (The oversized one is refused by the WebSocket layer, 1009, before MQTT parsing.)
    CHECK(b.broker->stats().get("protocol_errors").as_int() >= 4);
  }

  TEST_CASE("floods are rate limited per client and per topic; abusers are dropped") {
    MqttBroker::Options o;
    o.abuse_drops = 300;
    Broker b(o);
    Client sub(b.loop), flood(b.loop), honest(b.loop);
    REQUIRE(sub.connect(b.port));
    REQUIRE(flood.connect(b.port));
    REQUIRE(honest.connect(b.port));
    std::string t1 = topic(), t2 = topic();
    REQUIRE(sub.subscribe({t1, t2}));
    for (int i = 0; i < 2000 && !flood.closed; ++i) {
      flood.send(mq::publish(t1, ember::ByteView(reinterpret_cast<const uint8_t*>("f"), 1)));
      if (i % 100 == 0) test::run_for(b.loop, 1);
    }
    CHECK(test::run_until(b.loop, [&] { return flood.closed; }, 5000));
    CHECK(flood.close_code == kWsClosePolicy);
    size_t from_flood = sub.count(mq::Type::Publish);
    CHECK(from_flood <= 250);  // burst of the per-client and per-topic buckets, not 2000
    for (int i = 0; i < 10; ++i) {
      honest.send(mq::publish(t2, ember::ByteView(reinterpret_cast<const uint8_t*>("h"), 1)));
      test::run_for(b.loop, 15);
    }
    CHECK(test::run_until(b.loop, [&] { return sub.count(mq::Type::Publish) >= from_flood + 10; }));
    Json st = b.broker->stats();
    CHECK(st.get("dropped_rate").as_int() > 0);
    CHECK(st.get("evicted").as_int() == 1);
  }

  TEST_CASE("a subscriber that stops reading is dropped, the others keep receiving") {
    MqttBroker::Options o;
    o.slow_evict_ms = 1500;
    o.publish_per_s = 1000;
    o.topic_publish_per_s = 1000;
    o.bytes_per_s = 64 << 20;
    Broker b(o);
    Client pub(b.loop), good(b.loop);
    REQUIRE(pub.connect(b.port));
    REQUIRE(good.connect(b.port));
    std::string t = topic();
    REQUIRE(good.subscribe({t}));
    // The frozen subscriber: a raw socket with a tiny receive buffer that never reads.
    std::string err;
    SockAddr a;
    REQUIRE(resolve_numeric("127.0.0.1", b.port, &a));
    bool prog = false;
    sock_t s = tcp_connect_start(a, &prog, &err);
    REQUIRE(s != kInvalidSock);
    test::run_for(b.loop, 50);
    std::string key = ws_make_client_key();
    std::string req = ws_client_handshake_request("127.0.0.1", "/mqtt", key, "", {"mqtt"});
    sock_send(s, req.data(), req.size());
    test::run_for(b.loop, 50);
    auto frame = [](const ember::Bytes& pl) {
      std::string f;
      f.push_back(char(0x82));
      f.push_back(char(0x80 | pl.size()));
      f += std::string("\0\0\0\0", 4);
      f.append(pl.begin(), pl.end());
      return f;
    };
    std::string c1 = frame(mq::connect("efrozen", 240)), c2 = frame(mq::subscribe(1, {t}));
    sock_send(s, c1.data(), c1.size());
    sock_send(s, c2.data(), c2.size());
    REQUIRE(test::run_until(b.loop, [&] { return b.broker->stats().get("topics").as_int() == 1 &&
                                                  b.broker->stats().get("clients").as_int() == 3; }));
    test::run_for(b.loop, 100);
    std::string body(4000, 'q');
    uint64_t start = steady_ms();
    while (steady_ms() - start < 6000 && b.broker->stats().get("evicted").as_int() == 0) {
      for (int i = 0; i < 20; ++i)
        pub.send(mq::publish(t, ember::ByteView(reinterpret_cast<const uint8_t*>(body.data()), body.size())));
      test::run_for(b.loop, 20);
    }
    Json st = b.broker->stats();
    CHECK(st.get("evicted").as_int() == 1);
    CHECK(st.get("dropped_slow").as_int() > 0);
    CHECK(good.count(mq::Type::Publish) > 100);
    CHECK_FALSE(good.closed);
    sock_close(s);
  }

  TEST_CASE("the broker never logs topics or payloads") {
    std::mutex mu;
    std::string logs;
    set_log_level(LogLevel::Debug);
    set_log_sink([&](LogLevel, const char* line) {
      std::lock_guard<std::mutex> lk(mu);
      logs += line;
      logs += "\n";
    });
    {
      Broker b;
      Client c1(b.loop), c2(b.loop);
      REQUIRE(c1.connect(b.port));
      REQUIRE(c2.connect(b.port));
      std::string t = topic();
      REQUIRE(c1.subscribe({t, "#"}));
      std::string secret = "payload-" + random_hex(6);
      c2.send(mq::publish(t, ember::ByteView(reinterpret_cast<const uint8_t*>(secret.data()), secret.size())));
      REQUIRE(test::run_until(b.loop, [&] { return c1.count(mq::Type::Publish) == 1; }));
      std::lock_guard<std::mutex> lk(mu);
      CHECK(logs.find(t) == std::string::npos);
      CHECK(logs.find(secret) == std::string::npos);
    }
    set_log_sink(nullptr);
    set_log_level(LogLevel::Info);
  }

  TEST_CASE("at most 4 connections per source; behind a local proxy X-Forwarded-For is the source") {
    Broker b;
    std::vector<std::unique_ptr<Client>> cs;
    for (int i = 0; i < 4; ++i) {
      cs.emplace_back(new Client(b.loop));
      REQUIRE(cs.back()->connect(b.port));
    }
    Client fifth(b.loop);
    fifth.ws.connect("ws://127.0.0.1:" + std::to_string(b.port) + "/mqtt");
    REQUIRE(test::run_until(b.loop, [&] { return fifth.closed || fifth.open; }));
    CHECK_FALSE(fifth.open);
    CHECK(b.broker->stats().get("refused_source").as_int() == 1);
    // Through a reverse proxy on this machine: distinct X-Forwarded-For clients are distinct sources.
    auto via_proxy = [&](const std::string& client_ip) {
      std::string err;
      SockAddr a;
      resolve_numeric("127.0.0.1", b.port, &a);
      bool prog = false;
      sock_t s = tcp_connect_start(a, &prog, &err);
      test::run_for(b.loop, 30);
      std::string key = ws_make_client_key();
      std::string req = "GET /mqtt HTTP/1.1\r\nHost: relay.example.org\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                        "Sec-WebSocket-Key: " + key + "\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Protocol: mqtt\r\n"
                        "X-Forwarded-For: " + client_ip + "\r\n\r\n";
      sock_send(s, req.data(), req.size());
      std::string got;
      test::run_until(b.loop, [&] {
        char buf[1024];
        long n = sock_recv(s, buf, sizeof buf);
        if (n > 0) got.append(buf, size_t(n));
        return got.find("\r\n\r\n") != std::string::npos;
      }, 2000);
      sock_close(s);
      return got.compare(0, 12, "HTTP/1.1 101") == 0;
    };
    CHECK(via_proxy("198.51.100.7"));
    CHECK(via_proxy("198.51.100.8"));
  }

  TEST_CASE("the hub runs /mqtt on its own listener with no other routes") {
    test::TempDir web;
    web.file("index.html", "<!doctype html><title>ui</title>");
    test::FakeCore core;
    HostConfig c;
    c.hub_port = 0;
    c.hub_bind = "127.0.0.1";
    c.hub_autostart = true;
    c.web_root = web.path;
    c.hub_mqtt = true;
    c.hub_mqtt_port = 0;
    Host h(core.node(), c, test::FakeCore::api());
    h.loop().bind_thread();
    h.start();
    uint16_t hub = h.hub().port(), mq_port = h.hub().mqtt_port();
    REQUIRE(hub != 0);
    REQUIRE(mq_port != 0);
    CHECK(hub != mq_port);
    auto get = [&](uint16_t port, const std::string& path) {
      std::string got;
      SockAddr a;
      resolve_numeric("127.0.0.1", port, &a);
      bool prog = false;
      std::string err;
      sock_t s = tcp_connect_start(a, &prog, &err);
      test::run_for(h.loop(), 30);
      std::string req = "GET " + path + " HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
      sock_send(s, req.data(), req.size());
      test::run_until(h.loop(), [&] {
        char buf[4096];
        long n = sock_recv(s, buf, sizeof buf);
        if (n > 0) got.append(buf, size_t(n));
        return n == 0;
      }, 2000);
      sock_close(s);
      return got;
    };
    for (const char* p : {"/", "/index.html", "/hub.json", "/download/", "/ws", "/rpc"})
      CHECK_MESSAGE(get(mq_port, p).compare(0, 12, "HTTP/1.1 404") == 0, p);
    CHECK(get(mq_port, "/mqtt").compare(0, 12, "HTTP/1.1 426") == 0);  // only a WebSocket lives here
    CHECK(get(hub, "/mqtt").compare(0, 12, "HTTP/1.1 404") == 0);      // and not on the hub port
    CHECK(get(hub, "/").compare(0, 12, "HTTP/1.1 200") == 0);
  }

  TEST_CASE("random byte streams and mutated packets never break the broker") {
    MqttBroker::Options o;
    o.max_per_source = 1000;
    Broker b(o);
    std::mt19937 rng(99);
    std::string t = topic();
    std::vector<ember::Bytes> seeds = {mq::connect("e1", 60), mq::subscribe(1, {t}), mq::publish(t, ember::ByteView()),
                                       mq::pingreq(), mq::unsubscribe(2, {t}), mq::disconnect()};
    for (int round = 0; round < 60; ++round) {
      Client c(b.loop);
      c.ws.connect("ws://127.0.0.1:" + std::to_string(b.port) + "/mqtt");
      if (!test::run_until(b.loop, [&] { return c.open || c.closed; }) || !c.open) continue;
      for (int k = 0; k < 20 && !c.closed; ++k) {
        ember::Bytes m = seeds[rng() % seeds.size()];
        int kind = int(rng() % 3);
        if (kind == 0 && !m.empty()) {
          for (int f = 0; f < 3; ++f) m[rng() % m.size()] = uint8_t(rng());
        } else if (kind == 1) {
          m.resize(rng() % 64);
          for (auto& x : m) x = uint8_t(rng());
        } else if (!m.empty()) {
          m.resize(rng() % m.size());  // truncated: the rest never comes
        }
        if (!m.empty()) c.send(m);
      }
      test::run_for(b.loop, 5);
    }
    // Still serving.
    Client sub(b.loop), pub(b.loop);
    REQUIRE(sub.connect(b.port));
    REQUIRE(pub.connect(b.port));
    std::string t2 = topic();
    REQUIRE(sub.subscribe({t2}));
    pub.send(mq::publish(t2, ember::ByteView(reinterpret_cast<const uint8_t*>("ok"), 2)));
    CHECK(test::run_until(b.loop, [&] { return sub.count(mq::Type::Publish) == 1; }));
  }
}
