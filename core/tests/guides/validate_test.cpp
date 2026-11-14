#include <string>
#include <vector>

#include "doctest.h"
#include "fixture.h"
#include "guides/pack_builder.h"

using namespace ember::guides;
using guides_test::guide_text;

namespace {
struct Result {
  bool ok;
  std::vector<Diagnostic> diags;
  SourceGuide guide;
  bool has(const std::string& needle, bool error = true) const {
    for (const Diagnostic& d : diags)
      if (d.error == error && d.message.find(needle) != std::string::npos) return true;
    return false;
  }
  int line_of(const std::string& needle) const {
    for (const Diagnostic& d : diags)
      if (d.message.find(needle) != std::string::npos) return d.line;
    return -1;
  }
};
Result check(const std::string& path, const std::string& content) {
  Result r;
  r.ok = parse_guide_file(SourceFile{path, content}, r.guide, r.diags);
  return r;
}
}  // namespace

TEST_SUITE("guides") {

TEST_CASE("licence allow-list") {
  for (const char* ok : {"Public Domain (US Government)", "public domain", "CC0-1.0", "CC0", "CC BY 4.0",
                         "CC-BY-3.0", "CC BY-SA 4.0", "CC BY-SA 3.0 (Wikipedia)", "Creative Commons Attribution 4.0",
                         "Creative Commons Attribution-ShareAlike 4.0 International", "Open Government Licence v3.0",
                         "MIT", "GFDL 1.3 / CC BY-SA 3.0", "Unlicense"})
    CHECK_MESSAGE(license_allowed(ok), ok);
  for (const char* bad : {"", "CC BY-NC 4.0", "CC BY-NC-SA 4.0", "CC BY-ND 4.0", "CC-BY-NC-ND",
                          "Creative Commons Attribution-NonCommercial 4.0", "Attribution-NoDerivatives",
                          "All rights reserved", "Copyright 2020 Foo, all rights reserved", "proprietary",
                          "fair use", "unknown", "Some Custom License", "non-commercial use only"})
    CHECK_MESSAGE(!license_allowed(bad), bad);
  std::string why;
  CHECK_FALSE(license_allowed("CC BY-NC 4.0", &why));
  CHECK(why.find("NC") != std::string::npos);
}

TEST_CASE("valid guide parses into SourceGuide") {
  Result r = check("water/boil.md", guide_text("water/boil", "water", "part_of: water/book\norder: 2\n",
                                               "\r\n\r\n# Boil\r\n\r\nText.\r\n"));
  CHECK(r.ok);
  CHECK(r.diags.empty());
  CHECK(r.guide.meta.id == "water/boil");
  CHECK(r.guide.meta.tags == std::vector<std::string>{"a", "b"});
  CHECK(r.guide.meta.has_order);
  CHECK(r.guide.meta.order == 2);
  CHECK(r.guide.meta.part_of == "water/book");
  CHECK(r.guide.markdown == "# Boil\n\nText.\n");  // CRLF normalised, leading blank lines dropped
}

TEST_CASE("metadata longer than the reader's limit is an error") {
  std::string t = guide_text("water/boil", "water");
  size_t p = t.find("attribution: Test");
  REQUIRE(p != std::string::npos);
  t.replace(p, 17, "attribution: " + std::string(9000, 'x'));
  Result r = check("water/boil.md", t);
  CHECK_FALSE(r.ok);
  CHECK(r.has("'attribution' is longer than"));
  t = guide_text("water/boil", "water");
  t.replace(t.find("attribution: Test"), 17, "attribution: " + std::string(8000, 'x'));
  r = check("water/boil.md", t);
  CHECK(r.ok);
}

TEST_CASE("required fields and consistency checks") {
  Result r = check("water/boil.md", "---\nid: water/boil\ntitle: T\n---\nBody\n");
  CHECK_FALSE(r.ok);
  for (const char* f : {"'category'", "'summary'", "'source'", "'license'", "'attribution'"}) CHECK_MESSAGE(r.has(f), f);
  CHECK(r.has("tags", false));
  CHECK(r.has("source_title", false));

  r = check("water/boil.md", guide_text("water/other", "water"));
  CHECK_FALSE(r.ok);
  CHECK(r.has("must equal the path"));
  CHECK(r.line_of("must equal the path") == 2);

  r = check("water/boil.md", guide_text("water/boil", "food"));
  CHECK_FALSE(r.ok);
  CHECK(r.has("must equal the directory"));

  r = check("boil.md", guide_text("boil", "water"));
  CHECK_FALSE(r.ok);
  CHECK(r.has("category directory"));

  r = check("water/boil.txt", guide_text("water/boil", "water"));
  CHECK_FALSE(r.ok);

  r = check("newcat/x.md", guide_text("newcat/x", "newcat"));
  CHECK(r.ok);
  CHECK(r.has("unknown category", false));

  r = check("water/x.md", guide_text("water/x", "water", "order: two\n"));
  CHECK_FALSE(r.ok);
  CHECK(r.has("order must be an integer"));

  r = check("water/x.md", guide_text("water/x", "water", "title: Again\n"));
  CHECK_FALSE(r.ok);
  CHECK(r.has("duplicate key"));

  r = check("water/x.md", guide_text("water/x", "water", "summary2: x\n"));
  CHECK(r.has("unknown front-matter key", false));

  r = check("water/x.md", guide_text("water/x", "water", "", "# Title\n", "CC BY-NC-SA 4.0"));
  CHECK_FALSE(r.ok);
  CHECK(r.has("rejected"));
}

TEST_CASE("body checks: html, images, empty, size, utf8") {
  Result r = check("water/x.md", guide_text("water/x", "water", "", "# T\n\nText <br> more\n<div class=a>x</div>\n"));
  CHECK_FALSE(r.ok);
  CHECK(r.has("raw HTML"));
  CHECK(r.line_of("raw HTML") == 15);  // front matter is 12 lines, then "# T", blank, text

  r = check("water/x.md", guide_text("water/x", "water", "", "# T\n\nUse `<div>` in code, a < b > c, <5 minutes.\n```\n<html>\n```\n"));
  CHECK(r.ok);

  r = check("water/x.md", guide_text("water/x", "water", "", "# T\n\n![diagram](img.png)\n"));
  CHECK_FALSE(r.ok);
  CHECK(r.has("images are not allowed"));

  r = check("water/x.md", guide_text("water/x", "water", "", "\n  \n"));
  CHECK_FALSE(r.ok);
  CHECK(r.has("empty body"));

  r = check("water/x.md", guide_text("water/x", "water", "", "# T\n\n" + std::string(70000, 'a') + "\n"));
  CHECK_FALSE(r.ok);
  CHECK(r.has("limit is"));

  r = check("water/x.md", guide_text("water/x", "water", "", "# T\n\n" + std::string(61000, 'a') + "\n"));
  CHECK(r.ok);
  CHECK(r.has("keep guides under", false));

  r = check("water/x.md", guide_text("water/x", "water", "", "# T\n\nbad \xC3( utf8\n"));
  CHECK_FALSE(r.ok);
  CHECK(r.has("invalid UTF-8"));
  CHECK(r.line_of("invalid UTF-8") == 15);

  r = check("water/x.md", "# no front matter\n");
  CHECK_FALSE(r.ok);
  CHECK(r.has("missing front matter"));

  r = check("water/x.md", "---\nid: water/x\n");
  CHECK_FALSE(r.ok);
  CHECK(r.has("unterminated"));
}

TEST_CASE("corpus validation: ignored paths and duplicate ids") {
  CHECK(path_ignored("_sources.md"));
  CHECK(path_ignored("water/_sources.md"));
  CHECK(path_ignored("_drafts/x.md"));
  CHECK(path_ignored("water/.hidden.md"));
  CHECK_FALSE(path_ignored("water/a_b.md"));
  std::vector<SourceFile> files = {
      {"water/a.md", guide_text("water/a", "water")},
      {"water/A.md", guide_text("water/A", "water")},  // same id case-insensitively
      {"water/_sources.md", "garbage"},
      {"_drafts/x.md", "garbage"},
      {"water/pic.png", "\x89PNG"},
      {"water/b.md", guide_text("water/b", "water", "part_of: water/book\norder: 1\n")},
      {"water/c.md", guide_text("water/c", "water", "part_of: water/book\norder: 1\n")},
  };
  std::vector<SourceGuide> ok;
  std::vector<Diagnostic> diags;
  size_t errors = validate_corpus(files, ok, diags);
  CHECK(errors == 1);
  CHECK(ok.size() == 3);
  bool dup = false, png = false, order = false;
  for (const Diagnostic& d : diags) {
    dup = dup || d.message.find("duplicate id") != std::string::npos;
    png = png || (d.file == "water/pic.png" && !d.error);
    order = order || (d.message.find("same part_of/order") != std::string::npos && !d.error);
    CHECK(d.file.find('_') == std::string::npos);
  }
  CHECK(dup);
  CHECK(png);
  CHECK(order);
}

TEST_CASE("fixture corpus is valid") {
  size_t errors = 99;
  auto g = guides_test::fixture_guides(&errors);
  CHECK(errors == 0);
  CHECK(g.size() == 13);
}

}  // TEST_SUITE
