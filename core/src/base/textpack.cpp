#include "base/textpack.h"

#include <algorithm>
#include <array>

namespace ember {

namespace {

constexpr int kMaxLen = 15;

struct Code {
  std::array<uint16_t, 256> code{};
  std::array<uint8_t, 256> len{};
  // canonical decoding
  std::array<uint16_t, kMaxLen + 2> first{};
  std::array<uint16_t, kMaxLen + 2> count{};
  std::array<uint16_t, kMaxLen + 2> offset{};
  std::array<uint8_t, 256> sorted{};
};

void base_weights(std::array<uint32_t, 256>& w) {
  for (auto& x : w) x = 1;
  struct { char c; uint32_t f; } letters[] = {
      {'e', 1000}, {'t', 730}, {'a', 650}, {'o', 620}, {'i', 580}, {'n', 570}, {'s', 520},
      {'h', 480},  {'r', 480}, {'d', 340}, {'l', 330}, {'u', 230}, {'c', 220}, {'m', 200},
      {'w', 190},  {'f', 180}, {'g', 170}, {'y', 170}, {'p', 150}, {'b', 120}, {'v', 80},
      {'k', 60},   {'x', 15},  {'j', 12},  {'q', 10},  {'z', 8}};
  for (auto& l : letters) {
    w[static_cast<uint8_t>(l.c)] = l.f;
    w[static_cast<uint8_t>(l.c - 'a' + 'A')] = l.f / 14 + 2;
  }
  w['I'] += 40; w['T'] += 20; w['A'] += 15; w['S'] += 15; w['W'] += 15; w['O'] += 10;
  w[' '] = 1700;
  w['.'] = 120; w[','] = 100; w['\''] = 30; w['?'] = 30; w['!'] = 25; w['-'] = 20;
  w[':'] = 15; w['\n'] = 20; w['/'] = 10; w['"'] = 10; w['('] = 5; w[')'] = 5; w['@'] = 4;
  w['#'] = 3; w[';'] = 4; w['&'] = 3; w['+'] = 3; w['='] = 2; w['*'] = 3; w['_'] = 2;
  for (int d = '0'; d <= '9'; ++d) w[d] = 35;
  w['0'] = 55; w['1'] = 60; w['2'] = 45;
  for (int c = 0x21; c < 0x7F; ++c) if (w[c] < 2) w[c] = 2;
  // Common UTF-8 lead bytes (Latin-1 supplement, Cyrillic, punctuation like curly quotes).
  w[0xC3] = 3; w[0xD0] = 2; w[0xD1] = 2; w[0xE2] = 3; w[0x80] = 3;
}

// Huffman code lengths from weights (deterministic two-queue construction).
bool build_lengths(const std::array<uint32_t, 256>& w, std::array<uint8_t, 256>& len) {
  struct Node { uint64_t w; int left, right; };
  std::vector<Node> nodes;
  nodes.reserve(512);
  std::array<int, 256> order;
  for (int i = 0; i < 256; ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    return w[a] != w[b] ? w[a] < w[b] : a < b;
  });
  for (int i = 0; i < 256; ++i) nodes.push_back({w[order[i]], -1, order[i]});  // leaf: right=sym
  std::vector<int> q2;
  size_t i1 = 0, i2 = 0;
  auto pick = [&]() {
    if (i1 < 256 && (i2 >= q2.size() || nodes[i1].w <= nodes[q2[i2]].w)) return static_cast<int>(i1++);
    return q2[i2++];
  };
  for (int k = 0; k < 255; ++k) {
    int a = pick();
    int b = pick();
    nodes.push_back({nodes[a].w + nodes[b].w, a, b});
    q2.push_back(static_cast<int>(nodes.size() - 1));
  }
  // Depths by iterative traversal.
  std::vector<std::pair<int, int>> stack;
  stack.push_back({static_cast<int>(nodes.size() - 1), 0});
  int maxd = 0;
  while (!stack.empty()) {
    auto [n, d] = stack.back();
    stack.pop_back();
    if (nodes[n].left < 0) {
      len[nodes[n].right] = static_cast<uint8_t>(d);
      maxd = std::max(maxd, d);
    } else {
      stack.push_back({nodes[n].left, d + 1});
      stack.push_back({nodes[n].right, d + 1});
    }
  }
  return maxd <= kMaxLen;
}

Code build_code() {
  Code c;
  std::array<uint32_t, 256> w;
  base_weights(w);
  while (!build_lengths(w, c.len)) {
    for (auto& x : w) x = x / 2 + 1;  // flatten until the longest code fits
  }
  std::array<int, 256> order;
  for (int i = 0; i < 256; ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    return c.len[a] != c.len[b] ? c.len[a] < c.len[b] : a < b;
  });
  uint16_t code = 0;
  int prev_len = 0;
  for (int i = 0; i < 256; ++i) {
    int s = order[i];
    int l = c.len[s];
    if (i > 0) ++code;
    code = static_cast<uint16_t>(code << (l - prev_len));
    prev_len = l;
    c.code[s] = code;
    c.sorted[i] = static_cast<uint8_t>(s);
  }
  for (int s = 0; s < 256; ++s) c.count[c.len[s]]++;
  uint16_t first = 0, off = 0;
  for (int l = 1; l <= kMaxLen; ++l) {
    first = static_cast<uint16_t>((first + (l > 1 ? c.count[l - 1] : 0)) << (l > 1 ? 1 : 0));
    c.first[l] = first;
    c.offset[l] = off;
    off = static_cast<uint16_t>(off + c.count[l]);
  }
  return c;
}

