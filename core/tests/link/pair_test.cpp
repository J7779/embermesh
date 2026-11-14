// Back-to-back tests for the byte/datagram drivers (raw, stream, kiss): every size 1..mtu, mtu+1
// refused, chunking, loss, corruption, garbage, priority order, queue limits, state reporting.
#include <set>

#include "doctest.h"
#include "link_harness.h"

using namespace ember;
using namespace linktest;

namespace {

struct Case {
  const char* type;
  const char* cfg;
  bool datagram;
  bool integrity;  // corrupted input never yields a wrong frame
};

const Case kCases[] = {
    {"raw", "{}", true, false},
    {"raw", R"({"preset":"websocket"})", true, false},
    {"raw", R"({"preset":"custom","mtu":64})", true, false},
    {"stream", "{}", false, true},
    {"stream", R"({"preset":"tcp"})", false, true},
    {"stream", R"({"preset":"ble"})", false, true},
    {"stream", R"({"preset":"ebyte"})", false, true},
    {"stream", R"({"preset":"hc12","fec_roots":32})", false, true},
    {"stream", R"({"preset":"sik"})", false, true},
    {"stream", R"({"preset":"xbee","write_chunk":20})", false, true},
    {"stream", R"({"preset":"custom","mtu":300,"fec":"yes"})", false, true},
    {"kiss", R"({"callsign":"QX0TST-1"})", false, false},
    {"kiss", R"({"callsign":"QX0TST-1","payload":"raw","licence":"licence_free","bitrate_bps":9600})", false, false},
    {"kiss", R"({"callsign":"QX0TST-1","path":"WIDE1-1","mtu":512})", false, false},
};

std::string cfg_b(const Case& c) {
  std::string s = c.cfg;
  // second station gets a different callsign
  size_t p = s.find("QX0TST-1");
  if (p != std::string::npos) s.replace(p, 8, "QX0TST-2");
  return s;
}

Bytes frame_of(size_t n, uint32_t tag) {
  Bytes b(n);
  for (size_t i = 0; i < n; ++i) b[i] = static_cast<uint8_t>(tag * 131 + i * 7 + (i >> 8));
  if (n >= 2) { b[0] = static_cast<uint8_t>(tag); b[1] = static_cast<uint8_t>(tag >> 8); }
  return b;
}

}  // namespace

