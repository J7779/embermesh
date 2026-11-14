// audio + rtl_tcp drivers through the dsp channel simulator: different TX/RX sample rates, noise,
// CSMA/PTT sequencing, AX.25 interop with the kiss driver, keyboard modes, IQ for the SDR receiver.
#include <cmath>

#include "doctest.h"
#include "dsp/channel_sim.h"
#include "dsp/resampler.h"
#include "framing/kiss.h"
#include "link_harness.h"
#include "modem/modem.h"

using namespace ember;
using namespace linktest;

namespace {

// Pulls one complete transmission from `l` at `rate`. Checks the PTT/sample contract on the way.
std::vector<float> transmit(Link& l, FakeHost& h, uint32_t rate, uint32_t max_ms = 120000) {
  std::vector<float> out;
  const size_t kBlock = rate / 100;  // 10 ms
  std::vector<int16_t> buf(kBlock);
  size_t on0 = h.count_events("ptt");
  bool started = false;
  for (uint32_t t = 0; t < max_ms; t += 10) {
    h.clk->t += 10;
    l.tick(h.clk->t);
    size_t n = l.pull_audio(buf.data(), kBlock, rate);
    if (n) {
      if (!started) {
        // ptt on was signalled before the first sample
        REQUIRE(h.count_events("ptt") >= on0);
        REQUIRE(!h.events.empty());
        CHECK(h.events.back().first == "ptt");
        CHECK(h.events.back().second == "{\"on\":true}");
        CHECK(l.channel_busy());
        started = true;
      }
      for (size_t i = 0; i < n; ++i) out.push_back(buf[i] / 32768.0f);
      if (n < kBlock) break;  // last block of the transmission
      // ptt still on while samples flow
      CHECK(h.events.back().second == "{\"on\":true}");
    } else if (started) {
      break;
    }
    if (started && h.events.back().second == "{\"on\":false}") break;
  }
  if (started) {
    CHECK(h.events.back().first == "ptt");
    CHECK(h.events.back().second == "{\"on\":false}");
    CHECK(l.pull_audio(buf.data(), kBlock, rate) == 0);  // idle afterwards
  }
  return out;
}

// Channel + resampling from tx_rate to rx_rate; delivers to `rx` in 7 ms-ish chunks.
void receive(Link& rx, FakeHost& h, const std::vector<float>& tx, uint32_t tx_rate, uint32_t rx_rate,
             double snr_db, uint64_t seed = 1) {
  dsp::ChannelParams p;
  p.seed = seed;
  p.snr_db = snr_db;
  p.quantize_pcm16 = true;
  p.clip_level = 0.999;
  dsp::ChannelSim ch(p, tx_rate);
  std::vector<float> y = ch.run(tx, tx_rate / 5, tx_rate / 2);
  std::vector<float> z;
  dsp::Resampler rs;
  REQUIRE(rs.init(tx_rate, rx_rate));
  rs.process(y.data(), y.size(), z);
  rs.flush(z);
  std::vector<int16_t> pcm(z.size());
  for (size_t i = 0; i < z.size(); ++i) {
    float v = z[i] * 32767.0f;
    pcm[i] = static_cast<int16_t>(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
  }
  size_t chunk = rx_rate / 143 + 1;
  for (size_t off = 0; off < pcm.size(); off += chunk) {
    size_t n = pcm.size() - off < chunk ? pcm.size() - off : chunk;
    rx.on_audio(pcm.data() + off, n, rx_rate);
    h.clk->t += 1000 * n / rx_rate;
    rx.tick(h.clk->t);
  }
}

Bytes pattern(size_t n, uint8_t tag) {
  Bytes b(n);
  for (size_t i = 0; i < n; ++i) b[i] = static_cast<uint8_t>(tag + i * 29);
  return b;
}

const char* kAfskA = R"({"modem":"afsk1200","callsign":"QX0XYZ-1","persistence":255,"txdelay_ms":200})";
const char* kAfskB = R"({"modem":"afsk1200","callsign":"QX0XYZ-2","persistence":255,"txdelay_ms":200})";

}  // namespace

TEST_SUITE("link") {

TEST_CASE("audio: AFSK1200 frames across sample rates, PTT sequencing, mtu limits") {
  struct Rates { uint32_t tx, rx; double snr; };
  const Rates rates[] = {{48000, 48000, 200}, {44100, 8000, 30}, {8000, 22050, 25}, {16000, 44100, 20}};
  for (const Rates& r : rates) {
    CAPTURE(r.tx);
    CAPTURE(r.rx);
    Clock clk;
    FakeHost ha(&clk, 1), hb(&clk, 2);
    std::string err;
    auto a = create_link("audio", kAfskA, ha, &err);
    auto b = create_link("audio", kAfskB, hb, &err);
    REQUIRE(a);
    REQUIRE(b);
    a->on_io_state(true);
    b->on_io_state(true);
    LinkCaps c = a->caps();
    CHECK(c.mtu == 256);
    CHECK(c.bitrate_bps == 1200);
    CHECK(c.half_duplex);
    CHECK(!c.encryption_allowed);
    CHECK(c.requires_station_id);
    CHECK(!a->send(ByteView(pattern(257, 1)), 0));
    std::vector<size_t> sizes = {1, 2, 31, 100, 255, 256};
    for (size_t i = 0; i < sizes.size(); ++i) REQUIRE(a->send(ByteView(pattern(sizes[i], static_cast<uint8_t>(i))), 3));
    std::vector<float> all;
    while (a->tx_queue_depth() > 0) {
      std::vector<float> tx = transmit(*a, ha, r.tx);
      REQUIRE(!tx.empty());
      all.insert(all.end(), tx.begin(), tx.end());
      all.insert(all.end(), r.tx / 10, 0.0f);
    }
    receive(*b, hb, all, r.tx, r.rx, r.snr);
    REQUIRE(hb.frames.size() == sizes.size());
    for (size_t i = 0; i < sizes.size(); ++i) {
      CHECK(hb.frames[i].data == pattern(sizes[i], static_cast<uint8_t>(i)));
      CHECK(hb.frames[i].meta.has_snr);
    }
    Json st;
    REQUIRE(Json::parse(b->status_json(), st));
    CHECK(st.get("modem_status").get_int("rx_rate") == r.rx);
    CHECK(st.get("modem_status").get_string("level_hint") != "no_audio");
    REQUIRE(Json::parse(a->status_json(), st));
    CHECK(st.get_int("tx_frames") == 6);
    CHECK(st.get_int("airtime_ms") > 0);
  }
}

TEST_CASE("audio: AFSK1200 is byte-compatible with the kiss driver (AX.25 UI)") {
  Clock clk;
  FakeHost ha(&clk, 1), hk(&clk, 3);
  std::string err;
  auto a = create_link("audio", kAfskA, ha, &err);
  auto k = create_link("kiss", R"({"callsign":"QX0XYZ-1","set_params":false})", hk, &err);
  REQUIRE(a);
  REQUIRE(k);
  a->on_io_state(true);
  k->on_io_state(true);
  Bytes f = pattern(40, 9);
  REQUIRE(a->send(ByteView(f), 1));
  REQUIRE(k->send(ByteView(f), 1));
  std::vector<float> tx = transmit(*a, ha, 12000);
  // demodulate with a stock afsk1200 modem
  modem::ModemOptions o;
  auto m = modem::create_modem("afsk1200", 12000, o, &err);
  REQUIRE(m);
  m->rx(tx.data(), tx.size());
  std::vector<float> pad(6000, 0.0f);
  m->rx(pad.data(), pad.size());
  modem::RxItem it;
  REQUIRE(m->pop(&it));
  kiss::Decoder d;
  Bytes w = hk.written();
  Bytes kiss_payload;
  d.feed(ByteView(w), [&](const kiss::Frame& fr) { kiss_payload = fr.payload.to_bytes(); });
  CHECK(it.data == kiss_payload);
  // and a frame from a KISS station's TNC is understood by the audio driver
  FakeHost hb(&clk, 2);
  auto b = create_link("audio", kAfskB, hb, &err);
  REQUIRE(b);
  b->on_io_state(true);
  receive(*b, hb, tx, 12000, 48000, 40);
  REQUIRE(hb.frames.size() == 1);
  CHECK(hb.frames[0].data == f);
}

TEST_CASE("audio: CSMA defers while the channel is busy, VOX tone, txdelay") {
  Clock clk;
  FakeHost ha(&clk, 1), hb(&clk, 2);
  std::string err;
  auto a = create_link("audio", kAfskA, ha, &err);
  auto b = create_link("audio",
                       R"({"modem":"afsk1200","callsign":"QX0XYZ-2","persistence":255,"vox_tone_ms":200,)"
                       R"("vox_tone_hz":1000,"txdelay_ms":100})",
                       hb, &err);
  REQUIRE(a);
  REQUIRE(b);
  a->on_io_state(true);
  b->on_io_state(true);
  REQUIRE(a->send(ByteView(pattern(200, 1)), 2));
  std::vector<float> tx = transmit(*a, ha, 8000);
  REQUIRE(tx.size() > 8000);
  // B has traffic queued while A's signal is on the air: B must not key up during it.
  std::vector<int16_t> pcm(tx.size());
  for (size_t i = 0; i < tx.size(); ++i) pcm[i] = static_cast<int16_t>(tx[i] * 30000);
  bool keyed_during = false;
  bool saw_busy = false;
  std::vector<int16_t> out(80);
  for (size_t off = 0; off + 80 <= pcm.size(); off += 80) {
    b->on_audio(pcm.data() + off, 80, 8000);
    clk.t += 10;
    b->tick(clk.t);
    if (off == 8000) REQUIRE(b->send(ByteView(pattern(50, 2)), 2));
    if (off > 8000 && off + 1600 < pcm.size()) {
      saw_busy |= b->channel_busy();
      if (b->pull_audio(out.data(), 80, 8000) > 0) keyed_during = true;
    }
  }
  CHECK(saw_busy);
  CHECK(!keyed_during);
  CHECK(hb.frames.size() == 1);
  // channel clear now: B transmits; the first 200 ms are the VOX tone (~1000 Hz, steady level)
  std::vector<float> btx = transmit(*b, hb, 8000);
  REQUIRE(btx.size() > 1600);
  int crossings = 0;
  for (size_t i = 1; i < 1600; ++i) crossings += (btx[i - 1] < 0) != (btx[i] < 0);
  CHECK(crossings > 360);
  CHECK(crossings < 440);
  Json st;
  REQUIRE(Json::parse(b->status_json(), st));
  CHECK(st.get_int("csma_deferrals") > 0);
  CHECK(!b->channel_busy());
}

TEST_CASE("audio: p-persistence defers randomly; no audio output aborts PTT") {
  Clock clk;
  FakeHost h(&clk, 5);
  std::string err;
  auto a = create_link("audio", R"({"modem":"afsk1200","callsign":"QX0XYZ-1","persistence":1})", h, &err);
  REQUIRE(a);
  a->on_io_state(true);
  REQUIRE(a->send(ByteView(pattern(10, 1)), 2));
  clk.t += 10;
  a->tick(clk.t);
  int tries = 0;
  while (h.count_events("ptt") == 0 && tries < 100000) {
    clk.t += 100;
    a->tick(clk.t);
    ++tries;
  }
  CHECK(tries > 5);  // p = 1/255 per slot: practically never immediate
  REQUIRE(h.count_events("ptt") == 1);
  // nobody pulls audio: PTT released after the timeout
  clk.t += 2500;
  a->tick(clk.t);
  REQUIRE(h.count_events("ptt") == 2);
  CHECK(h.events.back().second == "{\"on\":false}");
  Json st;
  REQUIRE(Json::parse(a->status_json(), st));
  CHECK(st.get_string("last_error").find("no audio") != std::string::npos);
}

TEST_CASE("audio: robust modem with noise, licence-free raw framing, different rates") {
  Clock clk;
  FakeHost ha(&clk, 1), hb(&clk, 2);
  std::string err;
  const char* cfg = R"({"modem":"robust250","licence":"licence_free","persistence":255,"txdelay_ms":100})";
  auto a = create_link("audio", cfg, ha, &err);
  auto b = create_link("audio", cfg, hb, &err);
  REQUIRE(a);
  REQUIRE(b);
  CHECK(a->caps().encryption_allowed);
  CHECK(a->caps().mtu == 128);
  CHECK(a->caps().bitrate_bps == 250);
  a->on_io_state(true);
  b->on_io_state(true);
  Bytes f = pattern(48, 3);
  REQUIRE(a->send(ByteView(f), 0));
  std::vector<float> tx = transmit(*a, ha, 44100);
  receive(*b, hb, tx, 44100, 16000, 8, 3);
  REQUIRE(hb.frames.size() == 1);
  CHECK(hb.frames[0].data == f);
  Json st;
  REQUIRE(Json::parse(b->status_json(), st));
  CHECK(st.get("modem_status").get_string("framing") == "raw");
}

TEST_CASE("audio: keyboard modes carry interop text both ways") {
  for (const char* m : {"cw", "rtty", "psk31"}) {
    CAPTURE(m);
    Clock clk;
    FakeHost ha(&clk, 1), hb(&clk, 2);
    std::string err;
    std::string cfg = std::string(R"({"persistence":255,"modem":")") + m + "\"}";
    auto a = create_link("audio", cfg, ha, &err);
    auto b = create_link("audio", cfg, hb, &err);
    REQUIRE(a);
    REQUIRE(b);
    CHECK(a->caps().mtu == 0);
    a->on_io_state(true);
    b->on_io_state(true);
    CHECK(!a->send(ByteView(pattern(10, 1)), 0));
    InteropText t;
    t.text = "CQ TEST DE QX0XYZ";
    REQUIRE(a->send_interop_text(t));
    std::vector<float> tx = transmit(*a, ha, 8000, 300000);
    REQUIRE(!tx.empty());
    receive(*b, hb, tx, 8000, 11025, 30);
    clk.t += 5000;
    b->tick(clk.t);
    REQUIRE(!hb.texts.empty());
    std::string got;
    for (const auto& x : hb.texts) got += x.text + " ";
    CHECK(got.find("TEST") != std::string::npos);
    CHECK(hb.texts[0].network == m);
  }
}

TEST_CASE("audio: hostile audio input and bad rates") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto a = create_link("audio", kAfskA, h, &err);
  REQUIRE(a);
  Prng r(9);
  std::vector<int16_t> pcm(4000);
  for (int i = 0; i < 20; ++i) {
    for (auto& s : pcm) s = static_cast<int16_t>(r.next());
    a->on_audio(pcm.data(), pcm.size(), 8000 + r.below(40000));
  }
  a->on_audio(pcm.data(), pcm.size(), 1000);  // unsupported rate: ignored
  a->on_audio(nullptr, 10, 8000);
  std::vector<int16_t> out(100);
  CHECK(a->pull_audio(out.data(), 100, 4000) == 0);
  CHECK(h.frames.empty());
  Json st;
  REQUIRE(Json::parse(a->status_json(), st));
  CHECK(st.get("modem_status").get_string("level_hint") == "clipping");
  CHECK(!create_link("audio", R"({"modem":"afsk1200","modem_params":"bogus"})", h, &err));
  CHECK(!create_link("audio", R"({"modem":"afsk1200","modem_params":"nosuchkey=1"})", h, &err));
  CHECK(create_link("audio", R"({"modem":"cw","modem_params":"wpm=15"})", h, &err));
  CHECK(!create_link("audio", R"({"modem":"afsk1200","mtu":3000})", h, &err));
  REQUIRE(a->reconfigure(R"({"modem":"afsk300","callsign":"QX0XYZ-1"})", &err));
  CHECK(a->caps().bitrate_bps == 300);
}

TEST_CASE("rtl_tcp: header, tuner commands and decoding a frame from IQ") {
  Clock clk;
  FakeHost hs(&clk, 1), hr(&clk, 2);
  std::string err;
  // Transmit side: an audio link generates AFSK1200 at 24 kHz
  auto a = create_link("audio", kAfskA, hs, &err);
  REQUIRE(a);
  a->on_io_state(true);
  Bytes f = pattern(60, 4);
  REQUIRE(a->send(ByteView(f), 0));
  InteropText t;
  t.to = "QX0XYZ-2";
  t.text = "hello sdr";
  REQUIRE(a->send_interop_text(t));
  std::vector<float> audio = transmit(*a, hs, 24000);
  REQUIRE(!audio.empty());
  std::vector<float> second = transmit(*a, hs, 24000);  // the APRS message went in a second key-up
  REQUIRE(!second.empty());
  audio.insert(audio.end(), 2400, 0.0f);
  audio.insert(audio.end(), second.begin(), second.end());
  // FM-modulate onto a carrier 50 kHz above the tuned centre, 240 kS/s, 3 kHz deviation, u8 IQ
  const double fs = 240000, off = 50000, dev = 3000;
  dsp::Resampler up;
  REQUIRE(up.init(24000, fs));
  std::vector<float> a240;
  std::vector<float> padded(2400, 0.0f);
  padded.insert(padded.end(), audio.begin(), audio.end());
  padded.insert(padded.end(), 4800, 0.0f);
  up.process(padded.data(), padded.size(), a240);
  up.flush(a240);
  Bytes iq;
  iq.reserve(a240.size() * 2);
  double ph = 0;
  Prng r(4);
  for (float s : a240) {
    ph += 2 * 3.14159265358979 * (off + dev * s / 0.5) / fs;  // tx_level 0.5 = full deviation
    double i = 0.7 * std::cos(ph) + (r.unit() - 0.5) * 0.05, q = 0.7 * std::sin(ph) + (r.unit() - 0.5) * 0.05;
    iq.push_back(static_cast<uint8_t>(127.5 + 127 * i));
    iq.push_back(static_cast<uint8_t>(127.5 + 127 * q));
  }
  auto l = create_link("rtl_tcp", R"({"frequency_hz":144390000,"gain_db":40.2,"callsign":"QX0XYZ-2"})", hr, &err);
  REQUIRE(l);
  CHECK(l->caps().mtu == 0);
  CHECK(!l->send(ByteView(f), 0));
  l->on_io_state(true);
  CHECK(l->state() == LinkState::Connecting);
  Bytes cmds = hr.written();
  const Bytes expect = {0x02, 0x00, 0x03, 0xA9, 0x80,   // 240000 S/s
                        0x01, 0x08, 0x9A, 0x74, 0x20,   // 144340000 Hz (50 kHz below)
                        0x05, 0x00, 0x00, 0x00, 0x00,   // 0 ppm
                        0x03, 0x00, 0x00, 0x00, 0x01,   // manual gain
                        0x04, 0x00, 0x00, 0x01, 0x92,   // 40.2 dB
                        0x08, 0x00, 0x00, 0x00, 0x00};  // RTL AGC off
  CHECK(cmds == expect);
  Bytes hdr = {'R', 'T', 'L', '0', 0, 0, 0, 5, 0, 0, 0, 29};
  l->on_io_bytes(ByteView(hdr).sub(0, 5));
  CHECK(l->state() == LinkState::Connecting);
  Bytes rest = ByteView(hdr).sub(5).to_bytes();
  rest.insert(rest.end(), iq.begin(), iq.begin() + 1001);  // odd split inside an IQ pair
  l->on_io_bytes(ByteView(rest));
  CHECK(l->state() == LinkState::Up);
  for (size_t off2 = 1001; off2 < iq.size(); off2 += 30000) l->on_io_bytes(ByteView(iq).sub(off2, 30000));
  REQUIRE(hr.frames.size() == 1);
  CHECK(hr.frames[0].data == f);
  REQUIRE(hr.texts.size() == 1);
  CHECK(hr.texts[0].text == "hello sdr");
  CHECK(hr.writes.size() == 1);  // receive-only: never acks or transmits
  Json st;
  REQUIRE(Json::parse(l->status_json(), st));
  CHECK(st.get_string("tuner") == "R820T");
  // meter fields (docs/RPC_API.md "Audio link status fields")
  CHECK(st.get_int("frames_ok") >= 1);
  CHECK(st.get("tx_active").is_bool());
  CHECK(!st.get_bool("tx_active"));
  CHECK(st.get("rx_level_dbfs").is_number());
  CHECK(st.get_double("rx_level_dbfs") > -120.0);
  CHECK(st.get("level_hint").is_string());
  // not an rtl_tcp server
  FakeHost hx(&clk, 3);
  auto x = create_link("rtl_tcp", "{}", hx, &err);
  REQUIRE(x);
  x->on_io_state(true);
  x->on_io_bytes(ByteView(std::string_view("HTTP/1.1 400 Bad")));
  CHECK(x->state() == LinkState::Error);
  CHECK(!create_link("rtl_tcp", R"({"tune_offset_hz":200000})", hx, &err));
}

namespace {
// AFSK1200 mesh frame `f`, FM-modulated 50 kHz above centre at 240 kS/s as rtl_tcp u8 IQ.
Bytes rtl_iq_for(const Bytes& f) {
  Clock clk;
  FakeHost hs(&clk, 1);
  std::string err;
  auto a = create_link("audio", kAfskA, hs, &err);
  REQUIRE(a);
  a->on_io_state(true);
  REQUIRE(a->send(ByteView(f), 0));
  std::vector<float> audio = transmit(*a, hs, 24000);
  REQUIRE(!audio.empty());
  const double fs = 240000, off = 50000, dev = 3000;
  dsp::Resampler up;
  REQUIRE(up.init(24000, fs));
  std::vector<float> a240, padded(2400, 0.0f);
  padded.insert(padded.end(), audio.begin(), audio.end());
  padded.insert(padded.end(), 4800, 0.0f);
  up.process(padded.data(), padded.size(), a240);
  up.flush(a240);
  Bytes iq;
  double ph = 0;
  Prng r(4);
  for (float s : a240) {
    ph += 2 * 3.14159265358979 * (off + dev * s / 0.5) / fs;
    double i = 0.7 * std::cos(ph) + (r.unit() - 0.5) * 0.05, q = 0.7 * std::sin(ph) + (r.unit() - 0.5) * 0.05;
    iq.push_back(static_cast<uint8_t>(127.5 + 127 * i));
    iq.push_back(static_cast<uint8_t>(127.5 + 127 * q));
  }
  return iq;
}
}  // namespace

TEST_CASE("rtl_tcp regression: missing, split or late header, and self-retry from Error (L9)") {
  Bytes f = pattern(60, 9);
  Bytes iq = rtl_iq_for(f);
  const Bytes hdr = {'R', 'T', 'L', '0', 0, 0, 0, 5, 0, 0, 0, 29};
  std::string err;
  // 1. Header split byte by byte, then IQ.
  {
    Clock clk;
    FakeHost h(&clk, 2);
    auto l = create_link("rtl_tcp", "{}", h, &err);
    REQUIRE(l);
    l->on_io_state(true);
    for (uint8_t b : hdr) { CHECK(l->state() == LinkState::Connecting); l->on_io_bytes(ByteView(&b, 1)); }
    CHECK(l->state() == LinkState::Up);
    for (size_t o = 0; o < iq.size(); o += 7777) l->on_io_bytes(ByteView(iq).sub(o, 7777));
    CHECK(h.frames.size() == 1);
    CHECK(parse_json(l->status_json()).get_string("tuner") == "R820T");
  }
  // 2. No header at all (endpoint strips it / connected mid-stream): Up after a short wait, decodes.
  {
    Clock clk;
    FakeHost h(&clk, 2);
    auto l = create_link("rtl_tcp", "{}", h, &err);
    REQUIRE(l);
    l->on_io_state(true);
    size_t o = 0;
    for (int k = 0; k < 400 && o < iq.size(); ++k) {
      l->on_io_bytes(ByteView(iq).sub(o, 1200));  // 1200 bytes per 5 ms = 240 kS/s
      o += 1200;
      clk.t += 5;
      l->tick(clk.t);
    }
    CHECK(l->state() == LinkState::Up);
    for (; o < iq.size(); o += 30000) l->on_io_bytes(ByteView(iq).sub(o, 30000));
    clk.t += 5000;
    l->tick(clk.t);
    CHECK(h.frames.size() == 1);
    Json st = parse_json(l->status_json());
    CHECK(st.get_bool("header_missing"));
  }
  // 3. Header after some junk (stale bytes of a previous session) is still found.
  {
    Clock clk;
    FakeHost h(&clk, 2);
    auto l = create_link("rtl_tcp", "{}", h, &err);
    REQUIRE(l);
    l->on_io_state(true);
    Bytes j(37, 0x81);
    j.insert(j.end(), hdr.begin(), hdr.begin() + 6);
    l->on_io_bytes(ByteView(j));
    l->on_io_bytes(ByteView(hdr).sub(6));
    CHECK(l->state() == LinkState::Up);
    CHECK(parse_json(l->status_json()).get_string("tuner") == "R820T");
  }
  // 4. Not an rtl_tcp server: Error, then the driver retries by itself (no host reconnect).
  {
    Clock clk;
    FakeHost h(&clk, 2);
    auto l = create_link("rtl_tcp", "{}", h, &err);
    REQUIRE(l);
    l->on_io_state(true);
    l->on_io_bytes(ByteView(std::string_view("HTTP/1.1 400 Bad Request\r\n")));
    CHECK(l->state() == LinkState::Error);
    size_t writes = h.writes.size();
    for (int k = 0; k < 200; ++k) { clk.t += 50; l->tick(clk.t); }
    CHECK(l->state() == LinkState::Connecting);
    CHECK(h.writes.size() > writes);  // tuning re-sent
    l->on_io_bytes(ByteView(hdr));
    CHECK(l->state() == LinkState::Up);
  }
}

TEST_CASE("regression: default-config licence-free audio and kiss links hear each other (no N0CALL self-drop)") {
  // Both hosts with the same entropy seed too: even identical pseudo ids must not make a station
  // drop the other's frames as its own echo.
  for (uint64_t seed_b : {uint64_t(22), uint64_t(11)}) {
    CAPTURE(seed_b);
    std::string err;
    {  // audio, AFSK1200 (AX.25), licence-free, otherwise defaults
      Clock clk;
      FakeHost ha(&clk, 11), hb(&clk, seed_b);
      const char* cfg = R"({"licence":"licence_free","persistence":255})";
      auto a = create_link("audio", cfg, ha, &err);
      auto b = create_link("audio", cfg, hb, &err);
      REQUIRE(a);
      REQUIRE(b);
      a->on_io_state(true);
      b->on_io_state(true);
      std::string call = parse_json(a->status_json()).get("modem_status").get("ax25").get_string("callsign");
      CHECK(call.substr(0, 2) == "QX");
      CHECK(call.substr(0, call.find('-')).find_first_of("0123456789") == std::string::npos);  // never a real call
      Bytes f = pattern(40, 3), g = pattern(50, 5);
      REQUIRE(a->send(ByteView(f), 0));
      std::vector<float> tx = transmit(*a, ha, 48000);
      REQUIRE(!tx.empty());
      receive(*b, hb, tx, 48000, 48000, 40);
      REQUIRE(hb.frames.size() == 1);
      CHECK(hb.frames[0].data == f);
      REQUIRE(b->send(ByteView(g), 0));
      std::vector<float> tx2 = transmit(*b, hb, 48000);
      receive(*a, ha, tx2, 48000, 48000, 40);
      REQUIRE(ha.frames.size() == 1);
      CHECK(ha.frames[0].data == g);
      // Our own transmission looped back (digipeater / acoustic echo) is still suppressed.
      receive(*a, ha, tx, 48000, 48000, 40);
      CHECK(ha.frames.size() == 1);
      // The pseudo id is stable across reconfigure.
      REQUIRE(a->reconfigure(R"({"licence":"licence_free","persistence":128})", &err));
      CHECK(parse_json(a->status_json()).get("modem_status").get("ax25").get_string("callsign") == call);
    }
    {  // kiss, AX.25 payload, licence-free, default callsign
      Pair p("kiss", R"({"licence":"licence_free"})", R"({"licence":"licence_free"})");
      p.hb = FakeHost(&p.clk, seed_b);
      p.b = create_link("kiss", R"({"licence":"licence_free"})", p.hb, &p.error);
      REQUIRE(p.ok());
      p.ab.set_dst(p.b.get());
      p.hb.on_write = [&p](ByteView v) { p.ba.push(v); };
      p.connect();
      Bytes f = pattern(30, 1), g = pattern(31, 2);
      REQUIRE(p.a->send(ByteView(f), 0));
      REQUIRE(p.b->send(ByteView(g), 0));
      p.run(20000);
      REQUIRE(p.hb.frames.size() == 1);
      CHECK(p.hb.frames[0].data == f);
      REQUIRE(p.ha.frames.size() == 1);
      CHECK(p.ha.frames[0].data == g);
    }
  }
  // Amateur mode with the default callsign still refuses to transmit.
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto am = create_link("audio", "{}", h, &err);
  REQUIRE(am);
  am->on_io_state(true);
  CHECK(!am->send(ByteView(pattern(10, 1)), 0));
  auto k = create_link("kiss", "{}", h, &err);
  REQUIRE(k);
  k->on_io_state(true);
  CHECK(!k->send(ByteView(pattern(10, 1)), 0));
  CHECK(parse_json(k->status_json()).get("ax25").get_string("callsign") == "N0CALL");
}

TEST_CASE("regression: audio link CSMA transmits after digital silence, DC or a quiet noise floor") {
  for (uint32_t rate : {uint32_t(8000), uint32_t(44100), uint32_t(48000), uint32_t(96000)}) {
    for (int kind = 0; kind < 3; ++kind) {
      CAPTURE(rate);
      CAPTURE(kind);
      Clock clk;
      FakeHost hs(&clk, 1), hr(&clk, 2);
      std::string err;
      auto a = create_link("audio", kAfskA, hs, &err);
      auto b = create_link("audio", R"({"modem":"afsk1200","callsign":"QX0XYZ-2","persistence":255})", hr, &err);
      REQUIRE(a);
      REQUIRE(b);
      a->on_io_state(true);
      b->on_io_state(true);
      REQUIRE(a->send(ByteView(pattern(80, 7)), 0));
      std::vector<float> tx = transmit(*a, hs, rate);
      REQUIRE(!tx.empty());
      // b hears half of a's transmission (cut mid-frame), then the input goes silent / DC / -70 dBFS.
      std::vector<int16_t> pcm(tx.size() / 2);
      for (size_t i = 0; i < pcm.size(); ++i) pcm[i] = static_cast<int16_t>(tx[i] * 16000);
      b->on_audio(pcm.data(), pcm.size(), rate);
      Prng r(5);
      std::vector<int16_t> fill(rate / 50);
      for (int blk = 0; blk < 100; ++blk) {  // 2 s, keep feeding input (the device never stops)
        for (auto& v : fill) v = kind == 0 ? 0 : kind == 1 ? 8000 : static_cast<int16_t>((r.unit() - 0.5) * 36);
        b->on_audio(fill.data(), fill.size(), rate);
        clk.t += 20;
        b->tick(clk.t);
      }
      CHECK(!b->channel_busy());
      REQUIRE(b->send(ByteView(pattern(20, 9)), 0));
      bool keyed = false;
      std::vector<int16_t> out(rate / 100);
      for (int blk = 0; blk < 300 && !keyed; ++blk) {  // within 3 s, still with input flowing
        for (auto& v : fill) v = kind == 0 ? 0 : kind == 1 ? 8000 : static_cast<int16_t>((r.unit() - 0.5) * 36);
        b->on_audio(fill.data(), out.size(), rate);
        clk.t += 10;
        b->tick(clk.t);
        keyed = b->pull_audio(out.data(), out.size(), rate) > 0;
      }
      CHECK(keyed);
    }
  }
}

TEST_CASE("audio: stable level-meter status fields") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto a = create_link("audio", kAfskA, h, &err);
  REQUIRE(a);
  a->on_io_state(true);
  auto fields = [&](Json* st) {
    REQUIRE(Json::parse(a->status_json(), *st));
    for (const char* k : {"rx_level_dbfs", "rx_peak_dbfs", "clip_percent", "frames_ok", "frames_bad"})
      CHECK_MESSAGE(st->get(k).is_number(), k);
    for (const char* k : {"clipping", "dcd", "tx_active"}) CHECK_MESSAGE(st->get(k).is_bool(), k);
    for (const char* k : {"last_error", "level_hint"}) CHECK_MESSAGE(st->get(k).is_string(), k);
    CHECK(st->has("snr_db"));
    CHECK(st->has("rx_audio_age_ms"));
  };
  Json st;
  // nothing received yet
  fields(&st);
  CHECK(st.get_double("rx_level_dbfs") == -120.0);
  CHECK(st.get_double("rx_peak_dbfs") == -120.0);
  CHECK(st.get("snr_db").is_null());
  CHECK(st.get("rx_audio_age_ms").is_null());
  CHECK(st.get_string("level_hint") == "no_audio");
  CHECK(st.get_string("last_error").empty());
  CHECK(!st.get_bool("clipping"));
  CHECK(st.get_int("frames_ok") == 0);
  // a -20 dBFS sine (amplitude 0.1): level ~ -20, no clipping, hint ok
  std::vector<int16_t> pcm(8000);
  for (size_t i = 0; i < pcm.size(); ++i)
    pcm[i] = static_cast<int16_t>(3277.0 * std::sin(2 * 3.14159265358979 * 1000.0 * static_cast<double>(i) / 8000.0));
  a->on_audio(pcm.data(), pcm.size(), 8000);
  fields(&st);
  CHECK(st.get_double("rx_level_dbfs") == doctest::Approx(-20.0).epsilon(0.05));
  CHECK(st.get_double("rx_peak_dbfs") == doctest::Approx(-20.0).epsilon(0.05));
  double v10 = st.get_double("rx_level_dbfs") * 10.0;
  CHECK(std::fabs(v10 - std::round(v10)) < 1e-6);  // 0.1 dB steps
  CHECK(!st.get_bool("clipping"));
  CHECK(st.get_double("clip_percent") == 0.0);
  CHECK(st.get_string("level_hint") == "ok");
  CHECK(st.get_int("rx_audio_age_ms") == 0);
  // audio stops: hint goes to no_audio after 2 s, dcd false
  clk.t += 2500;
  fields(&st);
  CHECK(st.get_int("rx_audio_age_ms") == 2500);
  CHECK(st.get_string("level_hint") == "no_audio");
  CHECK(!st.get_bool("dcd"));
  // full-scale square wave: clipping
  for (size_t i = 0; i < pcm.size(); ++i) pcm[i] = (i / 4) % 2 ? 32767 : -32768;
  a->on_audio(pcm.data(), pcm.size(), 8000);
  fields(&st);
  CHECK(st.get_bool("clipping"));
  CHECK(st.get_double("clip_percent") > 50.0);
  CHECK(st.get_double("clip_percent") <= 100.0);
  CHECK(st.get_string("level_hint") == "clipping");
  CHECK(st.get_double("rx_peak_dbfs") == doctest::Approx(0.0).epsilon(0.01));
  // transmitting
  REQUIRE(a->send(ByteView(pattern(40, 1)), 0));
  clk.t += 1000;
  a->tick(clk.t);
  fields(&st);
  CHECK(st.get_bool("tx_active"));
  std::vector<float> tx = transmit(*a, h, 8000);
  CHECK(!tx.empty());
  fields(&st);
  CHECK(!st.get_bool("tx_active"));
}

