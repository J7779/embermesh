// Seeded mutation fuzzing of every link driver through create_link(): mutated and random endpoint
// bytes (seeded from valid frames of each protocol), hostile audio at absurd and changing sample
// rates, interleaved with ticks (including large clock jumps), endpoint state changes, sends,
// interop sends and reconfigures. After the hostile phase every driver must come back: a clean
// device emulator session brings handshake drivers Up, byte drivers deliver a clean frame and
// transmit again, audio drivers decode a clean transmission and key up.
//
// Invariants checked after every operation: no sanitizer report, status_json is a JSON object,
// interop text is printable/bounded, delivered frames are 1..8192 bytes, queues and per-operation
// output stay bounded, and no single operation takes long.
//
// Fast version in the default suite; the long one: ./ember_tests -ts=link -tc="fuzz long*" --no-skip
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "doctest.h"
#include "fec/packet.h"
#include "framing/ax25.h"
#include "framing/kiss.h"
#include "framing/slip.h"
#include "framing/stream.h"
#include "link/link_util.h"
#include "link/meshtastic_proto.h"
#include "link_emulators.h"
#include "link_harness.h"
#include "modem/modem.h"

using namespace ember;
using namespace linktest;
namespace mt = ember::link::mt;

namespace {

Bytes S(const std::string& s) { return Bytes(s.begin(), s.end()); }

bool clean(const std::string& s) {
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x20 || c == 0x7F) return false;
    if (c == 0xC2 && i + 1 < s.size() && static_cast<unsigned char>(s[i + 1]) < 0xA0) return false;
  }
  return utf8_valid(s);
}

const uint8_t kInteresting[] = {0x00, 0xFF, 0xC0, 0xDB, 0xDC, 0xDD, '\r', '\n', ',', '{', '}', ':',
                                '+',  '=',  0x94, 0xC3, 0x7E, 0xE5, 0x4D, 0x80, 0x7F, '9', 'R', 'T', 'L', '0'};

void mutate(Prng& R, Bytes& b, const std::vector<Bytes>& corpus) {
  int n = 1 + static_cast<int>(R.below(4));
  for (int k = 0; k < n; ++k) {
    switch (R.below(9)) {
      case 0: if (!b.empty()) b[R.below(static_cast<uint32_t>(b.size()))] ^= static_cast<uint8_t>(1u << R.below(8)); break;
      case 1: b.insert(b.begin() + R.below(static_cast<uint32_t>(b.size() + 1)), R.byte()); break;
      case 2: if (!b.empty()) b.erase(b.begin() + R.below(static_cast<uint32_t>(b.size()))); break;
      case 3: if (!b.empty()) b[R.below(static_cast<uint32_t>(b.size()))] = kInteresting[R.below(sizeof kInteresting)]; break;
      case 4: if (!b.empty()) b.resize(R.below(static_cast<uint32_t>(b.size()))); break;
      case 5: {
        const Bytes& o = corpus[R.below(static_cast<uint32_t>(corpus.size()))];
        size_t at = R.below(static_cast<uint32_t>(b.size() + 1));
        size_t from = R.below(static_cast<uint32_t>(o.size() + 1));
        b.insert(b.begin() + static_cast<long>(at), o.begin() + static_cast<long>(from), o.end());
        break;
      }
      case 6:
        if (!b.empty() && b.size() < 20000) {
          size_t a = R.below(static_cast<uint32_t>(b.size()));
          size_t l = 1 + R.below(16);
          uint8_t v = b[a];
          b.insert(b.begin() + static_cast<long>(a), l, v);
        }
        break;
      case 7:
        if (b.size() >= 4) {  // length-field style: big or zero values
          size_t a = R.below(static_cast<uint32_t>(b.size() - 3));
          b[a] = R.byte();
          b[a + 1] = R.below(2) ? 0xFF : 0x00;
        }
        break;
      default: break;
    }
  }
  if (b.size() > 70000) b.resize(70000);
}

// ---------------- seed corpora ----------------
Bytes ax(const std::string& dst, const std::string& src, const std::string& info, std::vector<std::string> digis = {}) {
  ax25::Frame f;
  f.dst = ax25::make_address(dst);
  f.src = ax25::make_address(src);
  for (auto& d : digis) f.digis.push_back(ax25::make_address(d));
  f.control = 0x03;
  f.pid = 0xF0;
  f.info = S(info);
  f.set_command(true);
  Bytes o;
  ax25::encode(f, &o);
  return o;
}

