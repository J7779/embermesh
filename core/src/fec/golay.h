// EmberMesh FEC — extended binary Golay code (24,12,8).
//
// Corrects any 3 bit errors in a 24-bit codeword and detects any 4 (minimum distance 8). Handy
// for tiny, critical headers (length / type fields of modem frames) where a byte-oriented RS code
// is too coarse.
//
// Codeword layout (24 bits in a uint32_t, bits 23..0): data (12 bits) << 12 | parity (12 bits),
// parity = data * B over GF(2) with the standard symmetric matrix B (B*B = I), generator
// G = [I12 | B]. Decoding is the classical syndrome algorithm (Wicker, "Error Control Systems",
// sec. 5.4): no lookup tables, O(12) row operations per word.
#pragma once

#include <cstddef>
#include <cstdint>

#include "ember/bytes.h"

namespace ember::fec::golay {

// 12-bit data -> 24-bit codeword (upper bits of `data` ignored).
uint32_t encode(uint16_t data);

// Decodes a 24-bit word (upper 8 bits ignored). On success stores the 12 data bits in *data and
// returns the number of bit errors corrected (0..3). Returns -1 if 4 or more errors were detected
// (*data untouched). Patterns of >= 5 errors may be mis-decoded (inherent to the code).
int decode(uint32_t word, uint16_t* data);

// Byte helpers: every 3 input bytes (24 bits = two 12-bit halves) become 6 output bytes (two
// codewords, big-endian 3 bytes each). A final odd byte/pair is zero-padded to 3 bytes, so the
// output is 2 * ceil(n/3) * 3 bytes. decode_bytes() needs the original length n.
Bytes encode_bytes(ByteView data);
size_t encoded_size(size_t n);
// Returns total bits corrected (>= 0), or -1 if any codeword was uncorrectable or `in` is shorter
// than encoded_size(n). out receives exactly n bytes on success.
int decode_bytes(ByteView in, size_t n, Bytes* out);

}  // namespace ember::fec::golay
