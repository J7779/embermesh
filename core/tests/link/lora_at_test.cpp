// lora_at driver: scripted Reyax RYLR998/RYLR896 AT-command emulator, exact command strings, flash-
// friendly init, send/receive of every size, interop text, errors, timeouts, reboot, fuzz, relay.
#include <functional>
#include <map>

#include "doctest.h"
#include "link/link_util.h"
#include "link_harness.h"

using namespace ember;
using namespace linktest;

namespace {

struct AtEmu {
  std::string line;
  std::string outbox;
  bool alive = true;
  std::string nl = "\r\n";
  int addr = 0, net = 18, power = 22;
  long band = 915000000;
  std::string param = "9,7,1,12";
  std::string report_param;            // if set, AT+PARAMETER? reports this instead
  std::map<std::string, int> err_for;  // command prefix -> +ERR code
  int ignore_sends = 0;                // swallow this many AT+SEND commands (timeouts)
  bool net_reply_short = false;        // "+NETWORK=" as printed in the RYLR998 guide
  std::vector<std::string> cmds;
  std::vector<std::pair<int, std::string>> air;  // (dest, data)
  int flash_writes = 0;

  void say(const std::string& s) { outbox += s + nl; }
  void from_host(ByteView v) {
    for (uint8_t b : v) {
      if (b == '\n') {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        handle(line);
        line.clear();
      } else {
        line.push_back(static_cast<char>(b));
      }
    }
  }
  void handle(const std::string& c) {
    cmds.push_back(c);
    if (!alive) return;
    for (const auto& e : err_for)
      if (c.compare(0, e.first.size(), e.first) == 0) { say("+ERR=" + std::to_string(e.second)); return; }
    auto val = [&](size_t n) { return c.substr(n); };
    if (c == "AT") say("+OK");
    else if (c == "AT+VER?") say("+VER=RYLR998_V1.2.3");
    else if (c == "AT+ADDRESS?") say("+ADDRESS=" + std::to_string(addr));
    else if (c.rfind("AT+ADDRESS=", 0) == 0) { addr = std::stoi(val(11)); ++flash_writes; say("+OK"); }
    else if (c == "AT+NETWORKID?") say((net_reply_short ? "+NETWORK=" : "+NETWORKID=") + std::to_string(net));
    else if (c.rfind("AT+NETWORKID=", 0) == 0) { net = std::stoi(val(13)); ++flash_writes; say("+OK"); }
    else if (c == "AT+BAND?") say("+BAND=" + std::to_string(band));
    else if (c.rfind("AT+BAND=", 0) == 0) { band = std::stol(val(8)); say("+OK"); }
    else if (c == "AT+PARAMETER?") say("+PARAMETER=" + (report_param.empty() ? param : report_param));
    else if (c.rfind("AT+PARAMETER=", 0) == 0) { param = val(13); say("+OK"); }
    else if (c.rfind("AT+CRFOP=", 0) == 0) { power = std::stoi(val(9)); say("+OK"); }
    else if (c.rfind("AT+CPIN=", 0) == 0) say("+OK");
    else if (c.rfind("AT+SEND=", 0) == 0) {
      if (ignore_sends > 0) { --ignore_sends; return; }
      std::string rest = val(8);
      size_t c1 = rest.find(','), c2 = rest.find(',', c1 + 1);
      int dest = std::stoi(rest.substr(0, c1));
      size_t len = static_cast<size_t>(std::stoi(rest.substr(c1 + 1, c2 - c1 - 1)));
      std::string data = rest.substr(c2 + 1);
      if (data.size() != len) { say("+ERR=4"); return; }
      if (len > 240) { say("+ERR=12"); return; }
      air.emplace_back(dest, data);
      say("+OK");
    } else say("+ERR=2");
  }
  void rcv(int from, const std::string& data, int rssi = -71, int snr = 9) {
    say("+RCV=" + std::to_string(from) + "," + std::to_string(data.size()) + "," + data + "," + std::to_string(rssi) +
        "," + std::to_string(snr));
  }
};

std::vector<std::string> split_writes(const FakeHost& h) {
  std::vector<std::string> out;
  for (const auto& w : h.writes) out.push_back(ByteView(w).to_string());
  return out;
}

struct Rig {
  Clock clk;
  FakeHost host{&clk, 5};
  AtEmu emu;
  std::unique_ptr<Link> link;
  std::string err;
  Prng rng{13};
  size_t max_chunk = 0;

