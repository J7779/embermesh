// Passage splitting for semantic search (guides/passages.h).
#include <string>
#include <vector>

#include "doctest.h"
#include "fixture.h"
#include "guides/passages.h"

using namespace ember::guides;

namespace {
std::string text_of(const std::string& md, const PassageSpan& s) { return md.substr(s.off, s.len); }
std::string head_of(const std::string& md, const PassageSpan& s) { return md.substr(s.head_off, s.head_len); }
std::string para(const std::string& word, int sentences) {
  std::string o;
  for (int i = 0; i < sentences; ++i) o += "The " + word + " sentence number " + std::to_string(i) + " is here. ";
  return o;
}
}  // namespace

TEST_SUITE("guides") {

TEST_CASE("passages: sections, headings and packing") {
  std::string md = "# Title\n\nIntro paragraph about the guide and what it covers in detail.\n\n## Signs\n\n"
                   "Pale skin, sweating and confusion are early signs.\n\n- shaking\n- thirst\n\n## Treatment\n\n"
                   "Move the person to shade and give water slowly.\n";
  auto ps = split_passages(md);
  REQUIRE(ps.size() == 3);
  CHECK(text_of(md, ps[0]).find("Intro paragraph") == 0);
  CHECK(ps[0].head_len == 0);  // under the level-1 title: no heading
  CHECK(head_of(md, ps[1]) == "Signs");
  CHECK(text_of(md, ps[1]).find("- thirst") != std::string::npos);  // blocks of one section packed together
  CHECK(head_of(md, ps[2]) == "Treatment");
  for (auto& p : ps) CHECK(p.off + p.len <= md.size());
}

TEST_CASE("passages: size cap, sentence cuts and overlap") {
  PassageOptions o;
  o.max_bytes = 300;
  o.min_bytes = 80;
  o.overlap_bytes = 120;
  std::string md = "## Long\n\n" + para("alpha", 40) + "\n";
  auto ps = split_passages(md, o);
  REQUIRE(ps.size() >= 5);
  for (size_t i = 0; i < ps.size(); ++i) {
    CHECK(ps[i].len <= o.max_bytes + o.min_bytes);
    std::string t = text_of(md, ps[i]);
    CHECK(t.substr(0, 4) == "The ");     // cut at sentence starts
    if (i + 1 < ps.size()) {
      CHECK(t.back() == '.');            // and ends
      CHECK(ps[i + 1].off < ps[i].off + ps[i].len);  // one-sentence overlap with the next piece
    }
  }
  // A very long "sentence" without punctuation is cut at spaces; a word longer than the cap at a
  // code point boundary.
  std::string words(2000, 'x');
  for (size_t i = 5; i < words.size(); i += 9) words[i] = ' ';
  for (auto& p : split_passages("## W\n\n" + words + "\n", o)) CHECK(p.len <= o.max_bytes);
  std::string utf(1500, 'a');
  std::string md2 = "## U\n\n";
  for (int i = 0; i < 500; ++i) md2 += "\xC3\xA9";  // é x 500, no spaces
  for (auto& p : split_passages(md2, o)) {
    CHECK(p.len <= o.max_bytes);
    CHECK((static_cast<unsigned char>(md2[p.off]) & 0xC0) != 0x80);
  }
}

TEST_CASE("passages: boilerplate is skipped") {
  std::string md = "# T\n\nUseful text about water treatment with enough words.\n\n"
                   "_(Later sections of the original article were left out of this offline copy for length.)_\n\n"
                   "Source: Some Agency, 2020. Public domain.\n\n"
                   "## References\n\nRef one, a long citation that should never become a passage at all.\n\n"
                   "### Sub of references\n\nStill references, skipped as well entirely here.\n\n"
                   "## Storage\n\nKeep treated water in clean closed containers away from sunlight.\n";
  auto ps = split_passages(md);
  REQUIRE(ps.size() == 2);
  CHECK(text_of(md, ps[0]).find("Useful text") == 0);
  CHECK(text_of(md, ps[0]).find("Later sections") == std::string::npos);
  CHECK(text_of(md, ps[0]).find("Source:") == std::string::npos);
  CHECK(head_of(md, ps[1]) == "Storage");
}

TEST_CASE("passages: degenerate input") {
  CHECK(split_passages("").empty());
  CHECK(split_passages("\n\n\n").empty());
  CHECK(split_passages("# Only a title\n").empty());
  CHECK(split_passages("tiny").empty());  // under 24 bytes of text
  std::string fence = "## Code\n\n```\nline one of code that is long enough\n\nline two after a blank\n```\n";
  auto ps = split_passages(fence);
  REQUIRE(ps.size() == 1);
  CHECK(text_of(fence, ps[0]).find("line two") != std::string::npos);  // a code block is one block
  PassageOptions silly;
  silly.max_bytes = 1;
  silly.min_bytes = 1000;
  for (auto& p : split_passages(para("beta", 30), silly)) CHECK(p.len <= 64 + 32);
  // Every fixture guide splits into in-range passages.
  for (auto& g : guides_test::fixture_guides())
    for (auto& p : split_passages(g.markdown)) {
      CHECK(p.off + p.len <= g.markdown.size());
      CHECK(p.head_off + p.head_len <= g.markdown.size());
    }
}

}  // TEST_SUITE
