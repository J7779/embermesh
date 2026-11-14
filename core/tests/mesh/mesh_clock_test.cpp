// Mesh engine: host clock jumps (backward, forward, repeated, now_ms == 0). Regression for
// MeshEngine::tick subtracting a backward jump from its offset on every tick while the host clock
// stayed behind (mesh time ran weeks into the past), and for timers stalling after a jump back.
#include "doctest.h"
#include "mesh/sim.h"

using namespace sim;

namespace {

constexpr uint64_t kT0 = 1767225600000ull;  // 2026-01-01
constexpr uint64_t kDay = 86400000ull;

std::unique_ptr<MeshEngine> lone_engine() {
  std::string seed = "clock-test-node-padding-to-32-bytes-long";
  crypto::Csprng r{ByteView(std::string_view(seed))};
  Identity id = Identity::generate(r);
  Bytes ent = r.bytes(32);
  return std::make_unique<MeshEngine>(id, ent, Sim::default_config());
}

}  // namespace

TEST_SUITE("mesh") {

TEST_CASE("clock: system time follows the host clock exactly through jumps; timers stay monotonic") {
  auto e = lone_engine();
  e->tick(kT0);
  CHECK(e->now_ms() == kT0);
  CHECK(e->time_info().source == TimeSource::System);
  uint64_t raw = kT0, forward = 0;
  auto step = [&](int64_t d) {
    uint64_t prev = raw;
    raw = static_cast<uint64_t>(static_cast<int64_t>(raw) + d);
    if (raw > prev) forward += raw - prev;
    e->tick(raw);
    REQUIRE(e->now_ms() == raw);                 // mesh time == system clock
    REQUIRE(e->stats().uptime_ms == forward);    // timer clock: forward steps only
  };
  for (int i = 0; i < 10; ++i) step(20);
  step(-static_cast<int64_t>(kDay));             // back one day ...
  for (int i = 0; i < 200; ++i) step(20);        // ... applied once, not once per tick
  CHECK(e->now_ms() == kT0 - kDay + 200 + 4000);
  step(3600000);                                 // forward an hour
  for (int i = 0; i < 50; ++i) step(20);
  // repeated jumps both ways, including small ones and a jump back past the previous jump
  const int64_t jumps[] = {-1, 1, -1000, -7 * static_cast<int64_t>(kDay), 30 * static_cast<int64_t>(kDay), -5, 0, 0, 999};
  for (int64_t j : jumps) {
    step(j);
    for (int i = 0; i < 20; ++i) step(10);
  }
  // now_ms == 0 = "no clock reading": nothing moves, and the next real reading continues normally
  uint64_t before = e->now_ms(), up = e->stats().uptime_ms;
  for (int i = 0; i < 20; ++i) e->tick(0);
  CHECK(e->now_ms() == before);
  CHECK(e->stats().uptime_ms == up);
  step(25);
  CHECK(e->now_ms() == before + 25);
  CHECK(e->next_wakeup_ms() <= 60000);
}

TEST_CASE("clock: GPS / manual time ignores host clock steps backward, keeps counting forward") {
  auto e = lone_engine();
  e->tick(kT0);
  const uint64_t gps = kT0 + 123456;
  e->set_time(gps, TimeSource::Gps);
  uint64_t raw = kT0;
  for (int i = 0; i < 10; ++i) e->tick(raw += 100);
  CHECK(e->now_ms() == gps + 1000);
  e->tick(raw -= 3 * kDay);                      // host clock corrected backwards
  for (int i = 0; i < 100; ++i) e->tick(raw += 10);
  CHECK(e->now_ms() == gps + 2000);
  e->tick(0);
  CHECK(e->now_ms() == gps + 2000);
  CHECK(e->time_info().source == TimeSource::Gps);
}

TEST_CASE("clock: an engine started at now_ms == 0 starts and picks up a later real clock") {
  auto e = lone_engine();
  e->tick(0);
  CHECK(e->time_info().quality == 15);
  e->tick(0);
  e->tick(5000);
  CHECK(e->now_ms() == 5000);
  e->tick(kT0);
  CHECK(e->now_ms() == kT0);
  // mesh time never underflows below 0 even after an absurd backward step
  e->tick(1);
  CHECK(e->now_ms() == 1);
}

TEST_CASE("clock: messages flow and retries fire after a node's clock jumps back a day") {
  Sim s(77);
  Node& a = s.add_node("a");
  Node& b = s.add_node("b");
  Medium& lan = s.add_medium(lan_caps());
  s.join(a, lan);
  s.join(b, lan);
  s.introduce(a, b);
  s.run(2000);
  a.clock_skew_ms = -static_cast<int64_t>(kDay);
  b.clock_skew_ms = 3600000;
  s.run(3000);
  auto skew = [&](Node& n) { return static_cast<int64_t>(n.eng->now_ms()) - static_cast<int64_t>(s.now()); };
  CHECK(skew(a) == -static_cast<int64_t>(kDay));
  CHECK(skew(b) == 3600000);
  SendResult r = a.eng->send_text(Dest::to_node(b.nid()), "after the jump");
  REQUIRE(r.ok);
  CHECK(s.run_until([&] { return b.rec.got_text("after the jump"); }, 10000) != UINT64_MAX);
  CHECK(s.run_until([&] { return a.rec.status_of(r.id) == MsgStatus::Delivered; }, 30000) != UINT64_MAX);
  CHECK(skew(a) == -static_cast<int64_t>(kDay));  // still exactly one day, not drifting
}

}  // TEST_SUITE
