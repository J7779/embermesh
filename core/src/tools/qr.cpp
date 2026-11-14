// QR Code encoder (ISO/IEC 18004): segment bits → pad → Reed-Solomon per block → interleave →
// place in zigzag → choose the mask with the lowest penalty → format/version.
//
// This file is derived from (a C++17 adaptation of parts of) the QR Code generator library by
// Project Nayuki: the module-count formula, the Reed-Solomon divisor/remainder and GF(2^8)
// multiply, the block interleaving and the ECC tables follow that library closely. It is used
// under the MIT License, whose notice follows:
//
//   QR Code generator library (C++)
//
//   Copyright (c) Project Nayuki. (MIT License)
//   https://www.nayuki.io/page/qr-code-generator-library
//
//   Permission is hereby granted, free of charge, to any person obtaining a copy of
//   this software and associated documentation files (the "Software"), to deal in
//   the Software without restriction, including without limitation the rights to
//   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
//   the Software, and to permit persons to whom the Software is furnished to do so,
//   subject to the following conditions:
//   - The above copyright notice and this permission notice shall be included in
//     all copies or substantial portions of the Software.
//   - The Software is provided "as is", without warranty of any kind, express or
//     implied, including but not limited to the warranties of merchantability,
//     fitness for a particular purpose and noninfringement. In no event shall the
//     authors or copyright holders be liable for any claim, damages or other
//     liability, whether in an action of contract, tort or otherwise, arising from,
//     out of or in connection with the Software or the use or other dealings in the
//     Software.
#include "tools/qr.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace ember {
namespace qr {

namespace {

// Indexed [ecc][version]; index 0 unused.
const int8_t kEccPerBlock[4][41] = {
    {-1, 7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28,
     28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26,
     26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28},
    {-1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30,
     28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {-1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28,
     30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
};
const int8_t kNumBlocks[4][41] = {
    {-1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 6, 6, 6, 6, 7, 8,
     8, 9, 9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},
    {-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5, 5, 8, 9, 9, 10, 10, 11, 13, 14, 16,
     17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},
    {-1, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8, 8, 10, 12, 16, 12, 17, 16, 18, 21, 20,
     23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},
    {-1, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25,
     25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},
};
// Format-info ECC indicator bits: L=01, M=00, Q=11, H=10.
const int kFormatBits[4] = {1, 0, 3, 2};

int raw_data_modules(int ver) {
  int r = (16 * ver + 128) * ver + 64;
  if (ver >= 2) {
    int na = ver / 7 + 2;
    r -= (25 * na - 10) * na - 55;
    if (ver >= 7) r -= 36;
  }
  return r;
}
int data_codewords(int ver, Ecc e) {
  int i = static_cast<int>(e);
  return raw_data_modules(ver) / 8 - kEccPerBlock[i][ver] * kNumBlocks[i][ver];
}
int byte_mode_bits(int ver, size_t len) { return 4 + (ver <= 9 ? 8 : 16) + 8 * static_cast<int>(len); }

// ---- Reed-Solomon over GF(2^8), polynomial 0x11D ----
uint8_t gf_mul(uint8_t x, uint8_t y) {
  int z = 0;
  for (int i = 7; i >= 0; --i) {
    z = (z << 1) ^ ((z >> 7) * 0x11D);
    z ^= ((y >> i) & 1) * x;
  }
  return static_cast<uint8_t>(z);
}
std::vector<uint8_t> rs_divisor(int degree) {
  std::vector<uint8_t> r(static_cast<size_t>(degree), 0);
  r[static_cast<size_t>(degree) - 1] = 1;
  uint8_t root = 1;
  for (int i = 0; i < degree; ++i) {
    for (size_t j = 0; j < r.size(); ++j) {
      r[j] = gf_mul(r[j], root);
      if (j + 1 < r.size()) r[j] ^= r[j + 1];
    }
    root = gf_mul(root, 0x02);
  }
  return r;
}
std::vector<uint8_t> rs_remainder(const uint8_t* data, size_t n, const std::vector<uint8_t>& div) {
  std::vector<uint8_t> r(div.size(), 0);
  for (size_t k = 0; k < n; ++k) {
    uint8_t factor = static_cast<uint8_t>(data[k] ^ r[0]);
    r.erase(r.begin());
    r.push_back(0);
    for (size_t i = 0; i < r.size(); ++i) r[i] ^= gf_mul(div[i], factor);
  }
  return r;
}

class Builder {
 public:
  Builder(int ver, Ecc e) : ver_(ver), ecc_(e), size_(ver * 4 + 17),
      m_(static_cast<size_t>(size_) * size_, 0), fn_(static_cast<size_t>(size_) * size_, 0) {}

  void draw_function_patterns() {
    for (int i = 0; i < size_; ++i) {
      set_fn(6, i, i % 2 == 0);
      set_fn(i, 6, i % 2 == 0);
    }
    finder(3, 3);
    finder(size_ - 4, 3);
    finder(3, size_ - 4);
    std::vector<int> al = alignment_positions();
    size_t n = al.size();
    for (size_t i = 0; i < n; ++i)
      for (size_t j = 0; j < n; ++j) {
        if ((i == 0 && j == 0) || (i == 0 && j == n - 1) || (i == n - 1 && j == 0)) continue;
        alignment(al[i], al[j]);
      }
    draw_format(0);  // reserve; real bits drawn later
    draw_version();
  }

  void draw_codewords(const std::vector<uint8_t>& data) {
    size_t i = 0, nbits = data.size() * 8;
    for (int right = size_ - 1; right >= 1; right -= 2) {
      if (right == 6) right = 5;
      for (int vert = 0; vert < size_; ++vert) {
        for (int j = 0; j < 2; ++j) {
          int x = right - j;
          bool upward = ((right + 1) & 2) == 0;
          int y = upward ? size_ - 1 - vert : vert;
          if (!fn(x, y) && i < nbits) {
            mod(x, y) = static_cast<uint8_t>((data[i >> 3] >> (7 - (i & 7))) & 1);
            ++i;
          }
        }
      }
    }
  }

  void apply_mask(int mask) {
    for (int y = 0; y < size_; ++y)
      for (int x = 0; x < size_; ++x) {
        bool inv;
        switch (mask) {
          case 0: inv = (x + y) % 2 == 0; break;
          case 1: inv = y % 2 == 0; break;
          case 2: inv = x % 3 == 0; break;
          case 3: inv = (x + y) % 3 == 0; break;
          case 4: inv = (x / 3 + y / 2) % 2 == 0; break;
          case 5: inv = x * y % 2 + x * y % 3 == 0; break;
          case 6: inv = (x * y % 2 + x * y % 3) % 2 == 0; break;
          default: inv = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
        }
        if (inv && !fn(x, y)) mod(x, y) ^= 1;
      }
  }

  void draw_format(int mask) {
    int data = kFormatBits[static_cast<int>(ecc_)] << 3 | mask;
    int rem = data;
    for (int i = 0; i < 10; ++i) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    int bits = (data << 10 | rem) ^ 0x5412;
    for (int i = 0; i <= 5; ++i) set_fn(8, i, bit(bits, i));
    set_fn(8, 7, bit(bits, 6));
    set_fn(8, 8, bit(bits, 7));
    set_fn(7, 8, bit(bits, 8));
    for (int i = 9; i < 15; ++i) set_fn(14 - i, 8, bit(bits, i));
    for (int i = 0; i < 8; ++i) set_fn(size_ - 1 - i, 8, bit(bits, i));
    for (int i = 8; i < 15; ++i) set_fn(8, size_ - 15 + i, bit(bits, i));
    set_fn(8, size_ - 8, true);  // dark module
  }

  long penalty() const {
    long r = 0;
    for (int pass = 0; pass < 2; ++pass) {
      for (int a = 0; a < size_; ++a) {
        bool run_color = false;
        int run = 0;
        std::array<int, 7> hist{};
        for (int b = 0; b < size_; ++b) {
          bool c = pass == 0 ? get(b, a) : get(a, b);
          if (c == run_color) {
            ++run;
            if (run == 5) r += 3;
            else if (run > 5) r += 1;
          } else {
            add_history(run, hist);
            if (!run_color) r += count_patterns(hist) * 40;
            run_color = c;
            run = 1;
          }
        }
        r += terminate_and_count(run_color, run, hist) * 40;
      }
    }
    for (int y = 0; y < size_ - 1; ++y)
      for (int x = 0; x < size_ - 1; ++x) {
        bool c = get(x, y);
        if (c == get(x + 1, y) && c == get(x, y + 1) && c == get(x + 1, y + 1)) r += 3;
      }
    long dark = 0;
    for (uint8_t v : m_) dark += v;
    long total = static_cast<long>(size_) * size_;
    long k = (std::labs(dark * 20 - total * 10) + total - 1) / total - 1;
    r += k * 10;
    return r;
  }

  int size() const { return size_; }
  std::vector<uint8_t> take() { return std::move(m_); }

 private:
  static bool bit(int v, int i) { return ((v >> i) & 1) != 0; }
  uint8_t& mod(int x, int y) { return m_[static_cast<size_t>(y) * size_ + x]; }
  bool get(int x, int y) const { return m_[static_cast<size_t>(y) * size_ + x] != 0; }
  bool fn(int x, int y) const { return fn_[static_cast<size_t>(y) * size_ + x] != 0; }
  void set_fn(int x, int y, bool dark) {
    mod(x, y) = dark ? 1 : 0;
    fn_[static_cast<size_t>(y) * size_ + x] = 1;
  }
  void finder(int cx, int cy) {
    for (int dy = -4; dy <= 4; ++dy)
      for (int dx = -4; dx <= 4; ++dx) {
        int d = std::max(std::abs(dx), std::abs(dy));
        int x = cx + dx, y = cy + dy;
        if (x >= 0 && x < size_ && y >= 0 && y < size_) set_fn(x, y, d != 2 && d != 4);
      }
  }
  void alignment(int cx, int cy) {
    for (int dy = -2; dy <= 2; ++dy)
      for (int dx = -2; dx <= 2; ++dx) set_fn(cx + dx, cy + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
  }
  std::vector<int> alignment_positions() const {
    if (ver_ == 1) return {};
    int na = ver_ / 7 + 2;
    int step = (ver_ * 8 + na * 3 + 5) / (na * 4 - 4) * 2;
    std::vector<int> r(static_cast<size_t>(na));
    r[0] = 6;
    for (int i = na - 1, pos = size_ - 7; i >= 1; --i, pos -= step) r[static_cast<size_t>(i)] = pos;
    return r;
  }
  void draw_version() {
    if (ver_ < 7) return;
    int rem = ver_;
    for (int i = 0; i < 12; ++i) rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
    long bits = static_cast<long>(ver_) << 12 | rem;
    for (int i = 0; i < 18; ++i) {
      bool b = ((bits >> i) & 1) != 0;
      int a = size_ - 11 + i % 3, c = i / 3;
      set_fn(a, c, b);
      set_fn(c, a, b);
    }
  }
  void add_history(int run, std::array<int, 7>& h) const {
    if (h[0] == 0) run += size_;
    for (size_t i = 6; i > 0; --i) h[i] = h[i - 1];
    h[0] = run;
  }
  static int count_patterns(const std::array<int, 7>& h) {
    int n = h[1];
    bool core = n > 0 && h[2] == n && h[3] == n * 3 && h[4] == n && h[5] == n;
    return (core && h[0] >= n * 4 && h[6] >= n ? 1 : 0) + (core && h[6] >= n * 4 && h[0] >= n ? 1 : 0);
  }
  int terminate_and_count(bool color, int run, std::array<int, 7>& h) const {
    if (color) {
      add_history(run, h);
      run = 0;
    }
    run += size_;
    add_history(run, h);
    return count_patterns(h);
  }

  int ver_;
  Ecc ecc_;
  int size_;
  std::vector<uint8_t> m_;
  std::vector<uint8_t> fn_;
};

std::vector<uint8_t> add_ecc_and_interleave(const std::vector<uint8_t>& data, int ver, Ecc e) {
  int ei = static_cast<int>(e);
  int nblocks = kNumBlocks[ei][ver];
  int ecc_len = kEccPerBlock[ei][ver];
  int raw = raw_data_modules(ver) / 8;
  int nshort = nblocks - raw % nblocks;
  int short_len = raw / nblocks;
  std::vector<std::vector<uint8_t>> blocks;
  std::vector<uint8_t> div = rs_divisor(ecc_len);
  size_t k = 0;
  for (int i = 0; i < nblocks; ++i) {
    int dlen = short_len - ecc_len + (i < nshort ? 0 : 1);
    std::vector<uint8_t> blk(data.begin() + static_cast<long>(k), data.begin() + static_cast<long>(k + dlen));
    k += static_cast<size_t>(dlen);
    std::vector<uint8_t> ecc = rs_remainder(blk.data(), blk.size(), div);
    if (i < nshort) blk.push_back(0);  // placeholder so all blocks have equal length
    blk.insert(blk.end(), ecc.begin(), ecc.end());
    blocks.push_back(std::move(blk));
  }
  std::vector<uint8_t> out;
  out.reserve(static_cast<size_t>(raw));
  for (size_t i = 0; i < blocks[0].size(); ++i)
    for (size_t j = 0; j < blocks.size(); ++j)
      if (i != static_cast<size_t>(short_len - ecc_len) || static_cast<int>(j) >= nshort) out.push_back(blocks[j][i]);
  return out;
}

}  // namespace

int byte_capacity(int ver, Ecc e) {
  if (ver < 1 || ver > 40) return 0;
  int bits = data_codewords(ver, e) * 8 - 4 - (ver <= 9 ? 8 : 16);
  return bits / 8;
}

bool parse_ecc(const std::string& s, Ecc* out) {
  if (s.size() != 1) return false;
  switch (s[0]) {
    case 'L': case 'l': *out = Ecc::L; return true;
    case 'M': case 'm': *out = Ecc::M; return true;
    case 'Q': case 'q': *out = Ecc::Q; return true;
    case 'H': case 'h': *out = Ecc::H; return true;
    default: return false;
  }
}

std::vector<std::string> QrCode::rows() const {
  std::vector<std::string> r;
  r.reserve(static_cast<size_t>(size));
  for (int y = 0; y < size; ++y) {
    std::string s(static_cast<size_t>(size), '0');
    for (int x = 0; x < size; ++x)
      if (get(x, y)) s[static_cast<size_t>(x)] = '1';
    r.push_back(std::move(s));
  }
  return r;
}

bool encode(ByteView data, const EncodeOptions& opt, QrCode* out, std::string* error) {
  auto fail = [&](const char* m) { if (error) *error = m; return false; };
  int ei = static_cast<int>(opt.ecc);
  if (ei < 0 || ei > 3) return fail("invalid ECC level");
  if (opt.min_version < 1 || opt.max_version > 40 || opt.min_version > opt.max_version)
    return fail("invalid version range (1-40)");
  if (opt.force_mask < -1 || opt.force_mask > 7) return fail("mask must be -1 (auto) or 0-7");
  if (data.size() > 2953) return fail("data too long for a QR code (max 2953 bytes at ECC L)");

  int ver = 0;
  for (int v = opt.min_version; v <= opt.max_version; ++v) {
    if (byte_mode_bits(v, data.size()) <= data_codewords(v, opt.ecc) * 8) { ver = v; break; }
  }
  if (!ver) return fail("data too long for the requested ECC level / version range");
  Ecc ecc = opt.ecc;
  if (opt.boost_ecc) {
    for (int e = static_cast<int>(ecc) + 1; e <= 3; ++e)
      if (byte_mode_bits(ver, data.size()) <= data_codewords(ver, static_cast<Ecc>(e)) * 8) ecc = static_cast<Ecc>(e);
  }

  // Bit stream: mode 0100, count, bytes, terminator, pad to byte, pad codewords.
  int cap_bits = data_codewords(ver, ecc) * 8;
  std::vector<uint8_t> bits;
  bits.reserve(static_cast<size_t>(cap_bits));
  auto put = [&bits](uint32_t v, int n) { for (int i = n - 1; i >= 0; --i) bits.push_back((v >> i) & 1); };
  put(0x4, 4);
  put(static_cast<uint32_t>(data.size()), ver <= 9 ? 8 : 16);
  for (uint8_t b : data) put(b, 8);
  int term = std::min(4, cap_bits - static_cast<int>(bits.size()));
  put(0, term);
  put(0, static_cast<int>((8 - bits.size() % 8) % 8));
  for (uint8_t p = 0xEC; static_cast<int>(bits.size()) < cap_bits; p ^= 0xEC ^ 0x11) put(p, 8);
  std::vector<uint8_t> cw(bits.size() / 8, 0);
  for (size_t i = 0; i < bits.size(); ++i) cw[i >> 3] |= static_cast<uint8_t>(bits[i] << (7 - (i & 7)));

  std::vector<uint8_t> all = add_ecc_and_interleave(cw, ver, ecc);
  Builder b(ver, ecc);
  b.draw_function_patterns();
  b.draw_codewords(all);
  int mask = opt.force_mask;
  if (mask < 0) {
    long best = -1;
    for (int m = 0; m < 8; ++m) {
      b.apply_mask(m);
      b.draw_format(m);
      long p = b.penalty();
      if (best < 0 || p < best) { best = p; mask = m; }
      b.apply_mask(m);  // XOR undo
    }
  }
  b.apply_mask(mask);
  b.draw_format(mask);
  out->version = ver;
  out->size = b.size();
  out->ecc = ecc;
  out->mask = mask;
  out->modules = b.take();
  return true;
}

}  // namespace qr
}  // namespace ember