std::vector<Bytes> kiss_seeds(Prng& R) {
  std::vector<Bytes> v;
  auto k = [&](const Bytes& a) { v.push_back(kiss::encode_data(ByteView(a), 0)); };
  k(S("hello mesh frame"));
  k(R.bytes(200));
  k(ax("EMBER", "QX2XYZ-3", std::string("\x01\x02mesh\xC0\xDB", 8)));
  k(ax("EMBER", "QX0ABC", "own echo"));
  k(ax("APRS", "N0EVIL", ":QX0ABC   :hello there{123"));
  k(ax("APRS", "N0EVIL", ":QX0ABC   :ack1"));
  k(ax("APRS", "N0EVIL", ":QX0ABC   :reply{MM}AA"));
  k(ax("APRS", "N0EVIL", ":BLN1     :bulletin \x1b[2J text"));
  k(ax("APRS", "N0EVIL", "!4903.50N/07201.75W-Test /A=001234"));
  k(ax("APRS", "N0EVIL", "@092345z4903.50N/07201.75W>"));
  k(ax("APRS", "N0EVIL", "/092345z/5L!!<*e7>7P[ comp"));
  k(ax("T4SP4Y", "N0EVIL", "`(_fn\"Oj/]\"4-}mic-e"));
  k(ax("APRS", "N0EVIL", ">status report\r\nline"));
  k(ax("APRS", "N0EVIL", ":QX0ABC   :x", {"WIDE1-1", "WIDE2-2"}));
  v.push_back(kiss::encode_param(kiss::Command::TxDelay, 30));
  v.push_back(kiss::encode_return());
  return v;
}

std::vector<Bytes> stream_seeds(Prng& R) {
  std::vector<Bytes> v;
  for (size_t n : {1, 20, 199, 1000, 2048}) {
    Bytes p = R.bytes(n);
    v.push_back(stream::encode(ByteView(p)));
    for (uint8_t roots : {2, 16, 64}) {
      fec::ProtectParams pp;
      pp.nroots = roots;
      Bytes e = {0x7E, 0x1B, 0xA5, 0xD2};
      Bytes env = fec::protect(ByteView(p), pp);
      e.insert(e.end(), env.begin(), env.end());
      v.push_back(e);
    }
  }
  return v;
}

Bytes mtf(const mt::FromRadio& f, bool ble) {
  Bytes pb = mt::encode_from_radio(f);
  return ble ? pb : mt::frame(ByteView(pb));
}

std::vector<Bytes> mt_seeds(Prng& R, bool ble) {
  std::vector<Bytes> v;
  mt::FromRadio f;
  f.kind = mt::FromKind::MyInfo; f.my_node_num = 0x11223344; v.push_back(mtf(f, ble));
  f = mt::FromRadio(); f.kind = mt::FromKind::NodeInfo; f.node.num = 0x11223344; f.node.long_name = "Name\x1b[31m\r\n"; f.node.short_name = "N"; v.push_back(mtf(f, ble));
  f = mt::FromRadio(); f.kind = mt::FromKind::Config; f.has_lora = true; f.lora.use_preset = true; f.lora.modem_preset = 4; f.lora.region = 3; v.push_back(mtf(f, ble));
  f = mt::FromRadio(); f.kind = mt::FromKind::Metadata; f.firmware_version = "2.5.6\x07"; v.push_back(mtf(f, ble));
  f = mt::FromRadio(); f.kind = mt::FromKind::QueueStatus; f.queue.free = 0; f.queue.maxlen = 16; v.push_back(mtf(f, ble));
  f = mt::FromRadio(); f.kind = mt::FromKind::QueueStatus; f.queue.free = 5; f.queue.maxlen = 16; f.queue.res = -3; v.push_back(mtf(f, ble));
  f = mt::FromRadio(); f.kind = mt::FromKind::Rebooted; v.push_back(mtf(f, ble));
  f = mt::FromRadio(); f.kind = mt::FromKind::ConfigComplete; f.config_complete_id = 12345; v.push_back(mtf(f, ble));
  f = mt::FromRadio(); f.kind = mt::FromKind::Packet; f.packet.from = 0x55; f.packet.to = mt::kBroadcast;
  f.packet.has_decoded = true; f.packet.decoded.portnum = mt::kPortText; f.packet.decoded.payload = S("hello \x07\x1b bell\r\nx");
  f.packet.rx_rssi = -80; f.packet.has_snr = true; f.packet.rx_snr = 5.5f; v.push_back(mtf(f, ble));
  f.packet.decoded.portnum = mt::kPortPrivate; f.packet.decoded.payload = R.bytes(100); v.push_back(mtf(f, ble));
  f.packet.has_decoded = false; v.push_back(mtf(f, ble));
  if (!ble) v.push_back(S("INFO | log line\r\n"));
  return v;
}

