#include <string>

#include "doctest.h"
#include "guides/frontmatter.h"

using namespace ember::guides;

TEST_SUITE("guides") {

TEST_CASE("front matter: basic scalars, comments and lists") {
  std::string t =
      "---\n"
      "id: water/boil          # unique\n"
      "title: Purifying Water: Boiling\n"
      "tags: [water, \"a, b\", 'it''s']\n"
      "source: https://example.gov/page#anchor\n"
      "empty:\n"
      "---\n"
      "# Body\n";
  FrontMatter fm = parse_front_matter(t);
  CHECK(fm.present);
  CHECK(fm.terminated);
  CHECK(fm.errors.empty());
  CHECK(fm.get("id") == "water/boil");
  CHECK(fm.get("title") == "Purifying Water: Boiling");
  REQUIRE(fm.find("tags"));
  CHECK(fm.find("tags")->is_list);
  CHECK(fm.find("tags")->list == std::vector<std::string>{"water", "a, b", "it's"});
  CHECK(fm.get("source") == "https://example.gov/page#anchor");  // '#' not preceded by space
  CHECK(fm.find("empty"));
  CHECK(fm.get("empty") == "");
  CHECK(fm.find("id")->line == 2);
  CHECK(t.substr(fm.body_offset) == "# Body\n");
  CHECK(fm.body_line == 8);
}

TEST_CASE("front matter: CRLF and BOM") {
  std::string t = "\xEF\xBB\xBF---\r\nid: a/b\r\ntitle: \"Quoted \\\"title\\\" \\u00e9\"\r\n---\r\nBody\r\n";
  FrontMatter fm = parse_front_matter(t);
  CHECK(fm.present);
  CHECK(fm.terminated);
  CHECK(fm.errors.empty());
  CHECK(fm.get("id") == "a/b");
  CHECK(fm.get("title") == "Quoted \"title\" \xC3\xA9");
  CHECK(t.substr(fm.body_offset) == "Body\r\n");
}

TEST_CASE("front matter: block lists, folded and continued scalars") {
  std::string t =
      "---\n"
      "tags:\n"
      "  - one\n"
      "  - \"two, three\"\n"
      "  -\n"
      "summary: >\n"
      "  First line\n"
      "  second line.\n"
      "attribution: A long\n"
      "  continued value\n"
      "notes: |\n"
      "  line1\n"
      "  line2\n"
      "...\n"
      "body";
  FrontMatter fm = parse_front_matter(t);
  CHECK(fm.terminated);
  CHECK(fm.errors.empty());
  CHECK(fm.find("tags")->list == std::vector<std::string>{"one", "two, three"});
  CHECK(fm.get("summary") == "First line second line.");
  CHECK(fm.get("attribution") == "A long continued value");
  CHECK(fm.get("notes") == "line1\nline2");
  CHECK(t.substr(fm.body_offset) == "body");
}

TEST_CASE("front matter: missing and unterminated") {
  FrontMatter none = parse_front_matter("# Just markdown\n");
  CHECK_FALSE(none.present);
  CHECK(none.body_offset == 0);
  FrontMatter empty = parse_front_matter("");
  CHECK_FALSE(empty.present);

  FrontMatter un = parse_front_matter("---\nid: x\ntitle: y\n");
  CHECK(un.present);
  CHECK_FALSE(un.terminated);
  REQUIRE_FALSE(un.errors.empty());
  CHECK(un.errors[0].message.find("unterminated") != std::string::npos);
  CHECK(un.get("id") == "x");  // still parsed for diagnostics

  FrontMatter only = parse_front_matter("---");
  CHECK(only.present);
  CHECK_FALSE(only.terminated);

  FrontMatter imm = parse_front_matter("---\n---\n");
  CHECK(imm.terminated);
  CHECK(imm.values.empty());
}

TEST_CASE("front matter: errors carry line numbers") {
  std::string t =
      "---\n"
      "id: a\n"
      "id: b\n"                    // 3 duplicate
      "no colon here\n"            // 4
      "tags: [a, b\n"              // 5 unterminated list
      "title: \"open\n"            // 6 unterminated quote
      "bad: \"x\\q\"\n"            // 7 bad escape
      "  stray indented\n"         // 8
      "url:http://nospace\n"       // 9 key must be followed by space
      "---\n";
  FrontMatter fm = parse_front_matter(t);
  CHECK(fm.terminated);
  CHECK(fm.get("id") == "a");  // first kept
  std::vector<int> lines;
  for (const FmDiag& d : fm.errors) lines.push_back(d.line);
  CHECK(lines == std::vector<int>{3, 4, 5, 6, 7, 8, 9});
  CHECK(fm.errors[0].message.find("duplicate") != std::string::npos);
}

TEST_CASE("front matter: hostile input does not crash") {
  uint32_t x = 7;
  const char alphabet[] = "-:[]\"'#\n\r -abc|>\\u";
  for (int round = 0; round < 300; ++round) {
    std::string t = "---\n";
    int len = round % 97;
    for (int i = 0; i < len; ++i) { x = x * 1664525u + 1013904223u; t.push_back(alphabet[(x >> 24) % (sizeof alphabet - 1)]); }
    if (round & 1) t += "\n---\n";
    FrontMatter fm = parse_front_matter(t);
    CHECK(fm.body_offset <= t.size());
  }
}

}  // TEST_SUITE
