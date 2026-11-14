// Semantic search: packs with and without the semantic sections, determinism, hybrid ranking
// invariants (exact-term regression guard), related/ask behaviour, guides.info, and damaged /
// truncated / hostile semantic sections (seeded mutations; the library must load keyword-only).
#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "doctest.h"
#include "fixture.h"
#include "guides/deflate.h"
#include "guides/library.h"
#include "guides/pack_builder.h"
#include "guides/pack_format.h"
#include "guides/text.h"

using namespace ember;
using namespace ember::guides;

namespace {

SemanticBuildOptions sem_opts(uint32_t bits = 4) {
  SemanticBuildOptions o;
  o.model = ByteView(guides_test::committed_model());
  o.vector_bits = bits;
  return o;
}

const Bytes& sem_pack() {
  static const Bytes p = build_pack(guides_test::fixture_guides(), sem_opts());
  return p;
}

const GuideLibrary& sem_lib() {
  static GuideLibrary lib;
  static bool once = [] { return lib.load(ByteView(sem_pack()), false) == LoadStatus::kOk; }();
  (void)once;
  return lib;
}

uint32_t le32(const Bytes& b, size_t o) {
  return static_cast<uint32_t>(b[o]) | static_cast<uint32_t>(b[o + 1]) << 8 | static_cast<uint32_t>(b[o + 2]) << 16 |
         static_cast<uint32_t>(b[o + 3]) << 24;
}
void put32(Bytes& b, size_t o, uint32_t v) { for (int i = 0; i < 4; ++i) b[o + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i)); }

// Recomputes the extension directory's entry CRCs, its own CRC and the header CRC, so that
// mutations reach the structural checks behind the checksums.
void repair_ext(Bytes& p) {
  if (p.size() < kHeaderSize) return;
  uint32_t dir = le32(p, kHdrExtDir);
  if (dir >= kHeaderSize && dir <= p.size() && p.size() - dir >= 16) {
    uint32_t count = std::min<uint32_t>(le32(p, dir + 8), kMaxExtSections);
    if (static_cast<uint64_t>(count) * kExtEntrySize <= p.size() - dir - 16) {
      for (uint32_t i = 0; i < count; ++i) {
        size_t e = dir + 16 + static_cast<size_t>(i) * kExtEntrySize;
        uint32_t off = le32(p, e + 8), len = le32(p, e + 12);
        if (off <= p.size() && len <= p.size() - off) put32(p, e + 16, crc32_fast(ByteView(p.data() + off, len)));
      }
      put32(p, dir + 12, crc32_fast(ByteView(p.data() + dir + 16, static_cast<size_t>(count) * kExtEntrySize)));
    }
  }
  put32(p, kHdrCrc, crc32_fast(ByteView(p.data(), kHdrCrc)));
}

// Exercise every semantic entry point; nothing may crash, every result must be well-formed.
// light = one query (used inside the mutation loops to keep the suite fast).
void exercise(const GuideLibrary& lib, bool light = false) {
  const char* all[] = {"stop bleeding", "make water safe to drink", "antenna", "Ångström 日本 [CLS]", ""};
  for (const char* q : all) {
    if (light && q != all[0]) break;
    for (SearchMode m : {SearchMode::kAuto, SearchMode::kSemantic, SearchMode::kHybrid}) {
      auto r = lib.search(q, "", 10, m);
      for (const SearchResult& x : r) {
        CHECK(utf8_valid(x.snippet));
        CHECK(utf8_valid(x.passage.text));
        CHECK(x.passage.text.size() <= kPassageChars);
      }
    }
    AskResult a = lib.ask(q, "", 5);
    for (const AskPassage& p : a.passages) {
      CHECK(utf8_valid(p.text));
      CHECK(p.text.size() <= kPassageChars);
      CHECK(p.score >= 0.0);
      CHECK(p.score <= 1.0);
    }
  }
  lib.related("medical/burns", 5);
  lib.related("nope/none", 5);
  lib.info();
  std::string err;
  lib.verify(&err);
}

}  // namespace

