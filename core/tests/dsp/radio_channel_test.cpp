// dsp/radio_channel.h: the handheld radio path behaves like the radio it models.
#include <cmath>
#include <vector>

#include "doctest.h"
#include "dsp/radio_channel.h"

using namespace ember::dsp;

namespace {
constexpr double kFs = 16000;

std::vector<float> tone(double hz, double s, double amp) {
  std::vector<float> v(static_cast<size_t>(s * kFs));
  for (size_t i = 0; i < v.size(); ++i) v[i] = static_cast<float>(amp * std::sin(2 * 3.14159265358979 * hz * i / kFs));
  return v;
}
double rms(const std::vector<float>& v, size_t a, size_t b) {
  double s = 0;
  for (size_t i = a; i < b && i < v.size(); ++i) s += double(v[i]) * v[i];
  return b > a ? std::sqrt(s / double(b - a)) : 0;
}
// First sample index whose 10 ms RMS exceeds `thr`.
size_t first_loud(const std::vector<float>& v, double thr) {
  for (size_t i = 0; i + 160 < v.size(); i += 16)
    if (rms(v, i, i + 160) > thr) return i;
  return v.size();
}
}  // namespace

TEST_SUITE("dsp") {

TEST_CASE("radio path: VOX attack clips the start, hang keeps it keyed, squelch tail follows") {
  RadioPathParams p;
  p.keying = Keying::Vox;
  p.vox_attack_ms = 400;
  p.vox_hang_ms = 600;
  p.squelch_open_ms = 150;
  p.squelch_tail_ms = 200;
  p.fm_snr_db = 40;
  std::vector<float> tx = tone(1200, 2.0, 0.3);
  tx.resize(tx.size() + static_cast<size_t>(0.3 * kFs), 0.0f);  // 300 ms gap < hang
  std::vector<float> t2 = tone(1200, 1.0, 0.3);
  tx.insert(tx.end(), t2.begin(), t2.end());
  RadioPath path(p, kFs);
  std::vector<float> rx = path.run(tx, {}, static_cast<size_t>(2 * kFs));
  CHECK(path.stats().key_ups == 1);  // the 300 ms gap is bridged by the hang time
  CHECK(path.stats().first_key_ms == doctest::Approx(400).epsilon(0.05));
  size_t heard = first_loud(rx, 0.05);
  CHECK(heard / kFs * 1000 == doctest::Approx(550).epsilon(0.08));  // attack + squelch opening
  // After the audio ends: hang, then carrier off, then a tail noise burst, then silence.
  size_t end = tx.size() + static_cast<size_t>(0.6 * kFs);
  CHECK(rms(rx, end + 160, end + 160 + 1600) > 0.02);  // tail burst (hiss well above the silence after it)
  CHECK(rms(rx, end + static_cast<size_t>(0.5 * kFs), rx.size()) < 1e-6);                  // squelch shut
}

TEST_CASE("radio path: PTT and manual keying delays; voice passband; acoustic coupling") {
  RadioPathParams p;
  p.keying = Keying::Ptt;
  p.ptt_delay_ms = 150;
  p.squelch_open_ms = 250;
  p.fm_snr_db = 40;
  std::vector<float> tx = tone(1000, 1.0, 0.3);
  std::vector<uint8_t> ptt(tx.size(), 1);
  RadioPath path(p, kFs);
  std::vector<float> rx = path.run(tx, ptt, static_cast<size_t>(kFs));
  CHECK(path.stats().first_key_ms == doctest::Approx(150).epsilon(0.05));
  CHECK(first_loud(rx, 0.05) / kFs * 1000 == doctest::Approx(400).epsilon(0.08));
  // 100 Hz and 6 kHz are outside the voice passband; 1 kHz passes.
  for (double hz : {100.0, 6000.0}) {
    RadioPathParams q = p;
    q.rx_agc = false;
    RadioPath a(q, kFs), b(q, kFs);
    std::vector<float> in = tone(hz, 1.0, 0.3), ref = tone(1000, 1.0, 0.3);
    std::vector<float> o = a.run(in, ptt, 0), r = b.run(ref, ptt, 0);
    CHECK(rms(o, 8000, 16000) < 0.3 * rms(r, 8000, 16000));
  }
  // Manual PTT: the operator's reaction time delays keying.
  RadioPathParams m = p;
  m.keying = Keying::Manual;
  m.ptt_delay_ms = 700;
  RadioPath pm(m, kFs);
  pm.run(tx, ptt, 0);
  CHECK(pm.stats().first_key_ms == doctest::Approx(700).epsilon(0.05));
  // Acoustic coupling adds room noise while nothing is sent and smears the signal (reverb).
  RadioPathParams ac = p;
  ac.acoustic = true;
  ac.room_noise = 0.02;
  RadioPath pa(ac, kFs);
  std::vector<float> o = pa.run(tx, ptt, static_cast<size_t>(kFs));
  CHECK(rms(o, o.size() - 4000, o.size()) > 0.005);  // background noise reaches the device mic
}

}  // TEST_SUITE
