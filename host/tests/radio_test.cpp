// Tests for the radio-side endpoints: rtl_tcp, rigctld client, audio (no hardware required).
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "doctest.h"
#include "host/endpoint.h"
#include "host/endpoints/ptt.h"
#include "host/endpoints/rigctl.h"
#include "host/endpoints/rtl_tcp.h"
#include "host/loop.h"
#include "host/stream.h"
#include "host/util.h"
#ifdef EMBER_HOST_HAVE_AUDIO
#include "host/endpoints/audio.h"
#endif

using namespace ember_host;

namespace {

struct RecSink : EndpointSink {
  std::string rx;
  std::vector<std::pair<bool, std::string>> states;
  int status_changes = 0;
  void ep_rx(Endpoint&, const uint8_t* d, size_t n) override { rx.append(reinterpret_cast<const char*>(d), n); }
  void ep_state(Endpoint&, bool c, const std::string& e) override { states.emplace_back(c, e); }
  void ep_status_changed(Endpoint&) override { ++status_changes; }
};

template <class F>
bool run_until(Loop& loop, F cond, int timeout_ms = 3000) {
  uint64_t end = steady_ms() + uint64_t(timeout_ms);
  while (!cond()) {
    if (steady_ms() > end) return false;
    loop.run_once(10);
  }
  return true;
}

// Minimal one-client TCP server on 127.0.0.1:<ephemeral> driven by the loop.
struct FakeServer {
  Loop& loop;
  sock_t lsock = kInvalidSock;
  uint16_t port = 0;
  uint64_t watch = 0;
  std::unique_ptr<TcpStream> conn;
  std::string received;
  std::function<void(TcpStream&)> on_accept;
  std::function<void(TcpStream&, const std::string& all)> on_rx;

  explicit FakeServer(Loop& l) : loop(l) {
    std::string err;
    lsock = tcp_listen("127.0.0.1", 0, &err, &port);
    REQUIRE(lsock != kInvalidSock);
    watch = loop.watch(lsock, kPollIn, [this](short) {
      SockAddr peer;
      sock_t c = tcp_accept(lsock, &peer);
      if (c == kInvalidSock) return;
      conn.reset(new TcpStream(loop));
      conn->adopt(c, peer);
      conn->on_data = [this](const uint8_t* d, size_t n) {
        received.append(reinterpret_cast<const char*>(d), n);
        if (on_rx) on_rx(*conn, received);
      };
      if (on_accept) on_accept(*conn);
    });
  }
  ~FakeServer() {
    conn.reset();
    loop.unwatch(watch);
    sock_close(lsock);
  }
};

std::string bytes(std::initializer_list<int> v) {
  std::string s;
  for (int b : v) s += char(b);
  return s;
}

}  // namespace