Bytes rn(uint8_t cmd, Bytes args) {
  Bytes body{cmd};
  body.insert(body.end(), args.begin(), args.end());
  return slip::encode(ByteView(body), true);
}
Bytes be32(uint32_t v) { return Bytes{uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)}; }

std::vector<Bytes> rnode_seeds(Prng& R) {
  return {rn(0x08, {0x46}), rn(0x01, be32(869525000)), rn(0x02, be32(125000)), rn(0x03, {14}), rn(0x04, {9}),
          rn(0x05, {5}), rn(0x06, {1}), rn(0x06, {0}), rn(0x00, R.bytes(60)), rn(0x00, Bytes(508, 0xC0)),
          rn(0x0F, {1}), rn(0x23, {100}), rn(0x24, {0xF0}), rn(0x25, R.bytes(10)), rn(0x27, {1, 200}),
          rn(0x29, {140}), rn(0x50, {1, 74}), rn(0x55, {0xF8}), rn(0x90, {0x02}), rn(0x90, {0x01})};
}

std::vector<Bytes> lora_seeds() {
  std::vector<std::string> l = {"+OK", "+READY", "+RESET", "+VER=RYLR998_REYAX_V1.2.2", "+ADDRESS=0",
                                "+NETWORKID=18", "+PARAMETER=9,7,1,12", "+BAND=869525000", "+ERR=4", "+ERR=17",
                                "+ERR=9999999999999999999", "+ADDRESS=99999999999999999999",
                                "+RCV=50,5,HELLO,-99,40", "+RCV=1,12,EM1:AQIDBAUG,-40,11",
                                "+RCV=7,9,a,b\r\nc,d,e,-1,2", "+RCV=65535,240," + std::string(240, 'x') + ",-200,100",
                                "+RCV=50,5,\x1b[2J\x07,-99,40", "+RCV=50, 5, HELLO, -99, 40", "\xFF\xFE+OK", "garbage line"};
  std::vector<Bytes> v;
  for (auto& s : l) v.push_back(S(s + "\r\n"));
  return v;
}

std::vector<Bytes> rtl_seeds(Prng& R) {
  Bytes h = {'R', 'T', 'L', '0', 0, 0, 0, 5, 0, 0, 0, 29};
  Bytes hi = h;
  Bytes iq = R.bytes(4000);
  hi.insert(hi.end(), iq.begin(), iq.end());
  return {h, hi, S("HTTP/1.1 400 Bad\r\n"), R.bytes(16384)};
}

// ---------------- targets ----------------
enum class Family { Raw, Stream, Kiss, Audio, Meshtastic, MeshtasticBle, RNode, LoraAt, Rtl };

struct Target {
  std::string type, cfg;
  Family fam;
};

std::vector<Target> targets() {
  std::vector<Target> t = {
      {"raw", "{}", Family::Raw},
      {"raw", R"({"preset":"websocket"})", Family::Raw},
      {"stream", "{}", Family::Stream},
      {"stream", R"({"preset":"ebyte"})", Family::Stream},
      {"stream", R"({"preset":"ble","fec":"yes","fec_roots":64})", Family::Stream},
      {"kiss", R"({"callsign":"QX0ABC","mtu":2048})", Family::Kiss},
      {"kiss", R"({"payload":"raw","licence":"licence_free"})", Family::Kiss},
      {"meshtastic", "{}", Family::Meshtastic},
      {"meshtastic", R"({"framing":"ble","handshake_timeout_s":2})", Family::MeshtasticBle},
      {"rnode", "{}", Family::RNode},
      {"lora_at", "{}", Family::LoraAt},
      {"rtl_tcp", "{}", Family::Rtl},
  };
  for (const std::string& m : modem::modem_names())
    t.push_back({"audio", R"({"modem":")" + m + R"(","callsign":"QX0ABC","persistence":255})", Family::Audio});
  return t;
}

