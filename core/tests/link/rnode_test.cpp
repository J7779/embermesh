// rnode driver: scripted RNode firmware emulator, byte-exact command vectors, handshake, flow control,
// errors, reboot, fuzz and an end-to-end relay between two drivers.
#include <algorithm>
#include <functional>

#include "doctest.h"
#include "framing/slip.h"
#include "link/lora_common.h"
#include "link_harness.h"

using namespace ember;
using namespace linktest;

namespace {

Bytes kiss(std::initializer_list<uint8_t> body) {
  Bytes b(body);
  Bytes out{0xC0};
  slip::escape_append(ByteView(b), out);
  out.push_back(0xC0);
  return out;
}

bool contains(const Bytes& hay, const Bytes& needle) {
  if (needle.size() > hay.size()) return false;
  for (size_t i = 0; i + needle.size() <= hay.size(); ++i)
    if (std::equal(needle.begin(), needle.end(), hay.begin() + static_cast<long>(i))) return true;
  return false;
}

// Minimal RNode firmware model: answers detect / version / platform / MCU, echoes configuration,
// transmits DATA frames onto `air`, sends READY after each transmission.
struct RNodeEmu {
  slip::Decoder dec{1024, true};
  Bytes outbox;
  std::vector<Bytes> air;
  bool alive = true;         // answers at all
  bool send_ready = true;    // flow-control READY after each packet
  int clamp_txp = -1;        // reports min(requested, clamp) like hardware with a lower maximum
  bool refuse_on = false;    // radio stays off
  uint32_t freq = 0, bw = 0;
  uint8_t sf = 0, cr = 0, txp = 0, state = 0;
  uint16_t st_alock = 0, lt_alock = 0;
  int configs = 0, detects = 0;

  void reply(const Bytes& body) {
    Bytes out{0xC0};
    slip::escape_append(ByteView(body), out);
    out.push_back(0xC0);
    outbox.insert(outbox.end(), out.begin(), out.end());
  }
  static Bytes u32(uint8_t c, uint32_t v) {
    return Bytes{c, static_cast<uint8_t>(v >> 24), static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v)};
  }
  void from_host(ByteView v) {
    dec.feed(v, [&](ByteView f) { handle(f); });
  }
  void handle(ByteView f) {
    if (!alive || f.empty()) return;
    uint8_t c = f[0];
    ByteView a = f.sub(1);
    switch (c) {
      case 0x08:
        if (a.size() == 1 && a[0] == 0x73) { ++detects; reply({0x08, 0x46}); }
        break;
      case 0x50: reply({0x50, 0x01, 0x4A}); break;  // firmware 1.74
      case 0x48: reply({0x48, 0x80}); break;        // ESP32
      case 0x49: reply({0x49, 0x81}); break;
      case 0x01:
        if (a.size() == 4) { freq = static_cast<uint32_t>(a[0]) << 24 | a[1] << 16 | a[2] << 8 | a[3]; reply(u32(0x01, freq)); ++configs; }
        break;
      case 0x02:
        if (a.size() == 4) { bw = static_cast<uint32_t>(a[0]) << 24 | a[1] << 16 | a[2] << 8 | a[3]; reply(u32(0x02, bw)); }
        break;
      case 0x03:
        if (a.size() == 1) { txp = clamp_txp >= 0 && a[0] > clamp_txp ? static_cast<uint8_t>(clamp_txp) : a[0]; reply({0x03, txp}); }
        break;
      case 0x04: if (a.size() == 1) { sf = a[0]; reply({0x04, sf}); } break;
      case 0x05: if (a.size() == 1) { cr = a[0]; reply({0x05, cr}); } break;
      case 0x0B: if (a.size() == 2) { st_alock = static_cast<uint16_t>(a[0] << 8 | a[1]); reply({0x0B, a[0], a[1]}); } break;
      case 0x0C: if (a.size() == 2) { lt_alock = static_cast<uint16_t>(a[0] << 8 | a[1]); reply({0x0C, a[0], a[1]}); } break;
      case 0x06:
        if (a.size() == 1) { state = refuse_on ? 0 : a[0]; reply({0x06, state}); }
        break;
      case 0x00:
        air.push_back(a.to_bytes());
        if (send_ready) reply({0x0F});
        break;
      default: break;
    }
  }
  // Something heard on air: RSSI + SNR reports followed by the packet (as the firmware does).
  void receive(const Bytes& pkt, uint8_t rssi_raw = 77, uint8_t snr_raw = 0xF6) {
    reply({0x23, rssi_raw});
    reply({0x24, snr_raw});
    Bytes b{0x00};
    b.insert(b.end(), pkt.begin(), pkt.end());
    reply(b);
  }
};

