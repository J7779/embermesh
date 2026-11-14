#include "fec/golay.h"

#include <utility>

namespace ember::fec::golay {

namespace {

// Rows of B; bit 11 = column 0.
constexpr uint16_t kB[12] = {
    0xDC5,  // 110111000101
    0xB8B,  // 101110001011
    0x717,  // 011100010111
    0xE2D,  // 111000101101
    0xC5B,  // 110001011011
    0x8B7,  // 100010110111
    0x16F,  // 000101101111
    0x2DD,  // 001011011101
    0x5B9,  // 010110111001
    0xB71,  // 101101110001
    0x6E3,  // 011011100011
    0xFFE,  // 111111111110
};

int weight12(uint32_t v) {
  int w = 0;
  v &= 0xFFF;
  while (v) { v &= v - 1; ++w; }
  return w;
}

// v * B for a 12-bit row vector v (bit 11 = element 0).
uint16_t mul_b(uint16_t v) {
  uint16_t out = 0;
  for (int i = 0; i < 12; ++i)
    if (v & (0x800 >> i)) out ^= kB[i];
  return out;
}

}  // namespace

uint32_t encode(uint16_t data) {
  data &= 0xFFF;
  return static_cast<uint32_t>(data) << 12 | mul_b(data);
}

int decode(uint32_t word, uint16_t* data) {
  uint16_t r1 = static_cast<uint16_t>((word >> 12) & 0xFFF);
  uint16_t r2 = static_cast<uint16_t>(word & 0xFFF);
  // e = (x, y): s1 = xB + y, s2 = s1 B = x + yB.
  uint16_t s1 = static_cast<uint16_t>(mul_b(r1) ^ r2);
  uint16_t x = 0, y = 0;
  bool found = false;
  if (weight12(s1) <= 3) {
    y = s1;
    found = true;
  } else {
    for (int i = 0; i < 12 && !found; ++i) {
      uint16_t c = static_cast<uint16_t>(s1 ^ kB[i]);
      if (weight12(c) <= 2) {
        x = static_cast<uint16_t>(0x800 >> i);
        y = c;
        found = true;
      }
    }
  }
  if (!found) {
    uint16_t s2 = mul_b(s1);
    if (weight12(s2) <= 3) {
      x = s2;
      found = true;
    } else {
      for (int j = 0; j < 12 && !found; ++j) {
        uint16_t c = static_cast<uint16_t>(s2 ^ kB[j]);
        if (weight12(c) <= 2) {
          x = c;
          y = static_cast<uint16_t>(0x800 >> j);
          found = true;
        }
      }
    }
  }
  if (!found) return -1;
  *data = static_cast<uint16_t>(r1 ^ x);
  return weight12(x) + weight12(y);
}

size_t encoded_size(size_t n) { return (n + 2) / 3 * 6; }

Bytes encode_bytes(ByteView data) {
  Bytes out;
  out.reserve(encoded_size(data.size()));
  for (size_t i = 0; i < data.size(); i += 3) {
    uint8_t b0 = data[i];
    uint8_t b1 = i + 1 < data.size() ? data[i + 1] : 0;
    uint8_t b2 = i + 2 < data.size() ? data[i + 2] : 0;
    uint16_t h0 = static_cast<uint16_t>(b0 << 4 | b1 >> 4);
    uint16_t h1 = static_cast<uint16_t>((b1 & 0x0F) << 8 | b2);
    for (uint16_t h : {h0, h1}) {
      uint32_t cw = encode(h);
      out.push_back(static_cast<uint8_t>(cw >> 16));
      out.push_back(static_cast<uint8_t>(cw >> 8));
      out.push_back(static_cast<uint8_t>(cw));
    }
  }
  return out;
}

int decode_bytes(ByteView in, size_t n, Bytes* out) {
  // encoded_size(n) >= 2n, so n > in.size() can never fit; checking first also keeps
  // encoded_size() from wrapping for absurd n (which would then allocate n bytes).
  if (n > in.size() || in.size() < encoded_size(n)) return -1;
  Bytes tmp;
  tmp.reserve(n + 2);
  int total = 0;
  for (size_t i = 0; i < encoded_size(n); i += 6) {
    uint16_t h[2];
    for (int k = 0; k < 2; ++k) {
      size_t o = i + 3 * static_cast<size_t>(k);
      uint32_t w = static_cast<uint32_t>(in[o]) << 16 | static_cast<uint32_t>(in[o + 1]) << 8 | in[o + 2];
      int e = decode(w, &h[k]);
      if (e < 0) return -1;
      total += e;
    }
    tmp.push_back(static_cast<uint8_t>(h[0] >> 4));
    tmp.push_back(static_cast<uint8_t>((h[0] & 0x0F) << 4 | h[1] >> 8));
    tmp.push_back(static_cast<uint8_t>(h[1]));
  }
  tmp.resize(n);
  *out = std::move(tmp);
  return total;
}

}  // namespace ember::fec::golay
