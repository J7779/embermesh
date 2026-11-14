// Long-running measurements: frame success rate vs SNR and CPU cost for every data modem.
// Skipped by default; run with:
//   ./ember_tests -ts=modem -tc="sweep*" --no-skip
// Results are recorded in core/src/modem/PERFORMANCE.md.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "doctest.h"
#include "test_util.h"

using namespace ember;
using namespace ember::modem;

namespace {

struct SweepCfg {
  std::string name;
  size_t frame_len = 50;
  int frames = 50;
  uint32_t rate = 16000;
  int tilt = 0;
  double off = 0, ppm = 0;
  bool bandlimit = true;
  KeyValues params;
};

// Frames are sent as separate transmissions (each with its own preamble), as in real traffic.
double success(const SweepCfg& s, double snr, uint64_t seed) {
  mt::LoopCfg c;
  c.tx_rate = c.rx_rate = s.rate;
  c.ch.seed = seed;
  c.ch.snr_db = snr;
  c.ch.tilt = s.tilt;
  c.ch.freq_offset_hz = s.off;
  c.ch.clock_ppm = s.ppm;
  c.ch.bandlimit = s.bandlimit;
  c.one_tx = false;
  c.rx_opt.params = s.params;
  // PARAMS="k=v,k2=v2" adds receiver params (tuning experiments).
  if (const char* e = std::getenv("PARAMS")) {
    std::string all = e;
    size_t pos = 0;
    while (pos < all.size()) {
      size_t comma = all.find(',', pos);
      if (comma == std::string::npos) comma = all.size();
      std::string kv = all.substr(pos, comma - pos);
      size_t eq = kv.find('=');
      if (eq != std::string::npos) c.rx_opt.params[kv.substr(0, eq)] = kv.substr(eq + 1);
      pos = comma + 1;
    }
  }
  c.rx_opt.rx_queue_max = 100000;
  c.tx_opt.tx_queue_max = 100000;
  dsp::Prng r(seed);
  std::vector<Bytes> frames;
  for (int i = 0; i < s.frames; ++i) frames.push_back(mt::random_bytes(r, s.frame_len));
  auto sc = mt::loopback(s.name, frames, c);
  return 100.0 * sc.good / s.frames;
}

void sweep(const SweepCfg& s, const std::vector<double>& snrs) {
  std::printf("%-10s len=%zu n=%d rate=%u tilt=%d off=%.0f ppm=%.0f %s\n", s.name.c_str(), s.frame_len, s.frames,
              s.rate, s.tilt, s.off, s.ppm, s.params.empty() ? "" : "(params)");
  for (double snr : snrs) std::printf("  %6.1f dB: %5.1f%%\n", snr, success(s, snr, 1234 + static_cast<uint64_t>(snr * 10 + 500)));
  std::fflush(stdout);
}

std::vector<double> range(double a, double b, double step) {
  std::vector<double> v;
  for (double x = a; x <= b + 1e-9; x += step) v.push_back(x);
  return v;
}

// Device-rate realtime factor of RX (continuous signal) and TX.
void cpu(const std::string& name, uint32_t rate) {
  const char* text = name == "dtmf" ? "0123456789*#ABCD" : "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789 ";
  auto tx = mt::make(name, rate);
  auto rx = mt::make(name, rate);
  dsp::Prng r(1);
  double audio_s = 0;
  std::vector<float> a;
  while (audio_s < 20) {
    if (tx->info().frames) tx->send_frame(ByteView(mt::random_bytes(r, 100)));
    else REQUIRE(tx->send_text(text));
    double t = mt::time_s([&] { auto b = mt::drain_tx(*tx); a.insert(a.end(), b.begin(), b.end()); });
    (void)t;
    audio_s = static_cast<double>(a.size()) / rate;
  }
  auto tx2 = mt::make(name, rate);
  double ttx = mt::time_s([&] {
    for (int i = 0; i < 4; ++i) {
      if (tx2->info().frames) tx2->send_frame(ByteView(mt::random_bytes(r, 100)));
      else tx2->send_text(text);
      mt::drain_tx(*tx2);
    }
  });
  double txs = 0;
  {
    auto tx3 = mt::make(name, rate);
    dsp::Prng r2(1);
    for (int i = 0; i < 4; ++i) {
      if (tx3->info().frames) tx3->send_frame(ByteView(mt::random_bytes(r2, 100)));
      else tx3->send_text(text);
      txs += static_cast<double>(mt::drain_tx(*tx3).size()) / rate;
    }
  }
  double trx = mt::time_s([&] {
    for (size_t i = 0; i < a.size(); i += 1024) rx->rx(&a[i], std::min<size_t>(1024, a.size() - i));
  });
  std::printf("CPU %-10s @%5u Hz: RX %7.1fx realtime, TX %7.1fx realtime\n", name.c_str(), rate, audio_s / trx,
              txs / ttx);
  std::fflush(stdout);
}

}  // namespace

