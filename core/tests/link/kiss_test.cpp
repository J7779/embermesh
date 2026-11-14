// KISS driver: byte-exact KISS/AX.25 output, TNC setup, APRS interop with standard frames.
#include "doctest.h"
#include "framing/aprs.h"
#include "framing/ax25.h"
#include "framing/kiss.h"
#include "link_harness.h"

using namespace ember;
using namespace linktest;

namespace {

Bytes aprs_frame(const char* src, const char* dst, const char* path, const std::string& info) {
  std::vector<ax25::Address> digis;
  std::string p = path;
  size_t s = 0;
  while (s < p.size()) {
    size_t e = p.find(',', s);
    if (e == std::string::npos) e = p.size();
    digis.push_back(ax25::make_address(p.substr(s, e - s)));
    s = e + 1;
  }
  ax25::Frame f = ax25::make_ui(ax25::make_address(dst), ax25::make_address(src), digis,
                                ByteView(std::string_view(info)));
  f.set_command(true);
  return kiss::encode_data(ByteView(ax25::encode(f)));
}

// Decodes every KISS data frame written by the host into AX.25 frames.
std::vector<ax25::Frame> written_frames(const FakeHost& h) {
  std::vector<ax25::Frame> out;
  kiss::Decoder d(4096, true);
  Bytes all = h.written();
  d.feed(ByteView(all), [&](const kiss::Frame& k) {
    if (k.command != kiss::Command::Data) return;
    ax25::Frame f;
    if (ax25::decode(k.payload, &f)) out.push_back(f);
  });
  return out;
}

}  // namespace

TEST_SUITE("link") {

TEST_CASE("kiss: byte-exact mesh frame in AX.25 UI") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto l = create_link("kiss", R"({"callsign":"qx0xyz-7","set_params":false})", h, &err);
  REQUIRE(l);
  l->on_io_state(true);
  CHECK(h.writes.empty());
  Bytes mesh = {0x01, 0xC0, 0xDB, 0x42};
  REQUIRE(l->send(ByteView(mesh), 2));
  const Bytes expect = {
      0xC0, 0x00,
      0x8A, 0x9A, 0x84, 0x8A, 0xA4, 0x40, 0xE0,  // EMBER, C bit (command), SSID 0
      0xA2, 0xB0, 0x60, 0xB0, 0xB2, 0xB4, 0x6F,  // QX0XYZ-7 (SSID 7), last address
      0x03, 0xF0,                                // UI, no layer 3
      0x01, 0xDB, 0xDC, 0xDB, 0xDD, 0x42,        // info with KISS escapes
      0xC0};
  REQUIRE(h.writes.size() == 1);
  CAPTURE(to_hex(ByteView(h.writes[0])));
  CHECK(h.writes[0] == expect);
  LinkCaps c = l->caps();
  CHECK(!c.encryption_allowed);
  CHECK(c.requires_station_id);
  CHECK(c.mtu == 256);
  CHECK(c.bitrate_bps == 1200);
  CHECK(c.half_duplex);
  CHECK(c.cost == 40);
}

TEST_CASE("kiss: digipeater path, port, parameters and init text") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto l = create_link("kiss",
                       R"({"callsign":"W1AW","port":2,"path":"WIDE1-1","txdelay_ms":250,"persistence":128,)"
                       R"("slottime_ms":50,"txtail_ms":20,"init":"kiss_on"})",
                       h, &err);
  REQUIRE(l);
  l->on_io_state(true);
  Bytes all = h.written();
  std::string init = "\rKISS ON\rRESTART\r";
  REQUIRE(all.size() > init.size());
  CHECK(ByteView(all).sub(0, init.size()).to_string() == init);
  const Bytes params = {0xC0, 0x21, 25, 0xC0, 0xC0, 0x22, 128, 0xC0, 0xC0, 0x23, 5, 0xC0,
                        0xC0, 0x24, 2, 0xC0, 0xC0, 0x25, 0, 0xC0};
  CHECK(ByteView(all).sub(init.size()) == ByteView(params));
  h.writes.clear();
  REQUIRE(l->send(ByteView(Bytes{9, 9, 9}), 0));
  REQUIRE(h.writes.size() == 1);
  CHECK(h.writes[0][1] == 0x20);  // port 2 data
  auto fr = written_frames(h);
  REQUIRE(fr.size() == 1);
  REQUIRE(fr[0].digis.size() == 1);
  CHECK(fr[0].digis[0].to_string() == "WIDE1-1");
  // frames for other ports are ignored; ours delivered
  Bytes other = aprs_frame("QX1ABC", "EMBER", "", "\x05\x06");
  l->on_io_bytes(ByteView(other));  // port 0
  CHECK(h.frames.empty());
  other[1] = 0x20;
  l->on_io_bytes(ByteView(other));
  REQUIRE(h.frames.size() == 1);
  CHECK((h.frames[0].data == Bytes{5, 6}));
  // custom init text with escapes; bad escape rejected
  CHECK(!create_link("kiss", R"({"callsign":"W1AW","init":"custom","init_custom":"\\q"})", h, &err));
  CHECK(create_link("kiss", R"({"callsign":"W1AW","init":"custom","init_custom":"TC 0\\r"})", h, &err));
  CHECK(!create_link("kiss", R"({"callsign":"TOOLONGCALL"})", h, &err));
  CHECK(!create_link("kiss", R"({"callsign":"W1AW","path":"WIDE1-1,BAD CALL"})", h, &err));
}

