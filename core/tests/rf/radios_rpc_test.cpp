// rf.radios over the C API: result shape per docs/RPC_API.md §6.
#include <string>
#include <vector>

#include "base/json.h"
#include "doctest.h"
#include "ember/ember.h"

using namespace ember;

TEST_SUITE("rf") {

TEST_CASE("rf.radios RPC returns the Radio records of the contract") {
  std::vector<uint8_t> ent(32, 7);
  ember_node* n = ember_create("{}", ent.data(), ent.size());
  REQUIRE(n);
  char* r = ember_call(n, "rf.radios", "{}");
  REQUIRE(r);
  Json env;
  REQUIRE(Json::parse(std::string(r), env));
  ember_free(r);
  REQUIRE(env.get_bool("ok"));
  const Json& radios = env.get("result").get("radios");
  REQUIRE(radios.is_array());
  REQUIRE(radios.size() >= 15);
  for (const Json& x : radios.items()) {
    for (const char* k : {"id", "maker", "model", "kind", "connector", "cable_notes", "notes"}) CHECK(x.get(k).is_string());
    for (const char* k : {"vox", "data_port", "builtin_tnc", "bluetooth", "verified"}) CHECK(x.get(k).is_bool());
    CHECK(x.get("bands").is_array());
    CHECK(x.get("power_w").is_array());
    REQUIRE(x.get("settings").is_array());
    for (const Json& s : x.get("settings").items())
      CHECK((s.get("label").is_string() && s.get("value").is_string() && s.get("how").is_string()));
    const Json& rec = x.get("recommended");
    CHECK(rec.get("link").get("type").is_string());
    CHECK(rec.get("link").get("config").is_object());
  }
  r = ember_call(n, "rf.radios", R"({"query":"baofeng"})");
  Json q;
  REQUIRE(Json::parse(std::string(r), q));
  ember_free(r);
  CHECK(q.get("result").get("radios").size() >= 3);
  r = ember_call(n, "rf.radios", R"({"query":5})");
  Json bad;
  REQUIRE(Json::parse(std::string(r), bad));
  ember_free(r);
  CHECK(!bad.get_bool("ok"));
  ember_destroy(n);
}

}  // TEST_SUITE
