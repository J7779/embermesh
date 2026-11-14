#include <vector>

#include "doctest.h"
#include "ember/crc.h"
#include "framing/stream.h"
#include "framing/test_rng.h"

using namespace ember;
using framing_test::Rng;

namespace {

struct Got {
  uint8_t type;
  Bytes payload;
  bool operator==(const Got& o) const { return type == o.type && payload == o.payload; }
};

std::vector<Got> feed_all(stream::Decoder& d, const Bytes& s, Rng& rng, size_t max_chunk) {
  std::vector<Got> got;
  size_t i = 0;
  while (i < s.size()) {
    size_t n = 1 + rng.below(max_chunk);
    if (n > s.size() - i) n = s.size() - i;
    d.feed(ByteView(s.data() + i, n), [&](uint8_t t, ByteView p) { got.push_back({t, p.to_bytes()}); });
    i += n;
  }
  return got;
}

}  // namespace

TEST_SUITE("framing") {

TEST_CASE("stream framer: wire format is as documented") {
  Bytes f = stream::encode(ByteView(std::string_view("hi")), 7);
  REQUIRE(f.size() == 12);
  CHECK(f[0] == 0xE5);
  CHECK(f[1] == 0x4D);
  CHECK(f[2] == 7);
  CHECK(f[3] == 0);
  CHECK(f[4] == 2);
  CHECK(f[5] == crc8(ByteView(f.data() + 2, 3)));
  CHECK(f[6] == 'h');
  CHECK(f[7] == 'i');
  uint32_t c = crc32(ByteView(f.data() + 2, 6));
  CHECK(f[8] == static_cast<uint8_t>(c >> 24));
  CHECK(f[11] == static_cast<uint8_t>(c));
  CHECK(stream::encode(ByteView()).size() == stream::kOverhead);
  Bytes big(65536);
  CHECK(stream::encode(big).empty());
  big.resize(65535);
  CHECK(stream::encode(big).size() == 65535 + 10);
}

TEST_CASE("stream framer: empty, 1 byte, max, max+1") {
  stream::Decoder d(100);
  Rng rng(21);
  Bytes s;
  stream::encode_append(ByteView(), s, 1);
  stream::encode_append(Bytes{0x42}, s, 2);
  Bytes mx(100, 0xE5);  // payload full of sync bytes
  stream::encode_append(mx, s, 3);
  Bytes over(101, 0x11);
  stream::encode_append(over, s, 4);  // rejected by the cap
  stream::encode_append(Bytes{0x43}, s, 5);
  auto got = feed_all(d, s, rng, 1);
  REQUIRE(got.size() == 4);
  CHECK(got[0] == Got{1, {}});
  CHECK(got[1] == Got{2, {0x42}});
  CHECK(got[2] == Got{3, mx});
  CHECK(got[3] == Got{5, {0x43}});
  CHECK(d.stats().oversize >= 1);
  CHECK(d.buffered() == 0);
}

TEST_CASE("stream framer: pull interface consumes exactly one frame and buffers the rest") {
  stream::Decoder d;
  Bytes s = stream::encode(Bytes{1});
  stream::encode_append(Bytes{2, 2}, s);
  bool ready = false;
  size_t n = d.feed_some(s, &ready);
  CHECK(ready);
  CHECK(n == 11);
  CHECK(d.payload() == ByteView(Bytes{1}));
  n = d.feed_some(ByteView(s).sub(n), &ready);
  CHECK(ready);
  CHECK(n == 12);
  CHECK(d.payload() == ByteView(Bytes{2, 2}));
  n = d.feed_some(ByteView(), &ready);
  CHECK_FALSE(ready);
  CHECK(n == 0);
}

TEST_CASE("stream framer: random frames, leading garbage, arbitrary chunking") {
  Rng rng(22);
  for (int iter = 0; iter < 30; ++iter) {
    std::vector<Got> sent;
    Bytes s = rng.bytes(rng.below(200));  // leading garbage (may contain sync bytes)
    for (auto& b : s)
      if (rng.below(6) == 0) b = rng.below(2) ? stream::SYNC0 : stream::SYNC1;
    for (int i = 0; i < 20; ++i) {
      Got g{static_cast<uint8_t>(rng.byte()), rng.bytes(rng.below(300))};
      for (auto& b : g.payload)
        if (rng.below(8) == 0) b = stream::SYNC0;
      stream::encode_append(g.payload, s, g.type);
      sent.push_back(g);
    }
    for (size_t mc : {size_t(1), size_t(5), size_t(64), size_t(100000)}) {
      stream::Decoder d(300);
      auto got = feed_all(d, s, rng, mc);
      CHECK(got == sent);
    }
  }
}

TEST_CASE("stream framer: corruption loses only the damaged frame; corrupted length resyncs fast") {
  Rng rng(23);
  int recovered_total = 0, damaged_total = 0;
  for (int iter = 0; iter < 200; ++iter) {
    std::vector<Got> sent;
    std::vector<size_t> starts;
    Bytes s;
    for (int i = 0; i < 6; ++i) {
      Got g{0, rng.bytes(1 + rng.below(120))};
      starts.push_back(s.size());
      stream::encode_append(g.payload, s, 0);
      sent.push_back(g);
    }
    starts.push_back(s.size());
    // damage frame k: flip a bit, corrupt its length, drop bytes, or insert garbage
    size_t k = rng.below(6);
    size_t a = starts[k], b = starts[k + 1];
    switch (iter % 4) {
      case 0: s[a + rng.below(b - a)] ^= static_cast<uint8_t>(1u << rng.below(8)); break;
      case 1: s[a + 3] = 0xFF; s[a + 4] = 0xFF; break;  // length 65535 (header CRC now wrong)
      case 2: {
        size_t from = a + 2 + rng.below(b - a - 2);
        size_t to = from + 1 + rng.below(b - from);
        s.erase(s.begin() + static_cast<std::ptrdiff_t>(from), s.begin() + static_cast<std::ptrdiff_t>(to));
        break;
      }
      default: {
        Bytes junk = rng.bytes(1 + rng.below(30));
        s.insert(s.begin() + static_cast<std::ptrdiff_t>(a + 1 + rng.below(b - a - 1)), junk.begin(), junk.end());
      }
    }
    stream::Decoder d(4096);
    auto got = feed_all(d, s, rng, 1 + rng.below(50));
    // never deliver anything that was not sent
    for (auto& g : got) {
      bool found = false;
      for (auto& x : sent) found |= (x == g);
      CHECK(found);
    }
    // every undamaged frame arrives
    for (size_t i = 0; i < sent.size(); ++i) {
      if (i == k) continue;
      bool found = false;
      for (auto& g : got) found |= (g == sent[i]);
      CHECK(found);
    }
    recovered_total += static_cast<int>(got.size());
    damaged_total += 1;
  }
  CHECK(recovered_total >= damaged_total * 5);
}

TEST_CASE("stream framer: valid-looking header with huge length does not stall the stream") {
  // A forged header claiming 4000 bytes followed by real frames: with max_payload 4096 the decoder
  // waits for 4010 bytes, then rejects the CRC and rescans -> all real frames found in order.
  Bytes s = {stream::SYNC0, stream::SYNC1, 0, 0x0F, 0xA0};
  s.push_back(crc8(ByteView(s.data() + 2, 3)));
  std::vector<Got> sent;
  Rng rng(24);
  while (s.size() < 6000) {
    Got g{9, rng.bytes(50)};
    stream::encode_append(g.payload, s, 9);
    sent.push_back(g);
  }
  stream::Decoder d(4096);
  auto got = feed_all(d, s, rng, 37);
  CHECK(got == sent);
  // with a small cap the forged header is rejected immediately as oversize
  stream::Decoder d2(256);
  got = feed_all(d2, s, rng, 37);
  CHECK(got == sent);
  CHECK(d2.stats().oversize == 1);
}

TEST_CASE("stream framer fuzz: random noise is never accepted; memory stays bounded") {
  Rng rng(25);
  stream::Decoder d(512);
  size_t accepted = 0;
  for (int iter = 0; iter < 300; ++iter) {
    Bytes junk = rng.bytes(rng.below(2000));
    for (auto& b : junk)
      if (rng.below(4) == 0) b = rng.below(2) ? stream::SYNC0 : stream::SYNC1;
    d.feed(junk, [&](uint8_t, ByteView) { ++accepted; });
    CHECK(d.buffered() <= 512 + stream::kOverhead);
  }
  CHECK(accepted == 0);
  // bit-flipped real frames are never accepted
  Bytes f = stream::encode(rng.bytes(64));
  for (size_t i = 0; i < f.size() * 8; ++i) {
    Bytes g = f;
    g[i / 8] ^= static_cast<uint8_t>(1u << (i % 8));
    stream::Decoder dd(512);
    dd.feed(g, [&](uint8_t, ByteView) { ++accepted; });
  }
  CHECK(accepted == 0);
  // reset clears partial state
  d.feed(ByteView(f).sub(0, 20), [](uint8_t, ByteView) {});
  d.reset();
  CHECK(d.buffered() == 0);
  int n = 0;
  d.feed(f, [&](uint8_t, ByteView) { ++n; });
  CHECK(n == 1);
}

}  // TEST_SUITE
