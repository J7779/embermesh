#include "doctest.h"
#include "host/http.h"
#include "host/util.h"
#include "test_util.h"

#ifndef _WIN32
#include <unistd.h>
#endif

using namespace ember_host;

namespace {
HttpParser::Result P(const std::string& s, HttpParser& p, size_t* used = nullptr) {
  size_t u = 0;
  auto r = p.parse(s, &u);
  if (used) *used = u;
  return r;
}
int err_of(const std::string& s, HttpLimits l = HttpLimits()) {
  HttpParser p(l);
  return P(s, p) == HttpParser::Result::Error ? p.error_status() : 0;
}
}  // namespace

TEST_SUITE("http") {
  TEST_CASE("simple GET") {
    HttpParser p;
    std::string r = "GET /a%20b/c.html?x=1&y=%41 HTTP/1.1\r\nHost: h\r\nX-Foo:  bar \r\n\r\n";
    size_t used;
    REQUIRE(P(r, p, &used) == HttpParser::Result::Done);
    CHECK(used == r.size());
    CHECK(p.request().method == "GET");
    CHECK(p.request().path == "/a b/c.html");
    CHECK(p.request().query == "x=1&y=%41");
    CHECK(*p.request().header("x-foo") == "bar");
    CHECK(p.request().keep_alive);
  }
  TEST_CASE("keep-alive semantics") {
    HttpParser p;
    REQUIRE(P("GET / HTTP/1.1\r\nHost: h\r\nConnection: close\r\n\r\n", p) == HttpParser::Result::Done);
    CHECK_FALSE(p.request().keep_alive);
    REQUIRE(P("GET / HTTP/1.0\r\n\r\n", p) == HttpParser::Result::Done);
    CHECK_FALSE(p.request().keep_alive);
    REQUIRE(P("GET / HTTP/1.0\r\nConnection: keep-alive\r\n\r\n", p) == HttpParser::Result::Done);
    CHECK(p.request().keep_alive);
  }
  TEST_CASE("partial input needs more, byte by byte") {
    std::string r = "POST /rpc HTTP/1.1\r\nHost: h\r\nContent-Length: 5\r\n\r\nhello";
    for (size_t i = 0; i < r.size(); ++i) {
      HttpParser p;
      CHECK(P(r.substr(0, i), p) == HttpParser::Result::NeedMore);
    }
    HttpParser p;
    REQUIRE(P(r, p) == HttpParser::Result::Done);
    CHECK(p.request().body == "hello");
  }
  TEST_CASE("pipelining consumes exactly one request") {
    std::string a = "GET /1 HTTP/1.1\r\nHost: h\r\n\r\n", b = "GET /2 HTTP/1.1\r\nHost: h\r\n\r\n";
    std::string buf = a + b;
    HttpParser p;
    size_t used;
    REQUIRE(P(buf, p, &used) == HttpParser::Result::Done);
    CHECK(used == a.size());
    CHECK(p.request().path == "/1");
    REQUIRE(P(buf.substr(used), p, &used) == HttpParser::Result::Done);
    CHECK(p.request().path == "/2");
  }
  TEST_CASE("malformed requests") {
    CHECK(err_of("GET / HTTP/1.1\r\n\r\n") == 400);                  // no Host
    CHECK(err_of("GET / HTTP/2.0\r\nHost: h\r\n\r\n") == 505);
    CHECK(err_of("GET / HTTX/1.1\r\nHost: h\r\n\r\n") == 400);
    CHECK(err_of("GET  / HTTP/1.1\r\nHost: h\r\n\r\n") == 400);      // double space
    CHECK(err_of("G(T / HTTP/1.1\r\nHost: h\r\n\r\n") == 400);
    CHECK(err_of("GET / HTTP/1.1\r\nHost: h\r\nBad header\r\n\r\n") == 400);
    CHECK(err_of("GET / HTTP/1.1\r\nHost: h\r\nX : y\r\n\r\n") == 400);   // space before colon
    CHECK(err_of("GET / HTTP/1.1\r\nHost: h\r\nX: a\r\n folded\r\n\r\n") == 400);
    CHECK(err_of("GET / HTTP/1.1\r\nHost: h\r\nX: a\x01" "b\r\n\r\n") == 400);
    CHECK(err_of("GET nopath HTTP/1.1\r\nHost: h\r\n\r\n") == 400);
    CHECK(err_of("GET /%zz HTTP/1.1\r\nHost: h\r\n\r\n") == 400);
    CHECK(err_of("GET /a%00b HTTP/1.1\r\nHost: h\r\n\r\n") == 400);
    CHECK(err_of("POST / HTTP/1.1\r\nHost: h\r\nContent-Length: x\r\n\r\n") == 400);
    CHECK(err_of("POST / HTTP/1.1\r\nHost: h\r\nContent-Length: 1\r\nContent-Length: 2\r\n\r\n") == 400);
    CHECK(err_of("POST / HTTP/1.1\r\nHost: h\r\nTransfer-Encoding: chunked\r\n\r\n") == 501);
    CHECK(err_of("POST / HTTP/1.1\r\nHost: h\r\nTransfer-Encoding: chunked\r\nContent-Length: 3\r\n\r\n") == 400);
    HttpLimits small;
    small.max_body = 10;
    CHECK(err_of("POST / HTTP/1.1\r\nHost: h\r\nContent-Length: 11\r\n\r\n", small) == 413);
    CHECK(err_of("GET * HTTP/1.1\r\nHost: h\r\n\r\n") == 400);
  }
  TEST_CASE("huge headers, long URIs, too many headers") {
    HttpLimits l;
    std::string big = "GET / HTTP/1.1\r\nHost: h\r\nX: " + std::string(20000, 'a') + "\r\n\r\n";
    CHECK(err_of(big, l) == 431);
    // Without the terminating blank line the parser still refuses once the limit is exceeded.
    std::string unterminated = "GET / HTTP/1.1\r\nHost: h\r\nX: " + std::string(20000, 'a');
    CHECK(err_of(unterminated, l) == 431);
    CHECK(err_of("GET /" + std::string(9000, 'a') + " HTTP/1.1\r\nHost: h\r\n\r\n") == 414);
    CHECK(err_of(std::string(20000, 'A')) == 414);
    std::string many = "GET / HTTP/1.1\r\nHost: h\r\n";
    for (int i = 0; i < 100; ++i) many += "X" + std::to_string(i) + ": v\r\n";
    CHECK(err_of(many + "\r\n") == 431);
  }
  TEST_CASE("lenient forms: bare LF, leading CRLF, absolute-form, OPTIONS *") {
    HttpParser p;
    REQUIRE(P("\r\nGET /x HTTP/1.1\nHost: h\n\n", p) == HttpParser::Result::Done);
    CHECK(p.request().path == "/x");
    REQUIRE(P("GET http://example/a/b?q HTTP/1.1\r\nHost: h\r\n\r\n", p) == HttpParser::Result::Done);
    CHECK(p.request().path == "/a/b");
    CHECK(p.request().query == "q");
    REQUIRE(P("OPTIONS * HTTP/1.1\r\nHost: h\r\n\r\n", p) == HttpParser::Result::Done);
  }
  TEST_CASE("mime types") {
    CHECK(mime_type_for("/a/app.wasm") == "application/wasm");
    CHECK(mime_type_for("x.mjs") == "text/javascript; charset=utf-8");
    CHECK(mime_type_for("index.HTML") == "text/html; charset=utf-8");
    CHECK(mime_type_for("m.webmanifest") == "application/manifest+json");
    CHECK(mime_type_for("a.apk") == "application/vnd.android.package-archive");
    CHECK(mime_type_for("noext") == "application/octet-stream");
    CHECK(mime_type_for("dir.d/noext") == "application/octet-stream");
  }
  TEST_CASE("range parsing") {
    uint64_t s, l;
    CHECK(parse_range("bytes=0-9", 100, &s, &l) == 1);
    CHECK((s == 0 && l == 10));
    CHECK(parse_range("bytes=90-", 100, &s, &l) == 1);
    CHECK((s == 90 && l == 10));
    CHECK(parse_range("bytes=-5", 100, &s, &l) == 1);
    CHECK((s == 95 && l == 5));
    CHECK(parse_range("bytes=-500", 100, &s, &l) == 1);
    CHECK((s == 0 && l == 100));
    CHECK(parse_range("bytes=50-1000", 100, &s, &l) == 1);
    CHECK(l == 50);
    CHECK(parse_range("bytes=100-", 100, &s, &l) == -1);
    CHECK(parse_range("bytes=-0", 100, &s, &l) == -1);
    CHECK(parse_range("bytes=5-1", 100, &s, &l) == 0);
    CHECK(parse_range("bytes=0-1,5-6", 100, &s, &l) == 0);
    CHECK(parse_range("items=0-1", 100, &s, &l) == 0);
    CHECK(parse_range("bytes=x-1", 100, &s, &l) == 0);
    CHECK(parse_range("bytes=0-0", 0, &s, &l) == -1);
  }
  TEST_CASE("static path resolution refuses traversal") {
    test::TempDir root, outside;
    root.file("index.html", "<h1>hi</h1>");
    root.file("js/app.js", "x");
    root.file(".secret", "no");
    outside.file("passwd", "secret");
    bool isdir = false;
    CHECK(resolve_static_path(root.path, "/", &isdir) == ember_host::path_join(root.path, "index.html"));
    CHECK(isdir);
    CHECK(resolve_static_path(root.path, "/js/app.js") == ember_host::path_join(ember_host::path_join(root.path, "js"), "app.js"));
    CHECK(resolve_static_path(root.path, "/js//./app.js") != "");
    CHECK(resolve_static_path(root.path, "/../" + outside.path.substr(outside.path.find_last_of("/\\") + 1) + "/passwd") == "");
    CHECK(resolve_static_path(root.path, "/js/../../passwd") == "");
    CHECK(resolve_static_path(root.path, "/..") == "");
    CHECK(resolve_static_path(root.path, "/.secret") == "");
    CHECK(resolve_static_path(root.path, "/js\\..\\index.html") == "");
    CHECK(resolve_static_path(root.path, "/C:/x") == "");
    CHECK(resolve_static_path(root.path, "relative") == "");
    CHECK(resolve_static_path(root.path, "/missing.js") == "");
    CHECK(resolve_static_path(root.path, std::string("/a\0b", 4)) == "");
#ifndef _WIN32
    // A symlink inside the root that points outside it must not be followed.
    std::string link = ember_host::path_join(root.path, "escape");
    REQUIRE(::symlink(outside.path.c_str(), link.c_str()) == 0);
    CHECK(resolve_static_path(root.path, "/escape/passwd") == "");
#endif
  }
}
