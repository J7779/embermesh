// Node: guides.* against the fixture pack, tools.* with every tool's defaults, rf.bands, tools.qr.
#include <set>

#include "guides/fixture.h"
#include "node/node_harness.h"

using namespace nt;

TEST_SUITE("node") {

TEST_CASE("lib: guides bindings over the fixture pack") {
  TNode a("", 300);
  CHECK(a.ok("guides.categories").get_int("total") == 0);  // nothing loaded yet
  CHECK(a.ok("guides.search", J(R"({"query":"water"})")).get("results").size() == 0);
  const ember::Bytes& pack = guides_test::fixture_pack();
  REQUIRE(ember_guides_load(a.n, pack.data(), pack.size(), 0) == 0);
  Json cats = a.ok("guides.categories");
  size_t expect = guides_test::fixture_guides().size();
  CHECK(cats.get_int("total") == static_cast<int64_t>(expect));
  int64_t sum = 0;
  for (const Json& c : cats.get("categories").items()) {
    CHECK(c.get("id").is_string());
    CHECK(c.get("title").is_string());
    sum += c.get_int("count");
  }
  CHECK(sum == cats.get_int("total"));
  Json all = a.ok("guides.list");
  CHECK(all.get_int("total") == cats.get_int("total"));
  Json g0 = all.get("guides")[0];
  for (const char* k : {"id", "title", "category", "summary", "tags", "source", "source_title", "license", "attribution", "bytes"})
    CHECK(g0.has(k));
  Json water = a.ok("guides.list", J(R"({"category":"water","limit":2,"offset":1})"));
  CHECK(water.get("guides").size() <= 2);
  CHECK(water.get_int("total") >= 3);
  // A book: chapters carry part_of/order and prev/next links.
  Json book = a.ok("guides.list", J(R"({"part_of":"water/field-manual"})"));
  REQUIRE(book.get("guides").size() >= 2);
  std::string ch = book.get("guides")[0].get_string("id");
  CHECK(book.get("guides")[0].has("order"));
  Json g = a.ok("guides.get", Json::object({{"id", ch}}));
  CHECK(g.get_string("markdown").size() > 10);
  CHECK(g.get_string("part_of") == "water/field-manual");
  CHECK(g.has("next"));
  CHECK_FALSE(g.has("prev"));
  Json plain = a.ok("guides.get", J(R"({"id":"water/purification-boiling"})"));
  CHECK_FALSE(plain.has("part_of"));
  CHECK_FALSE(plain.has("order"));
  CHECK(a.err("guides.get", J(R"({"id":"water/nope"})")) == "not_found");
  CHECK(a.err("guides.get", J(R"({})")) == "bad_params");
  Json s = a.ok("guides.search", J(R"({"query":"boil water","limit":5})"));
  REQUIRE(s.get("results").size() >= 1);
  for (const char* k : {"id", "title", "category", "score", "snippet", "terms"}) CHECK(s.get("results")[0].has(k));
  for (const Json& r : a.items("guides.search", "results", J(R"({"query":"water","category":"medical"})")))
    CHECK(r.get_string("category") == "medical");
  CHECK(a.err("guides.search", J(R"({"query":5})")) == "bad_params");
  CHECK(a.err("guides.search", J(R"({"query":"x","limit":0})")) == "bad_params");
  CHECK(a.err("guides.list", J(R"({"offset":-1})")) == "bad_params");
}

TEST_CASE("lib: tools.list schema and tools.run with every tool's defaults") {
  TNode a("", 301);
  Json tl = a.ok("tools.list").get("tools");
  REQUIRE(tl.size() > 10);
  std::set<std::string> cats = {"antenna", "rf", "power", "navigation", "signal", "survival", "crypto"};
  std::set<std::string> types = {"number", "text", "select", "bool", "textarea", "password"};
  int ran = 0;
  for (const Json& t : tl.items()) {
    CAPTURE(t.get_string("id"));
    CHECK(cats.count(t.get_string("category")));
    Json inputs = Json::object();
    bool runnable = true;
    for (const Json& f : t.get("inputs").items()) {
      CHECK(types.count(f.get_string("type")));
      CHECK(f.get("required").is_bool());
      CHECK(f.get("advanced").is_bool());
      if (f.has("default")) {
        if (f.get_string("type") == "number") CHECK(f.get("default").is_number());
        if (f.get_string("type") == "bool") CHECK(f.get("default").is_bool());
        if (f.get_string("type") == "select") {
          bool in = false;
          for (const Json& o : f.get("options").items()) in = in || o.get("value") == f.get("default");
          CHECK(in);
        }
        inputs.set(f.get_string("key"), f.get("default"));
      } else if (f.get_bool("required") && f.get_string("key") != "random_hex") {
        runnable = false;
      }
    }
    if (!runnable) continue;
    Json r = a.ok("tools.run", Json::object({{"id", t.get("id")}, {"inputs", inputs}}));
    CHECK(r.get("outputs").is_array());
    for (const Json& o : r.get("outputs").items()) {
      CHECK(o.get("key").is_string());
      CHECK(o.get("value").is_string());
    }
    ++ran;
  }
  CHECK(ran > 10);
  CHECK(a.err("tools.run", J(R"({"id":"no_such_tool"})")) == "not_found");
  CHECK(a.err("tools.run", J(R"({"id":"otp_sheet","inputs":{"groups":[1]}})")) == "bad_params");
  CHECK(a.err("tools.run", J(R"({"id":"otp_sheet","inputs":{"groups":"lots"}})")) == "bad_params");
  // OTP randomness comes from the node CSPRNG when omitted: two sheets differ.
  Json s1 = a.ok("tools.run", J(R"({"id":"otp_sheet","inputs":{"groups":10}})"));
  Json s2 = a.ok("tools.run", J(R"({"id":"otp_sheet","inputs":{"groups":10}})"));
  CHECK(s1.dump() != s2.dump());
}

TEST_CASE("lib: rf.bands and tools.qr") {
  TNode a("", 302);
  Json all = a.ok("rf.bands").get("services");
  REQUIRE(all.size() > 3);
  for (const Json& s : all.items()) {
    for (const char* k : {"id", "name", "region", "license", "notes", "channels"}) CHECK(s.has(k));
    for (const Json& c : s.get("channels").items()) CHECK(c.get("freq_hz").is_number());
  }
  Json us = a.ok("rf.bands", J(R"({"region":"us"})")).get("services");
  CHECK(us.size() > 0);
  CHECK(us.size() <= all.size());
  std::string sid = all[0].get_string("id");
  CHECK(a.ok("rf.bands", Json::object({{"service", sid}})).get("services").size() == 1);
  CHECK(a.ok("rf.bands", J(R"({"service":"nonexistent"})")).get("services").size() == 0);
  CHECK(a.err("rf.bands", J(R"({"region":7})")) == "bad_params");
  Json br = a.ok("rf.bands", J(R"({"region":"BR"})"));
  CHECK(br.get("services").size() > 3);
  CHECK(br.get("matched_regions").size() >= 3);

  Json q = a.ok("tools.qr", J(R"({"text":"ember:card?id=0011223344556677","ecc":"Q"})"));
  int64_t size = q.get_int("size");
  CHECK(size >= 21);
  REQUIRE(q.get("rows").size() == static_cast<size_t>(size));
  for (const Json& row : q.get("rows").items()) {
    CHECK(row.as_string().size() == static_cast<size_t>(size));
    CHECK(row.as_string().find_first_not_of("01") == std::string::npos);
  }
  CHECK(a.err("tools.qr", J(R"({"text":""})")) == "bad_params");
  CHECK(a.err("tools.qr", J(R"({"text":"x","ecc":"Z"})")) == "bad_params");
  CHECK(a.err("tools.qr", Json::object({{"text", std::string(3000, 'x')}, {"ecc", "H"}})) == "too_large");
}

}  // TEST_SUITE
