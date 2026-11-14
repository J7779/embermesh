// The `relay` link driver against an in-test broker (docs/CONNECTIVITY.md §4, PROTOCOL.md §12): consent
// and clock gating, the topic window, sealing, and the review items H2, M3, M14 and M16.
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "doctest.h"
#include "framing/mqtt.h"
#include "link/relay_link.h"
#include "link_harness.h"
#include "proto/relay.h"

using namespace ember;
using namespace linktest;
namespace rly = ember::proto::relay;

namespace {

const char* kUrl = "wss://broker.example.org:8084/mqtt";
const std::string kHost = "broker.example.org:8084";
const uint64_t kNoon = 1763121600000ull;  // a UTC day (epoch 20406) at 12:00
const uint8_t kOwnHop[4] = {0xAA, 0xAA, 0xAA, 0xAA};
const uint8_t kPeerHop[4] = {0x11, 0x22, 0x33, 0x44};

crypto::Key32 root_of(uint8_t v) {
  crypto::Key32 k;
  k.fill(v);
  return k;
}

// A minimal mesh frame (PROTOCOL §2): b0 = version 2, type control; hop_src; one body byte.
Bytes mesh_frame(const uint8_t hop[4], uint8_t body = 0x01) {
  return Bytes{0xA0, hop[0], hop[1], hop[2], hop[3], body, 0x00};
}

struct Rig {
  Clock clk;
  FakeHost host{&clk};
  std::unique_ptr<Link> link;
  link::RelayControl* rc = nullptr;
  size_t read_pos = 0;
  std::vector<mqtt::Packet> sent;  // what the driver wrote, decoded
  std::string err;

  explicit Rig(bool connect = true, uint64_t wall = kNoon) {
    link = create_link("relay", std::string("{\"url\":\"") + kUrl + "\"}", host, &err);
    REQUIRE(link);
    rc = link::as_relay(link.get());
    REQUIRE(rc);
    rc->set_own_hop(kOwnHop);
    rc->set_wall_clock(wall);
    if (connect) {
      link->on_io_state(true);
      rc->set_consent(true);
      link->tick(clk.t);
    }
  }
  void pair_session(uint8_t root = 1) {
    link::RelaySession s;
    s.key = "c:peer";
    s.root = root_of(root);
    std::memcpy(s.peer_hop, kPeerHop, 4);
    rc->set_sessions({s});
  }
  void channel_session(uint8_t root = 2) {
    link::RelaySession s;
    s.key = "ch:0000002a";
    s.channel = true;
    s.root = root_of(root);
    rc->set_sessions({s});
  }
  // Decodes everything written since the last call.
  std::vector<mqtt::Packet> drain() {
    Bytes all = host.written();
    mqtt::Decoder d(1 << 16);
    d.feed(ByteView(all).sub(read_pos));
    read_pos = all.size();
    std::vector<mqtt::Packet> out;
    mqtt::Packet p;
    while (d.next(&p)) out.push_back(p);
    REQUIRE_FALSE(d.failed());
    return out;
  }
  void deliver(const Bytes& b) { link->on_io_bytes(ByteView(b)); }
  void connack() { deliver(mqtt::connack(false, 0)); }
  void ack_subscriptions() {
    for (const mqtt::Packet& p : drain())
      if (p.type == mqtt::Type::Subscribe) {
        std::vector<uint8_t> codes(p.filters.size(), 0);
        deliver(mqtt::suback(p.packet_id, codes));
      } else if (p.type == mqtt::Type::Unsubscribe) {
        deliver(mqtt::unsuback(p.packet_id));
      }
  }
  void step(uint64_t ms) {
    clk.t += ms;
    link->tick(clk.t);
  }
  Json status() { return parse_json(link->status_json()); }
  // Publishes a sealed frame to the driver as the broker would.
  void inject(uint8_t root, uint32_t epoch, uint64_t sent_ms, const Bytes& frame, uint8_t seed = 1, uint8_t header = 0x30) {
    std::string t = rly::topic(root_of(root), epoch, kHost);
    crypto::Key32 k = rly::frame_key(root_of(root), epoch, kHost);
    uint8_t rnd[16];
    std::memset(rnd, seed, sizeof rnd);
    Bytes m = rly::seal(k, kHost, t, rnd, sent_ms, ByteView(frame));
    // Built by hand so the fixed-header flags (RETAIN, DUP, QoS) can be anything the broker might send.
    Bytes body{static_cast<uint8_t>(t.size() >> 8), static_cast<uint8_t>(t.size())};
    body.insert(body.end(), t.begin(), t.end());
    if (header & 0x06) body.insert(body.end(), {0x00, 0x05});  // QoS 1/2 carry a packet id
    body.insert(body.end(), m.begin(), m.end());
    Bytes pub{header};
    for (size_t n = body.size();;) {
      uint8_t d = static_cast<uint8_t>(n % 128);
      n /= 128;
      pub.push_back(static_cast<uint8_t>(n ? d | 0x80 : d));
      if (!n) break;
    }
    pub.insert(pub.end(), body.begin(), body.end());
    deliver(pub);
  }
};

}  // namespace

