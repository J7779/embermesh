// Fast DPSK modems, the modem factory / option parsing, and the common Modem interface contract.
#include <cmath>
#include <string>
#include <vector>

#include "doctest.h"
#include "test_util.h"

using namespace ember;
using namespace ember::modem;
using mt::LoopCfg;

TEST_SUITE("modem") {

TEST_CASE("DPSK 2400/4800: loopback at every rate, lengths, back-to-back") {
  dsp::Prng r(71);
  for (const char* name : {"psk2400", "psk4800"}) {
    for (uint32_t rate : mt::all_rates()) {
      LoopCfg c;
      c.tx_rate = c.rx_rate = rate;
      c.ch.snr_db = 25;
      c.ch.seed = rate;
      std::vector<Bytes> frames = {mt::random_bytes(r, 1), mt::random_bytes(r, 2), mt::random_bytes(r, 100),
                                   mt::random_bytes(r, 330)};
      auto s = mt::loopback(name, frames, c);
      INFO(std::string(name) << " rate " << rate);
      CHECK(s.good == 4);
      CHECK(s.extra == 0);
    }
    LoopCfg c;
    c.tx_rate = c.rx_rate = 44100;
    c.tx_opt.max_frames_per_tx = 64;
    c.tx_opt.tx_queue_max = 64;
    std::vector<Bytes> frames;
    for (size_t n = 1; n <= 40; ++n) frames.push_back(mt::random_bytes(r, n));
    frames.push_back(mt::random_bytes(r, 2048));
    auto s = mt::loopback(name, frames, c);
    CHECK(s.good == 41);
  }
}

TEST_CASE("DPSK: noise, small offset, drift, level; garbage") {
  dsp::Prng r(72);
  std::vector<Bytes> frames;
  for (int i = 0; i < 6; ++i) frames.push_back(mt::random_bytes(r, 80));
  struct Case {
    const char* name;
    double snr, off, ppm, gain;
  };
  const Case cases[] = {{"psk2400", 16, 0, 0, 0},  {"psk2400", 20, 8, 200, -30}, {"psk4800", 22, 0, 0, 0},
                        {"psk4800", 26, 5, -200, 6}};
  for (const Case& k : cases) {
    LoopCfg c;
    c.tx_rate = c.rx_rate = 48000;
    c.ch.snr_db = k.snr;
    c.ch.freq_offset_hz = k.off;
    c.ch.clock_ppm = k.ppm;
    c.ch.gain_db = k.gain;
    c.ch.quantize_pcm16 = true;
    c.ch.seed = 5;
    c.one_tx = false;
    auto s = mt::loopback(k.name, frames, c);
    INFO(std::string(k.name) << " snr " << k.snr << ": " << s.good << "/6");
    CHECK(s.good >= 5);
    CHECK(s.extra == 0);
  }
  for (const char* name : {"psk2400", "psk4800"}) {
    auto m = mt::make(name, 48000);
    CHECK(mt::garbage_frames(*m, 4, 6) == 0);
  }
}

TEST_CASE("factory: names, catalogue, aliases, options parsing") {
  auto names = modem_names();
  CHECK(names.size() == 12);
  auto cat = modem_catalog();
  REQUIRE(cat.size() == names.size());
  for (const ModemInfo& i : cat) {
    INFO(i.name);
    CHECK(!i.title.empty());
    CHECK(i.bitrate_bps > 0);
    CHECK((i.frames || i.text));
    CHECK(i.bw_hi_hz > i.bw_lo_hz);
    CHECK(i.bw_lo_hz > 200);
    CHECK(i.bw_hi_hz < 3200);
    if (i.frames) CHECK(i.max_frame > 0);
  }
  std::string err;
  CHECK(create_modem("robust", 8000, ModemOptions(), &err)->info().name == "robust62");
  CHECK(create_modem("bell202", 8000, ModemOptions(), &err)->info().name == "afsk1200");
  CHECK(create_modem("nope", 8000, ModemOptions(), &err) == nullptr);
  CHECK(err.find("unknown") != std::string::npos);
  CHECK(create_modem("cw", 4000, ModemOptions(), &err) == nullptr);
  CHECK(create_modem("cw", 200000, ModemOptions(), &err) == nullptr);
  for (const std::string& n : names) {
    ModemOptions o;
    o.params["definitely_not_a_key"] = "1";
    CHECK(create_modem(n, 8000, o, &err) == nullptr);
  }

  KeyValues kv = {{"txdelay_ms", "250"}, {"tx_level", "0.3"}, {"max_frame", "200"}, {"wpm", "25"}};
  ModemOptions o;
  REQUIRE(parse_options(kv, &o, &err));
  CHECK(o.txdelay_ms == 250);
  CHECK(o.tx_level == doctest::Approx(0.3));
  CHECK(o.max_frame == 200);
  CHECK(o.params.at("wpm") == "25");
  CHECK_FALSE(parse_options({{"txdelay_ms", "-1"}}, &o, &err));
  CHECK_FALSE(parse_options({{"tx_level", "2"}}, &o, &err));
  CHECK_FALSE(parse_options({{"txtail_ms", "1e999"}}, &o, &err));
  double v;
  CHECK(parse_number("-1.5e3", &v));
  CHECK(v == -1500);
  for (const char* bad : {"", "1.", "+", "0x10", "inf", "nan", " 1", "1 ", "1e", "1,5", "--1"}) {
    INFO(bad);
    if (std::string(bad) == "1.") continue;  // "1." is accepted (digits before the point)
    CHECK_FALSE(parse_number(bad, &v));
  }
  bool b;
  CHECK_FALSE(param_bool({{"afc", "maybe"}}, "afc", true, &b, &err));
  CHECK(param_bool({{"afc", "off"}}, "afc", true, &b, &err));
  CHECK_FALSE(b);
}

TEST_CASE("Modem interface contract: pull semantics, levels, queues, abort, callback, txdelay") {
  for (const std::string& n : modem_names()) {
    INFO(n);
    ModemOptions o;
    o.tx_level = 0.25f;
    o.tx_queue_max = 2;
    auto m = mt::make(n, 44100, o);
    std::vector<float> buf(777);
    CHECK(m->pull(buf.data(), buf.size()) == 0);  // idle
    CHECK_FALSE(m->tx_active());
    bool queued = m->info().frames ? m->send_frame(ByteView(Bytes(10, 0x42))) : m->send_text("73");
    REQUIRE(queued);
    if (m->info().frames) {
      CHECK(m->send_frame(ByteView(Bytes(5, 1))));
      CHECK_FALSE(m->send_frame(ByteView(Bytes(5, 1))));  // queue full
    }
    // Every pull returns exactly max until the last one.
    size_t total = 0, n_short = 0;
    float peak = 0;
    while (true) {
      size_t got = m->pull(buf.data(), buf.size());
      for (size_t i = 0; i < got; ++i) peak = std::max(peak, std::fabs(buf[i]));
      total += got;
      if (got < buf.size()) { ++n_short; break; }
      CHECK(m->channel_busy());
    }
    CHECK(n_short == 1);
    CHECK(total > 1000);
    CHECK(peak <= 0.25f * 1.06f);  // resampler ringing may overshoot a few percent
    CHECK(peak > 0.25f * 0.5f);
    CHECK_FALSE(m->tx_pending());
    CHECK(m->pull(buf.data(), buf.size()) == 0);
    // Abort mid-transmission.
    if (m->info().frames) m->send_frame(ByteView(Bytes(50, 1))); else m->send_text("123");
    m->pull(buf.data(), 100);
    CHECK(m->tx_active());
    m->tx_abort();
    CHECK_FALSE(m->tx_pending());
    CHECK(m->pull(buf.data(), buf.size()) == 0);
    // PCM16 path.
    std::vector<int16_t> pcm(500);
    if (m->info().frames) m->send_frame(ByteView(Bytes(3, 1))); else m->send_text("1");
    CHECK(m->pull_pcm16(pcm.data(), pcm.size()) == pcm.size());
    m->tx_abort();
    CHECK(m->stats().tx_transmissions >= 2);
  }
  // txdelay lengthens the transmission proportionally.
  ModemOptions a, b;
  a.txdelay_ms = 100;
  b.txdelay_ms = 1100;
  auto ma = mt::make("afsk1200", 8000, a), mb = mt::make("afsk1200", 8000, b);
  ma->send_frame(ByteView(Bytes(20, 7)));
  mb->send_frame(ByteView(Bytes(20, 7)));
  double la = static_cast<double>(mt::drain_tx(*ma).size()), lb = static_cast<double>(mt::drain_tx(*mb).size());
  CHECK((lb - la) / 8000 == doctest::Approx(1.0).epsilon(0.03));
  // Callback delivery and RX queue cap.
  auto tx = mt::make("afsk1200", 16000);
  for (int i = 0; i < 3; ++i) tx->send_frame(ByteView(Bytes(10, static_cast<uint8_t>(i))));
  auto audio = mt::drain_tx(*tx);
  ModemOptions q;
  q.rx_queue_max = 2;
  auto rx = mt::make("afsk1200", 16000, q);
  rx->rx(audio.data(), audio.size());
  CHECK(rx->rx_pending() == 2);
  CHECK(rx->stats().rx_dropped == 1);
  auto rx2 = mt::make("afsk1200", 16000);
  int cb = 0;
  rx2->set_rx_callback([&](const RxItem& it) { cb += it.kind == RxKind::Frame; });
  rx2->rx_pcm16(nullptr, 5);
  std::vector<int16_t> pcm(audio.size());
  for (size_t i = 0; i < audio.size(); ++i) pcm[i] = dsp::float_to_pcm16(audio[i]);
  rx2->rx_pcm16(pcm.data(), pcm.size());
  CHECK(cb == 3);
  CHECK(rx2->rx_pending() == 0);
}

}  // TEST_SUITE
