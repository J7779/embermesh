#include <cstring>
#include <string>
#include <vector>

#include "doctest.h"
#include "host/endpoint.h"
#include "host/endpoints/ptt.h"
#include "host/endpoints/serial_port.h"
#include "host/json.h"
#include "host/loop.h"
#include "host/util.h"

#ifndef _WIN32
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#endif

using namespace ember_host;

namespace {

Json J(const char* s) {
  Json j;
  REQUIRE(Json::parse(s, &j));
  return j;
}

struct FakeSink : EndpointSink {
  std::string rx;
  std::vector<std::pair<bool, std::string>> states;
  void ep_rx(Endpoint&, const uint8_t* d, size_t n) override { rx.append(reinterpret_cast<const char*>(d), n); }
  void ep_state(Endpoint&, bool c, const std::string& e) override { states.emplace_back(c, e); }
};

const EndpointKind* serial_kind(EndpointRegistry& r) {
  register_serial_endpoints(r);
  return r.find("serial");
}

}  // namespace

TEST_SUITE("serial") {
  TEST_CASE("config defaults and mapping") {
    SerialConfig c;
    std::string err;
    REQUIRE(serial_config_from_json(J(R"({"port":"/dev/ttyUSB0"})"), &c, &err));
    CHECK(c.port == "/dev/ttyUSB0");
    CHECK(c.baud == 9600);
    CHECK(c.data_bits == 8);
    CHECK(c.parity == 'N');
    CHECK(c.stop_bits == 1);
    CHECK(c.flow == "none");
    CHECK(c.dtr == "on");
    CHECK(c.rts == "on");
    CHECK(c.exclusive);

    REQUIRE(serial_config_from_json(J(R"({"port":" COM3 ","baud":115200,"data_bits":7,"parity":"even",
        "stop_bits":2,"flow":"rtscts","dtr":"off","rts":false,"exclusive":false})"),
                                    &c, &err));
    CHECK(c.port == "COM3");
    CHECK(c.baud == 115200);
    CHECK(c.data_bits == 7);
    CHECK(c.parity == 'E');
    CHECK(c.stop_bits == 2);
    CHECK(c.flow == "rtscts");
    CHECK(c.dtr == "off");
    CHECK(c.rts == "off");
    CHECK_FALSE(c.exclusive);
  }

  TEST_CASE("form values arrive as strings") {
    SerialConfig c;
    std::string err;
    REQUIRE(serial_config_from_json(
        J(R"({"port":"/dev/cu.usbserial-1410","baud":"38400","data_bits":"8","stop_bits":"1","parity":"O",
             "flow":"XON/XOFF","dtr":"1","rts":"low","exclusive":"no"})"),
        &c, &err));
    CHECK(c.baud == 38400);
    CHECK(c.parity == 'O');
    CHECK(c.flow == "xonxoff");
    CHECK(c.dtr == "on");
    CHECK(c.rts == "off");
    CHECK_FALSE(c.exclusive);
    // Empty strings mean "use the default".
    REQUIRE(serial_config_from_json(J(R"({"port":"COM1","baud":"","parity":"","flow":""})"), &c, &err));
    CHECK(c.baud == 9600);
    CHECK(c.parity == 'N');
    CHECK(c.flow == "none");
    // Advanced custom override wins.
    REQUIRE(serial_config_from_json(J(R"({"port":"COM1","baud":9600,"baud_custom":"250000"})"), &c, &err));
    CHECK(c.baud == 250000);
  }

  TEST_CASE("parity names") {
    SerialConfig c;
    std::string err;
    const char* names[][2] = {{"none", "N"}, {"N", "N"}, {"Even", "E"}, {"odd", "O"}, {"mark", "M"}, {"s", "S"}};
    for (auto& n : names) {
      Json j = Json::object();
      j.set("port", "COM1");
      j.set("parity", n[0]);
      REQUIRE(serial_config_from_json(j, &c, &err));
      CHECK(c.parity == n[1][0]);
    }
  }

  TEST_CASE("non-standard baud rates accepted in config") {
    SerialConfig c;
    std::string err;
    for (int b : {50, 250000, 31250, 1000000, 12000000}) {
      Json j = Json::object();
      j.set("port", "/dev/ttyUSB0");
      j.set("baud", b);
      CHECK(serial_config_from_json(j, &c, &err));
      CHECK(c.baud == uint32_t(b));
    }
  }

  TEST_CASE("invalid configs rejected") {
    SerialConfig c;
    const char* bad[] = {
        R"([])",
        R"({})",
        R"({"port":""})",
        R"({"port":"   "})",
        R"({"port":"COM1","baud":49})",
        R"({"port":"COM1","baud":12000001})",
        R"({"port":"COM1","baud":-9600})",
        R"({"port":"COM1","baud":9600.5})",
        R"({"port":"COM1","baud":"fast"})",
        R"({"port":"COM1","baud":true})",
        R"({"port":"COM1","baud":[9600]})",
        R"({"port":"COM1","data_bits":4})",
        R"({"port":"COM1","data_bits":9})",
        R"({"port":"COM1","data_bits":"eight"})",
        R"({"port":"COM1","stop_bits":0})",
        R"({"port":"COM1","stop_bits":3})",
        R"({"port":"COM1","stop_bits":1.5})",
        R"({"port":"COM1","parity":"x"})",
        R"({"port":"COM1","parity":"evenodd"})",
        R"({"port":"COM1","flow":"dsrdtr"})",
        R"({"port":"COM1","dtr":"maybe"})",
        R"({"port":"COM1","rts":"2"})",
    };
    for (const char* b : bad) {
      std::string err;
      INFO(b);
      CHECK_FALSE(serial_config_from_json(J(b), &c, &err));
      CHECK_FALSE(err.empty());
    }
    std::string long_port(300, 'a');
    Json j = Json::object();
    j.set("port", long_port);
    std::string err;
    CHECK_FALSE(serial_config_from_json(j, &c, &err));
  }

  TEST_CASE("standard baud constant table") {
    unsigned long k = 0;
#ifdef _WIN32
    CHECK_FALSE(posix_standard_baud(9600, &k));
#else
    CHECK(posix_standard_baud(9600, &k));
    CHECK(k == (unsigned long)B9600);
    CHECK(posix_standard_baud(1200, &k));
    CHECK(k == (unsigned long)B1200);
    CHECK(posix_standard_baud(38400, &k));
    CHECK(k == (unsigned long)B38400);
    CHECK(posix_standard_baud(115200, &k));
    CHECK(k == (unsigned long)B115200);
    CHECK(posix_standard_baud(230400, &k));
    CHECK(k == (unsigned long)B230400);
    CHECK_FALSE(posix_standard_baud(250000, &k));
    CHECK_FALSE(posix_standard_baud(0, &k));
    CHECK_FALSE(posix_standard_baud(9601, &k));
#endif
  }

  TEST_CASE("registry entry and endpoint creation validation") {
    EndpointRegistry r;
    const EndpointKind* k = serial_kind(r);
    REQUIRE(k);
    CHECK(k->scannable);
    CHECK(k->default_link_type == "kiss");
    CHECK(k->config.is_array());
    bool has_port = false;
    for (const auto& f : k->config.elements())
      if (f.get("key").as_string() == "port") has_port = f.get("required").as_bool();
    CHECK(has_port);
    Loop loop;
    FakeSink sink;
    std::string err;
    CHECK_FALSE(k->create(loop, sink, J(R"({"port":"COM1","ptt_line":"cts"})"), &err));
    CHECK_FALSE(k->create(loop, sink, J(R"({"port":"COM1","ptt_line":"rts","flow":"rtscts"})"), &err));
    CHECK_FALSE(k->create(loop, sink, J(R"({"baud":9600})"), &err));
    CHECK(k->create(loop, sink, J(R"({"port":"COM1","ptt_line":"dtr"})"), &err));
  }

  TEST_CASE("enumeration and scan do not crash") {
    auto ports = enumerate_serial_ports();
    for (const auto& p : ports) CHECK_FALSE(p.path.empty());
    EndpointRegistry r;
    const EndpointKind* k = serial_kind(r);
    Json res = k->scan(Json::object());
    CHECK(res.get("devices").is_array());
    CHECK(res.get("devices").size() == ports.size());
  }

  TEST_CASE("opening a missing port reports an error asynchronously") {
    EndpointRegistry r;
    const EndpointKind* k = serial_kind(r);
    Loop loop;
    FakeSink sink;
    std::string err;
    auto ep = k->create(loop, sink, J(R"({"port":"/dev/this-port-does-not-exist-ember"})"), &err);
    REQUIRE(ep);
    ep->open();
    CHECK(sink.states.empty());  // never synchronously
    for (int i = 0; i < 20 && sink.states.empty(); ++i) loop.run_once(10);
    REQUIRE(sink.states.size() == 1);
    CHECK_FALSE(sink.states[0].first);
    CHECK_FALSE(sink.states[0].second.empty());
    CHECK(ep->auto_reconnect());
    ep->write(reinterpret_cast<const uint8_t*>("x"), 1);  // dropped silently while closed
    ep->set_ptt(true);
  }

  TEST_CASE("make_serial_line_ptt validation") {
    std::string err;
    CHECK_FALSE(make_serial_line_ptt("/dev/null-nope", "cts", false, &err));
    CHECK_FALSE(make_serial_line_ptt("", "rts", false, &err));
    CHECK_FALSE(make_serial_line_ptt("/dev/this-port-does-not-exist-ember", "rts", false, &err));
    CHECK_FALSE(err.empty());
  }

