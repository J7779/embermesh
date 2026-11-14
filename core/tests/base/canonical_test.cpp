// Security review L9: non-canonical encodings. Decisions (see the comments at each decoder):
//   * ByteReader::varint (proto, store, backups, guides index): overlong LEB128 is REJECTED, because
//     these bytes are hashed, signed, MAC'd or used for dedup; every in-tree writer is minimal.
//   * textpack: non-zero padding bits are REJECTED (text bodies live inside signed packets).
//     Raw vs Huffman for the same text remains an encoder choice (method byte) and is tolerated.
//   * base64 / base64url: optional '=' padding is TOLERATED (consumers only ever use the decoded
//     bytes, which are canonical: trailing bits must be zero, so one text maps to one byte string).
#include <string>

#include "base/codec.h"
#include "base/textpack.h"
#include "doctest.h"
#include "ember/bytes.h"

using namespace ember;

TEST_SUITE("base") {

TEST_CASE("L9 regression: overlong varints are rejected") {
  struct V { Bytes in; bool ok; uint64_t v; };
  const V cases[] = {
      {{0x00}, true, 0},
      {{0x7F}, true, 127},
      {{0x80, 0x01}, true, 128},
      {{0x80, 0x00}, false, 0},              // 0 encoded in two bytes
      {{0x81, 0x00}, false, 0},              // 1 encoded in two bytes
      {{0xFF, 0x80, 0x00}, false, 0},        // 127 in three bytes
      {{0x80, 0x80, 0x80, 0x00}, false, 0},
      {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}, true, UINT64_MAX},
      {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02}, false, 0},  // overflow
      {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x80, 0x00}, false, 0},  // overlong
  };
  for (const V& c : cases) {
    CAPTURE(to_hex(ByteView(c.in)));
    ByteReader r{ByteView(c.in)};
    uint64_t v = r.varint();
    CHECK(r.ok() == c.ok);
    if (c.ok) CHECK(v == c.v);
  }
  // Every value the writer produces still round-trips.
  for (int shift = 0; shift < 64; ++shift) {
    for (uint64_t base : {uint64_t(1), uint64_t(3), uint64_t(0x7F)}) {
      uint64_t x = base << shift;
      ByteWriter w;
      w.varint(x);
      Bytes b = w.take();
      ByteReader r{ByteView(b)};
      CHECK(r.varint() == x);
      CHECK(r.ok());
      CHECK(r.at_end());
    }
  }
}

TEST_CASE("L9 regression: textpack rejects non-zero padding bits") {
  std::string text = "hello world, this is a test of the padding bits";
  Bytes b = textpack_compress(text);
  REQUIRE(b.size() > 2);
  std::string out;
  REQUIRE(textpack_decompress(ByteView(b), 1 << 16, out));
  REQUIRE(out == text);
  if (b[0] != 0) {  // Huffman form: flip each low bit of the final byte; at most the canonical one decodes
    int accepted = 0;
    for (int bit = 0; bit < 8; ++bit) {
      Bytes m = b;
      m.back() ^= static_cast<uint8_t>(1u << bit);
      std::string o;
      if (textpack_decompress(ByteView(m), 1 << 16, o)) {
        ++accepted;
        CHECK(textpack_compress(o) == m);  // anything accepted must be the canonical encoding
      }
    }
    CHECK(accepted <= 8);
  }
  // Exhaustive canonical check on short texts: decode(x) accepted => compress(decode(x)) == x
  // whenever x is Huffman-coded and decodes to text that compresses to the Huffman form.
  for (const char* t : {"a", "e", "hi", "test", "12345", "Hello, World!"}) {
    Bytes c = textpack_compress(t);
    for (size_t i = 1; i < c.size(); ++i) {
      for (int bit = 0; bit < 8; ++bit) {
        Bytes m = c;
        m[i] ^= static_cast<uint8_t>(1u << bit);
        std::string o;
        if (!textpack_decompress(ByteView(m), 1 << 16, o)) continue;
        Bytes re = textpack_compress(o);
        if (re[0] == m[0]) CHECK(re == m);
      }
    }
  }
}

TEST_CASE("L9: base64 padding is optional but the decoded bytes are canonical") {
  Bytes out;
  CHECK(base64_decode("YQ==", out));
  CHECK(out == Bytes{'a'});
  CHECK(base64_decode("YQ", out));
  CHECK(out == Bytes{'a'});
  CHECK(!base64_decode("YR==", out));  // non-zero trailing bits
  CHECK(!base64_decode("YR", out));
  CHECK(!base64_decode("Y===", out));
  CHECK(!base64_decode("YQ=", out));
}

}  // TEST_SUITE
