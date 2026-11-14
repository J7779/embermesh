// A link that requires a station id (amateur band) transmits nothing while the profile has no
// callsign (mesh/tx.cpp). The node surfaces that on the Link object (blocked:"no_callsign",
// status.last_error) and emits link.state when it starts and when it clears.
#include "doctest.h"
#include "node_harness.h"

using namespace nt;
using ember::Json;

TEST_SUITE("node") {

TEST_CASE("links: amateur link without a callsign reports blocked:no_callsign, cleared when one is set") {
  TNode a("", 91);
  uint64_t now = kStart;
  auto tick = [&](int n) {
    for (int i = 0; i < n; ++i) {
      now += 50;
      ember_tick(a.n, now);
      a.drain();
    }
  };
  Json add = a.ok("links.add", J(R"({"type":"kiss","config":{"licence":"amateur"}})"));
  uint32_t id = static_cast<uint32_t>(add.get("link").get_int("id"));
  a.ok("links.endpoint_state", Json::object({{"id", id}, {"connected", true}}));
  tick(10);
  auto find = [&]() {
    for (const Json& l : a.items("links.list", "links"))
      if (static_cast<uint32_t>(l.get_int("id")) == id) return l;
    return Json();
  };
  Json l = find();
  REQUIRE(l.is_object());
  CHECK(l.get_string("state") == "up");
  CHECK(l.get_string("blocked") == "no_callsign");
  CHECK(l.get("status").get_string("last_error").find("callsign") != std::string::npos);
  const Ev* ev = a.last("link.state");
  REQUIRE(ev);
  CHECK(ev->data.get("link").get_string("blocked") == "no_callsign");
  // Nothing goes out while blocked.
  a.ok("messages.send", J(R"({"conv":"ch:public","text":"will not be sent"})"));
  uint64_t tx0 = a.tx_items;
  tick(40);
  CHECK(a.tx_items == tx0);

  size_t states = a.count("link.state");
  a.ok("node.set_profile", J(R"({"callsign":"QX0XYZ"})"));
  tick(10);
  CHECK(a.count("link.state") > states);  // the condition cleared -> link.state
  ev = a.last("link.state");
  REQUIRE(ev);
  CHECK_FALSE(ev->data.get("link").has("blocked"));
  l = find();
  CHECK_FALSE(l.has("blocked"));
  tick(200);
  CHECK(a.tx_items > tx0);  // now it transmits

  // A licence-free link is never blocked.
  Json add2 = a.ok("links.add", J(R"({"type":"kiss","config":{"licence":"licence_free"}})"));
  CHECK_FALSE(add2.get("link").has("blocked"));
}

}  // TEST_SUITE