TEST_CASE("kiss: N0CALL refuses amateur transmissions, exit KISS on close") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto l = create_link("kiss", "{}", h, &err);
  REQUIRE(l);
  l->on_io_state(true);
  CHECK(!l->send(ByteView(Bytes{1, 2}), 0));
  Json st;
  REQUIRE(Json::parse(l->status_json(), st));
  CHECK(st.get_string("last_error").find("callsign") != std::string::npos);
  auto lf = create_link("kiss", R"({"licence":"licence_free"})", h, &err);
  REQUIRE(lf);
  lf->on_io_state(true);
  CHECK(lf->send(ByteView(Bytes{1, 2}), 0));
  CHECK(lf->caps().encryption_allowed);
  CHECK(!lf->caps().requires_station_id);
  h.writes.clear();
  {
    auto ex = create_link("kiss", R"({"callsign":"W1AW","exit_kiss":true,"set_params":false})", h, &err);
    REQUIRE(ex);
    ex->on_io_state(true);
  }
  REQUIRE(!h.writes.empty());
  CHECK((h.writes.back() == Bytes{0xC0, 0xFF, 0xC0}));
}

TEST_CASE("kiss: APRS message interop with acks (standard frames)") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto l = create_link("kiss", R"({"callsign":"QX0XYZ-7","set_params":false})", h, &err);
  REQUIRE(l);
  l->on_io_state(true);
  // A stock APRS radio sends us a numbered message.
  Bytes in = aprs_frame("QX0ABC-9", "APK004", "WIDE1-1", ":QX0XYZ-7 :Hello there{12");
  // deliver in odd chunks
  for (size_t i = 0; i < in.size(); i += 5) l->on_io_bytes(ByteView(in).sub(i, 5));
  REQUIRE(h.texts.size() == 1);
  CHECK(h.texts[0].from == "QX0ABC-9");
  CHECK(h.texts[0].to == "QX0XYZ-7");
  CHECK(h.texts[0].text == "Hello there");
  CHECK(h.texts[0].network == "aprs");
  // ack sent: ":QX0ABC-9 :ack12" from QX0XYZ-7 to APZEMB via WIDE1-1,WIDE2-1
  auto out = written_frames(h);
  REQUIRE(out.size() == 1);
  CHECK(out[0].dst.call == "APZEMB");
  CHECK(out[0].src.to_string() == "QX0XYZ-7");
  CHECK(out[0].digis.size() == 2);
  CHECK(ByteView(out[0].info).to_string() == ":QX0ABC-9 :ack12");
  // the sender retries (our ack was lost): ack again, but no duplicate chat message
  h.writes.clear();
  clk.t += 1000;
  l->on_io_bytes(ByteView(in));
  CHECK(h.texts.size() == 1);
  CHECK(written_frames(h).size() == 1);
  // monitored traffic for others, positions and status
  l->on_io_bytes(ByteView(aprs_frame("QX3XYZ", "APRS", "", ":BLN1     :Net tonight 8pm")));
  l->on_io_bytes(ByteView(aprs_frame("QX3XYZ", "APRS", "", "!4903.50N/07201.75W-Test house")));
  l->on_io_bytes(ByteView(aprs_frame("QX3XYZ", "APRS", "", ">On the air")));
  REQUIRE(h.texts.size() == 4);
  CHECK(h.texts[1].to == "BLN1");
  CHECK(h.texts[2].text.find("49.05833") != std::string::npos);
  CHECK(h.texts[2].text.find("-72.02917") != std::string::npos);
  CHECK(h.texts[3].text == "Status: On the air");
}

