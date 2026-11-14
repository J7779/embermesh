#include <cmath>
#include <string>

#include "base/json.h"
#include "doctest.h"

using namespace ember;

TEST_SUITE("base") {

TEST_CASE("json: build and dump") {
  Json j = Json::object();
  j.set("name", "Ann").set("age", 41).set("ok", true).set("none", nullptr);
  j.set("tags", Json::array({"a", 2, 3.5}));
  j.at_key("nested").set("x", -1);
  CHECK(j.dump() == R"({"name":"Ann","age":41,"ok":true,"none":null,"tags":["a",2,3.5],"nested":{"x":-1}})");
  j.set("age", 42);  // replace keeps position
  CHECK(j.members()[1].first == "age");
  CHECK(j.get_int("age") == 42);
  CHECK(j.erase("none"));
  CHECK_FALSE(j.erase("none"));
  CHECK(j.size() == 5);
  Json o = Json::object({{"a", 1}, {"b", "x"}, {"a", 2}});
  CHECK(o.dump() == R"({"a":2,"b":"x"})");
  CHECK(Json::array().dump() == "[]");
  CHECK(Json::object().dump() == "{}");
  CHECK(Json().dump() == "null");
  CHECK(Json(static_cast<const char*>(nullptr)).is_null());
  CHECK(Json(uint64_t(UINT64_MAX)).is_double());
  CHECK(Json(uint64_t(5)).is_int());
  CHECK(Json(uint8_t(7)).as_int() == 7);
}

TEST_CASE("json: pretty print") {
  Json j = Json::object({{"a", Json::array({1, 2})}, {"b", Json::object()}});
  CHECK(j.dump(2) == "{\n  \"a\": [\n    1,\n    2\n  ],\n  \"b\": {}\n}");
  auto p = Json::parse(j.dump(2));
  REQUIRE(p);
  CHECK(*p == j);
}

TEST_CASE("json: typed getters with defaults") {
  auto j = Json::parse(R"({"s":"str","i":7,"d":2.5,"di":3.0,"b":true,"n":null,"a":[1,2,3]})");
  REQUIRE(j);
  CHECK(j->get_string("s") == "str");
  CHECK(j->get_string("i", "dflt") == "dflt");
  CHECK(j->get_string("missing", "dflt") == "dflt");
  CHECK(j->get_int("i") == 7);
  CHECK(j->get_int("d", -1) == -1);  // non-integral double
  CHECK(j->get_int("di") == 3);
  CHECK(j->get_int("s", 9) == 9);
  CHECK(j->get_double("i") == 7.0);
  CHECK(j->get_double("d") == 2.5);
  CHECK(j->get_bool("b") == true);
  CHECK(j->get_bool("i", false) == false);
  CHECK(j->has("n"));
  CHECK_FALSE(j->has("zz"));
  CHECK(j->get("n").is_null());
  CHECK(j->get("zz").is_null());
  int64_t sum = 0;
  for (const Json& v : j->get("a").items()) sum += v.as_int();
  CHECK(sum == 6);
  CHECK(j->get("a")[1].as_int() == 2);
  CHECK(j->get("a")[99].is_null());
  CHECK(j->get("s")[0].is_null());
  CHECK(j->get("s").items().empty());
  CHECK((*j)["s"].as_string() == "str");
  CHECK((*j)["s"]["deep"]["deeper"].is_null());
  std::string s;
  CHECK(j->get_string_if("s", &s));
  CHECK_FALSE(j->get_string_if("i", &s));
  int64_t i;
  CHECK(j->get_int_if("i", &i));
  CHECK_FALSE(j->get_int_if("d", &i));
  bool b;
  CHECK(j->get_bool_if("b", &b));
  CHECK_FALSE(j->get_bool_if("n", &b));
  double d;
  CHECK(j->get_double_if("i", &d));
  CHECK_FALSE(j->get_double_if("s", &d));
}

TEST_CASE("json: numbers") {
  auto p = [](const char* t) { return Json::parse(t); };
  CHECK(p("0")->as_int() == 0);
  CHECK(p("-0")->is_int());
  CHECK(p("9223372036854775807")->as_int() == INT64_MAX);
  CHECK(p("-9223372036854775808")->as_int() == INT64_MIN);
  CHECK(p("9223372036854775808")->is_double());
  CHECK(p("123456789012345678901234567890")->is_double());
  CHECK(p("1e2")->as_double() == 100.0);
  CHECK(p("1.5E-3")->as_double() == doctest::Approx(0.0015));
  CHECK(p("-12.25")->as_double() == -12.25);
  CHECK_FALSE(p("1e400"));   // out of range
  CHECK_FALSE(p("-1e400"));
  CHECK_FALSE(p("01"));
  CHECK_FALSE(p("1."));
  CHECK_FALSE(p(".5"));
  CHECK_FALSE(p("+1"));
  CHECK_FALSE(p("1e"));
  CHECK_FALSE(p("1e+"));
  CHECK_FALSE(p("-"));
  CHECK_FALSE(p("NaN"));
  CHECK_FALSE(p("Infinity"));
  CHECK_FALSE(p("0x10"));
  CHECK(Json(0.1).dump() == "0.1");
  CHECK(Json(1.0).dump() == "1.0");
  CHECK(Json(1e300).dump() == "1e+300");
  CHECK(Json(std::nan("")).dump() == "null");
  CHECK(Json(INFINITY).dump() == "null");
  // Doubles round trip exactly.
  for (double d : {0.1, 1.0 / 3.0, 123456.789e-30, -2.5e17, 5e-324}) {
    auto r = Json::parse(Json(d).dump());
    REQUIRE(r);
    CHECK(r->as_double() == d);
  }
  CHECK(Json(INT64_MIN).dump() == "-9223372036854775808");
}

TEST_CASE("json: strings and escapes") {
  auto j = Json::parse(R"("a\"b\\c\/d\b\f\n\r\t\u0041\u00e9\u20ac\ud83d\ude00")");
  REQUIRE(j);
  CHECK(j->as_string() == "a\"b\\c/d\b\f\n\r\tA\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80");
  // Embedded NUL survives a round trip.
  auto z = Json::parse(R"("x\u0000y")");
  REQUIRE(z);
  CHECK(z->as_string().size() == 3);
  CHECK(z->as_string()[1] == '\0');
  CHECK(z->dump() == R"("x\u0000y")");
  std::string with_nul("a\0b", 3);
  CHECK(Json(with_nul).dump() == R"("a\u0000b")");
  // Serializer escapes controls, DEL, U+2028/2029 and replaces invalid UTF-8.
  CHECK(Json("\x01\x7f").dump() == R"("\u0001\u007f")");
  CHECK(Json("\xE2\x80\xA8").dump() == R"("\u2028")");
  CHECK(Json("a\xFF" "b").dump() == "\"a\xEF\xBF\xBD" "b\"");
  CHECK(Json("\xC3").dump() == "\"\xEF\xBF\xBD\"");
  // Output of the serializer always re-parses.
  std::string garbage;
  for (int i = 0; i < 256; ++i) garbage.push_back(static_cast<char>(i));
  auto g = Json::parse(Json(garbage).dump());
  REQUIRE(g);
  CHECK(utf8_valid(g->as_string()));
}

TEST_CASE("json: strict rejection") {
  const char* bad[] = {
      "", " ", "{", "}", "[", "]", "[1,]", "[,1]", "{\"a\":1,}", "{\"a\"}", "{\"a\" 1}", "{a:1}",
      "{'a':1}", "[1 2]", "\"abc", "\"\\x\"", "\"\\u12\"", "\"\\u12G4\"", "\"\\ud800\"",
      "\"\\ud800\\u0041\"", "\"\\udc00\"", "\"\\ud83d\"", "\"tab\there\"", "\"nl\nhere\"", "nul",
      "nulll", "true false", "tru", "[true,]", "{\"a\":1}x", "{\"a\":1,\"a\":2}",
      "\"\xC0\xAF\"",          // overlong '/'
      "\"\xED\xA0\x80\"",      // UTF-8 encoded surrogate
      "\"\xF4\x90\x80\x80\"",  // > U+10FFFF
      "\"\xE2\x82\"",          // truncated sequence
      "\"\x80\"",              // lone continuation
      "\"\xFF\"",
  };
  for (const char* b : bad) {
    std::string err;
    INFO("input: " << b);
    CHECK_FALSE(Json::parse(b, &err));
    CHECK_FALSE(err.empty());
  }
  // Duplicate key detection also works for large objects.
  std::string big = "{";
  for (int i = 0; i < 40; ++i) big += "\"k" + std::to_string(i) + "\":" + std::to_string(i) + ",";
  std::string ok = big + "\"last\":0}";
  CHECK(Json::parse(ok));
  std::string dup = big + "\"k7\":0}";
  CHECK_FALSE(Json::parse(dup));
  // Embedded NUL in raw input is a control character.
  CHECK_FALSE(Json::parse(std::string_view("\"a\0b\"", 5)));
  CHECK_FALSE(Json::parse(std::string_view("1\0", 2)));
  // Whitespace is allowed around values.
  CHECK(Json::parse(" \t\r\n{ \"a\" : [ 1 , 2 ] } \n"));
}

TEST_CASE("json: limits") {
  std::string deep(64, '[');
  deep += std::string(64, ']');
  CHECK(Json::parse(deep));
  std::string deeper(65, '[');
  deeper += std::string(65, ']');
  std::string err;
  CHECK_FALSE(Json::parse(deeper, &err));
  CHECK(err.find("deep") != std::string::npos);
  // Very deep hostile input fails quickly and does not overflow the stack.
  std::string hostile(1000000, '[');
  CHECK_FALSE(Json::parse(hostile));
  std::string objs;
  for (int i = 0; i < 100000; ++i) objs += "{\"a\":";
  CHECK_FALSE(Json::parse(objs));

  JsonLimits lim;
  lim.max_input = 10;
  CHECK_FALSE(Json::parse("[1,2,3,4,5,6]", nullptr, lim));
  lim = JsonLimits();
  lim.max_elements = 3;
  CHECK(Json::parse("[1,2]", nullptr, lim));
  CHECK_FALSE(Json::parse("[1,2,3]", nullptr, lim));
  lim = JsonLimits();
  lim.max_string = 4;
  CHECK(Json::parse("\"abcd\"", nullptr, lim));
  CHECK_FALSE(Json::parse("\"abcdefgh\"", nullptr, lim));
  std::string longnum(5000, '1');
  CHECK_FALSE(Json::parse(longnum));
}

TEST_CASE("json: fuzz never crashes and accepted output re-parses") {
  uint64_t s = 0x9E3779B97F4A7C15ull;
  auto rnd = [&]() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; };
  const char alphabet[] = "{}[]\",:0123456789-+.eEtrufalsn\\u \x80\xC3\xA9\xFF";
  std::string seed = R"({"a":[1,2,{"b":"c\u00e9"}],"d":-1.5e3,"e":true,"f":null})";
  int accepted = 0;
  for (int iter = 0; iter < 20000; ++iter) {
    std::string t = seed;
    int edits = 1 + static_cast<int>(rnd() % 4);
    for (int e = 0; e < edits; ++e) {
      size_t pos = rnd() % (t.size() + 1);
      switch (rnd() % 3) {
        case 0: t.insert(t.begin() + static_cast<std::ptrdiff_t>(pos), alphabet[rnd() % (sizeof(alphabet) - 1)]); break;
        case 1: if (pos < t.size()) t.erase(pos, 1); break;
        default: if (pos < t.size()) t[pos] = alphabet[rnd() % (sizeof(alphabet) - 1)]; break;
      }
    }
    auto j = Json::parse(t);
    if (j) {
      ++accepted;
      auto again = Json::parse(j->dump());
      REQUIRE(again);
      CHECK(*again == *j);
    }
  }
  CHECK(accepted > 0);
}

