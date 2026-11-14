// Node: time handling — clock jumps backward / forward, ticks with now_ms = 0, nodes created
// without a clock, next_wakeup sanity, manual / GPS time.
#include "node/node_harness.h"

using namespace nt;

TEST_SUITE("node") {

TEST_CASE("time: next_wakeup is sane and reflects real pending work") {
  Net net(400);
  int lan = net.add_medium();
  TNode a("", 400), b("", 401);
  net.join(a, lan);
  net.join(b, lan);
  net.run(65000, 100);  // let announcements, beacons and the first sweep settle
  uint32_t idle = ember_next_wakeup_ms(a.n);
  CHECK(idle <= 60000);
  CHECK(idle > 0);
  a.ok("messages.send", J(R"({"conv":"ch:public","text":"wake up"})"));
  CHECK(ember_next_wakeup_ms(a.n) == 0);  // events wait in the outbox
  a.drain();
  CHECK(ember_next_wakeup_ms(a.n) <= 1000);  // storage flush / transmit pending
  // Idle node without links sleeps long.
  TNode lonely("", 402);
  for (int i = 0; i < 5; ++i) {
    ember_tick(lonely.n, kStart + 70000u * static_cast<unsigned>(i + 1));
    lonely.drain();
  }
  uint32_t w = ember_next_wakeup_ms(lonely.n);
  MESSAGE("idle wakeups: two-node mesh " << idle << " ms, lonely node " << w << " ms");
  CHECK(w >= 1000);
  CHECK(w <= 60000);
}

TEST_CASE("time: backward and forward clock jumps, ticks with now_ms = 0") {
  Net net(410);
  int lan = net.add_medium();
  TNode a("", 410), b("", 411);
  net.join(a, lan);
  net.join(b, lan);
  introduce(a, b);
  net.run(2000);
  // a's host clock jumps back a day; b's forward an hour; plus zero ticks.
  uint64_t base = net.now;
  for (int i = 0; i < 50; ++i) {
    ember_tick(a.n, base - 86400000ull + static_cast<uint64_t>(i) * 20);
    ember_tick(b.n, base + 3600000ull + static_cast<uint64_t>(i) * 20);
    ember_tick(a.n, 0);
    a.drain();
    b.drain();
  }
  // Mesh time follows the system clock back by the jump once — not once per tick (regression:
  // MeshEngine::tick subtracts the jump from its offset on every tick while the clock stays behind).
  int64_t skew = a.ok("node.info").get_int("time_ms") - static_cast<int64_t>(base);
  CHECK(skew > -86400000ll - 5000);
  CHECK(skew < -86400000ll + 5000);
  Json m = a.ok("messages.send", Json::object({{"conv", "dm:" + b.id}, {"text", "time travel"}})).get("message");
  CHECK(m.get_int("ts") > 0);
  REQUIRE(net.run_until([&] { return b.has_text("dm:" + a.id, "time travel"); }, 20000));
  REQUIRE(net.run_until([&] { return a.find_message("dm:" + b.id, m.get_string("id")).get_string("status") == "delivered"; }, 60000));
  // A huge forward jump (device asleep for days) does not break anything.
  net.now += 5 * 86400000ull;
  net.run(5000);
  a.ok("messages.send", J(R"({"conv":"ch:public","text":"after the jump"})"));
  REQUIRE(net.run_until([&] { return b.has_text("ch:public", "after the jump"); }, 20000));
}

TEST_CASE("time: a node created without a clock starts its engine on the first tick") {
  uint8_t ent[32] = {9};
  ember_node* n = ember_create(R"({"storage_dir":""})", ent, sizeof ent);
  REQUIRE(n);
  char* r = ember_call(n, "messages.send", R"({"conv":"ch:public","text":"too early"})");
  std::optional<Json> j = Json::parse(r);
  ember_free(r);
  CHECK(j->get("error").get_string("code") == "busy");
  r = ember_call(n, "messages.list", R"({"conv":"ch:public"})");  // storage works without a clock
  CHECK(Json::parse(r)->get_bool("ok"));
  ember_free(r);
  ember_tick(n, 0);  // host has no idea of the time: the core still starts
  r = ember_call(n, "messages.send", R"({"conv":"ch:public","text":"now"})");
  CHECK(Json::parse(r)->get_bool("ok"));
  ember_free(r);
  CHECK(ember_next_wakeup_ms(n) <= 60000);
  ember_destroy(n);
}

TEST_CASE("time: GPS / manual time and time.updated") {
  TNode a("", 420);
  a.ok("node.set_time", Json::object({{"ts", kStart + 7200000}, {"source", "manual"}}));
  a.drain();
  const Ev* e = a.last("time.updated");
  REQUIRE(e);
  CHECK(e->data.get("time_ms").is_int());
  Json info = a.ok("node.info");
  CHECK(info.get_string("time_source") == "manual");
  CHECK(info.get_int("time_ms") >= static_cast<int64_t>(kStart + 7000000));
}

}  // TEST_SUITE

TEST_SUITE("node") {

// Battery: a host that sleeps for next_wakeup_ms between ticks must not spin when idle,
// including during the first minute after start and with links attached.
TEST_CASE("time: an idle host following next_wakeup ticks only a handful of times per minute") {
  struct Case { std::string name; std::vector<std::string> links; bool connect; };
  const Case cases[] = {{"no links", {}, false},
                        {"raw udp, connected", {R"({"type":"raw","config":{"preset":"udp_lan"}})"}, true},
                        {"hub websocket, connected", {R"({"type":"raw","config":{"preset":"websocket"}})"}, true},
                        {"raw, endpoint never opened", {R"({"type":"raw","config":{}})"}, false},
                        {"two links", {R"({"type":"raw","config":{"preset":"websocket"}})", R"({"type":"raw","config":{"preset":"udp_lan"}})"}, true}};
  for (const Case& c : cases) {
    CAPTURE(c.name);
    TNode a("", 600);
    for (const std::string& l : c.links) {
      Json lk = a.ok("links.add", J(l.c_str())).get("link");
      if (c.connect) a.ok("links.endpoint_state", Json::object({{"id", lk.get_int("id")}, {"connected", true}}));
    }
    uint64_t t = kStart;
    ember_tick(a.n, t);
    a.drain();
    CHECK(ember_next_wakeup_ms(a.n) > 0);  // nothing to do right after start
    int ticks_first = 0, ticks_later = 0;
    while (t < kStart + 180000) {
      uint32_t w = ember_next_wakeup_ms(a.n);
      t += w ? w : 1;
      ember_tick(a.n, t);
      a.drain();  // a host would write link TX to its endpoints here
      (t < kStart + 60000 ? ticks_first : ticks_later)++;
    }
    MESSAGE(c.name << ": " << ticks_first << " ticks in the first minute, " << ticks_later << " in the next two");
    CHECK(ticks_first <= 40);
    CHECK(ticks_later <= 40);
  }
}

}  // TEST_SUITE
