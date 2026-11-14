#include "fec/interleave.h"

namespace ember::fec {

namespace {
struct Shape {
  size_t cols;      // C
  size_t rows;      // rows actually used, R'
  size_t last_len;  // elements in the last used row (1..C)
};
Shape shape(size_t n, size_t rows) {
  // More rows than elements is the same as one element per row; clamping first also keeps
  // n + rows - 1 from wrapping for absurd row counts (rows is caller / header supplied).
  if (rows > n) rows = n;
  Shape s;
  s.cols = (n + rows - 1) / rows;
  s.rows = (n + s.cols - 1) / s.cols;
  s.last_len = n - (s.rows - 1) * s.cols;
  return s;
}
}  // namespace

size_t interleave_index(size_t i, size_t n, size_t rows) {
  if (rows <= 1 || n <= 1 || i >= n) return i;
  Shape s = shape(n, rows);
  size_t r = i / s.cols, c = i % s.cols;
  size_t before = c * s.rows - (c > s.last_len ? c - s.last_len : 0);
  return before + r;
}

void interleave(const uint8_t* in, uint8_t* out, size_t n, size_t rows) {
  if (rows <= 1 || n <= 1) {
    for (size_t i = 0; i < n; ++i) out[i] = in[i];
    return;
  }
  Shape s = shape(n, rows);
  size_t o = 0;
  for (size_t c = 0; c < s.cols; ++c)
    for (size_t r = 0; r < s.rows; ++r) {
      size_t i = r * s.cols + c;
      if (i < n) out[o++] = in[i];
    }
}

void deinterleave(const uint8_t* in, uint8_t* out, size_t n, size_t rows) {
  if (rows <= 1 || n <= 1) {
    for (size_t i = 0; i < n; ++i) out[i] = in[i];
    return;
  }
  Shape s = shape(n, rows);
  size_t o = 0;
  for (size_t c = 0; c < s.cols; ++c)
    for (size_t r = 0; r < s.rows; ++r) {
      size_t i = r * s.cols + c;
      if (i < n) out[i] = in[o++];
    }
}

Bytes interleave(ByteView in, size_t rows) {
  Bytes out(in.size());
  if (!in.empty()) interleave(in.data(), out.data(), in.size(), rows);
  return out;
}

Bytes deinterleave(ByteView in, size_t rows) {
  Bytes out(in.size());
  if (!in.empty()) deinterleave(in.data(), out.data(), in.size(), rows);
  return out;
}

}  // namespace ember::fec
