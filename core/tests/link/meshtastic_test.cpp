// Meshtastic driver: protobuf known vectors (cross-checked with the official Python `meshtastic`
// protobuf classes), and a scripted device emulator speaking the serial/TCP client API.
#include <string>
#include <vector>

#include "doctest.h"
#include "link/meshtastic_proto.h"
#include "link_harness.h"

using namespace ember;
using namespace linktest;
namespace mt = ember::link::mt;

namespace {

Bytes hexb(const char* h) {
  Bytes b;
  from_hex(h, b);
  return b;
}

// Scripted Meshtastic device attached to one driver instance.
struct Emu {
  Clock* clk;
  FakeHost host;
  std::unique_ptr<Link> link;
  mt::Deframer dfr;
  std::vector<Bytes> pending;  // raw writes not yet processed
  std::vector<mt::ToRadio> got;
  std::vector<mt::MeshPacket> packets;
  uint32_t node_num;
  bool respond = true;
  bool ble = false;
  bool send_queue_status = false;
  uint32_t queue_free = 16;
  uint32_t region = 3, preset = 4;
  size_t wake_bytes = 0;
  std::string error;

  Emu(Clock* c, uint32_t num, const std::string& cfg = "{}") : clk(c), host(c, num), node_num(num) {
    link = create_link("meshtastic", cfg, host, &error);
    host.on_write = [this](ByteView v) { pending.push_back(v.to_bytes()); };
  }
  void to_link(const mt::FromRadio& f) {
    Bytes pb = mt::encode_from_radio(f);
    if (ble) link->on_io_bytes(ByteView(pb));
    else link->on_io_bytes(ByteView(mt::frame(ByteView(pb))));
  }
  void handle(ByteView pb) {
    mt::ToRadio t;
    REQUIRE(mt::decode_to_radio(pb, &t));
    got.push_back(t);
    if (t.kind == mt::ToKind::WantConfig && respond) {
      mt::FromRadio f;
      f.kind = mt::FromKind::MyInfo; f.my_node_num = node_num; to_link(f);
      f = mt::FromRadio(); f.kind = mt::FromKind::NodeInfo; f.node.num = node_num;
      f.node.long_name = "Base Camp"; f.node.short_name = "BC"; to_link(f);
      f = mt::FromRadio(); f.kind = mt::FromKind::NodeInfo; f.node.num = 0x1234; to_link(f);
      f = mt::FromRadio(); f.kind = mt::FromKind::Config; f.has_lora = true;
      f.lora.use_preset = true; f.lora.modem_preset = preset; f.lora.region = region; to_link(f);
      f = mt::FromRadio(); f.kind = mt::FromKind::Metadata; f.firmware_version = "2.5.6.test"; to_link(f);
      f = mt::FromRadio(); f.kind = mt::FromKind::ConfigComplete; f.config_complete_id = t.want_config_id; to_link(f);
    } else if (t.kind == mt::ToKind::Packet) {
      packets.push_back(t.packet);
      if (send_queue_status) {
        if (queue_free) --queue_free;
        mt::FromRadio f;
        f.kind = mt::FromKind::QueueStatus; f.queue.free = queue_free; f.queue.maxlen = 16;
        f.queue.mesh_packet_id = t.packet.id;
        to_link(f);
      }
    }
  }
  void poll() {
    std::vector<Bytes> w;
    w.swap(pending);
    for (const Bytes& b : w) {
      if (ble) { handle(ByteView(b)); continue; }
      dfr.feed(ByteView(b), [&](ByteView pb) { handle(pb); }, [&](char c) { if (static_cast<uint8_t>(c) == 0xC3) ++wake_bytes; });
    }
  }
  void step(uint32_t ms) {
    clk->t += ms;
    link->tick(clk->t);
    poll();
  }
  void run(uint32_t ms, uint32_t st = 50) { for (uint32_t t = 0; t < ms; t += st) step(st); }
  void up() {
    link->on_io_state(true);
    poll();
    run(400);
  }
  size_t count(mt::ToKind k) const {
    size_t n = 0;
    for (const auto& t : got) n += t.kind == k;
    return n;
  }
  mt::FromRadio packet_from(uint32_t from, uint32_t port, ByteView payload, uint32_t channel = 0,
                            uint32_t to = mt::kBroadcast) {
    mt::FromRadio f;
    f.kind = mt::FromKind::Packet;
    f.packet.from = from; f.packet.to = to; f.packet.channel = channel; f.packet.id = 77;
    f.packet.has_decoded = true; f.packet.decoded.portnum = port; f.packet.decoded.payload = payload.to_bytes();
    f.packet.has_snr = true; f.packet.rx_snr = 6.25f; f.packet.rx_rssi = -97;
    return f;
  }
};

}  // namespace