  explicit Rig(const std::string& cfg = "{}") {
    link = create_link("lora_at", cfg, host, &err);
    host.on_write = [this](ByteView v) { emu.from_host(v); };
  }
  void deliver() {
    std::string b;
    b.swap(emu.outbox);
    ByteView v(b);
    if (max_chunk == 0) { if (!b.empty()) link->on_io_bytes(v); return; }
    for (size_t off = 0; off < b.size();) {
      size_t n = 1 + rng.below(static_cast<uint32_t>(max_chunk));
      link->on_io_bytes(v.sub(off, n));
      off += n;
    }
  }
  void step(uint32_t ms) {
    clk.t += ms;
    deliver();
    link->tick(clk.t);
    deliver();
  }
  void run(uint32_t total, uint32_t step_ms = 10) {
    for (uint32_t t = 0; t < total; t += step_ms) step(step_ms);
  }
  bool run_until(const std::function<bool()>& p, uint32_t max_ms, uint32_t step_ms = 10) {
    for (uint32_t t = 0; t < max_ms; t += step_ms) {
      if (p()) return true;
      step(step_ms);
    }
    return p();
  }
  bool up(uint32_t max_ms = 3000) { return run_until([&] { return link->state() == LinkState::Up; }, max_ms); }
  Json status() const { return parse_json(link->status_json()); }
};

}  // namespace

