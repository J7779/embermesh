#include <vector>

#include "doctest.h"
#include "framing/cobs.h"
#include "framing/kiss.h"
#include "framing/slip.h"
#include "framing/test_rng.h"

using namespace ember;
using framing_test::Rng;

namespace {

Bytes H(const char* hex) {
  Bytes b;
  REQUIRE(from_hex(hex, b));
  return b;
}

// Feeds `stream` to a decoder in random chunk sizes and collects frames.
template <class Dec, class Collect>
void feed_chunked(Dec& d, const Bytes& stream, Rng& rng, size_t max_chunk, Collect&& collect) {
  size_t i = 0;
  while (i < stream.size()) {
    size_t n = 1 + rng.below(max_chunk);
    if (n > stream.size() - i) n = stream.size() - i;
    d.feed(ByteView(stream.data() + i, n), collect);
    i += n;
  }
}

}  // namespace

TEST_SUITE("framing") {

// ---------------------------------------------------------------- SLIP

TEST_CASE("SLIP encode escapes END and ESC (RFC 1055)") {
  CHECK(slip::encode(H("01c002db03")) == H("c001dbdc02dbdd03c0"));
  CHECK(slip::encode(H("01"), false) == H("01c0"));
  CHECK(slip::encode(ByteView()) == H("c0c0"));
  Bytes out;
  CHECK(slip::unescape(H("01dbdc02dbdd03"), out));
  CHECK(out == H("01c002db03"));
  CHECK_FALSE(slip::unescape(H("01db"), out));    // dangling ESC
  CHECK_FALSE(slip::unescape(H("01db05"), out));  // invalid escape
  CHECK_FALSE(slip::unescape(H("01c0"), out));    // END inside body
  CHECK(out.empty());
}

TEST_CASE("SLIP decoder: back-to-back ENDs, garbage, bad escape resync, max frame") {
  slip::Decoder d(4);
  std::vector<Bytes> got;
  auto collect = [&](ByteView f) { got.push_back(f.to_bytes()); };
  // no leading END required by default: first frame delivered
  d.feed(H("0102c0c0c0"), collect);
  REQUIRE(got.size() == 1);
  CHECK(got[0] == H("0102"));
  // bad escape: frame dropped until the next END, following frame fine
  d.feed(H("aadb01bbc0" "05c0"), collect);
  REQUIRE(got.size() == 2);
  CHECK(got[1] == H("05"));
  CHECK(d.stats().bad_escape == 1);
  // exactly max (4) accepted, max+1 dropped
  d.feed(H("01020304c0" "0102030405c0" "07c0"), collect);
  REQUIRE(got.size() == 4);
  CHECK(got[2] == H("01020304"));
  CHECK(got[3] == H("07"));
  CHECK(d.stats().oversize == 1);
  // escaped byte that overflows max
  d.feed(H("01020304dbdcc0"), collect);
  CHECK(got.size() == 4);
  CHECK(d.stats().oversize == 2);
  // ESC END: frame aborted, END starts a new frame
  d.feed(H("0102dbc0" "09c0"), collect);
  REQUIRE(got.size() == 5);
  CHECK(got[4] == H("09"));
}

TEST_CASE("SLIP decoder with require_leading_end discards initial noise") {
  slip::Decoder d(100, true);
  std::vector<Bytes> got;
  d.feed(H("aabbccc0" "0102c0"), [&](ByteView f) { got.push_back(f.to_bytes()); });
  REQUIRE(got.size() == 1);
  CHECK(got[0] == H("0102"));
  CHECK(d.stats().discarded_bytes == 3);
  d.reset();
  got.clear();
  d.feed(H("0102c0"), [&](ByteView f) { got.push_back(f.to_bytes()); });
  CHECK(got.empty());
}

TEST_CASE("SLIP feed_some stops exactly at the frame end") {
  slip::Decoder d;
  Bytes s = H("c00102c00304c0");
  bool got = false;
  size_t n = d.feed_some(ByteView(s), &got);
  CHECK(got);
  CHECK(n == 4);
  CHECK(d.frame() == ByteView(H("0102")));
  n = d.feed_some(ByteView(s).sub(4), &got);
  CHECK(got);
  CHECK(n == 3);
  CHECK(d.frame() == ByteView(H("0304")));
  n = d.feed_some(ByteView(), &got);
  CHECK(n == 0);
  CHECK_FALSE(got);
}

TEST_CASE("SLIP random round trips in random chunkings") {
  Rng rng(1);
  for (int iter = 0; iter < 50; ++iter) {
    std::vector<Bytes> sent;
    Bytes stream;
    size_t nframes = 1 + rng.below(10);
    for (size_t i = 0; i < nframes; ++i) {
      Bytes f = rng.bytes(1 + rng.below(300));
      for (auto& b : f)
        if (rng.below(4) == 0) b = (rng.below(2) ? slip::END : slip::ESC);
      sent.push_back(f);
      slip::encode_append(f, stream, rng.below(2) == 0);
    }
    slip::Decoder d(300);
    std::vector<Bytes> got;
    feed_chunked(d, stream, rng, 1 + rng.below(40), [&](ByteView f) { got.push_back(f.to_bytes()); });
    CHECK(got == sent);
  }
}

// ---------------------------------------------------------------- KISS

TEST_CASE("KISS encoding matches the KISS spec examples") {
  // Data frame "TEST" on port 0.
  CHECK(kiss::encode_data(ByteView(std::string_view("TEST"))) == H("c00054455354c0"));
  // FEND / FESC in data are transposed.
  CHECK(kiss::encode(H("c0db")) == H("c000dbdcdbddc0"));
  // Port 5 data
  CHECK(kiss::encode(H("01"), 5) == H("c05001c0"));
  // Parameters
  CHECK(kiss::encode_param(kiss::Command::TxDelay, 50) == H("c00132c0"));
  CHECK(kiss::encode_param(kiss::Command::Persistence, 63, 1) == H("c0123fc0"));
  CHECK(kiss::encode_param(kiss::Command::SlotTime, 10) == H("c0030ac0"));
  CHECK(kiss::encode_param(kiss::Command::TxTail, 2) == H("c00402c0"));
  CHECK(kiss::encode_param(kiss::Command::FullDuplex, 1) == H("c00501c0"));
  CHECK(kiss::encode(H("aabb"), 0, kiss::Command::SetHardware) == H("c006aabbc0"));
  CHECK(kiss::encode_return() == H("c0ffc0"));
  // Port 12 data frame: type byte 0xC0 must itself be escaped; port 13 cmd 11 -> 0xDB.
  CHECK(kiss::encode(H("01"), 12) == H("c0dbdc01c0"));
  CHECK(kiss::encode(H("01"), 13, static_cast<kiss::Command>(11)) == H("c0dbdd01c0"));
  CHECK(kiss::encode(H("01"), 16).empty());
}

TEST_CASE("KISS decoder: ports, commands, return, garbage, back-to-back FENDs") {
  kiss::Decoder d(16);
  struct F { uint8_t port; kiss::Command cmd; Bytes payload; };
  std::vector<F> got;
  auto collect = [&](const kiss::Frame& f) { got.push_back({f.port, f.command, f.payload.to_bytes()}); };
  Bytes s = H("1234"           // garbage before the first FEND (discarded)
              "c0c0c0"         // back-to-back FENDs
              "c00054455354c0"
              "c0123fc0"
              "c0dbdc01c0"     // port 12
              "c0ffc0"
              "c0" "0e0102" "c0"  // non-standard command nibble 14 is passed through
              );
  d.feed(ByteView(s), collect);
  REQUIRE(got.size() == 5);
  CHECK(got[0].port == 0);
  CHECK(got[0].cmd == kiss::Command::Data);
  CHECK(got[0].payload == H("54455354"));
  CHECK(got[1].port == 1);
  CHECK(got[1].cmd == kiss::Command::Persistence);
  CHECK(got[1].payload == H("3f"));
  CHECK(got[2].port == 12);
  CHECK(got[2].payload == H("01"));
  CHECK(got[3].cmd == kiss::Command::Return);
  CHECK(got[3].payload.empty());
  CHECK(static_cast<uint8_t>(got[4].cmd) == 14);
  CHECK(d.stats().discarded_bytes == 2);
}

TEST_CASE("KISS decoder max frame boundary (max, max+1) and bad escape") {
  kiss::Decoder d(4);
  std::vector<Bytes> got;
  auto collect = [&](const kiss::Frame& f) { got.push_back(f.payload.to_bytes()); };
  d.feed(H("c00001020304c0"), collect);     // exactly 4 payload bytes
  d.feed(H("c0000102030405c0"), collect);   // 5: dropped
  d.feed(H("c000db41c0" "c00009c0"), collect);  // bad escape then good
  REQUIRE(got.size() == 2);
  CHECK(got[0] == H("01020304"));
  CHECK(got[1] == H("09"));
  CHECK(d.stats().oversize == 1);
  CHECK(d.stats().bad_escape == 1);
  // Empty payload data frame (just the type byte) is delivered with empty payload.
  got.clear();
  d.feed(H("c000c0"), collect);
  REQUIRE(got.size() == 1);
  CHECK(got[0].empty());
}

TEST_CASE("KISS round trip one byte at a time and random chunks, all ports") {
  Rng rng(2);
  for (int iter = 0; iter < 40; ++iter) {
    struct F { uint8_t port; Bytes payload; };
    std::vector<F> sent;
    Bytes stream = rng.bytes(rng.below(5));  // leading line noise
    stream.push_back(kiss::FEND);
    for (int i = 0; i < 8; ++i) {
      F f{static_cast<uint8_t>(rng.below(16)), rng.bytes(rng.below(200))};
      for (auto& b : f.payload)
        if (rng.below(5) == 0) b = rng.below(2) ? kiss::FEND : kiss::FESC;
      kiss::encode_append(f.payload, stream, f.port);
      sent.push_back(f);
    }
    for (size_t max_chunk : {size_t(1), size_t(7), size_t(1000)}) {
      kiss::Decoder d(256);
      std::vector<F> got;
      feed_chunked(d, stream, rng, max_chunk,
                   [&](const kiss::Frame& f) { got.push_back({f.port, f.payload.to_bytes()}); });
      REQUIRE(got.size() == sent.size());
      for (size_t i = 0; i < sent.size(); ++i) {
        CHECK(got[i].port == sent[i].port);
        CHECK(got[i].payload == sent[i].payload);
      }
    }
  }
}

TEST_CASE("KISS decoder fuzz: random garbage never crashes and respects max frame") {
  Rng rng(3);
  kiss::Decoder d(64);
  for (int iter = 0; iter < 200; ++iter) {
    Bytes junk = rng.bytes(rng.below(500));
    for (auto& b : junk)
      if (rng.below(8) == 0) b = rng.below(2) ? kiss::FEND : kiss::FESC;
    d.feed(ByteView(junk), [&](const kiss::Frame& f) { CHECK(f.payload.size() <= 64); });
  }
  // After garbage the decoder still decodes a clean frame.
  std::vector<Bytes> got;
  d.feed(kiss::encode_data(H("0102")), [&](const kiss::Frame& f) { got.push_back(f.payload.to_bytes()); });
  REQUIRE(!got.empty());
  CHECK(got.back() == H("0102"));
}

// ---------------------------------------------------------------- COBS

TEST_CASE("COBS known vectors (Wikipedia / Cheshire-Baker)") {
  CHECK(cobs::encode(ByteView()) == H("01"));
  CHECK(cobs::encode(H("00")) == H("0101"));
  CHECK(cobs::encode(H("0000")) == H("010101"));
  CHECK(cobs::encode(H("001100")) == H("01021101"));
  CHECK(cobs::encode(H("11220033")) == H("0311220233"));
  CHECK(cobs::encode(H("11223344")) == H("0511223344"));
  CHECK(cobs::encode(H("11000000")) == H("02110101" "01"));
  // 254 non-zero bytes 01..FE -> FF 01..FE (+ trailing 01 block with this encoder)
  Bytes in;
  for (int i = 1; i <= 254; ++i) in.push_back(static_cast<uint8_t>(i));
  Bytes enc = cobs::encode(in);
  REQUIRE(enc.size() == 256);
  CHECK(enc[0] == 0xFF);
  CHECK(enc[255] == 0x01);
  Bytes out;
  CHECK(cobs::decode(enc, out));
  CHECK(out == in);
  // The canonical (shorter) encoding without the trailing 01 decodes identically.
  CHECK(cobs::decode(ByteView(enc).sub(0, 255), out));
  CHECK(out == in);
  // 255 bytes 01..FF -> FF 01..FE 02 FF
  in.push_back(0xFF);
  enc = cobs::encode(in);
  CHECK(enc.size() == 257);
  CHECK(enc[255] == 0x02);
  CHECK(enc[256] == 0xFF);
  CHECK(cobs::decode(enc, out));
  CHECK(out == in);
}

TEST_CASE("COBS strict decode rejects malformed input") {
  Bytes out;
  CHECK_FALSE(cobs::decode(ByteView(), out));
  CHECK_FALSE(cobs::decode(H("00"), out));
  CHECK_FALSE(cobs::decode(H("0311"), out));      // code past end
  CHECK_FALSE(cobs::decode(H("031100"), out));    // zero inside data
  CHECK_FALSE(cobs::decode(H("0211" "00"), out)); // zero code
  CHECK(out.empty());
}

TEST_CASE("COBS exhaustive small round trips") {
  // every 0/1/2-byte input and a sample of 3-byte inputs with many zeros
  Bytes out;
  CHECK(cobs::decode(cobs::encode(ByteView()), out));
  CHECK(out.empty());
  for (int a = 0; a < 256; ++a) {
    Bytes in1 = {static_cast<uint8_t>(a)};
    REQUIRE(cobs::decode(cobs::encode(in1), out));
    CHECK(out == in1);
    for (int b = 0; b < 256; b += 3) {
      Bytes in2 = {static_cast<uint8_t>(a), static_cast<uint8_t>(b)};
      Bytes e = cobs::encode(in2);
      for (uint8_t x : e) CHECK(x != 0);
      REQUIRE(cobs::decode(e, out));
      CHECK(out == in2);
    }
  }
  Rng rng(4);
  for (int iter = 0; iter < 300; ++iter) {
    Bytes in = rng.bytes(rng.below(1200));
    for (auto& b : in)
      if (rng.below(3) == 0) b = 0;
    Bytes e = cobs::encode(in);
    CHECK(e.size() <= cobs::max_encoded_size(in.size()));
    REQUIRE(cobs::decode(e, out));
    CHECK(out == in);
  }
}

TEST_CASE("COBS streaming decoder: chunking, garbage, corruption resync, max frame") {
  Rng rng(5);
  std::vector<Bytes> sent;
  Bytes stream = {0x00, 0x00};
  for (int i = 0; i < 30; ++i) {
    Bytes f = rng.bytes(1 + rng.below(600));
    for (auto& b : f)
      if (rng.below(4) == 0) b = 0;
    sent.push_back(f);
    Bytes e = cobs::encode_frame(f);
    stream.insert(stream.end(), e.begin(), e.end());
  }
  for (size_t mc : {size_t(1), size_t(13), size_t(5000)}) {
    cobs::Decoder d(600);
    std::vector<Bytes> got;
    feed_chunked(d, stream, rng, mc, [&](ByteView f) { got.push_back(f.to_bytes()); });
    CHECK(got == sent);
  }
  // max / max+1
  cobs::Decoder d(10);
  std::vector<Bytes> got;
  auto col = [&](ByteView f) { got.push_back(f.to_bytes()); };
  d.feed(cobs::encode_frame(rng.bytes(10)), col);
  CHECK(got.size() == 1);
  Bytes big(11, 0x55);
  d.feed(cobs::encode_frame(big), col);
  CHECK(got.size() == 1);
  Bytes huge(5000, 0x55);  // triggers the encoded-length cap while streaming
  d.feed(cobs::encode_frame(huge), col);
  CHECK(got.size() == 1);
  CHECK(d.stats().oversize + d.stats().invalid == 2);
  // malformed piece then good frame
  d.feed(H("0511" "00"), col);
  d.feed(cobs::encode_frame(H("0102")), col);
  REQUIRE(got.size() == 2);
  CHECK(got[1] == H("0102"));
}

}  // TEST_SUITE