TEST_CASE("audio: robust modes get a modest max_frame from the mtu (false-header guard)") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto max_frame = [&](const char* cfg) -> int64_t {
    auto l = create_link("audio", cfg, h, &err);
    REQUIRE_MESSAGE(l, err);
    Json st;
    REQUIRE(Json::parse(l->status_json(), st));
    return st.get("modem_status").get_int("max_frame");
  };
  // licence-free: bare mesh frames, auto mtu 128 -> 128
  CHECK(max_frame(R"({"modem":"robust31","licence":"licence_free"})") == 128);
  CHECK(max_frame(R"({"modem":"robust250","licence":"licence_free","mtu":200})") == 200);
  CHECK(max_frame(R"({"modem":"robust62","licence":"licence_free","mtu":64})") == 128);  // floor: hears the default
  // amateur: AX.25 overhead on top
  int64_t ax = max_frame(R"({"modem":"robust31","callsign":"QX0XYZ-1"})");
  CHECK(ax > 128);
  CHECK(ax < 200);
  // explicit modem_params wins; HDLC modems are not limited
  CHECK(max_frame(R"({"modem":"robust31","licence":"licence_free","modem_params":"max_frame=600"})") == 600);
  CHECK(max_frame(R"({"modem":"afsk1200"})") > 300);
  // a full-mtu frame still goes through end to end (robust250, clean channel)
  FakeHost ha(&clk, 1), hb(&clk, 2);
  auto a = create_link("audio", R"({"modem":"robust250","licence":"licence_free","persistence":255})", ha, &err);
  auto b = create_link("audio", R"({"modem":"robust250","licence":"licence_free"})", hb, &err);
  REQUIRE(a);
  REQUIRE(b);
  a->on_io_state(true);
  b->on_io_state(true);
  REQUIRE(a->caps().mtu == 128);
  Bytes f = pattern(128, 7);
  REQUIRE(a->send(ByteView(f), 0));
  CHECK(!a->send(ByteView(pattern(129, 7)), 0));
  std::vector<float> tx = transmit(*a, ha, 8000, 300000);
  REQUIRE(!tx.empty());
  receive(*b, hb, tx, 8000, 8000, 40);
  REQUIRE(hb.frames.size() == 1);
  CHECK(hb.frames[0].data == f);
}

}  // TEST_SUITE