TEST_SUITE("link") {

TEST_CASE("pair: every frame size 1..mtu crosses, mtu+1 refused") {
  for (const Case& c : kCases) {
    CAPTURE(std::string(c.type) + " " + c.cfg);
    PipeOpts o;
    o.datagram = c.datagram;
    o.max_chunk = c.datagram ? 0 : 17;
    o.delay_ms = 5;
    Pair p(c.type, c.cfg, cfg_b(c), o);
    REQUIRE(p.ok());
    CHECK(p.a->state() == LinkState::Down);
    p.connect();
    CHECK(p.a->state() == LinkState::Up);
    size_t mtu = p.a->caps().mtu;
    REQUIRE(mtu >= 32);
    CHECK(!p.a->send(ByteView(frame_of(mtu + 1, 1)), 3));
    CHECK(!p.a->send(ByteView(), 3));
    // Step sizes so the test stays fast for big MTUs.
    std::vector<size_t> sizes;
    for (size_t n = 1; n <= mtu; n += (n < 64 ? 1 : n < mtu - 64 ? 13 : 1)) sizes.push_back(n);
    size_t sent = 0;
    for (size_t n : sizes) {
      Bytes f = frame_of(n, static_cast<uint32_t>(n));
      if (!p.a->send(ByteView(f), 4)) {
        // queue full on paced links: drain and retry
        REQUIRE(p.run_until([&] { return p.a->tx_queue_depth() == 0; }, 600000, 50));
        REQUIRE(p.a->send(ByteView(f), 4));
      }
      ++sent;
      p.step(1);
    }
    REQUIRE(p.run_until([&] { return p.hb.frames.size() >= sent; }, 3600000, 50));
    REQUIRE(p.hb.frames.size() == sent);
    for (size_t i = 0; i < sent; ++i) CHECK(p.hb.frames[i].data == frame_of(sizes[i], static_cast<uint32_t>(sizes[i])));
    // and the other direction
    Bytes back = frame_of(mtu, 77);
    REQUIRE(p.b->send(ByteView(back), 0));
    REQUIRE(p.run_until([&] { return !p.ha.frames.empty(); }, 600000, 50));
    CHECK(p.ha.frames.back().data == back);
    Json st;
    REQUIRE(Json::parse(p.a->status_json(), st));
    CHECK(st.get_int("tx_frames") == static_cast<int64_t>(sent));
    CHECK(st.get_string("state") == "up");
  }
}

TEST_CASE("pair: priority order and queue limits while down") {
  for (const Case& c : kCases) {
    CAPTURE(std::string(c.type) + " " + c.cfg);
    PipeOpts o;
    o.datagram = c.datagram;
    Pair p(c.type, c.cfg, cfg_b(c), o);
    REQUIRE(p.ok());
    p.b->on_io_state(true);
    // a is down: frames queue up
    const uint8_t prios[] = {7, 3, 0, 5, 3, 1};
    for (size_t i = 0; i < 6; ++i) REQUIRE(p.a->send(ByteView(frame_of(10, static_cast<uint32_t>(i))), prios[i]));
    CHECK(p.a->tx_queue_depth() == 6);
    CHECK(p.ha.writes.empty());
    p.a->on_io_state(true);
    REQUIRE(p.run_until([&] { return p.hb.frames.size() == 6; }, 600000, 20));
    const uint32_t order[] = {2, 5, 1, 4, 3, 0};
    for (size_t i = 0; i < 6; ++i) CHECK(p.hb.frames[i].data == frame_of(10, order[i]));
    // queue limit: down again, fill to the limit
    p.a->on_io_state(false);
    CHECK(p.a->state() == LinkState::Down);
    size_t accepted = 0;
    while (accepted < 2000 && p.a->send(ByteView(frame_of(20, 1)), 6)) ++accepted;
    CHECK(accepted >= 1);
    CHECK(accepted < 2000);
    CHECK(!p.a->send(ByteView(frame_of(20, 2)), 6));
    CHECK(!p.a->send(ByteView(frame_of(20, 2)), 7));
    CHECK(p.a->send(ByteView(frame_of(20, 3)), 0));  // higher priority evicts bulk
    Json st;
    REQUIRE(Json::parse(p.a->status_json(), st));
    CHECK(st.get_int("tx_evicted") >= 1);
    CHECK(st.get_int("tx_refused") >= 2);
  }
}

TEST_CASE("pair: loss, corruption and garbage are survived (seeded fuzz)") {
  for (const Case& c : kCases) {
    CAPTURE(std::string(c.type) + " " + c.cfg);
    for (uint64_t seed = 1; seed <= 3; ++seed) {
      PipeOpts o;
      o.datagram = c.datagram;
      o.max_chunk = c.datagram ? 0 : 9;
      o.drop = c.datagram ? 0.05 : 0.004;  // stream: per 1..9-byte chunk
      o.corrupt = 0.002;
      o.seed = seed;
      Pair p(c.type, c.cfg, cfg_b(c), o);
      REQUIRE(p.ok());
      p.connect();
      Prng r(seed * 1000 + 7);
      std::set<Bytes> sent;
      size_t mtu = p.a->caps().mtu;
      for (int i = 0; i < 40; ++i) {
        Bytes f = r.bytes(1 + r.below(static_cast<uint32_t>(mtu < 200 ? mtu : 200)));
        if (p.a->send(ByteView(f), static_cast<uint8_t>(r.below(8)))) sent.insert(f);
        if (r.below(4) == 0) {
          Bytes g = r.bytes(r.below(64));
          p.b->on_io_bytes(ByteView(g));  // line noise straight into the receiver
        }
        p.step(20);
      }
      p.run(120000, 100);
      size_t good = 0;
      for (const auto& f : p.hb.frames) {
        bool known = sent.count(f.data) != 0;
        if (c.integrity) CHECK(known);
        good += known;
      }
      if (c.integrity) CHECK(good >= sent.size() / 2);  // most survive 5 % loss
    }
  }
}

TEST_CASE("pair: truncated writes and endpoint reconnects") {
  for (const Case& c : kCases) {
    if (c.datagram) continue;
    CAPTURE(std::string(c.type) + " " + c.cfg);
    Pair p(c.type, c.cfg, cfg_b(c));
    REQUIRE(p.ok());
    p.connect();
    // a half-written frame, then the cable is pulled and re-plugged
    p.ab.enabled = false;
    REQUIRE(p.a->send(ByteView(frame_of(40, 9)), 2));
    p.step(1);
    Bytes partial = p.ha.written();
    REQUIRE(!partial.empty());
    p.ab.enabled = true;
    p.b->on_io_bytes(ByteView(partial).sub(0, partial.size() / 2));
    p.b->on_io_state(false);
    CHECK(p.b->state() == LinkState::Down);
    p.b->on_io_state(true);
    CHECK(p.b->state() == LinkState::Up);
    p.ha.writes.clear();
    REQUIRE(p.a->send(ByteView(frame_of(33, 10)), 2));
    REQUIRE(p.run_until([&] { return !p.hb.frames.empty(); }, 60000));
    CHECK(p.hb.frames.size() == 1);
    CHECK(p.hb.frames[0].data == frame_of(33, 10));
  }
}

TEST_CASE("raw: datagram rules") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto l = create_link("raw", R"({"preset":"webrtc"})", h, &err);
  REQUIRE(l);
  LinkCaps c = l->caps();
  CHECK(c.mtu == 4096);
  CHECK(!c.broadcast);
  CHECK(c.reliable);
  CHECK(c.encryption_allowed);
  CHECK(!c.half_duplex);
  l->on_io_bytes(ByteView());
  CHECK(h.frames.empty());
  l->on_io_bytes(ByteView(Bytes(9000, 1)));
  CHECK(h.frames.empty());
  l->on_io_bytes(ByteView(Bytes(5000, 1)));  // larger than our MTU but plausible from a peer
  CHECK(h.frames.size() == 1);
  CHECK(l->state() == LinkState::Up);
  REQUIRE(l->reconfigure(R"({"preset":"udp_lan","cost":5})", &err));
  CHECK(l->caps().mtu == 1400);
  CHECK(l->caps().cost == 5);
  CHECK(!l->reconfigure(R"({"mtu":5})", &err));
  CHECK(l->caps().mtu == 1400);
  l->on_io_state(true);
  Bytes f(1400, 3);
  REQUIRE(l->send(ByteView(f), 0));
  REQUIRE(h.writes.size() == 1);
  CHECK(h.writes[0] == f);  // exactly one datagram, no header
}

