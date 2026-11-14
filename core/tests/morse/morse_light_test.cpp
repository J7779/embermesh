// Light (camera) Morse receive quality: encode -> hand keying -> camera model -> LightKeyer ->
// text, over frame rates, speeds and impairments. Asserts character error rates and real-time
// (incremental) output; the long sweep prints the usable range table.
//   ./ember_tests -ts=morse -tc="light long*" --no-skip
#include <algorithm>
#include <cstdio>
#include <string>

#include "doctest.h"
#include "morse/rx.h"
#include "morse_sim.h"

using namespace ember;
using namespace morsesim;

namespace {

const char* kText = "CQ CQ DE N0CALL N0CALL PSE K";

struct Run {
  double cer = 1;
  std::string text;
  double first_char_ms = -1;  // time (signal clock) the first character appeared
  double end_ms = 0;          // end of the last element
  double wpm = 0, quality = 0;
};

Run run_light(const std::string& text, const HandOptions& h, const CameraOptions& c, uint64_t seed,
              size_t batch = 6, const Timeline* custom = nullptr) {
  Rng r(seed);
  Timeline tl = custom ? *custom : hand_key(text, h, r);
  Frames f = camera(tl, end_of(tl) + 4000, c, r);
  std::string err;
  auto s = morse_rx::Session::create("light", 0, 0, 0, &err);
  REQUIRE(s);
  Run out;
  out.end_ms = end_of(tl);
  for (size_t i = 0; i < f.t.size(); i += batch) {
    std::vector<double> t(f.t.begin() + static_cast<long>(i), f.t.begin() + static_cast<long>(std::min(i + batch, f.t.size())));
    std::vector<double> v(f.v.begin() + static_cast<long>(i), f.v.begin() + static_cast<long>(std::min(i + batch, f.v.size())));
    s->feed_times(t, v);
    if (out.first_char_ms < 0 && s->status().chars > 0) out.first_char_ms = t.back();
  }
  morse_rx::Status st = s->status();
  out.text = st.transcript;
  out.cer = cer(text, st.transcript);
  out.wpm = st.wpm;
  out.quality = st.quality;
  return out;
}

CameraOptions phone_camera(double fps) {
  CameraOptions c;
  c.fps = fps;
  c.jitter_ms = 0.08 * 1000 / fps;
  c.drop = 0.02;
  c.ae_strength = 0.5;
  c.drift = 0.15;
  c.flicker = 0.12;
  c.noise = 0.015;
  c.wander = 0.3;
  return c;
}

}  // namespace