TEST_SUITE("rtl_tcp") {
  TEST_CASE("command encoding") {
    CHECK(rtltcp_command(kRtlSetFreq, 144390000) == bytes({0x01, 0x08, 0x9b, 0x37, 0x70}));
    CHECK(rtltcp_command(kRtlSetSampleRate, 1024000) == bytes({0x02, 0x00, 0x0f, 0xa0, 0x00}));
    CHECK(rtltcp_command(kRtlSetGainMode, 1) == bytes({0x03, 0, 0, 0, 1}));
    CHECK(rtltcp_command_signed(kRtlSetGain, 496) == bytes({0x04, 0, 0, 0x01, 0xf0}));
    CHECK(rtltcp_command_signed(kRtlSetGain, -10) == bytes({0x04, 0xff, 0xff, 0xff, 0xf6}));
    CHECK(rtltcp_command_signed(kRtlSetFreqCorrection, -42) == bytes({0x05, 0xff, 0xff, 0xff, 0xd6}));
    CHECK(rtltcp_command_signed(kRtlSetFreqCorrection, 55) == bytes({0x05, 0, 0, 0, 55}));
    CHECK(rtltcp_command(kRtlSetAgcMode, 1) == bytes({0x08, 0, 0, 0, 1}));
    CHECK(rtltcp_command(kRtlSetDirectSampling, 2) == bytes({0x09, 0, 0, 0, 2}));
    CHECK(rtltcp_command(kRtlSetBiasTee, 1) == bytes({0x0e, 0, 0, 0, 1}));
    CHECK(rtltcp_command(kRtlSetFreq, 0xFFFFFFFFu) == bytes({0x01, 0xff, 0xff, 0xff, 0xff}));
    CHECK(rtltcp_command(kRtlSetFreq, 0).size() == 5);
  }

  TEST_CASE("header parsing") {
    std::string h = "RTL0" + bytes({0, 0, 0, 5, 0, 0, 0, 29});
    RtlTcpHeader hdr;
    CHECK(rtltcp_parse_header(reinterpret_cast<const uint8_t*>(h.data()), h.size(), &hdr) == 1);
    CHECK(hdr.tuner == 5);
    CHECK(hdr.gain_count == 29);
    CHECK(std::string(rtltcp_tuner_name(hdr.tuner)) == "R820T");
    CHECK(rtltcp_parse_header(reinterpret_cast<const uint8_t*>(h.data()), 0, &hdr) == 0);
    CHECK(rtltcp_parse_header(reinterpret_cast<const uint8_t*>(h.data()), 3, &hdr) == 0);
    CHECK(rtltcp_parse_header(reinterpret_cast<const uint8_t*>(h.data()), 11, &hdr) == 0);
    std::string bad = "HTTP/1.1 200";
    CHECK(rtltcp_parse_header(reinterpret_cast<const uint8_t*>(bad.data()), bad.size(), &hdr) == -1);
    CHECK(rtltcp_parse_header(reinterpret_cast<const uint8_t*>(bad.data()), 1, &hdr) == -1);  // early reject
    CHECK(std::string(rtltcp_tuner_name(99)) == "unknown");
  }

  TEST_CASE("sample rate validity") {
    CHECK_FALSE(rtltcp_valid_sample_rate(225000));
    CHECK(rtltcp_valid_sample_rate(225001));
    CHECK(rtltcp_valid_sample_rate(300000));
    CHECK_FALSE(rtltcp_valid_sample_rate(300001));
    CHECK_FALSE(rtltcp_valid_sample_rate(900000));
    CHECK(rtltcp_valid_sample_rate(900001));
    CHECK(rtltcp_valid_sample_rate(3200000));
    CHECK_FALSE(rtltcp_valid_sample_rate(3200001));
  }

  TEST_CASE("registry entry and config validation") {
    EndpointRegistry r;
    register_sdr_endpoints(r);
    const EndpointKind* k = r.find("rtl_tcp");
    REQUIRE(k);
    CHECK(k->scannable);
    CHECK(k->default_link_type == "rtl_tcp");
    CHECK(k->config.is_array());
    CHECK(k->config.size() >= 6);
    Loop loop;
    RecSink sink;
    std::string err;
    Json bad = Json::object();
    bad.set("sample_rate", 500000);
    CHECK(k->create(loop, sink, bad, &err) == nullptr);
    CHECK(!err.empty());
    Json badport = Json::object();
    badport.set("port", 70000);
    CHECK(k->create(loop, sink, badport, &err) == nullptr);
    CHECK(k->create(loop, sink, Json::object(), &err) != nullptr);
    Json scan = k->scan(Json::object());
    CHECK(scan.get("devices").is_array());
  }

  TEST_CASE("endpoint against fake rtl_tcp server: commands, split header, IQ") {
    Loop loop;
    FakeServer srv(loop);
    // Send the header split in three pieces with IQ attached to the last piece, then more IQ.
    srv.on_accept = [&](TcpStream& c) {
      std::string hdr = "RTL0" + bytes({0, 0, 0, 5, 0, 0, 0, 29});
      c.write(hdr.substr(0, 3));
      loop.timer(30, [&srv, hdr] {
        if (srv.conn) srv.conn->write(hdr.substr(3, 6));
      });
      loop.timer(60, [&srv, hdr] {
        if (srv.conn) srv.conn->write(hdr.substr(9) + "\x7f\x80\x81");
      });
      loop.timer(90, [&srv] {
        if (srv.conn) srv.conn->write(std::string("\x01\x02", 2));
      });
    };
    EndpointRegistry r;
    register_sdr_endpoints(r);
    RecSink sink;
    Json cfg = Json::object();
    cfg.set("host", "127.0.0.1");
    cfg.set("port", srv.port);
    cfg.set("freq_hz", 144800000);
    cfg.set("sample_rate", 240000);
    cfg.set("gain_db", 20.7);
    cfg.set("ppm", -3);
    cfg.set("agc", true);
    cfg.set("bias_tee", true);
    cfg.set("tune", true);
    std::string err;
    auto ep = r.find("rtl_tcp")->create(loop, sink, cfg, &err);
    REQUIRE(ep);
    ep->open();
    CHECK(sink.states.empty());  // never synchronous
    // The core's rtl_tcp driver needs the server header: it is passed through, then the I/Q.
    std::string hdr = "RTL0" + bytes({0, 0, 0, 5, 0, 0, 0, 29});
    REQUIRE(run_until(loop, [&] { return sink.rx.size() >= 12 + 5; }));
    CHECK(sink.rx == hdr + std::string("\x7f\x80\x81\x01\x02", 5));
    REQUIRE(!sink.states.empty());
    CHECK(sink.states.front().first);
    CHECK(ep->connected());
    Json st = ep->status();
    CHECK(st.get("tuner_name").as_string() == "R820T");
    CHECK(st.get("gain_count").as_int() == 29);
    CHECK(st.get("iq_bytes").as_int() == 5);

    std::string expect = rtltcp_command(kRtlSetBiasTee, 1) + rtltcp_command(kRtlSetSampleRate, 240000) +
                         rtltcp_command(kRtlSetFreq, 144800000) + rtltcp_command_signed(kRtlSetFreqCorrection, -3) +
                         rtltcp_command(kRtlSetGainMode, 1) + rtltcp_command_signed(kRtlSetGain, 207) +
                         rtltcp_command(kRtlSetAgcMode, 1);
    REQUIRE(run_until(loop, [&] { return srv.received.size() >= expect.size(); }));
    CHECK(srv.received == expect);

    // Core-originated bytes pass straight through (driver retune).
    std::string retune = rtltcp_command(kRtlSetFreq, 145000000);
    ep->write(reinterpret_cast<const uint8_t*>(retune.data()), retune.size());
    REQUIRE(run_until(loop, [&] { return srv.received.size() >= expect.size() + 5; }));
    CHECK(srv.received.substr(expect.size()) == retune);

    // Server goes away -> state down with an error.
    srv.conn.reset();
    REQUIRE(run_until(loop, [&] { return sink.states.size() >= 2; }));
    CHECK_FALSE(sink.states.back().first);
    CHECK_FALSE(ep->connected());
    ep->close();
  }

  TEST_CASE("bad magic closes with error") {
    Loop loop;
    FakeServer srv(loop);
    srv.on_accept = [](TcpStream& c) { c.write(std::string("HTTP/1.1 400 Bad\r\n\r\n")); };
    EndpointRegistry r;
    register_sdr_endpoints(r);
    RecSink sink;
    Json cfg = Json::object();
    cfg.set("port", srv.port);
    std::string err;
    auto ep = r.find("rtl_tcp")->create(loop, sink, cfg, &err);
    REQUIRE(ep);
    ep->open();
    REQUIRE(run_until(loop, [&] { return !sink.states.empty(); }));
    CHECK_FALSE(sink.states.back().first);
    CHECK(sink.states.back().second.find("magic") != std::string::npos);
    CHECK(sink.rx.empty());
  }

  TEST_CASE("connection refused reports error") {
    Loop loop;
    std::string err;
    uint16_t port = 0;
    sock_t s = tcp_listen("127.0.0.1", 0, &err, &port);
    REQUIRE(s != kInvalidSock);
    sock_close(s);  // nothing listens there now
    EndpointRegistry r;
    register_sdr_endpoints(r);
    RecSink sink;
    Json cfg = Json::object();
    cfg.set("port", port);
    auto ep = r.find("rtl_tcp")->create(loop, sink, cfg, &err);
    REQUIRE(ep);
    ep->open();
    REQUIRE(run_until(loop, [&] { return !sink.states.empty(); }));
    CHECK_FALSE(sink.states.back().first);
    CHECK(!sink.states.back().second.empty());
  }
}

