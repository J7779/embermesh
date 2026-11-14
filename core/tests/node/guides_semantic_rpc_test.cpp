// guides.search (semantic fields), guides.related, guides.ask and guides.info JSON bindings
// (core/src/node/node_guides.cpp) over the fixture corpus packed with the committed model.
#include "guides/fixture.h"
#include "guides/pack_builder.h"
#include "node/node_guides.h"
#include "node/node_harness.h"

using namespace nt;
using namespace ember;

namespace {
const guides::GuideLibrary& lib_with(bool semantic) {
  static Bytes sp, kp;
  static guides::GuideLibrary ls, lk;
  static bool once = [] {
    guides::SemanticBuildOptions o;
    o.model = ByteView(guides_test::committed_model());
    sp = guides::build_pack(guides_test::fixture_guides(), o);
    kp = guides::build_pack(guides_test::fixture_guides());
    return ls.load(ByteView(sp), false) == guides::LoadStatus::kOk && lk.load(ByteView(kp), false) == guides::LoadStatus::kOk;
  }();
  (void)once;
  return semantic ? ls : lk;
}
Json call(node::RpcResult (*f)(const guides::GuideLibrary&, node::Params&), bool semantic, const char* params) {
  Json j = J(params);
  node::Params p(j);
  node::RpcResult r = f(lib_with(semantic), p);
  REQUIRE(r.ok);
  return r.result;
}
std::string call_err(node::RpcResult (*f)(const guides::GuideLibrary&, node::Params&), const char* params) {
  Json j = J(params);
  node::Params p(j);
  node::RpcResult r = f(lib_with(true), p);
  return r.ok ? std::string() : r.code;
}
}  // namespace

