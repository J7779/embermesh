// EmberMesh core / modem — K=7, rate 1/2 convolutional code with soft-decision Viterbi decoding.
//
// Generator polynomials 0171 and 0133 (octal), the "NASA standard" code (free distance 10).
// Encoder definition (exact, for re-implementation):
//   sr = 0 (7 bits)
//   for each input bit b:  sr = ((sr << 1) | b) & 0x7F
//                          emit parity(sr & 0171), then parity(sr & 0133)
// Blocks are terminated by appending 6 zero input bits (the caller includes them), so the decoder
// starts and ends in state 0.
//
// LLR convention: llr > 0 means the coded bit is more likely 1. Magnitude = confidence.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ember {
namespace modem {

// Appends 2*n coded bits (0/1) for n input bits (0/1).
void conv_encode(const uint8_t* bits, size_t n, std::vector<uint8_t>& out);

class Viterbi {
 public:
  // Decodes n_steps input bits from 2*n_steps LLRs (terminated trellis: ends in state 0).
  // `out` receives n_steps bits. Returns the number of coded bits whose hard decision disagrees with
  // the re-encoded result (a measure of channel errors corrected). Memory: 8 bytes per step.
  size_t decode(const float* llr, size_t n_steps, std::vector<uint8_t>& out);

 private:
  std::vector<uint64_t> dec_;
};

}  // namespace modem
}  // namespace ember
