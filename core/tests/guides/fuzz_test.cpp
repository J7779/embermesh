// Seeded corruption fuzzing of guides.pack: random bit flips, truncations, garbage, and targeted
// field corruption with *repaired checksums* (so the structural validation and the bounds-checked
// readers behind the checksums are exercised, not just the CRC). Run under ASan/UBSan.
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "doctest.h"
#include "fixture.h"
#include "guides/library.h"
#include "guides/pack_builder.h"
#include "guides/pack_format.h"
#include "guides/text.h"

using namespace ember;
using namespace ember::guides;

namespace {
uint32_t rng(uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }

// Exercises every read path; must never crash, and must always return well-formed data.
void exercise(const GuideLibrary& lib) {
  for (const CategoryInfo& c : lib.categories()) {
    CHECK(utf8_valid(c.id));
    lib.list(c.id, "", 0, 50);
  }
  GuideList all = lib.list("", "", 0, 100);
  Guide g;
  for (const GuideMeta& m : all.guides) {
    CHECK(utf8_valid(m.id));
    CHECK(utf8_valid(m.title));
    if (lib.get(m.id, g) == GetStatus::kOk) CHECK(utf8_valid(g.markdown));
    lib.list("", m.part_of, 0, 10);
  }
  for (const char* q : {"water", "bleeding wont stop", "j-pole 2m", "frosbite", "w", "tourn", "zzzz"}) {
    for (const SearchResult& r : lib.search(q, "", 5)) {
      CHECK(utf8_valid(r.snippet));
      CHECK(utf8_valid(r.title));
    }
  }
  std::string err;
  lib.verify(&err);
  lib.stats();
}

uint32_t rd32(const Bytes& b, size_t off) {
  return static_cast<uint32_t>(b[off]) | static_cast<uint32_t>(b[off + 1]) << 8 |
         static_cast<uint32_t>(b[off + 2]) << 16 | static_cast<uint32_t>(b[off + 3]) << 24;
}
void wr32(Bytes& b, size_t off, uint32_t v) {
  for (int i = 0; i < 4; ++i) b[off + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i));
}
}  // namespace