struct Rig {
  Clock clk;
  FakeHost host{&clk, 3};
  RNodeEmu emu;
  std::unique_ptr<Link> link;
  std::string err;
  Prng rng{9};
  size_t max_chunk = 0;

  explicit Rig(const std::string& cfg = "{}") {
    link = create_link("rnode", cfg, host, &err);
    host.on_write = [this](ByteView v) { emu.from_host(v); };
  }
  void deliver() {
    Bytes b;
    b.swap(emu.outbox);
    if (max_chunk == 0) { if (!b.empty()) link->on_io_bytes(ByteView(b)); return; }
    for (size_t off = 0; off < b.size();) {
      size_t n = 1 + rng.below(static_cast<uint32_t>(max_chunk));
      link->on_io_bytes(ByteView(b).sub(off, n));
      off += n;
    }
  }
  void step(uint32_t ms) {
    clk.t += ms;
    deliver();
    link->tick(clk.t);
    deliver();
  }
  bool run_until(const std::function<bool()>& p, uint32_t max_ms, uint32_t step_ms = 10) {
    for (uint32_t t = 0; t < max_ms; t += step_ms) {
      if (p()) return true;
      step(step_ms);
    }
    return p();
  }
  void run(uint32_t total, uint32_t step_ms) {
    for (uint32_t t = 0; t < total; t += step_ms) step(step_ms);
  }
  bool up(uint32_t max_ms = 3000) {
    return run_until([&] { return link->state() == LinkState::Up; }, max_ms);
  }
  Json status() const { return parse_json(link->status_json()); }
};

}  // namespace

TEST_SUITE("link") {

TEST_CASE("rnode: detect and configuration commands are byte-exact") {
  Rig r;
  REQUIRE(r.link);
  CHECK(r.link->state() == LinkState::Down);
  r.link->on_io_state(true);
  CHECK(r.link->state() == LinkState::Connecting);
  REQUIRE(r.host.writes.size() == 1);
  // Same burst as Reticulum's RNodeInterface.detect()
  CHECK((r.host.writes[0] == Bytes{0xC0, 0x08, 0x73, 0xC0, 0x50, 0x00, 0xC0, 0x48, 0x00, 0xC0, 0x49, 0x00, 0xC0}));
  REQUIRE(r.up());
  Bytes all = r.host.written();
  CHECK(contains(all, Bytes{0xC0, 0x01, 0x33, 0xD3, 0xE6, 0x08, 0xC0}));  // 869.525 MHz
  CHECK(contains(all, Bytes{0xC0, 0x02, 0x00, 0x01, 0xE8, 0x48, 0xC0}));  // 125 kHz
  CHECK(contains(all, Bytes{0xC0, 0x03, 0x0E, 0xC0}));                    // 14 dBm (EU default)
  CHECK(contains(all, Bytes{0xC0, 0x04, 0x09, 0xC0}));                    // SF9
  CHECK(contains(all, Bytes{0xC0, 0x05, 0x05, 0xC0}));                    // CR 4/5
  CHECK(contains(all, Bytes{0xC0, 0x0C, 0x03, 0xE8, 0xC0}));              // LT airtime lock 10.00 %
  CHECK(contains(all, Bytes{0xC0, 0x06, 0x01, 0xC0}));                    // radio on
  CHECK(!contains(all, Bytes{0xC0, 0x0B}));                               // no short-term lock by default
  CHECK(r.emu.lt_alock == 1000);
  Json st = r.status();
  CHECK(st.get("device").get_string("firmware") == "1.74");
  CHECK(st.get("device").get_string("platform") == "esp32");
  CHECK(st.get_string("phase") == "running");
}

TEST_CASE("rnode: escaped frequency bytes and short-term airtime lock") {
  // 868276992 = 0x33C0DB00 contains both FEND and FESC
  Rig r("{\"frequency_hz\":868276992,\"st_alock_pct\":25.5,\"sf\":7,\"bandwidth_hz\":\"250000\",\"cr\":8,\"tx_power_dbm\":2}");
  REQUIRE(r.link);
  r.link->on_io_state(true);
  REQUIRE(r.up());
  Bytes all = r.host.written();
  CHECK(contains(all, Bytes{0xC0, 0x01, 0x33, 0xDB, 0xDC, 0xDB, 0xDD, 0x00, 0xC0}));
  CHECK(contains(all, Bytes{0xC0, 0x0B, 0x09, 0xF6, 0xC0}));  // 2550
  CHECK(r.emu.freq == 868276992u);
  CHECK(r.emu.bw == 250000u);
  CHECK(r.emu.sf == 7);
  CHECK(r.emu.cr == 8);
  CHECK(r.emu.txp == 2);
}

TEST_CASE("rnode: caps follow configuration") {
  Rig r;
  LinkCaps c = r.link->caps();
  CHECK(c.mtu == 254);
  CHECK(c.bitrate_bps == 1757);  // SF9 / 125 kHz / 4:5, same formula as Reticulum
  CHECK(c.half_duplex);
  CHECK(c.broadcast);
  CHECK(!c.reliable);
  CHECK(c.duty_cycle_permille == 100);
  CHECK(c.encryption_allowed);
  CHECK(!c.requires_station_id);
  CHECK(c.cost == 20);
  Rig us("{\"region\":\"us915\",\"amateur\":true,\"mtu\":508,\"sf\":12}");
  REQUIRE(us.link);
  c = us.link->caps();
  CHECK(c.duty_cycle_permille == 1000);
  CHECK(!c.encryption_allowed);
  CHECK(c.requires_station_id);
  CHECK(c.mtu == 508);
  CHECK(c.bitrate_bps == 292);
  us.link->on_io_state(true);
  REQUIRE(us.up());
  CHECK(us.emu.freq == 914875000u);
  CHECK(us.emu.lt_alock == 0);
  CHECK(link::lora_airtime_ms(20, 7, 125000, 5) == 57);
  CHECK(link::lora_airtime_ms(10, 12, 125000, 5) == 992);
}

TEST_CASE("rnode: config validation") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  CHECK(!create_link("rnode", "{\"frequency_hz\":100000000,\"region\":\"custom\"}", h, &err));
  CHECK(!create_link("rnode", "{\"frequency_hz\":915000000}", h, &err));  // outside eu868
  CHECK(err.find("region") != std::string::npos);
  CHECK(create_link("rnode", "{\"frequency_hz\":915000000,\"region\":\"custom\"}", h, &err));
  CHECK(!create_link("rnode", "{\"sf\":13}", h, &err));
  CHECK(!create_link("rnode", "{\"bandwidth_hz\":\"100000\"}", h, &err));
  CHECK(!create_link("rnode", "{\"mtu\":509}", h, &err));
}

