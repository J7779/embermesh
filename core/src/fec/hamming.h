// EmberMesh FEC — Hamming(7,4) (corrects 1 bit) and extended Hamming(8,4) SECDED (corrects 1,
// detects 2) for nibbles.
//
// Codeword bit i (0..6) holds Hamming position i+1: p1 p2 d0 p3 d1 d2 d3, where d0..d3 are
// nibble bits 0..3; so the syndrome directly names the erroneous position. (8,4) adds an overall
// even-parity bit as bit 7.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ember/bytes.h"

namespace ember::fec::hamming {

uint8_t encode74(uint8_t nibble);                 // low 4 bits used; returns 7-bit codeword
int decode74(uint8_t codeword, uint8_t* nibble);  // returns 0 or 1 (bits corrected); never fails
uint8_t encode84(uint8_t nibble);                 // 8-bit codeword
// Returns 0 (clean), 1 (single error corrected) or -1 (double error detected; *nibble untouched).
int decode84(uint8_t codeword, uint8_t* nibble);

// Byte helpers using (8,4): each byte -> 2 codewords (high nibble first).
Bytes encode84_bytes(ByteView data);
// Returns total corrected bits, or -1 if the input length is odd or any codeword had 2 errors.
int decode84_bytes(ByteView in, Bytes* out);

}  // namespace ember::fec::hamming
