#include "guides/embed.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "guides/deflate.h"
#include "guides/embed_format.h"
#include "guides/text.h"

namespace ember::guides {

const char* model_status_name(ModelStatus s) {
  switch (s) {
    case ModelStatus::kOk: return "ok";
    case ModelStatus::kTooSmall: return "too_small";
    case ModelStatus::kBadMagic: return "bad_magic";
    case ModelStatus::kBadVersion: return "bad_version";
    case ModelStatus::kBadChecksum: return "bad_checksum";
    case ModelStatus::kBadLayout: return "bad_layout";
    case ModelStatus::kUnsupported: return "unsupported";
  }
  return "unknown";
}

namespace {
uint32_t rd32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 | static_cast<uint32_t>(p[2]) << 16 |
         static_cast<uint32_t>(p[3]) << 24;
}
uint32_t rd16(const uint8_t* p) { return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8; }
uint64_t pad4(uint64_t v) { return (v + 3) & ~static_cast<uint64_t>(3); }
int32_t sext24(uint32_t v) {
  v &= 0xFFFFFFu;
  return (v & 0x800000u) ? static_cast<int32_t>(v) - 0x1000000 : static_cast<int32_t>(v);
}
bool valid_cp(int64_t cp) { return cp >= 0 && cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF); }
}  // namespace

uint32_t EmbedModel::rd(size_t off) const { return rd32(data_.data() + off); }

void EmbedModel::clear() { *this = EmbedModel(); }

