#include <string>

#include "base/codec.h"
#include "base/log.h"
#include "base/textpack.h"
#include "doctest.h"

using namespace ember;

namespace {
Bytes B(std::string_view s) { return ByteView(s).to_bytes(); }
}  // namespace

TEST_SUITE("base") {

TEST_CASE("base64 RFC 4648 vectors") {
  const char* plain[] = {"", "f", "fo", "foo", "foob", "fooba", "foobar"};
  const char* enc[] = {"", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy"};
  for (int i = 0; i < 7; ++i) {
    CHECK(base64_encode(B(plain[i])) == enc[i]);
    Bytes out;
    CHECK(base64_decode(enc[i], out));
    CHECK(out == B(plain[i]));
    // unpadded also accepted
    std::string unpadded = enc[i];
    while (!unpadded.empty() && unpadded.back() == '=') unpadded.pop_back();
    CHECK(base64_decode(unpadded, out));
    CHECK(out == B(plain[i]));
    CHECK(base64url_encode(B(plain[i])) == unpadded);
  }
  Bytes bin = {0xFB, 0xFF, 0xBF};
  CHECK(base64_encode(bin) == "+/+/");
  CHECK(base64url_encode(bin) == "-_-_");
  Bytes out;
  CHECK(base64url_decode("-_-_", out));
  CHECK(out == bin);
  CHECK_FALSE(base64_decode("-_-_", out));
  CHECK_FALSE(base64url_decode("+/+/", out));
}

TEST_CASE("base64 rejects malformed input") {
  Bytes out;
  for (const char* bad : {"Z", "Zg=", "Zg===", "Z===", "Zm9v=", "Zh==", "Zm9=", "Zm 9v", "Zm9v\n",
                          "====", "Zg==Zg==", "Zm9!"}) {
    INFO(bad);
    CHECK_FALSE(base64_decode(bad, out));
    CHECK(out.empty());
  }
  CHECK(base64_decode_lenient("Zm9v\nYmFy\r\n", out));
  CHECK(out == B("foobar"));
  CHECK(base64_decode_lenient("-_-_", out));
  // Round trip all lengths and byte values.
  Bytes all;
  for (int i = 0; i < 300; ++i) {
    all.push_back(static_cast<uint8_t>(i * 7));
    Bytes back;
    CHECK(base64_decode(base64_encode(all), back));
    CHECK(back == all);
    CHECK(base64url_decode(base64url_encode(all), back));
    CHECK(back == all);
  }
}

TEST_CASE("base32 Crockford") {
  CHECK(base32_encode(Bytes{}) == "");
  CHECK(base32_encode(B("f")) == "CR");
  CHECK(base32_encode(B("foobar")) == "CSQPYRK1E8");
  Bytes out;
  CHECK(base32_decode("CSQPYRK1E8", out));
  CHECK(out == B("foobar"));
  CHECK(base32_decode("csqp-yrk1-e8", out));  // lower case, separators
  CHECK(out == B("foobar"));
  CHECK(base32_decode("CSQPYRKIE8", out));  // I -> 1
  CHECK(out == B("foobar"));
  CHECK(base32_decode("CSQPYRKLE8", out));  // L -> 1
  CHECK(out == B("foobar"));
  CHECK_FALSE(base32_decode("CSQPYRK1EU", out));  // U invalid
  CHECK_FALSE(base32_decode("C", out));           // impossible length
  CHECK_FALSE(base32_decode("CS", out));          // non-zero trailing bits
  CHECK_FALSE(base32_decode("C*", out));
  Bytes all;
  for (int i = 0; i < 100; ++i) {
    all.push_back(static_cast<uint8_t>(255 - i * 3));
    Bytes back;
    CHECK(base32_decode(base32_encode(all), back));
    CHECK(back == all);
  }
}

TEST_CASE("textpack round trip and compression ratio") {
  const char* samples[] = {
      "",
      "a",
      "ok",
      "Are you OK? We are at the north camp, water is low.",
      "Meet at the bridge at 14:00. Bring the radio and spare batteries!",
      "need medical help, two injured, one with a broken leg",
      "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82",  // Cyrillic
  };
  for (const char* s : samples) {
    Bytes p = textpack_compress(s);
    CHECK(p.size() <= std::strlen(s) + 1);
    std::string back;
    REQUIRE(textpack_decompress(p, 4096, back));
    CHECK(back == s);
  }
  std::string english = "Are you OK? We are at the north camp, water is low and we need help.";
  Bytes p = textpack_compress(english);
  CHECK(p[0] == kTextPackHuffman);
  CHECK(p.size() * 100 < english.size() * 70);  // at least 30 % smaller
  // binary data is stored raw
  std::string bin;
  for (int i = 0; i < 256; ++i) bin.push_back(static_cast<char>(i));
  Bytes pb = textpack_compress(bin);
  CHECK(pb[0] == kTextPackRaw);
  std::string back;
  CHECK(textpack_decompress(pb, 1000, back));
  CHECK(back == bin);
  CHECK_FALSE(textpack_decompress(pb, 100, back));  // over the limit
}

TEST_CASE("textpack hostile input") {
  std::string out;
  CHECK_FALSE(textpack_decompress(Bytes{}, 100, out));
  CHECK_FALSE(textpack_decompress(Bytes{7}, 100, out));
  CHECK_FALSE(textpack_decompress(Bytes{1}, 100, out));                  // no length
  CHECK_FALSE(textpack_decompress(Bytes{1, 0xFF, 0xFF, 0xFF, 0x0F}, 1u << 30, out));  // huge length, no data
  CHECK_FALSE(textpack_decompress(Bytes{1, 50, 0x00}, 100, out));        // too few bits
  uint64_t s = 12345;
  auto rnd = [&]() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; };
  Bytes good = textpack_compress("the quick brown fox jumps over the lazy dog");
  for (int i = 0; i < 5000; ++i) {
    Bytes b = good;
    b[rnd() % b.size()] ^= static_cast<uint8_t>(1u << (rnd() % 8));
    if (rnd() % 3 == 0) b.resize(rnd() % b.size());
    if (textpack_decompress(b, 200, out)) CHECK(out.size() <= 200);
    Bytes r(rnd() % 40);
    for (auto& x : r) x = static_cast<uint8_t>(rnd());
    if (textpack_decompress(r, 64, out)) CHECK(out.size() <= 64);
  }
}

TEST_CASE("log hook") {
  struct Cap { int n = 0; std::string last; LogLevel lvl = LogLevel::Debug; } cap;
  Log log;
  log.info("t", "dropped %d", 1);  // no sink: no-op
  log.set_sink([](void* c, LogLevel l, const char* tag, const char* msg) {
    auto* cp = static_cast<Cap*>(c);
    cp->n++;
    cp->last = std::string(tag) + ":" + msg;
    cp->lvl = l;
  }, &cap, LogLevel::Info);
  log.debug("t", "hidden");
  CHECK(cap.n == 0);
  log.warn("mesh", "x=%d %s", 5, "y");
  CHECK(cap.n == 1);
  CHECK(cap.last == "mesh:x=5 y");
  CHECK(cap.lvl == LogLevel::Warn);
  std::string longs(1000, 'a');
  log.error("t", "%s", longs.c_str());  // truncated, not overflowed
  CHECK(cap.last.size() < 500);
  CHECK(std::string(log_level_name(LogLevel::Error)) == "error");
}

}  // TEST_SUITE
