// Security review H6 regressions: checksum-valid but hostile guides.pack files must not make
// load/get/search expensive. Packs are built with the real builder, then doctored (bodies
// appended, doc records re-pointed, checksums repaired) the way an attacker would.
#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>

#include "doctest.h"
#include "guides/deflate.h"
#include "guides/library.h"
#include "guides/pack_builder.h"
#include "guides/pack_format.h"
#include "guides/text.h"

using namespace ember;
using namespace ember::guides;

namespace {

uint32_t rd32(const Bytes& b, size_t o) {
  return static_cast<uint32_t>(b[o]) | static_cast<uint32_t>(b[o + 1]) << 8 | static_cast<uint32_t>(b[o + 2]) << 16 |
         static_cast<uint32_t>(b[o + 3]) << 24;
}
void wr32(Bytes& b, size_t o, uint32_t v) {
  for (int i = 0; i < 4; ++i) b[o + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i));
}

double seconds_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

std::vector<SourceGuide> water_guides(int n) {
  std::vector<SourceGuide> gs;
  for (int i = 0; i < n; ++i) {
    SourceGuide g;
    char id[32];
    std::snprintf(id, sizeof id, "water/w-%04d", i);
    g.meta.id = id;
    g.meta.title = "Water";
    g.meta.category = "water";
    g.meta.summary = "Water.";
    g.meta.tags = {"water"};
    g.meta.source = "original";
    g.meta.source_title = "t";
    g.meta.license = "CC0-1.0";
    g.meta.attribution = "t";
    g.markdown = "# Water\n\nwater\n";
    gs.push_back(g);
  }
  return gs;
}

// Raw DEFLATE made only of stored blocks (ratio ~1: passes every expansion check).
Bytes stored_stream(std::string_view s) {
  Bytes out;
  size_t off = 0;
  do {
    size_t n = std::min<size_t>(65535, s.size() - off);
    bool last = off + n == s.size();
    out.push_back(last ? 1 : 0);
    out.push_back(static_cast<uint8_t>(n));
    out.push_back(static_cast<uint8_t>(n >> 8));
    out.push_back(static_cast<uint8_t>(~n));
    out.push_back(static_cast<uint8_t>(~n >> 8));
    out.insert(out.end(), s.begin() + static_cast<std::ptrdiff_t>(off), s.begin() + static_cast<std::ptrdiff_t>(off + n));
    off += n;
  } while (off < s.size());
  return out;
}

// `nblocks` fixed-Huffman blocks, each holding one literal 'a' (decompression-bomb case 2).
Bytes many_fixed_blocks(size_t nblocks) {
  Bytes out;
  uint64_t acc = 0;
  int cnt = 0;
  auto put = [&](uint32_t v, int n) {
    acc |= static_cast<uint64_t>(v) << cnt;
    cnt += n;
    while (cnt >= 8) { out.push_back(static_cast<uint8_t>(acc)); acc >>= 8; cnt -= 8; }
  };
  auto rev = [](uint32_t c, int l) { uint32_t r = 0; for (int i = 0; i < l; ++i) { r = (r << 1) | (c & 1); c >>= 1; } return r; };
  for (size_t i = 0; i < nblocks; ++i) {
    put(i + 1 == nblocks ? 1 : 0, 1);
    put(1, 2);
    put(rev(0x30 + 'a', 8), 8);
    put(0, 7);
  }
  if (cnt) put(0, 8 - cnt);
  return out;
}

struct Body { Bytes comp; uint32_t bytes; uint32_t crc; };

// Appends `bodies` to BODIES (the last section) and points doc d at bodies[map[d]].
Bytes doctor(Bytes pack, const std::vector<Body>& bodies, const std::vector<size_t>& map) {
  uint32_t bsec = rd32(pack, kHdrSections + 12 * kSecBodies), blen = rd32(pack, kHdrSections + 12 * kSecBodies + 4);
  REQUIRE(bsec + blen == pack.size());
  std::vector<uint32_t> offs;
  uint32_t at = blen;
  for (const Body& b : bodies) {
    offs.push_back(at);
    pack.insert(pack.end(), b.comp.begin(), b.comp.end());
    at += static_cast<uint32_t>(b.comp.size());
  }
  wr32(pack, kHdrSections + 12 * kSecBodies + 4, at);
  uint32_t docs = rd32(pack, kHdrSections + 12 * kSecDocs);
  for (size_t d = 0; d < map.size(); ++d) {
    size_t r = docs + d * kDocRecordSize;
    const Body& b = bodies[map[d]];
    wr32(pack, r + kDocBytes, b.bytes);
    wr32(pack, r + kDocBodyOff, offs[map[d]]);
    wr32(pack, r + kDocBodyLen, static_cast<uint32_t>(b.comp.size()));
    wr32(pack, r + kDocBodyCrc, b.crc);
  }
  REQUIRE(repair_pack_checksums(pack));
  return pack;
}

