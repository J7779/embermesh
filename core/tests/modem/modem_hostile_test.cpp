// Hostile-audio robustness for every modem in modem_catalog() (security review, "not covered":
// modem/audio decoders). Checks: absurd sample rates are refused at creation, any audio (full-scale
// noise, DC, square waves, NaN/Inf, denormals, zero-length and large buffers) never crashes, never
// grows memory/output without bound, and never wedges the decoder: a clean signal sent right after
// the junk still decodes on the same receiver instance.
//
// Long version: ./ember_tests -ts=modem -tc="hostile long*" --no-skip
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "doctest.h"
#include "modem/test_util.h"

using namespace mt;
using ember::modem::create_modem;
using ember::modem::modem_catalog;
using ember::modem::ModemInfo;

namespace {

// Fills x with one of several hostile waveforms.
void hostile(std::vector<float>& x, int kind, Prng& r, uint32_t rate) {
  const float kNan = std::numeric_limits<float>::quiet_NaN();
  const float kInf = std::numeric_limits<float>::infinity();
  const float kDen = std::numeric_limits<float>::denorm_min();
  for (size_t i = 0; i < x.size(); ++i) {
    float v = 0;
    switch (kind) {
      case 0: v = static_cast<float>(r.uniform() * 2 - 1); break;                    // full-scale noise
      case 1: v = 1.0f; break;                                                        // DC at full scale
      case 2: v = ((i / (1 + rate / 2400)) & 1) ? 1.0f : -1.0f; break;               // square ~1200 Hz
      case 3: v = (i % 7 == 0) ? kNan : (i % 7 == 1 ? kInf : (i % 7 == 2 ? -kInf : 0.3f)); break;
      case 4: v = kNan; break;                                                        // all NaN
      case 5: v = (i & 1) ? kDen : -kDen; break;                                      // denormals
      case 6: v = (i & 1) ? 1e30f : -1e30f; break;                                    // huge finite
      case 7: v = static_cast<float>(r.uniform() * 2 - 1) * ((i / 50) % 2 ? 1.0f : 1e-30f); break;  // bursts
      default: v = (r.below(1000) == 0) ? 4.0f : 0.0f; break;                        // impulses
    }
    x[i] = v;
  }
}

ember::modem::ModemOptions opts_for(const ModemInfo& mi) {
  ember::modem::ModemOptions o;
  o.txdelay_ms = 200;
  (void)mi;
  return o;
}

// Clean transmission of a known frame / text, at the given rate.
std::vector<float> clean_signal(const std::string& name, uint32_t rate, const ModemInfo& mi, Bytes* frame,
                                std::string* text) {
  auto tx = make(name, rate, opts_for(mi));
  if (mi.frames) {
    *frame = Bytes{'h', 'e', 'l', 'l', 'o', ' ', 'm', 'e', 's', 'h', 1, 2, 3, 0xC0, 0x7E};
    REQUIRE(tx->send_frame(ByteView(*frame)));
  } else {
    *text = name == "dtmf" ? "123A" : "TEST TEST";
    REQUIRE(tx->send_text(*text));
  }
  return drain_tx(*tx);
}

struct Outcome {
  size_t items = 0, max_frame = 0, text_bytes = 0;
};

Outcome drain(Modem& m) {
  Outcome o;
  RxItem it;
  while (m.pop(&it)) {
    ++o.items;
    if (it.data.size() > o.max_frame) o.max_frame = it.data.size();
    o.text_bytes += it.text.size();
  }
  return o;
}

// One modem at one rate: hostile phase, bounds, then recovery.
void run_one(const ModemInfo& mi, uint32_t rate, double seconds, uint64_t seed, int only_kind = -1) {
  INFO("modem " << mi.name << " rate " << rate);
  Prng r(seed);
  auto rx = make(mi.name, rate, opts_for(mi));
  const size_t qmax = rx->options().rx_queue_max;
  std::vector<float> x;
  size_t total_samples = 0;
  for (int kind = 0; kind < 9; ++kind) {
    if (only_kind >= 0 && kind != only_kind) continue;
    size_t n = static_cast<size_t>(seconds * rate);
    x.resize(n);
    hostile(x, kind, r, rate);
    // Random chunking incl. zero-length chunks and nullptr.
    size_t pos = 0;
    while (pos < n) {
      size_t k = r.below(8) == 0 ? 0 : 1 + r.below(4000);
      if (k > n - pos) k = n - pos;
      rx->rx(k ? &x[pos] : nullptr, k);
      pos += k;
      CHECK(rx->rx_pending() <= qmax);
    }
    total_samples += n;
    Outcome o = drain(*rx);
    CHECK(o.max_frame <= mi.max_frame);
    // Text modes: at most a few hundred characters per second of junk (CW / RTTY / PSK31 are slow).
    CHECK(o.text_bytes <= 400 * (seconds + 1));
  }
  std::vector<int16_t> pcm(only_kind >= 0 ? 0 : rate / 2);
  for (auto& v : pcm) v = static_cast<int16_t>(r.u32());
  if (!pcm.empty()) rx->rx_pcm16(pcm.data(), pcm.size());
  rx->rx_pcm16(nullptr, 5);
  drain(*rx);
  CHECK(rx->stats().rx_samples == total_samples + pcm.size());

  // Recovery: silence, then a clean signal into the same receiver.
  Bytes frame;
  std::string text;
  std::vector<float> clean = clean_signal(mi.name, rate, mi, &frame, &text);
  std::vector<float> sil(rate, 0.0f);
  rx->rx(sil.data(), sil.size());
  drain(*rx);
  std::vector<float> sig = sil;
  sig.insert(sig.end(), clean.begin(), clean.end());
  sig.insert(sig.end(), rate, 0.0f);
  auto items = feed(*rx, sig, seed + 1);
  bool got = false;
  std::string got_text;
  for (auto& it : items) {
    if (mi.frames && it.kind == RxKind::Frame && it.data == frame) got = true;
    got_text += it.text;
  }
  if (!mi.frames) got = got_text.find(text.substr(0, 3)) != std::string::npos;
  INFO("text received after junk: '" << got_text << "'");
  CHECK_MESSAGE(got, "decoder wedged by hostile audio");
}

}  // namespace

