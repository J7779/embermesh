// Slow shared half-duplex channels with REAL audio links (AFSK1200 and the robust MFSK modes):
// first message between strangers (no keys exchanged, announce_interval 0 = node setting
// beacon_interval_s:0), collisions from reactive keying, repair of a lost fresh message, airtime.
// Uses tests/mesh/audio_air.h (summed audio, keyed radios are deaf, modem DCD latency is real).
//
// Long sweep with before/after delivery-time distributions (MeshConfig::slow_channel_etiquette
// off = previous behaviour):   ./ember_tests -ts=mesh -tc="air long*" --no-skip
#include <algorithm>
#include <set>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "doctest.h"
#include "mesh/audio_air.h"

using namespace air;

namespace {

struct Outcome {
  uint64_t ms = UINT64_MAX;     // until every other node delivered the message
  uint64_t overlap_ms = 0;      // two or more radios keyed at once, from send to delivery
  double airtime_s = 0;         // keyed time of all nodes, from send to delivery
  double idle_duty = 0;         // highest per-node keyed fraction over a fixed window from start-up
  bool unverified = false;      // some receiver got it flagged unauthenticated
  size_t copies_max = 0;        // most deliveries of the message at one node
  uint64_t repairs = 0;
};

MeshConfig slow_cfg(bool etiquette) {
  MeshConfig c;
  c.announce_interval_s = 0;  // node setting beacon_interval_s: 0
  c.slow_channel_etiquette = etiquette;
  return c;
}

// Strangers on one channel; node 0 sends one public broadcast `send_after_ms` after start-up.
// jam_first: the first key-up of node 0 after the send is destroyed by an interferer.
Outcome first_message(const std::string& link_cfg, int count, uint64_t seed, bool etiquette, bool jam_first,
                      uint64_t max_ms, uint64_t idle_ms = 0, uint64_t send_after_ms = 3000) {
  AirNet net(seed, link_cfg);
  const uint64_t t_start = net.now();
  for (int i = 0; i < count; ++i) net.add("n" + std::to_string(i), slow_cfg(etiquette));
  REQUIRE(net.error.empty());
  net.run(send_after_ms);
  const std::string text = "first contact " + std::to_string(seed);
  // The jam destroys the whole key-up that carries the text: the first one of node 0 that starts
  // after the engine handed the text frame to the driver (a real loss at every receiver).
  bool was_keyed = net.nodes[0]->keyed, started = false, done = false;
  size_t sent0 = net.nodes[0]->trace->sent.size();
  auto text_handed = [&](AirNet& n) {
    auto& v = n.nodes[0]->trace->sent;
    for (size_t i = sent0; i < v.size(); ++i)
      if (v[i].kind == "text") return true;
    return false;
  };
  bool handed_before_key = false;
  if (jam_first)
    net.jam = [&](AirNet& n) {
      bool k = n.nodes[0]->keyed;
      if (!k) handed_before_key = handed_before_key || text_handed(n);
      if (k && !was_keyed && !done && handed_before_key) started = true;
      if (!k && was_keyed && started) { started = false; done = true; }
      was_keyed = k;
      return started;
    };
  SendResult r = net.nodes[0]->eng->send_text(Dest::broadcast(), text);
  REQUIRE(r.ok);
  Outcome o;
  uint64_t ov0 = net.overlap_steps;
  std::vector<uint64_t> tx0;
  for (auto& n : net.nodes) tx0.push_back(n->tx_samples);
  o.ms = net.run_until([&] {
    for (int i = 1; i < count; ++i)
      if (!net.nodes[static_cast<size_t>(i)]->rec.got_text(text)) return false;
    return true;
  }, max_ms);
  o.overlap_ms = (net.overlap_steps - ov0) * AirNet::kStepMs;
  for (size_t i = 0; i < net.nodes.size(); ++i)
    o.airtime_s += static_cast<double>(net.nodes[i]->tx_samples - tx0[i]) / 8000.0;
  if (jam_first) CHECK(done);
  net.jam = nullptr;
  if (idle_ms) {
    // Same absolute window for every run (start-up convergence included), so before/after compare.
    if (net.now() - t_start < idle_ms) net.run(idle_ms - (net.now() - t_start));
    double win_s = static_cast<double>(net.now() - t_start) / 1000.0;
    for (auto& n : net.nodes) o.idle_duty = std::max(o.idle_duty, net.airtime_s(*n) / win_s);
  }
  for (auto& n : net.nodes) {
    o.copies_max = std::max(o.copies_max, n->rec.count_text(text));
    for (auto& t : n->rec.texts)
      if (t.body.text == text && !t.rx.authenticated) o.unverified = true;
    for (auto& ls : n->eng->stats().per_link) o.repairs += ls.repairs;
  }
  return o;
}

struct Dist {
  std::vector<uint64_t> v;
  size_t fails = 0;
  double air = 0, duty = 0;
  uint64_t overlap = 0;
  void add(const Outcome& o) {
    if (o.ms == UINT64_MAX) ++fails;
    else v.push_back(o.ms);
    air += o.airtime_s;
    duty = std::max(duty, o.idle_duty);
    overlap += o.overlap_ms;
  }
  uint64_t pct(double p) {
    if (v.empty()) return UINT64_MAX;
    std::sort(v.begin(), v.end());
    return v[static_cast<size_t>(p * static_cast<double>(v.size() - 1) + 0.5)];
  }
  std::string str() {
    char b[240];
    size_t n = v.size() + fails;
    std::snprintf(b, sizeof b,
                  "runs=%zu undelivered=%zu median=%.1fs p90=%.1fs max=%.1fs | air/run %.1fs, overlap/run %.2fs, "
                  "duty max %.2f%% (first 15 min)",
                  n, fails, pct(0.5) / 1000.0, pct(0.9) / 1000.0, pct(1.0) / 1000.0, n ? air / n : 0.0,
                  n ? overlap / 1000.0 / n : 0.0, duty * 100);
    return b;
  }
};

const char* kAfsk = R"({"modem":"afsk1200","licence":"licence_free"})";
const char* kRobust250 = R"({"modem":"robust250","licence":"licence_free"})";
const char* kRobust62 = R"({"modem":"robust62","licence":"licence_free"})";

struct Discovery {
  uint64_t ms = UINT64_MAX;  // from the last node's start until every node holds every other's keys
  size_t announces_max = 0;  // most distinct own announcements one node originated
  size_t relays_max = 0;     // most relayed announcements one node put on the air
  double duty_max = 0;       // highest per-node keyed fraction from the first start to the end
};

// Nodes start one after another with the given offsets (ms after the previous one), as when
// daemons / apps are switched on in some order. No user action, announce_interval 0.
Discovery discover(const std::string& cfg, const std::vector<uint64_t>& gaps, uint64_t seed, uint64_t max_ms,
                   uint64_t window_ms = 600000) {
  AirNet net(seed, cfg);
  uint64_t t0 = net.now();
  for (size_t i = 0; i < gaps.size(); ++i) {
    net.run(gaps[i]);
    net.add("n" + std::to_string(i), slow_cfg(true));
    REQUIRE(net.error.empty());
  }
  auto all = [&] {
    for (auto& a : net.nodes)
      for (auto& b : net.nodes)
        if (a != b && !a->eng->has_keys(b->eng->node_id())) return false;
    return true;
  };
  Discovery d;
  d.ms = net.run_until(all, max_ms);
  // Duty over the first `window_ms` from the first start-up (no user traffic at all).
  if (net.now() - t0 < window_ms) net.run(window_ms - (net.now() - t0));
  double span_s = static_cast<double>(net.now() - t0) / 1000.0;
  for (auto& n : net.nodes) {
    size_t rel = 0;
    std::set<std::string> own;
    for (auto& x : n->trace->sent) {
      if (x.kind.rfind("announce#", 0) == 0) own.insert(x.kind);
      rel += x.kind == "announce(relay)";
    }
    size_t ann = own.size();
    d.announces_max = std::max(d.announces_max, ann);
    d.relays_max = std::max(d.relays_max, rel);
    d.duty_max = std::max(d.duty_max, net.airtime_s(*n) / span_s);
  }
  return d;
}

}  // namespace

