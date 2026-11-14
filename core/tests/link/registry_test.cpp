// Registry, Field schema, config parsing and shared queue behaviour.
#include <map>
#include <set>

#include "doctest.h"
#include "link/drivers.h"
#include "link/link_util.h"
#include "link_harness.h"

using namespace ember;
using namespace linktest;

TEST_SUITE("link") {

TEST_CASE("link_types_json is valid JSON matching the Field schema") {
  std::string err;
  Json types;
  REQUIRE(Json::parse(link_types_json(), types, &err));
  REQUIRE(types.is_array());
  CHECK(types.size() == link::all_types().size());
  std::set<std::string> seen;
  const std::set<std::string> kinds = {"websocket", "ws_server", "tcp", "tcp_server", "udp", "serial", "ble",
                                       "ble_peer", "bt_spp", "audio", "webrtc", "multipeer", "wifi_direct",
                                       "rtl_tcp", "loopback", "audio_pipe"};
  const std::set<std::string> ftypes = {"number", "text", "select", "bool", "textarea", "password"};
  for (const Json& t : types.items()) {
    CAPTURE(t.dump());
    REQUIRE(t.is_object());
    std::string name = t.get_string("type");
    CHECK(!name.empty());
    CHECK(seen.insert(name).second);
    CHECK(!t.get_string("title").empty());
    CHECK(!t.get_string("description").empty());
    REQUIRE(t.get("endpoint_kinds").is_array());
    CHECK(t.get("endpoint_kinds").size() > 0);
    for (const Json& k : t.get("endpoint_kinds").items()) CHECK(kinds.count(k.as_string()) == 1);
    REQUIRE(t.get("config").is_array());
    std::set<std::string> keys;
    for (const Json& f : t.get("config").items()) {
      CAPTURE(f.dump());
      CHECK(!f.get_string("key").empty());
      CHECK(keys.insert(f.get_string("key")).second);
      CHECK(!f.get_string("label").empty());
      CHECK(ftypes.count(f.get_string("type")) == 1);
      CHECK(f.has("default"));
      CHECK(!f.get_string("help").empty());
      CHECK(f.get("required").is_bool());
      CHECK(f.get("advanced").is_bool());
      const Json& d = f.get("default");
      std::string ft = f.get_string("type");
      if (ft == "number") {
        CHECK(d.is_number());
        if (f.has("min")) CHECK(d.as_double() >= f.get_double("min"));
        if (f.has("max")) CHECK(d.as_double() <= f.get_double("max"));
      } else if (ft == "bool") {
        CHECK(d.is_bool());
      } else if (ft == "select") {
        REQUIRE(f.get("options").is_array());
        bool found = false;
        for (const Json& o : f.get("options").items()) {
          CHECK(!o.get_string("label").empty());
          found |= o.get_string("value") == d.as_string();
        }
        CHECK(found);
      } else {
        CHECK(d.is_string());
      }
    }
  }
}

TEST_CASE("every registered type is creatable from its schema defaults") {
  Clock clk;
  FakeHost h(&clk);
  for (const link::LinkTypeInfo* t : link::all_types()) {
    CAPTURE(t->type);
    std::string err;
    auto l = create_link(t->type, "{}", h, &err);
    CAPTURE(err);
    REQUIRE(l);
    CHECK(std::string(l->type()) == t->type);
    CHECK(l->state() == LinkState::Down);
    LinkCaps c = l->caps();
    CHECK((c.mtu == 0 || c.mtu >= 32));
    CHECK(c.bitrate_bps > 0);
    CHECK(c.duty_cycle_permille <= 1000);
    Json st;
    CHECK(Json::parse(l->status_json(), st));
    CHECK(st.is_object());
    // empty config string is accepted too, and reconfigure with defaults works
    auto l2 = create_link(t->type, "", h, &err);
    CHECK(l2);
    CHECK(l->reconfigure("{}", &err));
    // garbage input on a fresh link must not crash
    Prng r(5);
    for (int i = 0; i < 50; ++i) {
      Bytes g = r.bytes(r.below(300));
      l->on_io_bytes(ByteView(g));
    }
    l->on_io_state(true);
    for (int i = 0; i < 50; ++i) {
      Bytes g = r.bytes(r.below(300));
      l->on_io_bytes(ByteView(g));
      clk.t += 37;
      l->tick(clk.t);
    }
    l->on_io_state(false);
    l->tick(clk.t);
  }
}

TEST_CASE("create_link rejects unknown types and bad config") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  CHECK(!create_link("nope", "{}", h, &err));
  CHECK(err.find("unknown") != std::string::npos);
  CHECK(!create_link("raw", "[1,2]", h, &err));
  CHECK(!create_link("raw", "{\"mtu\":", h, &err));
  CHECK(!create_link("raw", "{\"mtu\":\"big\"}", h, &err));
  CHECK(!create_link("raw", "{\"mtu\":99999}", h, &err));
  CHECK(!create_link("raw", "{\"mtu\":12}", h, &err));
  CHECK(!create_link("raw", "{\"mtu\":100.5}", h, &err));
  CHECK(!create_link("raw", "{\"preset\":\"martian\"}", h, &err));
  CHECK(!create_link("raw", "{\"broadcast\":true}", h, &err));
  CHECK(create_link("raw", "{\"mtu\":100.0, \"unknown_key\": 5}", h, &err));
  CHECK(create_link("raw", "  ", h, nullptr));
}

