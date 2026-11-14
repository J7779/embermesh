#include "guides/deflate.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace ember::guides {

// ---------------------------------------------------------------------------------------------
// CRC-32, slice-by-4

namespace {
struct CrcTables {
  uint32_t t[4][256];
};
constexpr CrcTables make_crc_tables() {
  CrcTables r{};
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t c = i;
    for (int k = 0; k < 8; ++k) c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    r.t[0][i] = c;
  }
  for (uint32_t i = 0; i < 256; ++i)
    for (int k = 1; k < 4; ++k) r.t[k][i] = (r.t[k - 1][i] >> 8) ^ r.t[0][r.t[k - 1][i] & 0xFF];
  return r;
}
constexpr CrcTables kCrc = make_crc_tables();
}  // namespace

uint32_t crc32_fast(ByteView data, uint32_t prev) {
  uint32_t crc = ~prev;
  const uint8_t* p = data.data();
  size_t n = data.size();
  while (n >= 4) {
    crc ^= static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
           static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
    crc = kCrc.t[3][crc & 0xFF] ^ kCrc.t[2][(crc >> 8) & 0xFF] ^ kCrc.t[1][(crc >> 16) & 0xFF] ^
          kCrc.t[0][crc >> 24];
    p += 4;
    n -= 4;
  }
  while (n--) crc = kCrc.t[0][(crc ^ *p++) & 0xFF] ^ (crc >> 8);
  return ~crc;
}

// ---------------------------------------------------------------------------------------------
// Shared tables

namespace {
constexpr uint16_t kLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
                                   31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr uint8_t kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                   2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr uint16_t kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                                    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                                    6145, 8193, 12289, 16385, 24577};
constexpr uint8_t kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                                    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
constexpr uint8_t kClOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

void fixed_lengths(uint8_t* lit, uint8_t* dist) {
  int i = 0;
  for (; i < 144; ++i) lit[i] = 8;
  for (; i < 256; ++i) lit[i] = 9;
  for (; i < 280; ++i) lit[i] = 7;
  for (; i < 288; ++i) lit[i] = 8;
  for (i = 0; i < 30; ++i) dist[i] = 5;
}

uint32_t reverse_bits(uint32_t code, int len) {
  uint32_t r = 0;
  for (int i = 0; i < len; ++i) { r = (r << 1) | (code & 1); code >>= 1; }
  return r;
}

// ---------------------------------------------------------------------------------------------
// Decoder

constexpr int kFastBits = 9;

struct Huff {
  uint16_t count[16];
  uint16_t symbol[288];
  uint16_t fast[1 << kFastBits];  // 0 = use slow path; else sym | len << 12
};

// Returns 0 for a complete code, > 0 if incomplete, < 0 if over-subscribed.
int huff_build(Huff& h, const uint8_t* lens, int n) {
  std::memset(h.count, 0, sizeof h.count);
  std::memset(h.fast, 0, sizeof h.fast);
  for (int s = 0; s < n; ++s) h.count[lens[s]]++;
  if (h.count[0] == n) return 0;  // no codes: complete (but any decode will fail)
  int left = 1;
  for (int len = 1; len < 16; ++len) {
    left <<= 1;
    left -= h.count[len];
    if (left < 0) return left;
  }
  uint16_t offs[16];
  offs[1] = 0;
  for (int len = 1; len < 15; ++len) offs[len + 1] = static_cast<uint16_t>(offs[len] + h.count[len]);
  for (int s = 0; s < n; ++s)
    if (lens[s]) h.symbol[offs[lens[s]]++] = static_cast<uint16_t>(s);
  // Fast table for codes up to kFastBits long.
  uint32_t code = 0;
  int idx = 0;
  for (int len = 1; len < 16; ++len) {
    for (int j = 0; j < h.count[len]; ++j) {
      uint16_t sym = h.symbol[idx++];
      if (len <= kFastBits) {
        uint32_t rev = reverse_bits(code, len);
        for (uint32_t k = rev; k < (1u << kFastBits); k += 1u << len)
          h.fast[k] = static_cast<uint16_t>(sym | (len << 12));
      }
      ++code;
    }
    code <<= 1;
  }
  return left;
}

// Most blocks a stream may have: an encoder emits a new block every few KiB of output at the
// very least (ours: every 32768 symbols, zlib: every 16 K symbols), so expected/1024 + 16 is
// generous for any real stream while bounding the per-block table-building work on hostile input
// (security review H6: one literal per block made an 8 MiB body take seconds).
size_t max_blocks(size_t expected) { return expected / 1024 + 16; }