TEST_CASE("rnode: no answer -> retries -> error -> recovers later") {
  Rig r;
  r.emu.alive = false;
  r.link->on_io_state(true);
  r.run_until([&] { return r.link->state() == LinkState::Error; }, 10000);
  CHECK(r.link->state() == LinkState::Error);
  size_t detect_writes = 0;
  for (const auto& w : r.host.writes) detect_writes += w.size() > 2 && w[1] == 0x08;
  CHECK(detect_writes == 3);
  CHECK(r.status().get_string("last_error").find("no RNode") != std::string::npos);
  CHECK(r.link->send(ByteView(Bytes(10, 1)), 3));  // queued while in error is allowed
  r.emu.alive = true;
  REQUIRE(r.up(20000));
  REQUIRE(r.run_until([&] { return r.emu.air.size() == 1; }, 1000));
}

TEST_CASE("rnode: rejected settings put the link in error") {
  Rig r("{\"tx_power_dbm\":22}");
  r.emu.clamp_txp = 17;
  r.link->on_io_state(true);
  r.run_until([&] { return r.link->state() == LinkState::Error; }, 3000);
  CHECK(r.link->state() == LinkState::Error);
  CHECK(r.status().get_string("last_error").find("TX power") != std::string::npos);
  Rig off;
  off.emu.refuse_on = true;
  off.link->on_io_state(true);
  off.run_until([&] { return off.link->state() == LinkState::Error; }, 3000);
  CHECK(off.status().get_string("last_error").find("radio state") != std::string::npos);
}

