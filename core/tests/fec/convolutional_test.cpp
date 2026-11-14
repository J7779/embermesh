#include <cmath>
#include <string>
#include <vector>

#include "doctest.h"
#include "fec/convolutional.h"
#include "fec/test_rng.h"

using namespace ember;
using namespace ember::fec;
using fec_test::Rng;

namespace {

using Bits = std::vector<uint8_t>;

Bits random_bits(Rng& rng, size_t n) {
  Bits b(n);
  for (auto& x : b) x = static_cast<uint8_t>(rng.below(2));
  return b;
}

size_t diff(const Bits& a, const Bits& b) {
  size_t n = 0;
  for (size_t i = 0; i < a.size() && i < b.size(); ++i) n += a[i] != b[i];
  return n;
}

double gauss(Rng& rng) {
  double u1 = rng.uniform(), u2 = rng.uniform();
  if (u1 < 1e-300) u1 = 1e-300;
  return std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
}

// BPSK over AWGN at Eb/N0 (dB) for a code of rate r, quantised to 8-bit soft symbols.
std::vector<uint8_t> awgn_soft(const Bits& sym, double ebn0_db, double rate, Rng& rng, size_t* raw_errors) {
  double esn0 = std::pow(10.0, ebn0_db / 10.0) * rate;
  double sigma = std::sqrt(1.0 / (2.0 * esn0));
  std::vector<uint8_t> out(sym.size());
  *raw_errors = 0;
  for (size_t i = 0; i < sym.size(); ++i) {
    double x = (sym[i] ? 1.0 : -1.0) + sigma * gauss(rng);
    if ((x > 0) != (sym[i] != 0)) ++*raw_errors;
    double q = 128.0 + x * 48.0;  // ~2.7 sigma headroom at 4 dB
    out[i] = static_cast<uint8_t>(q < 0 ? 0 : (q > 255 ? 255 : q));
  }
  return out;
}

const conv::Termination kTerms[] = {conv::Termination::Flushed, conv::Termination::TailBiting};
const conv::Rate kRates[] = {conv::Rate::R1_2, conv::Rate::R2_3, conv::Rate::R3_4};

}  // namespace

