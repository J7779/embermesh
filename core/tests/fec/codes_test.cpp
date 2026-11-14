// Golay, Hamming, interleaver and scrambler tests.
#include <set>
#include <vector>

#include "doctest.h"
#include "fec/golay.h"
#include "fec/hamming.h"
#include "fec/interleave.h"
#include "fec/scrambler.h"
#include "fec/test_rng.h"

using namespace ember;
using namespace ember::fec;
using fec_test::Rng;

namespace {
int popcount32(uint32_t v) {
  int n = 0;
  while (v) { v &= v - 1; ++n; }
  return n;
}
}  // namespace

TEST_SUITE("fec") {

// ---------------------------------------------------------------- Golay

TEST_CASE("Golay(24,12): weight enumerator 1 + 759x^8 + 2576x^12 + 759x^16 + x^24") {
  int w[25] = {0};
  for (uint32_t d = 0; d < 4096; ++d) {
    uint32_t c = golay::encode(static_cast<uint16_t>(d));
    REQUIRE((c >> 24) == 0);
    REQUIRE((c >> 12) == d);  // systematic
    ++w[popcount32(c)];
  }
  CHECK(w[0] == 1);
  CHECK(w[8] == 759);
  CHECK(w[12] == 2576);
  CHECK(w[16] == 759);
  CHECK(w[24] == 1);
  CHECK(w[0] + w[8] + w[12] + w[16] + w[24] == 4096);
  CHECK(golay::encode(0xFFF) == 0xFFFFFF);
  CHECK(golay::encode(0x000) == 0);
  CHECK(golay::encode(0x800) == 0x800DC5);  // first row of G = [I | B]
}

TEST_CASE("Golay: all error patterns of weight <= 3 corrected, all of weight 4 detected") {
  std::vector<uint32_t> pats;
  pats.push_back(0);
  for (int a = 0; a < 24; ++a) {
    pats.push_back(1u << a);
    for (int b = a + 1; b < 24; ++b) {
      pats.push_back(1u << a | 1u << b);
      for (int c = b + 1; c < 24; ++c) pats.push_back(1u << a | 1u << b | 1u << c);
    }
  }
  REQUIRE(pats.size() == 2325);  // 1 + 24 + 276 + 2024
  Rng rng(200);
  for (int iter = 0; iter < 12; ++iter) {
    uint16_t d = static_cast<uint16_t>(iter == 0 ? 0 : rng.below(4096));
    uint32_t c = golay::encode(d);
    for (uint32_t e : pats) {
      uint16_t out = 0xFFFF;
      int r = golay::decode(c ^ e, &out);
      REQUIRE(r == popcount32(e));
      REQUIRE(out == d);
    }
  }
  // every weight-4 pattern on two codewords
  int detected = 0, total = 0;
  for (uint16_t d : {uint16_t(0), uint16_t(0xA5C)}) {
    uint32_t c = golay::encode(d);
    for (int a = 0; a < 24; ++a)
      for (int b = a + 1; b < 24; ++b)
        for (int x = b + 1; x < 24; ++x)
          for (int y = x + 1; y < 24; ++y) {
            uint16_t out = 0x1234;
            ++total;
            if (golay::decode(c ^ (1u << a | 1u << b | 1u << x | 1u << y), &out) == -1 && out == 0x1234) ++detected;
          }
  }
  CHECK(total == 2 * 10626);
  CHECK(detected == total);
  // upper 8 bits are ignored
  uint16_t out;
  CHECK(golay::decode(golay::encode(0x123) | 0xFF000000u, &out) == 0);
  CHECK(out == 0x123);
}

TEST_CASE("Golay byte helpers: sizes, round trip, corrections, truncation") {
  CHECK(golay::encoded_size(0) == 0);
  CHECK(golay::encoded_size(1) == 6);
  CHECK(golay::encoded_size(3) == 6);
  CHECK(golay::encoded_size(4) == 12);
  Rng rng(201);
  for (size_t n : {size_t(0), size_t(1), size_t(2), size_t(3), size_t(4), size_t(100)}) {
    Bytes in = rng.bytes(n);
    Bytes enc = golay::encode_bytes(in);
    CHECK(enc.size() == golay::encoded_size(n));
    Bytes out;
    CHECK(golay::decode_bytes(enc, n, &out) == 0);
    CHECK(out == in);
    if (n == 0) continue;
    // 3 bit errors in every 24-bit codeword
    for (size_t w = 0; w < enc.size(); w += 3)
      for (int k = 0; k < 3; ++k) enc[w + static_cast<size_t>(k)] ^= static_cast<uint8_t>(1u << rng.below(8));
    CHECK(golay::decode_bytes(enc, n, &out) == static_cast<int>(enc.size()));  // 3 per codeword = 1 per byte
    CHECK(out == in);
    CHECK(golay::decode_bytes(ByteView(enc).sub(0, enc.size() - 1), n, &out) == -1);
  }
}

// ---------------------------------------------------------------- Hamming

TEST_CASE("Hamming(7,4) and (8,4): exhaustive single errors, (8,4) double-error detection") {
  std::set<uint8_t> cws;
  for (uint8_t n = 0; n < 16; ++n) {
    uint8_t c7 = hamming::encode74(n);
    uint8_t c8 = hamming::encode84(n);
    CHECK(c7 < 128);
    cws.insert(c8);
    uint8_t out = 0xFF;
    CHECK(hamming::decode74(c7, &out) == 0);
    CHECK(out == n);
    CHECK(hamming::decode84(c8, &out) == 0);
    CHECK(out == n);
    for (int i = 0; i < 7; ++i) {
      CHECK(hamming::decode74(static_cast<uint8_t>(c7 ^ (1 << i)), &out) == 1);
      CHECK(out == n);
    }
    for (int i = 0; i < 8; ++i) {
      CHECK(hamming::decode84(static_cast<uint8_t>(c8 ^ (1 << i)), &out) == 1);
      CHECK(out == n);
      for (int j = i + 1; j < 8; ++j) {
        out = 0xEE;
        CHECK(hamming::decode84(static_cast<uint8_t>(c8 ^ (1 << i) ^ (1 << j)), &out) == -1);
        CHECK(out == 0xEE);
      }
    }
  }
  CHECK(cws.size() == 16);
  // minimum distance 4 for (8,4)
  for (uint8_t a : cws)
    for (uint8_t b : cws)
      if (a != b) CHECK(popcount32(static_cast<uint32_t>(a ^ b)) >= 4);
  Bytes in = {0x00, 0xFF, 0x5A, 0xC3};
  Bytes enc = hamming::encode84_bytes(in);
  CHECK(enc.size() == 8);
  enc[3] ^= 0x10;
  Bytes out;
  CHECK(hamming::decode84_bytes(enc, &out) == 1);
  CHECK(out == in);
  enc[5] ^= 0x03;
  CHECK(hamming::decode84_bytes(enc, &out) == -1);
  CHECK(hamming::decode84_bytes(ByteView(enc).sub(0, 7), &out) == -1);
  CHECK(hamming::decode84_bytes(ByteView(), &out) == 0);
  CHECK(out.empty());
}

// ---------------------------------------------------------------- interleaver

TEST_CASE("Interleaver: permutation for every length/depth, exact inverse, index function") {
  for (size_t n = 0; n <= 130; ++n)
    for (size_t rows = 0; rows <= 17; ++rows) {
      Bytes in(n);
      for (size_t i = 0; i < n; ++i) in[i] = static_cast<uint8_t>(i);
      Bytes il = interleave(in, rows);
      REQUIRE(il.size() == n);
      std::vector<int> seen(n, 0);
      for (size_t i = 0; i < n; ++i) {
        size_t j = interleave_index(i, n, rows);
        REQUIRE(j < n);
        REQUIRE(il[j] == in[i]);
        ++seen[j];
      }
      for (int s : seen) REQUIRE(s == 1);
      REQUIRE(deinterleave(il, rows) == in);
    }
  // known small example: 2 rows x 3 cols: 0 1 2 / 3 4 5 -> 0 3 1 4 2 5
  CHECK(interleave(Bytes{0, 1, 2, 3, 4, 5}, 2) == Bytes({0, 3, 1, 4, 2, 5}));
  // short last row: n=5 rows=2 -> 0 1 2 / 3 4 -> 0 3 1 4 2
  CHECK(interleave(Bytes{0, 1, 2, 3, 4}, 2) == Bytes({0, 3, 1, 4, 2}));
}

TEST_CASE("Interleaver spreads a burst evenly over the rows") {
  const size_t rows = 8, cols = 40, n = rows * cols;
  for (size_t start = 0; start + 24 <= n; start += 7) {
    // burst of 24 consecutive channel symbols
    std::vector<int> per_row(rows, 0);
    for (size_t i = 0; i < n; ++i) {
      size_t j = interleave_index(i, n, rows);
      if (j >= start && j < start + 24) ++per_row[i / cols];
    }
    for (int c : per_row) CHECK(c == 3);
  }
}

// ---------------------------------------------------------------- scramblers

TEST_CASE("PN9 whitening matches the TI CC1101 / SX1231 sequence") {
  Lfsr l = pn9();
  const uint8_t want[8] = {0xFF, 0xE1, 0x1D, 0x9A, 0xED, 0x85, 0x33, 0x24};
  for (uint8_t w : want) CHECK(l.next_byte() == w);
  // period 511 bits
  Lfsr p = pn9();
  uint32_t s0 = p.state();
  int period = 0;
  do {
    p.next_bit();
    ++period;
  } while (p.state() != s0 && period < 1000);
  CHECK(period == 511);
  Rng rng(202);
  Bytes data = rng.bytes(300);
  Bytes w = data;
  whiten_pn9(w.data(), w.size());
  CHECK(w != data);
  whiten_pn9(w.data(), w.size());
  CHECK(w == data);
  // zero seed is replaced (an all-zero LFSR would be stuck)
  Lfsr z(0x21, 9, 0);
  CHECK(z.state() == 0x1FF);
}

TEST_CASE("Self-synchronising G3RUH scrambler: round trip, self-sync, error multiplication") {
  Rng rng(203);
  std::vector<uint8_t> bits(2000);
  for (auto& b : bits) b = static_cast<uint8_t>(rng.below(2));
  SelfSyncScrambler tx, rx;
  std::vector<uint8_t> ch(bits.size());
  for (size_t i = 0; i < bits.size(); ++i) ch[i] = tx.scramble(bits[i]);
  for (size_t i = 0; i < bits.size(); ++i) CHECK(rx.descramble(ch[i]) == bits[i]);
  // receiver starting mid-stream with random state syncs after 17 bits
  SelfSyncScrambler late;
  for (int i = 0; i < 5; ++i) late.descramble(static_cast<uint8_t>(rng.below(2)));
  int wrong_after_sync = 0;
  for (size_t i = 100; i < bits.size(); ++i) {
    uint8_t b = late.descramble(ch[i]);
    if (i >= 117 && b != bits[i]) ++wrong_after_sync;
  }
  CHECK(wrong_after_sync == 0);
  // one channel error -> exactly 3 output errors
  SelfSyncScrambler rx2;
  ch[500] ^= 1;
  int errs = 0;
  for (size_t i = 0; i < bits.size(); ++i) errs += rx2.descramble(ch[i]) != bits[i];
  CHECK(errs == 3);
}

}  // TEST_SUITE