TEST_CASE("rnode: frames of every size, mtu+1 refused, flow control") {
  Rig r;
  r.link->on_io_state(true);
  REQUIRE(r.up());
  Prng p(4);
  for (size_t n = 1; n <= 254; ++n) {
    Bytes f = p.bytes(n);
    REQUIRE(r.link->send(ByteView(f), 4));
    REQUIRE(r.run_until([&] { return r.emu.air.size() == n; }, 5000));
    CHECK(r.emu.air.back() == f);
  }
  CHECK(!r.link->send(ByteView(Bytes(255, 0)), 4));
  CHECK(!r.link->send(ByteView(Bytes()), 4));
  // One packet in flight: two queued frames, the second only after READY.
  r.emu.send_ready = false;
  size_t before = r.emu.air.size();
  r.link->send(ByteView(Bytes(50, 1)), 4);
  r.link->send(ByteView(Bytes(50, 2)), 4);
  r.step(1);
  CHECK(r.emu.air.size() == before + 1);
  CHECK(r.link->channel_busy());
  // ... without READY the airtime fallback releases it.
  REQUIRE(r.run_until([&] { return r.emu.air.size() == before + 2; }, 3000));
  CHECK(r.status().get_int("tx_frames") == 256);
}

TEST_CASE("rnode: received packets carry RSSI and SNR; status reports") {
  Rig r;
  r.max_chunk = 3;
  r.link->on_io_state(true);
  REQUIRE(r.up());
  Prng p(8);
  for (size_t n = 1; n <= 508; n += 13) {
    Bytes f = p.bytes(n);
    r.emu.receive(f);
    r.step(10);
    REQUIRE(!r.host.frames.empty());
    CHECK(r.host.frames.back().data == f);
  }
  const auto& m = r.host.frames.back().meta;
  CHECK(m.has_rssi);
  CHECK(m.rssi_dbm == doctest::Approx(-80));
  CHECK(m.has_snr);
  CHECK(m.snr_db == doctest::Approx(-2.5));
  r.emu.reply({0x25, 0x00, 0x64, 0x00, 0x32, 0x01, 0x2C, 0x00, 0xC8, 60, 40, 0xFF});
  r.emu.reply({0x27, 0x01, 87});
  r.emu.reply({0x21, 0, 0, 1, 2});
  r.emu.reply({0x29, 145});
  r.step(10);
  Json st = r.status();
  CHECK(st.get_double("airtime_short_pct") == doctest::Approx(1.0));
  CHECK(st.get_double("channel_load_short_pct") == doctest::Approx(3.0));
  CHECK(st.get_int("noise_floor_dbm") == 40 - 157);
  CHECK(st.get_int("battery_pct") == 87);
  CHECK(st.get_int("device_rx_packets") == 258);
  CHECK(st.get_int("temperature_c") == 25);
  CHECK(st.get_int("last_rssi_dbm") == -80);
}

TEST_CASE("rnode: device errors and reboot mid-session") {
  Rig r;
  r.link->on_io_state(true);
  REQUIRE(r.up());
  r.emu.send_ready = false;
  r.link->send(ByteView(Bytes(20, 1)), 4);
  r.link->send(ByteView(Bytes(20, 2)), 4);
  r.step(1);
  CHECK(r.emu.air.size() == 1);
  r.emu.reply({0x90, 0x02});  // TX failed: releases the in-flight slot immediately
  r.step(1);
  CHECK(r.emu.air.size() == 2);
  CHECK(r.status().get_int("tx_failed") == 1);
  CHECK(r.status().get_string("last_error") == "transmit failed");
  CHECK(r.link->state() == LinkState::Up);
  // reboot announcement -> handshake again
  int detects = r.emu.detects;
  r.emu.reply({0x55, 0xF8});
  r.step(1);
  CHECK(r.link->state() != LinkState::Up);
  REQUIRE(r.up());
  CHECK(r.emu.detects == detects + 1);
  // radio switched off underneath -> reconfigure
  int configs = r.emu.configs;
  r.emu.reply({0x06, 0x00});
  r.deliver();
  CHECK(r.link->state() == LinkState::Connecting);
  REQUIRE(r.up());
  CHECK(r.emu.configs == configs + 1);
  // fatal radio init error
  r.emu.reply({0x90, 0x01});
  r.step(1);
  CHECK(r.link->state() == LinkState::Error);
  REQUIRE(r.up(20000));  // retried after the idle period
  // endpoint closes
  r.link->on_io_state(false);
  CHECK(r.link->state() == LinkState::Down);
}

