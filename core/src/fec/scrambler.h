// EmberMesh FEC — scramblers / data whitening.
//
// Lfsr: additive (synchronous) scrambler. Fibonacci LFSR of `degree` bits: each step outputs
// bit0 of the state, then shifts right inserting parity(state & taps) at bit (degree-1). XORing
// the same keystream twice restores the data; both ends must start from the same seed at the
// same frame position (typically right after the sync word). Bytes are whitened LSB-first:
// bit i of keystream byte = i-th output bit, which makes pn9() bit-exact with TI CC1101/CC1200
// and Semtech SX1231 "PN9" data whitening (x^9 + x^5 + 1, seed 0x1FF -> FF E1 1D 9A ED 85 ...).
//
// SelfSyncScrambler: multiplicative (self-synchronising) bit scrambler as used by G3RUH 9600 bd
// packet (x^17 + x^12 + 1): no shared seed needed, a channel bit error causes (taps+1) output
// errors. Operates on single bits; bit-level modems use it directly.
#pragma once

#include <cstddef>
#include <cstdint>

namespace ember::fec {

class Lfsr {
 public:
  // taps: bit mask of state bits XORed into the feedback (bit0 = the output stage).
  // degree: register length 2..32. seed: initial state (masked to degree bits; 0 is replaced by
  // all-ones because an all-zero additive LFSR would never leave zero).
  Lfsr(uint32_t taps, unsigned degree, uint32_t seed);

  uint8_t next_bit();
  uint8_t next_byte();  // 8 output bits, LSB first
  // XOR data with the keystream (whiten / de-whiten are the same operation).
  void apply(uint8_t* data, size_t n);
  uint32_t state() const { return state_; }

 private:
  uint32_t taps_, mask_, state_;
  unsigned degree_;
};

// PN9 whitening (x^9 + x^5 + 1, seed 0x1FF), compatible with TI CC1101 and Semtech SX1231.
inline Lfsr pn9(uint32_t seed = 0x1FF) { return Lfsr(0x21, 9, seed); }
// Convenience: whiten/de-whiten a buffer with a fresh PN9 sequence.
void whiten_pn9(uint8_t* data, size_t n, uint32_t seed = 0x1FF);

class SelfSyncScrambler {
 public:
  // Polynomial 1 + x^a + x^b given as the two tap delays (G3RUH: a = 12, b = 17). Max delay 32.
  explicit SelfSyncScrambler(unsigned tap_a = 12, unsigned tap_b = 17);
  uint8_t scramble(uint8_t bit);
  uint8_t descramble(uint8_t bit);
  void reset() { state_ = 0; }

 private:
  uint32_t state_ = 0;  // bit k-1 = output (scrambled) bit from k steps ago
  unsigned a_, b_;
};

}  // namespace ember::fec
