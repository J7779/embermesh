// Seeded robustness / mutation fuzzing of every FEC decoder with hostile input (security review,
// "not covered": FEC decoders). Fast versions run in the default suite; the long versions are
// skipped by default:   ./ember_tests -ts=fec -tc="fuzz*long*" --no-skip
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "doctest.h"
#include "ember/crc.h"
#include "fec/convolutional.h"
#include "fec/golay.h"
#include "fec/hamming.h"
#include "fec/interleave.h"
#include "fec/packet.h"
#include "fec/reed_solomon.h"
#include "fec/scrambler.h"
#include "fec/test_rng.h"

using namespace ember;
using namespace ember::fec;
using fec_test::Rng;

namespace {

constexpr size_t kHuge = std::numeric_limits<size_t>::max();

// ---- Reed-Solomon ---------------------------------------------------------------------------------
void rs_campaign(uint64_t seed, int iters) {
  Rng r(seed);
  const size_t roots[] = {1, 2, 3, 4, 6, 8, 16, 32, 64, 128, 200, 254};
  for (int it = 0; it < iters; ++it) {
    size_t nr = roots[r.below(sizeof roots / sizeof roots[0])];
    ReedSolomon rs(nr, static_cast<uint8_t>(r.below(3)));
    REQUIRE(rs.valid());
    size_t k = 1 + r.below(rs.max_data());
    Bytes data = r.bytes(k);
    Bytes cw = rs.encode(ByteView(data));
    REQUIRE(cw.size() == k + nr);
    const Bytes orig = cw;
    // Hostile mutation: random errors (sometimes far beyond capacity).
    size_t nerr = r.below(4) == 0 ? r.below(cw.size() + 1) : r.below(nr / 2 + 2);
    for (size_t e = 0; e < nerr; ++e) cw[r.below(cw.size())] ^= static_cast<uint8_t>(1 + r.below(255));
    // Hostile erasure list: random length (may exceed nroots), duplicates, out-of-range entries.
    std::vector<size_t> er;
    size_t ne = r.below(4) == 0 ? r.below(nr + 4) : r.below(nr / 2 + 1);
    for (size_t e = 0; e < ne; ++e) {
      switch (r.below(6)) {
        case 0: er.push_back(cw.size() + r.below(1000)); break;          // out of range
        case 1: er.push_back(er.empty() ? 0 : er[r.below(er.size())]); break;  // duplicate
        case 2: er.push_back(kHuge - r.below(3)); break;                  // absurd
        default: er.push_back(r.below(cw.size())); break;
      }
    }
    const Bytes before = cw;
    int n = rs.decode(cw.data(), cw.size(), er.empty() ? nullptr : er.data(), er.size());
    if (n < 0) {
      CHECK(cw == before);  // untouched on failure
    } else {
      CHECK(rs.check(ByteView(cw)));  // whatever it returns is a codeword
      CHECK(static_cast<size_t>(n) <= nr);
      // Within the guaranteed radius it must be the original codeword.
      size_t diff = 0;
      for (size_t i = 0; i < cw.size(); ++i) diff += before[i] != orig[i];
      if (2 * diff <= nr && er.empty()) CHECK(cw == orig);
    }
    // Invalid lengths and pointers never touch memory.
    uint8_t tiny[1] = {0};
    CHECK(rs.decode(tiny, 0) == -1);
    CHECK(rs.decode(nullptr, cw.size()) == -1);
    CHECK(rs.decode(cw.data(), nr) == -1);
    CHECK(rs.check(ByteView(cw.data(), nr)) == false);
  }
}

// ---- Golay / Hamming ------------------------------------------------------------------------------
int popcount32(uint32_t v) {
  int n = 0;
  while (v) { n += v & 1; v >>= 1; }
  return n;
}

void golay_campaign(uint64_t seed, int iters) {
  Rng r(seed);
  for (int it = 0; it < iters; ++it) {
    uint32_t w = static_cast<uint32_t>(r.next());  // upper bits must be ignored
    uint16_t d = 0xFFFF;
    int e = golay::decode(w, &d);
    if (e >= 0) {
      CHECK(e <= 3);
      CHECK(d < 4096);
      CHECK(popcount32((golay::encode(d) ^ w) & 0xFFFFFF) == e);
    } else {
      CHECK(d == 0xFFFF);
    }
    // Byte helper on mutated / truncated / oversized-length input.
    Bytes data = r.bytes(r.below(40));
    Bytes enc = golay::encode_bytes(ByteView(data));
    for (size_t k = r.below(6); k > 0 && !enc.empty(); --k) enc[r.below(enc.size())] ^= static_cast<uint8_t>(1u << r.below(8));
    if (r.below(3) == 0 && !enc.empty()) enc.resize(r.below(enc.size()));
    Bytes out;
    size_t n = r.below(5) == 0 ? (r.below(2) ? kHuge - r.below(8) : data.size() + 1 + r.below(100)) : data.size();
    int g = golay::decode_bytes(ByteView(enc), n, &out);
    if (g >= 0) CHECK(out.size() == n);
    // Hamming(8,4): never fails per byte, odd lengths rejected.
    Bytes hin = r.bytes(r.below(30));
    Bytes hout;
    int h = hamming::decode84_bytes(ByteView(hin), &hout);
    if (hin.size() % 2) CHECK(h == -1);
    else if (h >= 0) CHECK(hout.size() == hin.size() / 2);
    uint8_t nib;
    hamming::decode74(r.byte(), &nib);
    CHECK(nib < 16);
  }
}

// ---- Viterbi ------------------------------------------------------------------------------------
void viterbi_campaign(uint64_t seed, int iters) {
  Rng r(seed);
  const conv::Rate rates[] = {conv::Rate::R1_2, conv::Rate::R2_3, conv::Rate::R3_4};
  const conv::Termination terms[] = {conv::Termination::Flushed, conv::Termination::TailBiting};
  for (int it = 0; it < iters; ++it) {
    conv::Rate rate = rates[r.below(3)];
    conv::Termination term = terms[r.below(2)];
    size_t nbits = r.below(8) == 0 ? r.below(3) : 1 + r.below(600);
    size_t ns = conv::encoded_length(nbits, term, rate);
    // Extreme soft values: all 0, all 255, all 128 (erasure), alternating extremes, random.
    std::vector<uint8_t> soft(ns + 8);
    int mode = static_cast<int>(r.below(5));
    for (size_t i = 0; i < soft.size(); ++i)
      soft[i] = mode == 0 ? 0 : mode == 1 ? 255 : mode == 2 ? 128 : mode == 3 ? ((i & 1) ? 255 : 0) : r.byte();
    std::vector<uint8_t> out(nbits + 1, 0xAA);
    // Wrong lengths (off by one, zero) must be refused.
    size_t use = ns;
    if (r.below(4) == 0) use = r.below(2) ? ns + 1 + r.below(5) : (ns ? ns - 1 : 0);
    int e = conv::decode_soft(soft.data(), use, nbits, term, rate, out.data());
    if (use != ns || nbits == 0) CHECK(e == -1);
    if (e >= 0) {
      CHECK(static_cast<size_t>(e) <= ns);
      for (size_t i = 0; i < nbits; ++i) CHECK(out[i] <= 1);
    }
    CHECK(out[nbits] == 0xAA);  // never writes past nbits
    std::vector<uint8_t> hard(ns);
    for (auto& b : hard) b = r.byte();  // non-0/1 "bits"
    conv::decode_hard(hard.data(), ns, nbits, term, rate, out.data());
    CHECK(out[nbits] == 0xAA);
  }
  // Size limits: above kMaxBits refused without allocating, null pointers refused.
  uint8_t dummy[4] = {0};
  CHECK(conv::decode_soft(dummy, 4, conv::kMaxBits + 1, conv::Termination::Flushed, conv::Rate::R1_2, dummy) == -1);
  CHECK(conv::decode_soft(nullptr, 0, 1, conv::Termination::Flushed, conv::Rate::R1_2, dummy) == -1);
  CHECK(conv::decode_soft(dummy, kHuge, kHuge, conv::Termination::TailBiting, conv::Rate::R3_4, dummy) == -1);
  CHECK(conv::decode_hard(nullptr, 2, 1, conv::Termination::Flushed, conv::Rate::R1_2, dummy) == -1);
}

// ---- packet envelopes -----------------------------------------------------------------------------
void packet_campaign(uint64_t seed, int iters) {
  Rng r(seed);
  for (int it = 0; it < iters; ++it) {
    size_t len = r.below(6) == 0 ? r.below(4000) : r.below(300);
    Bytes payload = r.bytes(len);
    ProtectParams p;
    p.nroots = static_cast<uint8_t>(kMinRoots + r.below(kMaxRoots - kMinRoots + 1));
    p.interleave = r.below(2) != 0;
    Bytes env = protect(ByteView(payload), p);
    REQUIRE(!env.empty());
    switch (r.below(7)) {
      case 0: for (size_t k = 1 + r.below(8); k; --k) env[r.below(env.size())] ^= static_cast<uint8_t>(1 + r.below(255)); break;
      case 1: for (size_t k = 1 + r.below(env.size()); k; --k) env[r.below(env.size())] = r.byte(); break;  // heavy
      case 2: env.resize(r.below(env.size())); break;                                                         // truncate
      case 3: { Bytes junk = r.bytes(r.below(64)); env.insert(env.begin() + static_cast<std::ptrdiff_t>(r.below(env.size() + 1)), junk.begin(), junk.end()); break; }
      case 4: for (size_t i = 0; i < 10 && i < env.size(); ++i) env[i] = r.byte(); break;                    // header smash
      case 5: env = r.bytes(r.below(600)); break;                                                            // pure noise
      default: break;
    }
    size_t maxp = r.below(5) == 0 ? (r.below(2) ? kHuge : r.below(len + 1)) : 65535;
    Bytes out;
    RecoverInfo info;
    bool ok = recover(ByteView(env), &out, &info, maxp);
    size_t total = 0;
    bool hdr = peek_header(ByteView(env), &total, maxp);
    if (ok) {
      // Equal to the original (a CRC-valid mis-decode would need ~2^-32 luck; seeded, so this is
      // deterministic) and consistent with the header.
      CHECK(out == payload);
      CHECK(out.size() <= maxp);
      CHECK(info.consumed <= env.size());
      CHECK(hdr);
      CHECK(total == info.consumed);
    }
    if (hdr) CHECK(total <= kPacketHeaderSize + 2 * (kMaxProtectedPayload + 4) + 300 * 128);
  }
  Bytes out;
  CHECK_FALSE(recover(ByteView(), &out));
  size_t t;
  CHECK_FALSE(peek_header(ByteView(), &t));
}

// ---- interleaver / scramblers -------------------------------------------------------------------
void interleave_campaign(uint64_t seed, int iters) {
  Rng r(seed);
  for (int it = 0; it < iters; ++it) {
    size_t n = r.below(4) == 0 ? r.below(3) : r.below(700);
    size_t rows;
    switch (r.below(6)) {
      case 0: rows = 0; break;
      case 1: rows = 1; break;
      case 2: rows = n + r.below(50); break;           // rows >= size
      case 3: rows = kHuge - r.below(4); break;        // absurd
      case 4: rows = (kHuge >> 1) + r.below(4); break;
      default: rows = 2 + r.below(40); break;           // non-divisible sizes
    }
    Bytes in = r.bytes(n);
    Bytes il = interleave(ByteView(in), rows);
    REQUIRE(il.size() == n);
    CHECK(deinterleave(ByteView(il), rows) == in);
    // interleave_index is a permutation consistent with interleave().
    for (size_t i = 0; i < n && i < 64; ++i) {
      size_t j = interleave_index(i, n, rows);
      REQUIRE(j < n);
      CHECK(il[j] == in[i]);
    }
    CHECK(interleave_index(n + r.below(5), n, rows) >= n);
  }
}

void scrambler_campaign(uint64_t seed, int iters) {
  Rng r(seed);
  for (int it = 0; it < iters; ++it) {
    unsigned deg = static_cast<unsigned>(r.below(40));
    Lfsr a(static_cast<uint32_t>(r.next()), deg, static_cast<uint32_t>(r.next()));
    Lfsr b = a;
    Bytes d = r.bytes(r.below(64)), d0 = d;
    a.apply(d.data(), d.size());
    b.apply(d.data(), d.size());
    CHECK(d == d0);
    SelfSyncScrambler s(static_cast<unsigned>(r.below(100)), static_cast<unsigned>(r.below(100)));
    SelfSyncScrambler u = s;
    for (int i = 0; i < 200; ++i) {
      uint8_t bit = static_cast<uint8_t>(r.below(2));
      CHECK(u.descramble(s.scramble(bit)) == bit);
    }
  }
}

}  // namespace

