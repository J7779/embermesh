#include <string>

#include "doctest.h"
#include "ember/crc.h"
#include "guides/deflate.h"

using namespace ember;
using namespace ember::guides;

namespace {
uint32_t rng(uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }

void roundtrip(const std::string& in) {
  Bytes z = deflate_raw(ByteView(in));
  std::string out;
  REQUIRE(inflate_raw(ByteView(z), in.size(), out));
  CHECK(out == in);
  // Wrong expected size is rejected (both directions).
  std::string tmp;
  if (!in.empty()) CHECK_FALSE(inflate_raw(ByteView(z), in.size() - 1, tmp));
  CHECK_FALSE(inflate_raw(ByteView(z), in.size() + 1, tmp));
}
}  // namespace

TEST_SUITE("guides") {

TEST_CASE("crc32_fast matches ember::crc32") {
  CHECK(crc32_fast(ByteView(std::string_view("123456789"))) == 0xCBF43926u);
  CHECK(crc32_fast(ByteView()) == 0);
  uint32_t s = 1;
  Bytes b;
  for (int i = 0; i < 1031; ++i) b.push_back(static_cast<uint8_t>(rng(s)));
  for (size_t n : {0u, 1u, 3u, 4u, 5u, 1031u}) CHECK(crc32_fast(ByteView(b.data(), n)) == crc32(ByteView(b.data(), n)));
  CHECK(crc32_fast(ByteView(b.data() + 10, 500), crc32_fast(ByteView(b.data(), 10))) == crc32(ByteView(b.data(), 510)));
}

TEST_CASE("inflate known zlib vectors") {
  Bytes z;
  REQUIRE(from_hex("cb48cdc9c957c84027b900", z));  // zlib level 9, raw, "hello hello hello hello\n"
  std::string out;
  REQUIRE(inflate_raw(ByteView(z), 24, out));
  CHECK(out == "hello hello hello hello\n");
  REQUIRE(from_hex("0300", z));  // empty input
  REQUIRE(inflate_raw(ByteView(z), 0, out));
  CHECK(out.empty());
  // Stored block "abc".
  REQUIRE(from_hex("010300fcff616263", z));
  REQUIRE(inflate_raw(ByteView(z), 3, out));
  CHECK(out == "abc");
}

TEST_CASE("deflate round trips") {
  roundtrip("");
  roundtrip("a");
  roundtrip("ab");
  roundtrip("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  std::string text;
  for (int i = 0; i < 400; ++i) text += "Boil water for one minute; at altitude boil for three minutes. Line " + std::to_string(i) + "\n";
  roundtrip(text);
  Bytes z = deflate_raw(ByteView(text));
  CHECK(z.size() < text.size() / 5);  // compresses repetitive text well
  // Incompressible data -> stored blocks, bounded expansion.
  uint32_t s = 99;
  std::string noise;
  for (int i = 0; i < 200000; ++i) noise.push_back(static_cast<char>(rng(s)));
  roundtrip(noise);
  CHECK(deflate_raw(ByteView(noise)).size() < noise.size() + noise.size() / 100 + 64);
  // Long runs, max-distance matches and many blocks.
  std::string runs(70000, 'x');
  for (size_t i = 0; i < runs.size(); i += 33000) runs[i] = 'y';
  roundtrip(runs);
  std::string blk;
  for (int i = 0; i < 40000; ++i) blk.push_back(static_cast<char>('a' + rng(s) % 4));
  blk += blk.substr(0, 1000);
  roundtrip(blk);
  // Deterministic.
  CHECK(deflate_raw(ByteView(text)) == deflate_raw(ByteView(text)));
}

TEST_CASE("inflate rejects malformed input without over-reading") {
  std::string text;
  for (int i = 0; i < 300; ++i) text += "Purify water with bleach: 2 drops per litre, wait 30 minutes. ";
  Bytes z = deflate_raw(ByteView(text));
  std::string out;
  // Every truncation fails cleanly.
  for (size_t n = 0; n < z.size(); ++n) CHECK_FALSE(inflate_raw(ByteView(z.data(), n), text.size(), out));
  // Random bit flips: must not crash; if accepted, output has the expected size.
  uint32_t s = 2024;
  int accepted = 0;
  for (int round = 0; round < 2000; ++round) {
    Bytes c = z;
    int flips = 1 + static_cast<int>(rng(s) % 4);
    for (int f = 0; f < flips; ++f) c[rng(s) % c.size()] ^= static_cast<uint8_t>(1u << (rng(s) % 8));
    if (inflate_raw(ByteView(c), text.size(), out)) { ++accepted; CHECK(out.size() == text.size()); }
  }
  CHECK(accepted < 2000);
  // Pure garbage.
  for (int round = 0; round < 500; ++round) {
    Bytes g(1 + rng(s) % 300);
    for (uint8_t& b : g) b = static_cast<uint8_t>(rng(s));
    inflate_raw(ByteView(g), 1 + rng(s) % 5000, out);
  }
  // Reserved block type 3, distance too far back, invalid HLIT.
  Bytes bad;
  REQUIRE(from_hex("07", bad));
  CHECK_FALSE(inflate_raw(ByteView(bad), 10, out));
  REQUIRE(from_hex("ffff", bad));
  CHECK_FALSE(inflate_raw(ByteView(bad), 10, out));
  CHECK_FALSE(inflate_raw(ByteView(), 0, out));
}

}  // TEST_SUITE