TEST_SUITE("rigctl") {
  TEST_CASE("encoders") {
    CHECK(rigctl_cmd_set_freq(145500000) == "F 145500000\n");
    CHECK(rigctl_cmd_set_freq(7074000) == "F 7074000\n");
    CHECK(rigctl_cmd_set_mode("FM", 0) == "M FM 0\n");
    CHECK(rigctl_cmd_set_mode("pktusb", 3000) == "M PKTUSB 3000\n");
    CHECK(rigctl_cmd_set_mode("USB\nT 1", 0) == "M USBT1 0\n");  // no command injection
    CHECK(rigctl_cmd_set_mode("", -5) == "M FM 0\n");
    CHECK(rigctl_cmd_set_ptt(true) == "T 1\n");
    CHECK(rigctl_cmd_set_ptt(false) == "T 0\n");
    CHECK(rigctl_cmd_get_freq() == "f\n");
  }

  TEST_CASE("RPRT parsing") {
    int c = 99;
    CHECK(rigctl_parse_rprt("RPRT 0", &c));
    CHECK(c == 0);
    CHECK(rigctl_parse_rprt("RPRT -11\r", &c));
    CHECK(c == -11);
    CHECK(rigctl_parse_rprt("  RPRT 1  ", &c));
    CHECK(c == 1);
    CHECK_FALSE(rigctl_parse_rprt("145500000", &c));
    CHECK_FALSE(rigctl_parse_rprt("RPRT", &c));
    CHECK_FALSE(rigctl_parse_rprt("RPRT x", &c));
    CHECK_FALSE(rigctl_parse_rprt("RPRT -", &c));
    CHECK_FALSE(rigctl_parse_rprt("", &c));
  }

  TEST_CASE("client against fake rigctld") {
    Loop loop;
    FakeServer srv(loop);
    size_t handled = 0;
    srv.on_rx = [&](TcpStream& c, const std::string& all) {
      // Answer each complete line.
      size_t pos = 0, count = 0;
      while (true) {
        size_t nl = all.find('\n', pos);
        if (nl == std::string::npos) break;
        if (count++ >= handled) {
          std::string line = all.substr(pos, nl - pos);
          if (line == "f") c.write(std::string("145500000\n"));
          else if (line == "M BOGUS 0") c.write(std::string("RPRT -1\n"));
          else c.write(std::string("RPRT 0\n"));
          ++handled;
        }
        pos = nl + 1;
      }
    };
    RigctlClient rig(loop, "127.0.0.1", srv.port);
    std::vector<std::pair<int, std::string>> results;
    rig.set_freq(145500000, [&](int code, const std::string& r) { results.emplace_back(code, r); });
    rig.set_mode("FM", 0, [&](int code, const std::string& r) { results.emplace_back(code, r); });
    rig.set_ptt(true, [&](int code, const std::string& r) { results.emplace_back(code, r); });
    rig.send(rigctl_cmd_get_freq(), [&](int code, const std::string& r) { results.emplace_back(code, r); });
    rig.set_mode("BOGUS", 0, [&](int code, const std::string& r) { results.emplace_back(code, r); });
    REQUIRE(run_until(loop, [&] { return results.size() == 5; }));
    CHECK(results[0].first == 0);
    CHECK(results[1].first == 0);
    CHECK(results[2].first == 0);
    CHECK(results[3].first == 0);
    CHECK(results[3].second == "145500000");
    CHECK(results[4].first == -1);
    CHECK(srv.received == "F 145500000\nM FM 0\nT 1\nf\nM BOGUS 0\n");
    CHECK(rig.connected());
  }

  TEST_CASE("client reports connect failure") {
    Loop loop;
    std::string err;
    uint16_t port = 0;
    sock_t s = tcp_listen("127.0.0.1", 0, &err, &port);
    REQUIRE(s != kInvalidSock);
    sock_close(s);
    RigctlClient rig(loop, "127.0.0.1", port);
    int code = 0;
    bool done = false;
    rig.set_ptt(true, [&](int c, const std::string&) {
      code = c;
      done = true;
    });
    REQUIRE(run_until(loop, [&] { return done; }));
    CHECK(code == kRigctlConnectFailed);
  }

  TEST_CASE("ptt factory") {
    Loop loop;
    std::string err;
    Json c = Json::object();
    c.set("ptt", "vox");
    CHECK(make_ptt_from_config(loop, c, &err) == nullptr);
    CHECK(err.empty());
    c.set("ptt", "none");
    CHECK(make_ptt_from_config(loop, c, &err) == nullptr);
    CHECK(err.empty());
    c.set("ptt", "rigctld");
    auto p = make_ptt_from_config(loop, c, &err);
    REQUIRE(p);
    CHECK(p->describe() == "rigctld 127.0.0.1:4532");
    c.set("ptt", "serial_rts");
    CHECK(make_ptt_from_config(loop, c, &err) == nullptr);
    CHECK(!err.empty());  // ptt_port missing
    err.clear();
    c.set("ptt", "warp_drive");
    CHECK(make_ptt_from_config(loop, c, &err) == nullptr);
    CHECK(!err.empty());
  }

  TEST_CASE("rigctld ptt keys via T commands") {
    Loop loop;
    FakeServer srv(loop);
    srv.on_rx = [](TcpStream& c, const std::string& all) {
      if (!all.empty() && all.back() == '\n') c.write(std::string("RPRT 0\n"));
    };
    auto p = make_rigctld_ptt(loop, "127.0.0.1", srv.port);
    p->set(true);
    p->set(true);  // duplicate suppressed
    REQUIRE(run_until(loop, [&] { return srv.received == "T 1\n"; }));
    p->set(false);
    REQUIRE(run_until(loop, [&] { return srv.received == "T 1\nT 0\n"; }));
  }
}

