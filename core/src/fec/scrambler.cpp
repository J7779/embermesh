#include "fec/scrambler.h"

namespace ember::fec {

static uint32_t parity32(uint32_t v) {
  v ^= v >> 16;
  v ^= v >> 8;
  v ^= v >> 4;
  v ^= v >> 2;
  v ^= v >> 1;
  return v & 1;
}

Lfsr::Lfsr(uint32_t taps, unsigned degree, uint32_t seed) {
  if (degree < 2) degree = 2;
  if (degree > 32) degree = 32;
  degree_ = degree;
  mask_ = degree == 32 ? 0xFFFFFFFFu : ((1u << degree) - 1);
  taps_ = taps & mask_;
  state_ = seed & mask_;
  if (state_ == 0) state_ = mask_;
}

uint8_t Lfsr::next_bit() {
  uint8_t out = static_cast<uint8_t>(state_ & 1);
  uint32_t fb = parity32(state_ & taps_);
  state_ = ((state_ >> 1) | (fb << (degree_ - 1))) & mask_;
  return out;
}

uint8_t Lfsr::next_byte() {
  uint8_t b = 0;
  for (int i = 0; i < 8; ++i) b = static_cast<uint8_t>(b | (next_bit() << i));
  return b;
}

void Lfsr::apply(uint8_t* data, size_t n) {
  for (size_t i = 0; i < n; ++i) data[i] ^= next_byte();
}

void whiten_pn9(uint8_t* data, size_t n, uint32_t seed) {
  Lfsr l = pn9(seed);
  l.apply(data, n);
}

SelfSyncScrambler::SelfSyncScrambler(unsigned tap_a, unsigned tap_b)
    : a_(tap_a < 1 ? 1 : (tap_a > 32 ? 32 : tap_a)), b_(tap_b < 1 ? 1 : (tap_b > 32 ? 32 : tap_b)) {}

uint8_t SelfSyncScrambler::scramble(uint8_t bit) {
  uint8_t out = static_cast<uint8_t>((bit ^ (state_ >> (a_ - 1)) ^ (state_ >> (b_ - 1))) & 1);
  state_ = (state_ << 1) | out;
  return out;
}

uint8_t SelfSyncScrambler::descramble(uint8_t bit) {
  bit &= 1;
  uint8_t out = static_cast<uint8_t>((bit ^ (state_ >> (a_ - 1)) ^ (state_ >> (b_ - 1))) & 1);
  state_ = (state_ << 1) | bit;
  return out;
}

}  // namespace ember::fec