TEST_SUITE("fec") {

TEST_CASE("fuzz: Reed-Solomon hostile codewords and erasure lists") { rs_campaign(1, 600); }
TEST_CASE("fuzz: Golay and Hamming hostile words and lengths") { golay_campaign(2, 3000); }
TEST_CASE("fuzz: Viterbi extreme soft values and wrong lengths") { viterbi_campaign(3, 120); }
TEST_CASE("fuzz: packet recover on mutated envelopes") { packet_campaign(4, 400); }
TEST_CASE("fuzz: interleaver size edge cases incl. absurd rows") { interleave_campaign(5, 600); }
TEST_CASE("fuzz: scramblers with out-of-range degrees and taps") { scrambler_campaign(6, 200); }

TEST_CASE("regression: golay decode_bytes with an absurd length does not allocate") {
  Bytes out;
  Bytes enc = golay::encode_bytes(ByteView(Bytes{1, 2, 3}));
  CHECK(golay::decode_bytes(ByteView(enc), kHuge, &out) == -1);
  CHECK(golay::decode_bytes(ByteView(enc), kHuge - 1, &out) == -1);
  CHECK(golay::decode_bytes(ByteView(enc), kHuge / 2, &out) == -1);
  CHECK(golay::decode_bytes(ByteView(enc), 3, &out) == 0);
  CHECK(out == Bytes{1, 2, 3});
}

TEST_CASE("regression: interleaver with rows near SIZE_MAX behaves like rows >= n") {
  Bytes in = {1, 2, 3, 4, 5};
  CHECK(interleave(ByteView(in), kHuge) == interleave(ByteView(in), 5));
  CHECK(deinterleave(ByteView(in), kHuge) == deinterleave(ByteView(in), 5));
  CHECK(interleave_index(2, 5, kHuge) == interleave_index(2, 5, 5));
}

TEST_CASE("fuzz long: all FEC decoders" * doctest::skip()) {
  for (uint64_t s = 100; s < 110; ++s) {
    rs_campaign(s, 20000);
    golay_campaign(s, 100000);
    viterbi_campaign(s, 3000);
    packet_campaign(s, 20000);
    interleave_campaign(s, 20000);
    scrambler_campaign(s, 2000);
  }
}

}  // TEST_SUITE
