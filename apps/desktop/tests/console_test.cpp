#include <sstream>

#include "console.h"
#include "doctest.h"
#include "host/host.h"

using ember_host::Json;

namespace {

struct FakeRpc {
  std::vector<std::pair<std::string, Json>> calls;
  Json operator()(const std::string& m, const Json& p) {
    calls.emplace_back(m, p);
    if (m == "channels.list")
      return ember_host::rpc_ok(Json::parse_or_null(R"({"channels":[{"id":"pub1","name":"Public","is_default":true}]})"));
    if (m == "contacts.list")
      return ember_host::rpc_ok(Json::parse_or_null(R"({"contacts":[{"node_id":"0123456789abcdef","name":"Alice"}]})"));
    if (m == "conversations.list")
      return ember_host::rpc_ok(Json::parse_or_null(R"({"conversations":[{"id":"dm:0123456789abcdef","title":"Alice","unread":2}]})"));
    if (m == "messages.send") return ember_host::rpc_ok(Json::parse_or_null(R"({"message":{"status":"queued"}})"));
    if (m == "node.info") return ember_host::rpc_ok(Json::parse_or_null(R"({"node_id":"ffffffffffffffff","name":"me","fingerprint":["a","b"]})"));
    return ember_host::rpc_error("not_implemented", "fake");
  }
  const Json* last(const std::string& m) const {
    for (auto it = calls.rbegin(); it != calls.rend(); ++it)
      if (it->first == m) return &it->second;
    return nullptr;
  }
};

}  // namespace

TEST_SUITE("console") {
  TEST_CASE("say goes to the default channel") {
    FakeRpc rpc;
    std::ostringstream out;
    emberd::Console c([&](const std::string& m, const Json& p) { return rpc(m, p); }, out);
    CHECK(c.execute("say hello world"));
    const Json* p = rpc.last("messages.send");
    REQUIRE(p);
    CHECK(p->get("conv").as_string() == "ch:pub1");
    CHECK(p->get("text").as_string() == "hello world");
  }
  TEST_CASE("dm resolves contact names and node ids") {
    FakeRpc rpc;
    std::ostringstream out;
    emberd::Console c([&](const std::string& m, const Json& p) { return rpc(m, p); }, out);
    c.execute("dm alice hi");
    REQUIRE(rpc.last("messages.send"));
    CHECK(rpc.last("messages.send")->get("conv").as_string() == "dm:0123456789abcdef");
    rpc.calls.clear();
    c.execute("dm nobody hi");
    CHECK(rpc.last("messages.send") == nullptr);
    CHECK(out.str().find("unknown conversation") != std::string::npos);
  }
  TEST_CASE("convs numbering and read #n") {
    FakeRpc rpc;
    std::ostringstream out;
    emberd::Console c([&](const std::string& m, const Json& p) { return rpc(m, p); }, out);
    c.execute("convs");
    c.execute("read #1 5");
    const Json* p = rpc.last("messages.list");
    REQUIRE(p);
    CHECK(p->get("conv").as_string() == "dm:0123456789abcdef");
    CHECK(p->get("limit").as_int() == 5);
  }
  TEST_CASE("errors are printed, quit returns false, unknown commands are harmless") {
    FakeRpc rpc;
    std::ostringstream out;
    emberd::Console c([&](const std::string& m, const Json& p) { return rpc(m, p); }, out);
    CHECK(c.execute("peers"));
    CHECK(out.str().find("not_implemented") != std::string::npos);
    CHECK(c.execute("frobnicate"));
    CHECK(c.execute(""));
    CHECK(c.execute("checkin bogus"));
    CHECK(c.execute("link add {not json"));
    CHECK(c.execute("sos"));
    CHECK(rpc.last("sos.start") != nullptr);
    CHECK_FALSE(c.execute("quit"));
  }
  TEST_CASE("events are rendered") {
    FakeRpc rpc;
    std::ostringstream out;
    emberd::Console c([&](const std::string& m, const Json& p) { return rpc(m, p); }, out);
    c.on_event(R"({"event":"message.new","data":{"message":{"conv":"ch:pub1","from_name":"Bob","text":"hey","ts":0}}})");
    c.on_event(R"({"event":"sos.received","data":{"node_id":"0123456789abcdef","name":"Alice","text":"help","hops":2}})");
    c.on_event("garbage");
    CHECK(out.str().find("Bob: hey") != std::string::npos);
    CHECK(out.str().find("SOS from Alice") != std::string::npos);
  }
}
