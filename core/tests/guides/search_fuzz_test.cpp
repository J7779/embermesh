// Seeded mutation fuzzing of guides.pack with a full query workload (search incl. prefix / typo /
// category paths, get, list) after EVERY successful load, under a per-iteration work bound
// (security review H6 follow-up: "search can never hold the node for seconds").
//
//   fast (default suite):   ./ember_tests -ts=guides -tc="pack mutation fuzz*"
//   long (run by hand):     ./ember_tests -ts=guides -tc="pack mutation soak*" --no-skip
//     The long version also fuzzes the real corpus pack if one exists at
//     $EMBER_GUIDES_PACK or <repo>/build/guides.pack.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
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

struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed ? seed : 1) {}
  uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return static_cast<uint32_t>(s >> 11); }
  uint32_t below(uint32_t n) { return n ? next() % n : 0; }
};

uint32_t rd32(const Bytes& b, size_t o) {
  return static_cast<uint32_t>(b[o]) | static_cast<uint32_t>(b[o + 1]) << 8 | static_cast<uint32_t>(b[o + 2]) << 16 |
         static_cast<uint32_t>(b[o + 3]) << 24;
}
void wr32(Bytes& b, size_t o, uint32_t v) {
  for (int i = 0; i < 4; ++i) b[o + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i));
}

const uint32_t kInteresting[] = {0, 1, 2, 31, 32, 33, 127, 128, 255, 256, 4095, 4096, 65535, 65536,
                                 kMaxMetaBytes, kMaxMetaBytes + 1, kMaxGuideBytes, kMaxGuideBytes + 1,
                                 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu};

// One mutation; `repair` says whether checksums should be fixed up afterwards.
void mutate(Bytes& b, Rng& r, bool* repair) {
  *repair = r.below(4) != 0;  // mostly behind valid checksums: exercises validation, not the CRC
  if (b.size() < kHeaderSize) return;
  uint32_t sec = r.below(kSectionCount);
  size_t off = rd32(b, kHdrSections + 12 * sec), len = rd32(b, kHdrSections + 12 * sec + 4);
  if (off > b.size() || len > b.size() - off) { off = kHeaderSize; len = b.size() - kHeaderSize; }
  switch (r.below(10)) {
    case 0: {  // bit flips anywhere
      int n = 1 + static_cast<int>(r.below(8));
      for (int i = 0; i < n; ++i) b[r.below(static_cast<uint32_t>(b.size()))] ^= static_cast<uint8_t>(1u << r.below(8));
      break;
    }
    case 1:  // interesting u32 inside a section (offsets, lengths, counts, doc numbers)
      if (len >= 4) wr32(b, off + r.below(static_cast<uint32_t>(len - 3)), kInteresting[r.below(sizeof kInteresting / 4)]);
      break;
    case 2:  // header counts / section table entries
      wr32(b, kHdrDocCount + 4 * r.below(3), r.below(2) ? r.below(2000) : kInteresting[r.below(sizeof kInteresting / 4)]);
      break;
    case 3:  // section length change
      wr32(b, kHdrSections + 12 * sec + 4, static_cast<uint32_t>(r.below(static_cast<uint32_t>(len + 64))));
      break;
    case 4: {  // copy one doc record's body fields onto another (shared / overlapping bodies)
      uint32_t n = rd32(b, kHdrDocCount);
      size_t docs = rd32(b, kHdrSections + 12 * kSecDocs);
      if (n < 2 || docs > b.size() || static_cast<uint64_t>(n) * kDocRecordSize > b.size() - docs) break;
      size_t a = docs + r.below(n) * kDocRecordSize, c = docs + r.below(n) * kDocRecordSize;
      std::memcpy(&b[c + kDocBytes], &b[a + kDocBytes], 16);
      break;
    }
    case 5: {  // claim a bigger uncompressed size for a body
      uint32_t n = rd32(b, kHdrDocCount);
      size_t docs = rd32(b, kHdrSections + 12 * kSecDocs);
      if (!n || docs > b.size() || static_cast<uint64_t>(n) * kDocRecordSize > b.size() - docs) break;
      size_t rec = docs + r.below(n) * kDocRecordSize;
      uint32_t bytes = rd32(b, rec + kDocBytes);
      wr32(b, rec + kDocBytes, r.below(2) ? bytes * (2 + r.below(100)) : bytes + r.below(64));
      break;
    }
    case 6: {  // varint garbage in TERMS / POSTINGS (long continuation runs, overlong encodings)
      uint32_t ts = r.below(2) ? kSecTerms : kSecPostings;
      size_t to = rd32(b, kHdrSections + 12 * ts), tl = rd32(b, kHdrSections + 12 * ts + 4);
      if (!tl || to > b.size() || tl > b.size() - to) break;
      size_t p = to + r.below(static_cast<uint32_t>(tl));
      size_t n = std::min<size_t>(to + tl - p, 1 + r.below(16));
      for (size_t i = 0; i < n; ++i) b[p + i] = static_cast<uint8_t>(r.below(2) ? 0x80 | r.next() : 0xFF);
      break;
    }
    case 7: {  // point term blocks at each other (unsorted / repeated dictionary)
      size_t tb = rd32(b, kHdrSections + 12 * kSecTermBlocks), tl = rd32(b, kHdrSections + 12 * kSecTermBlocks + 4);
      if (tl < 16 || tb > b.size() || tl > b.size() - tb) break;
      uint32_t nb = static_cast<uint32_t>(tl / 8);
      size_t from = tb + 8 * r.below(nb);
      for (int k = 0; k < 1 + static_cast<int>(r.below(8)); ++k) std::memcpy(&b[tb + 8 * r.below(nb)], &b[from], 8);
      break;
    }
    case 8: {  // splice: duplicate a chunk of the file over another place
      size_t n = 1 + r.below(64);
      if (b.size() <= n + kHeaderSize) break;
      size_t src = r.below(static_cast<uint32_t>(b.size() - n)), dst = kHeaderSize + r.below(static_cast<uint32_t>(b.size() - n - kHeaderSize));
      std::memmove(&b[dst], &b[src], n);
      break;
    }
    default:  // truncate or extend
      if (r.below(2)) b.resize(kHeaderSize + r.below(static_cast<uint32_t>(b.size() - kHeaderSize)));
      else b.insert(b.end(), r.below(300), static_cast<uint8_t>(r.next()));
      break;
  }
}

