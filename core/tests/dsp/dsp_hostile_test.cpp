// Hostile-input robustness for dsp building blocks that take rates or sit on raw input paths
// (resampler, IIR/FIR filters, AGC, level meter, NBFM demod used by rtl_tcp): absurd parameters are
// refused or clamped, NaN/Inf never wedge a filter permanently, output stays bounded.
#include <cmath>
#include <limits>
#include <vector>

#include "doctest.h"
#include "dsp/agc.h"
#include "dsp/biquad.h"
#include "dsp/dsp.h"
#include "dsp/fir.h"
#include "dsp/level.h"
#include "dsp/nbfm.h"
#include "dsp/resampler.h"

using namespace ember::dsp;

namespace {
const float kNan = std::numeric_limits<float>::quiet_NaN();
const float kInf = std::numeric_limits<float>::infinity();
const double kDNan = std::numeric_limits<double>::quiet_NaN();
const double kDInf = std::numeric_limits<double>::infinity();

bool all_finite(const std::vector<float>& v) {
  for (float x : v)
    if (!std::isfinite(x)) return false;
  return true;
}
}  // namespace

TEST_SUITE("dsp") {

TEST_CASE("hostile: resampler refuses absurd rates and step scales") {
  Resampler rs;
  for (double in : {0.0, -1.0, 1.0, kDNan, kDInf, -kDInf, 1e12})
    for (double out : {0.0, 8000.0, kDNan, kDInf, 1e12}) {
      bool ok = rs.init(in, out);
      if (ok) CHECK(in / out <= 64);
      if (ok) CHECK(out / in <= 64);
    }
  CHECK_FALSE(rs.init(8000, 8000.0 * 65));
  CHECK_FALSE(rs.init(8000.0 * 65, 8000));
  CHECK(rs.init(48000, 8000, 1000, kDNan));  // zeros / cutoff clamped
  for (double s : {0.0, -1.0, kDNan, kDInf, 1e-300, 1e300}) rs.set_step_scale(s);
  std::vector<float> in(4800, 0.1f), out;
  rs.process(in.data(), in.size(), out);
  CHECK(out.size() <= in.size() / 6 + 2);
  CHECK(out.size() + rs.latency_in() / 6 + 2 >= in.size() / 6);
}

TEST_CASE("hostile: NaN/Inf do not wedge IIR filters, AGC, DC blocker or level meter") {
  Biquad bq = Biquad::lowpass(1000, 8000);
  Biquad bp = Biquad::bandpass(1200, 8000, 5);
  Agc agc;
  agc.init(8000);
  DcBlocker dc;
  dc.init(8000);
  LevelMeter lm;
  lm.init(8000);
  Fir fir(fir_lowpass(31, 1000, 8000));
  for (float bad : {kNan, kInf, -kInf}) {
    for (int i = 0; i < 10; ++i) {
      bq.process(bad);
      bp.process(bad);
      agc.process(bad);
      dc.process(bad);
      lm.push(bad);
      fir.process(bad);
    }
    // After a clean stretch everything is finite again.
    std::vector<float> o;
    for (int i = 0; i < 2000; ++i) {
      float x = 0.3f * static_cast<float>(std::sin(i * 0.5));
      o.push_back(bq.process(x));
      o.push_back(bp.process(x));
      o.push_back(agc.process(x));
      o.push_back(dc.process(x));
      o.push_back(fir.process(x));
      lm.push(x);
    }
    o.erase(o.begin(), o.begin() + 5 * 100);
    CHECK(all_finite(o));
    CHECK(std::isfinite(lm.rms()));
    CHECK(std::isfinite(agc.gain()));
  }
}

TEST_CASE("hostile: filter designs with absurd parameters stay finite") {
  for (double f : {0.0, -5.0, 4000.0, 1e9, kDNan, kDInf})
    for (double fs : {0.0, -1.0, 8000.0, kDNan, kDInf}) {
      Biquad b = Biquad::lowpass(f, fs, kDNan);
      float y = 0;
      for (int i = 0; i < 100; ++i) y = b.process(0.5f);
      (void)y;
      std::vector<float> t = fir_lowpass(31, f, fs);
      CHECK((t.size() == 31 || t.size() == 1));  // {1} = identity for unusable rates
      CHECK(all_finite(t));
      CHECK(kaiser_taps(60, f, fs, 4095) <= 4095);
      CHECK(kaiser_taps(60, f, fs, 4095) >= 3);
      Agc a;
      a.init(fs, f, f);
      CHECK(std::isfinite(a.process(0.5f)));
      DcBlocker d;
      d.init(fs, f);
      CHECK(std::isfinite(d.process(0.5f)));
    }
}

TEST_CASE("hostile: NBFM demod refuses absurd rates and survives NaN IQ") {
  NbfmDemod d;
  for (uint32_t iq : {0u, 1u, 15999u, 10000001u, 0xFFFFFFFFu}) {
    NbfmConfig c;
    c.iq_rate = iq;
    CHECK_FALSE(d.init(c));
  }
  for (uint32_t ar : {0u, 7999u, 384001u, 0xFFFFFFFFu}) {
    NbfmConfig c;
    c.audio_rate = ar;
    CHECK_FALSE(d.init(c));
  }
  NbfmConfig bad;
  bad.channel_bw_hz = kDNan;
  CHECK_FALSE(d.init(bad));
  bad = NbfmConfig();
  bad.deviation_hz = kDInf;
  CHECK_FALSE(d.init(bad));
  bad = NbfmConfig();
  bad.offset_hz = kDNan;
  bad.deemphasis_us = kDInf;
  bad.audio_lp_hz = kDNan;
  bad.if_min_hz = kDNan;
  if (d.init(bad)) {
    std::vector<float> a;
    std::vector<uint8_t> iq(20000, 128);
    d.process_u8(iq.data(), iq.size(), a);
    CHECK(all_finite(a));
  }

  NbfmConfig c;
  c.iq_rate = 240000;
  c.audio_rate = 48000;
  REQUIRE(d.init(c));
  std::vector<Cf> iq(24000);
  for (size_t i = 0; i < iq.size(); ++i) iq[i] = (i % 3) ? Cf(kNan, kInf) : Cf(-kInf, kNan);
  std::vector<float> audio;
  d.process_cf(iq.data(), iq.size(), audio);
  CHECK(std::isfinite(d.channel_power()));
  // Then a clean carrier: output must be finite again.
  for (size_t i = 0; i < iq.size(); ++i) iq[i] = Cf(static_cast<float>(std::cos(i * 0.1)), static_cast<float>(std::sin(i * 0.1)));
  audio.clear();
  d.process_cf(iq.data(), iq.size(), audio);
  d.process_cf(iq.data(), iq.size(), audio);
  REQUIRE(audio.size() > 2000);
  std::vector<float> tail(audio.end() - 2000, audio.end());
  CHECK(all_finite(tail));
  CHECK(std::isfinite(d.channel_power()));
  // Odd byte counts and empty input.
  std::vector<uint8_t> u8(1001, 200);
  d.process_u8(u8.data(), u8.size(), audio);
  d.process_u8(u8.data(), 0, audio);
  d.process_u8(nullptr, 0, audio);
  CHECK(all_finite(std::vector<float>(audio.end() - 100, audio.end())));
}

}  // TEST_SUITE
