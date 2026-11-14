#include "doctest.h"
#include "host/endpoint.h"
#include "host/http.h"
#include "host/loop.h"
#include "host/util.h"
#include "host/ws.h"

using namespace ember_host;

namespace {
std::string B(std::initializer_list<int> v) {
  std::string s;
  for (int x : v) s += char(x);
  return s;
}
WsEvent feed_one(WsDecoder& d, const std::string& bytes) {
  d.feed(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
  return d.next();
}
WsDecoder server_dec(size_t max = 1 << 20) {
  WsDecoder::Options o;
  o.expect_masked = true;
  o.max_message = max;
  return WsDecoder(o);
}
WsDecoder client_dec(size_t max = 1 << 20) {
  WsDecoder::Options o;
  o.expect_masked = false;
  o.max_message = max;
  return WsDecoder(o);
}
const uint8_t kMask[4] = {0x37, 0xfa, 0x21, 0x3d};
std::string masked(uint8_t op, const std::string& p, bool fin = true) { return ws_encode_frame(op, p.data(), p.size(), fin, kMask); }
}  // namespace

TEST_SUITE("websocket") {
  TEST_CASE("RFC 6455 section 5.7 examples") {
    // Single-frame unmasked text "Hello"
    CHECK(ws_encode_frame(kWsText, "Hello", 5) == B({0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f}));
    // Single-frame masked text "Hello"
    CHECK(masked(kWsText, "Hello") == B({0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d, 0x51, 0x58}));
    {
      auto d = client_dec();
      WsEvent e = feed_one(d, B({0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f}));
      CHECK(e.kind == WsEvent::Message);
      CHECK(e.opcode == kWsText);
      CHECK(e.payload == "Hello");
    }
    {
      auto d = server_dec();
      WsEvent e = feed_one(d, B({0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d, 0x51, 0x58}));
      CHECK(e.kind == WsEvent::Message);
      CHECK(e.payload == "Hello");
    }
    {
      // Fragmented unmasked text: "Hel" + "lo"
      auto d = client_dec();
      CHECK(feed_one(d, B({0x01, 0x03, 0x48, 0x65, 0x6c})).kind == WsEvent::None);
      WsEvent e = feed_one(d, B({0x80, 0x02, 0x6c, 0x6f}));
      CHECK(e.kind == WsEvent::Message);
      CHECK(e.payload == "Hello");
    }
    {
      // Unmasked ping "Hello" and masked pong "Hello"
      auto c = client_dec();
      WsEvent e = feed_one(c, B({0x89, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f}));
      CHECK(e.kind == WsEvent::Ping);
      CHECK(e.payload == "Hello");
      auto s = server_dec();
      e = feed_one(s, B({0x8a, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d, 0x51, 0x58}));
      CHECK(e.kind == WsEvent::Pong);
      CHECK(e.payload == "Hello");
    }
    // 256 bytes binary: 16-bit length; 64 KiB: 64-bit length
    std::string p256(256, 'x'), p64k(65536, 'y');
    CHECK(ws_encode_frame(kWsBinary, p256.data(), 256).substr(0, 4) == B({0x82, 0x7E, 0x01, 0x00}));
    CHECK(ws_encode_frame(kWsBinary, p64k.data(), 65536).substr(0, 10) == B({0x82, 0x7F, 0, 0, 0, 0, 0, 1, 0, 0}));
    CHECK(ws_encode_frame(kWsBinary, p256.data(), 125).substr(0, 2) == B({0x82, 125}));
    CHECK(ws_encode_frame(kWsBinary, p64k.data(), 65535).substr(0, 4) == B({0x82, 0x7E, 0xff, 0xff}));
  }

  TEST_CASE("handshake accept key (RFC 6455 section 1.3)") {
    CHECK(ws_accept_key("dGhlIHNhbXBsZSBub25jZQ==") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    HttpParser p;
    size_t u;
    std::string req =
        "GET /ws HTTP/1.1\r\nHost: server.example.com\r\nUpgrade: websocket\r\nConnection: keep-alive, Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
    REQUIRE(p.parse(req, &u) == HttpParser::Result::Done);
    CHECK(ws_validate_upgrade(p.request(), nullptr) == 0);
    std::string resp = ws_server_handshake_response(p.request());
    CHECK(resp.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos);
    CHECK(ws_check_server_response(resp, "dGhlIHNhbXBsZSBub25jZQ==", nullptr));
    CHECK_FALSE(ws_check_server_response(resp, "AAAAAAAAAAAAAAAAAAAAAA==", nullptr));
    CHECK_FALSE(ws_check_server_response("HTTP/1.1 200 OK\r\n\r\n", "x", nullptr));
    std::string ext = resp.substr(0, resp.size() - 2) + "Sec-WebSocket-Extensions: permessage-deflate\r\n\r\n";
    CHECK_FALSE(ws_check_server_response(ext, "dGhlIHNhbXBsZSBub25jZQ==", nullptr));
  }

  TEST_CASE("upgrade validation") {
    auto status = [](const std::string& hdrs) {
      HttpParser p;
      size_t u;
      std::string r = "GET /ws HTTP/1.1\r\nHost: h\r\n" + hdrs + "\r\n";
      REQUIRE(p.parse(r, &u) == HttpParser::Result::Done);
      return ws_validate_upgrade(p.request(), nullptr);
    };
    const std::string ok = "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\n";
    CHECK(status(ok + "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n") == 0);
    CHECK(status(ok + "Sec-WebSocket-Key: short\r\n") == 400);
    CHECK(status(ok) == 400);
    CHECK(status("Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 8\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n") == 426);
    CHECK(status("Upgrade: websocket\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n") == 400);
  }

  TEST_CASE("masking rules") {
    auto s = server_dec();
    WsEvent e = feed_one(s, ws_encode_frame(kWsText, "hi", 2));  // unmasked to server
    CHECK(e.kind == WsEvent::Error);
    CHECK(e.close_code == kWsCloseProtocol);
    CHECK(s.next().kind == WsEvent::Error);  // stays dead
    auto c = client_dec();
    e = feed_one(c, masked(kWsText, "hi"));  // masked to client
    CHECK(e.kind == WsEvent::Error);
    CHECK(e.close_code == kWsCloseProtocol);
  }

  TEST_CASE("protocol violations") {
    auto expect = [](const std::string& bytes, uint16_t code) {
      auto d = server_dec(1000);
      WsEvent e = feed_one(d, bytes);
      CHECK(e.kind == WsEvent::Error);
      CHECK(e.close_code == code);
    };
    std::string f = masked(kWsText, "x");
    f[0] = char(f[0] | 0x40);  // RSV1
    expect(f, kWsCloseProtocol);
    expect(masked(0x3, "x"), kWsCloseProtocol);                 // reserved opcode
    expect(masked(0xB, "x"), kWsCloseProtocol);                 // reserved control opcode
    expect(masked(kWsPing, "x", false), kWsCloseProtocol);      // fragmented control
    expect(masked(kWsPing, std::string(126, 'p')), kWsCloseProtocol);
    expect(masked(kWsCont, "x"), kWsCloseProtocol);             // continuation without start
    expect(masked(kWsText, "a", false) + masked(kWsText, "b"), kWsCloseProtocol);  // new msg inside fragmented
    expect(masked(kWsText, "\xc3\x28"), kWsCloseBadData);       // bad UTF-8
    expect(masked(kWsClose, "\x03"), kWsCloseProtocol);         // 1-byte close payload
    expect(masked(kWsClose, B({0x03, 0xe7})), kWsCloseProtocol);  // 999
    expect(masked(kWsClose, B({0x03, 0xed})), kWsCloseProtocol);  // 1005 must not be sent
    expect(masked(kWsClose, B({0x03, 0xee})), kWsCloseProtocol);  // 1006
    expect(masked(kWsClose, B({0x13, 0x88})), kWsCloseProtocol);  // 5000
    expect(masked(kWsClose, B({0x03, 0xe8}) + "\xff"), kWsCloseBadData);
    // 64-bit length with MSB set
    expect(B({0x82, 0xFF, 0x80, 0, 0, 0, 0, 0, 0, 1, 1, 2, 3, 4}), kWsCloseProtocol);
  }

  TEST_CASE("size limits are enforced before the payload arrives") {
    auto d = server_dec(100);
    std::string big = masked(kWsBinary, std::string(101, 'z'));
    WsEvent e = feed_one(d, big.substr(0, 4));  // only the header
    CHECK(e.kind == WsEvent::Error);
    CHECK(e.close_code == kWsCloseTooBig);
    // Fragmented messages count the total.
    auto d2 = server_dec(100);
    CHECK(feed_one(d2, masked(kWsBinary, std::string(60, 'a'), false)).kind == WsEvent::None);
    e = feed_one(d2, masked(kWsCont, std::string(60, 'b')));
    CHECK(e.kind == WsEvent::Error);
    CHECK(e.close_code == kWsCloseTooBig);
    auto d3 = server_dec(100);
    e = feed_one(d3, masked(kWsBinary, std::string(100, 'z')));
    CHECK(e.kind == WsEvent::Message);
  }

  TEST_CASE("fragmentation with interleaved control frames and split UTF-8") {
    auto d = server_dec();
    std::string stream = masked(kWsText, "caf\xc3", false) + masked(kWsPing, "p") + masked(kWsCont, "\xa9 ok", false) +
                         masked(kWsCont, "!", true) + masked(kWsClose, B({0x03, 0xe8}) + "bye");
    // Byte-by-byte delivery.
    std::vector<WsEvent> evs;
    for (char c : stream) {
      d.feed(reinterpret_cast<const uint8_t*>(&c), 1);
      for (WsEvent e = d.next(); e.kind != WsEvent::None; e = d.next()) evs.push_back(e);
    }
    REQUIRE(evs.size() == 3);
    CHECK(evs[0].kind == WsEvent::Ping);
    CHECK(evs[1].kind == WsEvent::Message);
    CHECK(evs[1].payload == "caf\xc3\xa9 ok!");
    CHECK(evs[2].kind == WsEvent::Close);
    CHECK(evs[2].close_code == 1000);
    CHECK(evs[2].payload == "bye");
  }

  TEST_CASE("close codes and payloads") {
    CHECK(ws_close_code_valid(1000));
    CHECK(ws_close_code_valid(1011));
    CHECK(ws_close_code_valid(3000));
    CHECK(ws_close_code_valid(4999));
    CHECK_FALSE(ws_close_code_valid(1004));
    CHECK_FALSE(ws_close_code_valid(1005));
    CHECK_FALSE(ws_close_code_valid(1006));
    CHECK_FALSE(ws_close_code_valid(1015));
    CHECK_FALSE(ws_close_code_valid(2999));
    std::string p = ws_close_payload(1001, std::string(200, 'r'));
    CHECK(p.size() == 125);
    // Truncation never splits a UTF-8 sequence.
    std::string euro;
    for (int i = 0; i < 50; ++i) euro += "\xe2\x82\xac";
    CHECK(utf8_valid(ws_close_payload(1000, euro).substr(2)));
    auto d = server_dec();
    WsEvent e = feed_one(d, masked(kWsClose, ""));
    CHECK(e.kind == WsEvent::Close);
    CHECK(e.close_code == kWsCloseNoStatus);
  }

  TEST_CASE("mask round trip for many sizes") {
    for (size_t n : {0u, 1u, 3u, 4u, 5u, 125u, 126u, 127u, 65535u, 65536u, 70000u}) {
      std::string payload(n, '\0');
      random_bytes(&payload[0], n);
      uint8_t mk[4];
      random_bytes(mk, 4);
      std::string f = ws_encode_frame(kWsBinary, payload.data(), n, true, mk);
      auto d = server_dec(1 << 20);
      WsEvent e = feed_one(d, f);
      REQUIRE(e.kind == WsEvent::Message);
      CHECK(e.payload == payload);
    }
  }

  TEST_CASE("url parsing") {
    std::string h, p, err;
    uint16_t port;
    CHECK(ws_parse_url("ws://10.42.0.1:8437/ws", &h, &port, &p, &err));
    CHECK(h == "10.42.0.1");
    CHECK(port == 8437);
    CHECK(p == "/ws");
    CHECK(ws_parse_url("ws://hub.local/rpc?token=x", &h, &port, &p, &err));
    CHECK(port == 80);
    CHECK(p == "/rpc?token=x");
    CHECK(ws_parse_url("ws://[::1]:9000", &h, &port, &p, &err));
    CHECK(h == "::1");
    CHECK(p == "/");
    CHECK(ws_parse_url("wss://x/ws", &h, &port, &p, &err) == ws_tls_available());  // wss only with TLS built in
    bool tls = false;
    if (ws_tls_available()) {
      CHECK(ws_parse_url_tls("wss://relay.example.org/mqtt", &h, &port, &p, &tls, &err));
      CHECK(tls);
      CHECK(port == 443);
      CHECK(h == "relay.example.org");
    }
    CHECK_FALSE(ws_parse_url("ws://x:99999/", &h, &port, &p, &err));
    CHECK_FALSE(ws_parse_url("ws:///x", &h, &port, &p, &err));
    CHECK_FALSE(ws_parse_url("ftp://x", &h, &port, &p, &err));
  }
  TEST_CASE("relay sockets (protocols contains mqtt): RPC_API section 11 rules") {
    struct NullSink : EndpointSink {
      void ep_rx(Endpoint&, const uint8_t*, size_t) override {}
      void ep_state(Endpoint&, bool, const std::string&) override {}
    } sink;
    EndpointRegistry reg;
    register_net_endpoints(reg);
    const EndpointKind* k = reg.find("websocket");
    REQUIRE(k);
    Loop loop;
    auto make = [&](const std::string& cfg, std::string* err) {
      err->clear();
      return k->create(loop, sink, Json::parse_or_null(cfg), err) != nullptr;
    };
    std::string err;
    // Not a relay socket: plain ws:// anywhere is fine (hub-to-hub links).
    CHECK(make(R"({"url":"ws://192.0.2.10:8437/ws"})", &err));
    // Relay socket, plaintext: refused without allow_plaintext, and to anything but a local host.
    CHECK_FALSE(make(R"({"url":"ws://192.0.2.10/mqtt","protocols":["mqtt"]})", &err));
    CHECK_FALSE(make(R"({"url":"ws://192.0.2.10/mqtt","protocols":["mqtt"],"allow_plaintext":true})", &err));
    CHECK(err.find("local") != std::string::npos);
    CHECK_FALSE(make(R"({"url":"ws://relay.example.org/mqtt","protocols":["mqtt"],"allow_plaintext":true})", &err));
    CHECK_FALSE(make(R"({"url":"ws://127.0.0.1:8438/mqtt","protocols":["mqtt"],"allow_plaintext":true})", &err));
    CHECK(make(R"({"url":"ws://relay-box.local:8438/mqtt","protocols":["mqtt"],"allow_plaintext":true})", &err));
    CHECK(make(R"({"url":"ws://10.0.0.2:8438/mqtt","protocols":["mqtt"],"allow_plaintext":true})", &err));
    CHECK(make(R"({"url":"ws://[fe80::1]:8438/mqtt","protocols":["mqtt"],"allow_plaintext":true})", &err));
    CHECK(make(R"({"url":"ws://[fd00::2]:8438/mqtt","protocols":["mqtt"],"allow_plaintext":true})", &err));
    CHECK_FALSE(make(R"({"url":"ws://198.51.100.7/mqtt","protocols":["mqtt"],"allow_plaintext":true})", &err));
    // "insecure" is never accepted, whatever its value.
    CHECK_FALSE(make(R"({"url":"wss://relay.example.org/mqtt","protocols":["mqtt"],"insecure":true})", &err));
    CHECK_FALSE(make(R"({"url":"wss://relay.example.org/mqtt","protocols":["mqtt"],"insecure":false})", &err));
    CHECK(err.find("insecure") != std::string::npos);
    // wss:// is accepted exactly when this build has TLS.
    CHECK(make(R"({"url":"wss://relay.example.org/mqtt","protocols":["mqtt"]})", &err) == ws_tls_available());
  }
}
