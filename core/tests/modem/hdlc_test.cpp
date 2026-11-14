// Bit-level HDLC: exact bit streams (flags, stuffing, FCS order, NRZI) and decoder robustness.
#include <string>
#include <vector>

#include "doctest.h"
#include "dsp/dsp.h"
#include "ember/crc.h"
#include "modem/hdlc.h"

using namespace ember;
using namespace ember::modem;

namespace {

std::string bits_str(const std::vector<uint8_t>& b) {
  std::string s;
  for (uint8_t v : b) s.push_back(v ? '1' : '0');
  return s;
}

// Independent reference: written from the AX.25 2.2 spec text, not sharing code with hdlc.cpp.
std::string reference_hdlc(const Bytes& frame, int pre, int post) {
  const std::string flag = "01111110";
  std::string out;
  for (int i = 0; i < pre; ++i) out += flag;
  Bytes all = frame;
  uint16_t fcs = crc16_x25_of(ByteView(frame));
  all.push_back(static_cast<uint8_t>(fcs & 0xFF));
  all.push_back(static_cast<uint8_t>(fcs >> 8));
  int run = 0;
  for (uint8_t byte : all) {
    for (int i = 0; i < 8; ++i) {
      char c = ((byte >> i) & 1) ? '1' : '0';
      out.push_back(c);
      run = c == '1' ? run + 1 : 0;
      if (run == 5) { out.push_back('0'); run = 0; }
    }
  }
  for (int i = 0; i < post; ++i) out += flag;
  return out;
}

}  // namespace

TEST_SUITE("modem") {

TEST_CASE("CRC-16/X-25 check value and HDLC good-FCS residue") {
  const char* s = "123456789";
  CHECK(crc16_x25_of(ByteView(std::string_view(s))) == 0x906E);
  Bytes f(s, s + 9);
  uint16_t fcs = crc16_x25_of(ByteView(f));
  f.push_back(static_cast<uint8_t>(fcs & 0xFF));
  f.push_back(static_cast<uint8_t>(fcs >> 8));
  CHECK(crc16_x25(ByteView(f)) == 0xF0B8);  // RFC 1662 PPPGOODFCS16
}

TEST_CASE("HDLC hand-checked bit stream with stuffing") {
  // Frame {0xFF}: FCS = 0xFF00 -> sent 0x00 then 0xFF. Both 0xFF bytes need one stuffed 0.
  Bytes f = {0xFF};
  CHECK(crc16_x25_of(ByteView(f)) == 0xFF00);
  auto bits = hdlc_encode({f}, 1, 1);
  CHECK(bits_str(bits) == "01111110" "111110111" "00000000" "111110111" "01111110");
  // NRZI: 0 toggles, 1 holds. Starting level 1 (mark).
  std::vector<uint8_t> b = {0, 1, 1, 0, 0, 1};
  uint8_t lvl = 1;
  nrzi_encode(b, &lvl);
  CHECK(bits_str(b) == "000100");
  CHECK(lvl == 0);
}

TEST_CASE("HDLC AX.25 UI frame bit stream matches the independent reference") {
  // APRS position-less status frame: N0CALL-7 > APRS, UI, PID F0, ">EmberMesh test"
  Bytes f;
  for (char c : std::string("APRS  ")) f.push_back(static_cast<uint8_t>(c << 1));
  f.push_back(0x60);
  for (char c : std::string("N0CALL")) f.push_back(static_cast<uint8_t>(c << 1));
  f.push_back(static_cast<uint8_t>(0x60 | (7 << 1) | 1));
  f.push_back(0x03);
  f.push_back(0xF0);
  for (char c : std::string(">EmberMesh test")) f.push_back(static_cast<uint8_t>(c));
  auto bits = hdlc_encode({f}, 3, 2);
  CHECK(bits_str(bits) == reference_hdlc(f, 3, 2));
  // And it decodes.
  HdlcDecoder d;
  d.init(15, 330);
  int frames = 0;
  for (uint8_t b : bits) {
    if (d.push(b)) { ++frames; CHECK(d.frame() == ByteView(f)); }
  }
  CHECK(frames == 1);
}

TEST_CASE("HDLC decoder: every length 1..max, back to back, errors, aborts, overlong") {
  dsp::Prng r(42);
  const size_t kMax = 1024;
  HdlcDecoder d;
  d.init(1, kMax);
  std::vector<Bytes> frames;
  for (size_t len = 1; len <= kMax; ++len) {
    Bytes f(len);
    for (auto& v : f) v = static_cast<uint8_t>(r.u32());
    if (len % 7 == 0) for (auto& v : f) v = 0xFF;  // worst-case stuffing
    if (len % 11 == 0) for (auto& v : f) v = 0x7E;  // flag-looking data
    frames.push_back(f);
  }
  auto bits = hdlc_encode(frames, 2, 2);
  size_t got = 0;
  for (uint8_t b : bits) {
    if (d.push(b)) {
      REQUIRE(got < frames.size());
      CHECK(d.frame() == ByteView(frames[got]));
      ++got;
    }
  }
  CHECK(got == frames.size());

  // max + 1 is rejected.
  Bytes big(kMax + 1, 0x55);
  int n = 0;
  for (uint8_t b : hdlc_encode({big}, 1, 1)) n += d.push(b);
  CHECK(n == 0);

  // A single flipped bit anywhere in a frame is always caught.
  Bytes f = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  auto good = hdlc_encode({f}, 1, 1);
  int false_frames = 0;
  for (size_t i = 8; i + 8 < good.size(); ++i) {
    auto bad = good;
    bad[i] ^= 1;
    d.reset();
    for (uint8_t b : bad) false_frames += d.push(b);
  }
  CHECK(false_frames == 0);
  CHECK(d.fcs_errors() > 0);

  // Abort in the middle (7 ones) kills the frame; the next frame is fine.
  std::vector<uint8_t> ab;
  hdlc_append_flags(ab, 1);
  hdlc_append_frame(ab, ByteView(f));
  ab.resize(ab.size() - 20);
  for (int i = 0; i < 8; ++i) ab.push_back(1);
  auto tail = hdlc_encode({f}, 1, 1);
  ab.insert(ab.end(), tail.begin(), tail.end());
  d.reset();
  n = 0;
  for (uint8_t b : ab) n += d.push(b);
  CHECK(n == 1);

  // min_len is honoured; random bits produce nothing (with overwhelming probability).
  HdlcDecoder d2;
  d2.init(15, 330);
  n = 0;
  for (uint8_t b : hdlc_encode({Bytes{1, 2, 3}}, 1, 1)) n += d2.push(b);
  CHECK(n == 0);
  for (int i = 0; i < 2000000; ++i) n += d2.push(static_cast<uint8_t>(r.u32() & 1));
  CHECK(n == 0);
}

}  // TEST_SUITE
