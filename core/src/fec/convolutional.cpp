#include "fec/convolutional.h"

#include <array>

namespace ember::fec::conv {

namespace {

constexpr unsigned kTail = kConstraint - 1;  // 6
constexpr unsigned kStates = 1u << kTail;    // 64
constexpr size_t kTbRoll = 48;               // tail-biting pre/post roll (bits)

struct Pattern {
  uint8_t keep[6];
  uint8_t len;  // symbols per period (2 * period bits)
};

constexpr Pattern pattern(Rate r) {
  switch (r) {
    case Rate::R2_3: return Pattern{{1, 1, 1, 0, 0, 0}, 4};
    case Rate::R3_4: return Pattern{{1, 1, 0, 1, 1, 0}, 6};
    case Rate::R1_2:
    default: return Pattern{{1, 1, 0, 0, 0, 0}, 2};
  }
}

constexpr uint8_t parity7(unsigned v) {
  v &= 0x7F;
  v ^= v >> 4;
  v ^= v >> 2;
  v ^= v >> 1;
  return static_cast<uint8_t>(v & 1);
}

struct OutTable {
  std::array<uint8_t, 128> o{};  // bit1 = c0, bit0 = c1 for register value sr7
};
constexpr OutTable make_out() {
  OutTable t;
  for (unsigned sr = 0; sr < 128; ++sr)
    t.o[sr] = static_cast<uint8_t>(parity7(sr & kPolyA) << 1 | parity7(sr & kPolyB));
  return t;
}
constexpr OutTable kOut = make_out();

size_t punctured_count(size_t full, Rate rate) {
  Pattern p = pattern(rate);
  size_t keeps = 0;
  for (size_t i = 0; i < p.len; ++i) keeps += p.keep[i];
  size_t n = (full / p.len) * keeps;
  for (size_t i = 0; i < full % p.len; ++i) n += p.keep[i];
  return n;
}

bool valid_args(size_t nbits) { return nbits > 0 && nbits <= kMaxBits; }

// Full-rate (unpunctured) encoder: 2 symbols per register step.
std::vector<uint8_t> encode_full(const uint8_t* bits, size_t nbits, Termination term) {
  size_t steps = nbits + (term == Termination::Flushed ? kTail : 0);
  std::vector<uint8_t> out;
  out.reserve(2 * steps);
  unsigned sr = 0;
  if (term == Termination::TailBiting) {
    // State before step 0 = the last 6 data bits (cyclically, oldest in the highest position).
    for (unsigned k = kTail; k > 0; --k) {
      size_t idx = (nbits * kTail - k) % nbits;  // u[-k], cyclically
      sr = ((sr << 1) | (bits[idx] & 1)) & 0x3F;
    }
  }
  for (size_t t = 0; t < steps; ++t) {
    unsigned b = t < nbits ? (bits[t] & 1u) : 0u;
    sr = ((sr << 1) | b) & 0x7F;
    uint8_t o = kOut.o[sr];
    out.push_back(static_cast<uint8_t>(o >> 1));
    out.push_back(static_cast<uint8_t>(o & 1));
  }
  return out;
}

}  // namespace

size_t encoded_length(size_t nbits, Termination term, Rate rate) {
  size_t steps = nbits + (term == Termination::Flushed ? kTail : 0);
  return punctured_count(2 * steps, rate);
}

std::vector<uint8_t> encode(const uint8_t* bits, size_t nbits, Termination term, Rate rate) {
  if (!valid_args(nbits) || bits == nullptr) return {};
  std::vector<uint8_t> full = encode_full(bits, nbits, term);
  if (rate == Rate::R1_2) return full;
  Pattern p = pattern(rate);
  std::vector<uint8_t> out;
  out.reserve(punctured_count(full.size(), rate));
  for (size_t i = 0; i < full.size(); ++i)
    if (p.keep[i % p.len]) out.push_back(full[i]);
  return out;
}

int decode_soft(const uint8_t* soft, size_t nsyms, size_t nbits, Termination term, Rate rate, uint8_t* out) {
  if (!valid_args(nbits) || soft == nullptr || out == nullptr) return -1;
  if (nsyms != encoded_length(nbits, term, rate)) return -1;
  const size_t steps = nbits + (term == Termination::Flushed ? kTail : 0);

  // Depuncture into signed values v = y - 128 (punctured -> 0).
  std::vector<int16_t> v(2 * steps, 0);
  {
    Pattern p = pattern(rate);
    size_t j = 0;
    for (size_t i = 0; i < v.size(); ++i)
      if (p.keep[i % p.len]) v[i] = static_cast<int16_t>(static_cast<int>(soft[j++]) - 128);
  }

  const bool tb = term == Termination::TailBiting;
  const size_t roll = tb ? kTbRoll : 0;
  const size_t total = steps + 2 * roll;
  std::vector<uint64_t> dec(total);

  int32_t pm[kStates], npm[kStates];
  const int32_t kInf = 1 << 28;
  for (unsigned s = 0; s < kStates; ++s) pm[s] = tb ? 0 : (s == 0 ? 0 : kInf);

  for (size_t t = 0; t < total; ++t) {
    size_t k = tb ? (t + steps - roll % steps) % steps : t;  // symbol step index (cyclic for tail-biting)
    int32_t y0 = v[2 * k], y1 = v[2 * k + 1];
    // cost of expected symbol o: o ? -y : y   (min cost == max correlation)
    int32_t c[4];
    c[0] = y0 + y1;    // o = 00
    c[1] = y0 - y1;    // o = 01 (c0=0, c1=1)
    c[2] = -y0 + y1;   // o = 10
    c[3] = -y0 - y1;   // o = 11
    uint64_t d = 0;
    int32_t best = INT32_MAX;
    for (unsigned ns = 0; ns < kStates; ++ns) {
      unsigned s0 = ns >> 1, s1 = (ns >> 1) | 0x20;
      int32_t m0 = pm[s0] + c[kOut.o[ns]];
      int32_t m1 = pm[s1] + c[kOut.o[0x40 | ns]];
      if (m1 < m0) {
        npm[ns] = m1;
        d |= uint64_t(1) << ns;
      } else {
        npm[ns] = m0;
      }
      if (npm[ns] < best) best = npm[ns];
    }
    dec[t] = d;
    for (unsigned s = 0; s < kStates; ++s) {
      int32_t m = npm[s] - best;
      pm[s] = m > kInf ? kInf : m;
    }
  }

  unsigned state = 0;
  if (tb) {
    int32_t best = INT32_MAX;
    for (unsigned s = 0; s < kStates; ++s)
      if (pm[s] < best) { best = pm[s]; state = s; }
  }
  // Traceback.
  for (size_t t = total; t-- > 0;) {
    uint8_t bit = static_cast<uint8_t>(state & 1);
    if (tb) {
      if (t >= roll && t < roll + steps) out[t - roll] = bit;
    } else if (t < nbits) {
      out[t] = bit;
    }
    unsigned x = static_cast<unsigned>((dec[t] >> state) & 1);
    state = (state >> 1) | (x << 5);
  }

  // Count channel symbol disagreements with the re-encoded decision.
  std::vector<uint8_t> re = encode(out, nbits, term, rate);
  int errs = 0;
  for (size_t i = 0; i < nsyms && i < re.size(); ++i)
    if ((soft[i] >= 128 ? 1 : 0) != re[i] && soft[i] != 128) ++errs;
  return errs;
}

int decode_hard(const uint8_t* bits, size_t nsyms, size_t nbits, Termination term, Rate rate, uint8_t* out) {
  if (bits == nullptr) return -1;
  std::vector<uint8_t> soft(nsyms);
  for (size_t i = 0; i < nsyms; ++i) soft[i] = (bits[i] & 1) ? 255 : 0;
  return decode_soft(soft.data(), nsyms, nbits, term, rate, out);
}

std::vector<uint8_t> unpack_bits(ByteView bytes) {
  std::vector<uint8_t> out;
  out.reserve(bytes.size() * 8);
  for (uint8_t b : bytes)
    for (int i = 7; i >= 0; --i) out.push_back(static_cast<uint8_t>((b >> i) & 1));
  return out;
}

Bytes pack_bits(const uint8_t* bits, size_t nbits) {
  Bytes out((nbits + 7) / 8, 0);
  for (size_t i = 0; i < nbits; ++i)
    if (bits[i] & 1) out[i / 8] |= static_cast<uint8_t>(0x80 >> (i % 8));
  return out;
}

}  // namespace ember::fec::conv