class Inflater {
 public:
  // cap < expected: prefix mode (stop after cap bytes or at the end of the input).
  Inflater(ByteView in, size_t expected, size_t cap, std::string& out)
      : in_(in), expected_(expected), cap_(cap < expected ? cap : expected), prefix_(cap < expected), out_(out) {}

  bool run() {
    out_.assign(cap_, '\0');
    dst_ = cap_ ? &out_[0] : nullptr;
    int last;
    size_t blocks = 0;
    const size_t block_cap = max_blocks(expected_);
    do {
      if (++blocks > block_cap) return false;
      last = static_cast<int>(bits(1));
      uint32_t type = bits(2);
      if (bad_) return stop_ok();
      bool ok;
      if (type == 0) ok = stored();
      else if (type == 1) ok = fixed();
      else if (type == 2) ok = dynamic();
      else ok = false;
      if (done_) return finish_prefix();
      if (!ok || bad_) return stop_ok();
    } while (!last);
    if (prefix_) return false;  // stream ended before even the prefix: shorter than announced
    return o_ == expected_;
  }

 private:
  bool need(int k) {
    while (cnt_ < k) {
      if (pos_ >= in_.size()) { bad_ = true; eof_ = true; return false; }
      buf_ |= static_cast<uint64_t>(in_[pos_++]) << cnt_;
      cnt_ += 8;
    }
    return true;
  }
  uint32_t bits(int k) {
    if (k == 0) return 0;
    if (!need(k)) return 0;
    uint32_t v = static_cast<uint32_t>(buf_ & ((1ull << k) - 1));
    buf_ >>= k;
    cnt_ -= k;
    return v;
  }
  void refill() {
    while (cnt_ <= 56 && pos_ < in_.size()) {
      buf_ |= static_cast<uint64_t>(in_[pos_++]) << cnt_;
      cnt_ += 8;
    }
  }
  int decode(const Huff& h) {
    refill();
    if (cnt_ >= kFastBits) {
      uint16_t e = h.fast[buf_ & ((1u << kFastBits) - 1)];
      if (e) {
        int len = e >> 12;
        buf_ >>= len;
        cnt_ -= len;
        return e & 0x0FFF;
      }
    }
    int code = 0, first = 0, index = 0;
    if (cnt_ == 0 && pos_ >= in_.size()) eof_ = true;
    for (int len = 1; len < 16; ++len) {
      code |= static_cast<int>(bits(1));
      if (bad_) return -1;
      int count = h.count[len];
      if (code - count < first) return h.symbol[index + (code - first)];
      index += count;
      first += count;
      first <<= 1;
      code <<= 1;
    }
    bad_ = true;
    return -1;
  }
  bool emit(char c) {
    if (o_ >= expected_) return false;
    if (o_ >= cap_) { done_ = true; return false; }
    dst_[o_++] = c;
    return true;
  }

  bool stored() {
    // Discard bits up to the byte boundary.
    int drop = cnt_ & 7;
    buf_ >>= drop;
    cnt_ -= drop;
    uint32_t len = bits(16);
    uint32_t nlen = bits(16);
    if (bad_ || (len ^ 0xFFFF) != nlen) return false;
    if (len > expected_ - o_) return false;
    while (len && cnt_ >= 8) {
      char c = static_cast<char>(bits(8));
      if (o_ >= cap_) { done_ = true; return false; }
      dst_[o_++] = c;
      --len;
    }
    size_t take = len;
    if (take > cap_ - o_) take = cap_ - o_;
    if (take > in_.size() - pos_) {
      if (!prefix_) return false;
      take = in_.size() - pos_;
      eof_ = true;
    }
    if (take) std::memcpy(dst_ + o_, in_.data() + pos_, take);
    o_ += take;
    pos_ += take;
    if (take < len) {
      if (o_ >= cap_) done_ = true;
      return false;
    }
    return true;
  }

