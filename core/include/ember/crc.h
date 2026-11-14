// EmberMesh core — checksums shared by framing, modems, storage and the wire protocol. Header-only.
#pragma once

#include <cstdint>

#include "ember/bytes.h"

namespace ember {

// CRC-16/X-25 (a.k.a. CRC-16/IBM-SDLC): poly 0x1021 reflected (0x8408), init 0xFFFF, xorout 0xFFFF.
// This is the HDLC / AX.25 frame check sequence (transmitted low byte first). check("123456789") = 0x906E.
inline uint16_t crc16_x25(ByteView data, uint16_t crc = 0xFFFF) {
  for (uint8_t b : data) {
    crc ^= b;
    for (int i = 0; i < 8; ++i) crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0x8408) : static_cast<uint16_t>(crc >> 1);
  }
  return crc;
}
inline uint16_t crc16_x25_final(uint16_t crc) { return static_cast<uint16_t>(~crc); }
inline uint16_t crc16_x25_of(ByteView data) { return crc16_x25_final(crc16_x25(data)); }

// CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no xorout. check("123456789") = 0x29B1.
inline uint16_t crc16_ccitt(ByteView data, uint16_t crc = 0xFFFF) {
  for (uint8_t b : data) {
    crc ^= static_cast<uint16_t>(b << 8);
    for (int i = 0; i < 8; ++i) crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
  }
  return crc;
}

// CRC-32 (IEEE 802.3 / zlib): reflected poly 0xEDB88320, init/xorout 0xFFFFFFFF. check("123456789") = 0xCBF43926.
// Streaming use: crc = crc32(chunk1); crc = crc32(chunk2, crc);
// Table-driven (one 1 KiB constant table, slice-by-1): receivers CRC every resync candidate of
// hostile input, so this must not be the bitwise loop (security review H5).
namespace crc_detail {
struct Crc32Table {
  uint32_t t[256];
  constexpr Crc32Table() : t() {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
      t[i] = c;
    }
  }
};
struct Crc8Table {
  uint8_t t[256];
  constexpr Crc8Table() : t() {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = (c & 0x80) ? ((c << 1) ^ 0x07) & 0xFF : (c << 1) & 0xFF;
      t[i] = static_cast<uint8_t>(c);
    }
  }
};
inline constexpr Crc32Table kCrc32{};
inline constexpr Crc8Table kCrc8{};
}  // namespace crc_detail

inline uint32_t crc32(ByteView data, uint32_t prev = 0) {
  uint32_t crc = ~prev;
  for (uint8_t b : data) crc = crc_detail::kCrc32.t[(crc ^ b) & 0xFF] ^ (crc >> 8);
  return ~crc;
}

// CRC-8 (poly 0x07, init 0x00). check("123456789") = 0xF4.
inline uint8_t crc8(ByteView data, uint8_t crc = 0) {
  for (uint8_t b : data) crc = crc_detail::kCrc8.t[crc ^ b];
  return crc;
}

}  // namespace ember
