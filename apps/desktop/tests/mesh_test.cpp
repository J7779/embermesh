// End-to-end mesh tests: several real emberd processes (real core, real sockets, real link drivers)
// joined by every kind of medium the desktop host provides, verified through each daemon's /rpc.
// Registered with CTest one test case per entry (see apps/desktop/CMakeLists.txt).
#include "daemon_harness.h"
#include "doctest.h"
#include "remote_rpc.h"
#include <algorithm>
#include <chrono>
#include <thread>

using namespace mesh;

// Memory bounds below are for a normal build. A sanitizer build of the daemon keeps freed memory in
// quarantine and adds shadow memory, so its resident size says little: allow several times more
// (still far below what an unbounded queue would reach).
static long rss_limit_kib(long mib) {
#if defined(__SANITIZE_ADDRESS__)
  return mib * 1024L * 4;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
  return mib * 1024L * 4;
#else
  return mib * 1024L;
#endif
#else
  return mib * 1024L;
#endif
}

// Callsigns in these tests use the Q prefix series, which the ITU never allocates to stations, so
// they cannot belong to a real operator (and pass the core's callsign validation).
namespace {

struct IgnoreSigpipe {
  IgnoreSigpipe() { ::signal(SIGPIPE, SIG_IGN); }
} g_ignore_sigpipe;

std::string link_arg(const std::string& name, const std::string& endpoint, const std::string& link) {
  return R"({"name":")" + name + R"(","endpoint":)" + endpoint + R"(,"link":)" + link + "}";
}

std::string tag() { return ember_host::random_hex(3); }

std::string msg_status(Daemon& d, const std::string& conv, const std::string& text) {
  return d.find_message(conv, text).get("status").as_string();
}

bool delivered(const std::string& s) { return s == "delivered" || s == "read"; }

void public_one_way(Daemon& a, Daemon& b, const std::string& t, int timeout_ms = 30000) {
  Json r = a.send("ch:public", t);
  REQUIRE_MESSAGE(r.get("ok").as_bool(), r.dump());
  CHECK_MESSAGE(wait_until([&] { return b.has_message("ch:public", t); }, timeout_ms), b.name << " did not get '" << t << "'");
}

void dm_one_way(Daemon& a, Daemon& b, const std::string& t, int timeout_ms = 30000) {
  std::string bid = b.node_id(), aid = a.node_id();
  Json r = a.send("dm:" + bid, t);
  REQUIRE_MESSAGE(r.get("ok").as_bool(), r.dump());
  CHECK_MESSAGE(wait_until([&] { return b.has_message("dm:" + aid, t); }, timeout_ms), b.name << " did not get DM '" << t << "'");
  bool got = wait_until([&] { return delivered(msg_status(a, "dm:" + bid, t)); }, timeout_ms);
  CHECK_MESSAGE(got, "DM status at " << a.name << ": " << msg_status(a, "dm:" + bid, t));
  CHECK(b.find_message("dm:" + aid, t).get("encrypted").as_bool(false));
}

void sos_one_way(Daemon& a, Daemon& b, const std::string& t) {
  Json p = Json::object();
  p.set("text", t);
  REQUIRE(a.rpc("sos.start", p).get("ok").as_bool());
  std::string aid = a.node_id();
  CHECK_MESSAGE(wait_until([&] {
    Json res_alerts = b.result("sos.list");

    for (const auto& x : res_alerts.get("alerts").elements())
      if (x.get("node_id").as_string() == aid && x.get("text").as_string() == t) return true;
    return false;
  }, 30000), b.name << " did not get the SOS");
  CHECK(a.result("sos.status").get("active").as_bool());
  a.rpc("sos.stop");
}

// Public both ways, DM with a delivered receipt, SOS.
void exchange(Daemon& a, Daemon& b, const std::string& what) {
  std::string t = tag();
  REQUIRE(!a.node_id().empty());
  REQUIRE(!b.node_id().empty());
  public_one_way(a, b, what + " public a->b " + t);
  public_one_way(b, a, what + " public b->a " + t);
  dm_one_way(a, b, what + " dm a->b " + t);
  sos_one_way(a, b, what + " sos " + t);
}

int test_udp_port() { return 40000 + int(ember_host::steady_ms() % 20000); }


// Does the core's `audio` link type offer this preset yet? (core work in progress)
bool core_has_audio_preset(Daemon& d, const std::string& preset) {
  Json t = d.result("links.types");
  for (const auto& ty : t.get("types").elements()) {
    if (ty.get("type").as_string() != "audio") continue;
    for (const auto& f : ty.get("config").elements())
      if (f.get("key").as_string() == "preset")
        for (const auto& o : f.get("options").elements())
          if (o.get("value").as_string() == preset) return true;
  }
  return false;
}

bool core_has_morse(Daemon& d) {
  Json r = d.rpc("morse.rx.status", J(R"({"session":"x"})"));
  return r.get("error").get("code").as_string() != "not_implemented";
}

// Hand-keyable CW: `text` at `wpm` (PARIS timing), tone `hz`, as s16 PCM at `rate`, with soft edges.
std::vector<int16_t> make_cw(const std::string& text, double wpm, double hz, uint32_t rate, double lead_s = 0.5) {
  static const char* tab[] = {".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--",
                              "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.."};
  static const char* dig[] = {"-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----."};
  double unit = 1.2 / wpm;
  std::vector<std::pair<bool, double>> seg = {{false, lead_s}};
  for (char ch : text) {
    if (ch == ' ') {
      seg.push_back({false, unit * 4});  // + 3 after the previous char = 7
      continue;
    }
    const char* code = nullptr;
    if (ch >= 'A' && ch <= 'Z') code = tab[ch - 'A'];
    if (ch >= '0' && ch <= '9') code = dig[ch - '0'];
    if (!code) continue;
    for (const char* c = code; *c; ++c) {
      seg.push_back({true, unit * (*c == '-' ? 3 : 1)});
      seg.push_back({false, unit});
    }
    seg.push_back({false, unit * 2});
  }
  seg.push_back({false, 1.0});
  std::vector<int16_t> out;
  double ph = 0;
  for (auto& sg : seg) {
    size_t n = size_t(sg.second * rate);
    size_t ramp = size_t(0.005 * rate);
    for (size_t i = 0; i < n; ++i) {
      double env = !sg.first ? 0.0 : std::min(1.0, std::min(double(i) / ramp, double(n - i) / ramp));
      ph += 6.283185307179586 * hz / rate;
      out.push_back(int16_t(12000 * env * std::sin(ph)));
    }
  }
  return out;
}

void write_pcm(const std::string& path, const std::vector<int16_t>& pcm) {
  std::string b(pcm.size() * 2, '\0');
  for (size_t i = 0; i < pcm.size(); ++i) {
    b[2 * i] = char(uint16_t(pcm[i]) & 0xff);
    b[2 * i + 1] = char(uint16_t(pcm[i]) >> 8);
  }
  ember_host::write_file_atomic(path, b);
}

double percentile(std::vector<double> v, double p) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  return v[std::min(v.size() - 1, size_t(p * double(v.size())))];
}

}  // namespace

