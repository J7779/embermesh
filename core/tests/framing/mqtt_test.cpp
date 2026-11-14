// MQTT 3.1.1 codec (framing/mqtt.h): encodings per the OASIS standard, every malformed form the
// relay must refuse (review M16), streaming across arbitrary splits, and seeded mutation fuzzing.
//   ./ember_tests -ts=framing -tc="mqtt long*" --no-skip
#include <string>
#include <vector>

#include "doctest.h"
#include "framing/mqtt.h"

using namespace ember;

namespace {

Bytes H(const char* hex) {
  Bytes b;
  from_hex(hex, b);
  return b;
}

bool parse_all(const Bytes& wire, std::vector<mqtt::Packet>* out, size_t max = 8192) {
  mqtt::Decoder d(max);
  if (!d.feed(ByteView(wire))) return false;
  mqtt::Packet p;
  while (d.next(&p)) out->push_back(p);
  return !d.failed() && d.buffered() == 0;
}

bool rejects(const char* hex) {
  std::vector<mqtt::Packet> v;
  mqtt::Decoder d(8192);
  d.feed(ByteView(H(hex)));
  mqtt::Packet p;
  while (d.next(&p)) v.push_back(p);
  return d.failed();
}

struct Rng {
  uint64_t s;
  uint64_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
  uint32_t below(uint32_t n) { return n ? static_cast<uint32_t>(next() % n) : 0; }
};

void fuzz(uint64_t seed, int iters) {
  Rng r{seed * 2654435761u + 1};
  std::vector<Bytes> corpus = {
      mqtt::connect("", 240), mqtt::connect("x", 60, true, nullptr, nullptr), mqtt::connack(false, 0),
      mqtt::publish("0123456789abcdef0123456789abcdef", ByteView(Bytes(300, 7))), mqtt::subscribe(1, {"a/b", "+/x", "#"}),
      mqtt::suback(1, {0, 0x80}), mqtt::unsubscribe(2, {"a"}), mqtt::unsuback(2), mqtt::pingreq(), mqtt::pingresp(),
      mqtt::disconnect()};
  for (int i = 0; i < iters; ++i) {
    Bytes b = corpus[r.below(static_cast<uint32_t>(corpus.size()))];
    int n = 1 + static_cast<int>(r.below(4));
    for (int k = 0; k < n; ++k) {
      switch (r.below(6)) {
        // Every draw in its own statement: as call arguments their order is unspecified and the
        // stream would differ between compilers (Clang vs GCC/MSVC).
        case 0:
          if (!b.empty()) {
            uint32_t pos = r.below(static_cast<uint32_t>(b.size()));
            b[pos] ^= static_cast<uint8_t>(1u << r.below(8));
          }
          break;
        case 1: {
          uint32_t pos = r.below(static_cast<uint32_t>(b.size() + 1));
          uint8_t v = static_cast<uint8_t>(r.next());
          b.insert(b.begin() + pos, v);
          break;
        }
        case 2: if (!b.empty()) b.erase(b.begin() + r.below(static_cast<uint32_t>(b.size()))); break;
        case 3: if (!b.empty()) b.resize(r.below(static_cast<uint32_t>(b.size()))); break;
        case 4: { const Bytes& o = corpus[r.below(static_cast<uint32_t>(corpus.size()))]; b.insert(b.end(), o.begin(), o.end()); break; }
        default: if (b.size() > 1) b[1] = static_cast<uint8_t>(r.next()); break;  // the length byte
      }
    }
    mqtt::Decoder d(4400);
    size_t off = 0;
    while (off < b.size()) {  // arbitrary splits
      size_t take = 1 + r.below(static_cast<uint32_t>(b.size() - off));
      d.feed(ByteView(b).sub(off, take));
      off += take;
      mqtt::Packet p;
      while (d.next(&p)) {
        CHECK(p.topic.size() <= mqtt::kMaxTopic);
        CHECK(p.payload.size() <= 4400);
        CHECK(p.filters.size() <= mqtt::kMaxFilters);
      }
    }
    CHECK(d.buffered() <= 4400 + 5 + 64 * 1024);
  }
}

}  // namespace

