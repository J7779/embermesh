#include <algorithm>
#include <vector>

#include "doctest.h"
#include "fec/gf256.h"
#include "fec/reed_solomon.h"
#include "fec/test_rng.h"

using namespace ember;
using namespace ember::fec;
using fec_test::Rng;

namespace {

// k distinct positions in [0, n)
std::vector<size_t> positions(Rng& rng, size_t n, size_t k) {
  std::vector<size_t> all(n);
  for (size_t i = 0; i < n; ++i) all[i] = i;
  for (size_t i = 0; i < k; ++i) std::swap(all[i], all[i + rng.below(n - i)]);
  all.resize(k);
  return all;
}

void corrupt(Bytes& cw, const std::vector<size_t>& pos, Rng& rng) {
  for (size_t p : pos) cw[p] ^= static_cast<uint8_t>(1 + rng.below(255));  // never a no-op
}

}  // namespace

TEST_SUITE("fec") {

TEST_CASE("GF(256) tables: field axioms") {
  CHECK(gf256::exp(0) == 1);
  CHECK(gf256::exp(1) == 2);
  CHECK(gf256::exp(8) == 0x1D);  // x^8 = x^4+x^3+x^2+1
  CHECK(gf256::exp(255) == 1);
  for (int a = 1; a < 256; ++a) {
    uint8_t x = static_cast<uint8_t>(a);
    CHECK(gf256::mul(x, gf256::inv(x)) == 1);
    CHECK(gf256::exp(gf256::log(x)) == x);
    CHECK(gf256::div(gf256::mul(x, 0x53), 0x53) == x);
  }
  CHECK(gf256::mul(0, 7) == 0);
  CHECK(gf256::pow(3, 0) == 1);
  CHECK(gf256::pow(0, 0) == 1);
  CHECK(gf256::pow(0, 5) == 0);
  CHECK(gf256::pow(2, 8) == 0x1D);
}

TEST_CASE("RS parity matches the ISO 18004 QR code example (HELLO WORLD 1-M, 10 EC codewords)") {
  ReedSolomon rs(10, 0);
  REQUIRE(rs.valid());
  Bytes data = {32, 91, 11, 120, 209, 114, 220, 77, 67, 64, 236, 17, 236, 17, 236, 17};
  Bytes want = {196, 35, 39, 119, 235, 215, 231, 226, 93, 23};
  uint8_t parity[10];
  REQUIRE(rs.encode(data, parity));
  CHECK(Bytes(parity, parity + 10) == want);
  Bytes cw = rs.encode(data);
  CHECK(rs.check(cw));
  // QR generator polynomial for 10 EC codewords (alpha exponents 251,67,46,61,118,70,64,94,32,45)
  const unsigned g_exp[10] = {45, 32, 94, 64, 70, 118, 61, 46, 67, 251};  // x^0 .. x^9
  for (size_t i = 0; i < 10; ++i) CHECK(rs.generator(i) == gf256::exp(g_exp[i]));
  CHECK(rs.generator(10) == 1);
  // correct 5 errors in it
  Rng rng(100);
  corrupt(cw, {0, 3, 17, 20, 25}, rng);
  CHECK(rs.decode(cw) == 5);
  CHECK(Bytes(cw.begin(), cw.begin() + 16) == data);
}

TEST_CASE("RS invalid parameters and arguments") {
  CHECK_FALSE(ReedSolomon(0).valid());
  CHECK_FALSE(ReedSolomon(255).valid());
  CHECK(ReedSolomon(254).valid());
  ReedSolomon bad(0);
  uint8_t p[4];
  CHECK_FALSE(bad.encode(Bytes{1}, p));
  ReedSolomon rs(16);
  CHECK(rs.max_data() == 239);
  CHECK_FALSE(rs.encode(ByteView(), p));
  CHECK(rs.encode(Bytes(240, 1)).empty());  // max+1
  CHECK(rs.encode(Bytes(239, 1)).size() == 255);
  Bytes cw = rs.encode(Bytes{1, 2, 3});
  CHECK(rs.decode(cw.data(), 16) == -1);   // len <= nroots
  CHECK(rs.decode(cw.data(), 256) == -1);  // len > 255
  CHECK(rs.decode(nullptr, 19) == -1);
  cw[0] ^= 1;
  size_t dup[2] = {1, 1};
  CHECK(rs.decode(cw, dup, 2) == -1);
  size_t oor[1] = {19};
  CHECK(rs.decode(cw, oor, 1) == -1);
  CHECK(rs.decode(cw, nullptr, 3) == -1);
  CHECK(rs.decode(cw) == 1);
}

TEST_CASE("RS(255,223): corrects exactly t=16 errors, fails cleanly at 17") {
  ReedSolomon rs(32);
  Rng rng(101);
  for (int iter = 0; iter < 40; ++iter) {
    Bytes data = rng.bytes(223);
    Bytes cw = rs.encode(data);
    REQUIRE(cw.size() == 255);
    Bytes bad = cw;
    corrupt(bad, positions(rng, 255, 16), rng);
    CHECK(rs.decode(bad) == 16);
    CHECK(bad == cw);
    bad = cw;
    corrupt(bad, positions(rng, 255, 17), rng);
    Bytes before = bad;
    int r = rs.decode(bad);
    CHECK(r == -1);
    CHECK(bad == before);  // untouched on failure
  }
}

TEST_CASE("RS erasures: up to nroots erasures, mixed 2e+f <= nroots, and beyond") {
  ReedSolomon rs(32);
  Rng rng(102);
  for (int iter = 0; iter < 30; ++iter) {
    Bytes cw = rs.encode(rng.bytes(200));
    const size_t n = cw.size();
    // 32 erasures
    auto pos = positions(rng, n, 32);
    Bytes bad = cw;
    corrupt(bad, pos, rng);
    CHECK(rs.decode(bad, pos.data(), pos.size()) == 32);
    CHECK(bad == cw);
    // 33 erasures: too many
    pos = positions(rng, n, 33);
    bad = cw;
    corrupt(bad, pos, rng);
    CHECK(rs.decode(bad, pos.data(), pos.size()) == -1);
    // e errors + f erasures with 2e+f = 32
    size_t e = rng.below(17);
    size_t f = 32 - 2 * e;
    pos = positions(rng, n, e + f);
    bad = cw;
    corrupt(bad, pos, rng);
    CHECK(rs.decode(bad, pos.data() + e, f) == static_cast<int>(e + f));
    CHECK(bad == cw);
    // erasure flags on correct symbols are fine (magnitude 0 -> not counted)
    pos = positions(rng, n, 20);
    bad = cw;
    std::vector<size_t> err(pos.begin(), pos.begin() + 5);
    corrupt(bad, err, rng);
    CHECK(rs.decode(bad, pos.data(), pos.size()) == 5);
    CHECK(bad == cw);
    // 2e+f = 33 must not succeed with the original
    e = 1 + rng.below(16);
    f = 33 - 2 * e;
    pos = positions(rng, n, e + f);
    bad = cw;
    corrupt(bad, pos, rng);
    int r = rs.decode(bad, pos.data() + e, f);
    if (r >= 0) CHECK(rs.check(bad));  // a mis-correction must still be a valid codeword
    CHECK(bad != cw);
  }
}

TEST_CASE("RS shortened codes and small parity counts") {
  Rng rng(103);
  for (size_t nroots : {size_t(2), size_t(4), size_t(8), size_t(16), size_t(32), size_t(64)}) {
    for (uint8_t fcr : {uint8_t(0), uint8_t(1)}) {
      ReedSolomon rs(nroots, fcr);
      for (size_t k : {size_t(1), size_t(2), size_t(10), size_t(50), rs.max_data()}) {
        Bytes data = rng.bytes(k);
        Bytes cw = rs.encode(data);
        REQUIRE(cw.size() == k + nroots);
        CHECK(rs.decode(cw) == 0);
        size_t t = nroots / 2;
        Bytes bad = cw;
        corrupt(bad, positions(rng, cw.size(), t), rng);
        CHECK(rs.decode(bad) == static_cast<int>(t));
        CHECK(bad == cw);
        bad = cw;
        corrupt(bad, positions(rng, cw.size(), std::min(cw.size(), t + 1)), rng);
        int r = rs.decode(bad);
        if (r >= 0) {  // possible for small nroots: lands on another codeword
          CHECK(rs.check(bad));
          CHECK(bad != cw);
        }
      }
    }
  }
}

TEST_CASE("RS exhaustive: every single-symbol error of RS(5,3) and every error pair of RS(8,4)") {
  ReedSolomon rs2(2);
  Bytes cw = rs2.encode(Bytes{0x12, 0x34, 0x56});
  for (size_t p = 0; p < cw.size(); ++p)
    for (int v = 1; v < 256; ++v) {
      Bytes bad = cw;
      bad[p] ^= static_cast<uint8_t>(v);
      REQUIRE(rs2.decode(bad) == 1);
      REQUIRE(bad == cw);
    }
  ReedSolomon rs4(4);
  cw = rs4.encode(Bytes{0xDE, 0xAD, 0xBE, 0xEF});
  int ok = 0;
  for (size_t p = 0; p < cw.size(); ++p)
    for (size_t q = p + 1; q < cw.size(); ++q)
      for (int v = 1; v < 256; v += 17)
        for (int w = 1; w < 256; w += 13) {
          Bytes bad = cw;
          bad[p] ^= static_cast<uint8_t>(v);
          bad[q] ^= static_cast<uint8_t>(w);
          ok += rs4.decode(bad) == 2 && bad == cw;
        }
  CHECK(ok == 28 * 15 * 20);
  // nroots = 1: detects one error, corrects one erasure
  ReedSolomon rs1(1);
  cw = rs1.encode(Bytes{1, 2, 3});
  Bytes bad = cw;
  bad[1] ^= 0x40;
  CHECK(rs1.decode(bad) == -1);
  size_t er = 1;
  CHECK(rs1.decode(bad, &er, 1) == 1);
  CHECK(bad == cw);
}

TEST_CASE("RS fuzz: random words never crash, successes are always codewords") {
  Rng rng(104);
  ReedSolomon rs(16);
  int succ = 0;
  for (int iter = 0; iter < 2000; ++iter) {
    size_t len = 17 + rng.below(239);
    Bytes w = rng.bytes(len);
    size_t ne = rng.below(4);
    std::vector<size_t> er;
    for (size_t i = 0; i < ne; ++i) er.push_back(rng.below(len + 2));  // may be out of range / dup
    int r = rs.decode(w, er.data(), er.size());
    if (r >= 0) {
      ++succ;
      CHECK(rs.check(w));
    }
  }
  CHECK(succ < 20);  // random words are almost never within distance 8 of a codeword
}

}  // TEST_SUITE