TEST_CASE("rnode: priority order and queue limits") {
  Rig r("{\"queue_frames\":4}");
  r.link->on_io_state(true);
  // queued while still connecting
  CHECK(r.link->send(ByteView(Bytes{7}), 7));
  CHECK(r.link->send(ByteView(Bytes{5}), 5));
  CHECK(r.link->send(ByteView(Bytes{1}), 1));
  CHECK(r.link->send(ByteView(Bytes{6}), 6));
  CHECK(!r.link->send(ByteView(Bytes{8}), 7));  // full
  CHECK(r.link->send(ByteView(Bytes{0}), 0));   // evicts the lowest priority
  CHECK(r.link->tx_queue_depth() == 4);
  REQUIRE(r.up());
  REQUIRE(r.run_until([&] { return r.emu.air.size() == 4; }, 5000));
  CHECK(r.emu.air[0] == Bytes{0});
  CHECK(r.emu.air[1] == Bytes{1});
  CHECK(r.emu.air[2] == Bytes{5});
  CHECK(r.emu.air[3] == Bytes{6});
}

TEST_CASE("rnode: duty cycle limit holds back transmissions") {
  Rig r("{\"region\":\"eu868_1pct\",\"sf\":12}");
  r.link->on_io_state(true);
  REQUIRE(r.up());
  // 1 % of an hour = 36 s; each 200-byte SF12 packet is ~ 7 s on air.
  for (int i = 0; i < 10; ++i) r.link->send(ByteView(Bytes(200, static_cast<uint8_t>(i))), 4);
  r.run(60000, 100);
  CHECK(r.emu.air.size() >= 4);
  CHECK(r.emu.air.size() <= 6);
  CHECK(r.link->tx_queue_depth() > 0);
}

TEST_CASE("rnode: reconfigure re-sends radio parameters when connected") {
  Rig r;
  r.link->on_io_state(true);
  REQUIRE(r.up());
  std::string err;
  CHECK(!r.link->reconfigure("{\"sf\":99}", &err));
  CHECK(r.link->state() == LinkState::Up);
  CHECK(r.link->reconfigure("{\"sf\":11}", &err));
  CHECK(r.link->state() == LinkState::Connecting);
  REQUIRE(r.up());
  CHECK(r.emu.sf == 11);
  CHECK(r.link->caps().bitrate_bps == link::lora_bitrate(11, 125000, 5));
  int configs = r.emu.configs;
  CHECK(r.link->reconfigure("{\"sf\":11,\"cost\":30}", &err));  // no radio change: stays up
  CHECK(r.link->state() == LinkState::Up);
  CHECK(r.emu.configs == configs);
  CHECK(r.link->caps().cost == 30);
}

TEST_CASE("rnode: garbage and truncated input fuzz") {
  Prng p(1234);
  for (int round = 0; round < 30; ++round) {
    Rig r;
    r.link->on_io_state(true);
    for (int i = 0; i < 200; ++i) {
      uint32_t kind = p.below(4);
      Bytes b;
      if (kind == 0) b = p.bytes(p.below(64));
      else {
        Bytes body = p.bytes(p.below(kind == 3 ? 600 : 16));
        if (!body.empty() && p.below(2)) body[0] = static_cast<uint8_t>(p.below(0x30));
        b.push_back(0xC0);
        slip::escape_append(ByteView(body), b);
        if (p.below(3)) b.push_back(0xC0);
        if (p.below(4) == 0 && b.size() > 2) b.resize(p.below(static_cast<uint32_t>(b.size())));
      }
      r.link->on_io_bytes(ByteView(b));
      if (p.below(5) == 0) r.link->send(ByteView(p.bytes(1 + p.below(254))), static_cast<uint8_t>(p.below(8)));
      r.step(p.below(50));
    }
    Json st;
    CHECK(Json::parse(r.link->status_json(), st));
  }
}

TEST_CASE("rnode: end to end between two drivers over emulated radios") {
  Rig a, b;
  a.max_chunk = 7;
  b.max_chunk = 1;
  a.link->on_io_state(true);
  b.link->on_io_state(true);
  REQUIRE(a.up());
  REQUIRE(b.up());
  Prng p(77);
  size_t sent = 0;
  for (size_t n = 1; n <= 254; n += 3) {
    Bytes f = p.bytes(n);
    REQUIRE(a.link->send(ByteView(f), 3));
    ++sent;
    for (int i = 0; i < 300 && a.emu.air.size() < sent; ++i) { a.step(10); b.step(0); }
    REQUIRE(a.emu.air.size() == sent);
    b.emu.receive(a.emu.air.back(), 100, 20);
    b.step(5);
    REQUIRE(b.host.frames.size() == sent);
    CHECK(b.host.frames.back().data == f);
    CHECK(b.host.frames.back().meta.snr_db == doctest::Approx(5.0));
  }
}

}  // TEST_SUITE