TEST_SUITE("morse") {

TEST_CASE("light: clean camera, 30 fps, 5..20 WPM decodes exactly") {
  for (double wpm : {5.0, 10.0, 15.0, 20.0}) {
    CAPTURE(wpm);
    HandOptions h;
    h.wpm = wpm;
    CameraOptions c;
    c.fps = 30;
    Run r = run_light(kText, h, c, 11);
    CAPTURE(r.text);
    CHECK(r.cer == 0);
    CHECK(std::fabs(r.wpm - wpm) < 0.25 * wpm);
  }
}

TEST_CASE("light: phone camera impairments (AE pumping, drift, flicker, noise, drops, shake) at 15/24/30/60 fps") {
  struct C { double fps, wpm, max_cer; };
  const C cases[] = {{15, 5, 0.05}, {15, 8, 0.10}, {24, 10, 0.05}, {24, 12, 0.10}, {30, 12, 0.05},
                     {30, 15, 0.10}, {60, 15, 0.05}, {60, 20, 0.05}};
  for (const C& k : cases) {
    for (uint64_t seed : {1, 2, 3}) {
      CAPTURE(k.fps);
      CAPTURE(k.wpm);
      CAPTURE(seed);
      HandOptions h;
      h.wpm = k.wpm;
      Run r = run_light(kText, h, phone_camera(k.fps), seed);
      CAPTURE(r.text);
      CHECK(r.cer <= k.max_cer);
    }
  }
}

TEST_CASE("light: sloppy hand keying (dash:dot 2..5, 15 % jitter, Farnsworth)") {
  for (double ratio : {2.0, 3.0, 4.0, 5.0}) {
    for (uint64_t seed : {4, 5}) {
      CAPTURE(ratio);
      CAPTURE(seed);
      HandOptions h;
      h.wpm = 10;
      h.dash_ratio = ratio;
      h.jitter = 0.15;
      Run r = run_light(kText, h, phone_camera(30), seed);
      CAPTURE(r.text);
      // At 2:1 with 15 % jitter dots and dashes genuinely overlap; still mostly readable.
      CHECK(r.cer <= (ratio < 2.5 ? 0.20 : 0.10));
    }
  }
  HandOptions f;
  f.wpm = 15;
  f.farnsworth_wpm = 6;
  f.jitter = 0.1;
  Run r = run_light(kText, f, phone_camera(30), 9);
  CAPTURE(r.text);
  CHECK(r.cer <= 0.05);
}

TEST_CASE("light: speed change mid-message re-adapts") {
  Rng r(21);
  HandOptions a, b;
  a.wpm = 6;
  b.wpm = 16;
  b.lead_ms = 0;
  Timeline tl = hand_key("SLOW TEXT HERE", a, r);
  Timeline t2 = hand_key(" AND NOW FAST TEXT", b, r, end_of(tl) + 7 * 1200.0 / 6);
  tl.insert(tl.end(), t2.begin(), t2.end());
  HandOptions none;
  Run run = run_light("SLOW TEXT HERE AND NOW FAST TEXT", none, phone_camera(30), 21, 6, &tl);
  CAPTURE(run.text);
  // A 2.7x jump costs about one character before the new speed is learned, then it is exact.
  CHECK(run.cer <= 0.16);
  CHECK(normalise(run.text).size() >= 9);
  CHECK(normalise(run.text).substr(normalise(run.text).size() - 9) == "FAST TEXT");
  CHECK(run.wpm > 12);  // ended at the new speed
}

TEST_CASE("light: text appears incrementally (real time), prosigns and digits") {
  HandOptions h;
  h.wpm = 12;
  std::string text = "<SOS> 73 DE N0CALL <AR> <SK> 1234567890 <BT> <KN> <AS>";
  Run r = run_light(text, h, phone_camera(30), 31, 3);
  CAPTURE(r.text);
  CHECK(r.cer <= 0.05);
  // The first character is out within a few dots of its end, long before the message ends.
  REQUIRE(r.first_char_ms > 0);
  CHECK(r.first_char_ms < 1500 + 2300 + 600);  // <SOS> is 23 units at 12 WPM, then its gap
  CHECK(r.first_char_ms < r.end_ms / 4);
}

TEST_CASE("light: no signal, steady light, noise only and a dark scene decode nothing") {
  std::string err;
  for (int kind = 0; kind < 4; ++kind) {
    CAPTURE(kind);
    auto s = morse_rx::Session::create("light", 0, 0, 0, &err);
    Rng r(kind + 50);
    for (int i = 0; i < 30 * 60; ++i) {  // one minute at 30 fps
      double v = kind == 0 ? 0.3 : kind == 1 ? 0.8 : kind == 2 ? 0.4 + 0.02 * r.gauss() : 0.0;
      s->feed_times({i * 33.3}, {v});
    }
    CHECK(s->status().chars == 0);
  }
}

// A message typed on the Send tab, flashed and filmed from its very first element (no tune-up):
// 30 fps with frame-time jitter, a full-frame exposure (one frame of lag at every edge), noise. The
// first letters used to fuse while the speed was still unknown ("TEST" -> "NST").
TEST_CASE("light: cold start, a short message reads exactly from its first letter (30 fps, jitter, exposure lag)") {
  for (const char* text : {"TEST", "HELLO WORLD 73", "SOS", "CQ CQ TEST", "TEN"}) {
    for (double wpm : {7.0, 12.0, 15.0}) {
      for (uint64_t seed = 1; seed <= 6; ++seed) {
        CAPTURE(text);
        CAPTURE(wpm);
        CAPTURE(seed);
        HandOptions h;
        h.wpm = wpm;
        h.lead_ms = 1000;
        CameraOptions c;
        c.fps = 30;
        c.jitter_ms = 3;
        c.exposure = 1.0;
        c.noise = 0.02;
        Run r = run_light(text, h, c, seed, 3);
        CAPTURE(r.text);
        CHECK(r.cer == 0);
      }
    }
  }
}

TEST_CASE("light long: usable range table (fps x wpm, phone camera)" * doctest::skip()) {
  std::printf("light CER (phone camera model, 3 seeds, 60-char message)\n   fps:");
  const double fpss[] = {10, 15, 24, 30, 60};
  for (double f : fpss) std::printf("%7.0f", f);
  std::printf("\n");
  for (double wpm : {3.0, 5.0, 8.0, 10.0, 12.0, 15.0, 18.0, 20.0, 25.0}) {
    std::printf("%4.0f wpm", wpm);
    for (double fps : fpss) {
      double total = 0;
      for (uint64_t seed : {1, 2, 3}) {
        HandOptions h;
        h.wpm = wpm;
        total += run_light(kText, h, phone_camera(fps), seed).cer;
      }
      std::printf("%6.1f%%", 100 * total / 3);
    }
    std::printf("\n");
  }
  // Frames per dot at which CER first stays <= 5 %.
  for (double fps : {15.0, 30.0}) {
    for (double fpd = 0.8; fpd <= 4.01; fpd += 0.2) {
      double wpm = 1200.0 / (fpd * 1000 / fps);
      double total = 0;
      for (uint64_t seed : {1, 2, 3, 4}) {
        HandOptions h;
        h.wpm = wpm;
        total += run_light(kText, h, phone_camera(fps), seed).cer;
      }
      if (total / 4 <= 0.05) {
        std::printf("fps %.0f: CER <= 5 %% from %.1f frames per dot (%.1f wpm)\n", fps, fpd, wpm);
        break;
      }
    }
  }
}

}  // TEST_SUITE
