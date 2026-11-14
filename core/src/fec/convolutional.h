// EmberMesh FEC — rate 1/2, constraint length 7 convolutional code ("NASA standard", polynomials
// 171/133 octal) with a soft/hard-decision Viterbi decoder, optional puncturing to rate 2/3 or
// 3/4, and either flushed (zero-tail) or tail-biting termination.
//
// Conventions (shared by encoder and decoder; the modem must use the same):
//   * Bits are unpacked: one bit per uint8_t, value 0 or 1. Use pack/unpack helpers for bytes
//     (MSB first).
//   * Shift register sr = ((sr << 1) | bit) & 0x7F (newest bit in bit 0); output pair per input
//     bit: c0 = parity(sr & 0x4F), c1 = parity(sr & 0x6D), transmitted c0 then c1, no inversion.
//     0x4F / 0x6D are 171 / 133 octal bit-reversed, so the impulse responses in time order are
//     1111001 / 1011011 as in CCSDS 131.0-B and Phil Karn's libfec "v27" (also used by many
//     amateur modems).
//   * Soft symbols are uint8_t: 0 = confident "0", 255 = confident "1", 128 = no information
//     (used for punctured positions / erasures). Hard bits b map to 0 / 255.
//   * Puncturing (applied to the c0,c1 stream, period P input bits):
//       rate 2/3: keep c0 c1 | c0 --   (3 of 4 symbols)
//       rate 3/4: keep c0 c1 | -- c1 | c0 --   (4 of 6 symbols)
//   * Flushed: 6 zero tail bits are appended by the encoder (encoder and decoder start and end in
//     state 0). Tail-biting: the encoder starts in the state given by the last 6 data bits so no
//     tail is sent; the decoder uses a circular (wrap-around) Viterbi with 48-bit pre/post-roll.
//     Tail-biting is the better choice for short packets (no 12-symbol tail overhead) but needs
//     >= ~24 data bits to perform well.
//
// Free distance 10 (rate 1/2). Coding gain vs. uncoded BPSK at BER 1e-5 is ~5 dB (soft) / ~3 dB
// (hard). See tests for measured performance.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ember/bytes.h"

namespace ember::fec::conv {

constexpr unsigned kConstraint = 7;
constexpr uint8_t kPolyA = 0x4F;  // 171 octal, bit-reversed for the left-shifting register
constexpr uint8_t kPolyB = 0x6D;  // 133 octal, bit-reversed
constexpr size_t kMaxBits = 1u << 20;  // decoder input cap (1 Mbit); memory = 8 bytes per bit

enum class Termination : uint8_t { Flushed, TailBiting };
enum class Rate : uint8_t { R1_2, R2_3, R3_4 };

// Number of channel symbols produced for nbits data bits.
size_t encoded_length(size_t nbits, Termination term, Rate rate = Rate::R1_2);

// Encodes nbits unpacked bits. Returns an empty vector if nbits == 0 or nbits > kMaxBits.
std::vector<uint8_t> encode(const uint8_t* bits, size_t nbits, Termination term, Rate rate = Rate::R1_2);
inline std::vector<uint8_t> encode(const std::vector<uint8_t>& bits, Termination term, Rate rate = Rate::R1_2) {
  return encode(bits.data(), bits.size(), term, rate);
}

// Soft-decision Viterbi. `soft` must hold exactly encoded_length(nbits, term, rate) symbols.
// Writes nbits decoded bits (0/1) to out. Returns the number of hard-decision channel symbols that
// disagree with the re-encoded decision (an estimate of channel bit errors corrected, >= 0), or -1
// on invalid arguments.
int decode_soft(const uint8_t* soft, size_t nsyms, size_t nbits, Termination term, Rate rate, uint8_t* out);
// Hard-decision convenience: `bits` are 0/1 channel bits.
int decode_hard(const uint8_t* bits, size_t nsyms, size_t nbits, Termination term, Rate rate, uint8_t* out);

// Byte helpers (MSB first).
std::vector<uint8_t> unpack_bits(ByteView bytes);
Bytes pack_bits(const uint8_t* bits, size_t nbits);  // nbits rounded up; trailing bits zero

}  // namespace ember::fec::conv