  bool codes(const Huff& lit, const Huff& dist) {
    for (;;) {
      int sym = decode(lit);
      if (sym < 0) return false;
      if (sym < 256) {
        if (!emit(static_cast<char>(sym))) return false;
      } else if (sym == 256) {
        return true;
      } else {
        sym -= 257;
        if (sym >= 29) return false;
        uint32_t len = kLenBase[sym] + bits(kLenExtra[sym]);
        int ds = decode(dist);
        if (ds < 0 || ds >= 30) return false;
        uint32_t d = kDistBase[ds] + bits(kDistExtra[ds]);
        if (bad_) return false;
        if (d > o_) return false;
        if (len > expected_ - o_) return false;
        uint32_t n = len;
        if (n > cap_ - o_) n = static_cast<uint32_t>(cap_ - o_);
        const char* from = dst_ + (o_ - d);
        char* to = dst_ + o_;
        for (uint32_t k = 0; k < n; ++k) to[k] = from[k];  // may overlap: byte order matters
        o_ += n;
        if (n < len) { done_ = true; return false; }
      }
    }
  }

  bool fixed() {
    if (!fixed_built_) {  // built once per stream, not once per block
      uint8_t lit[288], dist[30];
      fixed_lengths(lit, dist);
      huff_build(fixed_lit_, lit, 288);
      huff_build(fixed_dist_, dist, 30);
      fixed_built_ = true;
    }
    return codes(fixed_lit_, fixed_dist_);
  }

  bool dynamic() {
    uint32_t nlen = bits(5) + 257, ndist = bits(5) + 1, ncode = bits(4) + 4;
    if (bad_ || nlen > 286 || ndist > 30) return false;
    uint8_t lens[320] = {0};
    for (uint32_t i = 0; i < ncode; ++i) lens[kClOrder[i]] = static_cast<uint8_t>(bits(3));
    if (bad_) return false;
    Huff hc;
    if (huff_build(hc, lens, 19) != 0) return false;
    std::memset(lens, 0, sizeof lens);
    uint32_t idx = 0;
    while (idx < nlen + ndist) {
      int sym = decode(hc);
      if (sym < 0) return false;
      if (sym < 16) {
        lens[idx++] = static_cast<uint8_t>(sym);
      } else {
        uint8_t v = 0;
        uint32_t rep;
        if (sym == 16) {
          if (idx == 0) return false;
          v = lens[idx - 1];
          rep = 3 + bits(2);
        } else if (sym == 17) {
          rep = 3 + bits(3);
        } else {
          rep = 11 + bits(7);
        }
        if (bad_ || idx + rep > nlen + ndist) return false;
        while (rep--) lens[idx++] = v;
      }
    }
    if (lens[256] == 0) return false;
    Huff hl, hd;
    int e = huff_build(hl, lens, static_cast<int>(nlen));
    if (e < 0 || (e > 0 && nlen - hl.count[0] != 1)) return false;
    e = huff_build(hd, lens + nlen, static_cast<int>(ndist));
    if (e < 0 || (e > 0 && ndist - hd.count[0] != 1)) return false;
    return codes(hl, hd);
  }

  // Prefix mode: the input ran out (we were handed a cut-off slice) -> keep what we have.
  bool stop_ok() {
    if (prefix_ && eof_) return finish_prefix();
    return false;
  }
  bool finish_prefix() {
    if (!prefix_) return false;
    out_.resize(o_);
    return true;
  }

  ByteView in_;
  size_t expected_;
  size_t cap_;
  bool prefix_;
  bool done_ = false;  // prefix mode: cap reached
  bool eof_ = false;   // ran out of input
  bool fixed_built_ = false;
  Huff fixed_lit_, fixed_dist_;
  std::string& out_;
  char* dst_ = nullptr;
  size_t o_ = 0;
  size_t pos_ = 0;
  uint64_t buf_ = 0;
  int cnt_ = 0;
  bool bad_ = false;
};

// ---------------------------------------------------------------------------------------------
// Encoder

struct Sym {
  uint16_t lit_or_len;  // < 256 literal (dist == 0) or match length 3..258
  uint16_t dist;        // 0 for literals, else 1..32768 (32768 stored as 0x8000)
};

class BitWriter {
 public:
  void put(uint32_t v, int n) {
    acc_ |= static_cast<uint64_t>(v) << cnt_;
    cnt_ += n;
    while (cnt_ >= 8) { out_.push_back(static_cast<uint8_t>(acc_)); acc_ >>= 8; cnt_ -= 8; }
  }
  void align() { if (cnt_) put(0, 8 - cnt_); }
  void raw(const uint8_t* p, size_t n) { out_.insert(out_.end(), p, p + n); }
  Bytes take() { align(); return std::move(out_); }

 private:
  Bytes out_;
  uint64_t acc_ = 0;
  int cnt_ = 0;
};

