// links.list / links.add / links.configure report the effective link config (preset applied,
// profile callsign substituted), plus `preset` and `overrides` (docs/RPC_API.md §6).
#include "doctest.h"
#include "node_harness.h"

using namespace ember;
using namespace nt;

TEST_SUITE("node") {

TEST_CASE("links: config is the effective configuration, with preset and overrides") {
  TNode a("", 340);
  a.ok("node.set_profile", J(R"({"callsign":"QX4ABC"})"));
  Json l = a.ok("links.add", J(R"({"type":"audio","name":"ht","config":{"preset":"handheld_vox","txtail_ms":90}})"))
               .get("link");
  CHECK(l.get_string("preset") == "handheld_vox");
  const Json& c = l.get("config");
  CHECK(c.get_int("txdelay_ms") == 600);       // preset, not the schema default 300
  CHECK(c.get_int("vox_tone_ms") == 400);
  CHECK(c.get_string("modem") == "afsk1200");
  CHECK(c.get_int("txtail_ms") == 90);         // explicit key wins over the preset (150)
  CHECK(c.get_string("callsign") == "QX4ABC");  // profile callsign, not N0CALL
  CHECK(l.get("overrides").size() == 2);
  CHECK(l.get("overrides").get_int("txtail_ms") == 90);
  CHECK(!l.get("overrides").has("txdelay_ms"));
  int64_t id = l.get_int("id");

  // links.list shows the same; links.configure re-reports after a preset change.
  Json list = a.ok("links.list", J("{}"));
  bool seen = false;
  for (const Json& x : list.get("links").items())
    if (x.get_int("id") == id) {
      seen = true;
      CHECK(x.get("config").get_int("txdelay_ms") == 600);
      CHECK(x.get_string("preset") == "handheld_vox");
    }
  CHECK(seen);
  Json r = a.ok("links.configure", Json::object({{"id", id}, {"config", J(R"({"preset":"acoustic"})")}})).get("link");
  CHECK(r.get_string("preset") == "acoustic");
  CHECK(r.get("config").get_string("modem") == "robust125");
  CHECK(r.get("config").get_int("txdelay_ms") == 1500);

  // Licence-free AX.25 link without any callsign: the pseudo station id actually transmitted.
  TNode b("", 341);
  Json k = b.ok("links.add", J(R"({"type":"kiss","config":{"licence":"licence_free"}})")).get("link");
  CHECK(k.get("config").get_string("callsign").rfind("QX", 0) == 0);
  CHECK(k.get("overrides").size() == 1);
  CHECK(!k.has("preset"));
}

}  // TEST_SUITE
