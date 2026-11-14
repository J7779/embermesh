// Integration tests: start the hub on an ephemeral port inside a Host with a fake core and drive it
// with our own HTTP / WebSocket client code on the same loop.
#include "doctest.h"
#include "host/host.h"
#include "host/stream.h"
#include "host/ws_client.h"
#include "test_util.h"

using namespace ember_host;
using test::Json;

namespace {

// Raw HTTP exchange: sends `request` (optionally in small chunks) and collects bytes until the
// server closes or `until` matches.
struct RawClient {
  Loop& loop;
  TcpStream s;
  std::string got;
  bool closed = false, connected = false;
  std::string cerr;
  explicit RawClient(Loop& l) : loop(l), s(l) {
    s.on_connect = [this](const std::string& e) {
      cerr = e;
      connected = e.empty();
      if (!e.empty()) closed = true;
    };
    s.on_data = [this](const uint8_t* d, size_t n) { got.append(reinterpret_cast<const char*>(d), n); };
    s.on_close = [this](const std::string&) { closed = true; };
  }
  bool connect(uint16_t port) {
    s.connect("127.0.0.1", port, 2000);
    return test::run_until(loop, [&] { return connected || closed; }) && connected;
  }
};

std::string http_get(Loop& loop, uint16_t port, const std::string& req, bool wait_close = true, size_t min_bytes = 1) {
  RawClient c(loop);
  if (!c.connect(port)) return "CONNECT FAILED";
  c.s.write(req);
  test::run_until(loop, [&] { return wait_close ? c.closed : c.got.size() >= min_bytes; }, 3000);
  return c.got;
}

struct HubFixture {
  test::TempDir web, data, dl;
  test::FakeCore core;
  std::unique_ptr<Host> host;
  uint16_t port = 0;
  explicit HubFixture(bool local_no_token = false) {
    web.file("index.html", "<!doctype html><title>ui</title>");
    web.file("app.wasm", std::string("\0asm\1\0\0\0", 8) + std::string(1000, 'w'));
    web.file("js/main.mjs", "export default 1;");
    dl.file("embermesh.html", "<html>single file</html>");
    dl.file("embermesh.apk", "PK-apk");
    HostConfig c;
    c.data_dir = data.path;
    c.web_root = web.path;
    c.download_dirs = {dl.path};
    c.download_files = {web.file("single.html", "<html>one</html>")};
    c.admin_token = "s3cret-token";
    c.local_no_token = local_no_token;
    c.hub_port = 0;
    c.hub_bind = "127.0.0.1";
    c.hub_autostart = true;
    host = std::make_unique<Host>(core.node(), c, test::FakeCore::api());
    host->loop().bind_thread();
    host->start();
    port = host->hub().port();
  }
  Loop& loop() { return host->loop(); }
};

}  // namespace

