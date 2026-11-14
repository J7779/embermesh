// AFSK 1200 / 300 HDLC modems: on-air waveform checks, loopback at every rate, impairments.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "doctest.h"
#include "dsp/goertzel.h"
#include "dsp/nbfm.h"
#include "dsp/resampler.h"
#include "ember/crc.h"
#include "modem/hdlc.h"
#include "test_util.h"

using namespace ember;
using namespace ember::modem;
using mt::LoopCfg;

namespace {

Bytes ax25_ui(const std::string& info) {
  Bytes f;
  for (char c : std::string("APRS  ")) f.push_back(static_cast<uint8_t>(c << 1));
  f.push_back(0x60);
  for (char c : std::string("N0CALL")) f.push_back(static_cast<uint8_t>(c << 1));
  f.push_back(static_cast<uint8_t>(0x60 | (7 << 1) | 1));
  f.push_back(0x03);
  f.push_back(0xF0);
  for (char c : info) f.push_back(static_cast<uint8_t>(c));
  return f;
}

std::vector<Bytes> frames_of_lengths(std::initializer_list<size_t> lens, uint64_t seed) {
  dsp::Prng r(seed);
  std::vector<Bytes> v;
  for (size_t n : lens) v.push_back(mt::random_bytes(r, n));
  return v;
}

}  // namespace

