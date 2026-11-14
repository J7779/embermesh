// Regression tests for the security review findings that live in the link layer (H5, M5, M6, M7,
// L6, L9). Each case reproduces the reported attack with deterministic input. CPU-blowup cases
// assert a time bound that is generous (sanitizer Debug builds are ~20x slower than -O2) but far
// below what the unfixed code needed.
#include "timing_slack.h"
#include <chrono>
#include <string>

#include "doctest.h"
#include "ember/crc.h"
#include "fec/packet.h"
#include "framing/ax25.h"
#include "framing/kiss.h"
#include "framing/stream.h"
#include "link/link_util.h"
#include "link_harness.h"

using namespace ember;
using namespace linktest;

namespace {

const uint8_t kSync[4] = {0x7E, 0x1B, 0xA5, 0xD2};

Bytes kiss_aprs(const std::string& src, const std::string& info, const std::string& dst = "APRS") {
  ax25::Address d = ax25::make_address(dst), sa = ax25::make_address(src);
  ax25::Frame f = ax25::make_ui(d, sa, {}, ByteView(std::string_view(info)));
  Bytes a;
  ax25::encode(f, &a);
  return kiss::encode_data(ByteView(a));
}

// No C0/C1 controls, no DEL, no bidi overrides/isolates, no line/paragraph separators.
bool clean_text(const std::string& s) {
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x20 || c == 0x7F) return false;
    if (c == 0xC2 && i + 1 < s.size() && static_cast<unsigned char>(s[i + 1]) < 0xA0) return false;  // U+0080..9F
    if (c == 0xE2 && i + 2 < s.size()) {
      unsigned b1 = static_cast<unsigned char>(s[i + 1]), b2 = static_cast<unsigned char>(s[i + 2]);
      if (b1 == 0x80 && (b2 == 0xA8 || b2 == 0xA9 || (b2 >= 0xAA && b2 <= 0xAE))) return false;  // 2028-202E
      if (b1 == 0x81 && b2 >= 0xA6 && b2 <= 0xA9) return false;                                  // 2066-2069
    }
  }
  return true;
}

double seconds_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// FEC sync + the 10-byte RS-protected header of an envelope announcing `len` bytes.
Bytes fec_header(size_t len, uint8_t nroots) {
  Bytes payload(len, 0x55);
  fec::ProtectParams p;
  p.nroots = nroots;
  Bytes env = fec::protect(ByteView(payload), p);
  Bytes out(kSync, kSync + 4);
  out.insert(out.end(), env.begin(), env.begin() + 10);
  return out;
}

// Hostile byte streams (security review H5/M5), `total` bytes each.
Bytes hostile_stream(const std::string& mode, size_t total) {
  Prng rng(42);
  Bytes data;
  data.reserve(total + 20000);
  if (mode == "random") {
    while (data.size() < total) data.push_back(rng.byte());
  } else if (mode == "plain_dense") {  // framing/stream headers with valid hcrc, len 8192, every 6 bytes
    uint8_t hd[6] = {0xE5, 0x4D, 0x00, 0x20, 0x00, 0};
    hd[5] = crc8(ByteView(hd + 2, 3));
    while (data.size() < total) data.insert(data.end(), hd, hd + 6);
  } else if (mode == "plain_mtu") {  // same, but with a length just below the cable mtu
    uint8_t hd[6] = {0xE5, 0x4D, 0x00, 0x07, 0xF0, 0};
    hd[5] = crc8(ByteView(hd + 2, 3));
    while (data.size() < total) data.insert(data.end(), hd, hd + 6);
  } else if (mode == "fec_dense") {  // valid FEC sync+header every 14 bytes
    Bytes h = fec_header(2000, 2);
    while (data.size() < total) data.insert(data.end(), h.begin(), h.end());
  } else if (mode == "fec_big") {  // header announcing an envelope larger than the receive buffer
    Bytes h = fec_header(8192, 128);
    while (data.size() < total) {
      data.insert(data.end(), h.begin(), h.end());
      for (int i = 0; i < 16500; ++i) data.push_back(0x00);
    }
  } else if (mode == "fec_bad") {  // valid header, random body (RS fails), repeated
    Bytes h = fec_header(2000, 2);
    size_t tot = 0;
    fec::peek_header(ByteView(h).sub(4), &tot, 8192);
    while (data.size() < total) {
      data.insert(data.end(), h.begin(), h.end());
      for (size_t i = 0; i + 10 < tot; ++i) data.push_back(rng.byte() & 0x7F);
    }
  }
  data.resize(total);
  return data;
}

