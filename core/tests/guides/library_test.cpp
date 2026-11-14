#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include "doctest.h"
#include "fixture.h"
#include "guides/library.h"
#include "guides/pack_builder.h"
#include "guides/text.h"

using namespace ember;
using namespace ember::guides;
using guides_test::fixture_pack;
using guides_test::ids_of;

namespace {
const GuideLibrary& lib() {
  static GuideLibrary* l = [] {
    auto* x = new GuideLibrary();
    REQUIRE(x->load(ByteView(fixture_pack()), false) == LoadStatus::kOk);
    return x;
  }();
  return *l;
}
std::string top(std::string_view q, std::string_view cat = "") {
  auto r = lib().search(q, cat, 5);
  return r.empty() ? std::string() : r[0].id;
}
bool contains(const std::vector<std::string>& v, const std::string& s) {
  return std::find(v.begin(), v.end(), s) != v.end();
}
}  // namespace

TEST_SUITE("guides") {

TEST_CASE("pack round trip and determinism") {
  auto guides = guides_test::fixture_guides();
  Bytes a = build_pack(guides);
  std::reverse(guides.begin(), guides.end());  // input order must not matter
  Bytes b = build_pack(guides);
  CHECK(a == b);
  CHECK(a == fixture_pack());
  GuideLibrary copy;
  {
    Bytes tmp = a;
    REQUIRE(copy.load(ByteView(tmp), true) == LoadStatus::kOk);
    std::fill(tmp.begin(), tmp.end(), 0);  // copy=true must not reference the caller's buffer
  }
  CHECK(copy.total() == 13);
  CHECK(copy.verify());
  Guide g;
  REQUIRE(copy.get("water/purification-boiling", g) == GetStatus::kOk);
  CHECK(g.markdown.find("rolling boil") != std::string::npos);
  CHECK(lib().verify());
  PackStats s = lib().stats();
  CHECK(s.docs == 13);
  CHECK(s.terms > 100);
}

TEST_CASE("categories with counts and titles in canonical order") {
  auto cats = lib().categories();
  REQUIRE(cats.size() == 5);
  CHECK(cats[0].id == "medical");
  CHECK(cats[0].title == "Medical & First Aid");
  CHECK(cats[0].count == 4);
  CHECK(cats[1].id == "hygiene");
  CHECK(cats[2].id == "water");
  CHECK(cats[2].count == 5);
  CHECK(cats[3].id == "radio");
  CHECK(cats[4].id == "reference");
  uint32_t sum = 0;
  for (auto& c : cats) sum += c.count;
  CHECK(sum == lib().total());
}

TEST_CASE("list ordering, filters and pagination") {
  GuideList all = lib().list("", "", 0, 100);
  CHECK(all.total == 13);
  CHECK(all.guides.size() == 13);
  GuideList water = lib().list("water", "", 0, 100);
  REQUIRE(water.total == 5);
  // Book "field manual" sorts as a group by its name, chapters by order; then titles.
  std::vector<std::string> ids;
  for (auto& m : water.guides) ids.push_back(m.id);
  CHECK(ids == std::vector<std::string>{"water/purification-bleach", "water/field-manual/sources",
                                        "water/field-manual/treatment", "water/field-manual/storage",
                                        "water/purification-boiling"});
  GuideList page = lib().list("water", "", 1, 2);
  CHECK(page.total == 5);
  REQUIRE(page.guides.size() == 2);
  CHECK(page.guides[0].id == "water/field-manual/sources");
  CHECK(lib().list("water", "", 5, 10).guides.empty());
  CHECK(lib().list("water", "", 99, 10).total == 5);
  CHECK(lib().list("water", "", 0, 0).guides.empty());
  GuideList book = lib().list("", "water/field-manual", 0, 100);
  CHECK(book.total == 3);
  CHECK(lib().list("medical", "water/field-manual", 0, 100).total == 0);
  CHECK(lib().list("nope", "", 0, 100).total == 0);
  const GuideMeta& m = water.guides[2];
  CHECK(m.title == "Water Field Manual: Treatment");
  CHECK(m.part_of == "water/field-manual");
  CHECK(m.has_order);
  CHECK(m.order == 2);
  CHECK(m.license == "CC0-1.0");
  CHECK(m.source == "original");
  CHECK(m.source_title == "EmberMesh test fixtures");
  CHECK(m.attribution == "EmberMesh contributors");
  CHECK(m.tags == std::vector<std::string>{"water", "treatment"});
  CHECK(m.bytes > 0);
  CHECK_FALSE(water.guides[0].has_order);
  CHECK(water.guides[0].part_of.empty());
}

TEST_CASE("get with prev/next") {
  Guide g;
  REQUIRE(lib().get("water/field-manual/treatment", g) == GetStatus::kOk);
  CHECK(g.prev == "water/field-manual/sources");
  CHECK(g.next == "water/field-manual/storage");
  CHECK(g.markdown.rfind("# Treatment", 0) == 0);
  CHECK(g.meta.bytes == g.markdown.size());
  REQUIRE(lib().get("water/field-manual/sources", g) == GetStatus::kOk);
  CHECK(g.prev.empty());
  CHECK(g.next == "water/field-manual/treatment");
  REQUIRE(lib().get("water/field-manual/storage", g) == GetStatus::kOk);
  CHECK(g.next.empty());
  REQUIRE(lib().get("medical/frostbite", g) == GetStatus::kOk);
  CHECK(g.prev.empty());
  CHECK(g.next.empty());
  CHECK(g.markdown.find("front matter") == std::string::npos);
  CHECK(g.markdown.find("---\nid:") == std::string::npos);
  CHECK(lib().get("medical/nope", g) == GetStatus::kNotFound);
  CHECK(lib().get("", g) == GetStatus::kNotFound);
  CHECK(lib().get("water/_sources", g) == GetStatus::kNotFound);
  CHECK(lib().get("_drafts/broken", g) == GetStatus::kNotFound);
  GuideMeta m;
  CHECK(lib().meta("radio/j-pole-antenna", m));
  CHECK(m.category == "radio");
  // Every listed guide can be opened and is valid UTF-8.
  for (const GuideMeta& x : lib().list("", "", 0, 100).guides) {
    REQUIRE(lib().get(x.id, g) == GetStatus::kOk);
    CHECK(utf8_valid(g.markdown));
  }
}

TEST_CASE("empty and unloaded libraries") {
  GuideLibrary none;
  CHECK_FALSE(none.loaded());
  CHECK(none.total() == 0);
  CHECK(none.categories().empty());
  CHECK(none.list("", "", 0, 10).total == 0);
  CHECK(none.search("water", "", 10).empty());
  Guide g;
  CHECK(none.get("water/x", g) == GetStatus::kNotFound);
  CHECK_FALSE(none.verify());

  Bytes empty = build_pack({});
  GuideLibrary e;
  REQUIRE(e.load(ByteView(empty), false) == LoadStatus::kOk);
  CHECK(e.loaded());
  CHECK(e.total() == 0);
  CHECK(e.categories().empty());
  CHECK(e.list("", "", 0, 10).guides.empty());
  CHECK(e.search("water", "", 10).empty());
  CHECK(e.search("w", "", 10).empty());
  CHECK(e.get("a/b", g) == GetStatus::kNotFound);
  CHECK(e.verify());
  // Reload replaces; clear empties.
  REQUIRE(e.load(ByteView(fixture_pack()), false) == LoadStatus::kOk);
  CHECK(e.total() == 13);
  CHECK(e.load(ByteView(), false) == LoadStatus::kTooSmall);
  CHECK(e.total() == 0);
  CHECK_FALSE(e.loaded());
}

TEST_CASE("search: stressed real-world queries") {
  CHECK(top("bleeding wont stop") == "medical/bleeding-control");
  CHECK(top("bleeding won't stop") == "medical/bleeding-control");
  CHECK(top("BLEEDING") == "medical/bleeding-control");
  CHECK(top("purify water bleach") == "water/purification-bleach");
  CHECK(top("snake bite") == "medical/snake-bites");
  CHECK(top("snakebite") == "");  // not in corpus as one word; must not crash
  CHECK(top("snake bites") == "medical/snake-bites");
  CHECK(top("hand washing") == "hygiene/handwashing");  // adjacent words joined: "handwashing"
  CHECK(top("j-pole 2m") == "radio/j-pole-antenna");
  CHECK(top("jpole") == "radio/j-pole-antenna");
  CHECK(top("J pole antenna") == "radio/j-pole-antenna");
  CHECK(top("frostbite") == "medical/frostbite");
  CHECK(top("boiling water") == "water/purification-boiling");
  CHECK(top("how do i wash hands") == "hygiene/handwashing");
  CHECK(top("tourniquet") == "medical/bleeding-control");
  CHECK(top("146.52") != "");
}

TEST_CASE("search: prefix (search-as-you-type) and typos") {
  CHECK(top("frostb") == "medical/frostbite");
  CHECK(top("tourn") == "medical/bleeding-control");
  CHECK(top("snake bi") == "medical/snake-bites");
  CHECK(top("purif") != "");
  // A trailing space means the last word is complete: no prefix expansion.
  CHECK(lib().search("tourn ", "", 5).empty());
  // Typo tolerance.
  CHECK(top("frosbite") == "medical/frostbite");
  CHECK(top("hypotermia") == "medical/frostbite");
  CHECK(top("tourniqet") == "medical/bleeding-control");
  CHECK(top("bleach") == "water/purification-bleach");
  CHECK(top("bleech") == "water/purification-bleach");
}

TEST_CASE("search: ranking prefers title matches and all terms") {
  auto r = lib().search("water", "", 20);
  REQUIRE(r.size() >= 5);
  CHECK(r[0].category == "water");
  for (size_t i = 1; i < r.size(); ++i) CHECK(r[i - 1].score >= r[i].score);
  // "bleeding" appears in burns too, but the bleeding guide must win.
  r = lib().search("bleeding", "", 20);
  REQUIRE(r.size() >= 2);
  CHECK(r[0].id == "medical/bleeding-control");
  CHECK(contains(ids_of(r), "medical/burns"));
  // AND preference: a guide with both terms beats guides with only one.
  r = lib().search("water bleach", "", 20);
  REQUIRE(!r.empty());
  CHECK(r[0].id == "water/purification-bleach");
  CHECK(r[0].score > 2 * r[1].score);
}

TEST_CASE("search: category filter and limits") {
  auto r = lib().search("water", "water", 20);
  REQUIRE(!r.empty());
  for (auto& x : r) CHECK(x.category == "water");
  CHECK(lib().search("bleeding", "water", 20).empty());
  CHECK(lib().search("water", "nope", 20).empty());
  CHECK(lib().search("water", "", 2).size() == 2);
  CHECK(lib().search("water", "", 0).size() == 1);  // clamped to >= 1
  auto big = lib().search("water", "", 100000);
  CHECK(big.size() <= 13);
}

TEST_CASE("search: snippets and matched terms") {
  auto r = lib().search("purify water bleach", "", 3);
  REQUIRE(!r.empty());
  const SearchResult& x = r[0];
  CHECK(x.title == "Disinfecting Water with Household Bleach");
  CHECK(x.snippet.find("bleach") != std::string::npos);
  CHECK(x.snippet.find("purify") != std::string::npos);
  CHECK(x.snippet.size() < 400);
  CHECK(x.snippet.find('\n') == std::string::npos);
  CHECK(x.snippet.find("**") == std::string::npos);
  CHECK(utf8_valid(x.snippet));
  CHECK(contains(x.terms, "bleach"));
  CHECK(contains(x.terms, "purify"));
  CHECK(contains(x.terms, "water"));
  // Snippet does not just repeat the H1 title.
  r = lib().search("tourniquet", "", 1);
  REQUIRE(!r.empty());
  CHECK(r[0].snippet.find("tourniquet") != std::string::npos);
  CHECK(r[0].snippet.rfind("\xE2\x80\xA6", 0) == 0);  // starts with an ellipsis (mid-text)
  // Non-ASCII snippets are valid UTF-8 and terms are lowercased.
  r = lib().search("\xD0\x92\xD0\x9E\xD0\x94\xD0\x90", "", 3);  // "ВОДА"
  REQUIRE(r.size() == 1);
  CHECK(r[0].id == "reference/unicode-sample");
  CHECK(utf8_valid(r[0].snippet));
  CHECK(contains(r[0].terms, "\xD0\xB2\xD0\xBE\xD0\xB4\xD0\xB0"));
  CHECK(top("cafe") == "reference/unicode-sample");
  CHECK(top("caf\xC3\xA9") == "reference/unicode-sample");
  CHECK(top("UBERLEBEN") == "reference/unicode-sample");
  CHECK(top("\xE6\xB0\xB4") == "reference/unicode-sample");
  CHECK(top("multi word") == "reference/unicode-sample");  // tag with a comma inside quotes
}

TEST_CASE("search: empty, garbage and very long queries") {
  CHECK(lib().search("", "", 10).empty());
  CHECK(lib().search("   ", "", 10).empty());
  CHECK(lib().search("the and of", "", 10).empty());  // stop words only
  CHECK(lib().search("!!!???", "", 10).empty());
  CHECK(lib().search("zzqxj", "", 10).empty());
  CHECK(lib().search("\xFF\xFE\xC3", "", 10).empty());
  std::string nul("water\0bleach", 12);
  CHECK(top(nul) == "water/purification-bleach");
  std::string long_q;
  for (int i = 0; i < 5000; ++i) long_q += "water ";
  auto t0 = std::chrono::steady_clock::now();
  auto r = lib().search(long_q, "", 10);
  CHECK(!r.empty());
  std::string mixed;
  for (int i = 0; i < 300; ++i) mixed += "w" + std::to_string(i) + " x-y-z-q-r-s-t-u-v-w ";
  lib().search(mixed, "", 10);
  std::string garbage;
  uint32_t s = 5;
  for (int i = 0; i < 20000; ++i) { s = s * 1103515245u + 12345u; garbage.push_back(static_cast<char>(s >> 24)); }
  lib().search(garbage, "", 10);
  for (size_t n = 1; n < 40; ++n) lib().search(garbage.substr(n * 13, n), "", 5);
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  CHECK(ms < 2000);  // generous: sanitizer builds are slow
  // Single characters (prefix of everything) are fine.
  lib().search("w", "", 10);
  lib().search("2", "", 10);
}

}  // TEST_SUITE
