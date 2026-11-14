// EmberMesh FEC — block interleaver / deinterleaver for any length.
//
// Conceptually the input is written row by row into a matrix with `rows` rows and
// C = ceil(n / rows) columns, and read out column by column. When n is not a multiple of `rows`
// the last row is short and the missing cells are simply skipped, so the permutation is defined
// for every n and the output has exactly n elements. Consecutive output elements come from
// different rows, so if each row is one RS codeword a channel burst of B bytes hits each
// codeword in at most ceil(B / rows_used) symbols.
//
// Works on any byte-sized element (bytes for RS symbols, unpacked bits for the Viterbi decoder,
// soft symbols). Elements are uint8_t. In-place is not supported (in and out must not alias).
#pragma once

#include <cstddef>
#include <cstdint>

#include "ember/bytes.h"

namespace ember::fec {

// rows == 0 or 1 (or n <= 1) is the identity permutation.
void interleave(const uint8_t* in, uint8_t* out, size_t n, size_t rows);
void deinterleave(const uint8_t* in, uint8_t* out, size_t n, size_t rows);
Bytes interleave(ByteView in, size_t rows);
Bytes deinterleave(ByteView in, size_t rows);

// Index of input element i in the interleaved output.
size_t interleave_index(size_t i, size_t n, size_t rows);

}  // namespace ember::fec