const char* const kQueries[] = {"water",       "boil water",      "bleeding wont stop", "j-pole 2m", "frosbite",
                                "w",           "tourn",           "zzzz",               "snake bite", "a",
                                "radio antena", "the of and",     "hypothermia signs treatment child baby kid",
                                "\xC3\xA9t\xC3\xA9", "fire starter ", "x-y-z-w-v-u-t-s"};

struct Workload {
  double worst = 0;
  uint64_t searches = 0, loaded = 0;
};

// Exercises the read paths a node serves; returns seconds taken.
double exercise(const GuideLibrary& lib, Rng& r, Workload& w) {
  auto t0 = std::chrono::steady_clock::now();
  std::vector<CategoryInfo> cats = lib.categories();
  for (int q = 0; q < 4; ++q) {
    const char* query = kQueries[r.below(sizeof kQueries / sizeof kQueries[0])];
    std::string cat = (!cats.empty() && r.below(3) == 0) ? cats[r.below(static_cast<uint32_t>(cats.size()))].id : "";
    uint32_t limit = r.below(2) ? 200 : 1 + r.below(20);
    std::vector<SearchResult> res = lib.search(query, cat, limit);
    ++w.searches;
    CHECK(res.size() <= limit);
    for (const SearchResult& s : res) {
      CHECK(utf8_valid(s.snippet));
      CHECK(utf8_valid(s.title));
      CHECK(s.snippet.size() < 1024);
      GuideMeta m;
      CHECK(lib.meta(s.id, m));
    }
  }
  GuideList l = lib.list("", "", 0, 10000);
  Guide g;
  for (size_t i = 0; i < l.guides.size() && i < 3; ++i) {
    const GuideMeta& m = l.guides[r.below(static_cast<uint32_t>(l.guides.size()))];
    if (lib.get(m.id, g) == GetStatus::kOk) CHECK(utf8_valid(g.markdown));
  }
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

void campaign(const Bytes& base, uint64_t seed, int iterations, double bound_s, const char* name) {
  GuideLibrary lib;
  Rng r(seed);
  Workload w;
  // Baseline: the unmutated pack.
  REQUIRE(lib.load(ByteView(base), false) == LoadStatus::kOk);
  double base_t = exercise(lib, r, w);
  for (int it = 0; it < iterations; ++it) {
    Bytes b = base;
    bool repair = false;
    int n = 1 + static_cast<int>(r.below(3));
    for (int k = 0; k < n; ++k) mutate(b, r, &repair);
    if (repair) repair_pack_checksums(b);
    auto t0 = std::chrono::steady_clock::now();
    LoadStatus st = lib.load(ByteView(b), r.below(4) == 0);
    double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (st == LoadStatus::kOk) {
      ++w.loaded;
      t += exercise(lib, r, w);
    } else {
      CHECK(lib.search("water", "", 10).empty());
    }
    if (t > w.worst) w.worst = t;
    if (t > bound_s) {
      FAIL_CHECK(std::string(name) << ": iteration " << it << " took " << t << " s (load " << load_status_name(st) << ")");
      break;
    }
  }
  MESSAGE(std::string(name) << ": " << iterations << " mutations, " << w.loaded << " loaded, " << w.searches
               << " searches, baseline " << base_t << " s, worst iteration " << w.worst << " s");
}

bool read_file(const std::string& path, Bytes& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return !out.empty();
}

}  // namespace

TEST_SUITE("guides") {

TEST_CASE("pack mutation fuzz: search after every load, bounded work") {
  campaign(guides_test::fixture_pack(), 0x5EED0001, 1500, 2.0, "fixture");
}

TEST_CASE("pack mutation soak: fixture and real corpus" * doctest::skip()) {
  campaign(guides_test::fixture_pack(), 0x5EED0002, 100000, 2.0, "fixture-long");
  Bytes real;
  const char* env = std::getenv("EMBER_GUIDES_PACK");
  std::string path = env ? env : std::string(EMBER_REPO_ROOT) + "/build/guides.pack";
  if (read_file(path, real)) campaign(real, 0x5EED0003, 400, 10.0, "real-pack");
  else MESSAGE("no real pack at " << path << "; skipped that part");
}

}  // TEST_SUITE