ModelStatus EmbedModel::load(ByteView d) {
  clear();
  if (d.size() < kModelHeaderSize) return ModelStatus::kTooSmall;
  const uint8_t* p = d.data();
  if (std::memcmp(p, kModelMagic, sizeof kModelMagic) != 0) return ModelStatus::kBadMagic;
  if (rd32(p + kMhVersion) != kModelVersion) return ModelStatus::kBadVersion;
  if (crc32_fast(ByteView(p, kMhHeaderCrc)) != rd32(p + kMhHeaderCrc)) return ModelStatus::kBadChecksum;
  if (rd32(p + kMhHeaderSize) != kModelHeaderSize) return ModelStatus::kBadLayout;
  const uint32_t V = rd32(p + kMhVocab), D = rd32(p + kMhDims), bits = rd32(p + kMhBits);
  const uint32_t unk = rd32(p + kMhUnk), mwc = rd32(p + kMhMaxWordChars), flags = rd32(p + kMhFlags);
  const uint32_t R = rd32(p + kMhNormRuns), sbytes = rd32(p + kMhNormStrBytes), C = rd32(p + kMhClassRuns);
  const uint32_t vbytes = rd32(p + kMhVocabBytes), nlen = rd32(p + kMhNameLen), S = rd32(p + kMhSpecials);
  const uint32_t total = rd32(p + kMhTotal), toff = rd32(p + kMhTableOffset);
  if (bits != 4 && bits != 8 && bits != 32) return ModelStatus::kUnsupported;
  if (flags != 1) return ModelStatus::kUnsupported;
  if (V == 0 || V > kModelMaxVocab || D == 0 || D % 8 != 0 || D > kModelMaxDims || unk >= V || mwc == 0 ||
      mwc > kModelMaxWordChars || R == 0 || R > kModelMaxNormRuns || C == 0 || C > kModelMaxClassRuns ||
      S > kModelMaxSpecials || nlen > 256 || sbytes > (1u << 20) || vbytes > (16u << 20))
    return ModelStatus::kBadLayout;
  if (total != d.size()) return ModelStatus::kBadLayout;
  // Block layout (all sizes bounded above, so no u64 overflow).
  uint64_t o = kModelHeaderSize;
  const uint64_t name_off = o;  o = pad4(o + nlen);
  const uint64_t spec_off = o;  o = pad4(o + 4ull * S);
  const uint64_t runs_off = o;  o += 8ull * R;
  const uint64_t strs_off = o;  o = pad4(o + sbytes);
  const uint64_t cls_off = o;   o += 8ull * C;
  const uint64_t voff = o;      o += 4ull * (V + 1);
  const uint64_t blob = o;      o = pad4(o + vbytes);
  const uint64_t sorted = o;    o = pad4(o + 2ull * V);
  const uint64_t scales = o;    if (bits != 32) o = pad4(o + 2ull * V);
  const uint64_t row = static_cast<uint64_t>(D) * bits / 8;
  if (o != toff || o + row * V != total) return ModelStatus::kBadLayout;
  if (crc32_fast(d.sub(kModelHeaderSize, toff - kModelHeaderSize)) != rd32(p + kMhBodyCrc))
    return ModelStatus::kBadChecksum;

  data_ = d;
  auto fail = [&]() { clear(); return ModelStatus::kBadLayout; };
  name_ = std::string_view(reinterpret_cast<const char*>(p + name_off), nlen);
  if (!utf8_valid(name_)) return fail();
  // Normalisation runs: start at 0, strictly ascending, known kinds, string refs in range.
  std::string_view strs(reinterpret_cast<const char*>(p + strs_off), sbytes);
  if (!utf8_valid(strs)) return fail();
  for (uint32_t i = 0; i < R; ++i) {
    uint32_t start = rd32(p + runs_off + 8ull * i), v = rd32(p + runs_off + 8ull * i + 4);
    if ((i == 0 && start != 0) || start > 0x10FFFF) return fail();
    if (i > 0 && start <= rd32(p + runs_off + 8ull * (i - 1))) return fail();
    uint32_t kind = v >> 24, pay = v & 0xFFFFFFu;
    if (kind > kNormHangul) return fail();
    if (kind == kNormStr) {
      uint32_t so = pay >> 8, sl = pay & 255;
      if (sl == 0 || so > sbytes || sl > sbytes - so) return fail();
      std::string_view s = strs.substr(so, sl);
      if (utf8_floor(s, 0) != 0 || (so + sl < sbytes && utf8_floor(strs, so + sl) != so + sl)) return fail();
      if (utf8_floor(strs, so) != so) return fail();
    }
  }
  for (uint32_t i = 0; i < C; ++i) {
    uint32_t start = rd32(p + cls_off + 8ull * i), c = rd32(p + cls_off + 8ull * i + 4);
    if ((i == 0 && start != 0) || start > 0x10FFFF || c > kClsSpace) return fail();
    if (i > 0 && start <= rd32(p + cls_off + 8ull * (i - 1))) return fail();
  }
  // Vocabulary: offsets ascending, every token 1..kModelMaxTokenBytes bytes, UTF-8.
  if (rd32(p + voff) != 0 || rd32(p + voff + 4ull * V) != vbytes) return fail();
  for (uint32_t i = 0; i < V; ++i) {
    uint32_t a = rd32(p + voff + 4ull * i), b = rd32(p + voff + 4ull * (i + 1));
    if (b <= a || b - a > kModelMaxTokenBytes || b > vbytes) return fail();
  }
  if (!utf8_valid(std::string_view(reinterpret_cast<const char*>(p + blob), vbytes))) return fail();
  vocab_ = V;
  voff_off_ = voff;
  blob_off_ = blob;
  blob_len_ = vbytes;
  // Sorted index: a permutation in strictly increasing bytewise order (binary search relies on it).
  {
    std::vector<bool> seen(V, false);
    std::string_view prev;
    for (uint32_t i = 0; i < V; ++i) {
      uint32_t id = rd16(p + sorted + 2ull * i);
      if (id >= V || seen[id]) return fail();
      seen[id] = true;
      std::string_view t = token(id);
      if (i > 0 && !(prev < t)) return fail();
      prev = t;
    }
  }
  for (uint32_t i = 0; i < S; ++i) {
    uint32_t id = rd32(p + spec_off + 4ull * i);
    if (id >= V || token(id).size() > 64) return fail();
  }
  dims_ = D;
  bits_ = bits;
  unk_ = unk;
  max_word_chars_ = mwc;
  n_runs_ = R;
  n_classes_ = C;
  n_specials_ = S;
  runs_off_ = runs_off;
  strs_off_ = strs_off;
  strs_len_ = sbytes;
  classes_off_ = cls_off;
  specials_off_ = spec_off;
  sorted_off_ = sorted;
  scales_off_ = scales;
  table_off_ = toff;
  row_bytes_ = static_cast<size_t>(row);
  table_crc_ = rd32(p + kMhTableCrc);
  // ASCII fast path (0xFF = not a single ASCII byte: use the general path).
  for (uint32_t c = 0; c < 128; ++c) {
    uint32_t rs;
    Norm n = norm_of(c, rs);
    uint32_t out = 0xFF;
    if (n.kind == kNormSame) out = c;
    else if (n.kind == kNormDrop) out = 0;
    else if (n.kind == kNormSpace) out = ' ';
    else if (n.kind == kNormDelta || (n.kind == kNormAlt && ((c - rs) & 1) == 0)) {
      int64_t m = static_cast<int64_t>(c) + sext24(n.payload);
      if (m > 0 && m < 128) out = static_cast<uint32_t>(m);
    } else if (n.kind == kNormAlt) {
      out = c;
    }
    ascii_map_[c] = static_cast<uint8_t>(out);
    ascii_cls_[c] = static_cast<uint8_t>(out < 128 ? class_of(out) : kClsWord);
  }
  ok_ = true;
  return ModelStatus::kOk;
}