TEST_CASE("json: copy, move and self-assignment") {
  Json a = Json::object({{"x", Json::array({1, 2, "three"})}});
  Json b = a;
  CHECK(a == b);
  b.at_key("x").push(4);
  CHECK(a != b);
  Json c = std::move(b);
  CHECK(c.get("x").size() == 4);
  Json& alias = c;
  c = alias;  // self-assignment through an alias
  CHECK(c.get("x").size() == 4);
  Json child_owner = Json::object({{"k", Json::object({{"v", 1}})}});
  child_owner = std::move(child_owner.at_key("k"));  // move from own child
  CHECK(child_owner.get_int("v") == 1);
  Json n;
  n.push(1).push("x");
  CHECK(n.dump() == "[1,\"x\"]");
  Json m;
  m.set("k", 1);
  CHECK(m.is_object());
  CHECK(Json(1) == Json(1.0));
  CHECK(Json("1") != Json(1));
}

TEST_CASE("utf8 helpers") {
  CHECK(utf8_valid(""));
  CHECK(utf8_valid("abc\xC3\xA9\xF0\x9F\x98\x80"));
  CHECK_FALSE(utf8_valid("\xC3"));
  CHECK(utf8_sanitize("a\xFF") == "a\xEF\xBF\xBD");
  CHECK(utf8_truncate("abc", 10) == "abc");
  CHECK(utf8_truncate("ab\xC3\xA9", 3) == "ab");
  CHECK(utf8_truncate("ab\xC3\xA9", 4) == "ab\xC3\xA9");
  CHECK(utf8_truncate("\xF0\x9F\x98\x80x", 2) == "");
  CHECK(utf8_truncate("abc", 0) == "");
}

}  // TEST_SUITE