TEST_SUITE("mesh") {

TEST_CASE("air: first message between strangers on a 1200 bit/s channel arrives fast, verified, once") {
  for (int count : {2, 3, 5}) {
    for (uint64_t seed : {1u, 2u}) {
      if (count == 5 && seed == 2) continue;  // keep the default suite fast
      CAPTURE(count);
      CAPTURE(seed);
      Outcome o = first_message(kAfsk, count, seed, true, false, 120000);
      MESSAGE(count << " nodes seed " << seed << ": " << o.ms << " ms, overlap " << o.overlap_ms << " ms");
      REQUIRE(o.ms != UINT64_MAX);
      CHECK(o.ms < 30000);   // before the fix: median 36 s, up to 52 s (3 nodes)
      CHECK(!o.unverified);  // the piggy-backed announcement verified it, no WHOIS round trip
      CHECK(o.copies_max == 1);
    }
  }
}

TEST_CASE("air: a fresh message lost to a collision is repaired within seconds, not a sync gap") {
  for (uint64_t seed : {1u, 2u}) {
    CAPTURE(seed);
    Outcome o = first_message(kAfsk, 2, seed, true, true, 180000);
    MESSAGE("jammed first copy, seed " << seed << ": " << o.ms << " ms, repairs " << o.repairs);
    REQUIRE(o.ms != UINT64_MAX);
    CHECK(o.ms < 40000);  // before: waits for the 120 s slow-link sync gap
    CHECK(o.repairs >= 1);
    CHECK(o.repairs <= 2);
    CHECK(o.copies_max == 1);
  }
}

TEST_CASE("air: robust250 (250 bit/s MFSK) first message between strangers") {
  Outcome o = first_message(kRobust250, 2, 1, true, false, 300000);
  MESSAGE("robust250 2 nodes: " << o.ms << " ms");
  REQUIRE(o.ms != UINT64_MAX);
  CHECK(o.ms < 120000);
  CHECK(!o.unverified);
}

// One modem per case; EMBER_AIR_COUNTS (e.g. "2,3,5") and EMBER_AIR_SEEDS (count) choose the sweep.
void long_sweep(const char* name, const char* cfg, uint64_t max_ms, int default_seeds) {
  std::vector<int> counts = {2, 3, 5};
  if (const char* c = std::getenv("EMBER_AIR_COUNTS")) {
    counts.clear();
    for (const char* p = c; *p; ++p)
      if (*p >= '1' && *p <= '9') counts.push_back(*p - '0');
  }
  int seeds = default_seeds;
  if (const char* sd = std::getenv("EMBER_AIR_SEEDS")) seeds = std::atoi(sd);
  for (int count : counts) {
    for (bool jam : {false, true}) {
      Dist before, after;
      for (int s = 0; s < seeds; ++s) {
        uint64_t seed = 1000 + static_cast<uint64_t>(s);
        before.add(first_message(cfg, count, seed, false, jam, max_ms, s == 0 ? 900000 : 0));
        Outcome o = first_message(cfg, count, seed, true, jam, max_ms, s == 0 ? 900000 : 0);
        after.add(o);
        CHECK(o.ms != UINT64_MAX);
        CHECK(o.copies_max <= 1);
      }
      std::printf("%-9s %d nodes%s\n  before: %s\n  after:  %s\n", name, count, jam ? " (first copy jammed)" : "",
                  before.str().c_str(), after.str().c_str());
      std::fflush(stdout);
      CHECK(after.fails == 0);
      // Airtime over the first 15 minutes (start-up convergence, the message, housekeeping) is no
      // worse than before; absolute budgets are checked by "air long duty".
      CHECK(after.duty <= before.duty + 0.05);
    }
  }
}

TEST_CASE("air: stations started in either order or at random times discover each other") {
  // Two stations, each order (the second one starts 20 / 45 s later), then 3 and 5 at random offsets.
  struct C { std::vector<uint64_t> gaps; uint64_t seed; uint64_t bound_ms; };
  const C cases[] = {{{0, 20000}, 1, 40000}, {{0, 45000}, 2, 40000}, {{0, 3000, 61000}, 3, 90000},
                     {{0, 17000, 5000, 40000, 2000}, 4, 150000}};
  for (const C& c : cases) {
    CAPTURE(c.gaps.size());
    CAPTURE(c.seed);
    Discovery d = discover(kAfsk, c.gaps, c.seed, 240000, 180000);
    MESSAGE(c.gaps.size() << " nodes seed " << c.seed << ": all keys after " << d.ms << " ms, max own announces "
                          << d.announces_max << ", max relayed " << d.relays_max << ", max duty " << d.duty_max * 100
                          << " % (first 3 min)");
    CHECK(d.ms != UINT64_MAX);
    CHECK(d.ms < c.bound_ms);
    CHECK(d.announces_max <= 4);  // start-up + discovery replies, coalesced: no storm
    // Start-up convergence is a burst (announcements of every newcomer reconciled to the others);
    // the 10 % budget over 10 minutes is checked by "air long discovery".
    CHECK(d.duty_max < (c.gaps.size() >= 5 ? 0.20 : 0.10));
  }
}

TEST_CASE("air: no discovery announcement for a newcomer that already has our keys") {
  for (int knows : {1, 0}) {
    CAPTURE(knows);
    AirNet net(91 + static_cast<uint64_t>(knows), kAfsk);
    AirNode& a = net.add("a", slow_cfg(true));
    net.run(30000);  // a's start-up announcement is long over
    auto own_announces = [&] {
      std::set<std::string> own;
      for (auto& x : a.trace->sent)
        if (x.kind.rfind("announce#", 0) == 0) own.insert(x.kind);
      return own.size();
    };
    size_t before = own_announces();
    AirNode& b = net.add("b", slow_cfg(true));
    net.run(50);  // b's engine starts
    if (knows) {
      // b already has a's card and writes to a straight away: proof that it holds a's keys.
      REQUIRE(b.eng->add_known_keys(net.nodes[0]->id.public_keys(), "a"));
      auto r = b.eng->send_text(Dest::to_node(net.nodes[0]->eng->node_id()), "hello a");
      CHECK(r.ok);
    } else {
      auto r = b.eng->send_text(Dest::broadcast(), "hello all");
      CHECK(r.ok);
    }
    net.run(60000);
    size_t replies = own_announces() - before;
    MESSAGE("newcomer knows us: " << knows << ", discovery announcements from a: " << replies);
    if (knows) {
      CHECK(net.nodes[0]->rec.got_text("hello a"));
      CHECK(replies == 0);
    } else {
      CHECK(replies == 1);
    }
  }
}

TEST_CASE("air long discovery: random start orders and offsets, 2/3/5 nodes, afsk1200 and robust250" * doctest::skip()) {
  struct M { const char* name; const char* cfg; uint64_t max_ms; int seeds; };
  const M modes[] = {{"afsk1200", kAfsk, 600000, 12}, {"robust250", kRobust250, 1800000, 4}};
  const char* only = std::getenv("EMBER_AIR_MODEM");
  std::vector<int> counts = {2, 3, 5};
  if (const char* c = std::getenv("EMBER_AIR_COUNTS")) {
    counts.clear();
    for (const char* p = c; *p; ++p)
      if (*p >= '1' && *p <= '9') counts.push_back(*p - '0');
  }
  for (const M& m : modes) {
    if (only && std::string(only) != m.name) continue;
    for (int count : counts) {
      Dist d;
      size_t ann = 0, rel = 0;
      double duty = 0;
      for (int s = 0; s < m.seeds; ++s) {
        uint64_t seed = 2000 + static_cast<uint64_t>(s);
        sim::Rng r(seed);
        std::vector<uint64_t> gaps;
        for (int i = 0; i < count; ++i) gaps.push_back(i == 0 ? 0 : r.below(60000));
        Discovery x = discover(m.cfg, gaps, seed, m.max_ms, 1800000);  // duty over 30 min
        // Bounded, not a storm: the start-up announcement, at most one discovery reply per minute
        // per link while newcomers keep appearing (+1 for the last), at most one WHOIS answer.
        uint64_t spread = 0;
        for (uint64_t g : gaps) spread += g;
        if (std::getenv("EMBER_AIR_VERBOSE"))
          std::printf("  seed %llu spread %.0f s: keys after %.1f s, own announces %zu\n", (unsigned long long)seed,
                      spread / 1000.0, x.ms / 1000.0, x.announces_max);
        CHECK(x.announces_max <= 3 + (spread + 59999) / 60000);
        Outcome o;
        o.ms = x.ms;
        d.add(o);
        ann = std::max(ann, x.announces_max);
        rel = std::max(rel, x.relays_max);
        duty = std::max(duty, x.duty_max);
        CHECK(x.ms != UINT64_MAX);
      }
      std::printf("discovery %-9s %d nodes: %s; max own announces %zu, max relayed %zu, max duty %.2f%% (30 min)\n",
                  m.name, count, d.str().c_str(), ann, rel, duty * 100);
      std::fflush(stdout);
      CHECK(d.fails == 0);
      // Regulatory duty cycles are hourly: the whole start-up convergence fits a 10 % budget
      // (360 s of airtime per hour) with room to spare.
      CHECK(duty * 1800.0 <= 360.0);
    }
  }
}

TEST_CASE("air long afsk1200: delivery-time distributions before/after" * doctest::skip()) {
  long_sweep("afsk1200", kAfsk, 300000, 20);
}
TEST_CASE("air long robust250: delivery-time distributions before/after" * doctest::skip()) {
  long_sweep("robust250", kRobust250, 900000, 6);
}
TEST_CASE("air long robust62: delivery-time distributions before/after" * doctest::skip()) {
  long_sweep("robust62", kRobust62, 1800000, 3);
}

TEST_CASE("air: a 10 % duty-cycle budget holds on the slow audio path under heavy offered load") {
  // Everything the engine sends (user traffic, piggy-backed announcements, repairs, discovery
  // replies, beacons, sync) goes through the per-link token bucket: at 100 permille it starts with
  // 10 minutes' worth (60 s) and earns 0.1 ms per ms. Offer far more than that and check the cap.
  AirNet net(77, R"({"modem":"afsk1200","licence":"licence_free","duty_cycle_permille":100})");
  for (int i = 0; i < 2; ++i) net.add("n" + std::to_string(i), slow_cfg(true));
  net.run(2000);
  for (int k = 0; k < 60; ++k)
    net.nodes[0]->eng->send_text(Dest::broadcast(), "budget test message number " + std::to_string(k) +
                                                         " with enough text to take real airtime on a slow link");
  const uint64_t span_ms = 150000;
  net.run(span_ms);
  double sent = net.airtime_s(*net.nodes[0]);
  double allowed = 60.0 + 0.1 * (span_ms + 2000) / 1000.0;
  MESSAGE("keyed " << sent << " s of an allowed " << allowed << " s");
  CHECK(sent <= allowed + 3.0);  // + one frame in flight when the bucket ran dry
  CHECK(sent > 0.6 * allowed);    // the load really hit the budget (it is not trivially idle)
}

TEST_CASE("air long duty: one hour on a 10 % duty-cycle 1200 bit/s channel, 3 nodes" * doctest::skip()) {
  for (uint64_t seed : {1000u, 1001u, 1002u}) {
    AirNet net(seed, R"({"modem":"afsk1200","licence":"licence_free","duty_cycle_permille":100})");
    for (int i = 0; i < 3; ++i) net.add("n" + std::to_string(i), slow_cfg(true));
    net.run(3000);
    const std::string text = "duty " + std::to_string(seed);
    net.nodes[0]->eng->send_text(Dest::broadcast(), text);
    uint64_t t = net.run_until([&] {
      return net.nodes[1]->rec.got_text(text) && net.nodes[2]->rec.got_text(text);
    }, 300000);
    net.run(3600000 - (net.now() - 1767225600000ull));
    double worst = 0;
    for (auto& n : net.nodes) worst = std::max(worst, net.airtime_s(*n));
    std::printf("duty seed %llu: delivered in %.1f s; most keyed node %.1f s in 1 h (budget 360 s)\n",
                (unsigned long long)seed, t / 1000.0, worst);
    std::fflush(stdout);
    CHECK(t != UINT64_MAX);
    CHECK(worst <= 360.0);
  }
}

TEST_CASE("air trace: print the timeline of one first-contact run" * doctest::skip()) {
  AirNet net(2, kAfsk);
  for (int i = 0; i < 2; ++i) net.add("n" + std::to_string(i), slow_cfg(true));
  uint64_t t0 = net.now();
  net.run(3000);
  net.nodes[0]->eng->send_text(Dest::broadcast(), "hello");
  uint64_t got = net.run_until([&] { return net.nodes[1]->rec.got_text("hello"); }, 120000);
  for (auto& n : net.nodes)
    for (auto& s : n->trace->sent)
      std::printf("%8.2f %s send %-9s %zu B\n", (s.ms - t0) / 1000.0, n->name.c_str(), s.kind.c_str(), s.bytes);
  std::printf("delivered after %llu ms (sent at 3.00 s)\n", static_cast<unsigned long long>(got));
}

TEST_CASE("air probe: per-seed times (debug)" * doctest::skip()) {
  for (int jam = 0; jam < 2; ++jam)
    for (uint64_t seed = 1000; seed < 1020; ++seed) {
      Outcome o = first_message(kAfsk, 2, seed, true, jam != 0, 300000);
      std::printf("jam %d seed %llu: %llu ms repairs %llu\n", jam, (unsigned long long)seed, (unsigned long long)o.ms,
                  (unsigned long long)o.repairs);
    }
}

TEST_CASE("air trace seed: timeline for EMBER_AIR_SEED / EMBER_AIR_JAM (debug)" * doctest::skip()) {
  const char* se = std::getenv("EMBER_AIR_SEED");
  uint64_t seed = se ? std::strtoull(se, nullptr, 10) : 1000;
  bool jam_first = std::getenv("EMBER_AIR_JAM") != nullptr;
  const char* mm = std::getenv("EMBER_AIR_MODEM");
  AirNet net(seed, mm ? std::string(R"({"modem":")") + mm + R"(","licence":"licence_free"})" : std::string(kAfsk));
  for (int i = 0; i < 2; ++i) net.add("n" + std::to_string(i), slow_cfg(true));
  uint64_t t0 = net.now();
  net.run(3000);
  bool was_keyed = net.nodes[0]->keyed, started = false, done = false;
  if (jam_first)
    net.jam = [&](AirNet& n) {
      bool k = n.nodes[0]->keyed;
      if (k && !was_keyed && !done) started = true;
      if (!k && was_keyed && started) { started = false; done = true; }
      was_keyed = k;
      return started;
    };
  const std::string text = "first contact " + std::to_string(seed);
  net.nodes[0]->eng->send_text(Dest::broadcast(), text);
  net.trace_events = true;
  uint64_t keys_at = 0;
  NodeId n0id = net.nodes[0]->eng->node_id();
  uint64_t got = net.run_until([&] {
    if (!keys_at && net.nodes[1]->eng->has_keys(n0id)) keys_at = net.now();
    static bool printed = false;
    if (!printed && net.now() - t0 > 46000) {
      printed = true;
      MeshStats st = net.nodes[1]->eng->stats();
      std::printf("n1 @46s: dup %llu ttl %llu exp %llu rate %llu auth %llu delivered %llu rx_bad %llu\n",
                  (unsigned long long)st.dropped_duplicates, (unsigned long long)st.dropped_ttl,
                  (unsigned long long)st.dropped_expired, (unsigned long long)st.dropped_rate,
                  (unsigned long long)st.dropped_auth, (unsigned long long)st.delivered, (unsigned long long)st.rx_bad);
    }
    return net.nodes[1]->rec.got_text(text);
  }, 300000);
  if (keys_at) std::printf("%8.2f n1 learned n0's keys\n", (keys_at - t0) / 1000.0);
  for (auto& n : net.nodes) {
    for (auto& s : n->trace->sent)
      std::printf("%8.2f %s send %-9s %zu B\n", (s.ms - t0) / 1000.0, n->name.c_str(), s.kind.c_str(), s.bytes);
    for (auto& e : n->events)
      if (e.second == "KEY" || e.second == "unkey" || e.second.rfind("rx ", 0) == 0)
        std::printf("%8.2f %s    %s\n", (e.first - t0) / 1000.0, n->name.c_str(), e.second.c_str());
  }
  std::printf("delivered after %llu ms (sent at 3.00 s)\n", static_cast<unsigned long long>(got));
}

TEST_CASE("air idle: what is sent after delivery (debug)" * doctest::skip()) {
  const char* m = std::getenv("EMBER_AIR_MODEM");
  std::string cfg = std::string(R"({"modem":")") + (m ? m : "robust62") + R"(","licence":"licence_free"})";
  for (int et = 0; et < 2; ++et) {
    AirNet net(1000, cfg);
    for (int i = 0; i < 2; ++i) net.add("n" + std::to_string(i), slow_cfg(et != 0));
    net.run(3000);
    net.nodes[0]->eng->send_text(Dest::broadcast(), "x");
    uint64_t t = net.run_until([&] { return net.nodes[1]->rec.got_text("x"); }, 1800000);
    size_t s0 = net.nodes[0]->trace->sent.size(), s1 = net.nodes[1]->trace->sent.size();
    uint64_t a0 = net.nodes[0]->tx_samples + net.nodes[1]->tx_samples;
    uint64_t t0 = net.now();
    net.run(600000);
    std::printf("etiquette %d: delivered %llu ms; idle 600 s keyed %.1f s (both nodes)\n", et,
                (unsigned long long)t, (net.nodes[0]->tx_samples + net.nodes[1]->tx_samples - a0) / 8000.0);
    size_t idx[2] = {s0, s1};
    for (int n = 0; n < 2; ++n)
      for (size_t i = idx[n]; i < net.nodes[static_cast<size_t>(n)]->trace->sent.size(); ++i) {
        auto& x = net.nodes[static_cast<size_t>(n)]->trace->sent[i];
        std::printf("  %7.1f n%d %-9s %zu B\n", (x.ms - t0) / 1000.0, n, x.kind.c_str(), x.bytes);
      }
  }
}

}  // TEST_SUITE