std::string_view EmbedModel::token(uint32_t id) const {
  if (id >= vocab_) return std::string_view();
  uint32_t a = rd(voff_off_ + 4ull * id), b = rd(voff_off_ + 4ull * (id + 1));
  return std::string_view(reinterpret_cast<const char*>(data_.data() + blob_off_ + a), b - a);
}

bool EmbedModel::verify_table() const {
  if (!ok_) return false;
  return crc32_fast(data_.sub(table_off_, static_cast<size_t>(vocab_) * row_bytes_)) == table_crc_;
}

EmbedModel::Norm EmbedModel::norm_of(uint32_t cp, uint32_t& run_start) const {
  // Last run whose start <= cp (run 0 starts at 0).
  uint32_t lo = 0, hi = n_runs_;
  while (hi - lo > 1) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (rd(runs_off_ + 8ull * mid) <= cp) lo = mid; else hi = mid;
  }
  run_start = rd(runs_off_ + 8ull * lo);
  uint32_t v = rd(runs_off_ + 8ull * lo + 4);
  return Norm{v >> 24, v & 0xFFFFFFu};
}

uint32_t EmbedModel::class_of(uint32_t cp) const {
  if (cp < 128 && ok_) return ascii_cls_[cp] <= kClsSpace && ascii_map_[cp] == cp ? ascii_cls_[cp] : kClsWord;
  uint32_t lo = 0, hi = n_classes_;
  while (hi - lo > 1) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (rd(classes_off_ + 8ull * mid) <= cp) lo = mid; else hi = mid;
  }
  return rd(classes_off_ + 8ull * lo + 4);
}

bool EmbedModel::lookup(const char* s, size_t n, uint32_t& id) const {
  std::string_view key(s, n);
  uint32_t lo = 0, hi = vocab_;
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    uint32_t t = rd16(data_.data() + sorted_off_ + 2ull * mid);
    int c = token(t).compare(key);
    if (c == 0) { id = t; return true; }
    if (c < 0) lo = mid + 1; else hi = mid;
  }
  return false;
}

void EmbedModel::word_pieces(const std::string& w, size_t chars, std::vector<uint32_t>& ids, size_t max_ids) const {
  if (w.empty()) return;
  if (chars > max_word_chars_) return;  // [UNK], dropped
  const size_t mark = ids.size();
  char buf[2 + kModelMaxWordChars * 4];
  size_t start = 0, n = w.size();
  if (n > sizeof buf - 2) return;
  while (start < n) {
    size_t end = n;
    bool found = false;
    uint32_t id = 0;
    while (start < end) {
      size_t len = 0;
      if (start > 0) { buf[0] = '#'; buf[1] = '#'; len = 2; }
      std::memcpy(buf + len, w.data() + start, end - start);
      len += end - start;
      if (lookup(buf, len, id)) { found = true; break; }
      --end;  // previous code point boundary
      while (end > start && (static_cast<unsigned char>(w[end]) & 0xC0) == 0x80) --end;
    }
    if (!found) { ids.resize(mark); return; }  // the whole word is [UNK]
    if (id != unk_) {
      if (ids.size() >= max_ids) return;
      ids.push_back(id);
    }
    start = end;
  }
}

