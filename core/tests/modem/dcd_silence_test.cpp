// Regression: DCD (carrier detect) must drop for bit-exact digital silence, DC and a very quiet
// noise floor, for every modem at every supported sample rate — otherwise CSMA in the audio link
// never transmits (muted inputs, virtual audio cables, digital audio paths).
#include <cmath>
#include <string>
#include <vector>

#include "doctest.h"
#include "modem/modem.h"

using namespace ember;
using namespace ember::modem;

namespace {

struct Lcg {
  uint64_t s;
  float unit() {  // uniform [-1, 1)
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<float>(static_cast<int64_t>(s >> 11) - (1ll << 52)) / static_cast<float>(1ll << 52);
  }
};

std::vector<float> own_signal(const std::string& name, uint32_t rate) {
  std::string err;
  auto tx = create_modem(name, rate, ModemOptions(), &err);
  REQUIRE(tx);
  if (tx->info().frames) REQUIRE(tx->send_frame(ByteView(Bytes(12, 0x5A))));
  else REQUIRE(tx->send_text(name == "dtmf" ? "123#*0" : "CQ TEST DE QX"));
  std::vector<float> out, buf(rate / 50);
  for (int k = 0; k < 3000; ++k) {  // at most 60 s
    size_t n = tx->pull(buf.data(), buf.size());
    out.insert(out.end(), buf.begin(), buf.begin() + n);
    if (n < buf.size()) break;
  }
  return out;
}

// Feeds `seconds` of the given filler in 20 ms blocks; returns true if DCD is off at the end.
bool dcd_off_after(Modem& m, uint32_t rate, int kind, double seconds) {
  Lcg r{42};
  std::vector<float> b(rate / 50);
  for (int blk = 0; blk < static_cast<int>(seconds * 50); ++blk) {
    for (float& v : b) {
      if (kind == 0) v = 0.0f;                        // bit-exact silence
      else if (kind == 1) v = 0.25f;                  // DC offset
      else v = 0.000316f * r.unit() * 1.7320508f;     // ~ -70 dBFS white noise (RMS 0.000316)
    }
    m.rx(b.data(), b.size());
  }
  return !m.dcd();
}

void check_modem_rate(const std::string& name, uint32_t rate, double filler_s) {
  CAPTURE(name);
  CAPTURE(rate);
  std::string err;
  auto rx = create_modem(name, rate, ModemOptions(), &err);
  REQUIRE(rx);
  CHECK(dcd_off_after(*rx, rate, 0, 0.3));  // cold start: nothing but zeros
  std::vector<float> sig = own_signal(name, rate);
  for (int kind = 0; kind < 3; ++kind) {
    CAPTURE(kind);
    // A real transmission cut off mid-way (worst case for a latched detector), then filler.
    rx->rx(sig.data(), sig.size() / 2);
    CHECK(dcd_off_after(*rx, rate, kind, filler_s));
  }
  // And it still decodes afterwards.
  rx->rx(sig.data(), sig.size());
  std::vector<float> z(rate, 0.0f);
  rx->rx(z.data(), z.size());
  RxItem it;
  bool got = false;
  while (rx->pop(&it)) got = true;
  CHECK(got);
}

const uint32_t kRates[] = {8000, 11025, 16000, 22050, 44100, 48000, 96000, 192000};

}  // namespace

TEST_SUITE("modem") {

TEST_CASE("regression: DCD is off for digital silence, DC and a -70 dBFS noise floor (every modem)") {
  for (uint32_t rate : kRates) check_modem_rate("afsk1200", rate, 1.0);
  for (const std::string& name : modem_names())
    if (name != "afsk1200") check_modem_rate(name, 48000, 1.0);
}

TEST_CASE("long: DCD silence gate, every modem at every rate" * doctest::skip()) {
  for (const std::string& name : modem_names())
    for (uint32_t rate : kRates) check_modem_rate(name, rate, 1.5);
}

TEST_CASE("regression: AFSK DCD stays off on a -46 dBFS noise floor (random HDLC flags in noise)") {
  for (const char* name : {"afsk1200", "afsk300"}) {
    for (uint32_t rate : {8000u, 48000u}) {
      CAPTURE(std::string(name));
      CAPTURE(rate);
      std::string err;
      auto rx = create_modem(name, rate, ModemOptions(), &err);
      REQUIRE(rx);
      Lcg r{7};
      const float sigma = 0.005f * 1.7320508f;  // RMS 0.005 = -46 dBFS
      std::vector<float> b(rate / 100);
      size_t on = 0, total = 0;
      for (int blk = 0; blk < 1000; ++blk) {  // 10 s, sampled every 10 ms
        for (float& v : b) v = sigma * r.unit();
        rx->rx(b.data(), b.size());
        if (blk >= 50) { ++total; on += rx->dcd(); }
      }
      // Was ON most of the time (flags in noise); now a rare blip. HF afsk300 runs 7 slicers.
      CHECK(static_cast<double>(on) / static_cast<double>(total) < (std::string(name) == "afsk300" ? 0.05 : 0.03));
      // And a real transmission is still detected while it plays.
      std::vector<float> sig = own_signal(name, rate);
      size_t seen = 0, blocks = 0;
      for (size_t off = 0; off + b.size() <= sig.size(); off += b.size()) {
        std::vector<float> x(sig.begin() + static_cast<long>(off), sig.begin() + static_cast<long>(off + b.size()));
        for (size_t i = 0; i < x.size(); ++i) x[i] = 0.5f * x[i] + sigma * r.unit();
        rx->rx(x.data(), x.size());
        ++blocks;
        seen += rx->dcd();
      }
      CHECK(static_cast<double>(seen) / static_cast<double>(blocks) > 0.6);
    }
  }
}

}  // TEST_SUITE
