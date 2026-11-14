// Guides search ranking: lay synonyms / spelling variants, metadata coverage, title precision and
// a rerank pool independent of `limit`. Uses a small in-memory corpus shaped like the real one
// (the real-corpus evaluation lives in tools/guidepack: `guidepack search`).
#include <string>
#include <vector>

#include "doctest.h"
#include "fixture.h"
#include "guides/library.h"
#include "guides/pack_builder.h"

using namespace ember;
using namespace ember::guides;

namespace {

std::string doc(const std::string& id, const std::string& title, const std::string& tags, const std::string& summary,
                const std::string& body) {
  std::string cat = id.substr(0, id.find('/'));
  return "---\nid: " + id + "\ntitle: " + title + "\ncategory: " + cat + "\ntags: [" + tags + "]\nsummary: " + summary +
         "\nsource: original\nsource_title: Test\nlicense: CC0-1.0\nattribution: Test\nretrieved: 2026-01-01\n---\n# " +
         title + "\n\n" + body + "\n";
}

std::string repeat(const std::string& s, int n) {
  std::string o;
  for (int i = 0; i < n; ++i) o += s + " ";
  return o;
}

const GuideLibrary& rank_lib() {
  static Bytes pack;
  static GuideLibrary* l = [] {
    std::vector<SourceFile> files;
    auto add = [&](const std::string& id, const std::string& text) { files.push_back({id + ".md", text}); };
    // Long article that mentions babies and "not breathing" a lot, but is about drowning.
    add("medical/drowning",
        doc("medical/drowning", "Drowning", "drowning, near drowning, not breathing after water, cpr",
            "How drowning happens, rescue and resuscitation.",
            repeat("A drowning baby is not breathing after the water; give the baby breaths. Babies drown in baths.", 12) +
                repeat("Pull the person from the water safely and call for help.", 40)));
    // Short guide about infant CPR that never says "baby".
    add("medical/compressions",
        doc("medical/compressions", "First Aid: C for Compressions", "cpr, chest compressions, not breathing, infant cpr",
            "Chest compressions for adults, children and infants.",
            repeat("If the infant is not breathing, give 30 compressions with two fingers.", 3) +
                repeat("Push hard and fast in the centre of the chest.", 10)));
    add("medical/heat-stroke",
        doc("medical/heat-stroke", "Heat Stroke", "heat stroke, hyperthermia", "Signs and treatment of heat stroke.",
            repeat("Heat stroke: hot dry skin and confusion are signs of dangerous overheating.", 6)));
    add("medical/stroke",
        doc("medical/stroke", "Stroke", "stroke, brain attack, fast", "Recognising a stroke and its treatment.",
            repeat("Stroke signs: face drooping, arm weakness, speech difficulty; a stroke needs fast help.", 6) +
                repeat("Types of stroke: a clot or a bleed in the brain. Note the time the stroke started.", 10)));
    add("medical/diarrhea",
        doc("medical/diarrhea", "Diarrhea", "diarrhea, loose stools", "Treating diarrhea with oral rehydration.",
            repeat("Diarrhea causes dehydration; drink oral rehydration solution.", 5)));
    add("food/fridge-free",
        doc("food/fridge-free", "Keeping Food Without a Refrigerator", "food storage, root cellar, refrigerator",
            "Storing food safely when the refrigerator is gone.", repeat("Store food cool and dry; a refrigerator is not needed for grains.", 5)));
    for (int i = 0; i < 40; ++i) {  // filler that matches common words, so the rerank pool matters
      std::string id = "survival/filler-" + std::to_string(100 + i);
      add(id, doc(id, "Camp Notes " + std::to_string(i), "camp, notes", "Assorted camp notes.",
                  repeat("Water, food, the baby slept, breathing deep on the trail in the heat.", 4 + i % 5)));
    }
    std::vector<SourceGuide> ok;
    std::vector<Diagnostic> diags;
    validate_corpus(files, ok, diags);
    REQUIRE(ok.size() == files.size());
    pack = build_pack(ok);
    auto* x = new GuideLibrary();
    REQUIRE(x->load(ByteView(pack), false) == LoadStatus::kOk);
    return x;
  }();
  return *l;
}

std::string first(std::string_view q, uint32_t limit = 10) {
  auto r = rank_lib().search(q, "", limit);
  return r.empty() ? std::string() : r[0].id;
}

size_t pos(std::string_view q, const std::string& id) {
  auto r = rank_lib().search(q, "", 20);
  for (size_t i = 0; i < r.size(); ++i)
    if (r[i].id == id) return i;
  return 999;
}

}  // namespace

TEST_SUITE("guides") {

TEST_CASE("search ranking: lay words find the guide's vocabulary (baby -> infant)") {
  CHECK(first("baby not breathing") == "medical/compressions");
  CHECK(pos("baby not breathing", "medical/compressions") < pos("baby not breathing", "medical/drowning"));
  CHECK(first("infant cpr") == "medical/compressions");
  CHECK(first("store food without fridge") == "food/fridge-free");
  // spelling variants both ways
  CHECK(first("diarrhoea") == "medical/diarrhea");
  CHECK(first("Diarrhoea dehydration") == "medical/diarrhea");
  auto r = rank_lib().search("baby not breathing", "", 5);
  REQUIRE(!r.empty());
  bool infant_term = false;
  for (const auto& t : r[0].terms) infant_term |= t.find("infant") != std::string::npos;
  CHECK(infant_term);  // the synonym is highlighted in the snippet terms
}

TEST_CASE("search ranking: title precision and metadata coverage") {
  // ("stroke signs" -> Stroke over Heat Stroke is checked on the real corpus with guidepack; on a
  // corpus this small BM25 statistics differ too much for it to be meaningful.)
  CHECK(first("stroke treatment") == "medical/stroke");
  CHECK(first("heat stroke") == "medical/heat-stroke");
  CHECK(first("stroke") == "medical/stroke");
}

TEST_CASE("search ranking: order does not depend on limit") {
  for (const char* q : {"baby not breathing", "stroke signs", "water food", "camp notes", "baby"}) {
    CAPTURE(q);
    auto big = rank_lib().search(q, "", 20);
    for (uint32_t lim : {1u, 2u, 5u}) {
      auto small = rank_lib().search(q, "", lim);
      REQUIRE(small.size() <= big.size());
      for (size_t i = 0; i < small.size(); ++i) CHECK(small[i].id == big[i].id);
    }
  }
}

}  // TEST_SUITE