void EmbedModel::segment(std::string_view text, std::vector<uint32_t>& ids, size_t max_ids, std::string& word) const {
  size_t chars = 0;
  word.clear();
  auto flush = [&]() {
    word_pieces(word, chars, ids, max_ids);
    word.clear();
    chars = 0;
  };
  auto out_cp = [&](uint32_t cp) {
    uint32_t c = class_of(cp);
    if (c == kClsSpace) { flush(); return; }
    if (c == kClsPunct) {
      flush();
      utf8_append(word, cp);
      chars = 1;
      flush();
      return;
    }
    utf8_append(word, cp);
    ++chars;
  };
  size_t i = 0;
  while (i < text.size() && ids.size() < max_ids) {
    unsigned char b = static_cast<unsigned char>(text[i]);
    if (b < 0x80) {
      uint8_t m = ascii_map_[b];
      if (m != 0xFF) {
        ++i;
        if (m == 0) continue;
        uint32_t c = ascii_cls_[b];
        if (c == kClsSpace) { flush(); continue; }
        if (c == kClsPunct) { flush(); word.push_back(static_cast<char>(m)); chars = 1; flush(); continue; }
        word.push_back(static_cast<char>(m));
        ++chars;
        if (word.size() > 4 * kModelMaxWordChars + 8) { word.clear(); chars = max_word_chars_ + 1; }
        continue;
      }
    }
    size_t start = i;
    uint32_t cp = utf8_next(text, i);
    if (cp == 0xFFFD && i - start != 3) continue;  // invalid byte: dropped
    uint32_t rs;
    Norm n = norm_of(cp, rs);
    switch (n.kind) {
      case kNormSame: out_cp(cp); break;
      case kNormDrop: break;
      case kNormSpace: flush(); break;
      case kNormPad: flush(); out_cp(cp); flush(); break;
      case kNormDelta:
      case kNormAlt: {
        int64_t m = cp;
        if (n.kind == kNormDelta || ((cp - rs) & 1) == 0) m += sext24(n.payload);
        if (valid_cp(m)) out_cp(static_cast<uint32_t>(m));
        break;
      }
      case kNormStr: {
        size_t so = n.payload >> 8, sl = n.payload & 255;
        if (so > strs_len_ || sl > strs_len_ - so) break;
        std::string_view s(reinterpret_cast<const char*>(data_.data() + strs_off_ + so), sl);
        size_t k = 0;
        while (k < s.size()) {
          uint32_t c = utf8_next(s, k);
          if (c == ' ') flush(); else if (c != 0xFFFD) out_cp(c);
        }
        break;
      }
      case kNormHangul: {
        if (cp < 0xAC00 || cp > 0xD7A3) { out_cp(cp); break; }
        uint32_t s = cp - 0xAC00;
        out_cp(0x1100 + s / 588);
        out_cp(0x1161 + (s % 588) / 28);
        if (s % 28) out_cp(0x11A7 + s % 28);
        break;
      }
      default: break;
    }
    if (word.size() > 4 * kModelMaxWordChars + 8) { word.clear(); chars = max_word_chars_ + 1; }
  }
  flush();
}

size_t EmbedModel::tokenize(std::string_view text, std::vector<uint32_t>& ids, size_t max_ids) const {
  const size_t before = ids.size();
  if (!ok_ || max_ids == 0) return 0;
  const size_t cap = before + max_ids;
  std::string word;
  // Special tokens ([CLS], [SEP], ...) are matched literally in the raw text first, like the
  // reference tokenizer's added-token pass; the text between them is tokenized normally.
  size_t seg = 0, i = 0;
  while (i < text.size() && ids.size() < cap) {
    if (text[i] == '[' && n_specials_) {
      uint32_t best = 0;
      size_t best_len = 0;
      for (uint32_t k = 0; k < n_specials_; ++k) {
        uint32_t id = rd(specials_off_ + 4ull * k);
        std::string_view t = token(id);
        if (t.size() > best_len && text.substr(i, t.size()) == t) { best = id; best_len = t.size(); }
      }
      if (best_len) {
        segment(text.substr(seg, i - seg), ids, cap, word);
        if (best != unk_ && ids.size() < cap) ids.push_back(best);
        i += best_len;
        seg = i;
        continue;
      }
    }
    ++i;
  }
  if (seg < text.size() && ids.size() < cap) segment(text.substr(seg), ids, cap, word);
  return ids.size() - before;
}

