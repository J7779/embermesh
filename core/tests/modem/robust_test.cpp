// Ember robust MFSK modem: FEC code, codec layer for every length, audio loopback and impairments.
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "doctest.h"
#include "modem/conv_code.h"
#include "modem/robust.h"
#include "test_util.h"

using namespace ember;
using namespace ember::modem;
using mt::LoopCfg;

namespace {

// Demodulation of a tone sequence into LLRs: noise-free, or noncoherent AWGN at es_n0 (linear,
// energy per symbol over noise energy per bin), as an ideal synchronised receiver would see it.
std::vector<float> ideal_llrs(const std::vector<uint8_t>& tones, size_t from, size_t count, dsp::Prng* r,
                              double es_n0) {
  std::vector<float> llr;
  for (size_t i = from; i < from + count; ++i) {
    float e[16];
    for (int k = 0; k < 16; ++k) {
      if (!r) { e[k] = (k == tones[i]) ? 20.0f : 1.0f; continue; }
      double a = (k == tones[i]) ? std::sqrt(es_n0) : 0.0;
      double re = a + r->gauss() * std::sqrt(0.5), im = r->gauss() * std::sqrt(0.5);
      e[k] = static_cast<float>(re * re + im * im);
    }
    float l4[4];
    robust::symbol_llrs(e, r ? static_cast<float>(es_n0) : 19.0f, 1.0f, l4);
    llr.insert(llr.end(), l4, l4 + 4);
  }
  return llr;
}

}  // namespace