TEST_SUITE("guides") {

TEST_CASE("semantic: pack with the semantic section loads and reports itself") {
  REQUIRE_FALSE(guides_test::committed_model().empty());
  REQUIRE_FALSE(sem_pack().empty());
  const GuideLibrary& lib = sem_lib();
  REQUIRE(lib.loaded());
  REQUIRE(lib.semantic_available());
  LibraryInfo li = lib.info();
  CHECK(li.guides == guides_test::fixture_guides().size());
  CHECK(li.semantic.available);
  CHECK(li.semantic.status == "ok");
  CHECK(li.semantic.model.find("potion") != std::string::npos);
  CHECK(li.semantic.bits == 4);
  CHECK(li.semantic.vector_bits == 4);
  CHECK(li.semantic.passages > li.guides);  // one metadata passage per guide plus body passages
  uint32_t licensed = 0;
  for (auto& l : li.licences) licensed += l.second;
  CHECK(licensed == li.guides);
  std::string err;
  CHECK(lib.verify(&err));
  // Passage ranges stay inside their guides and passages are in guide order.
  const SemanticIndex& s = lib.semantic();
  CHECK(s.first(0) == 0);
  CHECK(s.first(li.guides) == s.passages());
  for (uint32_t d = 0; d < li.guides; ++d) {
    CHECK(s.first(d) < s.first(d + 1));
    CHECK(s.passage(s.first(d)).meta);
  }
}

TEST_CASE("semantic: keyword-only pack and old-reader compatibility") {
  std::vector<SourceGuide> g = guides_test::fixture_guides();
  Bytes plain = build_pack(g);
  Bytes plain2 = build_pack(g, SemanticBuildOptions());
  CHECK(plain == plain2);                              // no model: byte-identical to the old builder
  CHECK(le32(plain, kHdrExtDir) == 0);
  // The semantic pack is the keyword pack plus a trailer: same header fields (except the
  // extension offset and header CRC) and the same 8 sections, so older readers see the same pack.
  const Bytes& sp = sem_pack();
  REQUIRE(sp.size() > plain.size());
  CHECK(std::equal(plain.begin(), plain.begin() + kHdrExtDir, sp.begin()));
  CHECK(std::equal(plain.begin() + kHeaderSize, plain.end(), sp.begin() + kHeaderSize));
  GuideLibrary lib;
  REQUIRE(lib.load(ByteView(plain), false) == LoadStatus::kOk);
  CHECK_FALSE(lib.semantic_available());
  CHECK_FALSE(lib.info().semantic.available);
  CHECK(lib.info().semantic.status.find("no semantic") != std::string::npos);
  auto r = lib.search("bleeding", "", 5);
  REQUIRE_FALSE(r.empty());
  for (auto& x : r) { CHECK(x.match == "words"); CHECK_FALSE(x.has_passage); }
  CHECK(lib.related("medical/burns", 5).results.empty());
  AskResult a = lib.ask("how do i stop bleeding", "", 5);
  CHECK(a.passages.empty());
  CHECK(a.weak);
  // Keyword mode on a semantic pack returns exactly what the keyword pack returns.
  for (const char* q : {"bleeding", "water bleach", "j-pole", "frostbite toes", "snake"}) {
    auto k1 = lib.search(q, "", 10);
    auto k2 = sem_lib().search(q, "", 10, SearchMode::kKeyword);
    CHECK(guides_test::ids_of(k1) == guides_test::ids_of(k2));
  }
}

TEST_CASE("semantic: the packer is deterministic (4, 8 and 32-bit vectors)") {
  std::vector<SourceGuide> g = guides_test::fixture_guides();
  for (uint32_t bits : {4u, 8u, 32u}) {
    SemanticBuildStats s1, s2;
    Bytes a = build_pack(g, sem_opts(bits), &s1);
    Bytes b = build_pack(g, sem_opts(bits), &s2);
    REQUIRE_FALSE(a.empty());
    CHECK(a == b);
    CHECK(s1.passages == s2.passages);
    CHECK(s1.tokens > 0);
    GuideLibrary lib;
    REQUIRE(lib.load(ByteView(a), false) == LoadStatus::kOk);
    CHECK(lib.semantic_available());
    CHECK(lib.info().semantic.vector_bits == bits);
    exercise(lib);
  }
  // Integer-only embedding: the same bytes on every platform (compare this line across builds,
  // e.g. native vs wasm32).
  MESSAGE("fixture semantic pack: " << sem_pack().size() << " bytes, crc32 " << crc32_fast(ByteView(sem_pack())));
  std::string err;
  SemanticBuildOptions bad = sem_opts();
  Bytes junk(100, 7);
  bad.model = ByteView(junk);
  CHECK(build_pack(g, bad, nullptr, &err).empty());
  CHECK_FALSE(err.empty());
}

TEST_CASE("semantic: hybrid never ranks an exact-term match below keyword search") {
  const GuideLibrary& lib = sem_lib();
  // Every guide's own title, and distinctive terms from the fixture.
  std::vector<std::string> queries = {"j-pole", "bleach", "frostbite", "snake bites", "boiling", "handheld radio",
                                      "tourniquet", "burns", "146.52", "storage"};
  for (const GuideMeta& m : lib.list("", "", 0, 1000).guides) queries.push_back(m.title);
  for (const std::string& q : queries) {
    auto kw = lib.search(q, "", 10, SearchMode::kKeyword);
    auto hy = lib.search(q, "", 10, SearchMode::kHybrid);
    if (kw.empty()) continue;
    // The keyword winner stays in the hybrid top 3 and never drops more than one place; titles
    // stay first.
    auto it = std::find_if(hy.begin(), hy.end(), [&](const SearchResult& r) { return r.id == kw[0].id; });
    REQUIRE_MESSAGE(it != hy.end(), q);
    CHECK_MESSAGE(it - hy.begin() <= 1, q);
  }
  for (const GuideMeta& m : lib.list("", "", 0, 1000).guides) {
    auto hy = lib.search(m.title, "", 5, SearchMode::kHybrid);
    auto kw = lib.search(m.title, "", 5, SearchMode::kKeyword);
    if (!kw.empty() && kw[0].id == m.id) {
      REQUIRE_FALSE(hy.empty());
      CHECK_MESSAGE(hy[0].id == m.id, m.title);
    }
  }
}

TEST_CASE("semantic: meaning finds guides that share no words with the query") {
  const GuideLibrary& lib = sem_lib();
  struct Case { const char* q; const char* want; };
  const Case cases[] = {
      {"blood pouring out of a deep cut", "medical/bleeding-control"},
      {"fingers numb and white after the cold", "medical/frostbite"},
      {"scalded my arm with hot water", "medical/burns"},
  };
  for (const Case& c : cases) {
    auto hy = lib.search(c.q, "", 5, SearchMode::kHybrid);
    auto it = std::find_if(hy.begin(), hy.end(), [&](const SearchResult& r) { return r.id == c.want; });
    CHECK_MESSAGE(it != hy.end(), c.q);
    if (it != hy.end()) {
      CHECK(it->match != "");
      CHECK(it->has_passage);
      CHECK_FALSE(it->passage.text.empty());
    }
  }
  // Result fields.
  auto r = lib.search("bleeding", "", 10);
  REQUIRE_FALSE(r.empty());
  CHECK(r[0].id == "medical/bleeding-control");
  std::set<std::string> matches;
  for (auto& x : r) {
    matches.insert(x.match);
    CHECK((x.match == "words" || x.match == "meaning" || x.match == "both"));
  }
  // Category filter applies to meaning too.
  for (auto& x : lib.search("make water safe to drink", "water", 10)) CHECK(x.category == "water");
  for (auto& x : lib.search("make water safe to drink", "nonexistent", 10)) CHECK(false);
}

TEST_CASE("semantic: guides.related") {
  const GuideLibrary& lib = sem_lib();
  RelatedList r = lib.related("water/purification-boiling", 8);
  REQUIRE_FALSE(r.results.empty());
  CHECK(r.results.size() <= 8);
  for (size_t i = 0; i < r.results.size(); ++i) {
    CHECK(r.results[i].meta.id != "water/purification-boiling");
    CHECK(r.results[i].score >= 0.0);
    CHECK(r.results[i].score <= 1.0);
    if (i) CHECK(r.results[i].score <= r.results[i - 1].score);
  }
  CHECK(r.results[0].meta.category == "water");  // the closest guide to boiling water is about water
  CHECK(lib.related("no/such-guide", 8).results.empty());
  CHECK(lib.related("", 8).results.empty());
  CHECK(lib.related("water/purification-boiling", 1).results.size() == 1);
  CHECK(lib.related("water/purification-boiling", 1000).results.size() <= 50);
}

TEST_CASE("semantic: guides.ask returns quoted passages") {
  const GuideLibrary& lib = sem_lib();
  AskResult a = lib.ask("how much bleach do I put in drinking water", "", 5);
  REQUIRE_FALSE(a.passages.empty());
  CHECK(a.passages.size() <= 5);
  CHECK(a.passages[0].id == "water/purification-bleach");
  CHECK_FALSE(a.weak);
  std::map<std::string, int> per;
  for (const AskPassage& p : a.passages) {
    CHECK(++per[p.id] <= 2);
    CHECK_FALSE(p.text.empty());
    CHECK(p.text.size() <= kPassageChars);
    // Extractive: the passage's words come from the guide body.
    Guide g;
    REQUIRE(lib.get(p.id, g) == GetStatus::kOk);
    std::string plain = markdown_to_plain(g.markdown);
    std::string first_words = p.text.substr(0, std::min<size_t>(p.text.find(' ', 20), 40));
    std::string flat;
    bool sp = false;
    for (char ch : plain) {
      if (ch == ' ' || ch == '\n' || ch == '\t') { sp = !flat.empty(); continue; }
      if (sp) { flat.push_back(' '); sp = false; }
      flat.push_back(ch);
    }
    CHECK_MESSAGE(flat.find(first_words) != std::string::npos, p.text);
  }
  for (const AskPassage& p : lib.ask("stop the bleeding", "medical", 5).passages) CHECK(p.category == "medical");
  CHECK(lib.ask("", "", 5).passages.empty());
  CHECK(lib.ask("", "", 5).weak);
  CHECK(lib.ask("bleeding", "nope", 5).passages.empty());
  CHECK(lib.ask("bleeding", "", 100).passages.size() <= 20);
  // Off-topic questions are flagged weak.
  CHECK(lib.ask("celebrity gossip and football transfer news", "", 3).weak);
}

TEST_CASE("semantic: damaged semantic sections degrade to keyword-only, never fail the load") {
  const Bytes& good = sem_pack();
  const uint32_t dir = le32(good, kHdrExtDir);
  REQUIRE(dir > kHeaderSize);
  auto check_keyword_ok = [](const Bytes& b) {
    GuideLibrary lib;
    REQUIRE(lib.load(ByteView(b), false) == LoadStatus::kOk);
    CHECK_FALSE(lib.search("bleeding", "", 5).empty());
    exercise(lib, true);
    return lib.semantic_available();
  };
  {  // truncated in the middle of the semantic sections
    for (size_t cut : {static_cast<size_t>(dir) + 3, static_cast<size_t>(dir) + 40, (good.size() + dir) / 2, good.size() - 1}) {
      Bytes b(good.begin(), good.begin() + static_cast<std::ptrdiff_t>(cut));
      CHECK_FALSE(check_keyword_ok(b));
    }
  }
  {  // checksum mismatch in the directory or the passage table (not repaired)
    Bytes b = good;
    b[dir + 20] ^= 1;
    CHECK_FALSE(check_keyword_ok(b));
  }
  {  // hostile directory offsets (header CRC repaired)
    for (uint32_t v : {1u, 175u, 176u, 0xFFFFFFF0u, 0xFFFFFFFFu, static_cast<uint32_t>(good.size()),
                       static_cast<uint32_t>(good.size() - 8)}) {
      Bytes b = good;
      put32(b, kHdrExtDir, v);
      put32(b, kHdrCrc, crc32_fast(ByteView(b.data(), kHdrCrc)));
      CHECK_FALSE(check_keyword_ok(b));
    }
  }
  {  // hostile entry fields with every checksum repaired
    const uint32_t count = le32(good, dir + 8);
    for (uint32_t i = 0; i < count; ++i)
      for (size_t f : {4u, 8u, 12u})
        for (uint32_t v : {0u, 0xFFFFFFFFu, static_cast<uint32_t>(good.size())}) {
          Bytes b = good;
          put32(b, dir + 16 + i * kExtEntrySize + f, v);
          repair_ext(b);
          check_keyword_ok(b);
        }
    for (uint32_t v : {0u, 17u, 0xFFFFFFFFu}) {
      Bytes b = good;
      put32(b, dir + 8, v);
      repair_ext(b);
      CHECK_FALSE(check_keyword_ok(b));
    }
  }
  {  // hostile counts inside the passage table and vector headers
    const uint32_t count = le32(good, dir + 8);
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t off = le32(good, dir + 16 + i * kExtEntrySize + 8);
      for (size_t f : {0u, 4u, 8u, 16u})
        for (uint32_t v : {0u, 0x10000u, 0xFFFFFFFFu}) {
          Bytes b = good;
          if (off + f + 4 > b.size()) continue;
          put32(b, off + f, v);
          repair_ext(b);
          check_keyword_ok(b);
        }
    }
  }
}