TEST_SUITE("modem") {

TEST_CASE("hostile: absurd sample rates are refused at creation") {
  for (const ModemInfo& mi : modem_catalog()) {
    for (uint32_t rate : {0u, 1u, 7999u, 192001u, 384000u, 384001u, 1000000u, 0x7FFFFFFFu, 0xFFFFFFFFu}) {
      std::string err;
      auto m = create_modem(mi.name, rate, ember::modem::ModemOptions(), &err);
      INFO(mi.name << " @ " << rate);
      CHECK(m == nullptr);
      CHECK(!err.empty());
    }
    for (uint32_t rate : {8000u, 192000u}) {
      std::string err;
      CHECK(create_modem(mi.name, rate, ember::modem::ModemOptions(), &err) != nullptr);
    }
  }
}

TEST_CASE("hostile: every modem survives junk audio and still decodes afterwards") {
  uint64_t seed = 10;
  for (const ModemInfo& mi : modem_catalog()) {
    // Alternate rates so changing device rates (new instance per rate, as the audio link does)
    // are covered, incl. the 8 kHz and 192 kHz extremes.
    uint32_t rate = (seed % 3 == 0) ? 8000 : (seed % 3 == 1) ? 48000 : 192000;
    run_one(mi, rate, 0.15, seed++);
  }
}

TEST_CASE("hostile: one large buffer in a single rx() call") {
  // 2 s of full-scale noise at 192 kHz handed over in one call (the modem must chunk internally).
  std::vector<float> x(2 * 192000);
  Prng r(77);
  for (auto& v : x) v = static_cast<float>(r.uniform() * 2 - 1);
  for (const char* name : {"afsk1200", "robust62", "psk31"}) {
    auto m = make(name, 192000);
    m->rx(x.data(), x.size());
    CHECK(m->rx_pending() <= m->options().rx_queue_max);
    drain(*m);
  }
}

// Found by the long run: PSK31's signal-gone reference only updated while the squelch was open,
// so 1.5 s of a strong steady carrier on the centre frequency (or simply a strong station followed
// by a weaker one) kept the squelch closed forever.
TEST_CASE("regression: psk31 reopens its squelch after a strong steady carrier") {
  for (const ModemInfo& mi : modem_catalog())
    if (mi.name == "psk31") run_one(mi, 8000, 1.5, 1000, /*square wave*/ 2);
}

// Found by the long run: CW's adaptive timing learned from 1.5 s of impulse noise and decoded the
// next transmission as dahs; timing is now forgotten after >= 1 s (and >= 15 dots) of key-up.
TEST_CASE("regression: cw relearns its speed after impulse noise") {
  for (const ModemInfo& mi : modem_catalog())
    if (mi.name == "cw") run_one(mi, 48000, 1.5, 7, /*impulses*/ 8);
}

TEST_CASE("hostile long: all modems, all rates, longer junk" * doctest::skip()) {
  uint64_t seed = 1000;
  for (const ModemInfo& mi : modem_catalog())
    for (uint32_t rate : {8000u, 11025u, 22050u, 44100u, 48000u, 96000u, 192000u}) run_one(mi, rate, 1.5, seed++);
  // 10 M samples in one call.
  std::vector<float> x(10000000);
  Prng r(5);
  for (auto& v : x) v = static_cast<float>(r.gauss()) * 0.5f;
  for (const ModemInfo& mi : modem_catalog()) {
    auto m = make(mi.name, 48000);
    m->rx(x.data(), x.size());
    CHECK(m->rx_pending() <= m->options().rx_queue_max);
  }
}

}  // TEST_SUITE