#ifndef _WIN32
  TEST_CASE("pty loopback through SerialEndpoint") {
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    REQUIRE(master >= 0);
    REQUIRE(grantpt(master) == 0);
    REQUIRE(unlockpt(master) == 0);
    const char* sname = ptsname(master);
    REQUIRE(sname);
    std::string slave = sname;
    // Raw mode on the master side so bytes pass unchanged in both directions.
    struct termios t;
    if (tcgetattr(master, &t) == 0) {
      cfmakeraw(&t);
      tcsetattr(master, TCSANOW, &t);
    }
    fcntl(master, F_SETFL, fcntl(master, F_GETFL, 0) | O_NONBLOCK);

    EndpointRegistry r;
    const EndpointKind* k = serial_kind(r);
    Loop loop;
    FakeSink sink;
    std::string err;
    Json cfg = Json::object();
    cfg.set("port", slave);
    cfg.set("baud", "115200");
    cfg.set("ptt_line", "rts");
    auto ep = k->create(loop, sink, cfg, &err);
    REQUIRE_MESSAGE(ep, err);
    ep->open();
    for (int i = 0; i < 50 && sink.states.empty(); ++i) loop.run_once(10);
    REQUIRE(sink.states.size() >= 1);
    REQUIRE_MESSAGE(sink.states[0].first, sink.states[0].second);
    CHECK(ep->connected());

    // endpoint -> master (including bytes a cooked tty would mangle)
    const uint8_t frame[] = {0xC0, 0x00, 'h', 'i', '\r', '\n', 0x03, 0x11, 0x13, 0xDB, 0xDC, 0xC0};
    ep->write(frame, sizeof frame);
    std::string got;
    for (int i = 0; i < 100 && got.size() < sizeof frame; ++i) {
      loop.run_once(5);
      char buf[256];
      ssize_t n = read(master, buf, sizeof buf);
      if (n > 0) got.append(buf, size_t(n));
    }
    CHECK(got == std::string(reinterpret_cast<const char*>(frame), sizeof frame));

    // master -> endpoint
    std::string big;
    for (int i = 0; i < 5000; ++i) big += char(i * 7 + 1);
    size_t off = 0;
    for (int i = 0; i < 400 && (off < big.size() || sink.rx.size() < big.size()); ++i) {
      if (off < big.size()) {
        ssize_t n = write(master, big.data() + off, big.size() - off);
        if (n > 0) off += size_t(n);
      }
      loop.run_once(5);
    }
    CHECK(sink.rx == big);

    // PTT on a pty: modem-control ioctls may fail (ENOTTY/EINVAL) — must only log.
    ep->set_ptt(true);
    CHECK(ep->status().get("ptt").as_bool());
    ep->set_ptt(false);
    CHECK_FALSE(ep->status().get("ptt").as_bool());
    CHECK(ep->status().get("port").as_string() == slave);
    CHECK(ep->status().get("baud").as_number() == 115200);

    // Hang up the master: Linux reports EIO, macOS POLLHUP / read 0. Tolerate platforms that don't.
    close(master);
    for (int i = 0; i < 50 && sink.states.size() < 2; ++i) loop.run_once(10);
    if (sink.states.size() >= 2) {
      CHECK_FALSE(sink.states.back().first);
      CHECK_FALSE(ep->connected());
    } else {
      MESSAGE("pty hangup not reported on this platform (tolerated)");
    }
    ep->write(frame, sizeof frame);  // after disconnect: no crash
    ep->close();
    for (int i = 0; i < 3; ++i) loop.run_once(1);
  }

  TEST_CASE("pty: serial line PTT opens and keys without crashing") {
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    REQUIRE(master >= 0);
    REQUIRE(grantpt(master) == 0);
    REQUIRE(unlockpt(master) == 0);
    std::string slave = ptsname(master);
    std::string err;
    auto ptt = make_serial_line_ptt(slave, "both", true, &err);
    REQUIRE_MESSAGE(ptt, err);
    CHECK(ptt->ok());
    CHECK(ptt->describe().find(slave) != std::string::npos);
    ptt->set(true);
    ptt->set(false);
    ptt.reset();
    close(master);
  }

  TEST_CASE("pty: SerialPort non-standard baud on a pty fails cleanly or works") {
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    REQUIRE(master >= 0);
    REQUIRE(grantpt(master) == 0);
    REQUIRE(unlockpt(master) == 0);
    Loop loop;
    SerialPort p(&loop);
    SerialConfig c;
    c.port = ptsname(master);
    c.baud = 250000;
    std::string err;
    bool ok = p.open(c, false, &err);
    if (!ok) CHECK_FALSE(err.empty());
    p.close();
    CHECK_FALSE(p.is_open());
    // Not a tty at all:
    c.port = "/dev/null";
    CHECK_FALSE(p.open(c, false, &err));
    close(master);
  }
#endif
}
