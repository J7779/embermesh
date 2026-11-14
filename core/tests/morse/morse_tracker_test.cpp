// Whole-frame light tracker (docs/RPC_API.md §12): synthetic scenes rendered at full resolution
// and max/mean-pooled exactly as a capture host does (scene_sim.h) -> Session::feed_frames ->
// tracker -> light decoder. Asserts the transcript (against the sent text and against an oracle: the
// old region reader with a perfectly placed box on the same frames), the time to lock, the target
// position, and that scenes without a keyed light produce no text at all. Every scene is seeded.
// Long sweeps (many seeds, smallest light, cost per frame) are skipped by default:
//   ./ember_tests -ts=morse -tc="tracker long*" --no-skip
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "doctest.h"
#include "morse/light_tracker.h"
#include "morse/rx.h"
#include "morse_sim.h"
#include "scene_sim.h"

using namespace ember;
using namespace morsesim;
using namespace scenesim;

namespace {

// A scene plus the Morse it carries. Timelines live in the case (lights point at them).
struct Case {
  Scene sc;
  std::vector<std::unique_ptr<Timeline>> tls;
  std::string text;      // what the light that should be read sends ("" = nothing)
  int keyed = -1;        // index of that light in sc.lights
  double start = 0, end = 0;
  double duration = 0;   // ms of video
  uint64_t seed = 1;
  const Timeline* add_text(const std::string& t, double wpm, double lead_ms = 1500, double jitter = 0.05) {
    Rng r(seed * 7919 + tls.size() * 131 + 1);
    HandOptions h;
    h.wpm = wpm;
    h.lead_ms = lead_ms;
    h.jitter = jitter;
    tls.emplace_back(new Timeline(hand_key(t, h, r)));
    return tls.back().get();
  }
};

struct Outcome {
  std::string text, oracle;
  double cer = 1, oracle_cer = 1;
  double lock_ms = -1;        // first "locked", from the start of keying
  double first_char_ms = -1;  // first character in the transcript, from the start of keying
  int max_candidates = 0;
  bool ever_locked = false;
  double max_err_cells = 0;   // worst distance target <-> truth, while locked and the light is lit
  morse_rx::Status last;
};

Outcome play(const Case& c, size_t batch = 3, bool verbose = false) {
  const Scene& sc = c.sc;
  Renderer rd(sc, c.seed);
  std::vector<Frame> frames = rd.run(c.duration);
  std::string err;
  auto s = morse_rx::Session::create("light", 0, 0, 0, &err);
  REQUIRE(s);
  Outcome o;
  std::vector<morse_rx::GridFrame> g;
  for (size_t i = 0; i < frames.size(); i += batch) {
    g.clear();
    for (size_t k = i; k < std::min(frames.size(), i + batch); ++k) {
      morse_rx::GridFrame f;
      f.t_ms = frames[k].t;
      f.w = sc.gw;
      f.h = sc.gh;
      f.max = frames[k].max.data();
      f.mean = frames[k].mean.data();
      g.push_back(f);
    }
    s->feed_frames(g.data(), g.size());
    morse_rx::Status st = s->status();
    const Frame& last = frames[std::min(frames.size(), i + batch) - 1];
    o.max_candidates = std::max(o.max_candidates, st.target.candidates);
    if (o.first_char_ms < 0 && st.chars > 0) o.first_char_ms = last.t - c.start;
    if (st.target.state == morse_rx::Target::kLocked) {
      if (!o.ever_locked) o.lock_ms = last.t - c.start;
      o.ever_locked = true;
      // Position is checked while the light is lit (a dark light cannot be seen to move).
      if (c.keyed >= 0 && !last.away) {
        const Light& kl = sc.lights[static_cast<size_t>(c.keyed)];
        double fx, fy;
        truth(sc, kl, last, &fx, &fy);
        bool lit = kl.keyed && lit_fraction(*kl.keyed, last.t - 40, last.t) > 0.99;
        if (lit && fx > 0.02 && fx < 0.98 && fy > 0.02 && fy < 0.98 && last.t - c.start > o.lock_ms + 300) {
          double e = std::hypot((st.target.x - fx) * sc.gw, (st.target.y - fy) * sc.gh);
          o.max_err_cells = std::max(o.max_err_cells, e);
          if (verbose && e > 1.5) std::printf("ERR t=%.0f e=%.1f\n", last.t, e);
        }
      }
    }
    if (verbose && (i / batch) % 10 == 0)
      std::printf("t=%7.0f %s x=%.3f y=%.3f c=%.2f n=%d %s\n", last.t, morse_rx::target_state_name(st.target.state),
                  st.target.x, st.target.y, st.target.confidence, st.target.candidates, st.transcript.c_str());
  }
  o.last = s->status();
  o.text = o.last.transcript;
  o.cer = c.text.empty() ? (normalise(o.text).empty() ? 0 : 1) : cer(c.text, o.text);
  if (c.keyed >= 0 && sc.lights[static_cast<size_t>(c.keyed)].keyed) {
    // Oracle: the section-6 region reader with a perfectly placed box and no noise, same frames.
    auto ref = morse_rx::Session::create("light", 0, 0, 0, &err);
    const Timeline& tl = *sc.lights[static_cast<size_t>(c.keyed)].keyed;
    double exp_ms = std::max(1.0, sc.exposure * 1000.0 / sc.fps);
    for (const Frame& f : frames) ref->feed_times({f.t}, {f.away ? 0.1 : 0.1 + 0.6 * lit_fraction(tl, f.tc - exp_ms, f.tc)});
    o.oracle = ref->status().transcript;
    o.oracle_cer = cer(c.text, o.oracle);
  }
  return o;
}

// ---- scenes ----------------------------------------------------------------------------------

Scene dark_room(double fps) {
  Scene sc;
  sc.fps = fps;
  sc.jitter_ms = 0.08 * 1000 / fps;
  sc.bg = 12;
  sc.texture = 10;
  sc.noise = 2.5;
  sc.shot = 0.3;
  sc.drop = 0.05;
  return sc;
}

Light steady(double x, double y, double amp, double sigma = 1.0) {
  Light l;
  l.x = x;
  l.y = y;
  l.amp = amp;
  l.sigma_px = sigma;
  return l;
}

Light keyed_light(double x, double y, double amp, double sigma, const Timeline* tl) {
  Light l = steady(x, y, amp, sigma);
  l.keyed = tl;
  return l;
}

void finish(Case& c, const Timeline* tl, const std::string& text, double tail_ms = 3000) {
  c.text = text;
  c.start = tl->empty() ? 0 : tl->front().first;
  c.end = end_of(*tl);
  c.duration = c.end + tail_ms;
}

// A distant LED: a 1–2 pixel point (sigma 0.5 / 0.8 px of a 512×288 image, 8×8 pixels per cell)
// that adds `amp` luma at its centre, in a dark room with sensor noise; 5 % dropped frames.
Case tiny(uint64_t seed, double fps, double wpm, const std::string& text, double amp = 55, double sigma = -1) {
  Case c;
  c.seed = seed;
  c.sc = dark_room(fps);
  const Timeline* tl = c.add_text(text, wpm);
  double x = 64 + std::fmod(37.0 * static_cast<double>(seed), 380.0), y = 40 + std::fmod(23.0 * static_cast<double>(seed), 200.0);
  c.sc.lights.push_back(keyed_light(x, y, amp, sigma > 0 ? sigma : (seed % 2 ? 0.8 : 0.5), tl));
  c.keyed = 0;
  finish(c, tl, text);
  return c;
}

// Night street: steady lamps (one saturating), a window, another small light 6 cells away, a mains
// lamp flickering at 100 Hz, headlights crossing, a lit person walking past, auto-exposure pumping.
Case clutter(uint64_t seed, double fps) {
  Case c;
  c.seed = seed;
  c.sc = dark_room(fps);
  c.sc.drop = 0.03;
  c.sc.ae_pump = 0.25;
  c.sc.ae_pump_hz = 0.6;
  const Timeline* tl = c.add_text("CQ CQ DE N0CALL K", 12);
  c.sc.lights.push_back(keyed_light(250.2, 150.7, 70, 0.6, tl));
  c.keyed = 0;
  c.sc.lights.push_back(steady(80, 40, 400, 1.5));
  c.sc.lights.push_back(steady(420, 220, 120, 0.7));
  c.sc.lights.push_back(steady(300, 150, 90, 0.6));
  Light lamp = steady(150, 230, 160, 2.0);
  lamp.flicker = 0.7;
  c.sc.lights.push_back(lamp);
  Light car = steady(-60, 200, 500, 1.5);
  car.vx = 160;
  car.bloom = 0.3;
  car.bloom_px = 4;
  c.sc.lights.push_back(car);
  Light walker = steady(600, 250, 45, 22);
  walker.vx = -90;
  c.sc.lights.push_back(walker);
  finish(c, tl, "CQ CQ DE N0CALL K");
  return c;
}

// Hand-held: random-walk shake of ~4 cells/s RMS and a sudden 5-cell jerk every ~3 s.
Case shaky(uint64_t seed) {
  Case c;
  c.seed = seed;
  c.sc = dark_room(30);
  const Timeline* tl = c.add_text("TEST DE N0CALL", 12);
  c.sc.lights.push_back(keyed_light(256, 144, 70, 0.6, tl));
  c.keyed = 0;
  c.sc.lights.push_back(steady(150, 100, 200, 1.0));
  c.sc.lights.push_back(steady(380, 200, 150, 0.8));
  c.sc.shake_px_s = 32;
  c.sc.jump_every_ms = 3000;
  c.sc.jump_px = 40;
  finish(c, tl, "TEST DE N0CALL");
  return c;
}

// The camera points away for 1.5 s in the middle of the message.
Case dropout(uint64_t seed) {
  Case c;
  c.seed = seed;
  c.sc = dark_room(30);
  const Timeline* tl = c.add_text("CQ CQ CQ DE N0CALL N0CALL K", 12);
  c.sc.lights.push_back(keyed_light(256, 144, 70, 0.6, tl));
  c.keyed = 0;
  c.sc.shake_px_s = 24;
  finish(c, tl, "CQ CQ CQ DE N0CALL N0CALL K");
  double a = c.start + 0.45 * (c.end - c.start);
  c.sc.away.push_back({a, a + 1500});
  return c;
}

// Zoom steps 1x -> 2x -> 5x -> 1x during the message (the capture layer changes what the grid covers).
Case zoomed(uint64_t seed) {
  Case c;
  c.seed = seed;
  c.sc = dark_room(30);
  const Timeline* tl = c.add_text("CQ CQ DE N0CALL N0CALL K", 12);
  c.sc.lights.push_back(keyed_light(276, 152, 70, 0.6, tl));
  c.keyed = 0;
  c.sc.lights.push_back(steady(150, 100, 200, 1.0));
  c.sc.lights.push_back(steady(330, 200, 150, 0.8));
  c.sc.shake_px_s = 12;
  finish(c, tl, "CQ CQ DE N0CALL N0CALL K");
  double span = c.end - c.start;
  c.sc.zoom = {{c.start + 0.3 * span, 2.0}, {c.start + 0.55 * span, 5.0}, {c.start + 0.8 * span, 1.0}};
  return c;
}

// Two senders: a clearer one (read) and a dimmer one that finishes first.
Case two_lights(uint64_t seed) {
  Case c;
  c.seed = seed;
  c.sc = dark_room(30);
  c.sc.drop = 0.03;
  const Timeline* a = c.add_text("CQ CQ DE N0CALL K", 12);
  const Timeline* b = c.add_text("QRZ DE N0CALL", 12, 1300);
  c.sc.lights.push_back(keyed_light(380, 200, 45, 0.5, b));
  c.sc.lights.push_back(keyed_light(120.4, 90.2, 110, 0.5, a));
  c.keyed = 1;
  finish(c, a, "CQ CQ DE N0CALL K");
  return c;
}

// A torch close enough to saturate and bloom over 3–4 cells, auto exposure reacting to it.
Case bloom(uint64_t seed) {
  Case c;
  c.seed = seed;
  c.sc = dark_room(30);
  c.sc.drop = 0.03;
  c.sc.ae_follow = 0.4;
  c.sc.ae_target = 20;
  const Timeline* tl = c.add_text("TEST DE N0CALL", 12);
  Light l = keyed_light(200.5, 100.5, 900, 1.5, tl);
  l.bloom = 0.35;
  l.bloom_px = 9;
  c.sc.lights.push_back(l);
  c.keyed = 0;
  finish(c, tl, "TEST DE N0CALL");
  return c;
}

// Daylight: bright textured background (cell maxima ~120-160), the light only ~30-40 levels above
// the brightest background pixel of its cell.
Case day_scene(uint64_t seed, double amp = 80) {
  Case c;
  c.seed = seed;
  c.sc.fps = 30;
  c.sc.jitter_ms = 2;
  c.sc.drop = 0.03;
  c.sc.bg = 110;
  c.sc.texture = 50;
  c.sc.texture_px = 10;
  c.sc.noise = 2;
  c.sc.shot = 0.25;
  c.sc.ae_pump = 0.06;
  const Timeline* tl = c.add_text("TEST DE N0CALL", 12);
  c.sc.lights.push_back(keyed_light(310.4, 170.6, amp, 0.8, tl));
  c.keyed = 0;
  finish(c, tl, "TEST DE N0CALL");
  return c;
}

// No keyed light at all: a dark room with high-gain noise.
Case empty_dark(uint64_t seed, double fps, double duration_ms = 60000) {
  Case c;
  c.seed = seed;
  c.sc = dark_room(fps);
  c.sc.noise = 4;
  c.sc.shot = 0.5;
  c.duration = duration_ms;
  return c;
}

// No keyed light: steady and flickering lamps (100 and 120 Hz), two standby blinkers, traffic, a
// lit person, AE pumping, hand shake and jerks.
Case empty_clutter(uint64_t seed, double fps, double duration_ms = 40000) {
  Case c;
  c.seed = seed;
  c.sc = dark_room(fps);
  c.sc.drop = 0.03;
  c.sc.ae_pump = 0.3;
  c.sc.shake_px_s = 24;
  c.sc.jump_every_ms = 5000;
  c.sc.jump_px = 30;
  c.sc.lights.push_back(steady(80, 40, 400, 1.5));
  c.sc.lights.push_back(steady(420, 220, 120, 0.7));
  Light l100 = steady(150, 230, 160, 2.0);
  l100.flicker = 0.9;
  Light l120 = steady(330, 60, 120, 1.0);
  l120.flicker = 0.9;
  l120.flicker_hz = 120;
  Light blink = steady(260, 150, 90, 0.6);  // a standby LED: 100 ms every second
  blink.blink = true;
  Light blink2 = steady(200, 120, 70, 0.6);  // and a faster one, 150 ms every 500 ms
  blink2.blink = true;
  blink2.blink_on_ms = 150;
  blink2.blink_period_ms = 500;
  c.sc.lights.push_back(l100);
  c.sc.lights.push_back(l120);
  c.sc.lights.push_back(blink);
  c.sc.lights.push_back(blink2);
  for (int i = 0; i < 4; ++i) {
    Light car = steady(-80 - 400.0 * i, 180 + 20.0 * i, 500, 1.5);
    car.vx = 120 + 30.0 * i;
    c.sc.lights.push_back(car);
  }
  Light walker = steady(600, 140, 45, 22);
  walker.vx = -60;
  c.sc.lights.push_back(walker);
  c.duration = duration_ms;
  return c;
}

std::string head(const std::string& s, size_t n) { return normalise(s).substr(0, n); }
std::string tail(const std::string& s, size_t n) {
  std::string t = normalise(s);
  return t.size() > n ? t.substr(t.size() - n) : t;
}

// "As good as a perfectly placed box would read the same frames, within one character."
bool as_good_as_box(const Outcome& o, const Case& c, double floor_cer = 0) {
  double one = 1.0 / static_cast<double>(std::max<size_t>(1, symbols(normalise(c.text)).size()));
  return o.cer <= std::max(floor_cer, o.oracle_cer + one + 1e-9);
}

}  // namespace