TEST_CASE("TxQueue: strict priority, FIFO, bounds, eviction") {
  link::TxQueue q(4, 1000);
  Bytes a{1}, b{2}, c{3}, d{4}, e{5};
  CHECK(q.push(ByteView(a), 5));
  CHECK(q.push(ByteView(b), 1));
  CHECK(q.push(ByteView(c), 5));
  CHECK(q.push(ByteView(d), 7));
  CHECK(q.size() == 4);
  CHECK(!q.push(ByteView(e), 7));   // full, nothing lower to evict
  CHECK(q.push(ByteView(e), 0));    // evicts the priority-7 frame
  CHECK(q.evicted() == 1);
  Bytes out;
  uint8_t p = 0;
  REQUIRE(q.pop(&out, &p));
  CHECK((out == Bytes{5})); CHECK(p == 0);
  REQUIRE(q.pop(&out, &p));
  CHECK((out == Bytes{2}));
  REQUIRE(q.pop(&out, &p));
  CHECK((out == Bytes{1}));
  REQUIRE(q.pop(&out, &p));
  CHECK((out == Bytes{3}));
  CHECK(!q.pop(&out, &p));
  // byte limit
  link::TxQueue q2(100, 10);
  Bytes big(8, 1), small(3, 2);
  CHECK(q2.push(ByteView(big), 3));
  CHECK(!q2.push(ByteView(small), 3));
  CHECK(q2.push(ByteView(small), 2));
  CHECK(q2.size() == 1);
  CHECK(q2.bytes() == 3);
  CHECK(!q2.push(ByteView(Bytes(11, 0)), 0));
}

TEST_CASE("base64 round trip and strictness") {
  Prng r(3);
  for (size_t n = 0; n < 40; ++n) {
    Bytes b = r.bytes(n);
    std::string s = link::base64_encode(ByteView(b));
    Bytes back;
    REQUIRE(link::base64_decode(s, &back, 100));
    CHECK(back == b);
  }
  Bytes o;
  CHECK(link::base64_encode(ByteView(std::string_view("foobar"))) == "Zm9vYmFy");
  CHECK(link::base64_encode(ByteView(std::string_view("fo"))) == "Zm8=");
  CHECK(!link::base64_decode("Zm9", &o, 100));
  CHECK(!link::base64_decode("Zm9=Zm9v", &o, 100));
  CHECK(!link::base64_decode("Zm9!", &o, 100));
  CHECK(!link::base64_decode("Zm9vYmFy", &o, 5));
  CHECK(link::base64_decode("Zm8=", &o, 100));
  CHECK(!link::base64_decode("Zn==", &o, 100));  // non-canonical
}

TEST_CASE("unescape_text") {
  Bytes o;
  CHECK(link::unescape_text("KISS ON\\rRESTART\\r", &o));
  CHECK(ByteView(o).to_string() == "KISS ON\rRESTART\r");
  CHECK(link::unescape_text("\\xC0\\x0a", &o));
  CHECK((o == Bytes{0xC0, 0x0A}));
  CHECK(!link::unescape_text("\\x", &o));
  CHECK(!link::unescape_text("\\x1", &o));
  CHECK(!link::unescape_text("\\q", &o));
  CHECK(!link::unescape_text("abc\\", &o));
}

TEST_CASE("DutyCycle window") {
  link::DutyCycle d(1000);
  CHECK(d.allows(0, 100, 100));
  d.add(0, 100);
  CHECK(!d.allows(10, 1, 100));
  CHECK(d.allows(1000, 100, 100));
  CHECK(d.allows(10, 1, 1000));
}

TEST_CASE("endpoint kinds per driver match what the platforms provide") {
  // Byte streams: tcp, tcp_server, serial, ble, ble_peer, bt_spp, wifi_direct (Android Wi-Fi Direct is
  // a TCP socket). Datagrams: udp, websocket, ws_server, webrtc, multipeer, loopback. Audio: audio,
  // audio_pipe (desktop test pipe). SDR: rtl_tcp (or plain tcp to an rtl_tcp server).
  const std::set<std::string> streams = {"tcp", "tcp_server", "serial", "ble", "ble_peer", "bt_spp", "wifi_direct"};
  const std::set<std::string> datagrams = {"udp", "websocket", "ws_server", "webrtc", "multipeer", "loopback"};
  const std::map<std::string, std::set<std::string>> expect = {
      {"raw", datagrams},
      {"stream", streams},
      {"kiss", {"serial", "tcp", "ble", "bt_spp"}},
      {"audio", {"audio", "audio_pipe"}},
      {"meshtastic", {"serial", "tcp", "ble"}},
      {"rnode", {"serial", "ble", "bt_spp", "tcp"}},
      {"lora_at", {"serial", "ble", "bt_spp", "tcp"}},
      {"rtl_tcp", {"rtl_tcp", "tcp"}},
  };
  Json types;
  REQUIRE(Json::parse(link_types_json(), types));
  size_t n = 0;
  for (const Json& t : types.items()) {
    std::string name = t.get_string("type");
    CAPTURE(name);
    auto it = expect.find(name);
    REQUIRE(it != expect.end());
    std::set<std::string> got;
    for (const Json& k : t.get("endpoint_kinds").items()) got.insert(k.as_string());
    CHECK(got == it->second);
    // No byte-stream driver may claim a datagram kind and vice versa.
    for (const std::string& k : got) {
      if (name == "raw") CHECK(streams.count(k) == 0);
      if (name != "raw" && name != "audio") CHECK(datagrams.count(k) == 0);
    }
    ++n;
  }
  CHECK(n == expect.size());
}

}  // TEST_SUITE