TEST_SUITE("link") {

TEST_CASE("meshtastic: protobuf encodings match the official classes") {
  mt::ToRadio t;
  t.kind = mt::ToKind::WantConfig;
  t.want_config_id = 0x12345678;
  CHECK(to_hex(ByteView(mt::encode_to_radio(t))) == "18f8acd19101");
  t = mt::ToRadio();
  t.kind = mt::ToKind::Heartbeat;
  CHECK(to_hex(ByteView(mt::encode_to_radio(t))) == "3a00");
  t = mt::ToRadio();
  t.kind = mt::ToKind::Packet;
  t.packet.to = mt::kBroadcast; t.packet.channel = 1; t.packet.has_decoded = true;
  t.packet.decoded.portnum = mt::kPortPrivate; t.packet.decoded.payload = {1, 2, 3};
  t.packet.id = 0x01020305; t.packet.hop_limit = 3; t.packet.priority = 64;
  CHECK(to_hex(ByteView(mt::encode_to_radio(t))) == "0a1a15ffffffff180122080880021203010203350503020148035840");

  mt::FromRadio f;
  REQUIRE(mt::decode_from_radio(ByteView(hexb("080712290dd4c3b2a115ffffffff220608011202686935fecaad0b450000c8404802609fffffffffffffffff01")), &f));
  CHECK(f.id == 7);
  CHECK(f.kind == mt::FromKind::Packet);
  CHECK(f.packet.from == 0xa1b2c3d4u);
  CHECK(f.packet.to == mt::kBroadcast);
  CHECK(f.packet.decoded.portnum == 1);
  CHECK(ByteView(f.packet.decoded.payload).to_string() == "hi");
  CHECK(f.packet.id == 0x0badcafeu);
  CHECK(f.packet.rx_snr == doctest::Approx(6.25));
  CHECK(f.packet.rx_rssi == -97);
  CHECK(f.packet.hop_limit == 2);
  // our encoder produces the same bytes
  CHECK(to_hex(ByteView(mt::encode_from_radio(f))) ==
        "080712290dd4c3b2a115ffffffff220608011202686935fecaad0b450000c8404802609fffffffffffffffff01");

  REQUIRE(mt::decode_from_radio(ByteView(hexb("38f8acd19101")), &f));
  CHECK(f.kind == mt::FromKind::ConfigComplete);
  CHECK(f.config_complete_id == 0x12345678u);
  REQUIRE(mt::decode_from_radio(ByteView(hexb("1a0608d487cb8d0a")), &f));
  CHECK(f.kind == mt::FromKind::MyInfo);
  CHECK(f.my_node_num == 0xa1b2c3d4u);
  REQUIRE(mt::decode_from_radio(ByteView(hexb("2a0e320c08011004380340034801501b")), &f));
  CHECK(f.kind == mt::FromKind::Config);
  REQUIRE(f.has_lora);
  CHECK(f.lora.use_preset);
  CHECK(f.lora.modem_preset == 4);
  CHECK(f.lora.region == 3);
  CHECK(f.lora.hop_limit == 3);
  CHECK(f.lora.tx_enabled);
  CHECK(f.lora.tx_power == 27);
  REQUIRE(mt::decode_from_radio(ByteView(hexb("5a06100f18102005")), &f));
  CHECK(f.kind == mt::FromKind::QueueStatus);
  CHECK(f.queue.free == 15);
  CHECK(f.queue.maxlen == 16);
  CHECK(f.queue.mesh_packet_id == 5);
  REQUIRE(mt::decode_from_radio(ByteView(hexb("6a0e0a0c322e352e362e616263646566")), &f));
  CHECK(f.kind == mt::FromKind::Metadata);
  CHECK(f.firmware_version == "2.5.6.abcdef");
  REQUIRE(mt::decode_from_radio(
      ByteView(hexb("222908d487cb8d0a121c0a092161316232633364341209426173652043616d701a024243280925000060c0")), &f));
  CHECK(f.kind == mt::FromKind::NodeInfo);
  CHECK(f.node.num == 0xa1b2c3d4u);
  CHECK(f.node.user_id == "!a1b2c3d4");
  CHECK(f.node.long_name == "Base Camp");
  CHECK(f.node.short_name == "BC");
  CHECK(f.node.hw_model == 9);
  CHECK(f.node.snr == doctest::Approx(-3.5));
}

TEST_CASE("meshtastic: protobuf decoder rejects malformed input") {
  mt::FromRadio f;
  CHECK(!mt::decode_from_radio(ByteView(hexb("38")), &f));            // truncated varint value
  CHECK(!mt::decode_from_radio(ByteView(hexb("12")), &f));            // missing length
  CHECK(!mt::decode_from_radio(ByteView(hexb("1205aabb")), &f));      // length past end
  CHECK(!mt::decode_from_radio(ByteView(hexb("0b")), &f));            // group start
  CHECK(!mt::decode_from_radio(ByteView(hexb("15aabbccdd")), &f));    // packet field with fixed32 wire type
  CHECK(!mt::decode_from_radio(ByteView(hexb("38ffffffffffffffffffff01")), &f));  // 11-byte varint
  CHECK(!mt::decode_from_radio(ByteView(hexb("00")), &f));            // field 0
  CHECK(!mt::decode_from_radio(ByteView(hexb("12020d01")), &f));      // truncated nested fixed32
  CHECK(mt::decode_from_radio(ByteView(hexb("f80701")), &f));         // unknown field 127 skipped
  CHECK(f.kind == mt::FromKind::Other);
  CHECK(mt::decode_from_radio(ByteView(), &f));
  CHECK(f.kind == mt::FromKind::None);
  // Deframer: garbage, oversize length, then a valid frame.
  mt::Deframer d;
  std::vector<Bytes> out;
  std::string text;
  Bytes in = {'l', 'o', 'g', '\n', 0x94, 0x00, 0x94, 0xC3, 0x02, 0x01, 0x94, 0xC3, 0x00, 0x02, 0x3a, 0x00};
  d.feed(ByteView(in), [&](ByteView p) { out.push_back(p.to_bytes()); }, [&](char c) { text.push_back(c); });
  REQUIRE(out.size() == 1);
  CHECK((out[0] == Bytes{0x3a, 0x00}));
  CHECK(text.substr(0, 4) == "log\n");
  CHECK(d.bad() >= 2);
}

TEST_CASE("meshtastic: handshake, state transitions and status") {
  Clock clk;
  Emu e(&clk, 0xa1b2c3d4);
  REQUIRE(e.link);
  CHECK(e.link->state() == LinkState::Down);
  e.respond = false;
  e.link->on_io_state(true);
  CHECK(e.link->state() == LinkState::Connecting);
  e.poll();
  CHECK(e.wake_bytes == 32);
  CHECK(e.count(mt::ToKind::WantConfig) == 0);  // waits for the wake-up delay
  e.run(200);
  REQUIRE(e.count(mt::ToKind::WantConfig) == 1);
  uint32_t nonce = e.got.back().want_config_id;
  // wrong nonce does not complete the handshake
  mt::FromRadio f;
  f.kind = mt::FromKind::ConfigComplete;
  f.config_complete_id = nonce + 1;
  e.to_link(f);
  CHECK(e.link->state() == LinkState::Connecting);
  f.config_complete_id = nonce;
  e.to_link(f);
  CHECK(e.link->state() == LinkState::Up);
  // full handshake through the emulator reports identity and device radio settings
  Emu g(&clk, 0xa1b2c3d4);
  g.up();
  REQUIRE(g.link->state() == LinkState::Up);
  Json st = parse_json(g.link->status_json());
  CHECK(st.get_string("node_id") == "!a1b2c3d4");
  CHECK(st.get_string("long_name") == "Base Camp");
  CHECK(st.get_string("short_name") == "BC");
  CHECK(st.get_string("firmware_version") == "2.5.6.test");
  CHECK(st.get_string("region") == "EU_868");
  CHECK(st.get_string("modem_preset") == "MEDIUM_FAST");
  CHECK(st.get_int("nodes_known") == 2);
  LinkCaps c = g.link->caps();
  CHECK(c.mtu == 233);
  CHECK(c.duty_cycle_permille == 100);
  CHECK(c.bitrate_bps == 3515);  // SF9 / 250 kHz / CR 4/5
  CHECK(c.broadcast);
  CHECK(c.half_duplex);
  CHECK(c.encryption_allowed);
  CHECK(!c.requires_station_id);
  CHECK(c.cost == 20);
  // endpoint closes
  g.link->on_io_state(false);
  CHECK(g.link->state() == LinkState::Down);
  // default caps before any device info: LongFast, no duty-cycle limit
  Emu d(&clk, 5);
  CHECK(d.link->caps().bitrate_bps == 1074);
  CHECK(d.link->caps().duty_cycle_permille == 1000);
}

TEST_CASE("meshtastic: handshake timeout, retries, error and recovery") {
  Clock clk;
  Emu e(&clk, 42, R"({"handshake_timeout_s":2})");
  e.respond = false;
  e.link->on_io_state(true);
  e.poll();
  e.run(2500);
  CHECK(e.count(mt::ToKind::WantConfig) == 2);
  CHECK(e.link->state() == LinkState::Connecting);
  e.run(5000);
  CHECK(e.count(mt::ToKind::WantConfig) == 3);
  CHECK(e.link->state() == LinkState::Error);
  CHECK(parse_json(e.link->status_json()).get_string("last_error").find("did not answer") != std::string::npos);
  // nonces differ per attempt
  CHECK(e.got[0].want_config_id != e.got[1].want_config_id);
  // device comes back: periodic retry succeeds
  e.respond = true;
  e.run(31000, 100);
  CHECK(e.link->state() == LinkState::Up);
  CHECK(parse_json(e.link->status_json()).get_string("last_error").empty());
  // device reboot restarts the handshake
  mt::FromRadio f;
  f.kind = mt::FromKind::Rebooted;
  e.to_link(f);
  CHECK(e.link->state() == LinkState::Connecting);
  e.poll();
  e.run(300);
  CHECK(e.link->state() == LinkState::Up);
}

TEST_CASE("meshtastic: send every size, reject mtu+1, pacing and heartbeat") {
  Clock clk;
  Emu e(&clk, 0x100, R"({"channel":2,"hop_limit":5,"queue_frames":128,"modem_preset":"SHORT_TURBO"})");
  e.up();
  REQUIRE(e.link->state() == LinkState::Up);
  CHECK(!e.link->send(ByteView(Bytes(234, 1)), 3));
  CHECK(!e.link->send(ByteView(), 3));
  Prng r(9);
  std::vector<Bytes> sent;
  for (size_t n = 1; n <= 233; ++n) {
    Bytes b = r.bytes(n);
    while (!e.link->send(ByteView(b), 4)) e.step(50);
    sent.push_back(b);
    e.poll();
  }
  e.run(60000, 100);
  REQUIRE(e.packets.size() == sent.size());
  for (size_t i = 0; i < sent.size(); ++i) {
    const auto& p = e.packets[i];
    CHECK(p.to == mt::kBroadcast);
    CHECK(p.channel == 2);
    CHECK(p.hop_limit == 5);
    CHECK(p.decoded.portnum == mt::kPortPrivate);
    CHECK(p.decoded.payload == sent[i]);
    CHECK(p.id != 0);
    CHECK(!p.want_ack);
  }
  // pacing: two frames queued at once do not reach the device in the same instant
  size_t before = e.packets.size();
  e.link->send(ByteView(Bytes(200, 1)), 3);
  e.link->send(ByteView(Bytes(200, 2)), 3);
  e.poll();
  CHECK(e.packets.size() == before + 1);
  CHECK(e.link->channel_busy());
  e.run(2000);
  CHECK(e.packets.size() == before + 2);
  // heartbeat
  size_t hb = e.count(mt::ToKind::Heartbeat);
  e.run(61000, 500);
  CHECK(e.count(mt::ToKind::Heartbeat) >= hb + 1);
  Json st = parse_json(e.link->status_json());
  CHECK(st.get_int("tx_frames") == 235);
  CHECK(st.get_int("airtime_ms") > 0);
}

TEST_CASE("meshtastic: priority order, queue limits and device queue back-pressure") {
  Clock clk;
  Emu e(&clk, 0x100, R"({"queue_frames":4})");
  e.up();
  e.link->send(ByteView(Bytes{9}), 3);  // goes straight out, starts the airtime pacer
  e.poll();
  REQUIRE(e.packets.size() == 1);
  CHECK(e.link->send(ByteView(Bytes{5}), 5));
  CHECK(e.link->send(ByteView(Bytes{7}), 7));
  CHECK(e.link->send(ByteView(Bytes{0}), 0));
  CHECK(e.link->send(ByteView(Bytes{6}), 6));
  CHECK(e.link->tx_queue_depth() == 4);
  CHECK(!e.link->send(ByteView(Bytes{8}), 7));  // full, nothing lower to evict
  CHECK(e.link->send(ByteView(Bytes{1}), 1));   // evicts the priority-7 frame
  e.run(20000, 100);
  REQUIRE(e.packets.size() == 5);
  CHECK((e.packets[1].decoded.payload == Bytes{0}));
  CHECK((e.packets[2].decoded.payload == Bytes{1}));
  CHECK((e.packets[3].decoded.payload == Bytes{5}));
  CHECK((e.packets[4].decoded.payload == Bytes{6}));
  CHECK(e.packets[1].priority == 100);
  // device reports a full queue: driver holds frames until it frees up
  e.send_queue_status = true;
  e.queue_free = 1;
  e.link->send(ByteView(Bytes{20}), 3);
  e.poll();
  e.run(3000);
  e.link->send(ByteView(Bytes{21}), 3);
  e.run(3000);
  CHECK(e.packets.size() == 6);
  CHECK(e.link->tx_queue_depth() == 1);
  mt::FromRadio f;
  f.kind = mt::FromKind::QueueStatus; f.queue.free = 8; f.queue.maxlen = 16;
  e.to_link(f);
  e.run(3000);
  CHECK(e.packets.size() == 7);
}

TEST_CASE("meshtastic: receive frames of every size with RSSI/SNR, filters") {
  Clock clk;
  Emu e(&clk, 0x100);
  e.up();
  Prng r(4);
  for (size_t n = 1; n <= 233; ++n) {
    Bytes b = r.bytes(n);
    e.to_link(e.packet_from(0x200, mt::kPortPrivate, ByteView(b)));
    REQUIRE(e.host.frames.size() == n);
    CHECK(e.host.frames.back().data == b);
  }
  const RxMeta& m = e.host.frames.back().meta;
  CHECK(m.has_rssi);
  CHECK(m.rssi_dbm == doctest::Approx(-97));
  CHECK(m.has_snr);
  CHECK(m.snr_db == doctest::Approx(6.25));
  size_t n0 = e.host.frames.size();
  e.to_link(e.packet_from(0x100, mt::kPortPrivate, ByteView(Bytes{1})));       // our own
  e.to_link(e.packet_from(0x200, mt::kPortPrivate, ByteView(Bytes{1}), 3));    // other channel
  e.to_link(e.packet_from(0x200, 67, ByteView(Bytes{1})));                     // telemetry port
  e.to_link(e.packet_from(0x200, mt::kPortPrivate, ByteView()));               // empty
  mt::FromRadio enc;
  enc.kind = mt::FromKind::Packet; enc.packet.from = 0x300; enc.packet.to = mt::kBroadcast;
  e.to_link(enc);                                                              // still encrypted
  CHECK(e.host.frames.size() == n0);
  Json st = parse_json(e.link->status_json());
  CHECK(st.get_int("own_packets") == 1);
  CHECK(st.get_int("ignored_packets") == 3);
  CHECK(st.get_int("encrypted_packets") == 1);
  CHECK(st.get_int("last_rssi_dbm") == -97);
}

TEST_CASE("meshtastic: text bridge both ways") {
  Clock clk;
  Emu e(&clk, 0x100);
  e.up();
  e.to_link(e.packet_from(0xdeadbeef, mt::kPortText, ByteView(std::string_view("hello camp"))));
  REQUIRE(e.host.texts.size() == 1);
  CHECK(e.host.texts[0].from == "!deadbeef");
  CHECK(e.host.texts[0].to == "");
  CHECK(e.host.texts[0].text == "hello camp");
  CHECK(e.host.texts[0].network == "meshtastic");
  // DM to us on another channel index still bridged; text on a foreign channel is not
  e.to_link(e.packet_from(0xdeadbeef, mt::kPortText, ByteView(std::string_view("dm")), 1, 0x100));
  e.to_link(e.packet_from(0xdeadbeef, mt::kPortText, ByteView(std::string_view("other")), 1));
  REQUIRE(e.host.texts.size() == 2);
  CHECK(e.host.texts[1].to == "!00000100");
  InteropText t;
  t.text = "reply all";
  CHECK(e.link->send_interop_text(t));
  t.to = "!deadbeef";
  t.text = std::string(300, 'x');
  t.network = "meshtastic";
  CHECK(e.link->send_interop_text(t));
  t.to = "nonsense!";
  CHECK(!e.link->send_interop_text(t));
  t.to = "";
  t.network = "aprs";
  CHECK(!e.link->send_interop_text(t));
  e.run(10000, 100);
  REQUIRE(e.packets.size() == 2);
  CHECK(e.packets[0].decoded.portnum == mt::kPortText);
  CHECK(ByteView(e.packets[0].decoded.payload).to_string() == "reply all");
  CHECK(e.packets[0].to == mt::kBroadcast);
  CHECK(e.packets[1].to == 0xdeadbeefu);
  CHECK(e.packets[1].want_ack);
  CHECK(e.packets[1].decoded.payload.size() == 200);
  // bridge disabled
  CHECK(e.link->reconfigure(R"({"text_bridge":false})", nullptr));
  CHECK(!e.link->send_interop_text(t));
  e.to_link(e.packet_from(0xdeadbeef, mt::kPortText, ByteView(std::string_view("x"))));
  CHECK(e.host.texts.size() == 2);
}

TEST_CASE("meshtastic: garbage, truncation and bit flips (seeded fuzz)") {
  Clock clk;
  Emu e(&clk, 0x100);
  e.up();
  Prng r(1234);
  for (int i = 0; i < 3000; ++i) {
    Bytes valid = mt::frame(ByteView(mt::encode_from_radio(e.packet_from(0x200, mt::kPortPrivate, ByteView(r.bytes(1 + r.below(233)))))));
    Bytes in;
    switch (r.below(5)) {
      case 0: in = r.bytes(r.below(600)); break;
      case 1: in.assign(valid.begin(), valid.begin() + static_cast<long>(r.below(static_cast<uint32_t>(valid.size())))); break;
      case 2: in = valid; for (int k = 0; k < 3; ++k) in[r.below(static_cast<uint32_t>(in.size()))] ^= static_cast<uint8_t>(1u << r.below(8)); break;
      case 3: { std::string s = "DEBUG | ??:??:?? 12 [Router] log line\r\n"; in.assign(s.begin(), s.end()); break; }
      default: { Bytes pb = r.bytes(r.below(80)); in = mt::frame(ByteView(pb)); break; }
    }
    e.link->on_io_bytes(ByteView(in));
    if (i % 50 == 0) e.step(20);
  }
  // resynchronise: a log line, then a clean frame is delivered
  std::string log = "INFO | booting\n";
  e.link->on_io_bytes(ByteView(std::string_view(log)));
  // flush any half-received frame the fuzz left behind with a long run of filler
  Bytes filler(600, 0x00);
  e.link->on_io_bytes(ByteView(filler));
  size_t n = e.host.frames.size();
  e.to_link(e.packet_from(0x200, mt::kPortPrivate, ByteView(Bytes{1, 2, 3})));
  REQUIRE(e.host.frames.size() == n + 1);
  CHECK((e.host.frames.back().data == Bytes{1, 2, 3}));
  Json st = parse_json(e.link->status_json());
  CHECK(st.get_int("decode_errors") > 0);
  CHECK(st.get_int("framing_errors") > 0);
}

TEST_CASE("meshtastic: end to end through two emulated devices") {
  Clock clk;
  Emu a(&clk, 0xA), b(&clk, 0xB);
  a.up();
  b.up();
  REQUIRE(a.link->state() == LinkState::Up);
  REQUIRE(b.link->state() == LinkState::Up);
  Prng r(77);
  std::vector<Bytes> sent;
  for (int i = 0; i < 20; ++i) {
    sent.push_back(r.bytes(1 + r.below(233)));
    REQUIRE(a.link->send(ByteView(sent.back()), static_cast<uint8_t>(3)));
    a.run(3000, 100);
    // relay over "the air": what device A transmitted arrives at device B
    for (const auto& p : a.packets) {
      mt::FromRadio f;
      f.kind = mt::FromKind::Packet;
      f.packet = p;
      f.packet.from = 0xA;
      f.packet.rx_rssi = -110;
      b.to_link(f);
    }
    a.packets.clear();
  }
  REQUIRE(b.host.frames.size() == sent.size());
  for (size_t i = 0; i < sent.size(); ++i) CHECK(b.host.frames[i].data == sent[i]);
}

TEST_CASE("meshtastic: BLE framing, reconfigure and licensed mode") {
  Clock clk;
  Emu e(&clk, 0x100, R"({"framing":"ble"})");
  e.ble = true;
  e.link->on_io_state(true);
  e.poll();
  REQUIRE(e.count(mt::ToKind::WantConfig) == 1);  // no wake-up bytes, no 0x94C3 header
  CHECK(e.link->state() == LinkState::Up);
  e.link->send(ByteView(Bytes{1, 2}), 3);
  e.poll();
  REQUIRE(e.packets.size() == 1);
  e.to_link(e.packet_from(0x200, mt::kPortPrivate, ByteView(Bytes{4, 5})));
  REQUIRE(e.host.frames.size() == 1);
  e.link->on_io_bytes(ByteView(Bytes(600, 0x12)));  // oversize datagram
  CHECK(parse_json(e.link->status_json()).get_int("rx_errors") == 1);

  std::string err;
  CHECK(!e.link->reconfigure(R"({"channel":9})", &err));
  CHECK(!err.empty());
  CHECK(e.link->reconfigure(R"({"framing":"ble","licensed":true,"region":"UA_868","modem_preset":"LONG_SLOW","cost":33})", &err));
  LinkCaps c = e.link->caps();
  CHECK(!c.encryption_allowed);
  CHECK(c.requires_station_id);
  CHECK(c.duty_cycle_permille == 10);
  CHECK(c.bitrate_bps == 183);
  CHECK(c.cost == 33);
  CHECK(e.link->state() == LinkState::Up);
  // switching framing restarts the handshake
  e.ble = false;
  CHECK(e.link->reconfigure("{}", &err));
  CHECK(e.link->state() == LinkState::Connecting);
  e.poll();
  e.run(300);
  CHECK(e.link->state() == LinkState::Up);
}

TEST_CASE("M5 regression: handshake times out while the device only prints its debug log") {
  Clock clk;
  Emu e(&clk, 0x11223344);
  REQUIRE(e.link);
  e.respond = false;  // lost our want_config (e.g. it was booting), only chatters
  e.link->on_io_state(true);
  bool saw_error = false;
  for (int s = 0; s < 120 && !saw_error; ++s) {  // two minutes
    for (int k = 0; k < 20; ++k) e.step(50);
    e.link->on_io_bytes(ByteView(std::string_view("INFO  | ??:??:?? 12 [Router] heard packet\r\n")));
    saw_error = e.link->state() == LinkState::Error;
  }
  CHECK(saw_error);
  Json st = parse_json(e.link->status_json());
  CHECK(st.get_int("handshakes") >= 3);
  // Once the device answers again the link comes up on its own (retry from Error), still chattering.
  e.respond = true;
  bool up = false;
  for (int s = 0; s < 120 && !up; ++s) {
    for (int k = 0; k < 20; ++k) e.step(50);
    e.link->on_io_bytes(ByteView(std::string_view("INFO  | log\r\n")));
    up = e.link->state() == LinkState::Up;
  }
  CHECK(up);
}

TEST_CASE("M5: a slow but progressing config download is not cut off") {
  Clock clk;
  Emu e(&clk, 0x11223344, R"({"handshake_timeout_s":2})");
  REQUIRE(e.link);
  e.respond = false;
  e.link->on_io_state(true);
  e.run(200);  // want_config sent
  uint32_t nonce = 0;
  for (const auto& t : e.got) if (t.kind == mt::ToKind::WantConfig) nonce = t.want_config_id;
  REQUIRE(nonce != 0);
  uint64_t hs = static_cast<uint64_t>(parse_json(e.link->status_json()).get_int("handshakes"));
  mt::FromRadio f;
  f.kind = mt::FromKind::MyInfo; f.my_node_num = 0x11223344; e.to_link(f);
  for (int i = 0; i < 40; ++i) {  // 40 node infos, one per second: 40 s > 2 s idle timeout
    e.run(1000);
    f = mt::FromRadio(); f.kind = mt::FromKind::NodeInfo; f.node.num = 0x1000u + static_cast<uint32_t>(i); e.to_link(f);
  }
  f = mt::FromRadio(); f.kind = mt::FromKind::ConfigComplete; f.config_complete_id = nonce; e.to_link(f);
  CHECK(e.link->state() == LinkState::Up);
  CHECK(static_cast<uint64_t>(parse_json(e.link->status_json()).get_int("handshakes")) == hs);
}

TEST_CASE("L9 regression: Meshtastic node names and firmware strings are sanitised and bounded") {
  Clock clk;
  Emu e(&clk, 0x11223344);
  REQUIRE(e.link);
  e.up();
  mt::FromRadio f;
  f.kind = mt::FromKind::NodeInfo; f.node.num = 0x11223344;
  f.node.long_name = std::string("Base\x1b[31m Camp\r\nFAKE\x07") + std::string(300, 'x');
  f.node.short_name = "B\x00\x01";
  e.to_link(f);
  f = mt::FromRadio(); f.kind = mt::FromKind::Metadata; f.firmware_version = std::string("2.5\x1b]0;x\x07") + std::string(500, 'y');
  e.to_link(f);
  Json st = parse_json(e.link->status_json());
  for (const char* key : {"long_name", "short_name", "firmware_version"}) {
    CAPTURE(key);
    std::string v = st.get_string(key);
    for (unsigned char c : v) CHECK((c >= 0x20 && c != 0x7F));
    CHECK(v.size() <= 64);
  }
  CHECK(st.get_string("long_name").find("Base") == 0);
}

}  // TEST_SUITE