#ifdef EMBER_HOST_HAVE_AUDIO
TEST_SUITE("audio") {
  TEST_CASE("registry entry and schema") {
    EndpointRegistry r;
    register_audio_endpoints(r);
    const EndpointKind* k = r.find("audio");
    REQUIRE(k);
    CHECK(k->title == "Sound card (radio audio)");
    CHECK(k->scannable);
    CHECK(k->default_link_type == "audio");
    bool has_ptt = false, has_rate = false;
    for (const auto& f : k->config.elements()) {
      CHECK(f.get("key").is_string());
      CHECK(f.get("label").is_string());
      CHECK(f.get("type").is_string());
      if (f.get("key").as_string() == "ptt") {
        has_ptt = true;
        CHECK(f.get("default").as_string() == "vox");
        CHECK(f.get("options").size() == 6);  // vox, serial_rts, serial_dtr, cm108, rigctld, none
      }
      if (f.get("key").as_string() == "sample_rate") {
        has_rate = true;
        CHECK(f.get("default").as_int() == 48000);
      }
    }
    CHECK(has_ptt);
    CHECK(has_rate);
  }

  TEST_CASE("config validation") {
    EndpointRegistry r;
    register_audio_endpoints(r);
    const EndpointKind* k = r.find("audio");
    REQUIRE(k);
    Loop loop;
    RecSink sink;
    std::string err;
    Json c = Json::object();
    c.set("sample_rate", 4000);
    CHECK(k->create(loop, sink, c, &err) == nullptr);
    c = Json::object();
    c.set("ptt", "serial_rts");
    CHECK(k->create(loop, sink, c, &err) == nullptr);
    c = Json::object();
    c.set("ptt", "laser");
    CHECK(k->create(loop, sink, c, &err) == nullptr);
    auto ep = k->create(loop, sink, Json::object(), &err);
    REQUIRE(ep);
    CHECK(ep->is_audio());
    Json st = ep->status();
    CHECK(st.get("sample_rate").as_int() == 48000);
    CHECK(st.get("ptt").as_string() == "vox");
    CHECK(st.get("ptt_on").as_bool() == false);
    ep->close();  // never opened: harmless
  }

  TEST_CASE("enumeration does not crash; scan shape") {
    std::string err;
    auto devs = enumerate_audio_devices(&err);
    for (const auto& d : devs) CHECK(!d.name.empty());
    EndpointRegistry r;
    register_audio_endpoints(r);
    Json s = r.find("audio")->scan(Json::object());
    CHECK(s.get("devices").is_array());
    CHECK(s.get("devices").size() == devs.size());
  }

  TEST_CASE("open with nonexistent device falls back or fails cleanly (hardware, opt-in)") {
    const char* hw = std::getenv("EMBER_HOST_AUDIO_HW_TEST");
    if (!hw || std::string(hw) != "1") return;  // opening devices may trigger mic permission prompts
    EndpointRegistry r;
    register_audio_endpoints(r);
    Loop loop;
    RecSink sink;
    std::string err;
    Json c = Json::object();
    c.set("input_device", "no-such-device-xyzzy");
    c.set("output_device", "none");  // never play sound
    auto ep = r.find("audio")->create(loop, sink, c, &err);
    REQUIRE(ep);
    ep->open();
    // The loop must stay responsive while the device opens (macOS may block ma_device_init on the
    // microphone-permission prompt): a timer keeps firing meanwhile.
    int ticks = 0;
    std::function<void()> tick = [&] {
      ++ticks;
      loop.timer(10, tick);
    };
    loop.timer(10, tick);
    bool reported = run_until(loop, [&] { return !sink.states.empty(); }, 4000);
    CHECK(ticks > 50);
    if (!reported) {
      MESSAGE("audio open still pending (permission prompt?) - checking cancel path");
      CHECK(ep->status().get("opening").as_bool());
    } else if (sink.states.back().first) {
      CHECK(ep->status().get("input_fallback").as_bool());
    } else {
      CHECK(!sink.states.back().second.empty());
    }
    ep->close();  // also cancels a pending open
    ep.reset();
    for (int i = 0; i < 5; ++i) loop.run_once(5);
  }

  TEST_CASE("close while opening and destroy immediately") {
    EndpointRegistry r;
    register_audio_endpoints(r);
    Loop loop;
    RecSink sink;
    std::string err;
    Json c = Json::object();
    c.set("input_device", "none");
    c.set("output_device", "none");  // nothing is ever opened: fails with an error, asynchronously
    auto ep = r.find("audio")->create(loop, sink, c, &err);
    REQUIRE(ep);
    ep->open();
    CHECK(sink.states.empty());
    REQUIRE(run_until(loop, [&] { return !sink.states.empty(); }));
    CHECK_FALSE(sink.states.back().first);
    CHECK(sink.states.back().second.find("disabled") != std::string::npos);
    sink.states.clear();
    ep->open();
    ep.reset();  // destroyed with the worker possibly still running
    for (int i = 0; i < 20; ++i) loop.run_once(5);
    CHECK(sink.states.empty());
  }
}
#endif
