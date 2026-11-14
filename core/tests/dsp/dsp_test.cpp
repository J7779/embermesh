// Tests for core/src/dsp: oscillators, FFT, FIR/IIR design, detectors, AGC, level/SNR, resampler,
// channel simulator and NBFM demodulator.
#include <algorithm>
#include <cmath>
#include <vector>

#include "doctest.h"
#include "dsp/agc.h"
#include "dsp/biquad.h"
#include "dsp/channel_sim.h"
#include "dsp/dsp.h"
#include "dsp/fft.h"
#include "dsp/fir.h"
#include "dsp/goertzel.h"
#include "dsp/level.h"
#include "dsp/nbfm.h"
#include "dsp/nco.h"
#include "dsp/resampler.h"

using namespace ember::dsp;

namespace {

std::vector<float> tone(double f, double fs, size_t n, float amp = 0.5f, double phase = 0) {
  std::vector<float> v(n);
  for (size_t i = 0; i < n; ++i) v[i] = amp * static_cast<float>(std::cos(kTwoPi * f * i / fs + phase));
  return v;
}

// Mean-square tone power at f over x[from, from+len).
float tone_pow(const std::vector<float>& x, double f, double fs, size_t from = 0, size_t len = 0) {
  if (!len || from + len > x.size()) len = x.size() > from ? x.size() - from : 0;
  Goertzel g(f, fs);
  for (size_t i = from; i < from + len; ++i) g.push(x[i]);
  return g.tone_power();
}

double mean_square(const std::vector<float>& x, size_t from = 0, size_t len = 0) {
  if (!len || from + len > x.size()) len = x.size() - from;
  double a = 0;
  for (size_t i = from; i < from + len; ++i) a += static_cast<double>(x[i]) * x[i];
  return len ? a / len : 0;
}

}  // namespace