TEST_SUITE("guides") {

TEST_CASE("pack load rejects bad headers") {
  const Bytes& good = guides_test::fixture_pack();
  GuideLibrary lib;
  CHECK(lib.load(ByteView(), false) == LoadStatus::kTooSmall);
  CHECK(lib.load(ByteView(good.data(), kHeaderSize - 1), false) == LoadStatus::kTooSmall);
  Bytes b = good;
  b[0] = 'X';
  CHECK(lib.load(ByteView(b), false) == LoadStatus::kBadMagic);
  b = good;
  wr32(b, kHdrVersion, 2);
  CHECK(lib.load(ByteView(b), false) == LoadStatus::kBadVersion);
  b = good;
  b[kHdrDocCount] ^= 1;
  CHECK(lib.load(ByteView(b), false) == LoadStatus::kBadChecksum);
  b = good;
  b[b.size() - 200] ^= 0x40;  // inside BODIES: load succeeds, verify / get catch it
  CHECK(lib.load(ByteView(b), false) == LoadStatus::kOk);
  CHECK_FALSE(lib.verify());
  b = good;
  size_t strings_off = rd32(b, kHdrSections);
  b[strings_off + 3] ^= 0x01;  // STRINGS: checked at load
  CHECK(lib.load(ByteView(b), false) == LoadStatus::kBadChecksum);
  // Truncation at every length fails cleanly.
  for (size_t n = 0; n < good.size(); n += 1 + n / 50) {
    LoadStatus st = lib.load(ByteView(good.data(), n), false);
    CHECK(st != LoadStatus::kOk);
    exercise(lib);
  }
  // Valid checksums but doc count inconsistent with the section size.
  b = good;
  wr32(b, kHdrDocCount, rd32(b, kHdrDocCount) + 1);
  REQUIRE(repair_pack_checksums(b));
  CHECK(lib.load(ByteView(b), false) == LoadStatus::kBadLayout);
  b = good;
  wr32(b, kHdrSections + 12 * kSecTerms, 0xFFFFFF00u);  // section offset out of range
  CHECK_FALSE(repair_pack_checksums(b));
}

TEST_CASE("pack fuzz: random corruption (raw)") {
  const Bytes& good = guides_test::fixture_pack();
  uint32_t s = 0xC0FFEE;
  GuideLibrary lib;
  int loaded = 0;
  for (int round = 0; round < 400; ++round) {
    Bytes b = good;
    int mode = static_cast<int>(rng(s) % 4);
    if (mode == 0) {
      int flips = 1 + static_cast<int>(rng(s) % 8);
      for (int i = 0; i < flips; ++i) b[rng(s) % b.size()] ^= static_cast<uint8_t>(1u << (rng(s) % 8));
    } else if (mode == 1) {
      b.resize(rng(s) % b.size());
    } else if (mode == 2) {
      size_t off = rng(s) % b.size(), len = std::min<size_t>(b.size() - off, 1 + rng(s) % 64);
      for (size_t i = 0; i < len; ++i) b[off + i] = static_cast<uint8_t>(rng(s));
    } else {
      b.resize(rng(s) % 600);
      for (uint8_t& x : b) x = static_cast<uint8_t>(rng(s));
      if (b.size() >= 8) std::memcpy(b.data(), kPackMagic, 8);
    }
    if (lib.load(ByteView(b), rng(s) & 1) == LoadStatus::kOk) ++loaded;
    exercise(lib);
  }
  CHECK(loaded < 400);
}

TEST_CASE("pack fuzz: corruption behind repaired checksums") {
  const Bytes& good = guides_test::fixture_pack();
  uint32_t s = 0xBADC0DE;
  GuideLibrary lib;
  int loaded = 0;
  for (int round = 0; round < 1500; ++round) {
    Bytes b = good;
    uint32_t sec = rng(s) % kSectionCount;
    size_t off = rd32(b, kHdrSections + 12 * sec), len = rd32(b, kHdrSections + 12 * sec + 4);
    int mode = static_cast<int>(rng(s) % 5);
    if (len == 0) continue;
    if (mode == 0) {  // bit flips inside one section
      int flips = 1 + static_cast<int>(rng(s) % 4);
      for (int i = 0; i < flips; ++i) b[off + rng(s) % len] ^= static_cast<uint8_t>(1u << (rng(s) % 8));
    } else if (mode == 1) {  // random 32-bit word inside a section (lengths, offsets, doc numbers)
      if (len < 4) continue;
      size_t p = off + (rng(s) % (len - 3));
      uint32_t v = rng(s);
      if (rng(s) & 1) v = v % 300;           // small values hit plausible ranges
      if (rng(s) % 4 == 0) v = 0xFFFFFFFFu;
      wr32(b, p, v);
    } else if (mode == 2) {  // shrink or grow a section length in the header
      uint32_t nl = static_cast<uint32_t>(rng(s) % (len + 16));
      wr32(b, kHdrSections + 12 * sec + 4, nl);
    } else if (mode == 3) {  // header counts
      size_t which = (rng(s) % 3) * 4 + kHdrDocCount;
      wr32(b, which, rng(s) % 64);
    } else {  // varint-ish garbage burst in TERMS / POSTINGS
      uint32_t ts = (rng(s) & 1) ? kSecTerms : kSecPostings;
      size_t to = rd32(b, kHdrSections + 12 * ts), tl = rd32(b, kHdrSections + 12 * ts + 4);
      if (tl == 0) continue;
      size_t p = to + rng(s) % tl;
      size_t n = std::min<size_t>(to + tl - p, 1 + rng(s) % 12);
      for (size_t i = 0; i < n; ++i) b[p + i] = static_cast<uint8_t>(0x80 | rng(s));
    }
    if (!repair_pack_checksums(b)) continue;
    if (lib.load(ByteView(b), false) == LoadStatus::kOk) ++loaded;
    exercise(lib);
  }
  // Many structurally-valid mutations still load (e.g. a flipped letter in a title) — that is
  // fine; what matters is that nothing crashes and every returned string is valid UTF-8.
  CHECK(loaded > 0);
}

}  // TEST_SUITE