double feed_time(const std::string& cfg, const Bytes& data, FakeHost* out_host = nullptr) {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto l = create_link("stream", cfg, h, &err);
  REQUIRE(l);
  l->on_io_state(true);
  auto t0 = std::chrono::steady_clock::now();
  for (size_t off = 0; off < data.size(); off += 4096) l->on_io_bytes(ByteView(data).sub(off, 4096));
  double s = seconds_since(t0);
  // After the attack a genuine frame must still get through.
  Bytes good(100, 0x42);
  Bytes wire;
  if (cfg.find("ebyte") != std::string::npos) {
    fec::ProtectParams pp;
    Bytes env = fec::protect(ByteView(good), pp);
    wire.assign(kSync, kSync + 4);
    wire.insert(wire.end(), env.begin(), env.end());
  } else {
    wire = stream::encode(ByteView(good));
  }
  // Enough trailing filler to flush any pending (hostile) candidate, then the good frame.
  Bytes flush(20000, 0x00);
  l->on_io_bytes(ByteView(flush));
  h.frames.clear();
  l->on_io_bytes(ByteView(wire));
  CHECK(h.frames.size() == 1);
  if (out_host) *out_host = h;
  return s;
}

}  // namespace

TEST_SUITE("link") {

TEST_CASE("H5 regression: hostile stream input costs linear CPU (plain and FEC receivers)") {
  const size_t kBytes = 512 * 1024;
  double base_plain = feed_time("{}", hostile_stream("random", kBytes));
  double base_fec = feed_time(R"({"preset":"ebyte"})", hostile_stream("random", kBytes));
  for (std::string mode : {"plain_dense", "plain_mtu", "fec_dense", "fec_big", "fec_bad"}) {
    CAPTURE(mode);
    Bytes d = hostile_stream(mode, kBytes);
    double tp = feed_time("{}", d);
    double tf = feed_time(R"({"preset":"ebyte"})", d);
    MESSAGE(mode << ": cable " << tp << " s (random " << base_plain << " s), ebyte " << tf << " s (random "
                  << base_fec << " s)");
    // Unfixed: plain_dense ~ 4 s per 512 KB at -O2 (minutes under ASan); fec_dense/fec_bad similar.
    CHECK(tp < 3.0);
    CHECK(tf < 3.0);
    CHECK(tp < 20 * base_plain + 0.5);
    CHECK(tf < 20 * base_fec + 0.5);
  }
}

TEST_CASE("H5: stream receiver caps frame length at the link mtu") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto l = create_link("stream", R"({"preset":"custom","mtu":64})", h, &err);
  REQUIRE(l);
  l->on_io_state(true);
  Bytes ok = stream::encode(ByteView(Bytes(64, 1)));
  Bytes big = stream::encode(ByteView(Bytes(65, 2)));
  l->on_io_bytes(ByteView(big));
  l->on_io_bytes(ByteView(ok));
  REQUIRE(h.frames.size() == 1);
  CHECK(h.frames[0].data == Bytes(64, 1));
}

TEST_CASE("H5: the FEC receiver only runs when FEC is enabled") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  fec::ProtectParams pp;
  Bytes env = fec::protect(ByteView(Bytes(50, 7)), pp);
  Bytes wire(kSync, kSync + 4);
  wire.insert(wire.end(), env.begin(), env.end());
  auto plain = create_link("stream", R"({"preset":"cable"})", h, &err);
  REQUIRE(plain);
  plain->on_io_state(true);
  plain->on_io_bytes(ByteView(wire));
  CHECK(h.frames.empty());
  auto withfec = create_link("stream", R"({"preset":"cable","fec":"yes"})", h, &err);
  REQUIRE(withfec);
  withfec->on_io_state(true);
  withfec->on_io_bytes(ByteView(wire));
  CHECK(h.frames.size() == 1);
  // A FEC link still accepts plain frames.
  withfec->on_io_bytes(ByteView(stream::encode(ByteView(Bytes(10, 3)))));
  CHECK(h.frames.size() == 2);
}

TEST_CASE("H5: framing/stream decoder work budget (direct)") {
  stream::Decoder dec(8192);
  Bytes d = hostile_stream("plain_dense", 256 * 1024);
  auto t0 = std::chrono::steady_clock::now();
  size_t frames = 0;
  for (size_t off = 0; off < d.size(); off += 1000) dec.feed(ByteView(d).sub(off, 1000), [&](uint8_t, ByteView) { ++frames; });
  CHECK(seconds_since(t0) < 2.0 * timing_slack());
  CHECK(frames == 0);
  CHECK(dec.stats().budget_skipped > 0);
  // Genuine frames, back to back after the attack, all decode.
  Bytes good;
  for (int i = 0; i < 50; ++i) stream::encode_append(ByteView(Bytes(3000, static_cast<uint8_t>(i))), good);
  Bytes flush(9000, 0);
  dec.feed(ByteView(flush), [&](uint8_t, ByteView) { ++frames; });
  dec.feed(ByteView(good), [&](uint8_t, ByteView) { ++frames; });
  CHECK(frames == 50);
}