std::unique_ptr<linkemu::Emulator> make_emu(Family f) {
  switch (f) {
    case Family::Meshtastic: return std::make_unique<linkemu::MeshtasticEmu>(false);
    case Family::MeshtasticBle: return std::make_unique<linkemu::MeshtasticEmu>(true);
    case Family::RNode: return std::make_unique<linkemu::RNodeEmu>();
    case Family::LoraAt: return std::make_unique<linkemu::AtEmu>();
    case Family::Rtl: return std::make_unique<linkemu::RtlEmu>();
    default: return nullptr;
  }
}

std::vector<Bytes> seeds_for(Family f, Prng& R) {
  switch (f) {
    case Family::Raw: return {R.bytes(1), R.bytes(100), R.bytes(1400), R.bytes(8192), R.bytes(8193)};
    case Family::Stream: return stream_seeds(R);
    case Family::Kiss: return kiss_seeds(R);
    case Family::Meshtastic: return mt_seeds(R, false);
    case Family::MeshtasticBle: return mt_seeds(R, true);
    case Family::RNode: return rnode_seeds(R);
    case Family::LoraAt: return lora_seeds();
    case Family::Rtl: return rtl_seeds(R);
    case Family::Audio: return {R.bytes(10)};
  }
  return {};
}

const uint32_t kRates[] = {0, 1, 7999, 8000, 11025, 44100, 48000, 96000, 192000, 192001, 384001, 1000000, 0xFFFFFFFFu};

struct Fuzzer {
  Target tg;
  uint64_t seed;
  Prng R;
  Clock clk;
  FakeHost h;
  std::unique_ptr<Link> l;
  std::unique_ptr<linkemu::Emulator> emu;
  std::vector<Bytes> corpus;
  bool connected = false;
  double worst_op_s = 0;
  size_t max_texts = 0, ops = 0;

  Fuzzer(const Target& t, uint64_t s) : tg(t), seed(s), R(s * 2654435761u + 17), h(&clk, s + 3) {
    std::string err;
    l = create_link(tg.type, tg.cfg, h, &err);
    REQUIRE_MESSAGE(l, err);
    emu = make_emu(tg.fam);
    if (emu) h.on_write = [this](ByteView v) { if (emu) emu->from_host(v); };
    corpus = seeds_for(tg.fam, R);
  }

  // The link is declared before the emulator, so it would otherwise be destroyed after it; some
  // drivers write a last command to the host while shutting down, which must not reach a dead
  // emulator.
  ~Fuzzer() {
    h.on_write = nullptr;
    l.reset();
  }

  void deliver_emu() {
    if (!emu || !connected) return;
    if (tg.fam == Family::MeshtasticBle) {
      auto* m = static_cast<linkemu::MeshtasticEmu*>(emu.get());
      std::vector<Bytes> d;
      d.swap(m->datagrams);
      for (const Bytes& b : d) l->on_io_bytes(ByteView(b));
      return;
    }
    Bytes b = emu->take();
    if (!b.empty()) chunked(b);
  }

  void chunked(const Bytes& b) {
    if (tg.fam == Family::Raw || tg.fam == Family::MeshtasticBle || R.below(3) == 0) {
      l->on_io_bytes(ByteView(b));
      return;
    }
    for (size_t off = 0; off < b.size();) {
      size_t n = 1 + R.below(R.below(2) ? 7 : 3000);
      l->on_io_bytes(ByteView(b).sub(off, n));
      off += n;
    }
  }

  void check() {
    Json st;
    REQUIRE(Json::parse(l->status_json(), st));
    REQUIRE(st.is_object());
    CHECK(!st.get_string("state").empty());
    for (const auto& t : h.texts) {
      CHECK(clean(t.text));
      CHECK(clean(t.from));
      CHECK(clean(t.to));
      CHECK(!t.text.empty());
      CHECK(t.text.size() <= link::kMaxInteropTextBytes);
      CHECK(t.from.size() <= link::kMaxInteropAddrBytes);
      CHECK(t.to.size() <= link::kMaxInteropAddrBytes);
    }
    for (const auto& f : h.frames) {
      CHECK(!f.data.empty());
      CHECK(f.data.size() <= 8192);
    }
    size_t written = 0;
    for (const auto& w : h.writes) written += w.size();
    CHECK(written <= 2u * 1024 * 1024);  // per operation
    CHECK(l->tx_queue_depth() <= 1024);
    max_texts = std::max(max_texts, h.texts.size());
    h.clear();
  }

