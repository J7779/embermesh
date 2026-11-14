#include <cmath>
#include <string>

#include "doctest.h"
#include "ember/crc.h"
#include "framing/aprs.h"
#include "framing/ax25.h"
#include "framing/kiss.h"
#include "framing/test_rng.h"

using namespace ember;
using framing_test::Rng;

namespace {

Bytes H(const char* hex) {
  Bytes b;
  REQUIRE(from_hex(hex, b));
  return b;
}
std::string S(const Bytes& b) { return std::string(b.begin(), b.end()); }
ByteView V(std::string_view s) { return ByteView(s); }

// APRS position packet from N0CALL to APRS via WIDE2-1 (address bytes derived by hand from the
// AX.25 2.2 spec: chars << 1, SSID byte 0b CRRSSSSX).
const char* kFrameHex =
    "82a0a4a64040e0"  // APRS    C=1 RR=11 SSID 0
    "9c608682989860"  // N0CALL  C=0 RR=11 SSID 0
    "ae92888a644063"  // WIDE2-1 H=0, last
    "03f0";           // UI, no layer 3
const char* kInfo = "!4903.50N/07201.75W-Test";

bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

}  // namespace

TEST_SUITE("framing") {

// ---------------------------------------------------------------- AX.25 addresses

TEST_CASE("AX.25 address parse / format / validation") {
  ax25::Address a;
  CHECK(ax25::parse_address("N0CALL-7", &a));
  CHECK(a.call == "N0CALL");
  CHECK(a.ssid == 7);
  CHECK_FALSE(a.flag);
  CHECK(a.to_string() == "N0CALL-7");
  CHECK(ax25::parse_address("wide2-1*", &a));
  CHECK(a.call == "WIDE2");
  CHECK(a.flag);
  CHECK(ax25::parse_address("A-15", &a));
  CHECK(a.to_string() == "A-15");
  CHECK(ax25::parse_address("ABCDEF", &a));
  CHECK(ax25::parse_address("X-0", &a));
  CHECK(a.to_string() == "X");
  for (const char* bad : {"", "-1", "ABCDEFG", "N0CALL-16", "N0CALL-", "N0CALL-05", "N0 CAL", "N0CALL-1-2",
                          "N0CALL-a", "qAR", "N0CÄL", "*", "N0CALL-123"}) {
    INFO(bad);
    if (std::string(bad) == "qAR") {  // lower case is upper-cased: valid AX.25 address QAR
      CHECK(ax25::parse_address(bad, &a));
      continue;
    }
    CHECK_FALSE(ax25::parse_address(bad, &a));
  }
  CHECK_FALSE(ax25::make_address("bad call").valid());
}

TEST_CASE("AX.25 7-byte address encoding, exhaustive SSID and flag bits") {
  uint8_t buf[7];
  ax25::Address a = ax25::make_address("APRS");
  CHECK(ax25::encode_address(a, false, buf));
  CHECK(ByteView(buf, 7) == ByteView(H("82a0a4a6404060")));
  for (int ssid = 0; ssid < 16; ++ssid)
    for (int flag = 0; flag < 2; ++flag)
      for (int last = 0; last < 2; ++last) {
        ax25::Address x = ax25::make_address("QX1ABC");
        x.ssid = static_cast<uint8_t>(ssid);
        x.flag = flag != 0;
        REQUIRE(ax25::encode_address(x, last != 0, buf));
        ax25::Address y;
        bool l = false;
        REQUIRE(ax25::decode_address(buf, &y, &l));
        CHECK(y == x);
        CHECK(y.flag == x.flag);
        CHECK(l == (last != 0));
      }
  ax25::Address bad;
  bad.call = "TOOLONG";
  CHECK_FALSE(ax25::encode_address(bad, true, buf));
  // decode rejects: extension bit in call, embedded space, lowercase, all spaces
  bool l;
  ax25::Address y;
  CHECK_FALSE(ax25::decode_address(H("83a0a4a6404061").data(), &y, &l));
  CHECK_FALSE(ax25::decode_address(H("8240a4a6404061").data(), &y, &l));
  CHECK_FALSE(ax25::decode_address(H("c2a0a4a6404061").data(), &y, &l));  // 'a'
  CHECK_FALSE(ax25::decode_address(H("40404040404061").data(), &y, &l));
}

// ---------------------------------------------------------------- AX.25 frames

TEST_CASE("AX.25 UI frame encode matches hand-derived bytes; decode round trip") {
  ax25::Frame f = ax25::make_ui(ax25::make_address("APRS"), ax25::make_address("N0CALL"),
                                {ax25::make_address("WIDE2-1")}, V(kInfo));
  Bytes enc = ax25::encode(f);
  Bytes want = H(kFrameHex);
  want.insert(want.end(), kInfo, kInfo + std::strlen(kInfo));
  CHECK(to_hex(enc) == to_hex(want));
  ax25::Frame g;
  REQUIRE(ax25::decode(enc, &g));
  CHECK(g.dst.to_string() == "APRS");
  CHECK(g.src.to_string() == "N0CALL");
  REQUIRE(g.digis.size() == 1);
  CHECK(g.digis[0].to_string() == "WIDE2-1");
  CHECK(g.is_ui());
  CHECK(g.has_pid());
  CHECK(g.pid == 0xF0);
  CHECK(g.command_response() == ax25::CommandResponse::Command);
  CHECK(g.next_digi() == 0);
  CHECK(S(g.info) == kInfo);
  CHECK(ax25::encode(g) == enc);  // bit-exact re-encode (reserved bits kept)
  CHECK(aprs::format_tnc2(g) == std::string("N0CALL>APRS,WIDE2-1:") + kInfo);
}

TEST_CASE("AX.25 FCS (CRC-16/X-25, low byte first) and the HDLC residue") {
  CHECK(crc16_x25_of(V("123456789")) == 0x906E);
  Bytes fr = H(kFrameHex);
  fr.insert(fr.end(), kInfo, kInfo + std::strlen(kInfo));
  Bytes with = fr;
  ax25::append_fcs(with);
  REQUIRE(with.size() == fr.size() + 2);
  uint16_t c = ax25::fcs(fr);
  CHECK(with[fr.size()] == (c & 0xFF));
  CHECK(with[fr.size() + 1] == (c >> 8));
  CHECK(crc16_x25(ByteView(with)) == 0xF0B8);  // good-FCS residue of HDLC
  CHECK(ax25::check_fcs(with));
  ax25::Frame g;
  CHECK(ax25::decode_with_fcs(with, &g));
  // every single bit flip is detected
  for (size_t i = 0; i < with.size() * 8; ++i) {
    Bytes b = with;
    b[i / 8] ^= static_cast<uint8_t>(1 << (i % 8));
    CHECK_FALSE(ax25::check_fcs(b));
  }
  CHECK_FALSE(ax25::check_fcs(H("0102")));
  CHECK_FALSE(ax25::check_fcs(ByteView()));
}

TEST_CASE("AX.25 digipeater path: 8 digis, H bits, 9 rejected") {
  std::vector<ax25::Address> path;
  for (int i = 0; i < 8; ++i) {
    ax25::Address a = ax25::make_address("DIGI" + std::to_string(i));
    a.ssid = static_cast<uint8_t>(i);
    a.flag = i < 3;
    path.push_back(a);
  }
  ax25::Frame f = ax25::make_ui(ax25::make_address("APZEMB"), ax25::make_address("QX1ABC-9"), path, V("x"));
  Bytes enc = ax25::encode(f);
  REQUIRE(enc.size() == 70 + 2 + 1);
  CHECK((enc[69] & 1) == 1);  // last bit on 10th address
  ax25::Frame g;
  REQUIRE(ax25::decode(enc, &g));
  REQUIRE(g.digis.size() == 8);
  for (int i = 0; i < 8; ++i) CHECK(g.digis[static_cast<size_t>(i)].flag == (i < 3));
  CHECK(g.next_digi() == 3);
  CHECK(aprs::format_tnc2(g) == "QX1ABC-9>APZEMB,DIGI0,DIGI1-1,DIGI2-2*,DIGI3-3,DIGI4-4,DIGI5-5,DIGI6-6,DIGI7-7:x");
  f.digis.push_back(ax25::make_address("NINE"));
  CHECK(ax25::encode(f).empty());
  // hand-made 11-address field is rejected by decode
  Bytes eleven;
  for (int i = 0; i < 11; ++i) {
    uint8_t a[7];
    ax25::encode_address(ax25::make_address("AB"), i == 10, a);
    eleven.insert(eleven.end(), a, a + 7);
  }
  eleven.push_back(0x03);
  eleven.push_back(0xF0);
  CHECK_FALSE(ax25::decode(eleven, &g));
}

TEST_CASE("AX.25 command/response bits and non-UI control fields") {
  ax25::Frame f = ax25::make_ui(ax25::make_address("A"), ax25::make_address("B"), {}, ByteView());
  f.set_command(false);
  ax25::Frame g;
  REQUIRE(ax25::decode(ax25::encode(f), &g));
  CHECK(g.command_response() == ax25::CommandResponse::Response);
  f.dst.flag = f.src.flag = true;
  REQUIRE(ax25::decode(ax25::encode(f), &g));
  CHECK(g.command_response() == ax25::CommandResponse::Legacy);
  // SABM (0x2F, no PID), RR (0x01 S-frame), I-frame (0x00 has PID), UI with P bit (0x13)
  f.control = 0x3F;  // SABM + P
  Bytes e = ax25::encode(f);
  CHECK(e.size() == 15);
  REQUIRE(ax25::decode(e, &g));
  CHECK_FALSE(g.has_pid());
  CHECK(g.poll_final());
  f.control = 0x01;
  REQUIRE(ax25::decode(ax25::encode(f), &g));
  CHECK_FALSE(g.has_pid());
  f.control = 0x00;
  f.info = H("abcd");
  e = ax25::encode(f);
  CHECK(e.size() == 18);
  REQUIRE(ax25::decode(e, &g));
  CHECK(g.has_pid());
  CHECK(g.info == H("abcd"));
  f.control = 0x13;
  REQUIRE(ax25::decode(ax25::encode(f), &g));
  CHECK(g.is_ui());
  CHECK(g.poll_final());
}

TEST_CASE("AX.25 decode boundaries and malformed frames") {
  ax25::Frame g;
  Bytes base = H(kFrameHex);
  CHECK(ax25::decode(base, &g));  // empty info is fine
  CHECK(g.info.empty());
  for (size_t n = 0; n < base.size(); ++n) CHECK_FALSE(ax25::decode(ByteView(base).sub(0, n), &g));
  // only one address (last bit on dst)
  Bytes one = H("82a0a4a64040e1" "03f0");
  CHECK_FALSE(ax25::decode(one, &g));
  // max_info boundary
  Bytes withinfo = base;
  withinfo.resize(base.size() + 256, 'x');
  CHECK(ax25::decode(withinfo, &g, 256));
  withinfo.push_back('x');
  CHECK_FALSE(ax25::decode(withinfo, &g, 256));
}

TEST_CASE("AX.25 fuzz: random bytes and bit flips never crash; decoded frames re-encode") {
  Rng rng(11);
  Bytes good = H(kFrameHex);
  good.insert(good.end(), kInfo, kInfo + std::strlen(kInfo));
  int accepted = 0;
  for (int iter = 0; iter < 3000; ++iter) {
    Bytes b;
    switch (iter % 3) {
      case 0: b = rng.bytes(rng.below(120)); break;
      case 1:
        b = good;
        for (int k = 0; k < 1 + static_cast<int>(rng.below(4)); ++k) b[rng.below(b.size())] ^= static_cast<uint8_t>(1u << rng.below(8));
        break;
      default: b = ByteView(good).sub(0, rng.below(good.size() + 1)).to_bytes(); break;
    }
    ax25::Frame g;
    if (ax25::decode(b, &g)) {
      ++accepted;
      CHECK(g.digis.size() <= 8);
      CHECK(g.dst.valid());
      CHECK(g.src.valid());
      CHECK(ax25::encode(g) == b);  // decode is lossless for anything it accepts
    }
    // with FCS: corrupted frames must never be accepted
    Bytes withfcs = good;
    ax25::append_fcs(withfcs);
    Bytes c = withfcs;
    c[rng.below(c.size())] ^= static_cast<uint8_t>(1u + rng.below(255));
    CHECK_FALSE(ax25::decode_with_fcs(c, &g));
  }
  CHECK(accepted > 0);
}

TEST_CASE("AX.25 inside KISS end to end") {
  ax25::Frame f = ax25::make_ui(ax25::make_address("APZEMB"), ax25::make_address("QX1ABC-7"),
                                {ax25::make_address("WIDE1-1")}, V(":N0CALL   :hi{1"));
  Bytes k = kiss::encode_data(ax25::encode(f));
  kiss::Decoder d;
  int n = 0;
  d.feed(k, [&](const kiss::Frame& kf) {
    ax25::Frame g;
    REQUIRE(ax25::decode(kf.payload, &g));
    aprs::Packet p = aprs::parse(g);
    REQUIRE(p.type == aprs::PacketType::Message);
    CHECK(p.message.addressee == "N0CALL");
    CHECK(p.message.text == "hi");
    CHECK(p.message.id == "1");
    ++n;
  });
  CHECK(n == 1);
}

// ---------------------------------------------------------------- APRS messages

TEST_CASE("APRS message build (APRS101 ch.14 examples)") {
  // WU2Z and KB2ICI-14 are the APRS specification's own examples (APRS101 chapter 14).
  std::string s;
  CHECK(aprs::build_message("WU2Z", "Testing", "003", &s));
  CHECK(s == ":WU2Z     :Testing{003");
  CHECK(aprs::build_ack("KB2ICI-14", "003", &s));
  CHECK(s == ":KB2ICI-14:ack003");
  CHECK(aprs::build_rej("KB2ICI-14", "003", &s));
  CHECK(s == ":KB2ICI-14:rej003");
  CHECK(aprs::build_bulletin('3', "Snow expected in Tampa RSN", &s));
  CHECK(s == ":BLN3     :Snow expected in Tampa RSN");
  CHECK(aprs::build_message("WU2Z", "", "", &s));
  CHECK(s == ":WU2Z     :");
  // limits
  CHECK(aprs::build_message("ABCDEFGHI", std::string(67, 'x'), "12345", &s));
  CHECK_FALSE(aprs::build_message("ABCDEFGHIJ", "x", "", &s));
  CHECK_FALSE(aprs::build_message("", "x", "", &s));
  CHECK_FALSE(aprs::build_message("A B", "x", "", &s));
  CHECK_FALSE(aprs::build_message("A", std::string(68, 'x'), "", &s));
  CHECK_FALSE(aprs::build_message("A", "x", "123456", &s));
  CHECK_FALSE(aprs::build_message("A", "x", "1-2", &s));
  CHECK_FALSE(aprs::build_message("A", "a{b", "", &s));
  CHECK_FALSE(aprs::build_message("A", "a|b", "", &s));
  CHECK_FALSE(aprs::build_message("A", "a\nb", "", &s));
  CHECK_FALSE(aprs::build_ack("A", "", &s));
  CHECK_FALSE(aprs::build_bulletin('a', "x", &s));
}

TEST_CASE("APRS message sanitize keeps UTF-8 and truncates on a character boundary") {
  CHECK(aprs::sanitize_message_text("a{b|c~d\x01") == "a?b?c?d?");
  std::string u = "Grüße";  // UTF-8
  CHECK(aprs::sanitize_message_text(u) == u);
  std::string longs(66, 'x');
  longs += "é";  // 2 bytes: would make 68 -> dropped
  CHECK(aprs::sanitize_message_text(longs) == std::string(66, 'x'));
  CHECK(aprs::sanitize_message_text("\xff\xfe") == "??");
  std::string s;
  CHECK(aprs::build_message("A", aprs::sanitize_message_text(std::string(500, '{')), "", &s));
}

TEST_CASE("APRS message parse: text, id, ack, rej, reply-ack, bulletin") {
  aprs::Message m;
  REQUIRE(aprs::parse_message(":WU2Z     :Testing{003", &m));
  CHECK(m.addressee == "WU2Z");
  CHECK(m.kind == aprs::MessageKind::Text);
  CHECK(m.text == "Testing");
  CHECK(m.id == "003");
  REQUIRE(aprs::parse_message(":KB2ICI-14:ack003\r\n", &m));
  CHECK(m.kind == aprs::MessageKind::Ack);
  CHECK(m.id == "003");
  CHECK(m.addressee == "KB2ICI-14");
  REQUIRE(aprs::parse_message(":KB2ICI-14:rej5", &m));
  CHECK(m.kind == aprs::MessageKind::Rej);
  CHECK(m.id == "5");
  REQUIRE(aprs::parse_message(":KB2ICI-14:Hello{AB}CD", &m));
  CHECK(m.text == "Hello");
  CHECK(m.id == "AB");
  CHECK(m.reply_ack == "CD");
  REQUIRE(aprs::parse_message(":KB2ICI-14:Hello{AB}", &m));
  CHECK(m.id == "AB");
  CHECK(m.reply_ack.empty());
  REQUIRE(aprs::parse_message(":BLN3     :Snow expected", &m));
  CHECK(m.is_bulletin());
  CHECK(m.id.empty());
  // "ack" as plain text and braces that are not ids
  REQUIRE(aprs::parse_message(":N0CALL   :acknowledged, thanks", &m));
  CHECK(m.kind == aprs::MessageKind::Text);
  CHECK(m.text == "acknowledged, thanks");
  REQUIRE(aprs::parse_message(":N0CALL   :set {x y}", &m));
  CHECK(m.text == "set {x y}");
  CHECK(m.id.empty());
  REQUIRE(aprs::parse_message(":N0CALL   :", &m));
  CHECK(m.text.empty());
  // malformed
  CHECK_FALSE(aprs::parse_message("", &m));
  CHECK_FALSE(aprs::parse_message(":N0CALL:hi", &m));
  CHECK_FALSE(aprs::parse_message(":N0CALL   ", &m));
  CHECK_FALSE(aprs::parse_message(":         :hi", &m));
  CHECK_FALSE(aprs::parse_message("!N0CALL   :hi", &m));
}

// ---------------------------------------------------------------- APRS positions

TEST_CASE("APRS uncompressed position parse (APRS101 examples)") {
  aprs::Position p;
  REQUIRE(aprs::parse_position("!4903.50N/07201.75W-Test 001234", &p));
  CHECK(near(p.lat, 49 + 3.5 / 60));
  CHECK(near(p.lon, -(72 + 1.75 / 60)));
  CHECK(p.symbol_table == '/');
  CHECK(p.symbol_code == '-');
  CHECK_FALSE(p.messaging);
  CHECK(p.comment == "Test 001234");
  CHECK_FALSE(p.has_course_speed);

  REQUIRE(aprs::parse_position("@092345/4903.50N/07201.75W>088/036", &p));
  CHECK(p.timestamp == "092345/");
  CHECK(p.messaging);
  CHECK(p.has_course_speed);
  CHECK(p.course_deg == 88);
  CHECK(p.speed_knots == 36);
  CHECK(p.symbol_code == '>');

  REQUIRE(aprs::parse_position("=4903.50S\\07201.75E#/A=001234 hi", &p));
  CHECK(near(p.lat, -(49 + 3.5 / 60)));
  CHECK(near(p.lon, 72 + 1.75 / 60));
  CHECK(p.symbol_table == '\\');
  CHECK(p.has_altitude);
  CHECK(p.altitude_ft == 1234);

  REQUIRE(aprs::parse_position("!4903.5 N/07201.7 W-", &p));
  CHECK(p.ambiguity == 1);
  REQUIRE(aprs::parse_position("!49  .  N/072  .  W-", &p));
  CHECK(p.ambiguity == 4);
  CHECK(near(p.lat, 49));
  CHECK(near(p.lon, -72));
  REQUIRE(aprs::parse_position("!4903.50N107201.75W#", &p));  // overlay '1'
  CHECK(p.symbol_table == '1');

  for (const char* bad : {"!", "!4903.50N/07201.75W", "!4903.50X/07201.75W-", "!9103.50N/07201.75W-",
                          "!4960.50N/07201.75W-", "!4903.50N/18101.75W-", "!49 3.50N/07201.75W-",
                          "!4903,50N/07201.75W-", "@0923/4903.50N/07201.75W>", "!4903.50N/07201.75Q-",
                          "x4903.50N/07201.75W-"}) {
    INFO(bad);
    CHECK_FALSE(aprs::parse_position(bad, &p));
  }
}

TEST_CASE("APRS compressed position (APRS101 ch.9 example)") {
  aprs::Position p;
  REQUIRE(aprs::parse_position("=/5L!!<*e7>7P[", &p));
  CHECK(p.compressed);
  CHECK(near(p.lat, 49.5, 1e-5));
  CHECK(near(p.lon, -72.75, 1e-5));
  CHECK(p.symbol_code == '>');
  CHECK(p.has_course_speed);
  CHECK(p.course_deg == 88);
  CHECK(p.speed_knots == 36);
  CHECK(p.messaging);
  // altitude form (T byte GGA): cs "S]" -> 1.002^((83-33)*91 + (93-33)) = 10004 ft
  REQUIRE(aprs::parse_position("!/5L!!<*e7>S]W", &p));
  CHECK(p.has_altitude);
  CHECK(p.altitude_ft == 10004);
  CHECK_FALSE(p.has_course_speed);
  CHECK_FALSE(aprs::parse_position("=/5L!!<*e7>7P", &p));      // too short
  CHECK_FALSE(aprs::parse_position("=/5L!~<*e7>7P[", &p));     // '~' not base91
}

TEST_CASE("APRS position build / parse round trips") {
  std::string s;
  aprs::Position p;
  p.lat = 49 + 3.5 / 60;
  p.lon = -(72 + 1.75 / 60);
  p.symbol_table = '/';
  p.symbol_code = '-';
  p.comment = "Test 001234";
  REQUIRE(aprs::build_position(p, &s));
  CHECK(s == "!4903.50N/07201.75W-Test 001234");
  p.messaging = true;
  p.has_course_speed = true;
  p.course_deg = 88;
  p.speed_knots = 36;
  p.timestamp = "092345z";
  p.comment.clear();
  REQUIRE(aprs::build_position(p, &s));
  CHECK(s == "@092345z4903.50N/07201.75W-088/036");
  p.has_course_speed = false;
  p.has_altitude = true;
  p.altitude_ft = -50;
  p.timestamp.clear();
  p.ambiguity = 2;
  REQUIRE(aprs::build_position(p, &s));
  CHECK(s == "=4903.  N/07201.  W-/A=-00050");
  aprs::Position q;
  REQUIRE(aprs::parse_position(s, &q));
  CHECK(q.ambiguity == 2);
  CHECK(q.altitude_ft == -50);
  // extremes
  p = aprs::Position();
  p.lat = -90;
  p.lon = 180;
  REQUIRE(aprs::build_position(p, &s));
  CHECK(s == "!9000.00S/18000.00E>");
  p.lat = 90.0001;
  CHECK_FALSE(aprs::build_position(p, &s));
  p.lat = NAN;
  CHECK_FALSE(aprs::build_position(p, &s));

  // compressed build matches the APRS101 example bytes for 49.5 / -72.75, course 88, 36 kt
  p = aprs::Position();
  p.compressed = true;
  p.lat = 49.5;
  p.lon = -72.75;
  p.symbol_code = '>';
  p.messaging = true;
  p.has_course_speed = true;
  p.course_deg = 88;
  p.speed_knots = 36;
  REQUIRE(aprs::build_position(p, &s));
  CHECK(s.substr(0, 13) == "=/5L!!<*e7>7P");

  Rng rng(12);
  for (int i = 0; i < 500; ++i) {
    aprs::Position r;
    r.lat = rng.uniform() * 180 - 90;
    r.lon = rng.uniform() * 360 - 180;
    r.compressed = (i & 1) != 0;
    r.symbol_table = (i % 3) ? '/' : '\\';
    r.symbol_code = static_cast<char>('!' + rng.below(90));
    r.comment = "c" + std::to_string(i);
    REQUIRE(aprs::build_position(r, &s));
    aprs::Position back;
    REQUIRE(aprs::parse_position(s, &back));
    double tol = r.compressed ? 1e-5 : 1.0 / 6000 + 1e-9;
    CHECK(std::fabs(back.lat - r.lat) <= tol);
    CHECK(std::fabs(back.lon - r.lon) <= tol);
    CHECK(back.symbol_code == r.symbol_code);
    CHECK(back.symbol_table == r.symbol_table);
    CHECK(back.comment == r.comment);
  }
}

TEST_CASE("APRS Mic-E decode (APRS101 ch.10 info field example)") {
  aprs::Position p;
  REQUIRE(aprs::parse_mic_e("S32UVT", "`(_fn\"Oj/", &p));
  CHECK(p.mic_e);
  CHECK(near(p.lat, 33 + 25.64 / 60));
  CHECK(near(p.lon, -(112 + 7.74 / 60)));
  CHECK(p.speed_knots == 20);
  CHECK(p.course_deg == 251);
  CHECK(p.symbol_code == 'j');
  CHECK(p.symbol_table == '/');
  CHECK(p.mic_e_message == 4);
  // altitude "}" extension after a Kenwood type byte: "\"4T}" = (1*8281 + 19*91 + 51) - 10000 m
  REQUIRE(aprs::parse_mic_e("S32UVT", "`(_fn\"Oj/]\"4T}hello", &p));
  CHECK(p.has_altitude);
  CHECK(p.altitude_ft == static_cast<int32_t>(std::lround((8281 + 19 * 91 + 51 - 10000.0) * 3.28084)));
  // southern / eastern, ambiguity
  REQUIRE(aprs::parse_mic_e("33256L", "`(_fn\"Oj/", &p));
  CHECK(p.lat < 0);
  CHECK(p.lon > 0);
  CHECK(p.ambiguity == 1);
  aprs::Packet pk = aprs::parse("S32UVT", "`(_fn\"Oj/");
  CHECK(pk.type == aprs::PacketType::Position);
  // malformed
  CHECK_FALSE(aprs::parse_mic_e("S32UV", "`(_fn\"Oj/", &p));
  CHECK_FALSE(aprs::parse_mic_e("S32UVT", "`(_fn\"Oj", &p));
  CHECK_FALSE(aprs::parse_mic_e("S32UMT", "`(_fn\"Oj/", &p));
  CHECK_FALSE(aprs::parse_mic_e("SL2UVT", "`(_fn\"Oj/", &p));          // space before digit
  CHECK_FALSE(aprs::parse_mic_e("S32UVT", "`\x01_fn\"Oj/", &p));
  CHECK_FALSE(aprs::parse_mic_e("S32AVT", "`(_fn\"Oj/", &p));          // custom bit outside 1-3
}

TEST_CASE("APRS status and dispatch") {
  std::string s;
  CHECK(aprs::build_status("Net Control Center", &s));
  CHECK(s == ">Net Control Center");
  CHECK_FALSE(aprs::build_status("a\rb", &s));
  CHECK_FALSE(aprs::build_status(std::string(300, 'x'), &s));
  std::string st;
  CHECK(aprs::parse_status(">092345zNet Control Center", &st));
  CHECK(st == "092345zNet Control Center");
  CHECK(aprs::parse("APRS", ">hi").type == aprs::PacketType::Status);
  CHECK(aprs::parse("APRS", "!4903.50N/07201.75W-").type == aprs::PacketType::Position);
  CHECK(aprs::parse("APRS", ";OBJECT   *111111z4903.50N/07201.75W-").type == aprs::PacketType::Other);
  CHECK(aprs::parse("APRS", "").type == aprs::PacketType::Other);
  CHECK(aprs::parse("APRS", "!garbage").type == aprs::PacketType::Other);
}

// ---------------------------------------------------------------- TNC2

TEST_CASE("TNC2 monitor format parse / format") {
  ax25::Frame f;
  REQUIRE(aprs::parse_tnc2("N0CALL-9>APDR16,WIDE1*,WIDE2-1:=4903.50N/07201.75W$Hello\r\n", &f));
  CHECK(f.src.to_string() == "N0CALL-9");
  CHECK(f.dst.to_string() == "APDR16");
  REQUIRE(f.digis.size() == 2);
  CHECK(f.digis[0].flag);
  CHECK_FALSE(f.digis[1].flag);
  CHECK(S(f.info) == "=4903.50N/07201.75W$Hello");
  CHECK(aprs::format_tnc2(f) == "N0CALL-9>APDR16,WIDE1*,WIDE2-1:=4903.50N/07201.75W$Hello");
  // '*' on a later digi marks all earlier ones as repeated
  REQUIRE(aprs::parse_tnc2("A>B,C,D,E*:x", &f));
  CHECK(f.digis[0].flag);
  CHECK(f.digis[1].flag);
  CHECK(f.digis[2].flag);
  CHECK(aprs::format_tnc2(f) == "A>B,C,D,E*:x");
  // the info may contain ':' and '>'
  REQUIRE(aprs::parse_tnc2("A>B::N0CALL   :a>b:c", &f));
  CHECK(S(f.info) == ":N0CALL   :a>b:c");
  REQUIRE(aprs::parse_tnc2("A>B:", &f));
  CHECK(f.info.empty());
  // round trip through AX.25 bytes
  Bytes enc = ax25::encode(f);
  ax25::Frame g;
  REQUIRE(ax25::decode(enc, &g));
  CHECK(aprs::format_tnc2(g) == "A>B:");
  for (const char* bad : {"", "A>B", "AB:x", ">B:x", "A>:x", "A>B,:x", "A>B,TOOLONGCALL:x", "A*>B:x",
                          "A>B*:x", "A>B,1,2,3,4,5,6,7,8,9:x", "A B>C:x", "A>B,qAR,N0CALL-15,TCPIP*,!!:x"}) {
    INFO(bad);
    CHECK_FALSE(aprs::parse_tnc2(bad, &f));
  }
  CHECK_FALSE(aprs::parse_tnc2("A>B:" + std::string(257, 'x'), &f));
  CHECK(aprs::parse_tnc2("A>B:" + std::string(256, 'x'), &f));
}

TEST_CASE("APRS parsers fuzz: random text never crashes") {
  Rng rng(13);
  const char* seeds[] = {"!4903.50N/07201.75W-Test", "=/5L!!<*e7>7P[", "`(_fn\"Oj/]\"4T}", ":WU2Z     :Hi{01",
                         "@092345/4903.50N/07201.75W>088/036", ">status", "N0CALL>APRS,WIDE2-1*:!4903.50N/07201.75W-"};
  for (int iter = 0; iter < 4000; ++iter) {
    std::string s = seeds[iter % 7];
    int mode = static_cast<int>(rng.below(3));
    if (mode == 0) {
      s = S(rng.bytes(rng.below(80)));
    } else if (mode == 1) {
      for (int k = 0; k < 3; ++k) s[rng.below(s.size())] = static_cast<char>(rng.byte());
    } else {
      s = s.substr(0, rng.below(s.size() + 1));
    }
    aprs::Packet p = aprs::parse("S32UVT", s);
    if (p.type == aprs::PacketType::Position) {
      CHECK(p.position.lat >= -90.0);
      CHECK(p.position.lat <= 90.0);
      CHECK(p.position.lon >= -180.0);
      CHECK(p.position.lon <= 180.0);
    }
    ax25::Frame f;
    if (aprs::parse_tnc2(s, &f)) CHECK(!ax25::encode(f).empty());
  }
}

}  // TEST_SUITE