TEST_SUITE("link") {

TEST_CASE("relay: nothing is sent before consent or without a sane clock") {
  Rig r(false, 0);
  r.pair_session();
  r.link->on_io_state(true);
  for (int i = 0; i < 50; ++i) r.step(100);
  CHECK(r.host.writes.empty());  // no consent
  r.rc->set_consent(true);
  for (int i = 0; i < 50; ++i) r.step(100);
  CHECK(r.host.writes.empty());  // consent, but the clock is unset
  CHECK(r.status().get_string("mqtt_state") == "waiting_for_time");
  r.rc->set_wall_clock(kNoon);
  r.step(100);
  std::vector<mqtt::Packet> p = r.drain();
  REQUIRE(p.size() == 1);
  CHECK(p[0].type == mqtt::Type::Connect);
  CHECK(p[0].client_id.empty());  // M10: nothing that names us
  CHECK(p[0].clean_session);
  CHECK(p[0].keepalive >= 200);
  CHECK(p[0].keepalive <= 280);
  // consent withdrawn: DISCONNECT, then silence
  r.connack();
  r.rc->set_consent(false);
  std::vector<mqtt::Packet> q = r.drain();
  REQUIRE(!q.empty());
  CHECK(q.back().type == mqtt::Type::Disconnect);
  for (int i = 0; i < 50; ++i) r.step(1000);
  CHECK(r.drain().empty());
}

TEST_CASE("relay: subscribes topic(E), plus E±1 only near midnight; seals on topic(E)") {
  Rig r;
  r.pair_session();
  r.drain();
  r.connack();
  std::vector<mqtt::Packet> subs = r.drain();
  REQUIRE(subs.size() == 1);
  REQUIRE(subs[0].type == mqtt::Type::Subscribe);
  REQUIRE(subs[0].filters.size() == 1);  // noon: only today
  CHECK(subs[0].filters[0].first == rly::topic(root_of(1), 20406, kHost));
  r.deliver(mqtt::suback(subs[0].packet_id, {0}));
  // a frame from the child goes out sealed on topic(E)
  REQUIRE(r.rc->child_count() == 1);
  Bytes f = mesh_frame(kOwnHop, 0x42);
  REQUIRE(r.rc->child(0)->send(ByteView(f), 3));
  std::vector<mqtt::Packet> pub = r.drain();
  REQUIRE(pub.size() == 1);
  CHECK(pub[0].type == mqtt::Type::Publish);
  CHECK(pub[0].topic == rly::topic(root_of(1), 20406, kHost));
  rly::Opened o;
  REQUIRE(rly::open(rly::frame_key(root_of(1), 20406, kHost), kHost, pub[0].topic, ByteView(pub[0].payload), &o));
  CHECK(o.frame == f);
  // 22:00 UTC: tomorrow's topic is added
  r.rc->set_wall_clock(kNoon + 10 * 3600000ull);
  r.step(100);
  std::vector<mqtt::Packet> more = r.drain();
  REQUIRE(more.size() == 1);
  CHECK(more[0].filters[0].first == rly::topic(root_of(1), 20407, kHost));
  // 02:00 the next day: E+1 is today; yesterday's stays for the first 3 hours; the day before goes
  r.rc->set_wall_clock(kNoon + 14 * 3600000ull);
  r.step(100);
  std::vector<mqtt::Packet> next = r.drain();
  bool unsub_none = true;
  for (const auto& p : next)
    if (p.type == mqtt::Type::Unsubscribe) unsub_none = false;
  CHECK(unsub_none);
  r.rc->set_wall_clock(kNoon + 16 * 3600000ull);  // 04:00: yesterday's topic is dropped
  r.step(20000);
  bool unsub = false;
  for (const auto& p : r.drain())
    if (p.type == mqtt::Type::Unsubscribe) unsub = p.filters[0].first == rly::topic(root_of(1), 20406, kHost);
  CHECK(unsub);
}

TEST_CASE("relay H2: own echoes, wrong senders and out-of-epoch frames never touch the replay window") {
  Rig r;
  r.pair_session();
  r.connack();
  r.ack_subscriptions();
  // My own frame echoed back by the broker, stamped far in the future: dropped first.
  r.inject(1, 20406, kNoon + 20 * 3600000ull, mesh_frame(kOwnHop), 9);
  CHECK(r.status().get_int("rx_own_echo") == 1);
  // The contact's frame at the right time still arrives (the echo did not move its window).
  r.inject(1, 20406, kNoon, mesh_frame(kPeerHop), 1);
  REQUIRE(r.host.frames.size() == 1);
  // A frame on the pair topic from a third hop id: dropped.
  const uint8_t other[4] = {9, 9, 9, 9};
  r.inject(1, 20406, kNoon, mesh_frame(other), 2);
  CHECK(r.status().get_int("rx_wrong_hop") == 1);
  // Outside [start(e) - 1 day, start(e) + 2 days): dropped.
  r.inject(1, 20406, 20406ull * 86400000ull + 3 * 86400000ull, mesh_frame(kPeerHop), 3);
  CHECK(r.status().get_int("rx_out_of_epoch") == 1);
  CHECK(r.host.frames.size() == 1);
}

TEST_CASE("relay H2: on a channel, one member's wrong clock cannot blackhole the others") {
  Rig r;
  r.channel_session();
  r.connack();
  r.ack_subscriptions();
  const uint8_t x[4] = {1, 1, 1, 1}, y[4] = {2, 2, 2, 2};
  r.inject(2, 20406, kNoon + 20 * 3600000ull, mesh_frame(x), 1);  // member x, 20 h ahead
  r.inject(2, 20406, kNoon, mesh_frame(y), 2);                     // member y, on time
  r.inject(2, 20406, kNoon + 1000, mesh_frame(y, 2), 3);
  CHECK(r.host.frames.size() == 3);
  CHECK(r.status().get_int("rx_replay") == 0);
  // y's own old frame (more than 10 minutes below y's highest) is a replay
  r.inject(2, 20406, kNoon - 11 * 60000ull, mesh_frame(y, 3), 4);
  CHECK(r.status().get_int("rx_replay") == 1);
}

TEST_CASE("relay M3: retained, duplicate-flagged and QoS 1 publishes and exact replays are dropped") {
  Rig r;
  r.pair_session();
  r.connack();
  r.ack_subscriptions();
  r.inject(1, 20406, kNoon, mesh_frame(kPeerHop), 1, 0x31);  // RETAIN
  r.inject(1, 20406, kNoon, mesh_frame(kPeerHop), 1, 0x3A);  // DUP + QoS 1
  r.inject(1, 20406, kNoon, mesh_frame(kPeerHop), 1, 0x32);  // QoS 1
  CHECK(r.status().get_int("rx_bad_publish") == 3);
  CHECK(r.host.frames.empty());
  r.inject(1, 20406, kNoon, mesh_frame(kPeerHop), 7);
  r.inject(1, 20406, kNoon, mesh_frame(kPeerHop), 7);  // the very same message again
  CHECK(r.host.frames.size() == 1);
  CHECK(r.status().get_int("rx_replay") == 1);
}

TEST_CASE("relay M14: a topic flooded with junk is unsubscribed for a while, then retried") {
  Rig r;
  r.pair_session();
  r.connack();
  r.ack_subscriptions();
  std::string t = rly::topic(root_of(1), 20406, kHost);
  for (int i = 0; i < 25; ++i) r.deliver(mqtt::publish(t, ByteView(Bytes(169, static_cast<uint8_t>(i)))));
  CHECK(r.status().get_int("rx_bad_seal") >= 21);
  CHECK(r.status().get_int("breaker_trips") == 1);
  CHECK(r.status().get_int("topics_paused") == 1);
  std::vector<mqtt::Packet> out = r.drain();
  bool unsub = false;
  for (const auto& p : out)
    if (p.type == mqtt::Type::Unsubscribe) {
      unsub = true;
      r.deliver(mqtt::unsuback(p.packet_id));
    }
  CHECK(unsub);
  r.step(61000);
  bool resub = false;
  for (const auto& p : r.drain()) resub = resub || (p.type == mqtt::Type::Subscribe && p.filters[0].first == t);
  CHECK(resub);
  CHECK(r.status().get_int("topics_paused") == 0);
}

TEST_CASE("relay M16: hostile broker behaviour closes the session or is refused") {
  SUBCASE("CONNECT from the broker") {
    Rig r;
    r.pair_session();
    r.deliver(mqtt::connect("x", 60));
    CHECK(r.link->state() == LinkState::Error);
  }
  SUBCASE("PUBLISH before CONNACK") {
    Rig r;
    r.pair_session();
    r.inject(1, 20406, kNoon, mesh_frame(kPeerHop));
    CHECK(r.link->state() == LinkState::Error);
    CHECK(r.host.frames.empty());
  }
  SUBCASE("an unsolicited SUBACK") {
    Rig r;
    r.connack();
    r.deliver(mqtt::suback(999, {0}));
    CHECK(r.link->state() == LinkState::Error);
  }
  SUBCASE("SUBACK 0x80 marks the topic refused") {
    Rig r;
    r.pair_session();
    r.connack();
    for (const mqtt::Packet& p : r.drain())
      if (p.type == mqtt::Type::Subscribe) r.deliver(mqtt::suback(p.packet_id, {0x80}));
    CHECK(r.link->state() == LinkState::Up);
    CHECK(r.status().get_int("topics_refused") == 1);
  }
  SUBCASE("a WebSocket message over 8 KiB") {
    Rig r;
    r.connack();
    r.deliver(Bytes(9000, 0x30));
    CHECK(r.link->state() == LinkState::Error);
  }
  SUBCASE("no CONNACK within 20 s") {
    Rig r;
    r.step(21000);
    CHECK(r.link->state() == LinkState::Error);
  }
  SUBCASE("no SUBACK within 30 s") {
    Rig r;
    r.pair_session();
    r.connack();
    r.step(31000);
    CHECK(r.link->state() == LinkState::Error);
  }
  SUBCASE("no PINGRESP within the keepalive") {
    Rig r;
    r.connack();
    // the ping goes out at 90 % of the keepalive; no answer within one more keepalive closes the session
    bool closed = false;
    for (int i = 0; i < 700 && !closed; ++i) {
      r.step(1000);
      closed = r.link->state() == LinkState::Error;
    }
    CHECK(closed);
    CHECK(r.status().get_int("reconnects") == 1);
  }
  SUBCASE("a publish on a topic we never subscribed is ignored") {
    Rig r;
    r.pair_session();
    r.connack();
    r.ack_subscriptions();
    r.deliver(mqtt::publish("ffffffffffffffffffffffffffffffff", ByteView(Bytes(169, 0))));
    r.deliver(mqtt::publish("not-hex", ByteView(Bytes(169, 0))));
    CHECK(r.status().get_int("rx_unknown_topic") == 2);
    CHECK(r.link->state() == LinkState::Up);
  }
}

TEST_CASE("relay: sessions come and go; the parent never carries frames itself") {
  Rig r;
  CHECK(r.link->caps().mtu == 0);
  CHECK_FALSE(r.link->send(ByteView(Bytes{1, 2, 3}), 0));
  r.pair_session(1);
  r.connack();
  r.ack_subscriptions();
  REQUIRE(r.rc->child_count() == 1);
  Link* c = r.rc->child(0);
  CHECK(c->caps().mtu == 4000);
  CHECK(c->caps().bitrate_bps == 64000);
  CHECK_FALSE(c->caps().requires_station_id);
  CHECK(c->state() == LinkState::Up);
  r.rc->set_sessions({});
  CHECK(r.rc->child_count() == 0);
  bool unsub = false;
  for (const auto& p : r.drain()) unsub = unsub || p.type == mqtt::Type::Unsubscribe;
  CHECK(unsub);
}

}  // TEST_SUITE

