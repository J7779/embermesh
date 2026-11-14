// EmberMesh core — short-text compressor for chat over slow links (1200 baud AFSK, LoRa ...).
//
// Static canonical Huffman code over bytes, built from a fixed English/chat letter-frequency table
// (no per-message tables, so even a 10-character message shrinks). Typical English chat compresses
// to ~55-65 % of its size; text that would grow (other scripts, binary) is stored raw. Output is
// never more than input + 1 byte.
//
// Format: byte 0 = method (0 raw, 1 huffman-v1).
//   raw:      method, bytes...
//   huffman:  method, varint original_length, MSB-first bitstream (zero padded to a byte)
// Decompression is bounded: it refuses any declared length above `max_out` and stops at the end
// of input, so hostile input can neither overrun nor make it allocate more than max_out bytes.
#pragma once

#include <string>
#include <string_view>

#include "ember/bytes.h"

namespace ember {

enum : uint8_t { kTextPackRaw = 0, kTextPackHuffman = 1 };

Bytes textpack_compress(std::string_view text);
bool textpack_decompress(ByteView packed, size_t max_out, std::string& out);

}  // namespace ember