TEST_SUITE("link") {

TEST_CASE("lora_at: init sequence strings are exact and flash is not rewritten needlessly") {
  Rig r;
  REQUIRE(r.link);
  r.link->on_io_state(true);
  CHECK(r.link->state() == LinkState::Connecting);
  REQUIRE(r.up());
  std::vector<std::string> want = {"AT\r\n", "AT+VER?\r\n", "AT+ADDRESS?\r\n", "AT+NETWORKID?\r\n",
                                   "AT+BAND=869525000\r\n", "AT+PARAMETER=9,7,1,12\r\n", "AT+CRFOP=14\r\n",
                                   "AT+PARAMETER?\r\n", "AT+BAND?\r\n"};
  CHECK(split_writes(r.host) == want);
  CHECK(r.emu.flash_writes == 0);
  Json st = r.status();
  CHECK(st.get_string("version") == "RYLR998_V1.2.3");
  CHECK(st.get_string("phase") == "running");

  Rig s("{\"address\":5,\"network_id\":6,\"password\":\"eedcaa90\"}");
  s.emu.net_reply_short = true;
  s.link->on_io_state(true);
  REQUIRE(s.up());
  auto w = split_writes(s.host);
  CHECK(std::find(w.begin(), w.end(), "AT+ADDRESS=5\r\n") != w.end());
  CHECK(std::find(w.begin(), w.end(), "AT+NETWORKID=6\r\n") != w.end());
  CHECK(std::find(w.begin(), w.end(), "AT+CPIN=EEDCAA90\r\n") != w.end());
  CHECK(s.emu.flash_writes == 2);
  s.link->on_io_state(false);
  CHECK(s.link->state() == LinkState::Down);
  s.link->on_io_state(true);
  REQUIRE(s.up());
  CHECK(s.emu.flash_writes == 2);  // values already stored: no second write
}

TEST_CASE("lora_at: caps and config validation") {
  Rig r;
  LinkCaps c = r.link->caps();
  CHECK(c.mtu == 177);
  CHECK(c.bitrate_bps == 1757);
  CHECK(c.half_duplex);
  CHECK(c.broadcast);
  CHECK(c.duty_cycle_permille == 100);
  CHECK(c.encryption_allowed);
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  CHECK(!create_link("lora_at", "{\"bw\":\"3\"}", h, &err));                      // RYLR998: 125..500 kHz only
  CHECK(create_link("lora_at", "{\"bw\":\"3\",\"model\":\"rylr896\",\"sf\":12}", h, &err));
  CHECK(!create_link("lora_at", "{\"sf\":10}", h, &err));                         // SF10 needs 250 kHz on RYLR998
  CHECK(create_link("lora_at", "{\"sf\":10,\"bw\":\"8\"}", h, &err));
  CHECK(!create_link("lora_at", "{\"network_id\":16}", h, &err));
  CHECK(!create_link("lora_at", "{\"network_id\":6,\"preamble\":8}", h, &err));
  CHECK(create_link("lora_at", "{\"preamble\":8}", h, &err));                     // ok with network 18
  CHECK(!create_link("lora_at", "{\"model\":\"rylr896\",\"power_dbm\":20}", h, &err));
  CHECK(!create_link("lora_at", "{\"password\":\"xyz12345\"}", h, &err));
  CHECK(!create_link("lora_at", "{\"password\":\"00000000\"}", h, &err));
  CHECK(!create_link("lora_at", "{\"band_hz\":915000000}", h, &err));             // outside eu868
  auto a = create_link("lora_at", "{\"region\":\"us915\",\"amateur\":true}", h, &err);
  REQUIRE(a);
  CHECK(a->caps().duty_cycle_permille == 1000);
  CHECK(!a->caps().encryption_allowed);
  CHECK(a->caps().requires_station_id);
}

TEST_CASE("lora_at: frames of every size, exact AT+SEND, mtu+1 refused") {
  Rig r;
  r.link->on_io_state(true);
  REQUIRE(r.up());
  REQUIRE(r.link->send(ByteView(Bytes{1, 2, 3}), 2));
  r.step(1);
  REQUIRE(!r.emu.cmds.empty());
  CHECK(r.emu.cmds.back() == "AT+SEND=0,8,EM1:AQID");
  Prng p(21);
  for (size_t n = 1; n <= 177; ++n) {
    Bytes f = p.bytes(n);
    REQUIRE(r.link->send(ByteView(f), 4));
    REQUIRE(r.run_until([&] { return r.emu.air.size() == n + 1; }, 10000));
    const std::string& d = r.emu.air.back().second;
    REQUIRE(d.compare(0, 4, "EM1:") == 0);
    CHECK(d.size() <= 240);
    Bytes back;
    REQUIRE(link::base64_decode(d.substr(4), &back, 300));
    CHECK(back == f);
  }
  CHECK(!r.link->send(ByteView(Bytes(178, 0)), 4));
  CHECK(!r.link->send(ByteView(Bytes()), 4));
  // one command in flight; the next waits for +OK and the time on air
  r.link->send(ByteView(Bytes(100, 1)), 4);
  r.link->send(ByteView(Bytes(100, 2)), 4);
  size_t before = r.emu.air.size();
  REQUIRE(r.run_until([&] { return r.emu.air.size() == before + 1; }, 5000));
  r.step(1);
  CHECK(r.emu.air.size() == before + 1);  // still on air (~700 ms at SF9)
  CHECK(r.link->tx_queue_depth() == 1);
  REQUIRE(r.run_until([&] { return r.emu.air.size() == before + 2; }, 5000));
}

TEST_CASE("lora_at: reception, RSSI/SNR, commas and newlines in payloads, malformed lines") {
  Rig r;
  r.max_chunk = 5;
  r.link->on_io_state(true);
  REQUIRE(r.up());
  Prng p(3);
  for (size_t n = 1; n <= 177; n += 4) {
    Bytes f = p.bytes(n);
    r.emu.rcv(12, "EM1:" + link::base64_encode(ByteView(f)), -101, -7);
    r.step(5);
    REQUIRE(!r.host.frames.empty());
    CHECK(r.host.frames.back().data == f);
  }
  CHECK(r.host.frames.back().meta.has_rssi);
  CHECK(r.host.frames.back().meta.rssi_dbm == doctest::Approx(-101));
  CHECK(r.host.frames.back().meta.snr_db == doctest::Approx(-7));
  size_t frames = r.host.frames.size();
  // foreign text with commas and an embedded CR LF
  r.emu.rcv(50, "hello, world\r\nline two,", -90, 3);
  r.step(5);
  REQUIRE(r.host.texts.size() == 1);
  CHECK(r.host.texts[0].from == "50");
  CHECK(r.host.texts[0].network == "lora_at");
  CHECK(r.host.texts[0].text == "hello, world  line two,");
  // the guide's spaced example
  r.emu.outbox += "+RCV=50, 5, HELLO, -99, 40\r\n";
  r.step(5);
  REQUIRE(r.host.texts.size() == 2);
  CHECK(r.host.texts[1].text == "HELLO");
  uint64_t errs = static_cast<uint64_t>(r.status().get_int("rx_errors"));
  r.emu.outbox += "+RCV=1,10,short,-50,5\r\n";   // length larger than data: keeps collecting, then rejected
  r.emu.outbox += "+RCV=1,3,abcdef,-50,5\r\n";   // inconsistent tail
  r.emu.outbox += "+RCV=1,9,EM1:@@@@@,-50,5\r\n";  // bad base64
  r.emu.outbox += "+RCV=99999999,1,a,-50,5\r\n";
  r.step(5);
  CHECK(static_cast<uint64_t>(r.status().get_int("rx_errors")) >= errs + 3);
  CHECK(r.host.frames.size() == frames);
  // interop off
  Rig q("{\"interop\":false}");
  q.link->on_io_state(true);
  REQUIRE(q.up());
  q.emu.rcv(50, "plain");
  q.step(5);
  CHECK(q.host.texts.empty());
}

TEST_CASE("lora_at: a corrupt +RCV claiming more data than it has does not swallow the next line") {
  for (int chunk : {1, 3, 64}) {
    CAPTURE(chunk);
    Rig r;
    r.max_chunk = static_cast<size_t>(chunk);
    r.link->on_io_state(true);
    REQUIRE(r.up());
    Prng p(static_cast<uint64_t>(40 + chunk));
    uint64_t errs = static_cast<uint64_t>(r.status().get_int("rx_errors"));
    // 1) corrupt header followed by a real frame
    Bytes f = p.bytes(60);
    r.emu.outbox += "+RCV=7,200,EM1:AAAA\r\n";
    r.emu.rcv(12, "EM1:" + link::base64_encode(ByteView(f)), -80, 4);
    r.step(5);
    REQUIRE(r.host.frames.size() == 1);
    CHECK(r.host.frames[0].data == f);
    CHECK(r.host.frames[0].meta.rssi_dbm == doctest::Approx(-80));
    // 2) two corrupt headers in a row, then a real frame
    Bytes g = p.bytes(10);
    r.emu.outbox += "+RCV=7,150,xx\r\n+RCV=8,99,yy,-1\r\n";
    r.emu.rcv(13, "EM1:" + link::base64_encode(ByteView(g)), -70, 2);
    r.step(5);
    REQUIRE(r.host.frames.size() == 2);
    CHECK(r.host.frames[1].data == g);
    CHECK(static_cast<uint64_t>(r.status().get_int("rx_errors")) >= errs + 3);
    CHECK(r.status().get_int("rx_resyncs") >= 3);
    // 3) corrupt header right before the +OK of a send: the send completes without a timeout
    r.emu.outbox += "+RCV=9,240,partial\r\n";
    r.step(1);
    size_t air = r.emu.air.size();
    REQUIRE(r.link->send(ByteView(Bytes{9, 9, 9}), 2));
    REQUIRE(r.run_until([&] { return r.emu.air.size() == air + 1; }, 3000));
    REQUIRE(r.link->send(ByteView(Bytes{8, 8}), 2));
    CHECK(r.run_until([&] { return r.emu.air.size() == air + 2; }, 3000));  // no 'no answer' stall
    CHECK(r.link->state() == LinkState::Up);
    // 4) genuine newline inside foreign text still works
    r.emu.rcv(50, "one\r\ntwo", -90, 3);
    r.step(5);
    REQUIRE(!r.host.texts.empty());
    CHECK(r.host.texts.back().text == "one  two");
  }
}

TEST_CASE("lora_at: send_interop_text sends plain text") {
  Rig r;
  r.link->on_io_state(true);
  REQUIRE(r.up());
  InteropText t;
  t.to = "50";
  t.text = "hi there\n";
  t.network = "lora_at";
  REQUIRE(r.link->send_interop_text(t));
  r.step(1);
  CHECK(r.emu.cmds.back() == "AT+SEND=50,8,hi there");
  t.to = "";
  t.text = "EM1:spoof";
  REQUIRE(r.link->send_interop_text(t));
  r.run(3000);
  CHECK(r.emu.air.back().first == 0);
  CHECK(r.emu.air.back().second == " EM1:spoof");
  t.to = "abc";
  CHECK(!r.link->send_interop_text(t));
  t.to = "70000";
  CHECK(!r.link->send_interop_text(t));
  t.to = "";
  t.network = "aprs";
  CHECK(!r.link->send_interop_text(t));
  t.network = "";
  t.text = "\r\n";
  CHECK(!r.link->send_interop_text(t));
}

TEST_CASE("lora_at: module errors, verify mismatch and recovery") {
  Rig r;
  r.emu.err_for["AT+PARAMETER="] = 17;
  r.link->on_io_state(true);
  REQUIRE(r.run_until([&] { return r.link->state() == LinkState::Error; }, 3000));
  CHECK(r.status().get_string("last_error").find("+ERR=17") != std::string::npos);
  r.emu.err_for.clear();
  REQUIRE(r.up(15000));  // retried after the idle period
  // TX error is counted and the queue keeps moving
  r.emu.err_for["AT+SEND="] = 5;
  r.link->send(ByteView(Bytes(10, 1)), 4);
  r.step(5);
  r.emu.err_for.clear();
  r.link->send(ByteView(Bytes(10, 2)), 4);
  REQUIRE(r.run_until([&] { return r.emu.air.size() == 1; }, 3000));
  CHECK(r.status().get_int("tx_failed") == 1);
  CHECK(r.link->state() == LinkState::Up);

  Rig m;
  m.emu.report_param = "12,7,1,12";
  m.link->on_io_state(true);
  REQUIRE(m.run_until([&] { return m.link->state() == LinkState::Error; }, 3000));
  CHECK(m.status().get_string("last_error").find("parameters") != std::string::npos);
}

TEST_CASE("lora_at: timeouts, retries, reboot mid-session") {
  Rig r;
  r.emu.alive = false;
  r.link->on_io_state(true);
  REQUIRE(r.run_until([&] { return r.link->state() == LinkState::Error; }, 10000));
  size_t probes = 0;
  for (const auto& c : r.emu.cmds) probes += c == "AT";
  CHECK(probes == 3);
  CHECK(r.status().get_string("last_error").find("no answer") != std::string::npos);
  r.emu.alive = true;
  REQUIRE(r.up(15000));
  // module stops answering AT+SEND three times -> re-initialise
  r.emu.ignore_sends = 3;
  for (int i = 0; i < 3; ++i) r.link->send(ByteView(Bytes(30, static_cast<uint8_t>(i))), 4);
  uint64_t reinits = static_cast<uint64_t>(r.status().get_int("reinits"));
  r.run(15000, 20);
  CHECK(static_cast<uint64_t>(r.status().get_int("reinits")) == reinits + 1);
  CHECK(r.status().get_int("tx_timeouts") == 3);
  CHECK(r.link->state() == LinkState::Up);
  // reboot announcement
  size_t n = r.emu.cmds.size();
  r.emu.say("+READY");
  r.step(1);
  REQUIRE(r.up());
  CHECK(r.emu.cmds.size() > n);
  CHECK(r.emu.cmds[n] == "AT");
  // LF-only line endings and leading noise are tolerated
  r.emu.nl = "\n";
  r.emu.outbox += std::string("\x00\xff", 2);
  r.emu.rcv(3, "lf only");
  r.step(1);
  CHECK(r.host.texts.back().text == "lf only");
}

TEST_CASE("lora_at: priority order and queue limits") {
  Rig r("{\"queue_frames\":3}");
  CHECK(r.link->send(ByteView(Bytes{6}), 6));
  CHECK(r.link->send(ByteView(Bytes{2}), 2));
  CHECK(r.link->send(ByteView(Bytes{4}), 4));
  CHECK(!r.link->send(ByteView(Bytes{7}), 7));
  CHECK(r.link->send(ByteView(Bytes{0}), 0));
  CHECK(r.link->tx_queue_depth() == 3);
  r.link->on_io_state(true);
  REQUIRE(r.up());
  REQUIRE(r.run_until([&] { return r.emu.air.size() == 3; }, 10000));
  Bytes b;
  link::base64_decode(r.emu.air[0].second.substr(4), &b, 10);
  CHECK(b == Bytes{0});
  link::base64_decode(r.emu.air[1].second.substr(4), &b, 10);
  CHECK(b == Bytes{2});
  link::base64_decode(r.emu.air[2].second.substr(4), &b, 10);
  CHECK(b == Bytes{4});
}

TEST_CASE("lora_at: reconfigure re-initialises when connected") {
  Rig r;
  r.link->on_io_state(true);
  REQUIRE(r.up());
  std::string err;
  CHECK(!r.link->reconfigure("{\"sf\":3}", &err));
  CHECK(r.link->state() == LinkState::Up);
  CHECK(r.link->reconfigure("{\"region\":\"custom\",\"band_hz\":868100000,\"sf\":7}", &err));
  CHECK(r.link->state() == LinkState::Connecting);
  REQUIRE(r.up());
  CHECK(r.emu.band == 868100000);
  CHECK(r.emu.param == "7,7,1,12");
  CHECK(r.link->caps().duty_cycle_permille == 1000);
}

TEST_CASE("lora_at: garbage and truncated input fuzz") {
  Prng p(99);
  const char* heads[] = {"+RCV=", "+OK", "+ERR=", "+READY", "+PARAMETER=", "+BAND=", "+ADDRESS=", "+NETWORKID=", "+VER="};
  for (int round = 0; round < 30; ++round) {
    Rig r;
    r.link->on_io_state(true);
    for (int i = 0; i < 200; ++i) {
      std::string s;
      uint32_t k = p.below(4);
      if (k == 0) { Bytes b = p.bytes(p.below(80)); s.assign(b.begin(), b.end()); }
      else if (k == 1) s = std::string(heads[p.below(9)]) + std::to_string(p.below(300)) + "," + std::to_string(p.below(300)) + ",EM1:" + link::base64_encode(ByteView(p.bytes(p.below(60)))) + "," + std::to_string(p.below(200)) + "\r\n";
      else if (k == 2) s = std::string(p.below(900), 'A');
      else { Bytes b = p.bytes(p.below(30)); s = "+RCV=" + std::string(b.begin(), b.end()) + "\n"; }
      r.emu.outbox += s;
      if (p.below(6) == 0) r.link->send(ByteView(p.bytes(1 + p.below(177))), static_cast<uint8_t>(p.below(8)));
      r.step(p.below(80));
    }
    Json st;
    CHECK(Json::parse(r.link->status_json(), st));
  }
}

TEST_CASE("lora_at: end to end between two drivers over emulated modules") {
  Rig a("{\"address\":1}"), b("{\"address\":2}");
  a.max_chunk = 9;
  b.max_chunk = 2;
  a.link->on_io_state(true);
  b.link->on_io_state(true);
  REQUIRE(a.up());
  REQUIRE(b.up());
  Prng p(5);
  size_t sent = 0;
  for (size_t n = 1; n <= 177; n += 5) {
    Bytes f = p.bytes(n);
    REQUIRE(a.link->send(ByteView(f), 3));
    ++sent;
    for (int i = 0; i < 400 && a.emu.air.size() < sent; ++i) a.step(10);
    REQUIRE(a.emu.air.size() == sent);
    b.emu.rcv(1, a.emu.air.back().second, -80, 6);
    b.step(5);
    REQUIRE(b.host.frames.size() == sent);
    CHECK(b.host.frames.back().data == f);
  }
}

}  // TEST_SUITE
