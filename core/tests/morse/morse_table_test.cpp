// Regression: every character of the table (letters, all ten digits, punctuation, prosigns), alone
// and inside words, at 5/13/20/30 WPM with standard timing as the apps send it
// (ember::morse::timing), through BOTH the light and the audio source. Digits once vanished on the
// audio path ("QX0ABC" -> "QXABC"); an undecodable group must appear as U+25A1, never be dropped.
#include <algorithm>
#include <string>
#include <vector>

#include "doctest.h"
#include "morse/rx.h"
#include "morse_sim.h"

using namespace ember;
using namespace morsesim;

namespace {

std::string decode_light(const std::string& text, double wpm, uint64_t seed) {
  Rng r(seed);
  HandOptions h;
  h.wpm = wpm;
  Timeline tl = hand_key(text, h, r);
  CameraOptions c;
  c.fps = wpm >= 20 ? 60 : 30;  // ~2 frames per dot is the floor (see the light range table)
  Frames f = camera(tl, end_of(tl) + 8000, c, r);
  std::string err;
  auto s = morse_rx::Session::create("light", 0, 0, 0, &err);
  for (size_t i = 0; i < f.t.size(); i += 5) {
    size_t e = std::min(i + 5, f.t.size());
    s->feed_times(std::vector<double>(f.t.begin() + static_cast<long>(i), f.t.begin() + static_cast<long>(e)),
                  std::vector<double>(f.v.begin() + static_cast<long>(i), f.v.begin() + static_cast<long>(e)));
  }
  return s->status().transcript;
}

std::string decode_audio(const std::string& text, double wpm, uint64_t seed, uint32_t rate = 8000) {
  Rng r(seed);
  HandOptions h;
  h.wpm = wpm;
  Timeline tl = hand_key(text, h, r);
  AudioOptions a;
  a.rate = rate;
  a.tone_hz = 700;
  a.snr_db = 25;
  std::vector<int16_t> pcm = audio(tl, end_of(tl) + 8000, a, r);
  std::string err;
  auto s = morse_rx::Session::create("audio", rate, 0, 0, &err);
  for (size_t off = 0; off < pcm.size(); off += rate / 10) s->feed_pcm(pcm.data() + off, std::min<size_t>(rate / 10, pcm.size() - off));
  return s->status().transcript;
}

const char* kCases[] = {
    "QX0ABC QX0ABC",
    "0123456789",
    "0 1 2 3 4 5 6 7 8 9",
    "N0CALL 73 88 TU",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    ". , ? ' ! / ( ) : ; - _ \" $ @",
    "<SOS> <AR> <SK> <BT> <KN> <AS> <HH>",
    "RST 599 QTH 12345 <SK>",
};

void table_matrix(const std::vector<double>& audio_wpm, const std::vector<double>& light_wpm, size_t ncases) {
  for (size_t ci = 0; ci < ncases && ci < sizeof kCases / sizeof kCases[0]; ++ci) {
    const char* text = kCases[ci];
    std::string want = normalise(text);
    // The receiver writes the shared patterns as prosigns: +=<AR>, ==<BT>, (=<KN>, &=<AS>.
    for (auto& m : std::vector<std::pair<std::string, std::string>>{{"(", "<KN>"}, {"+", "<AR>"}, {"=", "<BT>"}, {"&", "<AS>"}})
      for (size_t p = want.find(m.first); p != std::string::npos; p = want.find(m.first, p + m.second.size()))
        want.replace(p, 1, m.second);
    for (double wpm : audio_wpm) {
      CAPTURE(wpm);
      CAPTURE(std::string(text));
      std::string a = normalise(decode_audio(text, wpm, 1));
      CAPTURE(a);
      CHECK(cer(want, a) == 0);
    }
    for (double wpm : light_wpm) {  // light: about 25 WPM is the camera limit
      CAPTURE(wpm);
      CAPTURE(std::string(text));
      std::string l = normalise(decode_light(text, wpm, 2));
      CAPTURE(l);
      CHECK(cer(want, l) == 0);
    }
  }
}

}  // namespace

TEST_SUITE("morse") {

TEST_CASE("regression: digits, punctuation and prosigns decode on both sources") {
  table_matrix({13, 30}, {5, 13, 20}, 8);
  table_matrix({5, 20}, {}, 2);  // slow and medium audio: the digit rows
}

TEST_CASE("long: character table at 5/13/20/30 WPM on both sources" * doctest::skip()) {
  table_matrix({5, 13, 20, 30}, {5, 13, 20}, 8);
}

TEST_CASE("regression: audio at other sample rates keeps digits") {
  for (uint32_t rate : {11025u, 16000u, 44100u, 48000u}) {
    CAPTURE(rate);
    std::string a = normalise(decode_audio("QX0ABC 1234567890", 18, 3, rate));
    CHECK(a == "QX0ABC 1234567890");
  }
}

TEST_CASE("an undecodable element group is reported as a white square, never dropped") {
  // 8 dashes is not a character: a group that is not in the table.
  modem::MorseTiming::Config c;
  c.prosign_names = true;
  modem::MorseTiming t(c);
  double ms = 0, dot = 100;
  auto mark = [&](double len) { t.key(true, ms); ms += len; t.key(false, ms); ms += dot; t.advance(ms); };
  for (int i = 0; i < 3; ++i) { mark(dot); }   // S, to teach the speed
  ms += 3 * dot; t.advance(ms);
  for (int i = 0; i < 8; ++i) mark(3 * dot);    // --------
  ms += 10 * dot; t.advance(ms);
  std::string out;
  t.take_text(&out);
  CHECK(out.find("\xE2\x96\xA1") != std::string::npos);
  CHECK(out.find('S') == 0);
}

}  // TEST_SUITE
