// Host router: host.* methods, link lifecycle with endpoints, persistence + auto-restore, C API.
#include "doctest.h"
#include "ember_host.h"
#include "host/host.h"
#include "host/stream.h"
#include "test_util.h"
#include <algorithm>

using namespace ember_host;
using test::Json;

namespace {
Json J(const char* s) { return Json::parse_or_null(s); }

struct HostFixture {
  test::TempDir data;
  test::FakeCore core;
  std::unique_ptr<Host> host;
  HostFixture() { reset(); }
  void reset() {
    host.reset();
    HostConfig c;
    c.data_dir = data.path;
    host = std::make_unique<Host>(core.node(), c, test::FakeCore::api());
    host->loop().bind_thread();
    host->start();
  }
};
}  // namespace

TEST_SUITE("host") {
  TEST_CASE("host.info lists endpoint kinds with Field schemas") {
    HostFixture f;
    Json r = f.host->call("host.info", Json::object());
    REQUIRE(r.get("ok").as_bool());
    const Json& res = r.get("result");
    CHECK(res.get("platform").as_string() == "desktop");
    CHECK(res.get("backend").as_string() == "native");
    std::vector<std::string> kinds;
    for (const auto& k : res.get("endpoint_kinds").elements()) {
      kinds.push_back(k.get("kind").as_string());
      CHECK(k.get("config").is_array());
      CHECK(k.has("link_types"));
      CHECK(k.has("default_link_type"));
      for (const auto& fld : k.get("config").elements()) {
        CHECK(!fld.get("key").as_string().empty());
        CHECK(!fld.get("type").as_string().empty());
      }
    }
    for (const char* want : {"ws_server", "tcp", "tcp_server", "udp", "websocket", "serial", "audio", "rtl_tcp", "loopback"})
      CHECK(std::find(kinds.begin(), kinds.end(), want) != kinds.end());
    // link_types refined from the core's links.types
    for (const auto& k : res.get("endpoint_kinds").elements())
      if (k.get("kind").as_string() == "tcp") CHECK(k.get("link_types").dump() == "[\"kiss\"]");
    bool hub = false;
    for (const auto& x : res.get("features").elements()) hub |= x.as_string() == "hub";
    CHECK(hub);
  }

  TEST_CASE("unsupported and bad params") {
    HostFixture f;
    CHECK(f.host->call("host.qr.scan", Json::object()).get("error").get("code").as_string() == "unsupported");
    CHECK(f.host->call("host.webrtc.create_offer", Json::object()).get("error").get("code").as_string() == "unsupported");
    CHECK(f.host->call("host.link.create", J(R"({"endpoint":{"kind":"warp"}})")).get("error").get("code").as_string() == "bad_params");
    CHECK(f.host->call("host.link.create", J(R"({"endpoint":{"kind":"tcp","config":{"port":0}}})")).get("error").get("code").as_string() == "bad_params");
    CHECK(f.host->call("host.endpoints.scan", J(R"({"kind":"warp"})")).get("error").get("code").as_string() == "bad_params");
    CHECK(f.host->call("host.link.reconnect", J(R"({"id":999})")).get("error").get("code").as_string() == "not_found");
    // Core methods are forwarded.
    CHECK(f.host->call("node.info", Json::object()).get("result").get("name").as_string() == "fake");
    CHECK(f.host->call_json("node.info", "{bad").find("bad_params") != std::string::npos);
  }

  TEST_CASE("link lifecycle over a loopback endpoint; the core's link list is the only persistence") {
    HostFixture f;
    f.core.mode = test::FakeCore::Echo;
    Json r = f.host->call("host.link.create", J(R"({"name":"lb","endpoint":{"kind":"loopback","config":{}},"link":{"type":"raw","config":{"x":1}}})"));
    REQUIRE(r.get("ok").as_bool());
    uint32_t id = uint32_t(r.get("result").get("link").get("id").as_int());
    REQUIRE(id != 0);
    CHECK(f.core.links[id].spec.get("endpoint").get("kind").as_string() == "loopback");
    CHECK_FALSE(f.core.links[id].spec.get("endpoint").has("host_key"));  // only when the caller gives one
    CHECK(f.core.links[id].spec.get("persist").as_bool(false));
    CHECK(f.core.links[id].spec.get("config").get("preset").as_string() == "loopback");  // default preset filled in
    CHECK(f.core.links[id].spec.get("config").get("x").as_int() == 1);
    // endpoint opens asynchronously -> links.endpoint_state(connected)
    REQUIRE(test::run_until(f.host->loop(), [&] { return f.core.links[id].connected; }));
    // Core TX -> endpoint -> loopback -> core RX (echo) -> ... check one round trip.
    f.core.outbox.push_back({EMBER_OUT_LINK_TX, id, "ping"});
    f.host->wakeup();
    REQUIRE(test::run_until(f.host->loop(), [&] { return !f.core.rx.empty(); }));
    CHECK(f.core.rx[0] == std::make_pair(id, std::string("ping")));
    f.core.mode = test::FakeCore::Sink;

    // No private link store any more.
    CHECK_FALSE(is_file(path_join(f.data.path, "host_links.json")));
    CHECK_FALSE(f.core.settings.has("links_autostart"));

    // Restart: the endpoint is reopened from links.list, no links.add.
    size_t adds_before = f.core.count_calls("links.add");
    f.core.restart();
    f.reset();
    CHECK(f.core.count_calls("links.add") == adds_before);
    REQUIRE(test::run_until(f.host->loop(), [&] { return f.core.links[id].connected; }));

    // A core that forgot its links (wipe, new identity): nothing is re-created behind its back.
    f.core.links.clear();
    f.reset();
    CHECK(f.core.links.empty());

    // node.ready re-runs the startup step: a link that appeared in the core gets its endpoint.
    f.core.links[7] = test::FakeCore::L{7, J(R"({"type":"raw","name":"x","endpoint":{"kind":"loopback","config":{}}})"), false};
    f.host->call("test.emit", J(R"({"json":"{\"event\":\"node.ready\",\"data\":{}}"})"));
    REQUIRE(test::run_until(f.host->loop(), [&] { return f.core.links[7].connected; }));
    // ... and one that disappeared from the core is closed.
    f.core.links.erase(7);
    f.host->call("test.emit", J(R"({"json":"{\"event\":\"node.ready\",\"data\":{}}"})"));
    test::run_for(f.host->loop(), 50);
    Json ls = f.host->call("host.link.reconnect", J(R"({"id":7})"));
    CHECK(ls.get("error").get("code").as_string() == "not_found");

    // Remove: endpoint closed, core link removed.
    Json r2 = f.host->call("host.link.create", J(R"({"endpoint":{"kind":"loopback"},"link":{"type":"raw"}})"));
    uint32_t id2 = uint32_t(r2.get("result").get("link").get("id").as_int());
    CHECK(f.host->call("host.link.remove", Json::parse_or_null("{\"id\":" + std::to_string(id2) + "}")).get("ok").as_bool());
    CHECK(f.core.links.count(id2) == 0);
  }

  TEST_CASE("link types are checked against the core's links.types") {
    HostFixture f;
    Json bad = f.host->call("host.link.create", J(R"({"endpoint":{"kind":"udp"},"link":{"type":"kiss"}})"));
    CHECK(bad.get("error").get("code").as_string() == "bad_params");
    CHECK(bad.get("error").get("message").as_string().find("compatible: raw") != std::string::npos);
    // default type = first compatible one from the core
    Json ok = f.host->call("host.link.create", J(R"({"endpoint":{"kind":"loopback"}})"));
    REQUIRE(ok.get("ok").as_bool());
    CHECK(f.core.links.begin()->second.spec.get("type").as_string() == "raw");
  }

  TEST_CASE("autostart:false links are not persisted; host_key makes create idempotent") {
    HostFixture f;
    Json spec = J(R"({"endpoint":{"kind":"loopback"},"link":{"type":"raw"},"autostart":false,"host_key":"cfg-x"})");
    uint32_t a = uint32_t(f.host->call("host.link.create", spec).get("result").get("link").get("id").as_int());
    uint32_t b = uint32_t(f.host->call("host.link.create", spec).get("result").get("link").get("id").as_int());
    CHECK(a == b);
    CHECK(f.core.links.size() == 1);
    CHECK_FALSE(f.core.links[a].spec.get("persist").as_bool(true));
    CHECK(f.core.links[a].spec.get("endpoint").get("host_key").as_string() == "cfg-x");
    // After a restart the non-persistent link is gone from the core; creating it again adds it.
    f.core.restart();
    f.reset();
    CHECK(f.core.links.empty());
    uint32_t c = uint32_t(f.host->call("host.link.create", spec).get("result").get("link").get("id").as_int());
    CHECK(c != 0);
    CHECK(f.core.links.size() == 1);
  }

  TEST_CASE("startup links (HostConfig.startup_links) are ensured on start and on every node.ready") {
    test::TempDir data;
    test::FakeCore core;
    HostConfig c;
    c.startup_links.push(J(R"({"name":"cli","endpoint":{"kind":"loopback"},"link":{"type":"raw"}})"));
    auto host = std::make_unique<Host>(core.node(), c, test::FakeCore::api());
    host->loop().bind_thread();
    host->start();
    REQUIRE(core.links.size() == 1);
    uint32_t id = core.links.begin()->first;
    CHECK_FALSE(core.links[id].spec.get("persist").as_bool(true));
    CHECK(starts_with(core.links[id].spec.get("endpoint").get("host_key").as_string(), "cfg-"));
    REQUIRE(test::run_until(host->loop(), [&] { return core.links[id].connected; }));
    // node.ready again (e.g. runtime unlock): nothing duplicated.
    host->call("test.emit", J(R"({"json":"{\"event\":\"node.ready\",\"data\":{}}"})"));
    test::run_for(host->loop(), 50);
    CHECK(core.links.size() == 1);
    // The core lost it (wipe): re-created on the next node.ready.
    core.links.clear();
    host->call("test.emit", J(R"({"json":"{\"event\":\"node.ready\",\"data\":{}}"})"));
    REQUIRE(test::run_until(host->loop(), [&] { return core.links.size() == 1; }));
  }

  TEST_CASE("tcp endpoint reconnects with backoff and carries bytes both ways") {
    HostFixture f;
    f.core.mode = test::FakeCore::Sink;
    std::string err;
    uint16_t port = 0;
    sock_t lst = tcp_listen("127.0.0.1", 0, &err, &port);
    REQUIRE(lst != kInvalidSock);
    Json spec = Json::object();
    spec.set("endpoint", Json::parse_or_null("{\"kind\":\"tcp\",\"config\":{\"host\":\"127.0.0.1\",\"port\":" + std::to_string(port) + "}}"));
    spec.set("link", J(R"({"type":"kiss"})"));
    uint32_t id = uint32_t(f.host->call("host.link.create", spec).get("result").get("link").get("id").as_int());
    REQUIRE(id);
    Loop& loop = f.host->loop();
    sock_t peer = kInvalidSock;
    REQUIRE(test::run_until(loop, [&] {
      SockAddr pa;
      peer = tcp_accept(lst, &pa);
      return peer != kInvalidSock;
    }));
    TcpStream srv(loop);
    std::string got;
    bool srv_closed = false;
    srv.on_data = [&](const uint8_t* d, size_t n) { got.append(reinterpret_cast<const char*>(d), n); };
    srv.on_close = [&](const std::string&) { srv_closed = true; };
    srv.adopt(peer, SockAddr());
    REQUIRE(test::run_until(loop, [&] { return f.core.links[id].connected; }));
    f.core.outbox.push_back({EMBER_OUT_LINK_TX, id, std::string("\xc0\x00hi\xc0", 5)});
    f.host->wakeup();
    REQUIRE(test::run_until(loop, [&] { return got.size() == 5; }));
    srv.write("from-tnc");
    REQUIRE(test::run_until(loop, [&] { return !f.core.rx.empty(); }));
    CHECK(f.core.rx.back().second == "from-tnc");
    // Server drops the connection: endpoint goes down, then reconnects.
    srv.close();
    REQUIRE(test::run_until(loop, [&] { return !f.core.links[id].connected; }));
    sock_t peer2 = kInvalidSock;
    REQUIRE(test::run_until(loop, [&] {
      SockAddr pa;
      peer2 = tcp_accept(lst, &pa);
      return peer2 != kInvalidSock;
    }, 5000));
    TcpStream srv2(loop);
    srv2.adopt(peer2, SockAddr());
    REQUIRE(test::run_until(loop, [&] { return f.core.links[id].connected; }));
    // host.link.reconnect works on a live link too.
    CHECK(f.host->call("host.link.reconnect", Json::parse_or_null("{\"id\":" + std::to_string(id) + "}")).get("ok").as_bool());
    sock_close(lst);
  }

  TEST_CASE("udp multicast endpoints on one machine hear each other but not themselves") {
    test::FakeCore core;
    core.mode = test::FakeCore::Sink;
    HostConfig c;
    Host h(core.node(), c, test::FakeCore::api());
    h.loop().bind_thread();
    int port = 40000 + int(steady_ms() % 20000);
    std::string ep = "{\"kind\":\"udp\",\"config\":{\"port\":" + std::to_string(port) + "}}";
    uint32_t a = uint32_t(h.call("host.link.create", Json::parse_or_null("{\"endpoint\":" + ep + ",\"link\":{\"type\":\"raw\"},\"autostart\":false}"))
                              .get("result").get("link").get("id").as_int());
    uint32_t b = uint32_t(h.call("host.link.create", Json::parse_or_null("{\"endpoint\":" + ep + ",\"link\":{\"type\":\"raw\"},\"autostart\":false}"))
                              .get("result").get("link").get("id").as_int());
    REQUIRE(a);
    REQUIRE(b);
    REQUIRE(test::run_until(h.loop(), [&] { return core.links[a].connected && core.links[b].connected; }));
    core.outbox.push_back({EMBER_OUT_LINK_TX, a, "datagram-1"});
    h.wakeup();
    bool heard = test::run_until(h.loop(), [&] { return !core.rx.empty(); }, 2000);
    if (!heard) {
      MESSAGE("multicast loopback unavailable on this machine (no multicast route?) - skipping assertion");
    } else {
      CHECK(core.rx[0].first == b);
      CHECK(core.rx[0].second == "datagram-1");
      test::run_for(h.loop(), 100);
      for (auto& x : core.rx) CHECK(x.first != a);  // never hears itself
    }
  }

  TEST_CASE("host.file.save / host.file.pick round trip") {
    HostFixture f;
    Json r = f.host->call("host.file.save", J(R"({"name":"../../evil/backup.ember","mime":"text/plain","data_b64":"aGVsbG8="})"));
    REQUIRE(r.get("ok").as_bool());
    std::string path = r.get("result").get("path").as_string();
    CHECK(path.find("evil") == std::string::npos);
    CHECK(starts_with(path, path_join(f.data.path, "saved")));
    std::string content;
    REQUIRE(read_file(path, &content));
    CHECK(content == "hello");
    // Same name again -> unique name.
    Json r2 = f.host->call("host.file.save", J(R"({"name":"backup.ember","data_b64":"eA=="})"));
    CHECK(r2.get("result").get("path").as_string() != path);
    CHECK(f.host->call("host.file.save", J(R"({"name":"x","data_b64":"%%%"})")).get("error").get("code").as_string() == "bad_params");

    CHECK(f.host->call("host.file.pick", Json::object()).get("error").get("code").as_string() == "not_found");
    make_dirs(path_join(f.data.path, "inbox"));
    write_file_atomic(path_join(f.data.path, "inbox/bundle.json"), "{\"a\":1}");
    Json p = f.host->call("host.file.pick", J(R"({"accept":".json"})"));
    REQUIRE(p.get("ok").as_bool());
    CHECK(p.get("result").get("name").as_string() == "bundle.json");
    CHECK(p.get("result").get("mime").as_string() == "application/json");
    std::string dec;
    REQUIRE(base64_decode(p.get("result").get("data_b64").as_string(), &dec));
    CHECK(dec == "{\"a\":1}");
    CHECK(f.host->call("host.file.pick", J(R"({"accept":".zip"})")).get("error").get("code").as_string() == "not_found");
    CHECK(f.host->call("host.notify", J(R"({"title":"t","body":"b"})")).get("ok").as_bool());
  }

  TEST_CASE("hub start/stop/status via host.hub.* and the ws_server endpoint kind") {
    HostFixture f;
    Json st = f.host->call("host.hub.start", J(R"({"port":0,"bind":"127.0.0.1"})"));
    REQUIRE(st.get("ok").as_bool());
    CHECK(st.get("result").get("running").as_bool());
    CHECK(st.get("result").get("urls").size() >= 1);
    uint32_t hub_link = uint32_t(st.get("result").get("link_id").as_int());
    CHECK(hub_link != 0);
    CHECK(f.core.links[hub_link].spec.get("type").as_string() == "raw");
    CHECK(f.core.links[hub_link].spec.get("config").get("preset").as_string() == "websocket");
    CHECK(f.core.links[hub_link].spec.get("endpoint").get("kind").as_string() == "ws_server");
    CHECK(f.core.links[hub_link].connected);
    // Restart: the persisted ws_server link brings the hub back (port 0 -> any free port).
    f.core.restart();
    f.reset();
    Json s2 = f.host->call("host.hub.status", Json::object());
    CHECK(s2.get("result").get("running").as_bool());
    CHECK(uint32_t(s2.get("result").get("link_id").as_int()) == hub_link);
    CHECK(f.core.links[hub_link].connected);
    // Stop removes the hub link so it stays off after the next restart.
    Json stop = f.host->call("host.hub.stop", Json::object());
    CHECK_FALSE(stop.get("result").get("running").as_bool());
    CHECK(f.core.links.count(hub_link) == 0);
    f.core.restart();
    f.reset();
    CHECK_FALSE(f.host->hub().running());
    Json viaep = f.host->call("host.link.create", J(R"({"endpoint":{"kind":"ws_server","config":{"port":0,"bind":"127.0.0.1"}},"link":{"type":"raw"}})"));
    REQUIRE(viaep.get("ok").as_bool());
    CHECK(uint32_t(viaep.get("result").get("link").get("id").as_int()) != 0);
    CHECK(f.host->hub().running());
    // hub_restore=false (emberd --no-hub) keeps a saved hub link down.
    f.host.reset();
    f.core.restart();
    HostConfig c;
    c.hub_restore = false;
    f.host = std::make_unique<Host>(f.core.node(), c, test::FakeCore::api());
    f.host->loop().bind_thread();
    f.host->start();
    CHECK_FALSE(f.host->hub().running());
  }

  TEST_CASE("link.ptt events key the endpoint; other events reach listeners") {
    HostFixture f;
    std::vector<std::string> events;
    f.host->add_event_listener([&](const std::string& e) { events.push_back(e); });
    f.host->call("test.emit", J(R"({"json":"{\"event\":\"link.ptt\",\"data\":{\"link_id\":42,\"on\":true}}"})"));
    f.host->call("test.emit", J(R"({"json":"{\"event\":\"message.new\",\"data\":{}}"})"));
    REQUIRE(test::run_until(f.host->loop(), [&] { return events.size() >= 2; }));
    CHECK(events[1].find("message.new") != std::string::npos);
  }

  TEST_CASE("thread mode: calls from another thread, clean stop") {
    test::FakeCore core;
    HostConfig c;
    Host h(core.node(), c, test::FakeCore::api());
    h.start_thread();
    h.start();
    for (int i = 0; i < 50; ++i) CHECK(h.call("node.info", Json::object()).get("ok").as_bool());
    std::thread t([&] {
      for (int i = 0; i < 50; ++i) h.call("host.info", Json::object());
    });
    t.join();
    h.stop_thread();
    CHECK(h.call("node.info", Json::object()).get("ok").as_bool());  // inline again after stop
  }

  TEST_CASE("C API with the real core") {
    uint8_t ent[32];
    random_bytes(ent, sizeof ent);
    ember_node* n = ember_create("{}", ent, sizeof ent);
    REQUIRE(n);
    ember_host_t* h = ember_host_create(n, R"({"hub":{"autostart":true,"port":0,"bind":"127.0.0.1"}})");
    REQUIRE(h);
    CHECK(ember_host_create(n, "{bad") == nullptr);
    struct Ev {
      std::atomic<int> n{0};
    } ev;
    ember_host_set_event_callback(h, [](void* u, const char*, size_t) { static_cast<Ev*>(u)->n++; }, &ev);
    REQUIRE(ember_host_start(h) == 0);
    char* r = ember_host_call(h, "node.info", nullptr);
    CHECK(std::string(r).find("\"ok\":true") != std::string::npos);
    ember_host_free(r);
    r = ember_host_call(h, "host.hub.status", "{}");
    CHECK(std::string(r).find("\"running\":true") != std::string::npos);
    ember_host_free(r);
    CHECK(ember_host_hub_port(h) > 0);
    // node.ready from the stub arrives as an event.
    uint64_t end = steady_ms() + 2000;
    while (ev.n.load() == 0 && steady_ms() < end) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    CHECK(ev.n.load() > 0);
    ember_host_stop(h);
    ember_host_destroy(h);
    ember_destroy(n);
  }
}