TEST_SUITE("modem") {

TEST_CASE("robust: Costas sync is a Costas array; interleaver is a permutation") {
  std::set<uint8_t> vals(robust::kCostas, robust::kCostas + 16);
  CHECK(vals.size() == 16);
  std::set<std::pair<int, int>> vec;
  bool costas = true;
  for (int i = 0; i < 16; ++i)
    for (int j = i + 1; j < 16; ++j)
      costas &= vec.insert({j - i, robust::kCostas[j] - robust::kCostas[i]}).second;
  CHECK(costas);
  for (size_t n : {60u, 92u, 100u, 8252u, 32828u}) {
    size_t a = robust::interleave_step(n);
    std::set<size_t> pos;
    for (size_t j = 0; j < n; ++j) pos.insert((a * j) % n);
    CHECK(pos.size() == n);
    CHECK(a >= 14);
  }
  CHECK(robust::payload_symbols(1) == 23);  // 2*(8*5+6) = 92 bits
}

TEST_CASE("K=7 r=1/2 Viterbi: clean, noisy and burst-free random errors") {
  dsp::Prng r(17);
  std::vector<uint8_t> bits(1000);
  for (auto& b : bits) b = r.u32() & 1;
  bits.insert(bits.end(), 6, 0);
  std::vector<uint8_t> coded;
  conv_encode(bits.data(), bits.size(), coded);
  REQUIRE(coded.size() == 2 * bits.size());
  // Known first outputs: input 1 then zeros produces the generator impulse responses.
  std::vector<uint8_t> imp = {1, 0, 0, 0, 0, 0, 0};
  std::vector<uint8_t> ic;
  conv_encode(imp.data(), imp.size(), ic);
  std::string s;
  for (size_t i = 0; i < ic.size(); i += 2) s += char('0' + ic[i]);
  CHECK(s == "1001111");  // 0171 read from the newest tap (bit 0) to the oldest (bit 6)
  s.clear();
  for (size_t i = 1; i < ic.size(); i += 2) s += char('0' + ic[i]);
  CHECK(s == "1101101");  // 0133
  Viterbi v;
  for (double flip : {0.0, 0.02, 0.04}) {
    std::vector<float> llr(coded.size());
    size_t flips = 0;
    for (size_t i = 0; i < coded.size(); ++i) {
      bool f = r.uniform() < flip;
      flips += f;
      llr[i] = ((coded[i] ^ f) ? 1.0f : -1.0f) * static_cast<float>(1.0 + 0.3 * r.gauss());
    }
    std::vector<uint8_t> out;
    size_t corr = v.decode(llr.data(), bits.size(), out);
    CHECK(out == bits);
    CHECK(corr >= flips / 2);
  }
  std::vector<uint8_t> out;
  CHECK(v.decode(nullptr, 10, out) == 0);
}

void codec_sweep(const std::vector<size_t>& lens) {
  dsp::Prng r(23);
  for (size_t len : lens) {
    Bytes p = mt::random_bytes(r, len);
    std::vector<uint8_t> tones;
    REQUIRE(robust::frame_tones(ByteView(p), tones));
    REQUIRE(tones.size() == 16 + 15 + robust::payload_symbols(len));
    for (int i = 0; i < 16; ++i) CHECK(tones[static_cast<size_t>(i)] == robust::kCostas[i]);
    bool noisy = len % 5 == 0;
    auto h = ideal_llrs(tones, 16, 15, nullptr, 0);
    uint16_t got_len = 0;
    size_t errs = 0;
    REQUIRE(robust::decode_header(h.data(), &got_len, &errs));
    CHECK(got_len == len);
    auto pl = ideal_llrs(tones, 31, robust::payload_symbols(len), noisy ? &r : nullptr, 8.0);  // 9 dB
    Bytes out;
    REQUIRE(robust::decode_payload(pl.data(), len, &out, &errs));
    CHECK(out == p);
    if (noisy && len > 50) CHECK(errs > 0);
  }
}

TEST_CASE("robust codec: every length 1..256, stride to 1024, 4095; errors corrected; bad input") {
  std::vector<size_t> lens;
  for (size_t n = 1; n <= 256; ++n) lens.push_back(n);
  for (size_t n = 257; n <= 1024; n += 37) lens.push_back(n);
  lens.push_back(1024);
  lens.push_back(4095);
  codec_sweep(lens);
  std::vector<uint8_t> t;
  CHECK_FALSE(robust::frame_tones(ByteView(), t));
  CHECK_FALSE(robust::frame_tones(ByteView(Bytes(4096, 0)), t));
  // A corrupted header is rejected.
  std::vector<uint8_t> tones;
  robust::frame_tones(ByteView(Bytes(10, 1)), tones);
  for (int i = 16; i < 31; ++i) tones[static_cast<size_t>(i)] = static_cast<uint8_t>((tones[static_cast<size_t>(i)] + 5) % 16);
  auto h = ideal_llrs(tones, 16, 15, nullptr, 0);
  uint16_t l;
  CHECK_FALSE(robust::decode_header(h.data(), &l, nullptr));
}

TEST_CASE("robust codec: every payload length 1..1024 (slow)" * doctest::skip()) {
  std::vector<size_t> lens;
  for (size_t n = 1; n <= 1024; ++n) lens.push_back(n);
  codec_sweep(lens);
}

TEST_CASE("robust: loopback for every speed; fast speed at every sample rate") {
  dsp::Prng r(31);
  for (const char* name : {"robust250", "robust125", "robust62", "robust31"}) {
    LoopCfg c;
    c.tx_rate = c.rx_rate = 16000;
    c.ch.snr_db = 10;
    c.ch.seed = 3;
    c.tx_opt.txdelay_ms = 0;
    std::vector<Bytes> frames = {mt::random_bytes(r, 1), mt::random_bytes(r, 12)};
    auto s = mt::loopback(name, frames, c);
    INFO(name);
    CHECK(s.good == 2);
    CHECK(s.extra == 0);
  }
  for (uint32_t rate : mt::all_rates()) {
    LoopCfg c;
    c.tx_rate = c.rx_rate = rate;
    std::vector<Bytes> frames = {mt::random_bytes(r, 7), mt::random_bytes(r, 64), mt::random_bytes(r, 3)};
    auto s = mt::loopback("robust250", frames, c);
    INFO("rate " << rate);
    CHECK(s.good == 3);
  }
}

TEST_CASE("robust250: large frames, back-to-back, separate transmissions, quality fields") {
  dsp::Prng r(41);
  LoopCfg c;
  c.tx_rate = c.rx_rate = 8000;
  c.ch.snr_db = 5;
  c.ch.seed = 9;
  std::vector<Bytes> frames = {mt::random_bytes(r, 1024), mt::random_bytes(r, 200), mt::random_bytes(r, 1)};
  auto s = mt::loopback("robust250", frames, c);
  CHECK(s.good == 3);
  REQUIRE(!s.items.empty());
  CHECK(s.items[0].q.has_snr);
  CHECK(std::fabs(s.items[0].q.snr_db - 5) < 3);
  c.one_tx = false;
  c.ch.seed = 10;
  frames[0] = mt::random_bytes(r, 100);
  auto s2 = mt::loopback("robust250", frames, c);
  CHECK(s2.good == 3);
  auto m = mt::make("robust250", 8000);
  CHECK(m->info().max_frame == 1024);
  CHECK_FALSE(m->send_frame(ByteView(Bytes(1025, 0))));
  ModemOptions big;
  big.max_frame = 4095;
  CHECK(mt::make("robust250", 8000, big)->info().max_frame == 4095);
}

TEST_CASE("robust: weak signal, offset, drift, tilt, level extremes") {
  dsp::Prng r(51);
  std::vector<Bytes> frames;
  for (int i = 0; i < 4; ++i) frames.push_back(mt::random_bytes(r, 32));
  struct Case {
    const char* name;
    double snr, off, ppm;
    int tilt;
    double gain, clip;
  };
  const Case cases[] = {
      {"robust250", -4, 0, 0, 0, 0, 0},
      {"robust125", -7, 0, 0, 0, 0, 0},
      {"robust250", 0, 70, 0, 0, 0, 0},
      {"robust250", 0, -110, 0, 0, 0, 0},
      {"robust125", 0, 47, 300, 0, 0, 0},
      {"robust125", 0, -20, -300, 0, 0, 0},
      {"robust250", 0, 0, 1000, -1, 0, 0},
      {"robust250", 3, 0, -1000, +1, 0, 0},
      {"robust250", 10, 0, 0, 0, -50, 0},
      {"robust250", 10, 0, 0, -1, 20, 0.3},
  };
  for (const Case& k : cases) {
    LoopCfg c;
    c.tx_rate = c.rx_rate = 11025;
    c.ch.snr_db = k.snr;
    c.ch.freq_offset_hz = k.off;
    c.ch.clock_ppm = k.ppm;
    c.ch.tilt = k.tilt;
    c.ch.gain_db = k.gain;
    c.ch.clip_level = k.clip;
    c.ch.quantize_pcm16 = true;
    c.ch.bandlimit = true;
    c.ch.seed = 77;
    c.one_tx = false;
    auto s = mt::loopback(k.name, frames, c);
    INFO(std::string(k.name) << " snr " << k.snr << " off " << k.off << " ppm " << k.ppm << " tilt " << k.tilt
                             << " gain " << k.gain << ": " << s.good << "/4");
    CHECK(s.good >= 3);
    CHECK(s.extra == 0);
  }
}

TEST_CASE("robust: truncated transmission, noise, garbage") {
  dsp::Prng r(61);
  auto tx = mt::make("robust250", 8000);
  auto rx = mt::make("robust250", 8000);
  Bytes a = mt::random_bytes(r, 100), b = mt::random_bytes(r, 20);
  tx->send_frame(ByteView(a));
  auto audio = mt::drain_tx(*tx);
  audio.resize(audio.size() / 2);
  tx->send_frame(ByteView(b));
  auto audio2 = mt::drain_tx(*tx);
  audio.insert(audio.end(), audio2.begin(), audio2.end());
  audio.insert(audio.end(), 8000, 0.0f);
  auto items = mt::feed(*rx, audio);
  REQUIRE(items.size() == 1);
  CHECK(items[0].data == b);

  for (const char* name : {"robust250", "robust31"}) {
    auto m = mt::make(name, 22050);
    CHECK(mt::garbage_frames(*m, name[6] == '2' ? 8.0 : 4.0, 5) == 0);
  }
}

TEST_CASE("robust: DCD follows the signal") {
  auto tx = mt::make("robust125", 8000);
  auto rx = mt::make("robust125", 8000);
  tx->send_frame(ByteView(Bytes(40, 0x55)));
  auto audio = mt::drain_tx(*tx);
  CHECK_FALSE(rx->dcd());
  size_t busy = 0, chunks = 0;
  for (size_t i = 0; i + 400 <= audio.size(); i += 400) {
    rx->rx(&audio[i], 400);
    busy += rx->dcd();
    ++chunks;
  }
  CHECK(busy > chunks * 3 / 4);
  dsp::Prng r(1);
  std::vector<float> nz(16000);
  for (auto& v : nz) v = static_cast<float>(0.1 * r.gauss());
  rx->rx(nz.data(), nz.size());
  CHECK_FALSE(rx->dcd());
}

}  // TEST_SUITE