TEST_SUITE("modem") {

TEST_CASE("AFSK1200 on-air tones match NRZI(HDLC(AX.25 UI frame)) bit for bit") {
  Bytes f = ax25_ui(">EmberMesh interop test");
  ModemOptions o;
  o.txdelay_ms = 0;  // -> exactly 1 opening flag
  o.txtail_ms = 0;   // -> exactly 1 closing flag
  auto m = mt::make("afsk1200", 48000, o);
  REQUIRE(m->send_frame(ByteView(f)));
  auto audio = mt::drain_tx(*m, 4800);
  auto bits = hdlc_encode({f}, 1, 1);
  uint8_t lvl = 1;
  nrzi_encode(bits, &lvl);
  const size_t spb = 40;  // 48000 / 1200
  REQUIRE(audio.size() >= bits.size() * spb);
  size_t mismatches = 0;
  for (size_t i = 0; i < bits.size(); ++i) {
    dsp::Goertzel g1(1200, 48000), g2(2200, 48000);
    for (size_t k = i * spb + 8; k < i * spb + 32; ++k) { g1.push(audio[k]); g2.push(audio[k]); }
    uint8_t tone = g1.power() > g2.power() ? 1 : 0;
    if (tone != bits[i]) ++mismatches;
  }
  CHECK(mismatches == 0);
  // Phase continuity: no sample-to-sample jump bigger than a 2200 Hz sine can make.
  float maxstep = 0;
  for (size_t i = 1; i < audio.size(); ++i) maxstep = std::max(maxstep, std::fabs(audio[i] - audio[i - 1]));
  CHECK(maxstep < 0.5f * 2 * static_cast<float>(dsp::kPi) * 2200 / 48000 * 1.2f);
  // Level honours tx_level.
  float peak = 0;
  for (float v : audio) peak = std::max(peak, std::fabs(v));
  CHECK(peak == doctest::Approx(0.5).epsilon(0.03));
  // The receiver decodes it (with a normal txdelay so the PLL can lock before the frame).
  auto m2 = mt::make("afsk1200", 48000);
  REQUIRE(m2->send_frame(ByteView(f)));
  auto rx = mt::make("afsk1200", 48000);
  auto items = mt::feed(*rx, mt::drain_tx(*m2));
  REQUIRE(items.size() == 1);
  CHECK(items[0].data == f);
}

TEST_CASE("AFSK1200 loopback at every sample rate") {
  for (uint32_t rate : mt::all_rates()) {
    LoopCfg c;
    c.tx_rate = rate;
    c.rx_rate = rate;
    auto frames = frames_of_lengths({1, 2, 3, 17, 64, 200, 330}, rate);
    auto s = mt::loopback("afsk1200", frames, c);
    INFO("rate " << rate);
    CHECK(s.good == s.sent);
    CHECK(s.extra == 0);
    CHECK(s.dups == 0);
  }
  // TX and RX at different rates (two different sound cards).
  LoopCfg c;
  c.tx_rate = 44100;
  c.rx_rate = 8000;
  auto frames = frames_of_lengths({5, 100, 256}, 3);
  CHECK(mt::loopback("afsk1200", frames, c).good == 3);
}

TEST_CASE("AFSK1200 every frame length 1..64 plus large sizes, back-to-back") {
  std::vector<Bytes> frames;
  dsp::Prng r(5);
  for (size_t n = 1; n <= 64; ++n) frames.push_back(mt::random_bytes(r, n));
  LoopCfg c;
  c.tx_rate = c.rx_rate = 16000;
  c.tx_opt.max_frames_per_tx = 64;
  c.tx_opt.tx_queue_max = 64;
  auto s = mt::loopback("afsk1200", frames, c);
  CHECK(s.good == 64);
  CHECK(s.extra == 0);
  // Large frames, including the maximum.
  auto big = frames_of_lengths({511, 1024, 2048}, 9);
  auto s2 = mt::loopback("afsk1200", big, c);
  CHECK(s2.good == 3);
  // max + 1 refused.
  auto m = mt::make("afsk1200", 16000);
  CHECK(m->info().max_frame == 2048);
  CHECK_FALSE(m->send_frame(ByteView(Bytes(2049, 1))));
  CHECK_FALSE(m->send_frame(ByteView()));
}

TEST_CASE("AFSK1200 under AWGN, tilt, drift, offset, level extremes") {
  auto frames = frames_of_lengths({60, 60, 60, 60, 60, 60, 60, 60, 60, 60}, 21);
  struct Case {
    const char* what;
    double snr;
    int tilt;
    double ppm, off, gain, clip;
    bool quant;
    int min_good;
  };
  const Case cases[] = {
      {"awgn 15 dB", 15, 0, 0, 0, 0, 0, false, 10},
      {"awgn 12 dB", 12, 0, 0, 0, 0, 0, false, 9},
      {"de-emphasis + 15 dB", 15, -1, 0, 0, 0, 0, false, 10},
      {"pre-emphasis + 15 dB", 15, +1, 0, 0, 0, 0, false, 10},
      {"+1000 ppm clock", 20, 0, 1000, 0, 0, 0, false, 10},
      {"-1000 ppm clock", 20, 0, -1000, 0, 0, 0, false, 10},
      {"+40 Hz offset", 20, 0, 0, 40, 0, 0, false, 10},
      {"-40 Hz offset", 20, 0, 0, -40, 0, 0, false, 10},
      {"very quiet (-54 dB, pcm16)", 25, 0, 0, 0, -54, 0, true, 10},
      {"clipped (+20 dB into 0.5)", 25, 0, 0, 0, 20, 0.5, true, 10},
      {"de-emphasis + clipped", 20, -1, 0, 0, 16, 0.6, true, 10},
  };
  for (const Case& k : cases) {
    LoopCfg c;
    c.tx_rate = c.rx_rate = 22050;
    c.ch.seed = 1000 + static_cast<uint64_t>(k.snr * 10);
    c.ch.snr_db = k.snr;
    c.ch.tilt = k.tilt;
    c.ch.clock_ppm = k.ppm;
    c.ch.freq_offset_hz = k.off;
    c.ch.gain_db = k.gain;
    c.ch.clip_level = k.clip;
    c.ch.quantize_pcm16 = k.quant;
    c.ch.bandlimit = true;
    auto s = mt::loopback("afsk1200", frames, c);
    INFO(std::string(k.what) << ": " << s.good << "/10");
    CHECK(s.good >= k.min_good);
    CHECK(s.extra == 0);
  }
}

TEST_CASE("AFSK1200 leading/trailing noise, truncated transmission, separate transmissions") {
  auto frames = frames_of_lengths({40, 41, 42}, 8);
  LoopCfg c;
  c.tx_rate = c.rx_rate = 11025;
  c.ch.snr_db = 20;
  c.lead_ms = 3000;
  c.trail_ms = 3000;
  c.one_tx = false;
  auto s = mt::loopback("afsk1200", frames, c);
  CHECK(s.good == 3);

  // Truncated: cut the transmission in the middle of the frame, then a complete one follows.
  auto tx = mt::make("afsk1200", 11025);
  auto rx = mt::make("afsk1200", 11025);
  tx->send_frame(ByteView(frames[0]));
  auto a = mt::drain_tx(*tx);
  a.resize(a.size() / 2);
  tx->send_frame(ByteView(frames[1]));
  auto b = mt::drain_tx(*tx);
  a.insert(a.end(), b.begin(), b.end());
  auto items = mt::feed(*rx, a);
  REQUIRE(items.size() == 1);
  CHECK(items[0].data == frames[1]);
}

TEST_CASE("AFSK1200 single-slicer mode, DCD, quality metrics") {
  ModemOptions o;
  o.params["slicers"] = "1";
  auto tx = mt::make("afsk1200", 48000);
  auto rx = mt::make("afsk1200", 48000, o);
  Bytes f = ax25_ui("dcd");
  tx->send_frame(ByteView(f));
  CHECK(tx->tx_pending());
  CHECK_FALSE(tx->tx_active());
  auto a = mt::drain_tx(*tx);
  CHECK_FALSE(tx->tx_active());
  CHECK_FALSE(rx->dcd());
  bool saw_dcd = false;
  for (size_t i = 0; i + 480 <= a.size(); i += 480) {
    rx->rx(&a[i], 480);
    saw_dcd |= rx->dcd();
  }
  CHECK(saw_dcd);
  RxItem it;
  REQUIRE(rx->pop(&it));
  CHECK(it.data == f);
  CHECK(it.q.has_snr);
  CHECK(it.q.snr_db > 15);
  CHECK(it.q.level_dbfs < -4);
  CHECK(it.q.level_dbfs > -10);
  CHECK(rx->stats().rx_frames == 1);
  // Noise does not hold DCD.
  dsp::Prng r(3);
  std::vector<float> nz(48000);
  for (auto& v : nz) v = static_cast<float>(r.gauss() * 0.2);
  rx->rx(nz.data(), nz.size());
  CHECK_FALSE(rx->dcd());
}

TEST_CASE("AFSK pure noise and garbage: no frames, no crash") {
  for (const char* name : {"afsk1200", "afsk300"}) {
    auto m = mt::make(name, 44100);
    CHECK(mt::garbage_frames(*m, 5.0, 99) == 0);
  }
}

TEST_CASE("AFSK300 HF packet loopback, offset and noise") {
  auto frames = frames_of_lengths({1, 20, 100}, 4);
  for (uint32_t rate : {8000u, 48000u}) {
    LoopCfg c;
    c.tx_rate = c.rx_rate = rate;
    auto s = mt::loopback("afsk300", frames, c);
    CHECK(s.good == 3);
  }
  LoopCfg c;
  c.tx_rate = c.rx_rate = 16000;
  c.ch.snr_db = 10;
  c.ch.freq_offset_hz = 25;
  c.ch.seed = 4;
  auto s = mt::loopback("afsk300", frames, c);
  CHECK(s.good == 3);
}

TEST_CASE("AFSK1200 through an rtl_tcp-style NBFM receive chain (u8 IQ -> NbfmDemod -> modem)") {
  auto tx = mt::make("afsk1200", 48000);
  Bytes f = ax25_ui(">via SDR");
  tx->send_frame(ByteView(f));
  auto audio = mt::drain_tx(*tx);  // peak 0.5
  // FM-modulate at 240 kS/s: audio peak 0.5 -> 3 kHz deviation, carrier 30 kHz off centre.
  dsp::Resampler up;
  REQUIRE(up.init(48000, 240000));
  std::vector<float> a240;
  up.process(audio.data(), audio.size(), a240);
  up.flush(a240);
  dsp::Prng r(3);
  std::vector<uint8_t> iq;
  double ph = 0;
  for (size_t i = 0; i < a240.size() + 24000; ++i) {
    double m = i < a240.size() ? a240[i] : 0.0;
    ph += dsp::kTwoPi * (30000 + 6000 * m) / 240000;
    double I = 127.5 + 60 * std::cos(ph) + 4 * r.gauss(), Q = 127.5 + 60 * std::sin(ph) + 4 * r.gauss();
    iq.push_back(static_cast<uint8_t>(std::lround(dsp::clampf(static_cast<float>(I), 0, 255))));
    iq.push_back(static_cast<uint8_t>(std::lround(dsp::clampf(static_cast<float>(Q), 0, 255))));
  }
  dsp::NbfmConfig c;
  c.iq_rate = 240000;
  c.audio_rate = 22050;
  c.offset_hz = 30000;
  dsp::NbfmDemod d;
  REQUIRE(d.init(c));
  std::vector<float> rxaudio;
  d.process_u8(iq.data(), iq.size(), rxaudio);
  auto rx = mt::make("afsk1200", 22050);
  auto items = mt::feed(*rx, rxaudio);
  REQUIRE(items.size() == 1);
  CHECK(items[0].data == f);
}

TEST_CASE("AFSK parameter validation") {
  std::string err;
  ModemOptions o;
  o.params["mark_hz"] = "abc";
  CHECK(create_modem("afsk1200", 8000, o, &err) == nullptr);
  o.params["mark_hz"] = "2100";
  CHECK(create_modem("afsk1200", 8000, o, &err) == nullptr);  // too close to 2200
  o.params.clear();
  o.params["bogus"] = "1";
  CHECK(create_modem("afsk1200", 8000, o, &err) == nullptr);
  CHECK(err.find("bogus") != std::string::npos);
  CHECK(create_modem("afsk1200", 7999, ModemOptions(), &err) == nullptr);
}

}  // TEST_SUITE
