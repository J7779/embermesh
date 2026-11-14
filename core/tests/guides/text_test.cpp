#include <string>
#include <vector>

#include "doctest.h"
#include "guides/text.h"

using namespace ember::guides;

namespace {
std::vector<std::string> words(std::string_view s) {
  std::vector<Token> t;
  tokenize(s, t);
  std::vector<std::string> out;
  for (const Token& x : t) out.push_back(x.text);
  return out;
}
using V = std::vector<std::string>;
}  // namespace

TEST_SUITE("guides") {

TEST_CASE("utf8 validation") {
  CHECK(utf8_valid(""));
  CHECK(utf8_valid("plain ascii"));
  CHECK(utf8_valid("caf\xC3\xA9 \xE6\xB0\xB4 \xF0\x9F\x94\xA5"));
  size_t bad = 99;
  CHECK_FALSE(utf8_valid("ab\xC3", &bad));       // truncated
  CHECK(bad == 2);
  CHECK_FALSE(utf8_valid("\xC0\xAF"));           // overlong
  CHECK_FALSE(utf8_valid("\xED\xA0\x80"));       // surrogate
  CHECK_FALSE(utf8_valid("\xF4\x90\x80\x80"));   // > U+10FFFF
  CHECK_FALSE(utf8_valid("\x80"));               // lone continuation
  CHECK_FALSE(utf8_valid("\xFF"));
  std::string s = "a\xC3\xA9";
  CHECK(utf8_floor(s, 2) == 1);
  CHECK(utf8_floor(s, 3) == 3);
  CHECK(utf8_floor(s, 99) == 3);
  CHECK(utf8_lower("CAF\xC3\x89 \xD0\x92\xD0\x9E\xD0\x94\xD0\x90 \xCE\xA9") == "caf\xC3\xA9 \xD0\xB2\xD0\xBE\xD0\xB4\xD0\xB0 \xCF\x89");
  CHECK(utf8_valid(utf8_lower("bad\xFF\xC3")));
}

TEST_CASE("tokenizer basics") {
  CHECK(words("Hello, World!") == V{"hello", "world"});
  CHECK(words("") == V{});
  CHECK(words("   \n\t ") == V{});
  CHECK(words("a b c") == V{});                      // single letters dropped
  CHECK(words("2 m") == V{"2"});                     // single digits kept
  CHECK(words("won't stop") == V{"wont", "stop"});
  CHECK(words("people's water") == V{"people", "water"});
  CHECK(words("it\xE2\x80\x99s fine") == V{"it", "fine"});  // curly apostrophe + possessive-like 's
  CHECK(words("146.52 MHz") == V{"146.52", "mhz"});
  CHECK(words("1,000 litres") == V{"1000", "litres"});
  CHECK(words("end. Next") == V{"end", "next"});
  CHECK(words("2m band") == V{"2m", "band"});
}

TEST_CASE("tokenizer compounds and offsets") {
  std::vector<Token> t;
  tokenize("a J-Pole here", t);
  REQUIRE(t.size() == 3);
  CHECK(t[0].text == "pole");
  CHECK_FALSE(t[0].joined);
  CHECK(t[1].text == "jpole");
  CHECK(t[1].joined);
  CHECK(t[1].begin == 2);
  CHECK(t[1].end == 8);
  CHECK(t[2].text == "here");
  CHECK(words("first-aid kit") == V{"first", "aid", "firstaid", "kit"});
  CHECK(words("x - y") == V{});
  CHECK(words("well-") == V{"well"});
}

TEST_CASE("tokenizer unicode") {
  CHECK(words("Caf\xC3\xA9 \xC3\x91" "and\xC3\xBA") == V{"cafe", "nandu"});
  CHECK(words("\xC3\x9C" "berleben stra\xC3\x9F" "e") == V{"uberleben", "strasse"});
  CHECK(words("cafe\xCC\x81") == V{"cafe"});  // decomposed accent
  CHECK(words("\xD0\x92\xD0\xBE\xD0\xB4\xD0\xB0") == V{"\xD0\xB2\xD0\xBE\xD0\xB4\xD0\xB0"});  // Вода -> вода
  CHECK(words("\xE6\xB0\xB4\xE3\x81\xA8\xE7\x81\xAB") == V{"\xE6\xB0\xB4", "\xE3\x81\xA8", "\xE7\x81\xAB"});
  CHECK(words("fire\xF0\x9F\x94\xA5water") == V{"fire", "water"});  // emoji separates
  CHECK(words("\xEF\xBC\xA8\xEF\xBC\xA5\xEF\xBC\xAC\xEF\xBC\xB0") == V{"help"});  // fullwidth
  CHECK(words("bad\xFF\xFEutf") == V{"bad", "utf"});
  CHECK(words("\xE2\x80\x9Cquoted\xE2\x80\x9D") == V{"quoted"});
}

TEST_CASE("tokenizer limits") {
  std::string longword(200, 'x');
  CHECK(words(longword) == V{});
  CHECK(words("ok " + longword + " fine") == V{"ok", "fine"});
  std::vector<Token> t;
  std::string many;
  for (int i = 0; i < 1000; ++i) many += "word ";
  tokenize(many, t, 10);
  CHECK(t.size() == 10);
  // Garbage never crashes and offsets stay in range.
  std::string g;
  uint32_t x = 12345;
  for (int i = 0; i < 4096; ++i) { x = x * 1103515245u + 12345u; g.push_back(static_cast<char>(x >> 24)); }
  t.clear();
  tokenize(g, t);
  for (const Token& k : t) {
    CHECK(k.begin <= k.end);
    CHECK(k.end <= g.size());
    CHECK(utf8_valid(k.text));
  }
}

TEST_CASE("stop words") {
  CHECK(is_stop_word("the"));
  CHECK(is_stop_word("wont"));
  CHECK(is_stop_word("dont"));
  CHECK_FALSE(is_stop_word("water"));
  CHECK_FALSE(is_stop_word("well"));   // a water well
  CHECK_FALSE(is_stop_word("can"));    // food can
  CHECK_FALSE(is_stop_word("stop"));
  CHECK(index_term("the") == "");
  CHECK(index_term("bleeding") == "bleed");
}

TEST_CASE("stemmer") {
  CHECK(stem("bleeding") == "bleed");
  CHECK(stem("bleeds") == "bleed");
  CHECK(stem("burns") == "burn");
  CHECK(stem("burned") == "burn");
  CHECK(stem("stopping") == "stop");
  CHECK(stem("purify") == "purifi");
  CHECK(stem("purified") == "purifi");
  CHECK(stem("purifies") == "purifi");
  CHECK(stem("purifying") == "purifi");
  CHECK(stem("caresses") == "caress");
  CHECK(stem("ponies") == "poni");
  CHECK(stem("agreed") == "agree");
  CHECK(stem("hopping") == "hop");
  CHECK(stem("filing") == "file");
  CHECK(stem("falling") == "fall");
  CHECK(stem("conflated") == "conflate");
  CHECK(stem("sized") == "size");
  CHECK(stem("happy") == "happi");
  CHECK(stem("gas") == "gas");          // <= 3 letters untouched
  CHECK(stem("bus") == "bus");
  CHECK(stem("frostbite") == "frostbite");
  CHECK(stem("2m") == "2m");            // non-letters untouched
  CHECK(stem("caf\xC3\xA9") == "caf\xC3\xA9");
  CHECK(stem("") == "");
  // Derivational endings.
  CHECK(stem("purification") == stem("purify"));
  CHECK(stem("purifications") == stem("purified"));
  CHECK(stem("sterilization") == stem("sterilized"));
  CHECK(stem("dehydration") == stem("dehydrated"));
  CHECK(stem("disinfection") == stem("disinfect"));
  CHECK(stem("infection") == "infect");
  CHECK(stem("station") == "station");
  CHECK(stem("nation") == "nation");
  CHECK(stem("section") == "section");
}

TEST_CASE("markdown to plain text") {
  std::string md =
      "# Heading One\n"
      "Some *bold* and _em_ text with `code` and a [link](https://x.org/a) here.\n"
      "- item one\n"
      "12. numbered\n"
      "> quoted\n"
      "| a | b |\n"
      "|---|:-:|\n"
      "---\n"
      "```\n"
      "raw *code* block\n"
      "```\n"
      "see https://example.com/page now\n"
      "snake_case stays\n";
  std::string p = markdown_to_plain(md);
  CHECK(p.find("Heading One\n") == 0);
  CHECK(p.find("Some bold and em text with code and a link here.") != std::string::npos);
  CHECK(p.find("item one") != std::string::npos);
  CHECK(p.find("12.") == std::string::npos);
  CHECK(p.find("numbered") != std::string::npos);
  CHECK(p.find(">") == std::string::npos);
  CHECK(p.find("---") == std::string::npos);
  CHECK(p.find("|") == std::string::npos);
  CHECK(p.find("raw *code* block") != std::string::npos);  // code kept verbatim
  CHECK(p.find("example.com") == std::string::npos);
  CHECK(p.find("x.org") == std::string::npos);
  CHECK(p.find("snake_case") != std::string::npos);
  CHECK(markdown_to_plain("") == "\n");
  CHECK(markdown_to_plain("[unterminated](http://x") == "unterminated\n");
}

}  // TEST_SUITE
