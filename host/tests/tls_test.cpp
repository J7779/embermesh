// SPDX-License-Identifier: MIT
// wss:// through the host's TLS client against a local TLS-terminating proxy. A test CA and the
// server certificates are generated at test time in the temp dir (openssl CLI); nothing secret is
// committed. Checks: a valid chain works; a wrong host name, an untrusted CA, an expired
// certificate and a certificate not in the system store are all refused. Needs `openssl` and
// `python3` on PATH (skips with a message otherwise). POSIX only for now.
#if defined(EMBER_HOST_HAVE_TLS) && !defined(_WIN32)
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "doctest.h"
#include "host/loop.h"
#include "host/server.h"
#include "host/tls.h"
#include "host/util.h"
#include "host/ws_client.h"
#include "test_util.h"

extern char** environ;

using namespace ember_host;

namespace {

bool sh(const std::string& cmd) { return std::system((cmd + " >/dev/null 2>&1").c_str()) == 0; }

struct Proxy {
  pid_t pid = -1;
  uint16_t port = 0;
  Proxy(const std::string& dir, const std::string& cert, const std::string& key, uint16_t backend) {
    std::string pf = path_join(dir, "port-" + random_hex(4));
    std::vector<std::string> args = {"python3", EMBER_HOST_TEST_DIR "/tls_proxy.py", cert, key, std::to_string(backend), pf};
    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(&a[0]);
    argv.push_back(nullptr);
    if (posix_spawnp(&pid, "python3", nullptr, nullptr, argv.data(), environ) != 0) {
      pid = -1;
      return;
    }
    uint64_t end = steady_ms() + 10000;
    while (steady_ms() < end) {
      std::string s;
      if (read_file(pf, &s) && !s.empty()) {
        port = uint16_t(std::atoi(s.c_str()));
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }
  ~Proxy() {
    if (pid > 0) {
      ::kill(pid, SIGTERM);
      int st;
      ::waitpid(pid, &st, 0);
    }
  }
};

struct Result {
  bool open = false, closed = false;
  std::string reason, echoed;
};

Result try_wss(Loop& loop, const std::string& url, const std::string& ca_file) {
  WsClient::Options o;
  o.tls_ca_file_for_tests = ca_file;
  o.connect_timeout_ms = 5000;
  WsClient c(loop, o);
  Result r;
  c.on_open = [&] {
    r.open = true;
    c.send_binary("ping-over-tls", 13);
  };
  c.on_message = [&](uint8_t, std::string& d) { r.echoed = d; };
  c.on_close = [&](uint16_t, const std::string& why) {
    r.closed = true;
    r.reason = why;
  };
  std::string err;
  if (!c.connect(url, &err)) {
    r.closed = true;
    r.reason = err;
    return r;
  }
  test::run_until(loop, [&] { return r.closed || !r.echoed.empty(); }, 10000);
  c.abort();
  return r;
}

}  // namespace

TEST_SUITE("tls") {
  TEST_CASE("wss:// validates the chain, the host name and the dates") {
    if (!sh("openssl version") || !sh("python3 -c pass")) {
      MESSAGE("openssl or python3 not available: skipping");
      return;
    }
    test::TempDir tmp;
    std::string d = tmp.path;
    auto P = [&](const char* f) { return path_join(d, f); };
    auto ec_key = [&](const char* name) {
      return sh("openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:prime256v1 -out '" + P(name) + "'");
    };
    // Two independent CAs, valid now.
    REQUIRE(ec_key("ca.key"));
    REQUIRE(sh("openssl req -x509 -new -key '" + P("ca.key") + "' -out '" + P("ca.pem") + "' -subj '/CN=EmberMesh test CA' -days 2"));
    REQUIRE(ec_key("other.key"));
    REQUIRE(sh("openssl req -x509 -new -key '" + P("other.key") + "' -out '" + P("other.pem") + "' -subj '/CN=Other CA' -days 2"));
    auto leaf = [&](const std::string& name, const std::string& dns, const std::string& extra_dates) {
      std::string key = P((name + ".key").c_str()), csr = P((name + ".csr").c_str()), pem = P((name + ".pem").c_str());
      std::string ext = P((name + ".ext").c_str());
      write_file_atomic(ext, "subjectAltName=DNS:" + dns + "\nbasicConstraints=CA:FALSE\nextendedKeyUsage=serverAuth\n");
      if (!ec_key((name + ".key").c_str())) return false;
      if (!sh("openssl req -new -key '" + key + "' -out '" + csr + "' -subj '/CN=" + dns + "'")) return false;
      return sh("openssl x509 -req -in '" + csr + "' -CA '" + P("ca.pem") + "' -CAkey '" + P("ca.key") + "' -CAcreateserial -out '" +
                pem + "' -extfile '" + ext + "' " + extra_dates);
    };
    REQUIRE(leaf("good", "localhost", "-days 2"));
    REQUIRE(leaf("wrongname", "relay.example.org", "-days 2"));
    bool have_expired = leaf("expired", "localhost", "-not_before 20200101000000Z -not_after 20200201000000Z");
    if (!have_expired) MESSAGE("this openssl cannot backdate a certificate (-not_before): the expired-certificate case is skipped");

    // Backend: a plain WebSocket echo server, as a relay behind its reverse proxy.
    Loop loop;
    loop.bind_thread();
    HttpServer::Options so;
    so.bind = "127.0.0.1";
    so.port = 0;
    HttpServer srv(loop, so);
    HttpServer::WsRoute echo;
    echo.on_message = [](WsConn& c, uint8_t op, std::string& data) { c.send_binary(data.data(), data.size()); (void)op; };
    srv.add_ws_route("/echo", echo);
    std::string err;
    REQUIRE(srv.start(&err));

    Proxy good(d, P("good.pem"), P("good.key"), srv.port());
    REQUIRE(good.port);
    std::string url = "wss://localhost:" + std::to_string(good.port) + "/echo";

    Result ok = try_wss(loop, url, P("ca.pem"));
    CHECK_MESSAGE(ok.echoed == "ping-over-tls", ok.reason);

    Result untrusted = try_wss(loop, url, P("other.pem"));
    CHECK_FALSE(untrusted.open);
    CHECK(untrusted.reason.find("certificate") != std::string::npos);

    Result system_store = try_wss(loop, url, "");  // our test CA is in no system store
    CHECK_FALSE(system_store.open);

    Proxy wrong(d, P("wrongname.pem"), P("wrongname.key"), srv.port());
    REQUIRE(wrong.port);
    Result bad_name = try_wss(loop, "wss://localhost:" + std::to_string(wrong.port) + "/echo", P("ca.pem"));
    CHECK_FALSE(bad_name.open);
    CHECK_MESSAGE(bad_name.reason.find("certificate") != std::string::npos, bad_name.reason);

    if (have_expired) {
      Proxy exp(d, P("expired.pem"), P("expired.key"), srv.port());
      REQUIRE(exp.port);
      Result expired = try_wss(loop, "wss://localhost:" + std::to_string(exp.port) + "/echo", P("ca.pem"));
      CHECK_FALSE(expired.open);
      CHECK_MESSAGE(expired.reason.find("expired") != std::string::npos, expired.reason);
    }
    srv.stop();
  }

  TEST_CASE("the platform trust store provides anchors") {
    MESSAGE("system trust anchors found: " << TlsClient::system_anchor_count());
    CHECK(TlsClient::system_anchor_count() > 0);
  }

  TEST_CASE("a plain server on a wss:// URL fails cleanly") {
    Loop loop;
    loop.bind_thread();
    HttpServer::Options so;
    so.bind = "127.0.0.1";
    so.port = 0;
    HttpServer srv(loop, so);
    std::string err;
    REQUIRE(srv.start(&err));
    Result r = try_wss(loop, "wss://localhost:" + std::to_string(srv.port()) + "/x", "");
    CHECK_FALSE(r.open);
    CHECK(r.closed);
    srv.stop();
  }
}
#endif
