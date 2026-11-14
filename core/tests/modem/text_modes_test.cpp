// Keyboard / legacy modes: DTMF, CW (Morse), PSK31, RTTY.
#include <cstring>
#include <set>
#include <string>

#include "doctest.h"
#include "modem/morse.h"
#include "modem/psk31.h"
#include "modem/rtty.h"
#include "test_util.h"

using namespace ember;
using namespace ember::modem;
using mt::LoopCfg;

namespace {
std::string trim(const std::string& s) {
  size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' ');
  return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
}  // namespace

TEST_SUITE("modem") {

// ---- DTMF -----------------------------------------------------------------------------------

TEST_CASE("DTMF: all 16 digits at every sample rate, pauses, invalid text") {
  for (uint32_t rate : mt::all_rates()) {
    LoopCfg c;
    c.tx_rate = c.rx_rate = rate;
    c.ch.snr_db = 20;
    c.ch.bandlimit = true;
    INFO("rate " << rate);
    CHECK(mt::loopback_text("dtmf", "0123456789*#ABCD", c) == "0123456789*#ABCD");
  }
  LoopCfg c;
  c.tx_rate = c.rx_rate = 8000;
  CHECK(mt::loopback_text("dtmf", "1 1,1a", c) == "111A");
  auto m = mt::make("dtmf", 8000);
  CHECK_FALSE(m->send_text("12x"));
  CHECK_FALSE(m->send_text("   "));
  CHECK_FALSE(m->send_frame(ByteView(Bytes{1})));
  CHECK(m->send_text("5"));
}

TEST_CASE("DTMF: noise, tilt, twist and short timing") {
  LoopCfg c;
  c.tx_rate = c.rx_rate = 16000;
  c.ch.snr_db = 6;
  c.ch.tilt = -1;
  c.ch.seed = 3;
  CHECK(mt::loopback_text("dtmf", "159D#", c) == "159D#");
  LoopCfg f;
  f.tx_opt.params["tone_ms"] = "60";
  f.tx_opt.params["gap_ms"] = "60";
  f.ch.snr_db = 15;
  CHECK(mt::loopback_text("dtmf", "2468", f) == "2468");
  auto m = mt::make("dtmf", 44100);
  CHECK(mt::garbage_frames(*m, 5, 1) == 0);
  RxItem it;
  // Pure noise gives no digits.
  auto n = mt::make("dtmf", 8000);
  dsp::Prng r(4);
  std::vector<float> x(80000);
  for (auto& v : x) v = static_cast<float>(0.3 * r.gauss());
  n->rx(x.data(), x.size());
  CHECK_FALSE(n->pop(&it));
}

// ---- CW -------------------------------------------------------------------------------------

TEST_CASE("Morse table: complete, unique, prosigns") {
  const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,?'!/()&:;=+-_\"$@";
  std::set<std::string> seen;
  for (const char* p = chars; *p; ++p) {
    const char* pat = morse::pattern_for(*p);
    REQUIRE(pat != nullptr);
    CHECK(seen.insert(pat).second);
    CHECK(morse::decode(pat) == std::string(1, *p));
  }
  CHECK(std::string(morse::pattern_for('s')) == "...");
  CHECK(morse::decode("...-.-") == "<SK>");
  CHECK(morse::decode("........") == "<HH>");
  CHECK(morse::decode("...---...") == "<SOS>");
  CHECK(morse::decode(".-.-.") == "+");  // shared with <AR>
  CHECK(std::string(morse::prosign_pattern("ar")) == ".-.-.");
  CHECK(morse::decode("......-.-") == "");
  std::string tok;
  CHECK(morse::encode("SOS <SK> ~", &tok));
  CHECK(tok == "...|---|...| |...-.-|");
  CHECK_FALSE(morse::encode("~~", &tok));
}

TEST_CASE("CW: loopback at several speeds and rates, Farnsworth, prosigns") {
  const std::string text = "CQ CQ DE N0CALL K";
  const std::pair<const char*, uint32_t> combos[] = {{"12", 8000u}, {"20", 22050u}, {"35", 48000u}, {"35", 8000u}};
  for (const auto& wr : combos) {
    const char* wpm = wr.first;
    {
      uint32_t rate = wr.second;
      LoopCfg c;
      c.tx_rate = c.rx_rate = rate;
      c.ch.snr_db = 10;
      c.ch.seed = static_cast<uint64_t>(rate);
      c.tx_opt.params["wpm"] = wpm;
      // Receiver always starts at its default 20 wpm estimate and must adapt.
      std::string got = trim(mt::loopback_text("cw", "VVV " + text, c));
      INFO("wpm " << std::string(wpm) << " rate " << rate << " got [" << got << "]");
      CHECK(got.find(text) != std::string::npos);
    }
  }
  LoopCfg f;
  f.tx_opt.params["wpm"] = "18";
  f.tx_opt.params["farnsworth_wpm"] = "8";
  f.ch.snr_db = 15;
  std::string fg = trim(mt::loopback_text("cw", "VVV TEST 73 <SK>", f));
  INFO("farnsworth got [" << fg << "]");
  CHECK(fg.find("TEST 73 <SK>") != std::string::npos);
}

TEST_CASE("CW: AFC finds an off-frequency signal, noise is quiet") {
  LoopCfg c;
  c.tx_opt.params["tone_hz"] = "910";
  c.ch.snr_db = 12;
  c.lead_ms = 2500;
  std::string got = trim(mt::loopback_text("cw", "VVV VVV PARIS PARIS", c));
  CHECK(got.find("PARIS PARIS") != std::string::npos);
  auto m = mt::make("cw", 8000);
  dsp::Prng r(9);
  std::vector<float> x(8000 * 20);
  for (auto& v : x) v = static_cast<float>(0.2 * r.gauss());
  m->rx(x.data(), x.size());
  std::string out;
  RxItem it;
  while (m->pop(&it)) out += it.text;
  INFO("noise gave [" << out << "]");
  CHECK(out.size() <= 2);
  CHECK(mt::garbage_frames(*m, 5, 2) == 0);
  auto t = mt::make("cw", 8000);
  CHECK_FALSE(t->send_text("~~~"));
}

// ---- PSK31 ----------------------------------------------------------------------------------

TEST_CASE("PSK31 varicode table is prefix-free under the 00 rule") {
  std::set<std::string> codes;
  for (int c = 0; c < 128; ++c) {
    const char* v = psk31::varicode(static_cast<unsigned char>(c));
    REQUIRE(v != nullptr);
    std::string s = v;
    CHECK(s.front() == '1');
    CHECK(s.back() == '1');
    CHECK(s.find("00") == std::string::npos);
    CHECK(s.size() <= 10);
    CHECK(codes.insert(s).second);
    CHECK(psk31::varicode_decode(v, s.size()) == c);
  }
  CHECK(std::string(psk31::varicode(' ')) == "1");
  CHECK(std::string(psk31::varicode('e')) == "11");
  CHECK(std::string(psk31::varicode('t')) == "101");
  CHECK(psk31::varicode(200) == nullptr);
}

TEST_CASE("PSK31: loopback at every rate, noise, offset, drift") {
  const std::string text = "cq cq de n0call - PSK31 test 0123456789\n";
  for (uint32_t rate : mt::all_rates()) {
    LoopCfg c;
    c.tx_rate = c.rx_rate = rate;
    c.ch.snr_db = 0;
    c.ch.seed = rate;
    INFO("rate " << rate);
    CHECK(mt::loopback_text("psk31", text, c) == text);
  }
  LoopCfg c;
  c.ch.snr_db = -6;
  c.ch.freq_offset_hz = 12;
  c.ch.clock_ppm = 300;
  c.ch.seed = 5;
  std::string got = mt::loopback_text("psk31", text, c);
  CHECK(mt::edit_distance(got, text) <= 2);
  auto m = mt::make("psk31", 8000);
  dsp::Prng r(9);
  std::vector<float> x(8000 * 20);
  for (auto& v : x) v = static_cast<float>(0.2 * r.gauss());
  m->rx(x.data(), x.size());
  std::string out;
  RxItem it;
  while (m->pop(&it)) out += it.text;
  CHECK(out.size() <= 3);
  CHECK(mt::garbage_frames(*m, 5, 3) == 0);
}

// ---- RTTY -----------------------------------------------------------------------------------

TEST_CASE("ITA2 table round trip") {
  for (char c : std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789-'?:(),./;\"$!&#")) {
    bool figs, either;
    int code = ita2::encode(c, &figs, &either);
    REQUIRE(code >= 0);
    CHECK(ita2::decode(code, figs) == c);
  }
  bool f, e;
  CHECK(ita2::encode('E', &f, &e) == 1);
  CHECK(ita2::encode('3', &f, &e) == 1);
  CHECK(f);
  CHECK(ita2::encode('~', &f, &e) == -1);
}

TEST_CASE("RTTY: loopback at every rate, shifts, noise, offset") {
  const std::string text = "RYRYRY CQ DE N0CALL 599 73 -- 12.5/7\n";
  for (uint32_t rate : mt::all_rates()) {
    LoopCfg c;
    c.tx_rate = c.rx_rate = rate;
    c.ch.snr_db = 10;
    c.ch.seed = rate;
    std::string got = mt::loopback_text("rtty", text, c);
    INFO("rate " << rate << " got [" << got << "]");
    CHECK(mt::edit_distance(got, text) <= 1);  // a noise character after the signal is possible
  }
  LoopCfg c;
  c.ch.snr_db = 4;
  c.ch.freq_offset_hz = 15;
  c.ch.tilt = -1;
  c.ch.seed = 8;
  CHECK(mt::edit_distance(mt::loopback_text("rtty", text, c), text) <= 3);
  LoopCfg rv;
  rv.tx_opt.params["reverse"] = "1";
  rv.rx_opt.params["reverse"] = "true";
  CHECK(mt::loopback_text("rtty", "REVERSE 123", rv) == "REVERSE 123");
  auto m = mt::make("rtty", 8000);
  dsp::Prng r(9);
  std::vector<float> x(8000 * 20);
  for (auto& v : x) v = static_cast<float>(0.2 * r.gauss());
  m->rx(x.data(), x.size());
  std::string out;
  RxItem it;
  while (m->pop(&it)) out += it.text;
  CHECK(out.size() <= 3);
  CHECK(mt::garbage_frames(*m, 5, 4) == 0);
}

}  // TEST_SUITE