TEST_CASE("semantic: seeded random corruption of the semantic sections (fuzz)") {
  const Bytes& good = sem_pack();
  const uint32_t dir = le32(good, kHdrExtDir);
  const size_t span = good.size() - dir;
  uint64_t s = 0xC0FFEE123456789ull;
  auto rnd = [&]() { s = s * 6364136223846793005ull + 1442695040888963407ull; return s >> 17; };
  int loaded_semantic = 0;
  for (int trial = 0; trial < 60; ++trial) {
    Bytes b = good;
    int flips = 1 + static_cast<int>(rnd() % 8);
    for (int k = 0; k < flips; ++k) {
      size_t pos = dir + rnd() % span;
      b[pos] = static_cast<uint8_t>(rnd());
    }
    if (trial % 2 == 0) repair_ext(b);  // half the time get past the checksums
    GuideLibrary lib;
    REQUIRE(lib.load(ByteView(b), false) == LoadStatus::kOk);
    CHECK_FALSE(lib.search("water", "", 5, SearchMode::kKeyword).empty());
    loaded_semantic += lib.semantic_available() ? 1 : 0;
    exercise(lib, true);
  }
  MESSAGE("semantic sections still usable after " << loaded_semantic << " of 60 random corruptions");
}

TEST_CASE("semantic: 32-bit safety of the format limits") {
  // Every size computed from pack fields is bounded far below 2^32 by the load-time limits.
  CHECK(static_cast<uint64_t>(kMaxPassages) * kSemPassageRecord < (1ull << 31));
  CHECK(static_cast<uint64_t>(kMaxPassages) * (1024ull * 4 + 4) < (1ull << 33));  // checked in u64 at load
  CHECK(static_cast<uint64_t>(kMaxExtSections) * kExtEntrySize < 4096);
  CHECK(sizeof(uint32_t) == 4);
}

}  // TEST_SUITE