TEST_SUITE("fec") {

TEST_CASE("Convolutional K=7 impulse response is 1111001 / 1011011 (171/133 octal)") {
  Bits one = {1};
  Bits enc = conv::encode(one, conv::Termination::Flushed);
  REQUIRE(enc.size() == 14);
  Bits c0, c1;
  for (size_t i = 0; i < 7; ++i) {
    c0.push_back(enc[2 * i]);
    c1.push_back(enc[2 * i + 1]);
  }
  CHECK(c0 == Bits({1, 1, 1, 1, 0, 0, 1}));
  CHECK(c1 == Bits({1, 0, 1, 1, 0, 1, 1}));
  // linearity: all-zero input gives all-zero output
  Bits z(50, 0);
  for (uint8_t s : conv::encode(z, conv::Termination::Flushed)) CHECK(s == 0);
}

TEST_CASE("Convolutional encoded lengths and puncturing patterns") {
  CHECK(conv::encoded_length(100, conv::Termination::Flushed) == 212);
  CHECK(conv::encoded_length(100, conv::Termination::TailBiting) == 200);
  CHECK(conv::encoded_length(100, conv::Termination::Flushed, conv::Rate::R2_3) == 159);
  CHECK(conv::encoded_length(100, conv::Termination::TailBiting, conv::Rate::R2_3) == 150);
  CHECK(conv::encoded_length(100, conv::Termination::Flushed, conv::Rate::R3_4) == 142);
  CHECK(conv::encoded_length(99, conv::Termination::TailBiting, conv::Rate::R3_4) == 132);
  Rng rng(300);
  Bits b = random_bits(rng, 30);
  Bits full = conv::encode(b, conv::Termination::Flushed);
  Bits p23 = conv::encode(b, conv::Termination::Flushed, conv::Rate::R2_3);
  Bits p34 = conv::encode(b, conv::Termination::Flushed, conv::Rate::R3_4);
  // rate 2/3 keeps c0 c1 c0 (drops c1 of every second bit)
  for (size_t i = 0, j = 0; i < full.size(); ++i)
    if (i % 4 != 3) CHECK(p23[j++] == full[i]);
  // rate 3/4 keeps c0 c1 . c1 c0 .
  for (size_t i = 0, j = 0; i < full.size(); ++i)
    if (i % 6 != 2 && i % 6 != 5) CHECK(p34[j++] == full[i]);
  CHECK(conv::encode(nullptr, 0, conv::Termination::Flushed).empty());
}

TEST_CASE("Viterbi: clean round trip for all terminations / rates and odd lengths") {
  Rng rng(301);
  for (auto term : kTerms)
    for (auto rate : kRates)
      for (size_t n : {size_t(1), size_t(2), size_t(6), size_t(7), size_t(13), size_t(64), size_t(1000)}) {
        Bits b = random_bits(rng, n);
        Bits enc = conv::encode(b, term, rate);
        REQUIRE(enc.size() == conv::encoded_length(n, term, rate));
        Bits out(n, 9);
        int e = conv::decode_hard(enc.data(), enc.size(), n, term, rate, out.data());
        INFO("n=" << n << " term=" << int(term) << " rate=" << int(rate));
        CHECK(e == 0);
        CHECK(out == b);
      }
}

TEST_CASE("Viterbi: invalid arguments") {
  Bits out(10);
  Bits enc(32);  // 10 bits + 6 tail -> 32 symbols
  CHECK(conv::decode_hard(enc.data(), 31, 10, conv::Termination::Flushed, conv::Rate::R1_2, out.data()) == -1);
  CHECK(conv::decode_hard(enc.data(), 32, 0, conv::Termination::Flushed, conv::Rate::R1_2, out.data()) == -1);
  CHECK(conv::decode_hard(nullptr, 32, 10, conv::Termination::Flushed, conv::Rate::R1_2, out.data()) == -1);
  CHECK(conv::decode_hard(enc.data(), 32, 10, conv::Termination::Flushed, conv::Rate::R1_2, nullptr) == -1);
  CHECK(conv::decode_hard(enc.data(), 32, 10, conv::Termination::Flushed, conv::Rate::R1_2, out.data()) == 0);
}

TEST_CASE("Viterbi hard decision: isolated errors are always corrected and counted") {
  Rng rng(302);
  for (auto term : kTerms) {
    for (int iter = 0; iter < 20; ++iter) {
      Bits b = random_bits(rng, 400);
      Bits enc = conv::encode(b, term);
      size_t flips = 0;
      for (size_t i = rng.below(10); i < enc.size(); i += 25 + rng.below(10)) {
        enc[i] ^= 1;
        ++flips;
      }
      Bits out(b.size());
      int e = conv::decode_hard(enc.data(), enc.size(), b.size(), term, conv::Rate::R1_2, out.data());
      CHECK(out == b);
      CHECK(e == static_cast<int>(flips));
    }
    // double adjacent errors (free distance 10 -> 4 errors in a window are correctable)
    Bits b = random_bits(rng, 300);
    Bits enc = conv::encode(b, term);
    for (size_t i = 20; i + 1 < enc.size(); i += 60) {
      enc[i] ^= 1;
      enc[i + 1] ^= 1;
    }
    Bits out(b.size());
    conv::decode_hard(enc.data(), enc.size(), b.size(), term, conv::Rate::R1_2, out.data());
    CHECK(out == b);
  }
}

TEST_CASE("Viterbi soft decision: erased symbols carry no information") {
  Rng rng(303);
  Bits b = random_bits(rng, 500);
  Bits enc = conv::encode(b, conv::Termination::Flushed);
  std::vector<uint8_t> soft(enc.size());
  for (size_t i = 0; i < enc.size(); ++i) soft[i] = enc[i] ? 255 : 0;
  // erase every 4th symbol (like rate 2/3 puncturing) and flip a few weak ones
  for (size_t i = 3; i < soft.size(); i += 4) soft[i] = 128;
  for (size_t i = 50; i < soft.size(); i += 97) soft[i] = enc[i] ? 100 : 156;
  Bits out(b.size());
  int e = conv::decode_soft(soft.data(), soft.size(), b.size(), conv::Termination::Flushed, conv::Rate::R1_2,
                            out.data());
  CHECK(e >= 0);
  CHECK(out == b);
}

TEST_CASE("Viterbi performance on BSC and AWGN channels (measured, seeded)") {
  Rng rng(304);
  const size_t kBits = 20000;
  Bits b = random_bits(rng, kBits);
  struct Row { const char* what; double param; double out_ber; };
  std::vector<Row> rows;
  // hard decision over a binary symmetric channel
  for (double p : {0.01, 0.03, 0.05}) {
    Bits enc = conv::encode(b, conv::Termination::Flushed);
    for (auto& s : enc)
      if (rng.uniform() < p) s ^= 1;
    Bits out(kBits);
    conv::decode_hard(enc.data(), enc.size(), kBits, conv::Termination::Flushed, conv::Rate::R1_2, out.data());
    rows.push_back({"hard r1/2 BSC p", p, static_cast<double>(diff(out, b)) / kBits});
  }
  // soft decision BPSK/AWGN
  for (auto rate : kRates) {
    double r = rate == conv::Rate::R1_2 ? 0.5 : rate == conv::Rate::R2_3 ? 2.0 / 3 : 0.75;
    for (double ebn0 : {3.0, 4.0}) {
      Bits enc = conv::encode(b, conv::Termination::Flushed, rate);
      size_t raw = 0;
      auto soft = awgn_soft(enc, ebn0, r, rng, &raw);
      Bits out(kBits);
      conv::decode_soft(soft.data(), soft.size(), kBits, conv::Termination::Flushed, rate, out.data());
      const char* name = rate == conv::Rate::R1_2 ? "soft r1/2 Eb/N0" : rate == conv::Rate::R2_3 ? "soft r2/3 Eb/N0" : "soft r3/4 Eb/N0";
      rows.push_back({name, ebn0, static_cast<double>(diff(out, b)) / kBits});
    }
  }
  for (auto& row : rows) MESSAGE(std::string(row.what) << " " << row.param << " -> decoded BER " << row.out_ber);
  // Thresholds are loose versions of textbook K=7 performance (uncoded BPSK at 4 dB: 1.25e-2).
  CHECK(rows[0].out_ber < 1e-3);   // BSC 1%
  CHECK(rows[1].out_ber < 2e-2);   // BSC 3%
  CHECK(rows[3].out_ber < 2e-3);   // soft r1/2, 3 dB
  CHECK(rows[4].out_ber < 5e-4);   // soft r1/2, 4 dB
  CHECK(rows[6].out_ber < 5e-3);   // soft r2/3, 4 dB
  CHECK(rows[8].out_ber < 2e-2);   // soft r3/4, 4 dB
}

TEST_CASE("Viterbi tail-biting on short packets with noise") {
  Rng rng(305);
  int perfect = 0;
  const int kIter = 100;
  for (int iter = 0; iter < kIter; ++iter) {
    Bits b = random_bits(rng, 96);  // 12-byte header-sized packet
    Bits enc = conv::encode(b, conv::Termination::TailBiting);
    for (int k = 0; k < 4; ++k) enc[rng.below(enc.size())] ^= 1;
    Bits out(b.size());
    conv::decode_hard(enc.data(), enc.size(), b.size(), conv::Termination::TailBiting, conv::Rate::R1_2, out.data());
    perfect += out == b;
  }
  MESSAGE("tail-biting 96 bits, 4 random symbol errors: " << perfect << "/" << kIter << " perfect");
  CHECK(perfect >= kIter * 9 / 10);
}

TEST_CASE("Viterbi fuzz: random soft input never crashes; byte helpers") {
  Rng rng(306);
  for (int iter = 0; iter < 200; ++iter) {
    size_t n = 1 + rng.below(300);
    auto term = kTerms[iter % 2];
    auto rate = kRates[iter % 3];
    size_t ns = conv::encoded_length(n, term, rate);
    Bytes soft = rng.bytes(ns);
    Bits out(n);
    int e = conv::decode_soft(soft.data(), soft.size(), n, term, rate, out.data());
    CHECK(e >= 0);
    for (uint8_t x : out) CHECK(x <= 1);
  }
  Bytes data = {0xA5, 0x01, 0xFF};
  auto bits = conv::unpack_bits(data);
  CHECK(bits.size() == 24);
  CHECK(bits[0] == 1);
  CHECK(bits[1] == 0);
  CHECK(bits[15] == 1);
  CHECK(conv::pack_bits(bits.data(), bits.size()) == data);
  CHECK(conv::pack_bits(bits.data(), 4) == Bytes{0xA0});
}

}  // TEST_SUITE
