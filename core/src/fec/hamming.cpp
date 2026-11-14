#include "fec/hamming.h"

namespace ember::fec::hamming {

namespace {
unsigned syndrome7(uint8_t cw) {
  unsigned s = 0;
  for (unsigned pos = 1; pos <= 7; ++pos)
    if (cw & (1u << (pos - 1))) s ^= pos;
  return s;
}
uint8_t extract(uint8_t cw) {
  return static_cast<uint8_t>(((cw >> 2) & 1) | (((cw >> 4) & 1) << 1) | (((cw >> 5) & 1) << 2) | (((cw >> 6) & 1) << 3));
}
unsigned parity8(uint8_t v) {
  v ^= v >> 4;
  v ^= v >> 2;
  v ^= v >> 1;
  return v & 1u;
}
}  // namespace

uint8_t encode74(uint8_t n) {
  unsigned d0 = n & 1, d1 = (n >> 1) & 1, d2 = (n >> 2) & 1, d3 = (n >> 3) & 1;
  unsigned p1 = d0 ^ d1 ^ d3, p2 = d0 ^ d2 ^ d3, p3 = d1 ^ d2 ^ d3;
  return static_cast<uint8_t>(p1 | p2 << 1 | d0 << 2 | p3 << 3 | d1 << 4 | d2 << 5 | d3 << 6);
}

int decode74(uint8_t cw, uint8_t* nibble) {
  cw &= 0x7F;
  unsigned s = syndrome7(cw);
  int fixed = 0;
  if (s) {
    cw ^= static_cast<uint8_t>(1u << (s - 1));
    fixed = 1;
  }
  *nibble = extract(cw);
  return fixed;
}

uint8_t encode84(uint8_t n) {
  uint8_t c = encode74(n);
  return static_cast<uint8_t>(c | parity8(c) << 7);
}

int decode84(uint8_t cw, uint8_t* nibble) {
  unsigned s = syndrome7(cw & 0x7F);
  unsigned p = parity8(cw);
  if (s == 0 && p == 0) {
    *nibble = extract(cw);
    return 0;
  }
  if (p == 1) {  // odd number of errors: assume one
    if (s) cw ^= static_cast<uint8_t>(1u << (s - 1));
    *nibble = extract(cw);
    return 1;
  }
  return -1;  // s != 0, even parity: two errors
}

Bytes encode84_bytes(ByteView data) {
  Bytes out;
  out.reserve(data.size() * 2);
  for (uint8_t b : data) {
    out.push_back(encode84(static_cast<uint8_t>(b >> 4)));
    out.push_back(encode84(static_cast<uint8_t>(b & 0x0F)));
  }
  return out;
}

int decode84_bytes(ByteView in, Bytes* out) {
  if (in.size() % 2) return -1;
  Bytes tmp;
  tmp.reserve(in.size() / 2);
  int total = 0;
  for (size_t i = 0; i < in.size(); i += 2) {
    uint8_t hi = 0, lo = 0;
    int a = decode84(in[i], &hi), b = decode84(in[i + 1], &lo);
    if (a < 0 || b < 0) return -1;
    total += a + b;
    tmp.push_back(static_cast<uint8_t>(hi << 4 | lo));
  }
  *out = std::move(tmp);
  return total;
}

}  // namespace ember::fec::hamming
