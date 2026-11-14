#include "doctest.h"
#include "host/json.h"
#include "host/util.h"
#include <algorithm>

using namespace ember_host;

TEST_SUITE("codec") {
  TEST_CASE("sha1 FIPS 180 vectors") {
    CHECK(sha1_hex("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    CHECK(sha1_hex("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK(sha1_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") == "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    std::string m(1000000, 'a');
    CHECK(sha1_hex(m) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
    // Incremental updates across block boundaries give the same digest.
    Sha1 s;
    for (size_t i = 0; i < m.size(); i += 777) s.update(m.data() + i, std::min<size_t>(777, m.size() - i));
    uint8_t d[20];
    s.finish(d);
    CHECK(hex_encode(d, 20) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
    // 55/56/64-byte padding edge cases (cross-checked with shasum).
    CHECK(sha1_hex(std::string(55, 'x')) == "cef734ba81a024479e09eb5a75b6ddae62e6abf1");
    CHECK(sha1_hex(std::string(56, 'x')) == "901305367c259952f4e7af8323f480d59f81335b");
    CHECK(sha1_hex(std::string(64, 'a')) == "0098ba824b5c16427bd7a1122a5a442a25ec644d");
  }

  TEST_CASE("base64 RFC 4648 vectors") {
    const char* plain[] = {"", "f", "fo", "foo", "foob", "fooba", "foobar"};
    const char* enc[] = {"", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy"};
    for (int i = 0; i < 7; ++i) {
      CHECK(base64_encode(plain[i]) == enc[i]);
      std::string out;
      REQUIRE(base64_decode(enc[i], &out));
      CHECK(out == plain[i]);
    }
    std::string bin;
    for (int i = 0; i < 256; ++i) bin += char(i);
    std::string back;
    REQUIRE(base64_decode(base64_encode(bin), &back));
    CHECK(back == bin);
  }

  TEST_CASE("base64 rejects malformed input") {
    std::string o;
    CHECK_FALSE(base64_decode("Zg=", &o));
    CHECK_FALSE(base64_decode("Z", &o));
    CHECK_FALSE(base64_decode("Zm9v!A==", &o));
    CHECK_FALSE(base64_decode("Zm 9v", &o));
    CHECK_FALSE(base64_decode("Zh==", &o));   // non-canonical trailing bits
    CHECK_FALSE(base64_decode("=Zg=", &o));
    CHECK_FALSE(base64_decode("Zg==Zg==", &o));
    CHECK(base64_decode("Zg", &o, true));
    CHECK(o == "f");
    CHECK_FALSE(base64_decode("Z", &o, true));
  }

  TEST_CASE("utf8 validation") {
    CHECK(utf8_valid(""));
    CHECK(utf8_valid("hello"));
    CHECK(utf8_valid("\xc3\xa9\xe2\x82\xac\xf0\x9f\x94\xa5"));  // é € 🔥
    CHECK_FALSE(utf8_valid("\xc0\xaf"));          // overlong '/'
    CHECK_FALSE(utf8_valid("\xe0\x80\xaf"));      // overlong
    CHECK_FALSE(utf8_valid("\xed\xa0\x80"));      // surrogate U+D800
    CHECK_FALSE(utf8_valid("\xf4\x90\x80\x80"));  // > U+10FFFF
    CHECK_FALSE(utf8_valid("\xf5\x80\x80\x80"));
    CHECK_FALSE(utf8_valid("\x80"));
    CHECK_FALSE(utf8_valid("\xc3"));              // truncated
    CHECK_FALSE(utf8_valid("\xe2\x82"));
    CHECK(utf8_valid("\xf4\x8f\xbf\xbf"));        // U+10FFFF
    CHECK(utf8_valid("\xef\xbf\xbf"));
  }

  TEST_CASE("json parse / dump round trip and limits") {
    Json j;
    REQUIRE(Json::parse(R"({"a":1,"b":[true,false,null],"c":"x\"\\\n\u00e9\ud83d\udd25","d":-1.5e3,"e":{}})", &j));
    CHECK(j.get("a").as_int() == 1);
    CHECK(j.get("b").size() == 3);
    CHECK(j.get("c").as_string() == "x\"\\\n\xc3\xa9\xf0\x9f\x94\xa5");
    CHECK(j.get("d").as_number() == -1500);
    Json k;
    REQUIRE(Json::parse(j.dump(), &k));
    CHECK(j == k);
    CHECK(Json(1700000000123.0).dump() == "1700000000123");
    CHECK(Json(0.5).dump() == "0.5");
    const char* bad[] = {"", "{", "[1,]", "{\"a\"}", "01", "1.", "-", "\"\\ud800\"", "\"\x01\"", "nul", "[1] x", "{\"a\":1,}", "\"\\x\""};
    for (const char* b : bad) CHECK_FALSE(Json::parse(b, &k));
    std::string deep(200, '[');
    CHECK_FALSE(Json::parse(deep + std::string(200, ']'), &k));
    CHECK(Json::parse(std::string(60, '[') + std::string(60, ']'), &k));
    CHECK(Json().get("x").is_null());
    CHECK(Json::parse_or_null("{oops").is_null());
  }

  TEST_CASE("string helpers") {
    std::string o;
    CHECK(url_decode("a%20b%2Fc", &o));
    CHECK(o == "a b/c");
    CHECK_FALSE(url_decode("%zz", &o));
    CHECK_FALSE(url_decode("%4", &o));
    CHECK(query_param("a=1&token=x%2By&b", "token") == "x+y");
    bool present = true;
    CHECK(query_param("a=1", "token", &present).empty());
    CHECK_FALSE(present);
    CHECK(header_has_token("keep-alive, Upgrade", "upgrade"));
    CHECK_FALSE(header_has_token("upgrades", "upgrade"));
    CHECK(constant_time_equals("abc", "abc"));
    CHECK_FALSE(constant_time_equals("abc", "abd"));
    CHECK_FALSE(constant_time_equals("abc", "ab"));
    uint64_t v;
    CHECK(parse_u64("18446744073709551615", &v));
    CHECK_FALSE(parse_u64("18446744073709551616", &v));
    CHECK_FALSE(parse_u64("-1", &v));
    CHECK_FALSE(parse_u64("", &v));
  }
}