TEST_SUITE("morse") {

TEST_CASE("tracker: a one-pixel keyed light in a dark noisy room, 12 WPM, 30 fps") {
  Case c = tiny(1, 30, 12, "TEST DE N0CALL");
  Outcome o = play(c);
  CAPTURE(o.text);
  CHECK(o.cer == 0);
  CHECK(o.lock_ms > 0);
  CHECK(o.lock_ms < 2500);
  CHECK(o.first_char_ms < 3000);
  CHECK(o.max_err_cells < 1.5);
  CHECK(o.max_candidates == 1);
}

TEST_CASE("tracker: 1-2 pixel light, 5/12/15 WPM at 30 and 24 fps, jitter and 5 % dropped frames") {
  struct C { double fps, wpm; const char* text; double floor_cer, max_lock_ms; };
  const C cases[] = {{30, 5, "CQ TEST", 0, 4500},      {30, 12, "TEST DE N0CALL", 0, 2500},
                     {30, 15, "TEST DE N0CALL", 0, 2500}, {24, 5, "CQ TEST", 0, 4500},
                     {24, 12, "TEST DE N0CALL", 0.08, 2500}, {24, 15, "TEST DE N0CALL", 0.15, 2500}};
  uint64_t n = 0;
  for (const C& k : cases) {
    // one seed per case here (sub-pixel 2-px and pixel-centred 1-px lights alternate); 10 seeds
    // per case in the long sweep
    for (uint64_t seed : {2 + (n++ % 2)}) {
      CAPTURE(k.fps);
      CAPTURE(k.wpm);
      CAPTURE(seed);
      Case c = tiny(seed, k.fps, k.wpm, k.text);
      Outcome o = play(c, 2 + seed % 3);
      CAPTURE(o.text);
      CAPTURE(o.oracle);
      CAPTURE(o.lock_ms);
      // At 24 fps the decoder itself is near its limit (~2 frames per dot): compare with a
      // perfectly placed box on the same frames.
      CHECK(as_good_as_box(o, c, k.floor_cer));
      CHECK(o.lock_ms > 0);
      CHECK(o.lock_ms < k.max_lock_ms);
      CHECK(o.max_err_cells < 1.5);
    }
  }
}

TEST_CASE("tracker: clutter: steady lights, a flickering lamp, passing headlights, someone walking past, AE pumping") {
  for (double fps : {30.0, 24.0}) {
    CAPTURE(fps);
    Case c = clutter(5, fps);
    Outcome o = play(c);
    CAPTURE(o.text);
    CAPTURE(o.lock_ms);
    CHECK(o.cer == 0);
    CHECK(o.lock_ms > 0);
    CHECK(o.lock_ms < 2500);
    CHECK(o.max_err_cells < 1.5);
    CHECK(o.max_candidates == 1);
  }
}

TEST_CASE("tracker: hand shake (random walk, sudden jumps) follows the light") {
  for (uint64_t seed : {7, 8}) {
    CAPTURE(seed);
    Case c = shaky(seed);
    Outcome o = play(c);
    CAPTURE(o.text);
    CAPTURE(o.oracle);
    CHECK(as_good_as_box(o, c));
    CHECK(o.lock_ms > 0);
    CHECK(o.lock_ms < 2500);
    CHECK(o.max_err_cells < 1.5);
  }
}

TEST_CASE("tracker: the light leaves the frame for 1.5 s and is reacquired; transcript and speed kept") {
  Case c = dropout(9);
  Outcome o = play(c);
  CAPTURE(o.text);
  // What was sent while the camera looked away cannot be read; everything else is.
  CHECK(head(o.text, 9) == "CQ CQ CQ ");
  CHECK(tail(o.text, 8) == "N0CALL K");
  CHECK(o.cer <= 0.35);
  CHECK(o.last.target.state == morse_rx::Target::kLost);  // the sender stopped at the end
  CHECK(o.max_err_cells < 1.5);
  CHECK(o.last.wpm > 9);
  CHECK(o.last.wpm < 15);
}

TEST_CASE("tracker: zoom steps (1x -> 2x -> 5x -> 1x) mid-message: reacquired, transcript kept") {
  Case c = zoomed(15);
  Outcome o = play(c);
  CAPTURE(o.text);
  CAPTURE(o.oracle);
  // A zoom step may land in the middle of an element: at most about a character per step is lost.
  CHECK(o.cer <= std::max(0.15, o.oracle_cer + 0.15));
  CHECK(head(o.text, 6) == "CQ CQ ");
  CHECK(tail(o.text, 3) == "L K");
  CHECK(o.max_err_cells < 1.5);
}

TEST_CASE("tracker: two keyed lights: reads the clearer one consistently, reports two candidates") {
  Case c = two_lights(11);
  Outcome o = play(c);
  CAPTURE(o.text);
  CHECK(o.cer == 0);
  CHECK(o.max_candidates == 2);
  CHECK(o.max_err_cells < 1.5);
}

TEST_CASE("tracker: a light that saturates and blooms into its neighbours") {
  Case c = bloom(13);
  Outcome o = play(c);
  CAPTURE(o.text);
  CHECK(o.cer == 0);
  CHECK(o.lock_ms > 0);
  CHECK(o.lock_ms < 2500);
  CHECK(o.max_err_cells < 1.5);
}

TEST_CASE("tracker: daylight, textured bright background, low contrast") {
  Case c = day_scene(14);
  Outcome o = play(c);
  CAPTURE(o.text);
  CHECK(o.cer == 0);
  CHECK(o.lock_ms > 0);
  CHECK(o.lock_ms < 2500);
  CHECK(o.max_err_cells < 1.5);
}

TEST_CASE("tracker: a tap pins a light, auto releases it") {
  Case c = two_lights(12);
  Renderer rd(c.sc, c.seed);
  std::vector<Frame> frames = rd.run(c.duration);
  std::string err;
  auto s = morse_rx::Session::create("light", 0, 0, 0, &err);
  // Tap the dimmer light (index 0) right away: it is read even though the other one is clearer.
  double fx = 0, fy = 0;
  truth(c.sc, c.sc.lights[0], frames.front(), &fx, &fy);
  s->target_pin(fx, fy);
  for (const Frame& f : frames) {
    morse_rx::GridFrame g;
    g.t_ms = f.t;
    g.w = c.sc.gw;
    g.h = c.sc.gh;
    g.max = f.max.data();
    s->feed_frames(&g, 1);
  }
  morse_rx::Status st = s->status();
  CAPTURE(st.transcript);
  CHECK(st.target.pinned);
  CHECK(std::fabs(st.target.x - fx) < 0.03);
  CHECK(std::fabs(st.target.y - fy) < 0.05);
  CHECK(cer("QRZ DE N0CALL", st.transcript) <= 0.08);
  s->target_auto();
  CHECK(!s->status().target.pinned);
}

TEST_CASE("tracker: no keyed light: a dark noisy room stays searching, no text") {
  // (30 s here; the long sweep runs 10 seeds × 60 s at 24/30/60 fps)
  for (double fps : {30.0, 24.0}) {
    CAPTURE(fps);
    Outcome o = play(empty_dark(21, fps, 30000));
    CHECK(o.last.chars == 0);
    CHECK(o.text.empty());
    CHECK(!o.ever_locked);
    CHECK(o.last.target.state == morse_rx::Target::kSearching);
    CHECK(o.max_candidates == 0);
  }
}

TEST_CASE("tracker: no keyed light: clutter, flicker (100/120 Hz), standby blinkers, traffic, shake -> no text") {
  for (double fps : {30.0, 24.0, 60.0}) {  // (20 s here; 40 s × 10 seeds in the long sweep)
    CAPTURE(fps);
    Outcome o = play(empty_clutter(22, fps, 20000));
    CAPTURE(o.text);
    CHECK(o.last.chars == 0);
    CHECK(!o.ever_locked);
    CHECK(o.max_candidates == 0);
  }
}

TEST_CASE("tracker: honest pixels: 24x24 pixels per cell (1536x864) still finds a 1-pixel light") {
  Case c = tiny(4, 30, 12, "TEST", 55, 0.5);
  c.sc.W = 1536;
  c.sc.H = 864;
  c.sc.lights[0].x = 3 * 160.0;
  c.sc.lights[0].y = 3 * 100.0;
  Outcome o = play(c);
  CAPTURE(o.text);
  CHECK(o.cer == 0);
  CHECK(o.lock_ms < 2500);
}

// ---- long sweeps (skipped by default) --------------------------------------------------------

TEST_CASE("tracker long: every scene over 10 seeds" * doctest::skip()) {
  struct Row {
    const char* name;
    Case (*make)(uint64_t);
  };
  const Row rows[] = {
      {"tiny 30fps 12wpm", [](uint64_t s) { return tiny(s, 30, 12, "TEST DE N0CALL"); }},
      {"tiny 30fps 15wpm", [](uint64_t s) { return tiny(s, 30, 15, "TEST DE N0CALL"); }},
      {"tiny 30fps  5wpm", [](uint64_t s) { return tiny(s, 30, 5, "CQ TEST"); }},
      {"tiny 24fps 12wpm", [](uint64_t s) { return tiny(s, 24, 12, "TEST DE N0CALL"); }},
      {"tiny 24fps 15wpm", [](uint64_t s) { return tiny(s, 24, 15, "TEST DE N0CALL"); }},
      {"tiny 24fps  5wpm", [](uint64_t s) { return tiny(s, 24, 5, "CQ TEST"); }},
      {"tiny 60fps 15wpm", [](uint64_t s) { return tiny(s, 60, 15, "TEST DE N0CALL"); }},
      {"clutter 30fps", [](uint64_t s) { return clutter(s, 30); }},
      {"clutter 24fps", [](uint64_t s) { return clutter(s, 24); }},
      {"shake+jerks", shaky},
      {"dropout 1.5s", dropout},
      {"zoom 1-2-5-1", zoomed},
      {"two lights", two_lights},
      {"bloom", bloom},
      {"daylight", [](uint64_t s) { return day_scene(s); }},
  };
  std::printf("%-18s %6s %6s %6s %8s %8s %8s %6s\n", "scene", "exact", "cer", "box", "lock_md", "lock_max", "char1_md",
              "err");
  for (const Row& r : rows) {
    int exact = 0, n = 0;
    double sum_cer = 0, sum_box = 0, max_lock = 0, max_err = 0;
    std::vector<double> locks, firsts;
    for (uint64_t seed = 1; seed <= 10; ++seed) {
      Case c = r.make(seed);
      Outcome o = play(c, 1 + seed % 8);
      ++n;
      exact += o.cer == 0;
      sum_cer += o.cer;
      sum_box += o.oracle_cer;
      locks.push_back(o.lock_ms < 0 ? 1e9 : o.lock_ms);
      firsts.push_back(o.first_char_ms < 0 ? 1e9 : o.first_char_ms);
      max_lock = std::max(max_lock, o.lock_ms < 0 ? 1e9 : o.lock_ms);
      max_err = std::max(max_err, o.max_err_cells);
      if (o.cer > o.oracle_cer + 0.1) std::printf("   seed %llu: %s | box: %s\n", static_cast<unsigned long long>(seed), o.text.c_str(), o.oracle.c_str());
    }
    std::sort(locks.begin(), locks.end());
    std::sort(firsts.begin(), firsts.end());
    std::printf("%-18s %3d/%-2d %5.1f%% %5.1f%% %8.0f %8.0f %8.0f %6.1f\n", r.name, exact, n, 100 * sum_cer / n,
                100 * sum_box / n, locks[locks.size() / 2], max_lock, firsts[firsts.size() / 2], max_err);
  }
}

TEST_CASE("tracker long: no text from scenes without a keyed light, 10 seeds each" * doctest::skip()) {
  for (double fps : {24.0, 30.0, 60.0}) {
    uint64_t chars = 0;
    int locks = 0;
    for (uint64_t seed = 1; seed <= 10; ++seed) {
      for (int kind = 0; kind < 2; ++kind) {
        Outcome o = play(kind ? empty_clutter(seed, fps) : empty_dark(seed, fps));
        chars += o.last.chars;
        locks += o.ever_locked;
        if (o.last.chars) std::printf("   fps %.0f seed %llu kind %d: %s\n", fps, static_cast<unsigned long long>(seed), kind, o.text.c_str());
      }
    }
    std::printf("fps %.0f: 20 scenes (%.0f min of video): %llu characters, %d locks\n", fps, 10 * (60 + 40) / 60.0,
                static_cast<unsigned long long>(chars), locks);
  }
}

TEST_CASE("tracker long: smallest light (contrast) that still reads exactly" * doctest::skip()) {
  // Dark room: read noise 2.5 + shot noise, cell = 8×8 pixels. Amp = luma added at the light's pixel.
  for (double fps : {30.0, 24.0}) {
    for (double amp : {14.0, 16.0, 18.0, 20.0, 22.0, 25.0, 30.0, 35.0, 40.0}) {
      int ok = 0, locked = 0;
      for (uint64_t seed = 1; seed <= 6; ++seed) {
        Case c = tiny(seed, fps, 12, "TEST DE N0CALL", amp, 0.5);
        c.sc.lights[0].x = std::floor(c.sc.lights[0].x);  // centred on a pixel
        c.sc.lights[0].y = std::floor(c.sc.lights[0].y);
        Outcome o = play(c);
        ok += o.cer <= o.oracle_cer + 1e-9;
        locked += o.ever_locked;
      }
      std::printf("dark, %2.0f fps, one pixel +%2.0f luma: locked %d/6, as good as a box %d/6\n", fps, amp, locked, ok);
    }
  }
  for (double amp : {30.0, 40.0, 50.0, 60.0, 70.0, 80.0}) {
    int ok = 0, locked = 0;
    for (uint64_t seed = 1; seed <= 6; ++seed) {
      Outcome o = play(day_scene(seed, amp));
      ok += o.cer == 0;
      locked += o.ever_locked;
    }
    std::printf("daylight, 2-pixel light +%2.0f luma over a textured 110-160 background: locked %d/6, exact %d/6\n", amp, locked,
                ok);
  }
}

TEST_CASE("tracker long: cost per frame (64x36 and 64x64 grids)" * doctest::skip()) {
  for (int big = 0; big < 2; ++big) {
    Case c = clutter(3, 60);
    if (big) {
      c.sc.gh = 64;
      c.sc.H = 512;
    }
    Renderer rd(c.sc, 3);
    std::vector<Frame> frames = rd.run(c.duration);
    std::string err;
    auto s = morse_rx::Session::create("light", 0, 0, 0, &err);
    std::vector<morse_rx::GridFrame> g(frames.size());
    for (size_t i = 0; i < frames.size(); ++i) {
      g[i].t_ms = frames[i].t;
      g[i].w = c.sc.gw;
      g[i].h = c.sc.gh;
      g[i].max = frames[i].max.data();
      g[i].mean = frames[i].mean.data();
    }
    auto t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < g.size(); i += 4) s->feed_frames(&g[i], std::min<size_t>(4, g.size() - i));
    double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    std::printf("%dx%d grid, 60 fps busy night scene: %.1f us per frame (tracker + decoder), %zu frames; text: %s\n",
                c.sc.gw, c.sc.gh, us / static_cast<double>(g.size()), g.size(), s->status().transcript.c_str());
  }
}

}  // TEST_SUITE