TEST_SUITE("framing") {

TEST_CASE("mqtt: encodings match the standard") {
  CHECK(to_hex(ByteView(mqtt::connect("", 240))) == "100c00044d515454040200f00000");  // empty id, clean, ka 240
  std::string u = "user", pw = "pw";
  CHECK(to_hex(ByteView(mqtt::connect("c", 60, true, &u, &pw))) == "101700044d51545404c2003c000163000475736572000270" "77");
  CHECK(to_hex(ByteView(mqtt::publish("t", ByteView(std::string_view("AB"))))) == "30050001744142");
  CHECK(to_hex(ByteView(mqtt::subscribe(1, {"a/b"}))) == "82080001000361" "2f6200");
  CHECK(to_hex(ByteView(mqtt::unsubscribe(9, {"a"}))) == "a20500090001" "61");
  CHECK(to_hex(ByteView(mqtt::pingreq())) == "c000");
  CHECK(to_hex(ByteView(mqtt::pingresp())) == "d000");
  CHECK(to_hex(ByteView(mqtt::disconnect())) == "e000");
  CHECK(to_hex(ByteView(mqtt::connack(false, 0))) == "20020000");
  CHECK(to_hex(ByteView(mqtt::suback(1, {0, 0x80}))) == "9004000100" "80");
  // a 200-byte publish needs a two-byte remaining length (minimal)
  Bytes big = mqtt::publish("t", ByteView(Bytes(200, 0)));
  CHECK(big[1] == 0xCB);
  CHECK(big[2] == 0x01);
  // invalid arguments encode to nothing
  CHECK(mqtt::publish("a/#", ByteView()).empty());
  CHECK(mqtt::publish("", ByteView()).empty());
  CHECK(mqtt::subscribe(0, {"a"}).empty());
  CHECK(mqtt::subscribe(1, {}).empty());
  CHECK(mqtt::subscribe(1, {"a#"}).empty());
  CHECK(mqtt::connect("c", 60, true, nullptr, &pw).empty());  // password without user name
}

TEST_CASE("mqtt: round trips and streaming across every split") {
  Bytes wire;
  for (const Bytes& p : {mqtt::connack(true, 0), mqtt::publish("0123456789abcdef0123456789abcdef", ByteView(Bytes(600, 9))),
                         mqtt::suback(7, {0, 1, 0x80}), mqtt::pingresp(), mqtt::unsuback(8)})
    wire.insert(wire.end(), p.begin(), p.end());
  for (size_t split = 1; split < wire.size(); split += 7) {
    mqtt::Decoder d(4400);
    std::vector<mqtt::Packet> got;
    for (size_t off = 0; off < wire.size(); off += split) {
      REQUIRE(d.feed(ByteView(wire).sub(off, split)));
      mqtt::Packet p;
      while (d.next(&p)) got.push_back(p);
    }
    REQUIRE(got.size() == 5);
    CHECK(got[0].type == mqtt::Type::Connack);
    CHECK(got[0].session_present);
    CHECK(got[1].type == mqtt::Type::Publish);
    CHECK(got[1].payload == Bytes(600, 9));
    CHECK(got[1].qos == 0);
    CHECK((got[2].codes == std::vector<uint8_t>{0, 1, 0x80}));
    CHECK(got[3].type == mqtt::Type::Pingresp);
    CHECK(got[4].packet_id == 8);
  }
  // server side: CONNECT and SUBSCRIBE parse back
  std::string u = "user", pw = "pw";
  std::vector<mqtt::Packet> v;
  REQUIRE(parse_all(mqtt::connect("abc", 61, true, &u, &pw), &v));
  CHECK(v[0].client_id == "abc");
  CHECK(v[0].keepalive == 61);
  CHECK(v[0].username == "user");
  CHECK(v[0].password == "pw");
  v.clear();
  REQUIRE(parse_all(mqtt::subscribe(3, {"x/+", "#"}), &v));
  REQUIRE(v[0].filters.size() == 2);
  CHECK(v[0].filters[1].first == "#");
}

TEST_CASE("mqtt M16: the decoder refuses every malformed or oversized form") {
  CHECK(rejects("30ffffffff7f"));        // remaining length longer than 4 bytes
  CHECK(rejects("308000"));              // non-minimal remaining length (0 in two bytes)
  CHECK(rejects("0000"));
  CHECK(rejects("f000"));                // reserved type 15
  CHECK(rejects("2003000000"));          // CONNACK of the wrong length
  CHECK(rejects("20020200"));            // CONNACK reserved flags
  CHECK(rejects("21020000"));            // CONNACK with fixed-header flags
  CHECK(rejects("3605000174ff00"));      // PUBLISH QoS 3
  CHECK(rejects("38050001744142"));      // PUBLISH DUP with QoS 0
  CHECK(rejects("3005000123" "4142"));   // PUBLISH topic with a wildcard '#'
  CHECK(rejects("3004000000" "41"));     // PUBLISH empty topic
  CHECK(rejects("3005000100" "4142"));   // PUBLISH topic with U+0000
  CHECK(rejects("30050001ff" "4142"));   // PUBLISH topic not UTF-8
  CHECK(rejects("3203000174"));          // QoS 1 without packet id
  CHECK(rejects("320500017400" "00"));   // QoS 1 with packet id 0
  CHECK(rejects("80080001000361" "2f6200"));  // SUBSCRIBE without the 0010 flags
  CHECK(rejects("82020001"));            // SUBSCRIBE without filters
  CHECK(rejects("8208000100036123" "6200"));  // filter "a#b"
  CHECK(rejects("82080001000361" "2f6203"));  // requested QoS 3
  CHECK(rejects("90020001"));            // SUBACK without codes
  CHECK(rejects("9003000103"));          // SUBACK return code 3
  CHECK(rejects("c001" "00"));           // PINGREQ with a body
  CHECK(rejects("d100"));                // PINGRESP with flags
  CHECK(rejects("b0030001" "00"));       // UNSUBACK wrong length
  CHECK(rejects("60020001"));            // PUBREL must have flags 0010
  CHECK(rejects("10" "0c" "00044d5154540302" "00f00000"));  // CONNECT protocol level 3
  CHECK(rejects("10" "0c" "00044d5154540403" "00f00000"));  // CONNECT reserved flag
  CHECK(rejects("10" "0c" "00044d5154540406" "00f00000"));  // CONNECT with a will
  CHECK(rejects("10" "0c" "00044d5154540442" "00f00000"));  // password without user name
  // a packet larger than the decoder limit is refused before it is buffered
  mqtt::Decoder d(4400);
  Bytes huge = mqtt::publish("t", ByteView(Bytes(5000, 1)));
  d.feed(ByteView(huge).sub(0, 4));
  mqtt::Packet p;
  CHECK_FALSE(d.next(&p));
  CHECK(d.failed());
  // a flood of unparsed bytes is refused
  mqtt::Decoder d2(4400);
  Bytes part = mqtt::publish("t", ByteView(Bytes(4000, 1)));
  part.resize(part.size() - 1);  // never completes
  for (int i = 0; i < 40 && !d2.failed(); ++i) d2.feed(ByteView(part));
  CHECK(d2.failed());
  // failure is sticky
  CHECK_FALSE(d2.feed(ByteView(mqtt::pingresp())));
}

TEST_CASE("mqtt: topic validation") {
  CHECK(mqtt::valid_topic("0123456789abcdef0123456789abcdef", false));
  CHECK_FALSE(mqtt::valid_topic("a/+", false));
  CHECK(mqtt::valid_topic("a/+", true));
  CHECK(mqtt::valid_topic("+", true));
  CHECK(mqtt::valid_topic("#", true));
  CHECK(mqtt::valid_topic("a/#", true));
  CHECK_FALSE(mqtt::valid_topic("a#", true));
  CHECK_FALSE(mqtt::valid_topic("a/#/b", true));
  CHECK_FALSE(mqtt::valid_topic("a+/b", true));
  CHECK_FALSE(mqtt::valid_topic("", true));
  CHECK_FALSE(mqtt::valid_topic(std::string(257, 'a'), false));
}

TEST_CASE("mqtt: mutation fuzz (fast)") { fuzz(1, 3000); }

TEST_CASE("mqtt long: mutation fuzz" * doctest::skip()) {
  for (uint64_t s = 10; s < 30; ++s) fuzz(s, 50000);
}

}  // TEST_SUITE