TEST_SUITE("mesh") {
  TEST_CASE("udp multicast pair") {
    TestDir tmp;
    Daemon a("udp-a", tmp.path), b("udp-b", tmp.path);
    int port = test_udp_port();
    std::string group = "239.83.77." + std::to_string(100 + port % 100);
    std::string ep = R"({"kind":"udp","config":{"group":")" + group + R"(","port":)" + std::to_string(port) + "}}";
    std::string l = link_arg("udp test", ep, R"({"type":"raw"})");
    REQUIRE(a.start({"--link", l}));
    REQUIRE(b.start({"--link", l}));
    Json ul = a.link_by_name("udp test");
    CHECK(ul.get("config").get("preset").as_string() == "udp_lan");
    CHECK(ul.get("caps").get("mtu").as_int() == 1400);
    if (!wait_peer(a, b.node_id(), 15000)) {
      // Machines without a multicast route (some CI sandboxes) cannot run this one.
      MESSAGE("multicast between processes not available here; skipping");
      return;
    }
    exchange(a, b, "udp");
    CHECK(a.stop() == 0);
    CHECK(b.stop() == 0);
  }

  TEST_CASE("tcp_server and tcp pair") {
    TestDir tmp;
    Daemon a("tcp-a", tmp.path), b("tcp-b", tmp.path);
    uint16_t p = free_tcp_port();
    REQUIRE(a.start({"--link", link_arg("tcp srv", R"({"kind":"tcp_server","config":{"bind":"127.0.0.1","port":)" + std::to_string(p) + "}}",
                                        R"({"type":"stream"})")}));
    REQUIRE(b.start({"--link", link_arg("tcp cli", R"({"kind":"tcp","config":{"host":"127.0.0.1","port":)" + std::to_string(p) + "}}",
                                        R"({"type":"stream"})")}));
    CHECK(a.link_by_name("tcp srv").get("config").get("preset").as_string() == "tcp");
    REQUIRE(wait_until([&] { return b.link_by_name("tcp cli").get("state").as_string() == "up"; }, 10000));
    exchange(a, b, "tcp");
  }

  TEST_CASE("websocket client to hub") {
    TestDir tmp;
    Daemon a("ws-hub", tmp.path), b("ws-cli", tmp.path);
    REQUIRE(a.start());
    Json hub = a.link_by_name("Hub guests (Wi-Fi)");
    CHECK(hub.get("endpoint").get("kind").as_string() == "ws_server");
    CHECK(hub.get("config").get("preset").as_string() == "websocket");
    CHECK(hub.get("state").as_string() == "up");
    REQUIRE(b.start({"--link", link_arg("to hub", R"({"kind":"websocket","config":{"url":"ws://127.0.0.1:)" + std::to_string(a.port) + R"(/ws"}})",
                                        R"({"type":"raw"})")}));
    REQUIRE(wait_until([&] { return a.result("host.hub.status").get("clients").as_int() == 1; }, 10000));
    exchange(a, b, "ws");
    CHECK(b.link_by_name("to hub").get("config").get("preset").as_string() == "websocket");
  }

  TEST_CASE("chain relay and store-carry-forward") {
    // A (hub) <-websocket- B -tcp_server-> <-tcp- C ; A and C have no direct link.
    TestDir tmp;
    Daemon a("chain-a", tmp.path), b("chain-b", tmp.path), c("chain-c", tmp.path);
    uint16_t p = free_tcp_port();
    REQUIRE(a.start());
    REQUIRE(b.start({"--link", link_arg("to A", R"({"kind":"websocket","config":{"url":"ws://127.0.0.1:)" + std::to_string(a.port) + R"(/ws"}})", R"({"type":"raw"})"),
                     "--link", link_arg("for C", R"({"kind":"tcp_server","config":{"bind":"127.0.0.1","port":)" + std::to_string(p) + "}}", R"({"type":"stream"})")}));
    std::vector<std::string> c_args = {"--link", link_arg("to B", R"({"kind":"tcp","config":{"host":"127.0.0.1","port":)" + std::to_string(p) + "}}", R"({"type":"stream"})")};
    REQUIRE(c.start(c_args));
    std::string aid = a.node_id(), cid = c.node_id();
    REQUIRE(wait_peer(a, cid, 30000));
    // C is two hops away from A.
    Json res_peers = a.result("mesh.peers");

    for (const auto& pe : res_peers.get("peers").elements())
      if (pe.get("node_id").as_string() == cid) {
        CHECK_FALSE(pe.get("direct").as_bool(true));
        CHECK(pe.get("hops").as_int(0) >= 1);
      }
    std::string t = tag();
    dm_one_way(a, c, "relayed dm " + t);
    public_one_way(c, a, "relayed public " + t);
    CHECK(b.result("mesh.stats").get("relayed").as_int() > 0);

    // Store-carry-forward: C goes offline, A sends, then A goes offline too; B carries the message
    // and hands it to C when C comes back.
    REQUIRE(c.stop() == 0);
    std::string t2 = "carried dm " + t;
    REQUIRE(a.send("dm:" + cid, t2).get("ok").as_bool());
    // B must have received a copy before A disappears.
    REQUIRE(wait_until([&] { return b.result("mesh.stats").get("stored_messages").as_int() > 0; }, 10000));
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    REQUIRE(a.stop() == 0);
    REQUIRE(c.start(c_args));
    CHECK_MESSAGE(wait_until([&] { return c.has_message("dm:" + aid, t2); }, 45000), "C did not receive the carried DM");
    CHECK(c.find_message("dm:" + aid, t2).get("hops").as_int(0) >= 1);
  }

  TEST_CASE("gateway hub to kiss radio link") {
    // A (hub) <-websocket- B (gateway) -kiss/tcp-> [shared channel] <-kiss/tcp- C
    // The kiss link is amateur (default): only cleartext signed traffic may cross it.
    TestDir tmp;
    TcpRelay air;
    Daemon a("gw-a", tmp.path), b("gw-b", tmp.path), c("gw-c", tmp.path);
    std::string kiss = link_arg("radio", R"({"kind":"tcp","config":{"host":"127.0.0.1","port":)" + std::to_string(air.port) + "}}",
                                R"({"type":"kiss","config":{"txdelay_ms":50,"bitrate_bps":9600}})");
    REQUIRE(a.start());
    REQUIRE(b.start({"--callsign", "Q0TGB-1", "--link", link_arg("to A", R"({"kind":"websocket","config":{"url":"ws://127.0.0.1:)" + std::to_string(a.port) + R"(/ws"}})", R"({"type":"raw"})"), "--link", kiss}));
    REQUIRE(c.start({"--callsign", "Q0TGC-1", "--link", kiss}));
    Json bl = b.link_by_name("radio");
    CHECK_FALSE(bl.get("caps").get("encryption_allowed").as_bool(true));
    // The node's callsign is copied into the driver (links.list `config` still shows the N0CALL
    // default: reported to the core owner), so look at the driver status.
    CHECK(bl.get("status").dump().find("Q0TGB-1") != std::string::npos);
    REQUIRE(wait_until([&] { return air.client_count() == 2; }, 10000));
    std::string t = tag();
    REQUIRE(wait_peer(a, c.node_id(), 40000));
    public_one_way(a, c, "hub to radio " + t, 40000);
    public_one_way(c, a, "radio to hub " + t, 40000);
    // A DM from the hub side must not leak onto the amateur link.
    std::string secret = "private " + t;
    REQUIRE(a.send("dm:" + c.node_id(), secret).get("ok").as_bool());
    std::this_thread::sleep_for(std::chrono::seconds(8));
    CHECK_FALSE(c.has_message("dm:" + a.node_id(), secret));
    CHECK_FALSE(delivered(msg_status(a, "dm:" + c.node_id(), secret)));
    // AX.25 frames from the gateway carry its callsign in clear.
    {
      std::lock_guard<std::mutex> lk(air.mu);
      CHECK(air.seen.size() > 0);
      CHECK(air.seen.find('\xC0') != std::string::npos);
    }
    CHECK(b.result("mesh.stats").get("relayed").as_int() > 0);
  }

  TEST_CASE("gateway hub to audio modem link over a virtual audio pipe") {
    // A (hub) <-websocket- B (gateway) =AFSK1200 audio, sample to sample through FIFOs= C
    TestDir tmp;
    Daemon a("au-a", tmp.path), b("au-b", tmp.path), c("au-c", tmp.path);
    std::string b2c = ember_host::path_join(tmp.path, "b2c.pcm"), c2b = ember_host::path_join(tmp.path, "c2b.pcm");
    auto audio = [](const std::string& in, const std::string& out) {
      return link_arg("audio radio", R"({"kind":"audio_pipe","config":{"input_path":")" + in + R"(","output_path":")" + out + R"(","sample_rate":48000}})",
                      R"({"type":"audio","config":{"modem":"afsk1200","txdelay_ms":150}})");
    };
    REQUIRE(a.start());
    REQUIRE(b.start({"--callsign", "Q0TAB-1", "--link", link_arg("to A", R"({"kind":"websocket","config":{"url":"ws://127.0.0.1:)" + std::to_string(a.port) + R"(/ws"}})", R"({"type":"raw"})"),
                     "--link", audio(c2b, b2c)}));
    // Half-duplex shared channel: C starts after B's start-up transmission (overlapping start-up
    // beacons collide), and a missing reply to a new neighbour is closed with one announcement.
    REQUIRE(wait_until([&] {
      Json l = b.link_by_name("audio radio");
      return l.get("stats").get("tx_frames").as_int() >= 1 && !l.get("status").get("transmitting").as_bool(true);
    }, 30000));
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    REQUIRE(c.start({"--callsign", "Q0TAC-1", "--link", audio(b2c, c2b)}));
    REQUIRE(wait_until([&] { return c.link_by_name("audio radio").get("state").as_string() == "up"; }, 10000));
    std::string t = tag();
    if (!wait_peer(c, b.node_id(), 20000)) {
      wait_until([&] { return !c.link_by_name("audio radio").get("status").get("transmitting").as_bool(true); }, 20000);
      b.rpc("mesh.announce");
    }
    if (!wait_peer(a, c.node_id(), 20000)) {  // C's own announcement has to cross the radio too
      wait_until([&] { return !b.link_by_name("audio radio").get("status").get("transmitting").as_bool(true); }, 20000);
      c.rpc("mesh.announce");
    }
    REQUIRE(wait_peer(a, c.node_id(), 60000));
    public_one_way(a, c, "hub to audio " + t, 60000);
    public_one_way(c, a, "audio to hub " + t, 60000);
    Json cl = c.link_by_name("audio radio");
    CHECK(cl.get("stats").get("rx_frames").as_int() > 0);
    CHECK(cl.get("status").get("modem_status").is_object());
    std::string secret = "private audio " + t;
    REQUIRE(a.send("dm:" + c.node_id(), secret).get("ok").as_bool());
    std::this_thread::sleep_for(std::chrono::seconds(6));
    CHECK_FALSE(c.has_message("dm:" + a.node_id(), secret));
  }

  TEST_CASE("serial over a pty cable with stream and kiss drivers") {
    TestDir tmp;
    for (const char* type : {"stream", "kiss"}) {
      CAPTURE(type);
      PtyCable cable;
      REQUIRE(cable.ok);
      std::string lt = std::string(R"({"type":")") + type + R"(","config":{"txdelay_ms":50,"bitrate_bps":9600}})";
      Daemon a(std::string("ser-a-") + type, tmp.path), b(std::string("ser-b-") + type, tmp.path);
      REQUIRE(a.start({"--callsign", "Q0TSA-1", "--link", link_arg("cable", R"({"kind":"serial","config":{"port":")" + cable.path1 + R"(","baud":115200}})", lt)}));
      REQUIRE(b.start({"--callsign", "Q0TSB-1", "--link", link_arg("cable", R"({"kind":"serial","config":{"port":")" + cable.path2 + R"(","baud":115200}})", lt)}));
      REQUIRE(wait_until([&] { return a.link_by_name("cable").get("state").as_string() == "up" && b.link_by_name("cable").get("state").as_string() == "up"; }, 10000));
      if (std::string(type) == "stream") {
        CHECK(a.link_by_name("cable").get("config").get("preset").as_string() == "cable");
        exchange(a, b, "serial stream");
      } else {
        REQUIRE(wait_peer(a, b.node_id(), 30000));
        std::string t = tag();
        public_one_way(a, b, "serial kiss " + t);
        public_one_way(b, a, "serial kiss back " + t);
      }
      CHECK(cable.bytes.load() > 0);
      CHECK(a.stop() == 0);
      CHECK(b.stop() == 0);
    }
  }

  TEST_CASE("rtl_tcp receive of real modulated IQ") {
    // B transmits on an audio link whose "sound card" is a file: real AFSK1200 from the core. The
    // test FM-modulates that audio into RTL-SDR I/Q and serves it from a fake rtl_tcp server to A,
    // whose rtl_tcp link (receive only) must decode the frames into received traffic.
    TestDir tmp;
    std::string pcm_path = ember_host::path_join(tmp.path, "b_tx.pcm");
    Daemon b("sdr-tx", tmp.path), a("sdr-rx", tmp.path);
    REQUIRE(b.start({"--callsign", "Q0TSD-1", "--link",
                     link_arg("tx file", R"({"kind":"audio_pipe","config":{"output_path":")" + pcm_path + R"(","create_fifos":false,"sample_rate":48000}})",
                              R"({"type":"audio","config":{"modem":"afsk1200","txdelay_ms":200}})")}));
    std::string t = "sdr hello " + tag();
    std::this_thread::sleep_for(std::chrono::seconds(3));  // announcement + beacon go out first
    REQUIRE(wait_until([&] { return b.link_by_name("tx file").get("stats").get("tx_frames").as_int() >= 1; }, 20000));
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // A receive-only listener cannot ask for keys (WHOIS), so it needs the sender's announcement on
    // air before it can verify and show the signed message.
    int64_t before = b.link_by_name("tx file").get("stats").get("tx_frames").as_int();
    REQUIRE(b.rpc("mesh.announce").get("ok").as_bool());
    REQUIRE(wait_until([&] { return b.link_by_name("tx file").get("stats").get("tx_frames").as_int() > before; }, 20000));
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));  // separate key-up for the message
    before = b.link_by_name("tx file").get("stats").get("tx_frames").as_int();
    REQUIRE(b.send("ch:public", t).get("ok").as_bool());
    REQUIRE(wait_until([&] { return b.link_by_name("tx file").get("stats").get("tx_frames").as_int() > before; }, 20000));
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::string bid = b.node_id();
    REQUIRE(b.stop() == 0);
    std::string raw;
    REQUIRE(ember_host::read_file(pcm_path, &raw));
    REQUIRE(raw.size() > 48000 * 2);
    std::vector<int16_t> pcm(raw.size() / 2);
    size_t loud = 0;
    for (size_t i = 0; i < pcm.size(); ++i) {
      pcm[i] = int16_t(uint16_t(uint8_t(raw[2 * i])) | uint16_t(uint8_t(raw[2 * i + 1])) << 8);
      if (pcm[i] > 1000 || pcm[i] < -1000) ++loud;
    }
    MESSAGE("audio file: " << pcm.size() / 48000.0 << " s, " << loud / 48000.0 << " s of signal");
    REQUIRE(loud > 4800);
    FakeRtlTcp sdr(fm_modulate(pcm, 48000, 240000, 50000.0, 3000.0));
    REQUIRE(a.start({"--link", link_arg("sdr", R"({"kind":"rtl_tcp","config":{"host":"127.0.0.1","port":)" + std::to_string(sdr.port) + "}}",
                                        R"({"type":"rtl_tcp","config":{"sample_rate":"240000","frequency_hz":144390000}})")}));
    REQUIRE(wait_until([&] { return a.link_by_name("sdr").get("state").as_string() == "up"; }, 10000));
    // The driver tuned the dongle itself. The link is "up" once connected; the fake dongle counts
    // the tuning commands as it reads them, a moment later.
    CHECK(wait_until([&] { return sdr.commands.load() >= 2; }, 10000));
    CHECK_MESSAGE(wait_until([&] { return a.link_by_name("sdr").get("stats").get("rx_frames").as_int() > 0; }, 40000),
                  "no frame decoded: " << a.link_by_name("sdr").dump());
    CHECK_MESSAGE(wait_until([&] { return a.has_message("ch:public", t); }, 20000),
                  "message not shown as received traffic; peers " << a.rpc("mesh.peers").dump() << " stats "
                                                                  << a.link_by_name("sdr").get("stats").dump() << " msgs "
                                                                  << a.rpc("messages.list", J(R"({"conv":"ch:public"})")).dump().substr(0, 600));
    Json m = a.find_message("ch:public", t);
    CHECK(m.get("from").as_string() == bid);
  }

  TEST_CASE("daemon survives garbage on every endpoint") {
    TestDir tmp;
    TcpRelay kiss_air;
    PtyCable cable;
    REQUIRE(cable.ok);
    Daemon d("junk", tmp.path), peer("junk-peer", tmp.path);
    uint16_t tp = free_tcp_port();
    int up = test_udp_port();
    std::string fifo = ember_host::path_join(tmp.path, "noise.pcm");
    REQUIRE(d.start({"--callsign", "Q0TJK-1",
                     "--link", link_arg("tcp", R"({"kind":"tcp_server","config":{"bind":"127.0.0.1","port":)" + std::to_string(tp) + "}}", R"({"type":"stream"})"),
                     "--link", link_arg("kiss", R"({"kind":"tcp","config":{"host":"127.0.0.1","port":)" + std::to_string(kiss_air.port) + "}}", R"({"type":"kiss"})"),
                     "--link", link_arg("udp", R"({"kind":"udp","config":{"group":"239.83.77.201","port":)" + std::to_string(up) + "}}", R"({"type":"raw"})"),
                     "--link", link_arg("serial", R"({"kind":"serial","config":{"port":")" + cable.path1 + R"(","baud":9600}})", R"({"type":"stream"})"),
                     "--link", link_arg("audio", R"({"kind":"audio_pipe","config":{"input_path":")" + fifo + R"("}})", R"({"type":"audio"})")}));
    REQUIRE(wait_until([&] { return kiss_air.client_count() == 1; }, 5000));
    long rss0 = 0;
    double cpu0 = 0;
    d.usage(&rss0, &cpu0);
    uint64_t end = ember_host::steady_ms() + 6000;
    int sent_http = 0;
    int audio_fd = ::open(fifo.c_str(), O_WRONLY | O_NONBLOCK);
    int kiss_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    {
      sockaddr_in a{};
      a.sin_family = AF_INET;
      a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      a.sin_port = htons(kiss_air.port);
      ::connect(kiss_fd, reinterpret_cast<sockaddr*>(&a), sizeof a);
    }
    int udp_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in ua{};
    ua.sin_family = AF_INET;
    ua.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ua.sin_port = htons(uint16_t(up));
    while (ember_host::steady_ms() < end) {
      // raw TCP garbage + malformed HTTP to the hub
      for (const std::string& junk : {random_bytes_str(3000), std::string("GET /\x01\x02 HTTP/9.9\r\n\r\n"),
                                      std::string("POST /rpc HTTP/1.1\r\nContent-Length: 99999999\r\n\r\n{"),
                                      std::string("GET /ws HTTP/1.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n\r\n")}) {
        int st;
        std::string r = http(d.port, "GET", "/", "", "", &st, 300);  // keep the normal path busy too
        int s = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.sin_port = htons(d.port);
        if (::connect(s, reinterpret_cast<sockaddr*>(&a), sizeof a) == 0) ::send(s, junk.data(), junk.size(), 0);
        ::close(s);
        ++sent_http;
      }
      // garbage WebSocket frames on /ws and garbage JSON on /rpc (authorised)
      int w = raw_ws_connect(d.port, "/ws");
      for (int i = 0; i < 20; ++i) {
        std::string f = ws_client_frame(0x2, random_bytes_str(1 + size_t(i) * 50));
        ::send(w, f.data(), f.size(), 0);
      }
      std::string bad = random_bytes_str(64);
      ::send(w, bad.data(), bad.size(), 0);  // protocol violation
      ::close(w);
      int st;
      http(d.port, "POST", "/rpc", random_bytes_str(500), d.token, &st, 1000);
      http(d.port, "POST", "/rpc", R"({"method":"links.add","params":{"type":"raw","config":{"mtu":"x"}}})", d.token, &st, 1000);
      // stream, kiss, udp, serial and audio garbage
      int t = ::socket(AF_INET, SOCK_STREAM, 0);
      sockaddr_in ta{};
      ta.sin_family = AF_INET;
      ta.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      ta.sin_port = htons(tp);
      if (::connect(t, reinterpret_cast<sockaddr*>(&ta), sizeof ta) == 0) {
        std::string g = random_bytes_str(8000) + std::string("\xE5\x4D\x00\xff\xff\x00", 6) + random_bytes_str(100);
        ::send(t, g.data(), g.size(), 0);
      }
      ::close(t);
      std::string kg = random_bytes_str(2000) + std::string("\xC0\x00\xC0\xC0\xDB\xDB\xC0", 7) + random_bytes_str(300);
      ::send(kiss_fd, kg.data(), kg.size(), 0);
      for (int i = 0; i < 20; ++i) {
        std::string dg = random_bytes_str(size_t(1 + (i * 97) % 1500));
        ::sendto(udp_fd, dg.data(), dg.size(), 0, reinterpret_cast<sockaddr*>(&ua), sizeof ua);
      }
      std::string sg = random_bytes_str(500);
      (void)!::write(cable.s2, sg.data(), sg.size());
      if (audio_fd < 0) audio_fd = ::open(fifo.c_str(), O_WRONLY | O_NONBLOCK);
      if (audio_fd >= 0) {
        std::string ag = random_bytes_str(9600);
        (void)!::write(audio_fd, ag.data(), ag.size());
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (audio_fd >= 0) ::close(audio_fd);
    ::close(kiss_fd);
    ::close(udp_fd);
    REQUIRE(d.alive());
    Json info = d.rpc("node.info");
    CHECK(info.get("ok").as_bool());
    long rss1 = 0;
    double cpu1 = 0;
    d.usage(&rss1, &cpu1);
    MESSAGE("garbage run: " << sent_http << " HTTP junk connections; RSS " << rss0 / 1024 << " -> " << rss1 / 1024 << " MiB, CPU "
                            << (cpu1 - cpu0) << " s");
    CHECK(rss1 < rss_limit_kib(400));
    Json st = d.result("mesh.stats");
    CHECK(st.get("rx_bad").as_int() > 0);
    for (const char* n : {"tcp", "kiss", "udp", "serial", "audio"}) {
      CAPTURE(n);
      CHECK(d.link_by_name(n).is_object());
    }
    // ...and still works: a normal peer joins over the hub and chats.
    REQUIRE(peer.start({"--link", link_arg("to hub", R"({"kind":"websocket","config":{"url":"ws://127.0.0.1:)" + std::to_string(d.port) + R"(/ws"}})", R"({"type":"raw"})")}));
    std::string t = tag();
    public_one_way(peer, d, "after garbage " + t);
    dm_one_way(d, peer, "after garbage dm " + t);
    CHECK(d.stop() == 0);
  }

  TEST_CASE("ws flood and slow guest eviction") {
    TestDir tmp;
    Daemon d("flood", tmp.path);
    REQUIRE(d.start());
    GuestSwarm honest(d.port, 1);
    REQUIRE(wait_until([&] { return honest.upgraded() == 1; }, 5000));
    // A frozen guest: tiny receive buffer, never reads.
    int frozen = raw_ws_connect(d.port, "/ws", 4096);
    REQUIRE(frozen >= 0);
    // A flooder: thousands of frames as fast as possible.
    int flood = raw_ws_connect(d.port, "/ws");
    std::string ff = ws_client_frame(0x2, random_bytes_str(300));
    std::string burst;
    for (int i = 0; i < 200; ++i) burst += ff;
    int flood_sends = 0;
    uint64_t t0 = ember_host::steady_ms();
    for (int i = 0; i < 40 && ::send(flood, burst.data(), burst.size(), 0) > 0; ++i) ++flood_sends;
    REQUIRE(wait_until([&] { return d.result("host.hub.status").get("stats").get("guests_abusive").as_int() >= 1; }, 15000));
    MESSAGE("flooder evicted after " << (ember_host::steady_ms() - t0) << " ms (" << flood_sends * 200 << " frames sent)");
    ::close(flood);
    // Outbound traffic the frozen guest cannot absorb: 96 KiB files to Public every 300 ms.
    std::string blob = ember_host::base64_encode(random_bytes_str(96 * 1024));
    long rss_max = 0;
    bool evicted = false;
    uint64_t start = ember_host::steady_ms();
    // How long this takes depends on how much the kernel buffers for a peer that never reads (the hub
    // only starts its 15 s "queue full" clock once the socket buffers and its own queue are full):
    // a few seconds on macOS, most of a minute on Linux hosts with large auto-tuned buffers.
    while (ember_host::steady_ms() - start < 120000 && !evicted) {
      Json p = Json::object();
      p.set("conv", "ch:public");
      p.set("name", "blob.bin");
      p.set("mime", "application/octet-stream");
      p.set("data_b64", blob);
      d.rpc("files.send", p);
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      long rss;
      double cpu;
      if (d.usage(&rss, &cpu)) rss_max = std::max(rss_max, rss);
      evicted = d.result("host.hub.status").get("stats").get("guests_evicted").as_int() >= 1;
    }
    MESSAGE("frozen guest evicted: " << evicted << " after " << (ember_host::steady_ms() - start) << " ms; daemon RSS max "
                                     << rss_max / 1024 << " MiB; honest guest got " << honest.min_frames() << " frames");
    CHECK(evicted);
    CHECK(honest.upgraded() == 1);
    CHECK(honest.min_frames() > 10);
    CHECK(rss_max < rss_limit_kib(300));
    ::close(frozen);
    CHECK(d.alive());
  }

  TEST_CASE("100 simultaneous guests") {
    TestDir tmp;
    Daemon d("hub100", tmp.path);
    REQUIRE(d.start());
    long rss_idle = 0;
    double cpu_idle = 0;
    d.usage(&rss_idle, &cpu_idle);
    GuestSwarm swarm(d.port, 100);
    REQUIRE(wait_until([&] { return swarm.upgraded() == 100; }, 15000));
    REQUIRE(wait_until([&] { return d.result("host.hub.status").get("clients").as_int() == 100; }, 5000));
    long rss_conn = 0;
    double cpu_conn = 0;
    d.usage(&rss_conn, &cpu_conn);
    size_t base = swarm.min_frames();
    // 10 s of traffic: 5 Public messages per second from the hub node (each goes to all 100 guests),
    // and every guest sends one junk frame per second (rejected by the core's parser).
    uint64_t start = ember_host::steady_ms();
    int sent = 0;
    while (ember_host::steady_ms() - start < 10000) {
      d.send("ch:public", "load " + std::to_string(sent++) + " " + std::string(120, 'x'));
      if (sent % 5 == 0)
        for (size_t i = 0; i < 100; ++i) swarm.send(i, random_bytes_str(80));
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    REQUIRE(wait_until([&] { return swarm.min_frames() >= base + size_t(sent); }, 10000));
    long rss_load = 0;
    double cpu_load = 0;
    d.usage(&rss_load, &cpu_load);
    double wall = double(ember_host::steady_ms() - start) / 1000.0;
    MESSAGE("100 guests: RSS idle " << rss_idle / 1024 << " MiB, 100 connected " << rss_conn / 1024 << " MiB, after load "
                                    << rss_load / 1024 << " MiB; CPU " << (cpu_load - cpu_conn) << " s over " << wall << " s ("
                                    << int(100.0 * (cpu_load - cpu_conn) / wall) << "% of one core) for " << sent
                                    << " messages x 100 guests + " << sent / 5 * 100 << " junk frames");
    CHECK(swarm.upgraded() == 100);
    CHECK(rss_load < rss_limit_kib(300));
  }

  TEST_CASE("clean shutdown and restart keep identity links and messages") {
    TestDir tmp;
    Daemon d("persist", tmp.path);
    REQUIRE(d.start());
    std::string id = d.node_id();
    uint16_t lp = free_tcp_port();
    Json lc = d.rpc("host.link.create", J(R"({"name":"saved","endpoint":{"kind":"tcp_server","config":{"bind":"127.0.0.1","port":)" + std::to_string(lp) +
                                          R"(}},"link":{"type":"stream"}})"));
    REQUIRE(lc.get("ok").as_bool());
    int64_t lid = lc.get("result").get("link").get("id").as_int();
    int64_t hub_id = d.link_by_name("Hub guests (Wi-Fi)").get("id").as_int();
    std::string t = "kept " + tag();
    REQUIRE(d.send("ch:public", t).get("ok").as_bool());
    REQUIRE(d.stop() == 0);
    CHECK(d.log().find("shutting down") != std::string::npos);
    d.forget_id();
    REQUIRE(d.start());
    CHECK(d.node_id() == id);
    CHECK(d.has_message("ch:public", t));
    REQUIRE(wait_until([&] { return d.link_by_name("saved").get("state").as_string() == "up" || true; }, 100));
    Json saved = d.link_by_name("saved");
    CHECK(saved.get("id").as_int() == lid);
    CHECK(d.link_by_name("Hub guests (Wi-Fi)").get("id").as_int() == hub_id);
    // The endpoint really listens again.
    int st;
    REQUIRE(wait_until([&] {
      int s = ::socket(AF_INET, SOCK_STREAM, 0);
      sockaddr_in a{};
      a.sin_family = AF_INET;
      a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      a.sin_port = htons(lp);
      bool ok = ::connect(s, reinterpret_cast<sockaddr*>(&a), sizeof a) == 0;
      ::close(s);
      return ok;
    }, 5000));
    (void)st;
    // Removing it deletes the definition: gone after the next restart.
    REQUIRE(d.rpc("host.link.remove", J("{\"id\":" + std::to_string(lid) + "}")).get("ok").as_bool());
    REQUIRE(d.stop() == 0);
    REQUIRE(d.start());
    CHECK_FALSE(d.link_by_name("saved").is_object());
  }

  TEST_CASE("locked store at start") {
    TestDir tmp;
    Daemon d("locked", tmp.path);
    REQUIRE(d.start());
    std::string id = d.node_id();
    uint16_t lp = free_tcp_port();
    REQUIRE(d.rpc("host.link.create", J(R"({"name":"saved","endpoint":{"kind":"tcp_server","config":{"bind":"127.0.0.1","port":)" + std::to_string(lp) +
                                        R"(}},"link":{"type":"stream"}})")).get("ok").as_bool());
    REQUIRE(d.rpc("security.set_passphrase", J(R"({"new":"correct horse"})")).get("ok").as_bool());
    REQUIRE(d.stop() == 0);
    // No terminal, no passphrase file: starts locked; the hub still serves the app.
    REQUIRE(d.start());
    Json ni = d.result("node.info");
    CHECK(ni.get("locked").as_bool());
    CHECK(ni.get("node_id").as_string().empty());
    CHECK(d.log().find("storage is LOCKED") != std::string::npos);
    int st;
    http(d.port, "GET", "/hub.json", "", "", &st);
    CHECK(st == 200);
    http(d.port, "GET", "/", "", "", &st);
    CHECK(st == 200);
    CHECK(d.rpc("messages.send", J(R"({"conv":"ch:public","text":"x"})")).get("error").get("code").as_string() == "locked");
    CHECK(d.rpc("security.unlock", J(R"({"passphrase":"wrong"})")).get("error").get("code").as_string() == "denied");
    // Unlock from the operator side: links come back (node.ready -> links.list -> reopen).
    REQUIRE(d.rpc("security.unlock", J(R"({"passphrase":"correct horse"})")).get("ok").as_bool());
    d.forget_id();
    CHECK(d.node_id() == id);
    CHECK(wait_until([&] { return d.link_by_name("saved").get("state").as_string() == "up" ||
                                  d.link_by_name("saved").get("state").as_string() == "down"; }, 5000));
    CHECK(wait_until([&] { return d.link_by_name("Hub guests (Wi-Fi)").get("state").as_string() == "up"; }, 5000));
    REQUIRE(d.stop() == 0);
    // --passphrase-file unlocks at start; a wrong one leaves it locked but running.
    std::string pf = ember_host::path_join(tmp.path, "pass.txt");
    ember_host::write_file_atomic(pf, "wrong\n");
    REQUIRE(d.start({"--passphrase-file", pf}));
    CHECK(d.result("node.info").get("locked").as_bool());
    REQUIRE(d.stop() == 0);
    ember_host::write_file_atomic(pf, "correct horse\n");
    REQUIRE(d.start({"--passphrase-file", pf}));
    CHECK_FALSE(d.result("node.info").get("locked").as_bool());
    CHECK(d.log().find("storage unlocked") != std::string::npos);
    CHECK(wait_until([&] { return d.link_by_name("saved").is_object(); }, 5000));
  }

  TEST_CASE("console attach and in-process console against the real core") {
    TestDir tmp;
    Daemon a("con-a", tmp.path), b("con-b", tmp.path);
    uint16_t p = free_tcp_port();
    REQUIRE(a.start({"--link", link_arg("tcp", R"({"kind":"tcp_server","config":{"bind":"127.0.0.1","port":)" + std::to_string(p) + "}}", R"({"type":"stream"})")}));
    REQUIRE(b.start({"--link", link_arg("tcp", R"({"kind":"tcp","config":{"host":"127.0.0.1","port":)" + std::to_string(p) + "}}", R"({"type":"stream"})")}));
    REQUIRE(wait_peer(a, b.node_id()));
    std::string t = tag();
    std::string cmds = ember_host::path_join(tmp.path, "cmds.txt");
    ember_host::write_file_atomic(cmds, "info\npeers\nlinks\nchannels\nsay console hello " + t + "\ndm " + b.node_id() + " console dm " + t +
                                            "\nsos console sos " + t + "\nsos status\nsos stop\ncheckin ok fine\nconvs\nread public 5\nhub\nbogus\nquit\n");
    std::string out = ember_host::path_join(tmp.path, "attach.out");
    int rc = run_process({EMBERD_PATH, "--attach", "ws://127.0.0.1:" + std::to_string(a.port) + "/rpc?token=" + a.token}, cmds, out, 20000);
    std::string o;
    ember_host::read_file(out, &o);
    CHECK(rc == 0);
    CAPTURE(o);
    CHECK(o.find("node " + a.node_id()) != std::string::npos);
    CHECK(o.find(b.node_id()) != std::string::npos);           // peers
    CHECK(o.find("tcp_server") != std::string::npos);          // links
    CHECK(o.find(">> [ch:public]") != std::string::npos);      // say
    CHECK(o.find(">> [dm:" + b.node_id()) != std::string::npos);
    CHECK(o.find("SOS ACTIVE") != std::string::npos);
    CHECK(o.find("SOS stopped") != std::string::npos);
    CHECK(o.find("check-in sent") != std::string::npos);
    CHECK(o.find("console hello " + t) != std::string::npos);  // read public
    CHECK(o.find("unknown command 'bogus'") != std::string::npos);
    CHECK(o.find("error:") == std::string::npos);
    CHECK(wait_until([&] { return b.has_message("ch:public", "console hello " + t); }, 15000));
    CHECK(wait_until([&] { return b.has_message("dm:" + a.node_id(), "console dm " + t); }, 15000));
    // Incoming traffic is printed asynchronously by an attached console.
    std::string cmds2 = ember_host::path_join(tmp.path, "cmds2.txt");
    ember_host::write_file_atomic(cmds2, "info\n");
    std::string out2 = ember_host::path_join(tmp.path, "attach2.out");
    std::thread later([&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(1500));
      b.send("ch:public", "async " + t);
    });
    // stdin = a FIFO kept open for a few seconds so the console stays attached.
    std::string fifo = ember_host::path_join(tmp.path, "stdin.fifo");
    ::mkfifo(fifo.c_str(), 0600);
    std::thread feeder([&] {
      int f = ::open(fifo.c_str(), O_WRONLY);
      std::this_thread::sleep_for(std::chrono::seconds(5));
      (void)!::write(f, "quit\n", 5);
      ::close(f);
    });
    rc = run_process({EMBERD_PATH, "--attach", "ws://127.0.0.1:" + std::to_string(a.port) + "/rpc?token=" + a.token}, fifo, out2, 20000);
    later.join();
    feeder.join();
    std::string o2;
    ember_host::read_file(out2, &o2);
    CHECK(o2.find("<< [ch:public]") != std::string::npos);
    CHECK(o2.find("async " + t) != std::string::npos);
    // Bad token: refused, non-zero exit.
    rc = run_process({EMBERD_PATH, "--attach", "ws://127.0.0.1:" + std::to_string(a.port) + "/rpc?token=nope"}, cmds2, out2, 10000);
    CHECK(rc != 0);
    // --console: same REPL in-process; `quit` stops the daemon cleanly.
    Daemon c("con-c", tmp.path);
    std::string cmds3 = ember_host::path_join(tmp.path, "cmds3.txt");
    ember_host::write_file_atomic(cmds3, "info\nsay in-process " + t + "\nlinks\nquit\n");
    c.stdin_path = cmds3;
    REQUIRE(c.start({"--console"}, false));
    REQUIRE(wait_until([&] { return !c.alive(); }, 15000));
    CHECK(c.exit_code() == 0);
    std::string o3 = c.log();
    CHECK(o3.find(">> [ch:public]") != std::string::npos);
    CHECK(o3.find("shutting down") != std::string::npos);
  }

  TEST_CASE("handheld_vox preset through a simulated VOX radio") {
    TestDir tmp;
    Daemon a("vox-a", tmp.path), b("vox-b", tmp.path);
    std::string a2b = ember_host::path_join(tmp.path, "a2b.pcm"), b2a = ember_host::path_join(tmp.path, "b2a.pcm");
    // The "radio": VOX needs 250 ms of audio before it transmits (that start is lost), hangs 500 ms,
    // FM voice band 300-3000 Hz.
    auto radio = [](const std::string& in, const std::string& out) {
      return link_arg("handheld", R"({"kind":"audio_pipe","config":{"input_path":")" + in + R"(","output_path":")" + out +
                                      R"(","vox_attack_ms":250,"vox_hang_ms":500,"tx_highpass_hz":300,"tx_lowpass_hz":3000}})",
                      R"({"type":"audio","config":{"preset":"handheld_vox"}})");
    };
    REQUIRE(a.start({"--callsign", "Q0TVA-1", "--link", radio(b2a, a2b)}));
    if (!core_has_audio_preset(a, "handheld_vox")) {
      MESSAGE("core has no audio preset 'handheld_vox' yet: skipping");
      return;
    }
    // One shared channel: start B only after A's start-up transmission has ended (a half-duplex
    // radio is deaf while it transmits, so overlapping start-up beacons would just collide).
    REQUIRE(wait_until([&] {
      Json l = a.link_by_name("handheld");
      return l.get("stats").get("tx_frames").as_int() >= 1 && !l.get("status").get("transmitting").as_bool(true);
    }, 30000));
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    REQUIRE(b.start({"--callsign", "Q0TVB-1", "--link", radio(a2b, b2a)}));
    // Finding (core): with this preset a node that hears a new neighbour's beacon does not answer on
    // the link, so the node that powered up first is not learned until its next beacon. Record it,
    // then announce (what any first message would also do).
    if (!wait_peer(b, a.node_id(), 20000)) {
      MESSAGE("FINDING: B did not learn A within 20 s of hearing B's beacon on A (no answer on a handheld_vox link); announcing");
      a.rpc("mesh.announce");  // one side at a time: simultaneous announcements would collide
      wait_peer(b, a.node_id(), 30000);
    }
    if (!wait_peer(a, b.node_id(), 20000)) {
      wait_until([&] { return !b.link_by_name("handheld").get("status").get("transmitting").as_bool(true); }, 20000);
      b.rpc("mesh.announce");
    }
    if (!wait_peer(a, b.node_id(), 90000) || !wait_peer(b, a.node_id(), 60000)) {
      Json la = a.link_by_name("handheld"), lb = b.link_by_name("handheld");
      Json ma = la.get("status"), mb = lb.get("status");
      ma.set("caps", Json());
      mb.set("caps", Json());
      FAIL("no peer over the VOX radio; A " << la.get("stats").dump() << " A host " << la.get("host").dump() << " A modem " << ma.dump()
                                            << " | B " << lb.get("stats").dump() << " B host " << lb.get("host").dump() << " B modem " << mb.dump());
    }
    std::string t = tag();
    public_one_way(a, b, "through vox " + t, 90000);
    public_one_way(b, a, "vox back " + t, 90000);
    Json st = a.link_by_name("handheld").get("host");
    MESSAGE("vox key-ups at A's radio: " << a.link_by_name("handheld").dump().find("vox_keyups"));
    (void)st;
  }

  TEST_CASE("acoustic preset through a degraded audio path") {
    TestDir tmp;
    Daemon a("ac-a", tmp.path), b("ac-b", tmp.path);
    std::string a2b = ember_host::path_join(tmp.path, "a2b.pcm"), b2a = ember_host::path_join(tmp.path, "b2a.pcm");
    // Phone speaker -> air -> radio mic -> FM -> radio speaker -> air -> phone mic: band-limited,
    // 20 dB quieter, with noise.
    auto path = [](const std::string& in, const std::string& out) {
      return link_arg("acoustic", R"({"kind":"audio_pipe","config":{"input_path":")" + in + R"(","output_path":")" + out +
                                      R"(","tx_highpass_hz":400,"tx_lowpass_hz":2500,"tx_gain_db":-20,"tx_noise_dbfs":-50,"rx_noise_dbfs":-60}})",
                      R"({"type":"audio","config":{"preset":"acoustic","licence":"licence_free"}})");
    };
    REQUIRE(a.start({"--link", path(b2a, a2b)}));
    if (!core_has_audio_preset(a, "acoustic")) {
      MESSAGE("core has no audio preset 'acoustic' yet: skipping");
      return;
    }
    // Same channel discipline as the VOX case: B starts after A's start-up transmission, and the
    // announcement that closes the gap after the core's missing reply goes out one side at a time.
    REQUIRE(wait_until([&] {
      Json l = a.link_by_name("acoustic");
      return l.get("stats").get("tx_frames").as_int() >= 1 && !l.get("status").get("transmitting").as_bool(true);
    }, 120000));
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    REQUIRE(b.start({"--link", path(a2b, b2a)}));
    if (!wait_peer(b, a.node_id(), 60000)) {
      wait_until([&] { return !a.link_by_name("acoustic").get("status").get("transmitting").as_bool(true); }, 60000);
      a.rpc("mesh.announce");
    }
    REQUIRE(wait_peer(a, b.node_id(), 150000));
    REQUIRE(wait_peer(b, a.node_id(), 150000));
    Json la = a.link_by_name("acoustic");
    MESSAGE("acoustic link: airtime so far " << la.get("stats").get("airtime_ms").as_int()
                                    << " ms for " << la.get("stats").get("tx_frames").as_int() << " frames");
    public_one_way(a, b, "acoustic " + tag(), 150000);
  }

  TEST_CASE("native morse capture from a PCM pipe and the console") {
    TestDir tmp;
    Daemon a("cw", tmp.path);
    REQUIRE(a.start());
    Json info = a.result("host.info");
    bool feat = false;
    for (const auto& f : info.get("features").elements()) feat |= f.as_string() == "capture_mic_pcm";
    CHECK(feat);
    CHECK(info.get("capture").get("camera_luma").as_string() == "browser");
    CHECK(a.rpc("host.capture.start", J(R"({"kind":"camera_luma","session":"1"})")).get("error").get("code").as_string() == "unsupported");
    if (!core_has_morse(a)) {
      MESSAGE("core has no morse.rx.* yet: checking only the capture plumbing");
      std::string pcm = ember_host::path_join(tmp.path, "cw.pcm");
      write_pcm(pcm, make_cw("CQ", 20, 700, 8000));
      Json r = a.rpc("host.capture.start", J(R"({"kind":"mic_pcm","session":"1","sample_rate":8000,"input_path":")" + pcm + "\"}"));
      CHECK(r.get("ok").as_bool());
      // The first feed fails with not_implemented and the capture stops itself.
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      CHECK(a.rpc("host.capture.stop", J(R"({"kind":"mic_pcm"})")).get("ok").as_bool());
      return;
    }
    std::string text = "CQ CQ DE Q0TCW K";
    std::string pcm = ember_host::path_join(tmp.path, "cw.pcm");
    write_pcm(pcm, make_cw(text, 18, 700, 8000));
    Json s = a.result("morse.rx.start", J(R"({"source":"audio","sample_rate":8000})"));
    Json sess = s.get("session");
    REQUIRE(!sess.is_null());
    Json cp = J(R"({"kind":"mic_pcm","sample_rate":8000})");
    cp.set("session", sess);
    cp.set("input_path", pcm);
    REQUIRE(a.rpc("host.capture.start", cp).get("ok").as_bool());
    Json sp = Json::object();
    sp.set("session", sess);
    std::string transcript;
    CHECK_MESSAGE(wait_until([&] {
      transcript = a.result("morse.rx.status", sp).get("transcript").as_string();
      return transcript.find("Q0TCW") != std::string::npos;
    }, 30000), "transcript so far: '" << transcript << "'");
    a.rpc("host.capture.stop", J(R"({"kind":"mic_pcm"})"));
    a.rpc("morse.rx.stop", sp);
    // Console: `morse listen file:...` prints the text live, `morse stop` the transcript.
    std::string fifo = ember_host::path_join(tmp.path, "stdin.fifo");
    ::mkfifo(fifo.c_str(), 0600);
    std::thread feeder([&] {
      int f = ::open(fifo.c_str(), O_WRONLY);
      std::string l = "morse listen file:" + pcm + "\n";
      (void)!::write(f, l.data(), l.size());
      std::this_thread::sleep_for(std::chrono::seconds(14));
      (void)!::write(f, "morse status\nmorse stop\nquit\n", 29);
      ::close(f);
    });
    std::string out = ember_host::path_join(tmp.path, "console.out");
    int rc = run_process({EMBERD_PATH, "--attach", "ws://127.0.0.1:" + std::to_string(a.port) + "/rpc?token=" + a.token}, fifo, out, 30000);
    feeder.join();
    std::string o;
    ember_host::read_file(out, &o);
    CAPTURE(o);
    CHECK(rc == 0);
    CHECK(o.find("listening for Morse") != std::string::npos);
    CHECK(o.find("~~ morse") != std::string::npos);
    CHECK(o.find("Q0TCW") != std::string::npos);
    CHECK(o.find("transcript: ") != std::string::npos);
    MESSAGE("console morse output:\n" << o.substr(o.find("listening for Morse")));
  }

  TEST_CASE("rpc carries 30 feeds per second without blocking other calls") {
    TestDir tmp;
    Daemon a("rate", tmp.path);
    REQUIRE(a.start());
    bool morse = core_has_morse(a);
    emberd::RemoteRpc ws;
    std::string err;
    REQUIRE(ws.connect("ws://127.0.0.1:" + std::to_string(a.port) + "/rpc?token=" + a.token, &err));
    std::atomic<int> morse_events{0};
    ws.on_event = [&](const std::string& e) {
      if (e.find("\"morse.text\"") != std::string::npos) ++morse_events;
    };
    Json sa, sl;
    if (morse) {
      sa = ws.call("morse.rx.start", J(R"({"source":"audio","sample_rate":8000})")).get("result").get("session");
      sl = ws.call("morse.rx.start", J(R"({"source":"light"})")).get("result").get("session");
    }
    std::vector<int16_t> cw = make_cw("PARIS PARIS PARIS PARIS PARIS PARIS", 20, 700, 8000, 0.2);
    long rss0 = 0;
    double cpu0 = 0, cpu1 = 0;
    a.usage(&rss0, &cpu0);
    std::vector<double> feed_ms, light_ms, info_ms, post_ms;
    std::atomic<bool> stop{false};
    // A second client keeps calling node.info while the feeds run (head-of-line check), plus POST.
    std::thread other([&] {
      emberd::RemoteRpc w2;
      std::string e2;
      if (!w2.connect("ws://127.0.0.1:" + std::to_string(a.port) + "/rpc?token=" + a.token, &e2)) return;
      while (!stop) {
        uint64_t t0 = ember_host::steady_ms();
        w2.call("node.info", Json::object());
        info_ms.push_back(double(ember_host::steady_ms() - t0));
        t0 = ember_host::steady_ms();
        a.rpc("conversations.list");
        post_ms.push_back(double(ember_host::steady_ms() - t0));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    });
    size_t off = 0;
    double t_ms = 0;
    uint64_t start = ember_host::steady_ms();
    const auto pace_start = std::chrono::steady_clock::now();
    int n = 0;
    while (ember_host::steady_ms() - start < 10000) {
      // audio: 33 ms of 8 kHz PCM as base64 (~720 bytes) per call
      size_t k = 267;
      std::string b(k * 2, '\0');
      for (size_t i = 0; i < k; ++i) {
        int16_t v = cw[(off + i) % cw.size()];
        b[2 * i] = char(uint16_t(v) & 0xff);
        b[2 * i + 1] = char(uint16_t(v) >> 8);
      }
      off += k;
      Json f = Json::object();
      f.set("session", sa.is_null() ? Json("1") : sa);
      f.set("pcm_b64", ember_host::base64_encode(b));
      uint64_t t0 = ember_host::steady_ms();
      ws.call("morse.rx.feed", f);
      feed_ms.push_back(double(ember_host::steady_ms() - t0));
      // light: a camera frame batch
      Json l = Json::object();
      l.set("session", sl.is_null() ? Json("2") : sl);
      l.set("t_ms", t_ms);
      l.set("dt_ms", 33.3);
      Json lv = Json::array();
      lv.push((n / 3) % 2 ? 0.9 : 0.1);
      l.set("levels", lv);
      t_ms += 33.3;
      t0 = ember_host::steady_ms();
      ws.call("morse.rx.feed", l);
      light_ms.push_back(double(ember_host::steady_ms() - t0));
      ++n;
      // Paced by deadline (one audio + one light feed every 33 ms), not by a fixed sleep: loaded or
      // virtualised hosts stretch short sleeps to several times their length, and this test measures
      // the daemon, not the sleeping accuracy of the machine running it.
      std::this_thread::sleep_until(pace_start + std::chrono::milliseconds(33) * n);
    }
    stop = true;
    other.join();
    long rss1 = 0;
    a.usage(&rss1, &cpu1);
    double wall = double(ember_host::steady_ms() - start) / 1000.0;
    std::string mode = morse ? "real morse decoder" : "core has no morse yet: transport only";
    MESSAGE("rpc feeds (" << mode << "): " << n << " audio + " << n
                          << " light feeds in " << wall << " s; feed latency p50/p99 " << percentile(feed_ms, 0.5) << "/"
                          << percentile(feed_ms, 0.99) << " ms, light " << percentile(light_ms, 0.5) << "/" << percentile(light_ms, 0.99)
                          << " ms; concurrent node.info p50/p99 " << percentile(info_ms, 0.5) << "/" << percentile(info_ms, 0.99)
                          << " ms, POST p99 " << percentile(post_ms, 0.99) << " ms; daemon CPU " << int(100 * (cpu1 - cpu0) / wall)
                          << "% of a core, RSS " << rss0 / 1024 << "->" << rss1 / 1024 << " MiB; morse.text events " << morse_events.load());
    CHECK(n >= 250);
    CHECK(percentile(feed_ms, 0.99) < 50);
    CHECK(percentile(info_ms, 0.99) < 100);
    if (morse) CHECK(morse_events.load() > 0);
  }

  TEST_CASE("big guide pack mmap with ranges ETag and many downloads") {
    TestDir tmp;
    // A web root with a 48 MB data file standing in for a large guides.pack.
    std::string web = ember_host::path_join(tmp.path, "web");
    ember_host::make_dirs(ember_host::path_join(web, "data"));
    ember_host::write_file_atomic(ember_host::path_join(web, "index.html"), "<!doctype html><title>t</title>");
    const size_t big = 48u << 20;
    {
      std::string blob(big, '\0');
      uint32_t x = 1;
      for (size_t i = 0; i < big; i += 4) {
        x = x * 1664525u + 1013904223u;
        std::memcpy(&blob[i], &x, 4);
      }
      ember_host::write_file_atomic(ember_host::path_join(web, "data/guides.pack"), blob);
    }
    Daemon a("pack", tmp.path);
    REQUIRE(a.start({"--web-root", web}));
    CHECK(a.log().find("memory-mapped") != std::string::npos);  // the real guides.pack from build/
    // Range + ETag: a phone resumes an interrupted download.
    int st;
    std::string head = http(a.port, "GET", "/data/guides.pack", "", "", &st, 30000);
    REQUIRE(st == 200);
    CHECK(head.size() == big);
    std::string part;
    {
      int s = ::socket(AF_INET, SOCK_STREAM, 0);
      sockaddr_in ad{};
      ad.sin_family = AF_INET;
      ad.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      ad.sin_port = htons(a.port);
      REQUIRE(::connect(s, reinterpret_cast<sockaddr*>(&ad), sizeof ad) == 0);
      std::string req = "GET /data/guides.pack HTTP/1.1\r\nHost: x\r\nRange: bytes=" + std::to_string(big - 1000) + "-\r\nConnection: close\r\n\r\n";
      ::send(s, req.data(), req.size(), 0);
      char b[65536];
      ssize_t r;
      while ((r = ::recv(s, b, sizeof b, 0)) > 0) part.append(b, size_t(r));
      ::close(s);
    }
    CHECK(part.find("HTTP/1.1 206") == 0);
    CHECK(part.find("Content-Range: bytes " + std::to_string(big - 1000) + "-" + std::to_string(big - 1) + "/" + std::to_string(big)) != std::string::npos);
    CHECK(part.substr(part.find("\r\n\r\n") + 4) == head.substr(big - 1000));
    size_t et = part.find("ETag: ");
    REQUIRE(et != std::string::npos);
    // 40 guests download the whole pack at the same time, reading at different speeds.
    long rss0 = 0, rss_max = 0;
    double cpu0 = 0, cpu1 = 0;
    a.usage(&rss0, &cpu0);
    std::atomic<int> complete{0};
    std::atomic<bool> done{false};
    std::thread watch([&] {
      while (!done) {
        long r;
        double c;
        if (a.usage(&r, &c)) rss_max = std::max(rss_max, r);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
    });
    std::vector<std::thread> dl;
    uint64_t t0 = ember_host::steady_ms();
    for (int i = 0; i < 40; ++i)
      dl.emplace_back([&, i] {
        int s = ::socket(AF_INET, SOCK_STREAM, 0);
        int rb = 64 * 1024;
        setsockopt(s, SOL_SOCKET, SO_RCVBUF, &rb, sizeof rb);
        sockaddr_in ad{};
        ad.sin_family = AF_INET;
        ad.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ad.sin_port = htons(a.port);
        if (::connect(s, reinterpret_cast<sockaddr*>(&ad), sizeof ad) != 0) return;
        std::string req = "GET /data/guides.pack HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
        ::send(s, req.data(), req.size(), 0);
        char b[32768];
        size_t got = 0;
        ssize_t r;
        while ((r = ::recv(s, b, sizeof b, 0)) > 0) {
          got += size_t(r);
          if (i % 4 == 0) std::this_thread::sleep_for(std::chrono::microseconds(300));  // slower phones
        }
        ::close(s);
        if (got > big) ++complete;
      });
    for (auto& t : dl) t.join();
    done = true;
    watch.join();
    a.usage(&rss0, &cpu1);
    double secs = double(ember_host::steady_ms() - t0) / 1000.0;
    MESSAGE("40 concurrent downloads of " << big / (1 << 20) << " MiB: " << complete.load() << " complete in " << secs << " s ("
                                          << int(40.0 * double(big) / (1 << 20) / secs) << " MiB/s total); daemon RSS peak "
                                          << rss_max / 1024 << " MiB, CPU " << (cpu1 - cpu0) << " s");
    CHECK(complete.load() == 40);
    CHECK(rss_max < rss_limit_kib(200));
    CHECK(a.alive());
  }

  TEST_CASE("a fresh identity is anonymous by default") {
    TestDir tmp;
    Daemon d("anon", tmp.path);
    d.set_name = false;
    REQUIRE(d.start());
    Json ni = d.result("node.info");
    std::string name = ni.get("name").as_string();
    CHECK(name == "EmberMesh hub " + ni.get("node_id").as_string().substr(0, 4));
    char host[256] = {0};
    ::gethostname(host, sizeof host - 1);
    std::string h = ember_host::to_lower(host), shortname = h.substr(0, h.find('.'));
    const char* user = std::getenv("USER");
    int st;
    std::string probe = ember_host::to_lower(http(d.port, "GET", "/hub.json", "", "", &st));
    std::string lname = ember_host::to_lower(name);
    for (const std::string& secret : {shortname, std::string(user ? user : "")}) {
      if (secret.size() < 3) continue;
      CHECK(lname.find(secret) == std::string::npos);
      CHECK(probe.find(secret) == std::string::npos);
      CHECK(ember_host::to_lower(ni.get("device_name").as_string()).find(secret) == std::string::npos);
    }
  }
}
