#include "doctest.h"

#include "ember/bytes.h"
#include "ember/crc.h"

using namespace ember;

TEST_SUITE("base") {

TEST_CASE("ByteView sub is clamped") {
  Bytes b = {1, 2, 3, 4, 5};
  ByteView v(b);
  CHECK(v.sub(1, 2).to_bytes() == Bytes{2, 3});
  CHECK(v.sub(3).to_bytes() == Bytes{4, 5});
  CHECK(v.sub(3, 100).size() == 2);
  CHECK(v.sub(5).empty());
  CHECK(v.sub(99, 1).empty());
  CHECK(ByteView(nullptr, 10).size() == 0);
  CHECK(ByteView() == ByteView(nullptr, 0));
}

TEST_CASE("ByteWriter / ByteReader round trip") {
  ByteWriter w;
  w.u8(0xAB).u16be(0x1234).u32be(0xDEADBEEF).u64be(0x0102030405060708ULL);
  w.u16le(0x1234).u32le(0xDEADBEEF).u64le(0x0102030405060708ULL);
  w.varint(0).varint(127).varint(128).varint(300).varint(UINT64_MAX).str("hi");
  ByteReader r{ByteView(w.data())};
  CHECK(r.u8() == 0xAB);
  CHECK(r.u16be() == 0x1234);
  CHECK(r.u32be() == 0xDEADBEEF);
  CHECK(r.u64be() == 0x0102030405060708ULL);
  CHECK(r.u16le() == 0x1234);
  CHECK(r.u32le() == 0xDEADBEEF);
  CHECK(r.u64le() == 0x0102030405060708ULL);
  CHECK(r.varint() == 0);
  CHECK(r.varint() == 127);
  CHECK(r.varint() == 128);
  CHECK(r.varint() == 300);
  CHECK(r.varint() == UINT64_MAX);
  CHECK(r.bytes(2).to_string() == "hi");
  CHECK(r.ok());
  CHECK(r.at_end());
}

TEST_CASE("ByteReader failure is sticky and never over-reads") {
  Bytes b = {1, 2, 3};
  ByteReader r{ByteView(b)};
  CHECK(r.u16be() == 0x0102);
  CHECK(r.u32be() == 0);  // only one byte left
  CHECK_FALSE(r.ok());
  CHECK(r.u8() == 0);  // sticky: the remaining byte is not handed out after a failure
  CHECK(r.remaining() == 0);
  CHECK(r.bytes(1).empty());
  CHECK_FALSE(r.skip(0));

  ByteReader empty{ByteView()};
  CHECK(empty.at_end());
  CHECK(empty.rest().empty());
  CHECK(empty.ok());
  CHECK(empty.u8() == 0);
  CHECK_FALSE(empty.ok());
}

TEST_CASE("varint rejects truncation and overflow") {
  Bytes truncated = {0x80, 0x80};
  ByteReader r1{ByteView(truncated)};
  r1.varint();
  CHECK_FALSE(r1.ok());

  Bytes too_long(11, 0x80);
  too_long.back() = 0x01;
  ByteReader r2{ByteView(too_long)};
  r2.varint();
  CHECK_FALSE(r2.ok());

  // 10th byte may only contribute bit 63.
  Bytes overflow = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02};
  ByteReader r3{ByteView(overflow)};
  r3.varint();
  CHECK_FALSE(r3.ok());
}

TEST_CASE("hex") {
  Bytes b = {0x00, 0x0f, 0xa5, 0xff};
  CHECK(to_hex(b) == "000fa5ff");
  Bytes out;
  CHECK(from_hex("000FA5ff", out));
  CHECK(out == b);
  CHECK(from_hex("", out));
  CHECK(out.empty());
  CHECK_FALSE(from_hex("abc", out));
  CHECK_FALSE(from_hex("zz", out));
  CHECK_FALSE(from_hex("0x12", out));
}

TEST_CASE("ct_equal") {
  Bytes a = {1, 2, 3}, b = {1, 2, 3}, c = {1, 2, 4}, d = {1, 2};
  CHECK(ct_equal(a, b));
  CHECK_FALSE(ct_equal(a, c));
  CHECK_FALSE(ct_equal(a, d));
  CHECK(ct_equal(ByteView(), ByteView()));
}

TEST_CASE("CRC check values") {
  ByteView v{std::string_view("123456789")};
  CHECK(crc16_x25_of(v) == 0x906E);
  CHECK(crc16_ccitt(v) == 0x29B1);
  CHECK(crc32(v) == 0xCBF43926u);
  CHECK(crc8(v) == 0xF4);
  CHECK(crc32(ByteView()) == 0);
  // Streaming CRC-32 equals one-shot.
  uint32_t c = crc32(v.sub(0, 4));
  c = crc32(v.sub(4), c);
  CHECK(c == 0xCBF43926u);
}

}  // TEST_SUITE