TEST_SUITE("node") {

TEST_CASE("guides rpc: search gains match and passage") {
  Json r = call(node::guides_search_rpc, true, R"({"query":"blood pouring out of a deep cut"})");
  REQUIRE(r.get("results").size() > 0);
  bool any_passage = false;
  for (const Json& x : r.get("results").items()) {
    std::string m = x.get_string("match");
    CHECK((m == "words" || m == "meaning" || m == "both"));
    if (x.has("passage")) {
      any_passage = true;
      CHECK(x.get("passage").get("text").is_string());
      CHECK(x.get("passage").get_string("text").size() <= 600);
    }
  }
  CHECK(any_passage);
  Json k = call(node::guides_search_rpc, true, R"({"query":"bleeding","semantic":false})");
  for (const Json& x : k.get("results").items()) {
    CHECK(x.get_string("match") == "words");
    CHECK_FALSE(x.has("passage"));
  }
  CHECK(call_err(node::guides_search_rpc, R"({"query":"x","semantic":"yes"})") == "bad_params");
}

TEST_CASE("guides rpc: related, ask, info") {
  Json rel = call(node::guides_related_rpc, true, R"({"id":"medical/burns","limit":3})");
  REQUIRE(rel.get("results").size() > 0);
  CHECK(rel.get("results").size() <= 3);
  Json r0 = rel.get("results")[0];
  for (const char* key : {"id", "title", "category", "summary", "tags", "license", "score"}) CHECK(r0.has(key));
  CHECK(r0.get("score").as_double() <= 1.0);
  CHECK(call(node::guides_related_rpc, false, R"({"id":"medical/burns"})").get("results").size() == 0);
  CHECK(call_err(node::guides_related_rpc, R"({})") == "bad_params");
  CHECK(call_err(node::guides_related_rpc, R"({"id":"medical/burns","limit":0})") == "bad_params");

  Json ask = call(node::guides_ask_rpc, true, R"({"question":"how much bleach for drinking water","limit":3})");
  REQUIRE(ask.get("passages").size() > 0);
  Json p0 = ask.get("passages")[0];
  for (const char* key : {"id", "title", "category", "text", "score"}) CHECK(p0.has(key));
  CHECK_FALSE(ask.has("weak"));
  Json off = call(node::guides_ask_rpc, true, R"({"question":"celebrity gossip and football transfer news"})");
  CHECK(off.has("weak"));
  CHECK(off.get("weak").as_bool());
  Json none = call(node::guides_ask_rpc, false, R"({"question":"bleeding"})");
  CHECK(none.get("passages").size() == 0);
  CHECK(none.has("weak"));
  CHECK(call_err(node::guides_ask_rpc, R"({"limit":3})") == "bad_params");

  Json info = call(node::guides_info_rpc, true, R"({})");
  CHECK(info.get_int("guides") == static_cast<int64_t>(guides_test::fixture_guides().size()));
  CHECK(info.get_int("categories") > 0);
  CHECK(info.get_int("bytes") > 0);
  CHECK(info.get("semantic").get("available").as_bool());
  CHECK(info.get("semantic").get_string("model").find("potion") != std::string::npos);
  CHECK(info.get("semantic").get_int("dims") > 0);
  CHECK(info.get("semantic").get_int("bits") == 4);
  CHECK(info.get("semantic").get_int("passages") > 0);
  int64_t n = 0;
  for (const Json& l : info.get("licences").items()) { CHECK(l.get("name").is_string()); n += l.get_int("count"); }
  CHECK(n == info.get_int("guides"));
  Json kinfo = call(node::guides_info_rpc, false, R"({})");
  CHECK_FALSE(kinfo.get("semantic").get("available").as_bool());
  CHECK_FALSE(kinfo.get("semantic").has("model"));
}

TEST_CASE("guides rpc: guides.search through the node keeps working") {
  TNode a("", 300);
  guides::SemanticBuildOptions o;
  o.model = ByteView(guides_test::committed_model());
  static Bytes pack = guides::build_pack(guides_test::fixture_guides(), o);
  REQUIRE(ember_guides_load(a.n, pack.data(), pack.size(), 0) == 0);
  Json r = a.ok("guides.search", J(R"({"query":"snake bit me"})"));
  REQUIRE(r.get("results").size() > 0);
  CHECK(r.get("results")[0].has("match"));
}

TEST_CASE("guides rpc: guides.related, guides.ask and guides.info are registered in the node (also while locked)") {
  TNode a("", 301);
  guides::SemanticBuildOptions o;
  o.model = ByteView(guides_test::committed_model());
  static Bytes pack = guides::build_pack(guides_test::fixture_guides(), o);
  REQUIRE(ember_guides_load(a.n, pack.data(), pack.size(), 0) == 0);
  auto check_all = [&] {
    Json info = a.ok("guides.info");
    CHECK(info.get_int("guides") > 0);
    CHECK(info.get("semantic").get_bool("available"));
    CHECK(info.get("licences").is_array());
    Json ask = a.ok("guides.ask", J(R"({"question":"how do I treat a snake bite","limit":3})"));
    REQUIRE(ask.get("passages").is_array());
    REQUIRE(ask.get("passages").size() > 0);
    for (const char* f : {"id", "title", "category", "text"}) CHECK(ask.get("passages")[0].get(f).is_string());
    CHECK(ask.get("passages")[0].get("score").is_number());
    std::string id = ask.get("passages")[0].get_string("id");
    Json rel = a.ok("guides.related", Json::object({{"id", id}, {"limit", 4}}));
    REQUIRE(rel.get("results").is_array());
    for (const Json& x : rel.get("results").items()) {
      CHECK(x.get_string("id") != id);
      CHECK(x.get("score").is_number());
    }
    CHECK(a.err("guides.ask", J(R"({})")) == "bad_params");
    CHECK(a.err("guides.ask", J(R"({"question":5})")) == "bad_params");
    CHECK(a.err("guides.related", J(R"({})")) == "bad_params");
    CHECK(a.err("guides.related", J(R"({"id":"x","limit":"many"})")) == "bad_params");
  };
  check_all();
  // Guides work while the node is locked, exactly like guides.search.
  a.ok("security.set_passphrase", J(R"({"new":"fixture pass"})"));
  a.ok("security.lock");
  CHECK(a.ok("security.status").get_bool("locked"));
  check_all();
  // A pack without the semantic section: empty related / ask, info says so.
  TNode b("", 302);
  static Bytes kp = guides::build_pack(guides_test::fixture_guides());
  REQUIRE(ember_guides_load(b.n, kp.data(), kp.size(), 0) == 0);
  CHECK_FALSE(b.ok("guides.info").get("semantic").get_bool("available"));
  Json ask = b.ok("guides.ask", J(R"({"question":"how do I treat a snake bite"})"));
  CHECK(ask.get("passages").size() == 0);
  Json rel = b.ok("guides.related", J(R"({"id":"medical/snake-bites"})"));
  CHECK(rel.get("results").size() == 0);
}

}  // TEST_SUITE