const Code& code_table() {
  static const Code c = build_code();
  return c;
}

}  // namespace

Bytes textpack_compress(std::string_view text) {
  const Code& c = code_table();
  size_t bits = 0;
  for (char ch : text) bits += c.len[static_cast<uint8_t>(ch)];
  ByteWriter hdr;
  hdr.u8(kTextPackHuffman).varint(text.size());
  size_t huff_size = hdr.size() + (bits + 7) / 8;
  if (huff_size >= text.size() + 1) {
    Bytes out;
    out.reserve(text.size() + 1);
    out.push_back(kTextPackRaw);
    out.insert(out.end(), text.begin(), text.end());
    return out;
  }
  Bytes out = hdr.take();
  out.reserve(huff_size);
  uint32_t acc = 0;
  int nacc = 0;
  for (char ch : text) {
    uint8_t s = static_cast<uint8_t>(ch);
    acc = (acc << c.len[s]) | c.code[s];
    nacc += c.len[s];
    while (nacc >= 8) {
      nacc -= 8;
      out.push_back(static_cast<uint8_t>(acc >> nacc));
    }
    acc &= (1u << nacc) - 1;
  }
  if (nacc > 0) out.push_back(static_cast<uint8_t>(acc << (8 - nacc)));
  return out;
}

bool textpack_decompress(ByteView packed, size_t max_out, std::string& out) {
  out.clear();
  ByteReader r(packed);
  uint8_t method = r.u8();
  if (!r.ok()) return false;
  if (method == kTextPackRaw) {
    ByteView rest = r.rest();
    if (rest.size() > max_out) return false;
    out = rest.to_string();
    return true;
  }
  if (method != kTextPackHuffman) return false;
  uint64_t n = r.varint();
  if (!r.ok() || n > max_out) return false;
  ByteView bitsv = r.rest();
  // Every symbol needs at least one bit: reject impossible lengths before allocating.
  if (n > static_cast<uint64_t>(bitsv.size()) * 8) return false;
  const Code& c = code_table();
  out.reserve(static_cast<size_t>(n));
  size_t bitpos = 0;
  const size_t total_bits = bitsv.size() * 8;
  for (uint64_t k = 0; k < n; ++k) {
    uint32_t code = 0;
    int l = 0;
    for (;;) {
      if (bitpos >= total_bits || l >= kMaxLen) { out.clear(); return false; }
      code = (code << 1) | ((bitsv[bitpos >> 3] >> (7 - (bitpos & 7))) & 1u);
      ++bitpos;
      ++l;
      uint32_t idx = code - c.first[l];
      if (code >= c.first[l] && idx < c.count[l]) {
        out.push_back(static_cast<char>(c.sorted[c.offset[l] + idx]));
        break;
      }
    }
  }
  // Remaining bits must be zero padding within the final byte (canonical encoding, review L9).
  if (total_bits - bitpos >= 8) { out.clear(); return false; }
  for (size_t b = bitpos; b < total_bits; ++b)
    if ((bitsv[b >> 3] >> (7 - (b & 7))) & 1u) { out.clear(); return false; }
  return true;
}

}  // namespace ember