  void hostile_audio() {
    uint32_t rate = kRates[R.below(sizeof kRates / sizeof kRates[0])];
    size_t n = R.below(4) == 0 ? 0 : 1 + R.below(R.below(8) == 0 ? 60000 : 2000);
    std::vector<int16_t> pcm(n);
    int kind = static_cast<int>(R.below(5));
    for (size_t i = 0; i < n; ++i) {
      switch (kind) {
        case 0: pcm[i] = static_cast<int16_t>(R.next()); break;              // full-scale noise
        case 1: pcm[i] = 32767; break;                                       // DC
        case 2: pcm[i] = (i / 4) % 2 ? 32767 : -32768; break;                // square
        case 3: pcm[i] = 0; break;                                           // digital silence
        default: pcm[i] = static_cast<int16_t>((R.unit() - 0.5) * 40); break;  // quiet noise
      }
    }
    l->on_audio(n ? pcm.data() : nullptr, n, rate);
    if (R.below(2)) {
      std::vector<int16_t> out(R.below(5000) + 1);
      uint32_t prate = kRates[R.below(sizeof kRates / sizeof kRates[0])];
      size_t got = l->pull_audio(out.data(), out.size(), prate);
      CHECK(got <= out.size());
    }
  }

  void op() {
    auto t0 = std::chrono::steady_clock::now();
    uint32_t r = R.below(100);
    if (tg.fam == Family::Audio && r < 45) {
      hostile_audio();
    } else if (r < 40) {
      Bytes b = corpus[R.below(static_cast<uint32_t>(corpus.size()))];
      mutate(R, b, corpus);
      if (R.below(4) == 0) corpus[R.below(static_cast<uint32_t>(corpus.size()))] = b;  // evolve
      if (!b.empty()) chunked(b);
    } else if (r < 50) {
      Bytes b = R.bytes(R.below(R.below(10) == 0 ? 20000 : 300));
      if (!b.empty()) chunked(b);
    } else if (r < 72) {
      static const uint64_t steps[] = {1, 10, 20, 50, 100, 1000, 5000, 31000, 600000, 3600000, 86400000};
      uint32_t k = R.below(100);
      uint64_t dt = steps[k < 90 ? R.below(6) : 6 + R.below(5)];
      clk.t += dt;
      l->tick(clk.t);
      deliver_emu();
    } else if (r < 76) {
      connected = R.below(4) != 0;
      if (connected && tg.fam == Family::Rtl) static_cast<linkemu::RtlEmu*>(emu.get())->reset();
      l->on_io_state(connected);
      deliver_emu();
    } else if (r < 86) {
      size_t mtu = l->caps().mtu;
      size_t n = R.below(static_cast<uint32_t>(mtu + 20));
      Bytes f = R.bytes(n);
      l->send(ByteView(f), static_cast<uint8_t>(R.below(10)));
      deliver_emu();
    } else if (r < 91) {
      InteropText t;
      static const char* tos[] = {"", "QX0ABC", "!11223344", "50", "BLN1", "TOOLONGCALLSIGN", "a:b", "\x1b[2J", "-1"};
      t.to = tos[R.below(sizeof tos / sizeof tos[0])];
      Bytes txt = R.bytes(R.below(400));
      for (auto& c : txt) if (R.below(2)) c = static_cast<uint8_t>('a' + c % 26);
      t.text.assign(txt.begin(), txt.end());
      static const char* nets[] = {"", "aprs", "meshtastic", "lora_at", "rtty", "cw"};
      t.network = nets[R.below(6)];
      l->send_interop_text(t);
      deliver_emu();
    } else if (r < 93) {
      std::string err;
      l->reconfigure(R.below(2) ? tg.cfg : "{}", &err);
      deliver_emu();
    } else {
      emu_honesty();
      deliver_emu();
    }
    double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    worst_op_s = std::max(worst_op_s, dt);
    ++ops;
    check();
  }

  void emu_honesty() {
    if (emu) emu->honest = R.below(3) != 0;
  }

