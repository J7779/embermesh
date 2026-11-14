// Audio Morse receive quality: encode -> hand keying -> tone + channel (AWGN at an SNR in a
// 2500 Hz bandwidth, tone drift, QSB fading, clicks) -> AudioDecoder (shared CwReceiver) -> text.
//   ./ember_tests -ts=morse -tc="audio long*" --no-skip   prints the SNR at 10 % CER per speed.
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

struct Result {
  double cer = 1;
  std::string text;
  double tone = 0, wpm = 0;
  double first_char_s = -1;
};

Result run_audio(const std::string& text, const HandOptions& h, const AudioOptions& a, uint64_t seed, double tone_hint = 0,
                 const Timeline* custom = nullptr) {
  Rng r(seed);
  Timeline tl = custom ? *custom : hand_key(text, h, r);
  std::vector<int16_t> pcm = audio(tl, end_of(tl) + 3000, a, r);
  std::string err;
  auto s = morse_rx::Session::create("audio", a.rate, 0, tone_hint, &err);
  REQUIRE(s);
  Result out;
  size_t step = a.rate / 20;  // 50 ms feeds
  for (size_t off = 0; off < pcm.size(); off += step) {
    s->feed_pcm(pcm.data() + off, std::min(step, pcm.size() - off));
    if (out.first_char_s < 0 && s->status().chars > 0) out.first_char_s = static_cast<double>(off) / a.rate;
  }
  morse_rx::Status st = s->status();
  out.text = st.transcript;
  out.cer = cer(text, st.transcript);
  out.tone = st.tone_hz;
  out.wpm = st.wpm;
  return out;
}

}  // namespace

TEST_SUITE("morse") {

TEST_CASE("audio: tone auto-detection 300..1500 Hz, 5..40 WPM, clean") {
  for (double tone : {350.0, 600.0, 1000.0, 1450.0}) {
    for (double wpm : {5.0, 15.0, 25.0, 40.0}) {
      if (wpm == 5 && tone != 600) continue;  // keep it fast
      CAPTURE(tone);
      CAPTURE(wpm);
      HandOptions h;
      h.wpm = wpm;
      AudioOptions a;
      a.tone_hz = tone;
      a.snr_db = 30;
      Result r = run_audio(kText, h, a, 3);
      CAPTURE(r.text);
      CHECK(r.cer == 0);
      CHECK(std::fabs(r.tone - tone) < 30);
      CHECK(std::fabs(r.wpm - wpm) < 0.2 * wpm);
    }
  }
}

TEST_CASE("audio: noise, drift, QSB fading and clicks") {
  HandOptions h;
  h.wpm = 18;
  struct C { const char* what; double snr, drift, qsb, clicks, max_cer; };
  const C cases[] = {
      {"awgn 10 dB", 10, 0, 0, 0, 0.04},       {"awgn 6 dB", 6, 0, 0, 0, 0.05},
      {"drift 8 Hz/s", 15, 8, 0, 0, 0.05},    {"qsb 60 %", 15, 0, 0.6, 0, 0.05},
      {"clicks 3/s", 15, 0, 0, 3, 0.10},      {"everything", 10, 4, 0.5, 1, 0.10},
  };
  for (const C& c : cases) {
    for (uint64_t seed : {1, 2}) {
      CAPTURE(c.what);
      CAPTURE(seed);
      AudioOptions a;
      a.tone_hz = 750;
      a.snr_db = c.snr;
      a.drift_hz_per_s = c.drift;
      a.qsb_depth = c.qsb;
      a.clicks_per_s = c.clicks;
      Result r = run_audio(kText, h, a, seed);
      CAPTURE(r.text);
      CHECK(r.cer <= c.max_cer);
    }
  }
}

TEST_CASE("audio: hand keying, Farnsworth, speed change, real-time output") {
  AudioOptions a;
  a.snr_db = 15;
  HandOptions h;
  h.wpm = 15;
  h.jitter = 0.12;
  h.dash_ratio = 3.5;
  Result r = run_audio(kText, h, a, 5);
  CAPTURE(r.text);
  CHECK(r.cer <= 0.05);
  HandOptions f;
  f.wpm = 20;
  f.farnsworth_wpm = 8;
  r = run_audio(kText, f, a, 6);
  CAPTURE(r.text);
  CHECK(r.cer <= 0.05);
  // The first character appears right after its own gap (the message lasts ~12 s at 15 WPM).
  HandOptions g;
  g.wpm = 15;
  r = run_audio(kText, g, a, 7);
  CHECK(r.first_char_s > 0);
  CHECK(r.first_char_s < 1.5 + 0.9 + 0.5);
  // Speed change 12 -> 28 WPM mid-message.
  Rng rr(8);
  HandOptions s1, s2;
  s1.wpm = 12;
  s2.wpm = 28;
  s2.lead_ms = 0;
  Timeline tl = hand_key("SLOW PART", s1, rr);
  Timeline t2 = hand_key(" THEN FAST PART", s2, rr, end_of(tl) + 7 * 100);
  tl.insert(tl.end(), t2.begin(), t2.end());
  Result sc = run_audio("SLOW PART THEN FAST PART", g, a, 8, 0, &tl);
  CAPTURE(sc.text);
  CHECK(sc.cer <= 0.16);
  CHECK(sc.wpm > 20);
}

TEST_CASE("audio: pure noise and silence decode (almost) nothing") {
  std::string err;
  for (int kind = 0; kind < 2; ++kind) {
    auto s = morse_rx::Session::create("audio", 8000, 0, 0, &err);
    Rng r(kind + 3);
    std::vector<int16_t> pcm(8000 * 30);
    for (auto& v : pcm) v = kind == 0 ? 0 : static_cast<int16_t>(3000 * r.gauss());
    s->feed_pcm(pcm.data(), pcm.size());
    CAPTURE(kind);
    CHECK(s->status().chars <= 3);
  }
}

TEST_CASE("audio long: SNR at 10 % CER per speed" * doctest::skip()) {
  for (double wpm : {5.0, 10.0, 20.0, 30.0, 40.0}) {
    double found = 99;
    for (double snr = 20; snr >= -6; snr -= 1) {
      double total = 0;
      for (uint64_t seed : {1, 2, 3}) {
        HandOptions h;
        h.wpm = wpm;
        AudioOptions a;
        a.snr_db = snr;
        total += run_audio(kText, h, a, seed).cer;
      }
      if (total / 3 > 0.10) break;
      found = snr;
    }
    std::printf("audio %4.0f wpm: CER <= 10 %% down to %.0f dB SNR (2500 Hz bandwidth)\n", wpm, found);
  }
}

}  // TEST_SUITE