TEST_SUITE("modem") {

TEST_CASE("sweep fast psk" * doctest::skip()) {
  for (const char* n : {"psk2400", "psk4800"}) {
    SweepCfg s;
    s.name = n;
    s.frames = 50;
    s.frame_len = 50;
    s.rate = 48000;
    s.bandlimit = false;
    sweep(s, range(n[3] == '2' ? 6 : 12, n[3] == '2' ? 16 : 24, 1));
  }
}

TEST_CASE("sweep afsk" * doctest::skip()) {
  SweepCfg s;
  s.name = "afsk1200";
  s.frames = 100;
  s.frame_len = 50;
  const char* clen = std::getenv("CORR_LEN");
  const char* ccut = std::getenv("CORR_CUT");
  if (clen) s.params["corr_len"] = clen;
  if (ccut) s.params["corr_cut"] = ccut;
  if (const char* w = std::getenv("CORR_WIN")) s.params["corr_win"] = w;
  sweep(s, range(4, 14, 1));
  s.tilt = -1;
  sweep(s, range(4, 14, 1));
  s.tilt = 1;
  sweep(s, range(4, 14, 2));
  s.tilt = 0;
  if (!std::getenv("QUICK")) {
    KeyValues p = s.params;
    s.params["slicers"] = "1";
    sweep(s, range(4, 14, 2));
    s.tilt = -1;
    sweep(s, range(4, 14, 2));
    s.params = p;
    s.tilt = 0;
    s.name = "afsk300";
    s.frame_len = 50;
    s.frames = 50;
    sweep(s, range(-2, 10, 1));
  }
}

TEST_CASE("sweep robust" * doctest::skip()) {
  struct Sp {
    const char* name;
    double lo, hi;
    int frames;
  };
  const Sp sp[] = {{"robust250", -12, -2, 40}, {"robust125", -14, -5, 30}, {"robust62", -17, -8, 20}, {"robust31", -20, -11, 12}};
  const char* only = std::getenv("ONLY");
  for (const Sp& p : sp) {
    if (only && std::string(only) != p.name) continue;
    SweepCfg s;
    s.name = p.name;
    s.frames = p.frames;
    s.frame_len = 32;
    s.rate = 8000;
    sweep(s, range(p.lo, p.hi, 1));
    if (!std::getenv("QUICK")) {
      s.off = 37;
      s.ppm = 200;
      s.tilt = -1;
      sweep(s, range(p.lo + 1, p.hi, 2));
    }
  }
}

// False frames from pure noise over a long period (minutes = env NOISE_MIN, default 10).
TEST_CASE("sweep noise" * doctest::skip()) {
  double minutes = std::getenv("NOISE_MIN") ? std::atof(std::getenv("NOISE_MIN")) : 10.0;
  for (const std::string& n : modem_names()) {
    auto m = mt::make(n, 16000);
    if (!m->info().frames) continue;
    dsp::Prng r(12345);
    std::vector<float> x(16000);
    int frames = 0;
    for (int sec = 0; sec < minutes * 60; ++sec) {
      double lvl = (sec % 3 == 0) ? 0.05 : ((sec % 3 == 1) ? 0.3 : 0.9);
      for (auto& v : x) v = static_cast<float>(r.gauss() * lvl);
      m->rx(x.data(), x.size());
      RxItem it;
      while (m->pop(&it)) frames += it.kind == RxKind::Frame;
    }
    std::printf("NOISE %-10s %.0f min: %d false frames, %u bad\n", n.c_str(), minutes, frames, m->stats().rx_bad);
    std::fflush(stdout);
  }
}

TEST_CASE("sweep cpu" * doctest::skip()) {
  for (const std::string& n : modem_names()) {
    for (uint32_t rate : {8000u, 48000u}) cpu(n, rate);
  }
}

}  // TEST_SUITE