  void run(size_t n) {
    connected = true;
    l->on_io_state(true);
    deliver_emu();
    for (size_t i = 0; i < n; ++i) op();
    CHECK(worst_op_s < 2.0);
    recover();
  }

  // Wedge check: honest device, clean session.
  void recover() {
    if (emu) emu->honest = true;
    if (emu && tg.fam == Family::Rtl) static_cast<linkemu::RtlEmu*>(emu.get())->reset();
    connected = true;
    l->on_io_state(true);
    std::string err;
    REQUIRE(l->reconfigure(tg.cfg, &err));  // back to the real configuration
    h.clear();
    if (emu) {
      bool up = false;
      for (int k = 0; k < 4000 && !up; ++k) {  // up to 200 s of simulated time
        clk.t += 50;
        l->tick(clk.t);
        deliver_emu();
        up = l->state() == LinkState::Up;
      }
      CHECK_MESSAGE(up, tg.type << " " << tg.cfg << " seed " << seed << " not Up after a clean session: "
                                << l->status_json());
      h.clear();
    }
    switch (tg.fam) {
      case Family::Raw: case Family::Stream: case Family::Kiss: recover_bytes(); break;
      case Family::Audio: recover_audio(); break;
      default: break;
    }
  }

  void recover_bytes() {
    // A peer of the same configuration sends one clean frame; it must arrive. Then we must transmit.
    Clock pc;
    pc.t = clk.t;
    FakeHost ph(&pc, 999);
    std::string err;
    std::string pcfg = tg.cfg;
    size_t p = pcfg.find("QX0ABC");
    if (p != std::string::npos) pcfg.replace(p, 6, "QX2XYZ");
    auto peer = create_link(tg.type, pcfg, ph, &err);
    REQUIRE(peer);
    peer->on_io_state(true);
    Bytes f = R.bytes(40);
    REQUIRE(peer->send(ByteView(f), 0));
    for (int k = 0; k < 200 && ph.writes.empty(); ++k) { pc.t += 50; peer->tick(pc.t); }
    Bytes wire = ph.written();
    REQUIRE(!wire.empty());
    if (tg.fam == Family::Stream) {
      Bytes flush(20000, 0);  // disproves any half-received hostile candidate
      l->on_io_bytes(ByteView(flush));
    } else if (tg.fam == Family::Kiss) {
      Bytes fend{0xC0};
      l->on_io_bytes(ByteView(fend));
    }
    l->on_io_bytes(ByteView(wire));
    bool got = false;
    for (const auto& fr : h.frames) got |= fr.data == f;
    CHECK_MESSAGE(got, tg.type << " " << tg.cfg << " seed " << seed << " did not deliver a clean frame");
    h.clear();
    REQUIRE(l->send(ByteView(R.bytes(30)), 0));
    for (int k = 0; k < 2000 && h.writes.empty(); ++k) { clk.t += 50; l->tick(clk.t); }
    CHECK_MESSAGE(!h.writes.empty(), tg.type << " " << tg.cfg << " seed " << seed << " never transmitted again");
  }