TEST_SUITE("hub") {
  TEST_CASE("serves the web app with correct MIME types, ranges, HEAD, 304") {
    HubFixture f;
    REQUIRE(f.port != 0);
    std::string r = http_get(f.loop(), f.port, "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(r.find("HTTP/1.1 200 OK") == 0);
    CHECK(r.find("Content-Type: text/html; charset=utf-8") != std::string::npos);
    CHECK(r.find("<title>ui</title>") != std::string::npos);

    r = http_get(f.loop(), f.port, "GET /app.wasm HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(r.find("Content-Type: application/wasm") != std::string::npos);
    CHECK(r.find("Content-Length: 1008") != std::string::npos);
    r = http_get(f.loop(), f.port, "GET /js/main.mjs HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(r.find("Content-Type: text/javascript") != std::string::npos);

    r = http_get(f.loop(), f.port, "GET /app.wasm HTTP/1.1\r\nHost: x\r\nRange: bytes=0-3\r\nConnection: close\r\n\r\n");
    CHECK(r.find("HTTP/1.1 206") == 0);
    CHECK(r.find("Content-Range: bytes 0-3/1008") != std::string::npos);
    CHECK(r.substr(r.size() - 4) == std::string("\0asm", 4));
    r = http_get(f.loop(), f.port, "GET /app.wasm HTTP/1.1\r\nHost: x\r\nRange: bytes=5000-\r\nConnection: close\r\n\r\n");
    CHECK(r.find("HTTP/1.1 416") == 0);

    r = http_get(f.loop(), f.port, "HEAD /app.wasm HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(r.find("Content-Length: 1008") != std::string::npos);
    CHECK(r.find(std::string("\0asm", 4)) == std::string::npos);

    size_t et = r.find("ETag: ");
    REQUIRE(et != std::string::npos);
    std::string etag = r.substr(et + 6, r.find("\r\n", et) - et - 6);
    r = http_get(f.loop(), f.port, "GET /app.wasm HTTP/1.1\r\nHost: x\r\nIf-None-Match: " + etag + "\r\nConnection: close\r\n\r\n");
    CHECK(r.find("HTTP/1.1 304") == 0);
  }

  TEST_CASE("traversal, malformed and oversized requests") {
    HubFixture f;
    std::string r = http_get(f.loop(), f.port, "GET /../../etc/passwd HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(r.find("HTTP/1.1 404") == 0);
    r = http_get(f.loop(), f.port, "GET /%2e%2e/%2e%2e/etc/passwd HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(r.find("HTTP/1.1 404") == 0);
    r = http_get(f.loop(), f.port, "GET /..%5c..%5cwindows HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(r.find("HTTP/1.1 404") == 0);
    r = http_get(f.loop(), f.port, "BLAH\r\n\r\n");
    CHECK(r.find("HTTP/1.1 400") == 0);
    r = http_get(f.loop(), f.port, "GET / HTTP/1.1\r\nHost: x\r\nX: " + std::string(40000, 'a') + "\r\n\r\n");
    CHECK(r.find("HTTP/1.1 431") == 0);
    r = http_get(f.loop(), f.port, "DELETE / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(r.find("HTTP/1.1 405") == 0);
    r = http_get(f.loop(), f.port, "GET /ws HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(r.find("HTTP/1.1 426") == 0);
  }

  TEST_CASE("pipelining and byte-by-byte partial reads") {
    HubFixture f;
    std::string two = "GET /js/main.mjs HTTP/1.1\r\nHost: x\r\n\r\nGET /hub.json HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
    std::string r = http_get(f.loop(), f.port, two);
    size_t a = r.find("export default 1;"), b = r.find("\"ember_hub\":true");
    CHECK(a != std::string::npos);
    CHECK(b != std::string::npos);
    CHECK(a < b);

    RawClient c(f.loop());
    REQUIRE(c.connect(f.port));
    std::string req = "GET /hub.json HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
    for (char ch : req) {
      c.s.write(&ch, 1);
      test::run_for(f.loop(), 1);
    }
    test::run_until(f.loop(), [&] { return c.closed; });
    CHECK(c.got.find("HTTP/1.1 200") == 0);
  }

  TEST_CASE("hub.json probe and downloads") {
    HubFixture f;
    std::string r = http_get(f.loop(), f.port, "GET /hub.json HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    Json j = Json::parse_or_null(r.substr(r.find("\r\n\r\n") + 4));
    CHECK(j.get("embermesh_hub").as_bool());
    CHECK(j.get("ws").as_string() == "/ws");
    CHECK(j.get("name").as_string() == "fake");
    CHECK(r.find("Cache-Control: no-store") != std::string::npos);
    CHECK(j.get("node_id").as_string() == "00112233aabbccdd");
    CHECK(r.find("Access-Control-Allow-Origin: *") != std::string::npos);
    r = http_get(f.loop(), f.port, "GET /download/ HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(r.find("embermesh.apk") != std::string::npos);
    r = http_get(f.loop(), f.port, "GET /download/?format=json HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    Json l = Json::parse_or_null(r.substr(r.find("\r\n\r\n") + 4));
    CHECK(l.get("files").size() == 3);
    r = http_get(f.loop(), f.port, "GET /download/single.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(r.find("<html>one</html>") != std::string::npos);
    r = http_get(f.loop(), f.port, "GET /download/app.wasm HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(r.find("HTTP/1.1 404") == 0);  // web root files are not downloads
    r = http_get(f.loop(), f.port, "GET /download/embermesh.apk HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(r.find("application/vnd.android.package-archive") != std::string::npos);
    CHECK(r.find("Content-Disposition: attachment; filename=\"embermesh.apk\"") != std::string::npos);
    r = http_get(f.loop(), f.port, "GET /download/../index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(r.find("HTTP/1.1 404") == 0);
  }

  TEST_CASE("fallback page when no web root") {
    test::FakeCore core;
    HostConfig c;
    c.hub_port = 0;
    c.hub_bind = "127.0.0.1";
    c.hub_autostart = true;
    Host h(core.node(), c, test::FakeCore::api());
    h.loop().bind_thread();
    h.start();
    std::string r = http_get(h.loop(), h.hub().port(), "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(r.find("EmberMesh hub") != std::string::npos);
  }

  TEST_CASE("mesh bus: two guests, frames relayed through the core") {
    HubFixture f;
    REQUIRE(f.host->hub_link_id() != 0);
    WsClient a(f.loop()), b(f.loop());
    bool ao = false, bo = false;
    std::vector<std::string> ga, gb;
    a.on_open = [&] { ao = true; };
    b.on_open = [&] { bo = true; };
    a.on_message = [&](uint8_t op, std::string& d) {
      if (op == kWsBinary) ga.push_back(d);
    };
    b.on_message = [&](uint8_t op, std::string& d) {
      if (op == kWsBinary) gb.push_back(d);
    };
    std::string url = "ws://127.0.0.1:" + std::to_string(f.port) + "/ws";
    REQUIRE(a.connect(url));
    REQUIRE(b.connect(url));
    REQUIRE(test::run_until(f.loop(), [&] { return ao && bo; }));
    CHECK(f.host->hub().guest_count() == 2);
    std::string frame("\x01\x02\x00\xff mesh", 9);
    a.send_binary(frame.data(), frame.size());
    REQUIRE(test::run_until(f.loop(), [&] { return !gb.empty(); }));
    CHECK(gb[0] == frame);
    // The core saw it on the hub link.
    REQUIRE(!f.core.rx.empty());
    CHECK(f.core.rx[0].first == f.host->hub_link_id());
    CHECK(f.core.rx[0].second == frame);
    // Text frames are not mesh frames: ignored.
    size_t before = f.core.rx.size();
    a.send_text("hello");
    test::run_for(f.loop(), 50);
    CHECK(f.core.rx.size() == before);
    // Oversized frame: the guest is disconnected with 1009.
    uint16_t code = 0;
    a.on_close = [&](uint16_t c, const std::string&) { code = c; };
    std::string huge(4097, 'h');
    a.send_binary(huge.data(), huge.size());
    REQUIRE(test::run_until(f.loop(), [&] { return code != 0; }));
    CHECK(code == kWsCloseTooBig);
    REQUIRE(test::run_until(f.loop(), [&] { return f.host->hub().guest_count() == 1; }));
  }

  TEST_CASE("one flooding guest cannot starve the others (fair queuing, shared budget, eviction)") {
    HubFixture f;
    f.core.mode = test::FakeCore::Sink;
    WsClient bad(f.loop()), good(f.loop());
    bool bo = false, go = false;
    uint16_t bad_close = 0;
    bad.on_open = [&] { bo = true; };
    good.on_open = [&] { go = true; };
    bad.on_close = [&](uint16_t c, const std::string&) { bad_close = c; };
    std::string url = "ws://127.0.0.1:" + std::to_string(f.port) + "/ws";
    REQUIRE(bad.connect(url));
    REQUIRE(good.connect(url));
    REQUIRE(test::run_until(f.loop(), [&] { return bo && go; }));
    // The flooder sends 6000 frames as fast as it can; the honest guest 20 frames at 20/s.
    std::string junk(200, 'x');
    for (int i = 0; i < 6000 && bad.is_open(); ++i) {
      bad.send_binary(junk.data(), junk.size());
      if (i % 200 == 0) test::run_for(f.loop(), 1);
    }
    for (int i = 0; i < 20; ++i) {
      std::string m = "good-" + std::to_string(i);
      good.send_binary(m.data(), m.size());
      test::run_for(f.loop(), 50);
    }
    auto good_seen = [&] {
      size_t n = 0;
      for (auto& r : f.core.rx) n += r.second.compare(0, 5, "good-") == 0;
      return n;
    };
    REQUIRE(test::run_until(f.loop(), [&] { return good_seen() == 20; }, 5000));
    // The core never saw more than the shared budget allows (300/s plus the initial burst).
    size_t bad_in = f.core.rx.size() - good_seen();
    CHECK(bad_in < 1500);
    // The flooder overflowed its budget by far more than 2000 frames: evicted with 1008.
    REQUIRE(test::run_until(f.loop(), [&] { return bad_close != 0; }, 5000));
    CHECK(bad_close == kWsClosePolicy);
    CHECK(good.is_open());
    Json st = f.host->hub().stats();
    CHECK(st.get("guests_abusive").as_int() == 1);
    CHECK(st.get("frames_rate_limited").as_int() > 2000);
  }

  TEST_CASE("websocket echo through the core (echo mode)") {
    HubFixture f;
    f.core.mode = test::FakeCore::Echo;
    WsClient a(f.loop());
    bool open = false;
    std::string got;
    a.on_open = [&] { open = true; };
    a.on_message = [&](uint8_t, std::string& d) { got = d; };
    REQUIRE(a.connect("ws://127.0.0.1:" + std::to_string(f.port) + "/ws"));
    REQUIRE(test::run_until(f.loop(), [&] { return open; }));
    std::string big(4096, 'e');  // the raw `websocket` preset mtu is the largest accepted message
    a.send_binary(big.data(), big.size());
    REQUIRE(test::run_until(f.loop(), [&] { return !got.empty(); }));
    CHECK(got == big);
  }

  TEST_CASE("/rpc requires the admin token") {
    HubFixture f;
    auto try_connect = [&](const std::string& q, std::string* reason) {
      WsClient c(f.loop());
      int result = 0;  // 1 open, -1 closed
      c.on_open = [&] { result = 1; };
      c.on_close = [&](uint16_t, const std::string& r) {
        result = -1;
        *reason = r;
      };
      c.connect("ws://127.0.0.1:" + std::to_string(f.port) + "/rpc" + q);
      test::run_until(f.loop(), [&] { return result != 0; });
      return result;
    };
    std::string why;
    CHECK(try_connect("", &why) == -1);
    CHECK(why.find("401") != std::string::npos);
    CHECK(try_connect("?token=wrong", &why) == -1);
    CHECK(try_connect("?token=s3cret-token", &why) == 1);

    // Request / reply / event push.
    WsClient c(f.loop());
    bool open = false;
    std::vector<Json> msgs;
    c.on_open = [&] { open = true; };
    c.on_message = [&](uint8_t op, std::string& d) {
      if (op == kWsText) msgs.push_back(Json::parse_or_null(d));
    };
    c.connect("ws://127.0.0.1:" + std::to_string(f.port) + "/rpc?token=s3cret-token");
    REQUIRE(test::run_until(f.loop(), [&] { return open; }));
    c.send_text(R"({"id":7,"method":"node.info","params":{}})");
    c.send_text(R"({"id":8,"method":"host.info"})");
    c.send_text(R"({"id":9,"method":"no.such"})");
    c.send_text("not json");
    REQUIRE(test::run_until(f.loop(), [&] { return msgs.size() >= 4; }));
    CHECK(msgs[0].get("id").as_int() == 7);
    CHECK(msgs[0].get("ok").as_bool());
    CHECK(msgs[0].get("result").get("node_id").as_string() == "00112233aabbccdd");
    CHECK(msgs[1].get("result").get("backend").as_string() == "remote");
    CHECK(msgs[2].get("ok").as_bool() == false);
    CHECK(msgs[2].get("error").get("code").as_string() == "not_implemented");
    CHECK(msgs[3].get("error").get("code").as_string() == "bad_params");
    // Core events are pushed to remote sessions.
    f.host->call("test.emit", Json::parse_or_null(R"({"json":"{\"event\":\"message.new\",\"data\":{\"x\":1}}"})"));
    REQUIRE(test::run_until(f.loop(), [&] {
      for (auto& m : msgs)
        if (m.get("event").as_string() == "message.new") return true;
      return false;
    }));
  }

  TEST_CASE("POST /rpc with bearer token, and local_no_token origin rules") {
    HubFixture f(true);
    std::string body = R"({"method":"node.info","params":{}})";
    std::string req = "POST /rpc HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) +
                      "\r\nConnection: close\r\n";
    std::string r = http_get(f.loop(), f.port, req + "Authorization: Bearer s3cret-token\r\n\r\n" + body);
    CHECK(r.find("\"node_id\":\"00112233aabbccdd\"") != std::string::npos);
    // Loopback without a token is allowed (configured) ...
    r = http_get(f.loop(), f.port, req + "\r\n" + body);
    CHECK(r.find("HTTP/1.1 200") == 0);
    // ... but not from a foreign web page's Origin.
    r = http_get(f.loop(), f.port, req + "Origin: http://evil.example\r\n\r\n" + body);
    CHECK(r.find("HTTP/1.1 403") == 0);
    r = http_get(f.loop(), f.port, req + "Origin: http://localhost:8437\r\n\r\n" + body);
    CHECK(r.find("HTTP/1.1 200") == 0);
    // Malformed body.
    std::string bad = "{nope";
    r = http_get(f.loop(), f.port,
                 "POST /rpc HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\nConnection: close\r\nAuthorization: Bearer s3cret-token\r\n\r\n" + bad);
    CHECK(r.find("HTTP/1.1 400") == 0);
  }

  TEST_CASE("slow client: header timeout and write stall don't hang the hub") {
    test::FakeCore core;
    HostConfig c;
    c.hub_port = 0;
    c.hub_bind = "127.0.0.1";
    c.hub_autostart = true;
    Host h(core.node(), c, test::FakeCore::api());
    h.loop().bind_thread();
    h.start();
    // A client that sends half a request line and stops.
    RawClient slow(h.loop());
    REQUIRE(slow.connect(h.hub().port()));
    slow.s.write("GET / HT");
    // The hub keeps serving others meanwhile.
    std::string r = http_get(h.loop(), h.hub().port(), "GET /hub.json HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(r.find("HTTP/1.1 200") == 0);
    CHECK_FALSE(slow.closed);
  }
}

#include "ember_hub.h"

TEST_SUITE("hub_capi") {
  TEST_CASE("ember_hub.h: hub-only embedding without a node") {
    struct Rx {
      std::mutex mu;
      std::vector<std::string> frames;
      std::atomic<size_t> clients{0};
    } rx;
    ember_hub* h = ember_hub_create(
        [](void* u, const uint8_t* d, size_t n) {
          auto* r = static_cast<Rx*>(u);
          std::lock_guard<std::mutex> lk(r->mu);
          r->frames.emplace_back(reinterpret_cast<const char*>(d), n);
        },
        [](void* u, size_t g) { static_cast<Rx*>(u)->clients = g; }, &rx);
    REQUIRE(h);
    char err[128];
    CHECK(ember_hub_start(h, "{bad", err, sizeof err) != 0);
    CHECK(std::string(err).find("JSON") != std::string::npos);
    REQUIRE(ember_hub_start(h, R"({"port":0,"bind":"127.0.0.1","name":"Ann","node_id":"0011223344556677"})", err, sizeof err) == 0);
    char* st = ember_hub_status(h);
    Json s = Json::parse_or_null(st);
    ember_hub_free(st);
    CHECK(s.get("running").as_bool());
    uint16_t port = uint16_t(s.get("port").as_int());
    REQUIRE(port != 0);
    ember_hub_set_probe(h, R"({"name":"Bob"})");
    Loop loop;
    loop.bind_thread();
    std::string probe = http_get(loop, port, "GET /hub.json HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n");
    CHECK(probe.find("\"name\":\"Bob\"") != std::string::npos);
    CHECK(probe.find("0011223344556677") != std::string::npos);
    // /rpc is not offered.
    std::string r = http_get(loop, port, "POST /rpc HTTP/1.1\r\nHost: x\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}");
    CHECK(r.find("HTTP/1.1 401") == 0);
    WsClient c(loop);
    bool open = false;
    std::vector<std::string> got;
    c.on_open = [&] { open = true; };
    c.on_message = [&](uint8_t op, std::string& d) {
      if (op == kWsBinary) got.push_back(d);
    };
    REQUIRE(c.connect("ws://127.0.0.1:" + std::to_string(port) + "/ws"));
    REQUIRE(test::run_until(loop, [&] { return open && rx.clients.load() == 1; }));
    c.send_binary("guest-frame", 11);
    REQUIRE(test::run_until(loop, [&] {
      std::lock_guard<std::mutex> lk(rx.mu);
      return !rx.frames.empty();
    }));
    CHECK(rx.frames[0] == "guest-frame");
    ember_hub_broadcast(h, reinterpret_cast<const uint8_t*>("core-frame"), 10);
    REQUIRE(test::run_until(loop, [&] { return !got.empty(); }));
    CHECK(got[0] == "core-frame");
    ember_hub_stop(h);
    st = ember_hub_status(h);
    CHECK(Json::parse_or_null(st).get("running").as_bool(true) == false);
    ember_hub_free(st);
    ember_hub_destroy(h);
    ember_hub_destroy(nullptr);
  }
}