TEST_SUITE("dsp") {

TEST_CASE("NCO frequency, phase continuity and table accuracy") {
  double maxerr = 0;
  for (uint32_t p = 0; p < 0xFFFF0000u; p += 0x00100001u) {
    double e = std::fabs(sin_u32(p) - std::sin(kTwoPi * p / 4294967296.0));
    if (e > maxerr) maxerr = e;
  }
  CHECK(maxerr < 1e-5);

  Nco n(1234.5, 8000);
  CHECK(n.cycles_per_sample() == doctest::Approx(1234.5 / 8000).epsilon(1e-6));
  std::vector<float> v(8000);
  for (auto& s : v) s = n.next_cos();
  CHECK(tone_pow(v, 1234.5, 8000) == doctest::Approx(0.5).epsilon(0.01));
  // Negative frequencies and complex output rotate the right way.
  Nco m(-1000, 8000);
  Cf a = m.next(), b = m.next();
  CHECK(carg(b * conj(a)) == doctest::Approx(-kTwoPi * 1000 / 8000).epsilon(1e-3));
  // Garbage input is harmless.
  Nco bad(NAN, 8000);
  bad.set_freq(1e30, 0);
  (void)bad.next();
}

TEST_CASE("FFT matches a direct DFT and inverts") {
  Fft f;
  CHECK_FALSE(f.init(0));
  CHECK_FALSE(f.init(12));
  CHECK_FALSE(f.init(1 << 17));
  REQUIRE(f.init(64));
  Prng r(7);
  std::vector<Cf> x(64), X(64);
  for (auto& c : x) c = Cf(static_cast<float>(r.gauss()), static_cast<float>(r.gauss()));
  X = x;
  f.forward(X.data());
  double maxerr = 0;
  for (size_t k = 0; k < 64; ++k) {
    Cf acc;
    for (size_t n = 0; n < 64; ++n) acc += x[n] * expj(-kTwoPi * k * n / 64);
    double e = cabs(acc - X[k]);
    if (e > maxerr) maxerr = e;
  }
  CHECK(maxerr < 1e-3);
  f.inverse(X.data());
  for (size_t n = 0; n < 64; ++n) {
    CHECK(X[n].re / 64 == doctest::Approx(x[n].re).epsilon(1e-4));
    CHECK(X[n].im / 64 == doctest::Approx(x[n].im).epsilon(1e-4));
  }
}

TEST_CASE("FIR design responses") {
  double fs = 8000;
  auto lp = fir_lowpass(101, 1000, fs, Window::Kaiser, kaiser_beta(60));
  CHECK(fir_gain(lp, 0, fs) == doctest::Approx(1).epsilon(1e-3));
  CHECK(fir_gain(lp, 500, fs) == doctest::Approx(1).epsilon(0.01));
  CHECK(lin_to_db(static_cast<float>(fir_gain(lp, 1700, fs))) < -55);
  auto hp = fir_highpass(101, 1000, fs, Window::Kaiser, kaiser_beta(60));
  CHECK(fir_gain(hp, 3000, fs) == doctest::Approx(1).epsilon(0.01));
  CHECK(lin_to_db(static_cast<float>(fir_gain(hp, 300, fs))) < -55);
  auto bp = fir_bandpass(127, 1000, 2000, fs, Window::Blackman);
  CHECK(fir_gain(bp, 1500, fs) == doctest::Approx(1).epsilon(0.01));
  CHECK(lin_to_db(static_cast<float>(fir_gain(bp, 300, fs))) < -50);
  CHECK(lin_to_db(static_cast<float>(fir_gain(bp, 3000, fs))) < -50);
  auto hb = fir_hilbert(63);
  CHECK(fir_gain(hb, 1000, fs) == doctest::Approx(1).epsilon(0.02));
  auto rrc = fir_rrc(8, 8, 0.35);
  double e = 0;
  for (float v : rrc) e += v * v;
  CHECK(e == doctest::Approx(1).epsilon(1e-4));
  CHECK(rrc.size() == 65);
  size_t nt = kaiser_taps(60, 200, 8000);
  CHECK(nt % 2 == 1);
  CHECK(nt > 50);
  CHECK(nt < 200);
  // Degenerate arguments never crash.
  CHECK(fir_lowpass(0, 1000, fs).size() == 1);
  CHECK(fir_lowpass(5, -1, 0).size() == 1);
  CHECK(fir_bandpass(5, 3000, 1000, fs).size() == 5);
}

TEST_CASE("Streaming FIR equals direct convolution, decimators keep phase") {
  Prng r(3);
  std::vector<float> h(17), x(200);
  for (auto& v : h) v = static_cast<float>(r.gauss());
  for (auto& v : x) v = static_cast<float>(r.gauss());
  Fir f(h);
  FirC fc(h);
  for (size_t n = 0; n < x.size(); ++n) {
    double acc = 0;
    for (size_t k = 0; k < h.size() && k <= n; ++k) acc += h[k] * x[n - k];
    float y = f.process(x[n]);
    Cf yc = fc.process(Cf(x[n], -x[n]));
    CHECK(y == doctest::Approx(acc).epsilon(1e-4));
    CHECK(yc.im == doctest::Approx(-acc).epsilon(1e-4));
  }
  FirDecim d;
  d.init(h, 4);
  Fir ref(h);
  int outs = 0;
  for (size_t n = 0; n < x.size(); ++n) {
    float o, rv = ref.process(x[n]);
    if (d.push(x[n], &o)) { ++outs; CHECK(o == doctest::Approx(rv).epsilon(1e-5)); }
  }
  CHECK(outs == 50);
  Delay dl(3);
  CHECK(dl.process(1) == 0);
  dl.process(2); dl.process(3);
  CHECK(dl.process(4) == 1);
}

TEST_CASE("Biquads and emphasis tilt") {
  double fs = 48000;
  CHECK(lin_to_db(static_cast<float>(Biquad::lowpass(1000, fs).gain(1000, fs))) == doctest::Approx(-3.01).epsilon(0.01));
  CHECK(Biquad::lowpass(1000, fs).gain(10, fs) == doctest::Approx(1).epsilon(1e-3));
  CHECK(Biquad::highpass(1000, fs).gain(20000, fs) == doctest::Approx(1).epsilon(1e-2));
  CHECK(Biquad::bandpass(1500, fs, 5).gain(1500, fs) == doctest::Approx(1).epsilon(1e-3));
  CHECK(Biquad::notch(1500, fs, 5).gain(1500, fs) < 1e-3);
  CHECK(lin_to_db(static_cast<float>(Biquad::peaking(1500, fs, 2, 6).gain(1500, fs))) == doctest::Approx(6).epsilon(0.01));
  CHECK(lin_to_db(static_cast<float>(Biquad::lowshelf(200, fs, -10).gain(20, fs))) == doctest::Approx(-10).epsilon(0.02));
  CHECK(lin_to_db(static_cast<float>(Biquad::highshelf(5000, fs, 4).gain(20000, fs))) == doctest::Approx(4).epsilon(0.05));
  // De-emphasis with a 300 Hz corner: unity at 1 kHz, ~ -5.1 dB more at 2200 than at 1200.
  double tau = 1 / (kTwoPi * 300);
  Biquad de = Biquad::deemphasis(tau, fs);
  CHECK(de.gain(1000, fs) == doctest::Approx(1).epsilon(1e-3));
  double tw = lin_to_db(static_cast<float>(de.gain(2200, fs) / de.gain(1200, fs)));
  CHECK(tw < -4.5);
  CHECK(tw > -5.5);
  Biquad pre = Biquad::preemphasis(tau, fs);
  CHECK(pre.gain(1000, fs) == doctest::Approx(1).epsilon(1e-3));
  CHECK(pre.gain(2200, fs) > pre.gain(1200, fs) * 1.6);
  // Streaming result agrees with gain().
  Biquad lp = Biquad::lowpass(2000, 8000);
  auto x = tone(1000, 8000, 8000);
  for (auto& v : x) v = lp.process(v);
  CHECK(std::sqrt(tone_pow(x, 1000, 8000, 4000) / 0.125) ==
        doctest::Approx(Biquad::lowpass(2000, 8000).gain(1000, 8000)).epsilon(0.01));
}

TEST_CASE("Goertzel, AGC, DC blocker, peak/valley") {
  auto x = tone(697, 8000, 205, 0.3f);
  Goertzel g(697, 8000);
  for (float v : x) g.push(v);
  CHECK(g.tone_power() == doctest::Approx(0.045).epsilon(0.03));
  Goertzel g2(1209, 8000);
  for (float v : x) g2.push(v);
  CHECK(g2.tone_power() < 0.045 * 0.01);

  Agc agc;
  agc.init(8000, 0.002, 0.2, 0.25f);
  auto q = tone(1000, 8000, 16000, 0.001f);
  std::vector<float> y;
  for (float v : q) y.push_back(agc.process(v));
  CHECK(std::sqrt(mean_square(y, 8000)) == doctest::Approx(0.25).epsilon(0.1));
  CHECK(agc.process(NAN) != agc.process(NAN));  // NaN passes through, but state stays finite
  CHECK(std::isfinite(agc.gain()));

  DcBlocker dc;
  dc.init(8000, 20);
  float last = 0;
  for (int i = 0; i < 8000; ++i) last = dc.process(0.5f);
  CHECK(std::fabs(last) < 1e-3);

  PeakValley pv;
  pv.init(0.5f, 0.001f);
  for (int i = 0; i < 1000; ++i) pv.push(i % 2 ? 1.0f : 0.2f);
  CHECK(pv.peak() == doctest::Approx(1).epsilon(0.05));
  CHECK(pv.valley() == doctest::Approx(0.2).epsilon(0.1));
  CHECK(pv.normalise(1.0f) == doctest::Approx(0.5).epsilon(0.05));
}

TEST_CASE("Level meter and band SNR estimator") {
  LevelMeter m;
  m.init(8000);
  auto x = tone(1000, 8000, 16000, 0.5f);
  m.push(x.data(), x.size());
  CHECK(m.rms_dbfs() == doctest::Approx(-6.02).epsilon(0.02));
  CHECK(m.peak_dbfs() == doctest::Approx(-6.02).epsilon(0.02));
  CHECK(m.clip_fraction() < 0.01);
  std::vector<float> c(8000, 1.0f);
  m.push(c.data(), c.size());
  CHECK(m.clip_fraction() > 0.9);

  for (double snr : {0.0, 10.0, 20.0}) {
    ChannelParams p;
    p.snr_db = snr;
    p.seed = 11;
    // A "signal" occupying 1000-2000 Hz: band-pass noise.
    Prng r(5);
    std::vector<float> s(48000);
    Fir bp(fir_bandpass(255, 1000, 2000, 8000, Window::Blackman));
    for (auto& v : s) v = bp.process(static_cast<float>(r.gauss()));
    ChannelSim sim(p, 8000);
    auto rx = sim.run(s);
    BandSnrEstimator est;
    REQUIRE(est.init(8000, 1000, 2000, 2500, 3500, 3000, 256, 64));
    est.push(rx.data(), rx.size());
    float e = 0;
    REQUIRE(est.snr_db(&e));
    CHECK(std::fabs(e - snr) < 1.5);
  }
}

TEST_CASE("Resampler between all supported rates preserves tone frequency and level") {
  const double rates[] = {8000, 11025, 16000, 22050, 32000, 44100, 48000};
  for (double a : rates) {
    for (double b : {8000.0, 12000.0, 48000.0, 11025.0}) {
      Resampler rs;
      REQUIRE(rs.init(a, b));
      auto x = tone(1000, a, static_cast<size_t>(a), 0.5f);  // 1 second
      std::vector<float> y;
      // Feed in odd chunk sizes.
      size_t pos = 0, chunk = 1;
      while (pos < x.size()) {
        size_t n = std::min(chunk, x.size() - pos);
        rs.process(&x[pos], n, y);
        pos += n;
        chunk = chunk * 3 % 997 + 1;
      }
      CHECK(std::fabs(static_cast<double>(y.size()) - b) < b * 0.01 + 200);
      size_t skip = y.size() / 4;
      CHECK(tone_pow(y, 1000, b, skip, y.size() / 2) == doctest::Approx(0.125).epsilon(0.01));
      // Little energy anywhere else.
      double total = mean_square(y, skip, y.size() / 2);
      CHECK(total == doctest::Approx(0.125).epsilon(0.01));
    }
  }
  // Image rejection when up-sampling 8k -> 48k: a 3 kHz tone must not produce 5 kHz.
  Resampler up;
  REQUIRE(up.init(8000, 48000));
  auto x = tone(3000, 8000, 8000, 0.5f);
  std::vector<float> y;
  up.process(x.data(), x.size(), y);
  float img = tone_pow(y, 5000, 48000, 4000, 40000);
  float sig = tone_pow(y, 3000, 48000, 4000, 40000);
  CHECK(pow_to_db(img / sig) < -65);
  // Alias rejection when down-sampling 48k -> 8k: a 5 kHz tone must vanish.
  Resampler dn;
  REQUIRE(dn.init(48000, 8000));
  auto z = tone(5000, 48000, 48000, 0.5f);
  std::vector<float> w;
  dn.process(z.data(), z.size(), w);
  CHECK(pow_to_db(static_cast<float>(mean_square(w, 2000) / 0.125)) < -60);
  // Bad configurations are refused; unready resamplers do nothing.
  Resampler bad;
  CHECK_FALSE(bad.init(0, 8000));
  CHECK_FALSE(bad.init(8000, 8000 * 100.0));
  std::vector<float> o;
  bad.process(x.data(), x.size(), o);
  CHECK(o.empty());
  // Pass-through and drift.
  Resampler same;
  REQUIRE(same.init(8000, 8000));
  same.process(x.data(), 100, o);
  CHECK(o.size() == 100);
  same.set_step_scale(1.0 / (1 + 1000e-6));
  o.clear();
  same.process(x.data(), x.size(), o);
  CHECK(static_cast<double>(o.size()) == doctest::Approx(8008).epsilon(0.002));
}

TEST_CASE("Channel simulator: SNR definition, frequency shift, drift, clip, tilt") {
  double fs = 16000;
  auto x = tone(1000, fs, 32000, 0.5f);
  {
    ChannelParams p;
    p.snr_db = 10;
    ChannelSim sim(p, fs);
    auto y = sim.run(x, 1000, 1000);
    CHECK(y.size() == 34000);
    // Noise-only lead: power = Psig/10 * (fs/2)/3000.
    double np = mean_square(y, 0, 1000);
    CHECK(np == doctest::Approx(0.125 / 10 * 8000 / 3000).epsilon(0.15));
    CHECK(sim.signal_power() == doctest::Approx(0.125).epsilon(0.01));
  }
  for (double off : {-150.0, 37.0, 200.0}) {
    ChannelParams p;
    p.freq_offset_hz = off;
    ChannelSim sim(p, fs);
    auto y = sim.run(x);
    float want = tone_pow(y, 1000 + off, fs, 4000, 16000);
    float image = tone_pow(y, 1000 - off, fs, 4000, 16000);
    CHECK(want == doctest::Approx(0.125).epsilon(0.02));
    CHECK(pow_to_db(image / want) < -35);
  }
  {
    ChannelParams p;
    p.clock_ppm = 500;
    ChannelSim sim(p, fs);
    auto y = sim.run(x);
    CHECK(static_cast<double>(y.size()) == doctest::Approx(32000 * 1.0005).epsilon(0.0003));
    // The tone appears lower by 500 ppm when read at the nominal rate.
    CHECK(tone_pow(y, 1000 / 1.0005, fs, 4000, 24000) > tone_pow(y, 1000, fs, 4000, 24000) * 0.9);
  }
  {
    ChannelParams p;
    p.gain_db = 20;
    p.clip_level = 0.8;
    p.quantize_pcm16 = true;
    ChannelSim sim(p, fs);
    auto y = sim.run(x);
    float mx = 0;
    for (float v : y) mx = std::max(mx, std::fabs(v));
    CHECK(mx <= 0.8f + 1e-4f);
    CHECK(mx >= 0.79f);
  }
  {
    ChannelParams p;
    p.tilt = -1;
    ChannelSim sim(p, fs);
    auto a = sim.run(tone(1200, fs, 16000));
    ChannelSim sim2(p, fs);
    auto b = sim2.run(tone(2200, fs, 16000));
    CHECK(pow_to_db(tone_pow(b, 2200, fs, 4000) / tone_pow(a, 1200, fs, 4000)) < -4.5);
  }
  {
    ChannelParams p;
    p.bandlimit = true;
    p.fade_depth_db = 10;
    ChannelSim sim(p, fs);
    auto y = sim.run(tone(100, fs, 16000));
    CHECK(mean_square(y, 4000) < 0.125 * 0.05);
  }
}

TEST_CASE("NBFM demodulator recovers audio from rtl_tcp u8 IQ") {
  for (uint32_t iq_rate : {240000u, 1024000u, 2048000u}) {
    NbfmConfig c;
    c.iq_rate = iq_rate;
    c.audio_rate = 16000;
    c.offset_hz = iq_rate >= 1000000 ? 200000.0 : 50000.0;
    NbfmDemod d;
    REQUIRE(d.init(c));
    CHECK(d.if_rate() >= 24000);
    CHECK(d.if_rate() < 60000);
    // 1 kHz tone at 3 kHz deviation on a carrier at +offset, with a little noise.
    size_t n = iq_rate / 2;  // 0.5 s
    std::vector<uint8_t> iq(2 * n);
    Prng r(9);
    double ph = 0;
    for (size_t i = 0; i < n; ++i) {
      double t = static_cast<double>(i) / iq_rate;
      double f = c.offset_hz + 3000 * std::cos(kTwoPi * 1000 * t);
      ph += kTwoPi * f / iq_rate;
      double I = 127.5 + 90 * std::cos(ph) + 2 * r.gauss(), Q = 127.5 + 90 * std::sin(ph) + 2 * r.gauss();
      iq[2 * i] = static_cast<uint8_t>(std::lround(clampf(static_cast<float>(I), 0, 255)));
      iq[2 * i + 1] = static_cast<uint8_t>(std::lround(clampf(static_cast<float>(Q), 0, 255)));
    }
    std::vector<float> audio;
    // Odd-sized chunks exercise the pending-byte path.
    size_t pos = 0;
    while (pos < iq.size()) {
      size_t k = std::min<size_t>(4097, iq.size() - pos);
      d.process_u8(&iq[pos], k, audio);
      pos += k;
    }
    CHECK(static_cast<double>(audio.size()) == doctest::Approx(8000).epsilon(0.03));
    float p = tone_pow(audio, 1000, 16000, 2000, 4000);
    CHECK(std::sqrt(2 * p) == doctest::Approx(0.6).epsilon(0.05));  // 3k/5k deviation
    CHECK(mean_square(audio, 2000, 4000) < p * 1.1);                  // clean
  }
  // Garbage never crashes.
  NbfmConfig c;
  c.iq_rate = 1024000;
  NbfmDemod d;
  REQUIRE(d.init(c));
  Prng r(1);
  std::vector<uint8_t> junk(100001);
  for (auto& b : junk) b = static_cast<uint8_t>(r.u32());
  std::vector<float> a;
  d.process_u8(junk.data(), junk.size(), a);
  d.process_u8(nullptr, 10, a);
  for (float v : a) CHECK(std::isfinite(v));
  NbfmConfig bad;
  bad.iq_rate = 1000;
  CHECK_FALSE(d.init(bad));
}

}  // TEST_SUITE