// Length-limited Huffman code lengths (deterministic).
void build_lengths(const uint32_t* freq, int n, int limit, uint8_t* lens) {
  std::memset(lens, 0, static_cast<size_t>(n));
  std::vector<std::pair<uint32_t, int>> leaves;
  for (int s = 0; s < n; ++s)
    if (freq[s]) leaves.push_back({freq[s], s});
  if (leaves.empty()) return;
  if (leaves.size() == 1) {
    lens[leaves[0].second] = 1;
    lens[leaves[0].second == 0 ? 1 : 0] = 1;
    return;
  }
  std::sort(leaves.begin(), leaves.end());
  size_t m = leaves.size();
  std::vector<uint64_t> w(2 * m - 1);
  std::vector<size_t> parent(2 * m - 1, 0);
  for (size_t i = 0; i < m; ++i) w[i] = leaves[i].first;
  size_t q1 = 0, q2 = m, next = m;
  auto pick = [&]() {
    if (q1 < m && (q2 >= next || w[q1] <= w[q2])) return q1++;
    return q2++;
  };
  for (size_t k = 0; k + 1 < m; ++k) {
    size_t a = pick(), b = pick();
    w[next] = w[a] + w[b];
    parent[a] = parent[b] = next;
    ++next;
  }
  std::vector<int> depth(2 * m - 1, 0);
  for (size_t id = 2 * m - 2; id-- > 0;) depth[id] = depth[parent[id]] + 1;
  std::vector<uint32_t> bl(static_cast<size_t>(limit) + 1, 0);
  for (size_t i = 0; i < m; ++i) bl[static_cast<size_t>(std::min(depth[i], limit))]++;
  uint64_t total = 0;
  for (int i = 1; i <= limit; ++i) total += static_cast<uint64_t>(bl[static_cast<size_t>(i)]) << (limit - i);
  while (total > (1ull << limit)) {
    bl[static_cast<size_t>(limit)]--;
    for (int i = limit - 1; i > 0; --i) {
      if (bl[static_cast<size_t>(i)]) {
        bl[static_cast<size_t>(i)]--;
        bl[static_cast<size_t>(i + 1)] += 2;
        break;
      }
    }
    --total;
  }
  // Lowest frequencies (front of `leaves`) get the longest codes.
  size_t idx = 0;
  for (int len = limit; len >= 1; --len)
    for (uint32_t k = 0; k < bl[static_cast<size_t>(len)]; ++k) lens[leaves[idx++].second] = static_cast<uint8_t>(len);
}

void canonical_codes(const uint8_t* lens, int n, uint16_t* codes) {
  uint32_t bl[16] = {0}, next[16] = {0};
  for (int s = 0; s < n; ++s) bl[lens[s]]++;
  bl[0] = 0;
  uint32_t code = 0;
  for (int len = 1; len < 16; ++len) { code = (code + bl[len - 1]) << 1; next[len] = code; }
  for (int s = 0; s < n; ++s)
    if (lens[s]) codes[s] = static_cast<uint16_t>(reverse_bits(next[lens[s]]++, lens[s]));
}

int len_code(uint32_t len) {
  int i = 28;
  while (kLenBase[i] > len) --i;
  return i;
}
int dist_code(uint32_t d) {
  int i = 29;
  while (kDistBase[i] > d) --i;
  return i;
}

struct ClSym {
  uint8_t sym, extra_bits;
  uint8_t extra;
};

void rle_lengths(const uint8_t* L, size_t total, std::vector<ClSym>& out) {
  size_t i = 0;
  while (i < total) {
    uint8_t v = L[i];
    size_t run = 1;
    while (i + run < total && L[i + run] == v) ++run;
    i += run;
    if (v == 0) {
      while (run >= 3) {
        size_t r = std::min<size_t>(run, 138);
        if (r >= 11) out.push_back({18, 7, static_cast<uint8_t>(r - 11)});
        else out.push_back({17, 3, static_cast<uint8_t>(r - 3)});
        run -= r;
      }
      while (run--) out.push_back({0, 0, 0});
    } else {
      out.push_back({v, 0, 0});
      --run;
      while (run >= 3) {
        size_t r = std::min<size_t>(run, 6);
        out.push_back({16, 2, static_cast<uint8_t>(r - 3)});
        run -= r;
      }
      while (run--) out.push_back({v, 0, 0});
    }
  }
}