TEST_CASE("stream: wire format, pacing and FEC repair") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto l = create_link("stream", R"({"preset":"cable"})", h, &err);
  REQUIRE(l);
  l->on_io_state(true);
  Bytes f = {1, 2, 3};
  REQUIRE(l->send(ByteView(f), 0));
  REQUIRE(h.writes.size() == 1);
  // E5 4D type=0 len=0003 hcrc payload crc32
  CHECK(h.writes[0].size() == 13);
  CHECK(h.writes[0][0] == 0xE5);
  CHECK(h.writes[0][1] == 0x4D);
  CHECK(h.writes[0][2] == 0);
  CHECK(h.writes[0][4] == 3);

  // Paced radio modem: second frame waits for the first one's airtime
  FakeHost h2(&clk);
  auto r = create_link("stream", R"({"preset":"ebyte"})", h2, &err);
  REQUIRE(r);
  LinkCaps c = r->caps();
  CHECK(c.half_duplex);
  CHECK(c.broadcast);
  CHECK(c.mtu == 200);
  CHECK(c.bitrate_bps == 2400);
  r->on_io_state(true);
  REQUIRE(r->send(ByteView(Bytes(100, 1)), 3));
  REQUIRE(r->send(ByteView(Bytes(100, 2)), 3));
  CHECK(h2.writes.size() == 1);
  CHECK(r->channel_busy());
  size_t first = h2.writes[0].size();
  CHECK(h2.writes[0][0] == 0x7E);  // FEC sync
  clk.t += (first * 8 * 1000) / 2400 - 10;
  r->tick(clk.t);
  CHECK(h2.writes.size() == 1);
  clk.t += 200;
  r->tick(clk.t);
  CHECK(h2.writes.size() == 2);

  // FEC repairs byte errors and reports them
  FakeHost h3(&clk);
  auto rx = create_link("stream", R"({"preset":"ebyte"})", h3, &err);
  REQUIRE(rx);
  rx->on_io_state(true);
  Bytes w = h2.writes[0];
  w[1] ^= 0x01;  // sync bit error
  for (size_t i = 20; i < 26; ++i) w[i] ^= 0x5A;
  rx->on_io_bytes(ByteView(w));
  REQUIRE(h3.frames.size() == 1);
  CHECK(h3.frames[0].data == Bytes(100, 1));
  CHECK(h3.frames[0].meta.corrected_errors >= 6);

  // Amateur switch
  auto am = create_link("stream", R"({"preset":"ebyte","amateur":true,"duty_cycle_permille":100})", h3, &err);
  REQUIRE(am);
  CHECK(!am->caps().encryption_allowed);
  CHECK(am->caps().requires_station_id);
  CHECK(am->caps().duty_cycle_permille == 100);
}