TEST_CASE("M7 regression: APRS message text, addressee and source are sanitised and bounded") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto l = create_link("kiss", R"({"callsign":"QX0ABC","mtu":2048})", h, &err);
  REQUIRE(l);
  l->on_io_state(true);
  std::string text = std::string("hi\x1b[2J\x1b]0;pwned\x07\r\nFAKE> injected line\x00tail", 40);
  text += "\xC2\x9B" "31m \xE2\x80\xAE" "evil \xE2\x80\xA8" "x";
  text.append(1500, 'A');
  l->on_io_bytes(ByteView(kiss_aprs("N0EVIL", ":QX0ABC   :" + text + "{12")));
  l->on_io_bytes(ByteView(kiss_aprs("N0EVIL", std::string(":W1\x07" "B\x1b    :hello{13"))));
  l->on_io_bytes(ByteView(kiss_aprs("N0EVIL", ">status \x1b[31m\r\nline2" + std::string(1000, 'S'))));
  l->on_io_bytes(ByteView(kiss_aprs("N0EVIL", "!4903.50N/07201.75W-Test\x07\x1b" + std::string(1000, 'C'))));
  REQUIRE(h.texts.size() >= 3);
  for (const auto& t : h.texts) {
    CAPTURE(t.text);
    CHECK(clean_text(t.text));
    CHECK(clean_text(t.from));
    CHECK(clean_text(t.to));
    CHECK(t.to.size() <= 9);
    CHECK(t.from.size() <= 9);
    CHECK(t.text.size() <= 256);
  }
  CHECK(h.texts[0].text.size() <= 67);
  CHECK(h.texts[0].text.find("hi") == 0);
  CHECK(h.texts[0].text.find('\n') == std::string::npos);
}

TEST_CASE("M7: every interop path goes through the shared sanitiser (printable_text)") {
  std::string bad = std::string("a\x00\x01\x1b\r\n\t\x7f", 9) + "\xC2\x85\xC2\x9B\xE2\x80\xAE\xE2\x81\xA6\xE2\x80\xA9z";
  std::string out = link::printable_text(ByteView(std::string_view(bad)), 512);
  CHECK(clean_text(out));
  CHECK(out.front() == 'a');
  CHECK(out.back() == 'z');
  // Bounded, and truncation never splits a code point.
  std::string longs;
  for (int i = 0; i < 300; ++i) longs += "\xC3\xA9";
  std::string t = link::printable_text(ByteView(std::string_view(longs)), 67);
  CHECK(t.size() <= 67);
  CHECK(t.size() % 2 == 0);
}

TEST_CASE("M6 regression: parse_int rejects values that overflow int64 (no UB)") {
  int64_t v = 0;
  CHECK(!link::parse_int("9999999999999999999", &v));
  CHECK(!link::parse_int("-9999999999999999999", &v));
  CHECK(!link::parse_int("9223372036854775808", &v));
  CHECK(link::parse_int("9223372036854775807", &v));
  CHECK(v == INT64_MAX);
  CHECK(link::parse_int("-9223372036854775808", &v));
  CHECK(v == INT64_MIN);
  CHECK(link::parse_int("-0", &v));
  CHECK(v == 0);
  CHECK(!link::parse_int("-", &v));
  CHECK(!link::parse_int("+5", &v));
  CHECK(!link::parse_int("12a", &v));
  CHECK(!link::parse_int("", &v));
  CHECK(!link::parse_int(std::string(30, '1'), &v));
  CHECK(link::parse_int("0000000000000000001", &v));
  CHECK(v == 1);
  // The hostile AT replies, through the driver (UBSan would abort here before the fix).
  for (const char* reply : {"+ERR=9999999999999999999\r\n", "+OK\r\n+VER=RYLR998\r\n+ADDRESS=9999999999999999999\r\n"}) {
    Clock clk;
    FakeHost h(&clk);
    std::string err;
    auto l = create_link("lora_at", "{}", h, &err);
    REQUIRE(l);
    l->on_io_state(true);
    l->on_io_bytes(ByteView(std::string_view(reply)));
    Json st = parse_json(l->status_json());
    CHECK(st.is_object());
  }
}

}  // TEST_SUITE