Body body_of(const std::string& text, bool stored) {
  Body b;
  b.comp = stored ? stored_stream(text) : deflate_raw(ByteView(text));
  b.bytes = static_cast<uint32_t>(text.size());
  b.crc = crc32_fast(ByteView(text));
  return b;
}

}  // namespace

TEST_SUITE("guides") {

TEST_CASE("H6 regression: every doc sharing one decompression-bomb body is rejected") {
  // Decompression-bomb case 0 scaled to 1 MiB: 205 docs matching "water", all pointing at
  // ONE body that inflates to 1 MiB of "a\n". Before the fix this loaded and verified, and one
  // search inflated the body 205 times.
  std::string big;
  while (big.size() + 2 <= (1u << 20)) big += "a\n";
  Body bomb = body_of(big, false);
  Bytes pack = doctor(build_pack(water_guides(205)), {bomb}, std::vector<size_t>(205, 0));
  GuideLibrary lib;
  LoadStatus st = lib.load(ByteView(pack), false);
  CHECK(st == LoadStatus::kBadLayout);
  auto t0 = std::chrono::steady_clock::now();
  lib.search("water", "", 200);
  CHECK(seconds_since(t0) < 1.0);
}

TEST_CASE("H6: overlapping body ranges are rejected, adjacent and distinct ones accepted") {
  Bytes good = build_pack(water_guides(3));
  GuideLibrary lib;
  REQUIRE(lib.load(ByteView(good), false) == LoadStatus::kOk);
  CHECK(lib.verify());
  Body a = body_of("# A\n\nwater one\n", false), b = body_of("# B\n\nwater two\n", false);
  // Distinct, adjacent bodies: fine.
  Bytes ok = doctor(good, {a, b, a}, {0, 1, 2});
  CHECK(lib.load(ByteView(ok), false) == LoadStatus::kOk);
  CHECK(lib.verify());
  // Two docs sharing one body: rejected.
  Bytes shared = doctor(good, {a, b}, {0, 1, 0});
  CHECK(lib.load(ByteView(shared), false) == LoadStatus::kBadLayout);
  // Partial overlap (doc 1's range starts inside doc 0's).
  Bytes partial = good;
  uint32_t docs = rd32(partial, kHdrSections + 12 * kSecDocs);
  uint32_t off0 = rd32(partial, docs + kDocBodyOff), len0 = rd32(partial, docs + kDocBodyLen);
  REQUIRE(len0 > 2);
  wr32(partial, docs + kDocRecordSize + kDocBodyOff, off0 + len0 - 1);
  REQUIRE(repair_pack_checksums(partial));
  CHECK(lib.load(ByteView(partial), false) == LoadStatus::kBadLayout);
}

TEST_CASE("H6: expansion ratio and deflate block count are capped") {
  Bytes good = build_pack(water_guides(2));
  GuideLibrary lib;
  // 512 KiB of one byte compresses ~1000:1 — far above anything real text reaches (max 5.8:1).
  std::string rep(512u << 10, 'a');
  Body high = body_of(rep, false);
  REQUIRE(high.bytes / high.comp.size() > kMaxBodyExpansion);
  CHECK(lib.load(ByteView(doctor(good, {high, body_of("# B\n\nwater\n", false)}, {0, 1})), false) ==
        LoadStatus::kBadLayout);
  // Ordinary, compressible text well inside the cap still loads.
  std::string text;
  for (int i = 0; text.size() < 40000; ++i) text += "Boil water for one minute, line " + std::to_string(i) + ".\n";
  Body fine = body_of(text, false);
  Bytes okp = doctor(good, {fine, body_of("# B\n\nwater\n", false)}, {0, 1});
  REQUIRE(lib.load(ByteView(okp), false) == LoadStatus::kOk);
  CHECK(lib.verify());

  // Decompression-bomb case 2: one literal per fixed-Huffman block (18 bits per output byte).
  const size_t n = 200000;
  Body blocks;
  blocks.comp = many_fixed_blocks(n);
  blocks.bytes = static_cast<uint32_t>(n);
  blocks.crc = crc32_fast(ByteView(std::string(n, 'a')));
  CHECK(lib.load(ByteView(doctor(good, {blocks, body_of("# B\n\nwater\n", false)}, {0, 1})), false) ==
        LoadStatus::kBadLayout);
  // And the decoder itself refuses absurd block counts quickly.
  std::string out;
  auto t0 = std::chrono::steady_clock::now();
  CHECK_FALSE(inflate_raw(ByteView(blocks.comp), n, out));
  CHECK(seconds_since(t0) < 0.5);
  // A modest number of blocks (what zlib or our encoder produce) is still fine.
  Bytes few = many_fixed_blocks(16);
  CHECK(inflate_raw(ByteView(few), 16, out));
  CHECK(out == std::string(16, 'a'));
}

TEST_CASE("H6: inflate_raw_prefix decodes a bounded prefix") {
  std::string text;
  for (int i = 0; text.size() < 300000; ++i) text += "water line " + std::to_string(i) + " \xC3\xA9t\xC3\xA9\n";
  for (bool stored : {false, true}) {
    Bytes z = stored ? stored_stream(text) : deflate_raw(ByteView(text));
    std::string out;
    REQUIRE(inflate_raw_prefix(ByteView(z), text.size(), 1000, out));
    CHECK(out == text.substr(0, 1000));
    REQUIRE(inflate_raw_prefix(ByteView(z), text.size(), text.size() + 5, out));
    CHECK(out == text);
    // Input cut short: what was decoded so far is returned (prefix mode only).
    REQUIRE(inflate_raw_prefix(ByteView(z.data(), z.size() / 2), text.size(), 200000, out));
    CHECK(!out.empty());
    CHECK(out.size() <= 200000);
    CHECK(out == text.substr(0, out.size()));
    CHECK_FALSE(inflate_raw(ByteView(z.data(), z.size() / 2), text.size(), out));
  }
}

TEST_CASE("H6: search inflate work is bounded per query") {
  // 48 distinct (non-overlapping, ratio ~1) 1 MiB bodies that all match: legal under every load
  // check, so only the per-query budget keeps search fast.
  const int n = 48;
  std::vector<SourceGuide> gs = water_guides(n);
  std::vector<Body> bodies;
  std::vector<size_t> map;
  std::string text = "# Water\n\n";
  while (text.size() < (1u << 20)) text += "water a\n";  // many short lines: costly to convert
  for (int i = 0; i < n; ++i) {
    bodies.push_back(body_of(text, true));
    map.push_back(static_cast<size_t>(i));
  }
  Bytes pack = doctor(build_pack(gs), bodies, map);
  GuideLibrary lib;
  REQUIRE(lib.load(ByteView(pack), false) == LoadStatus::kOk);
  auto t0 = std::chrono::steady_clock::now();
  auto res = lib.search("water filter", "", 200);
  double t = seconds_since(t0);
  CHECK(res.size() == static_cast<size_t>(n));
  for (const SearchResult& r : res) CHECK(utf8_valid(r.snippet));
  CHECK(!res.empty());
  if (!res.empty()) CHECK(res[0].snippet.find("water") != std::string::npos);
  MESSAGE("search over 48 x 1 MiB bodies: " << t << " s");
  CHECK(t < 3.0);  // ~0.1 s at -O2; without the budget: 48 MiB inflated + converted per keystroke
  // get() still returns the whole body.
  Guide g;
  REQUIRE(lib.get("water/w-0000", g) == GetStatus::kOk);
  CHECK(g.markdown.size() == text.size());
}

TEST_CASE("pack load: id index must be a sorted permutation (fuzzer finding)") {
  Bytes good = build_pack(water_guides(4));
  GuideLibrary lib;
  REQUIRE(lib.load(ByteView(good), false) == LoadStatus::kOk);
  uint32_t idx = rd32(good, kHdrSections + 12 * kSecIdIndex);
  Bytes swapped = good;  // out of order: get() would miss guides that search returns
  uint32_t a = rd32(swapped, idx), b = rd32(swapped, idx + 4);
  wr32(swapped, idx, b);
  wr32(swapped, idx + 4, a);
  REQUIRE(repair_pack_checksums(swapped));
  CHECK(lib.load(ByteView(swapped), false) == LoadStatus::kBadLayout);
  Bytes dup = good;  // one doc listed twice, another never
  wr32(dup, idx + 4, rd32(dup, idx));
  REQUIRE(repair_pack_checksums(dup));
  CHECK(lib.load(ByteView(dup), false) == LoadStatus::kBadLayout);
}

TEST_CASE("H6: oversized metadata strings are rejected") {
  std::vector<SourceGuide> gs = water_guides(2);
  gs[1].meta.summary.assign(kMaxMetaBytes + 1, 'x');
  Bytes p = build_pack(gs);
  GuideLibrary lib;
  CHECK(lib.load(ByteView(p), false) == LoadStatus::kBadLayout);
  gs[1].meta.summary.assign(kMaxMetaBytes, 'x');
  Bytes q = build_pack(gs);
  CHECK(lib.load(ByteView(q), false) == LoadStatus::kOk);
}

}  // TEST_SUITE