void lz77(ByteView in, std::vector<Sym>& syms) {
  const size_t n = in.size();
  const uint8_t* p = in.data();
  constexpr int kHashBits = 15;
  constexpr size_t kWindow = 32768;
  constexpr int kMaxChain = 192;
  std::vector<int32_t> head(1u << kHashBits, -1);
  std::vector<int32_t> prev(n ? n : 1, -1);
  size_t ins = 0;
  auto hash = [&](size_t i) {
    return ((static_cast<uint32_t>(p[i]) << 10) ^ (static_cast<uint32_t>(p[i + 1]) << 5) ^ p[i + 2]) &
           ((1u << kHashBits) - 1);
  };
  auto advance_to = [&](size_t target) {
    while (ins < target) {
      if (ins + 2 < n) {
        uint32_t h = hash(ins);
        prev[ins] = head[h];
        head[h] = static_cast<int32_t>(ins);
      }
      ++ins;
    }
  };
  auto find = [&](size_t i, uint32_t& best_len, uint32_t& best_dist) {
    best_len = 0;
    best_dist = 0;
    if (i + 3 > n) return;
    size_t maxlen = std::min<size_t>(258, n - i);
    int32_t cand = head[hash(i)];
    int chain = kMaxChain;
    while (cand >= 0 && chain-- > 0) {
      size_t c = static_cast<size_t>(cand);
      if (i - c > kWindow) break;
      if (p[c + best_len] == p[i + best_len] || best_len == 0) {
        size_t l = 0;
        while (l < maxlen && p[c + l] == p[i + l]) ++l;
        if (l > best_len && l >= 3) {
          best_len = static_cast<uint32_t>(l);
          best_dist = static_cast<uint32_t>(i - c);
          if (l == maxlen) break;
        }
      }
      cand = prev[c];
    }
  };
  size_t i = 0;
  while (i < n) {
    advance_to(i);
    uint32_t len, dist;
    find(i, len, dist);
    if (len >= 3) {
      if (len < 64 && i + 1 < n) {
        advance_to(i + 1);
        uint32_t len2, dist2;
        find(i + 1, len2, dist2);
        if (len2 > len) {
          syms.push_back({p[i], 0});
          ++i;
          continue;
        }
      }
      syms.push_back({static_cast<uint16_t>(len), static_cast<uint16_t>(dist)});
      i += len;
    } else {
      syms.push_back({p[i], 0});
      ++i;
    }
  }
}

uint64_t data_cost(const std::vector<Sym>& s, size_t b, size_t e, const uint8_t* ll, const uint8_t* dl) {
  uint64_t bits = ll[256];
  for (size_t k = b; k < e; ++k) {
    if (s[k].dist == 0) { bits += ll[s[k].lit_or_len]; continue; }
    int lc = len_code(s[k].lit_or_len), dc = dist_code(s[k].dist);
    bits += ll[257 + lc] + kLenExtra[lc] + dl[dc] + kDistExtra[dc];
  }
  return bits;
}

void write_data(BitWriter& w, const std::vector<Sym>& s, size_t b, size_t e, const uint8_t* ll,
                const uint16_t* lc_codes, const uint8_t* dl, const uint16_t* dc_codes) {
  for (size_t k = b; k < e; ++k) {
    if (s[k].dist == 0) { w.put(lc_codes[s[k].lit_or_len], ll[s[k].lit_or_len]); continue; }
    int lc = len_code(s[k].lit_or_len), dc = dist_code(s[k].dist);
    w.put(lc_codes[257 + lc], ll[257 + lc]);
    w.put(s[k].lit_or_len - kLenBase[lc], kLenExtra[lc]);
    w.put(dc_codes[dc], dl[dc]);
    w.put(s[k].dist - kDistBase[dc], kDistExtra[dc]);
  }
  w.put(lc_codes[256], ll[256]);
}

}  // namespace

bool inflate_raw(ByteView in, size_t expected_size, std::string& out) {
  Inflater inf(in, expected_size, expected_size, out);
  if (!inf.run()) { out.clear(); return false; }
  return true;
}

bool inflate_raw_prefix(ByteView in, size_t expected_size, size_t max_out, std::string& out) {
  if (max_out >= expected_size) return inflate_raw(in, expected_size, out);
  Inflater inf(in, expected_size, max_out, out);
  if (!inf.run()) { out.clear(); return false; }
  return true;
}

