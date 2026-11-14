// EmberMesh guides — compact raw DEFLATE (RFC 1951) codec and a table-driven CRC-32.
//
// Written for guide bodies: the encoder (used by guidepack at build time) does hash-chain LZ77 with
// lazy matching and dynamic Huffman blocks and is fully deterministic. The decoder is hardened for
// hostile input: every bit read is bounds checked, Huffman tables are validated (over-subscribed
// and incomplete codes rejected, except the single-code case RFC 1951 permits), back references
// may not precede the output start, and output is capped at the caller's expected size. Output is
// interoperable with zlib (`zlib.decompress(data, -15)`).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "ember/bytes.h"

namespace ember::guides {

// Deterministic compression of `in` to a raw DEFLATE stream.
Bytes deflate_raw(ByteView in);

// Decompresses a raw DEFLATE stream that must expand to exactly `expected_size` bytes (and must
// consume the input up to its final block). Returns false on any malformation; `out` is then empty.
bool inflate_raw(ByteView in, size_t expected_size, std::string& out);

// Decodes only the first min(max_out, expected_size) bytes of such a stream (used for snippets).
// Unlike inflate_raw the stream may end early or be cut short: decoding stops at max_out bytes or
// at the end of `in`, and whatever was decoded without error is returned (possibly empty).
// Returns false only on malformation inside the decoded part. No CRC can be checked on a prefix.
bool inflate_raw_prefix(ByteView in, size_t expected_size, size_t max_out, std::string& out);

// CRC-32 (IEEE, same values as ember::crc32 in ember/crc.h) using a 4 KiB constexpr table
// (slice-by-4); several times faster than the bitwise version for whole-section checksums.
uint32_t crc32_fast(ByteView data, uint32_t prev = 0);

}  // namespace ember::guides