TEST_CASE("kiss: send_interop_text as APRS message, ack and retry") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto l = create_link("kiss", R"({"callsign":"QX0XYZ-7","set_params":false})", h, &err);
  REQUIRE(l);
  l->on_io_state(true);
  InteropText m;
  m.to = "qx0abc-9";
  m.text = "Hi from the mesh";
  m.network = "aprs";
  REQUIRE(l->send_interop_text(m));
  auto out = written_frames(h);
  REQUIRE(out.size() == 1);
  aprs::Message msg;
  REQUIRE(aprs::parse_message(ByteView(out[0].info).to_string(), &msg));
  CHECK(msg.addressee == "QX0ABC-9");
  CHECK(msg.text == "Hi from the mesh");
  REQUIRE(!msg.id.empty());
  // no ack yet: retried after 30 s
  h.writes.clear();
  for (int i = 0; i < 31; ++i) { clk.t += 1000; l->tick(clk.t); }
  CHECK(written_frames(h).size() == 1);
  // ack arrives
  l->on_io_bytes(ByteView(aprs_frame("QX0ABC-9", "APK004", "", ":QX0XYZ-7 :ack" + msg.id)));
  REQUIRE(h.count_events("aprs_ack") == 1);
  CHECK(h.events.back().second.find("\"acked\":true") != std::string::npos);
  h.writes.clear();
  for (int i = 0; i < 300; ++i) { clk.t += 1000; l->tick(clk.t); }
  CHECK(h.writes.empty());
  // unanswered message gives up after the retries
  m.text = "Anyone?";
  REQUIRE(l->send_interop_text(m));
  for (int i = 0; i < 600; ++i) { clk.t += 1000; l->tick(clk.t); }
  CHECK(written_frames(h).size() == 4);  // first + 3 retries
  REQUIRE(h.count_events("aprs_ack") == 2);
  CHECK(h.events.back().second.find("\"acked\":false") != std::string::npos);
  // long text is split; empty addressee becomes a bulletin
  h.writes.clear();
  m.to = "";
  m.text = std::string(100, 'x');
  REQUIRE(l->send_interop_text(m));
  for (int i = 0; i < 5; ++i) { clk.t += 1000; l->tick(clk.t); }
  out = written_frames(h);
  REQUIRE(out.size() == 2);
  CHECK(ByteView(out[0].info).to_string().rfind(":BLN0     :", 0) == 0);
  // invalid addressee / wrong network refused
  m.to = "WAYTOOLONGCALL";
  CHECK(!l->send_interop_text(m));
  m.to = "QX0ABC";
  m.network = "meshtastic";
  CHECK(!l->send_interop_text(m));
}

TEST_CASE("kiss: mesh frames from AX.25 peers, own echoes ignored, raw mode") {
  Clock clk;
  FakeHost h(&clk);
  std::string err;
  auto l = create_link("kiss", R"({"callsign":"QX0XYZ-7","set_params":false})", h, &err);
  REQUIRE(l);
  l->on_io_state(true);
  // Our own frame repeated by a digipeater is ignored ...
  REQUIRE(l->send(ByteView(Bytes{0x01, 0x02}), 0));
  l->on_io_bytes(ByteView(aprs_frame("QX0XYZ-7", "EMBER", "WIDE1-1*", "\x01\x02")));
  CHECK(h.frames.empty());
  l->on_io_bytes(ByteView(aprs_frame("QX0XYZ-8", "EMBER", "WIDE1-1*", "\x01\x02")));
  CHECK(h.frames.size() == 1);
  // ... but another station using the same callsign (content we never sent) is heard.
  l->on_io_bytes(ByteView(aprs_frame("QX0XYZ-7", "EMBER", "WIDE1-1*", "\x03\x04")));
  CHECK(h.frames.size() == 2);
  // ACKMODE / other commands and non-UI frames are ignored, not errors
  l->on_io_bytes(ByteView(Bytes{0xC0, 0x0C, 0x01, 0x02, 0xC0}));
  CHECK(h.frames.size() == 2);
  // raw payload mode: KISS data == mesh frame, nothing else interpreted
  FakeHost h2(&clk);
  auto r = create_link("kiss", R"({"payload":"raw","licence":"licence_free","set_params":false})", h2, &err);
  REQUIRE(r);
  r->on_io_state(true);
  REQUIRE(r->send(ByteView(Bytes{0xAA}), 0));
  CHECK((h2.writes[0] == Bytes{0xC0, 0x00, 0xAA, 0xC0}));
  r->on_io_bytes(ByteView(Bytes{0xC0, 0x00, 0x55, 0xC0}));
  REQUIRE(h2.frames.size() == 1);
  CHECK((h2.frames[0].data == Bytes{0x55}));
  InteropText m;
  m.to = "W1AW";
  m.text = "x";
  CHECK(!r->send_interop_text(m));
}

}  // TEST_SUITE