  void recover_audio() {
    modem::ModemInfo info;
    for (const auto& mi : modem::modem_catalog())
      if (tg.cfg.find("\"" + mi.name + "\"") != std::string::npos) info = mi;
    const uint32_t rate = 48000;
    // Our link must key up again (CSMA not wedged by the hostile audio): feed silence meanwhile.
    std::vector<int16_t> zero(rate / 100, 0), out(rate / 100);
    bool sent = info.frames ? l->send(ByteView(R.bytes(20)), 0) : [&] {
      InteropText t;
      t.text = info.name == "dtmf" ? "123" : "TEST";
      return l->send_interop_text(t);
    }();
    CHECK(sent);
    bool keyed = false;
    for (int k = 0; k < 3000 && !keyed; ++k) {
      l->on_audio(zero.data(), zero.size(), rate);
      clk.t += 10;
      l->tick(clk.t);
      keyed = l->pull_audio(out.data(), out.size(), rate) > 0;
    }
    CHECK_MESSAGE(keyed, tg.cfg << " seed " << seed << " never keyed up after hostile audio");
    // Drain that transmission.
    for (int k = 0; k < 30000; ++k) {
      clk.t += 10;
      l->tick(clk.t);
      if (l->pull_audio(out.data(), out.size(), rate) == 0 && !l->channel_busy()) break;
    }
    // A clean transmission from a peer is decoded.
    Clock pc;
    pc.t = clk.t;
    FakeHost ph(&pc, 777);
    std::string err, pcfg = tg.cfg;
    size_t p = pcfg.find("QX0ABC");
    if (p != std::string::npos) pcfg.replace(p, 6, "QX2XYZ");
    auto peer = create_link("audio", pcfg, ph, &err);
    REQUIRE(peer);
    peer->on_io_state(true);
    Bytes f = R.bytes(20);
    if (info.frames) REQUIRE(peer->send(ByteView(f), 0));
    else { InteropText t; t.text = info.name == "dtmf" ? "4567" : "CQ CQ"; REQUIRE(peer->send_interop_text(t)); }
    std::vector<int16_t> tx;
    bool started = false;
    for (int k = 0; k < 60000; ++k) {
      pc.t += 10;
      peer->tick(pc.t);
      size_t n = peer->pull_audio(out.data(), out.size(), rate);
      if (n) started = true;
      tx.insert(tx.end(), out.begin(), out.begin() + static_cast<long>(n));
      if (started && n < out.size()) break;
    }
    REQUIRE(!tx.empty());
    // Like a real platform, keep pulling our TX audio while receiving. Our link may still have APRS
    // retries pending from the hostile phase; if it keys up over the peer (half-duplex collision),
    // the peer simply tries again, as it would on air.
    bool got = false;
    for (int attempt = 0; attempt < 4 && !got; ++attempt) {
      // The peer does CSMA too: it starts only once our transmission (if any) has ended.
      for (int k = 0; k < 20000; ++k) {
        l->on_audio(zero.data(), zero.size(), rate);
        clk.t += 10;
        l->tick(clk.t);
        if (l->pull_audio(out.data(), out.size(), rate) == 0 && !l->channel_busy()) break;
      }
      h.clear();
      bool collided = false;
      for (size_t off = 0; off < tx.size(); off += 480) {
        l->on_audio(tx.data() + off, std::min<size_t>(480, tx.size() - off), rate);
        clk.t += 10;
        l->tick(clk.t);
        collided |= l->pull_audio(out.data(), out.size(), rate) > 0;
      }
      for (int k = 0; k < 600; ++k) {  // trailing silence, text flush
        l->on_audio(zero.data(), zero.size(), rate);
        clk.t += 10;
        l->tick(clk.t);
        l->pull_audio(out.data(), out.size(), rate);
      }
      got = info.frames ? !h.frames.empty() : !h.texts.empty();
      if (!collided) break;
    }
    if (!got) {  // control: does a fresh receiver decode the very same audio?
      Clock fc;
      fc.t = clk.t;
      FakeHost fh(&fc, 555);
      auto fresh = create_link("audio", tg.cfg, fh, &err);
      REQUIRE(fresh);
      fresh->on_io_state(true);
      for (size_t off = 0; off < tx.size(); off += 480) {
        fresh->on_audio(tx.data() + off, std::min<size_t>(480, tx.size() - off), rate);
        fc.t += 10;
        fresh->tick(fc.t);
      }
      for (int k = 0; k < 600; ++k) { fresh->on_audio(zero.data(), zero.size(), rate); fc.t += 10; fresh->tick(fc.t); }
      bool fresh_ok = info.frames ? !fh.frames.empty() : !fh.texts.empty();
      MESSAGE("fresh receiver decodes it: " << fresh_ok << "; wedged status: " << l->status_json());
    }
    CHECK_MESSAGE(got, tg.cfg << " seed " << seed << " did not decode a clean transmission after hostile audio");
  }
};

void fuzz_all(size_t ops, std::vector<uint64_t> seeds) {
  for (const Target& t : targets()) {
    for (uint64_t s : seeds) {
      CAPTURE(t.type);
      CAPTURE(t.cfg);
      CAPTURE(s);
      Fuzzer f(t, s);
      f.run(ops);
    }
  }
}

}  // namespace

TEST_SUITE("link") {

TEST_CASE("fuzz: every link driver survives mutated input and recovers (fast)") { fuzz_all(120, {1}); }

TEST_CASE("fuzz long: every link driver, many seeds" * doctest::skip()) {
  std::vector<uint64_t> seeds;
  for (uint64_t s = 100; s < 116; ++s) seeds.push_back(s);
  fuzz_all(2000, seeds);
}

}  // TEST_SUITE
