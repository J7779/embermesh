// EmberMesh FEC — GF(2^8) arithmetic with the primitive polynomial x^8+x^4+x^3+x^2+1 (0x11D),
// generator alpha = 2. This is the field used by QR codes, DVB, CD/DVD (CIRC) and most
// "RS(255,223)" software implementations. Tables are computed at compile time (constexpr), so
// there is no mutable global state. Header-only.
#pragma once

#include <array>
#include <cstdint>

namespace ember::fec::gf256 {

constexpr unsigned kPoly = 0x11D;

struct Tables {
  std::array<uint8_t, 512> exp{};  // exp[i] = alpha^i, doubled so exp[log a + log b] needs no mod
  std::array<uint8_t, 256> log{};  // log[0] is undefined (0)
};

constexpr Tables make_tables() {
  Tables t;
  unsigned x = 1;
  for (unsigned i = 0; i < 255; ++i) {
    t.exp[i] = static_cast<uint8_t>(x);
    t.log[x] = static_cast<uint8_t>(i);
    x <<= 1;
    if (x & 0x100) x ^= kPoly;
  }
  for (unsigned i = 255; i < 512; ++i) t.exp[i] = t.exp[i - 255];
  return t;
}

inline constexpr Tables kTables = make_tables();

constexpr uint8_t exp(unsigned i) { return kTables.exp[i % 255]; }  // alpha^i
constexpr uint8_t log(uint8_t a) { return kTables.log[a]; }         // a != 0
constexpr uint8_t mul(uint8_t a, uint8_t b) {
  return (a == 0 || b == 0) ? 0 : kTables.exp[kTables.log[a] + kTables.log[b]];
}
constexpr uint8_t inv(uint8_t a) { return a == 0 ? 0 : kTables.exp[255 - kTables.log[a]]; }  // inv(0) := 0
constexpr uint8_t div(uint8_t a, uint8_t b) {
  return (a == 0 || b == 0) ? 0 : kTables.exp[kTables.log[a] + 255 - kTables.log[b]];
}
// a^n for n >= 0
constexpr uint8_t pow(uint8_t a, unsigned n) {
  return a == 0 ? (n == 0 ? 1 : 0) : kTables.exp[(static_cast<unsigned>(kTables.log[a]) * (n % 255)) % 255];
}

}  // namespace ember::fec::gf256