TEST_CASE("stream: duty cycle budget holds frames back") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto l = create_link("stream", R"({"preset":"ebyte","duty_cycle_permille":10})", h, &err);
  REQUIRE(l);
  l->on_io_state(true);
  // 1 % of an hour = 36 s of airtime; each 200-byte frame is ~0.9 s at 2400 bit/s
  for (int i = 0; i < 30; ++i) l->send(ByteView(Bytes(200, static_cast<uint8_t>(i))), 4);
  for (int i = 0; i < 600; ++i) { clk.t += 1000; l->tick(clk.t); }
  Json st;
  REQUIRE(Json::parse(l->status_json(), st));
  CHECK(st.get_int("airtime_ms") <= 36000);
  CHECK(st.get_int("tx_frames") >= 20);
  CHECK(st.get_int("tx_frames") < 60);
}

TEST_CASE("stream M5: a nearby link speaks only after the EmberMesh hello") {
  const Bytes hello = {0x45, 0x4D, 0x4E, 0x42, 0x01, 0x00, 0x00, 0x00};
  std::string err;
  SUBCASE("the hello goes first; frames wait for the other side's hello") {
    Clock clk;
    FakeHost h(&clk);
    auto l = create_link("stream", R"({"preset":"nearby"})", h, &err);
    REQUIRE(l);
    l->on_io_state(true);
    REQUIRE(h.writes.size() == 1);
    CHECK(h.writes[0] == hello);
    REQUIRE(l->send(ByteView(Bytes{1, 2, 3}), 0));
    l->tick(clk.t);
    CHECK(h.writes.size() == 1);  // nothing before the peer proved it speaks EmberMesh
    // the peer's hello split across two reads, followed directly by a framed packet
    FakeHost peer_h(&clk);
    auto peer = create_link("stream", R"({"preset":"cable"})", peer_h, &err);
    peer->on_io_state(true);
    REQUIRE(peer->send(ByteView(Bytes{9, 8, 7}), 0));
    Bytes rest(hello.begin() + 3, hello.end());
    rest.insert(rest.end(), peer_h.writes[0].begin(), peer_h.writes[0].end());
    l->on_io_bytes(ByteView(hello).sub(0, 3));
    l->on_io_bytes(ByteView(rest));
    REQUIRE(h.frames.size() == 1);
    CHECK(h.frames[0].data == (Bytes{9, 8, 7}));
    l->tick(clk.t);
    CHECK(h.writes.size() == 2);  // the queued frame now goes out
    CHECK(parse_json(l->status_json()).get_string("hello") == "ok");
    clk.t += 5000;
    l->tick(clk.t);
    CHECK(l->state() == LinkState::Up);
  }
  SUBCASE("no hello within 2 s fails the link") {
    Clock clk;
    FakeHost h(&clk);
    auto l = create_link("stream", R"({"preset":"nearby"})", h, &err);
    REQUIRE(l);
    l->on_io_state(true);
    clk.t += 1900;
    l->tick(clk.t);
    CHECK(l->state() == LinkState::Up);
    clk.t += 200;
    l->tick(clk.t);
    CHECK(l->state() == LinkState::Error);
    CHECK(parse_json(l->status_json()).get_string("hello") == "failed");
  }
  SUBCASE("anything else (a web server, a printer) fails at the first wrong byte") {
    Clock clk;
    FakeHost h(&clk);
    auto l = create_link("stream", R"({"preset":"nearby"})", h, &err);
    REQUIRE(l);
    l->on_io_state(true);
    REQUIRE(l->send(ByteView(Bytes{1}), 0));
    std::string get = "GET / HTTP/1.1\r\n";
    l->on_io_bytes(ByteView(reinterpret_cast<const uint8_t*>(get.data()), get.size()));
    CHECK(l->state() == LinkState::Error);
    l->on_io_bytes(ByteView(hello));  // too late: stays failed, nothing delivered or sent
    l->tick(clk.t + 10);
    CHECK(h.frames.empty());
    CHECK(h.writes.size() == 1);
  }
  SUBCASE("other presets send no hello") {
    Clock clk;
    FakeHost h(&clk);
    auto l = create_link("stream", R"({"preset":"cable"})", h, &err);
    l->on_io_state(true);
    CHECK(h.writes.empty());
  }
}

}  // TEST_SUITE