Bytes deflate_raw(ByteView in) {
  std::vector<Sym> syms;
  syms.reserve(in.size() / 3 + 16);
  lz77(in, syms);

  BitWriter w;
  constexpr size_t kBlockSyms = 32768;
  size_t byte_pos = 0;
  size_t b = 0;
  do {
    size_t e = std::min(syms.size(), b + kBlockSyms);
    size_t block_bytes = 0;
    for (size_t k = b; k < e; ++k) block_bytes += syms[k].dist ? syms[k].lit_or_len : 1;
    bool final = e == syms.size();

    uint32_t lf[286] = {0}, df[30] = {0};
    for (size_t k = b; k < e; ++k) {
      if (syms[k].dist == 0) lf[syms[k].lit_or_len]++;
      else { lf[257 + len_code(syms[k].lit_or_len)]++; df[dist_code(syms[k].dist)]++; }
    }
    lf[256] = 1;
    uint8_t ll[288] = {0}, dl[30] = {0};
    build_lengths(lf, 286, 15, ll);
    build_lengths(df, 30, 15, dl);
    if (std::count_if(dl, dl + 30, [](uint8_t x) { return x != 0; }) == 0) { dl[0] = 1; dl[1] = 1; }
    int hlit = 286;
    while (hlit > 257 && ll[hlit - 1] == 0) --hlit;
    int hdist = 30;
    while (hdist > 1 && dl[hdist - 1] == 0) --hdist;
    uint8_t seq[316];
    std::memcpy(seq, ll, static_cast<size_t>(hlit));
    std::memcpy(seq + hlit, dl, static_cast<size_t>(hdist));
    std::vector<ClSym> cl;
    rle_lengths(seq, static_cast<size_t>(hlit + hdist), cl);
    uint32_t cf[19] = {0};
    for (const ClSym& c : cl) cf[c.sym]++;
    uint8_t cll[19] = {0};
    build_lengths(cf, 19, 7, cll);
    int hclen = 19;
    while (hclen > 4 && cll[kClOrder[hclen - 1]] == 0) --hclen;

    uint64_t dyn = 3 + 14 + 3ull * static_cast<uint64_t>(hclen);
    for (const ClSym& c : cl) dyn += cll[c.sym] + c.extra_bits;
    dyn += data_cost(syms, b, e, ll, dl);
    uint8_t fll[288], fdl[30];
    fixed_lengths(fll, fdl);
    uint64_t fix = 3 + data_cost(syms, b, e, fll, fdl);
    uint64_t sto = (block_bytes / 65535 + 1) * (3 + 7 + 32) + 8ull * block_bytes;

    if (sto < dyn && sto < fix) {
      size_t off = byte_pos, left = block_bytes;
      do {
        size_t chunk = std::min<size_t>(left, 65535);
        left -= chunk;
        w.put(final && left == 0 ? 1 : 0, 1);
        w.put(0, 2);
        w.align();
        w.put(static_cast<uint32_t>(chunk), 16);
        w.put(static_cast<uint32_t>(chunk) ^ 0xFFFF, 16);
        w.raw(in.data() + off, chunk);
        off += chunk;
      } while (left);
    } else if (fix <= dyn) {
      uint16_t lc[288], dc[30];
      canonical_codes(fll, 288, lc);
      canonical_codes(fdl, 30, dc);
      w.put(final ? 1 : 0, 1);
      w.put(1, 2);
      write_data(w, syms, b, e, fll, lc, fdl, dc);
    } else {
      uint16_t lc[288] = {0}, dc[30] = {0}, cc[19] = {0};
      canonical_codes(ll, 286, lc);
      canonical_codes(dl, 30, dc);
      canonical_codes(cll, 19, cc);
      w.put(final ? 1 : 0, 1);
      w.put(2, 2);
      w.put(static_cast<uint32_t>(hlit - 257), 5);
      w.put(static_cast<uint32_t>(hdist - 1), 5);
      w.put(static_cast<uint32_t>(hclen - 4), 4);
      for (int k = 0; k < hclen; ++k) w.put(cll[kClOrder[k]], 3);
      for (const ClSym& c : cl) {
        w.put(cc[c.sym], cll[c.sym]);
        if (c.extra_bits) w.put(c.extra, c.extra_bits);
      }
      write_data(w, syms, b, e, ll, lc, dl, dc);
    }
    byte_pos += block_bytes;
    b = e;
  } while (b < syms.size());
  return w.take();
}

}  // namespace ember::guides