namespace {

// Hostile broker streams: valid sealed publishes, forged/mutated ones, protocol packets in the wrong
// order, at arbitrary splits, with session and clock changes in between. The driver must never
// crash, never deliver a frame it did not authenticate, and recover after the broker behaves again.
void relay_fuzz(uint64_t seed, int iters) {
  Prng R(seed * 7919 + 3);
  Rig r;
  r.pair_session(1);
  std::vector<Bytes> corpus = {mqtt::connack(false, 0), mqtt::connack(false, 5), mqtt::suback(1, {0}),
                               mqtt::suback(2, {0x80}), mqtt::unsuback(3), mqtt::pingresp(), mqtt::pingreq(),
                               mqtt::connect("", 60), mqtt::disconnect()};
  std::string t = rly::topic(root_of(1), 20406, kHost);
  for (int k = 0; k < 4; ++k) {
    uint8_t rnd[16];
    std::memset(rnd, k + 1, 16);
    corpus.push_back(mqtt::publish(t, ByteView(rly::seal(rly::frame_key(root_of(1), 20406, kHost), kHost, t, rnd,
                                                         kNoon + k, ByteView(mesh_frame(kPeerHop, static_cast<uint8_t>(k)))))));
  }
  size_t authentic = 4;
  for (int i = 0; i < iters; ++i) {
    Bytes b = corpus[R.below(static_cast<uint32_t>(corpus.size()))];
    switch (R.below(5)) {
      case 0: if (!b.empty()) b[R.below(static_cast<uint32_t>(b.size()))] ^= static_cast<uint8_t>(1u << R.below(8)); break;
      case 1: b.insert(b.begin() + R.below(static_cast<uint32_t>(b.size() + 1)), R.byte()); break;
      case 2: if (b.size() > 1) b.resize(1 + R.below(static_cast<uint32_t>(b.size() - 1))); break;
      case 3: { Bytes x = R.bytes(R.below(300)); b.insert(b.end(), x.begin(), x.end()); break; }
      default: break;  // unchanged
    }
    size_t off = 0;
    while (off < b.size()) {
      size_t take = 1 + R.below(static_cast<uint32_t>(b.size() - off));
      r.deliver(Bytes(b.begin() + static_cast<long>(off), b.begin() + static_cast<long>(off + take)));
      off += take;
    }
    switch (R.below(40)) {
      case 0: r.rc->set_sessions({}); break;
      case 1: r.pair_session(1); break;
      case 2: r.rc->set_wall_clock(kNoon + R.below(86400000)); break;
      case 3: r.rc->set_consent(false); r.rc->set_consent(true); break;
      case 4: r.link->on_io_state(false); r.link->on_io_state(true); break;
      default: break;
    }
    r.step(R.below(40000));
    r.drain();
  }
  // Every delivered frame is one of the authentic ones (no forgery passed the seal).
  std::set<Bytes> ok;
  for (int k = 0; k < 4; ++k) ok.insert(mesh_frame(kPeerHop, static_cast<uint8_t>(k)));
  for (const auto& f : r.host.frames) CHECK(ok.count(f.data) == 1);
  CHECK(r.host.frames.size() <= authentic);  // each authentic message at most once (replay window)
  // The broker behaves again: a fresh session comes up and a fresh frame arrives.
  r.rc->set_wall_clock(kNoon);
  r.pair_session(1);
  r.link->on_io_state(false);
  r.link->on_io_state(true);
  r.step(31000);
  r.drain();
  for (int k = 0; k < 3 && r.status().get_string("mqtt_state") != "up"; ++k) {
    r.connack();
    r.ack_subscriptions();
    r.step(31000);
  }
  r.ack_subscriptions();
  REQUIRE(r.status().get_string("mqtt_state") == "up");
  size_t before = r.host.frames.size();
  r.inject(1, 20406, kNoon + 5000, mesh_frame(kPeerHop, 0x77), static_cast<uint8_t>(0xC0 + seed % 32));
  CHECK(r.host.frames.size() == before + 1);
}

}  // namespace

TEST_SUITE("link") {

TEST_CASE("relay fuzz: hostile broker streams (fast)") {
  for (uint64_t s = 1; s <= 4; ++s) relay_fuzz(s, 400);
}

TEST_CASE("relay long: hostile broker streams, many seeds" * doctest::skip()) {
  for (uint64_t s = 100; s < 140; ++s) relay_fuzz(s, 5000);
}

}  // TEST_SUITE