void EmbedModel::add_row(uint32_t id, int64_t* acc) const {
  if (!ok_ || id >= vocab_) return;
  const uint8_t* r = data_.data() + table_off_ + static_cast<size_t>(id) * row_bytes_;
  const uint32_t D = dims_;
  if (bits_ == 4) {
    const int64_t s = rd16(data_.data() + scales_off_ + 2ull * id);
    for (uint32_t j = 0; j < D / 2; ++j) {
      uint32_t b = r[j];
      acc[2 * j] += (static_cast<int64_t>(b & 15) - 8) * s;
      acc[2 * j + 1] += (static_cast<int64_t>(b >> 4) - 8) * s;
    }
  } else if (bits_ == 8) {
    const int64_t s = rd16(data_.data() + scales_off_ + 2ull * id);
    for (uint32_t j = 0; j < D; ++j) acc[j] += static_cast<int64_t>(static_cast<int8_t>(r[j])) * s;
  } else {
    for (uint32_t j = 0; j < D; ++j) {
      uint32_t u = rd32(r + 4ull * j);
      float f;
      std::memcpy(&f, &u, 4);
      double x = static_cast<double>(f);
      if (!(std::fabs(x) < 1000.0)) x = 0;  // also rejects NaN
      acc[j] += static_cast<int64_t>(std::llround(x * 16777216.0));
    }
  }
}

size_t EmbedModel::embed(std::string_view text, int64_t* acc) const {
  if (!ok_) return 0;
  if (text.size() > kMaxEmbedBytes) text = text.substr(0, utf8_floor(text, kMaxEmbedBytes));
  std::vector<uint32_t> ids;
  tokenize(text, ids, kMaxEmbedTokens);
  for (uint32_t id : ids) add_row(id, acc);
  return ids.size();
}

// ---------------------------------------------------------------------------------------------

uint32_t quantize_levels(const int64_t* acc, uint32_t dims, int levels, int8_t* out) {
  int64_t m = 0;
  for (uint32_t i = 0; i < dims; ++i) {
    int64_t a = acc[i] < 0 ? -acc[i] : acc[i];
    if (a > m) m = a;
  }
  uint32_t ss = 0;
  for (uint32_t i = 0; i < dims; ++i) {
    int64_t q = 0;
    if (m > 0) {
      int64_t a = acc[i];
      q = (2 * a * levels + (a >= 0 ? m : -m)) / (2 * m);
      if (q > levels) q = levels;
      if (q < -levels) q = -levels;
    }
    out[i] = static_cast<int8_t>(q);
    ss += static_cast<uint32_t>(q * q);
  }
  return ss;
}

void encode_vector(const int64_t* acc, uint32_t dims, uint32_t bits, Bytes& vec, Bytes& norms) {
  auto put = [](Bytes& b, uint32_t v, int n) { for (int i = 0; i < n; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i))); };
  if (bits == 32) {
    int64_t m = 0;
    for (uint32_t i = 0; i < dims; ++i) m = std::max<int64_t>(m, acc[i] < 0 ? -acc[i] : acc[i]);
    double ss = 0;
    for (uint32_t i = 0; i < dims; ++i) {
      float f = m ? static_cast<float>(static_cast<double>(acc[i]) / static_cast<double>(m)) : 0.0f;
      uint32_t u;
      std::memcpy(&u, &f, 4);
      put(vec, u, 4);
      ss += static_cast<double>(f) * f;
    }
    float n = static_cast<float>(std::sqrt(ss));
    uint32_t u;
    std::memcpy(&u, &n, 4);
    put(norms, u, 4);
    return;
  }
  std::vector<int8_t> q(dims);
  uint32_t ss = quantize_levels(acc, dims, bits == 4 ? 7 : 127, q.data());
  if (bits == 4) {
    for (uint32_t i = 0; i + 1 < dims; i += 2)
      vec.push_back(static_cast<uint8_t>((q[i] + 8) | ((q[i + 1] + 8) << 4)));
    put(norms, ss, 2);
  } else {
    for (uint32_t i = 0; i < dims; ++i) vec.push_back(static_cast<uint8_t>(q[i]));
    put(norms, ss, 4);
  }
}

}  // namespace ember::guides
