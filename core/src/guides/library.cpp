#include "guides/library.h"

#include <algorithm>
#include <cstring>

#include "guides/deflate.h"
#include "guides/pack_format.h"
#include "guides/text.h"

namespace ember::guides {

const char* load_status_name(LoadStatus s) {
  switch (s) {
    case LoadStatus::kOk: return "ok";
    case LoadStatus::kTooSmall: return "too_small";
    case LoadStatus::kBadMagic: return "bad_magic";
    case LoadStatus::kBadVersion: return "bad_version";
    case LoadStatus::kBadChecksum: return "bad_checksum";
    case LoadStatus::kBadLayout: return "bad_layout";
    case LoadStatus::kTooLarge: return "too_large";
  }
  return "unknown";
}

namespace {
uint32_t rd32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
         static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}
uint64_t rd64(const uint8_t* p) { return rd32(p) | static_cast<uint64_t>(rd32(p + 4)) << 32; }

bool is_cont(uint8_t b) { return (b & 0xC0) == 0x80; }
}  // namespace

void GuideLibrary::clear() {
  own_.clear();
  own_.shrink_to_fit();
  pack_ = ByteView();
  valid_ = false;
  n_docs_ = n_cats_ = n_terms_ = n_blocks_ = 0;
  for (Ref& r : sec_) r = Ref();
  docs_off_ = 0;
  for (double& a : avg_len_) a = 1;
  sem_.clear();
  sem_status_.clear();
}

uint32_t GuideLibrary::u32(size_t off) const {
  if (off > pack_.size() || pack_.size() - off < 4) return 0;
  return rd32(pack_.data() + off);
}

std::string_view GuideLibrary::str_at(size_t rec_off) const {
  uint32_t off = u32(rec_off), len = u32(rec_off + 4);
  const Ref& s = sec_[kSecStrings];
  if (off > s.len || len > s.len - off) return std::string_view();
  return std::string_view(reinterpret_cast<const char*>(pack_.data()) + s.off + off, len);
}

LoadStatus GuideLibrary::load(ByteView in, bool copy) {
  clear();
  if (in.size() < kHeaderSize) return LoadStatus::kTooSmall;
  if (in.size() > kMaxPackBytes) return LoadStatus::kTooLarge;
  if (std::memcmp(in.data(), kPackMagic, sizeof kPackMagic) != 0) return LoadStatus::kBadMagic;
  if (rd32(in.data() + kHdrVersion) != kPackVersion) return LoadStatus::kBadVersion;
  if (crc32_fast(ByteView(in.data(), kHdrCrc)) != rd32(in.data() + kHdrCrc)) return LoadStatus::kBadChecksum;

  if (copy) {
    own_ = in.to_bytes();
    pack_ = ByteView(own_);
  } else {
    pack_ = in;
  }
  const uint8_t* p = pack_.data();
  const size_t size = pack_.size();
  auto fail = [&](LoadStatus s) { clear(); return s; };

  if (rd32(p + kHdrHeaderSize) != kHeaderSize || rd32(p + kHdrSectionCount) != kSectionCount ||
      rd32(p + kHdrTermBlockSize) != kTermBlockSize)
    return fail(LoadStatus::kBadLayout);
  uint32_t crcs[kSectionCount];
  for (uint32_t s = 0; s < kSectionCount; ++s) {
    const uint8_t* e = p + kHdrSections + 12 * s;
    uint32_t off = rd32(e), len = rd32(e + 4);
    if (off < kHeaderSize || off > size || len > size - off) return fail(LoadStatus::kBadLayout);
    sec_[s].off = off;
    sec_[s].len = len;
    crcs[s] = rd32(e + 8);
  }
  for (uint32_t s = 0; s < kSectionCount; ++s) {
    if (s == kSecBodies) continue;  // checked per body on open, and by verify()
    if (crc32_fast(pack_.sub(sec_[s].off, sec_[s].len)) != crcs[s]) return fail(LoadStatus::kBadChecksum);
  }

  const uint32_t n = rd32(p + kHdrDocCount), c = rd32(p + kHdrCategoryCount), t = rd32(p + kHdrTermCount);
  if (n > kMaxDocs) return fail(LoadStatus::kBadLayout);
  const uint64_t blocks = (static_cast<uint64_t>(t) + kTermBlockSize - 1) / kTermBlockSize;
  if (sec_[kSecDocs].len != static_cast<uint64_t>(n) * kDocRecordSize ||
      sec_[kSecCategories].len != static_cast<uint64_t>(c) * kCategoryRecordSize ||
      sec_[kSecIdIndex].len != static_cast<uint64_t>(n) * 4 || sec_[kSecTermBlocks].len != blocks * 8 ||
      (n > 0 && c == 0))
    return fail(LoadStatus::kBadLayout);

  // String pool must be valid UTF-8 and every reference must start/end on a code point boundary.
  const Ref& sp = sec_[kSecStrings];
  std::string_view pool(reinterpret_cast<const char*>(p) + sp.off, sp.len);
  if (!utf8_valid(pool)) return fail(LoadStatus::kBadLayout);
  auto ref_ok = [&](size_t rec) {
    uint32_t off = rd32(p + rec), len = rd32(p + rec + 4);
    if (off > sp.len || len > sp.len - off || len > kMaxMetaBytes) return false;
    if (len == 0) return true;
    if (is_cont(static_cast<uint8_t>(pool[off]))) return false;
    if (off + len < sp.len && is_cont(static_cast<uint8_t>(pool[off + len]))) return false;
    return true;
  };

  n_docs_ = n;
  n_cats_ = c;
  n_terms_ = t;
  n_blocks_ = static_cast<uint32_t>(blocks);
  docs_off_ = sec_[kSecDocs].off;

  // Categories: contiguous doc ranges covering [0, n).
  uint32_t expect_first = 0;
  for (uint32_t i = 0; i < c; ++i) {
    size_t r = sec_[kSecCategories].off + static_cast<size_t>(i) * kCategoryRecordSize;
    if (!ref_ok(r) || !ref_ok(r + 8)) return fail(LoadStatus::kBadLayout);
    uint32_t first = rd32(p + r + 16), cnt = rd32(p + r + 20);
    if (first != expect_first || cnt > n - first) return fail(LoadStatus::kBadLayout);
    expect_first = first + cnt;
  }
  if (expect_first != n) return fail(LoadStatus::kBadLayout);

  for (uint32_t i = 0; i < c; ++i) {
    size_t r = sec_[kSecCategories].off + static_cast<size_t>(i) * kCategoryRecordSize;
    uint32_t first = rd32(p + r + 16), cnt = rd32(p + r + 20);
    for (uint32_t d = first; d < first + cnt; ++d)
      if (rd32(p + doc_rec(d) + kDocCategory) != i) return fail(LoadStatus::kBadLayout);
  }

  // Bodies (security review H6): every doc owns a disjoint byte range of BODIES (the builder never
  // shares bodies), and the claimed size must be plausible for its compressed length both ways:
  // at most kMaxBodyExpansion:1 (real text: <= 6:1; DEFLATE allows ~1032:1) and at most 1/8 larger
  // than the text (stored blocks cost 5 bytes per 64 KiB; fixed Huffman at most 9/8). So inflating
  // any one body costs O(its share of the pack), and all of them O(pack size).
  const Ref& bodies = sec_[kSecBodies];
  std::vector<std::pair<uint32_t, uint32_t>> ranges;
  ranges.reserve(n);
  for (uint32_t d = 0; d < n; ++d) {
    size_t r = doc_rec(d);
    for (size_t f = 0; f <= kDocPartOf; f += 8)
      if (!ref_ok(r + f)) return fail(LoadStatus::kBadLayout);
    if (rd32(p + r + kDocId + 4) == 0) return fail(LoadStatus::kBadLayout);
    uint32_t bytes = rd32(p + r + kDocBytes), boff = rd32(p + r + kDocBodyOff), blen = rd32(p + r + kDocBodyLen);
    if (bytes > kMaxGuideBytes || boff > bodies.len || blen > bodies.len - boff) return fail(LoadStatus::kBadLayout);
    if (static_cast<uint64_t>(bytes) > static_cast<uint64_t>(blen) * kMaxBodyExpansion + kBodyExpansionSlack ||
        static_cast<uint64_t>(blen) > static_cast<uint64_t>(bytes) + bytes / 8 + 1024)
      return fail(LoadStatus::kBadLayout);
    if (blen) ranges.emplace_back(boff, blen);
    uint32_t prev = rd32(p + r + kDocPrev), next = rd32(p + r + kDocNext);
    if ((prev != kNone && prev >= n) || (next != kNone && next >= n) || prev == d || next == d)
      return fail(LoadStatus::kBadLayout);
    for (int f = 0; f < kFieldCount; ++f)
      if (rd32(p + r + kDocFieldLens + 4 * static_cast<size_t>(f)) > (1u << 24)) return fail(LoadStatus::kBadLayout);
  }
  std::sort(ranges.begin(), ranges.end());
  for (size_t i = 1; i < ranges.size(); ++i)
    if (static_cast<uint64_t>(ranges[i - 1].first) + ranges[i - 1].second > ranges[i].first)
      return fail(LoadStatus::kBadLayout);
  // IDINDEX must be a permutation of the docs sorted by strictly increasing id, or get(id) could
  // miss guides that list/search return (found by the search mutation fuzzer).
  {
    std::vector<bool> seen(n, false);
    std::string_view prev_id;
    for (uint32_t i = 0; i < n; ++i) {
      uint32_t d = rd32(p + sec_[kSecIdIndex].off + 4 * static_cast<size_t>(i));
      if (d >= n || seen[d]) return fail(LoadStatus::kBadLayout);
      seen[d] = true;
      size_t rec = docs_off_ + static_cast<size_t>(d) * kDocRecordSize;
      std::string_view id(reinterpret_cast<const char*>(p) + sp.off + rd32(p + rec + kDocId), rd32(p + rec + kDocId + 4));
      if (i > 0 && !(prev_id < id)) return fail(LoadStatus::kBadLayout);
      prev_id = id;
    }
  }
  for (uint32_t b = 0; b < n_blocks_; ++b) {
    size_t e = sec_[kSecTermBlocks].off + 8 * static_cast<size_t>(b);
    if (rd32(p + e) >= sec_[kSecTerms].len || rd32(p + e + 4) > sec_[kSecPostings].len)
      return fail(LoadStatus::kBadLayout);
  }

  for (int f = 0; f < kFieldCount; ++f) {
    uint64_t total = rd64(p + kHdrFieldTotals + 8 * static_cast<size_t>(f));
    double avg = n ? static_cast<double>(total) / n : 1.0;
    avg_len_[f] = avg < 1.0 ? 1.0 : avg;
  }
  valid_ = true;
  load_semantic();
  return LoadStatus::kOk;
}

// Optional semantic sections: never fail the load (a damaged or missing section means keyword-only).
void GuideLibrary::load_semantic() {
  sem_.clear();
  uint32_t ext = u32(kHdrExtDir);
  if (ext == 0) { sem_status_ = "no semantic section in this pack"; return; }
  std::vector<uint32_t> doc_bytes(n_docs_);
  for (uint32_t d = 0; d < n_docs_; ++d) doc_bytes[d] = u32(doc_rec(d) + kDocBytes);
  std::string why;
  if (sem_.load(pack_, ext, static_cast<uint32_t>(kHeaderSize), doc_bytes, &why)) sem_status_ = "ok";
  else sem_status_ = why.empty() ? "semantic section invalid" : why;
}

PassageText GuideLibrary::passage_text(uint32_t doc, uint32_t p, const std::string& md) const {
  PassageText out;
  PassageRec r = sem_.passage(p);
  std::string plain;
  if (r.meta || md.empty()) {
    plain = std::string(str_at(doc_rec(doc) + kDocSummary));
  } else {
    if (r.off > md.size() || r.len > md.size() - r.off) return out;
    plain = markdown_to_plain(std::string_view(md).substr(r.off, r.len));
    if (r.head_len && r.head_off <= md.size() && r.head_len <= md.size() - r.head_off)
      out.heading = markdown_to_plain(std::string_view(md).substr(r.head_off, r.head_len));
    while (!out.heading.empty() && (out.heading.back() == '\n' || out.heading.back() == ' ')) out.heading.pop_back();
  }
  // Collapse whitespace; cut at a word boundary.
  std::string t;
  t.reserve(std::min<size_t>(plain.size(), kPassageChars + 8));
  bool sp = false;
  for (char ch : plain) {
    if (ch == ' ' || ch == '\n' || ch == '\t' || ch == '\r') { sp = !t.empty(); continue; }
    if (sp) { t.push_back(' '); sp = false; }
    t.push_back(ch);
    if (t.size() > kPassageChars + 4) break;
  }
  if (t.size() > kPassageChars) {
    const size_t room = kPassageChars - 3;  // "…" is 3 bytes
    size_t cut = utf8_floor(t, room);
    size_t k = cut;
    while (k > room / 2 && t[k] != ' ') --k;
    if (k > room / 2) cut = k;
    t.resize(cut);
    while (!t.empty() && t.back() == ' ') t.pop_back();
    t += "\xE2\x80\xA6";
  }
  if (!utf8_valid(t)) t.clear();
  out.text = std::move(t);
  if (!utf8_valid(out.heading)) out.heading.clear();
  return out;
}

// ---------------------------------------------------------------------------------------------
// Metadata

std::string_view GuideLibrary::doc_id(uint32_t doc) const { return str_at(doc_rec(doc) + kDocId); }
std::string_view GuideLibrary::doc_title(uint32_t doc) const { return str_at(doc_rec(doc) + kDocTitle); }

void GuideLibrary::fill_meta(uint32_t doc, GuideMeta& m) const {
  size_t r = doc_rec(doc);
  m.id = std::string(str_at(r + kDocId));
  m.title = std::string(str_at(r + kDocTitle));
  uint32_t cat = u32(r + kDocCategory);
  m.category = cat < n_cats_ ? std::string(str_at(sec_[kSecCategories].off + static_cast<size_t>(cat) * kCategoryRecordSize))
                             : std::string();
  m.summary = std::string(str_at(r + kDocSummary));
  m.tags.clear();
  std::string_view tags = str_at(r + kDocTags);
  while (!tags.empty()) {
    size_t nl = tags.find('\n');
    std::string_view one = tags.substr(0, nl);
    if (!one.empty()) m.tags.emplace_back(one);
    if (nl == std::string_view::npos) break;
    tags.remove_prefix(nl + 1);
  }
  m.source = std::string(str_at(r + kDocSource));
  m.source_title = std::string(str_at(r + kDocSourceTitle));
  m.license = std::string(str_at(r + kDocLicense));
  m.attribution = std::string(str_at(r + kDocAttribution));
  m.part_of = std::string(str_at(r + kDocPartOf));
  m.has_order = (u32(r + kDocFlags) & kDocFlagHasOrder) != 0;
  m.order = static_cast<int32_t>(u32(r + kDocOrder));
  m.bytes = u32(r + kDocBytes);
}

int GuideLibrary::find_category(std::string_view id) const {
  for (uint32_t i = 0; i < n_cats_; ++i)
    if (str_at(sec_[kSecCategories].off + static_cast<size_t>(i) * kCategoryRecordSize) == id) return static_cast<int>(i);
  return -1;
}

std::vector<CategoryInfo> GuideLibrary::categories() const {
  std::vector<CategoryInfo> out;
  if (!valid_) return out;
  out.reserve(n_cats_);
  for (uint32_t i = 0; i < n_cats_; ++i) {
    size_t r = sec_[kSecCategories].off + static_cast<size_t>(i) * kCategoryRecordSize;
    CategoryInfo ci;
    ci.id = std::string(str_at(r));
    ci.title = std::string(str_at(r + 8));
    ci.count = u32(r + 20);
    out.push_back(std::move(ci));
  }
  return out;
}

GuideList GuideLibrary::list(std::string_view category, std::string_view part_of, uint32_t offset,
                             uint32_t limit) const {
  GuideList out;
  if (!valid_) return out;
  limit = std::min<uint32_t>(limit, 10000);
  uint32_t lo = 0, hi = n_docs_;
  if (!category.empty()) {
    int c = find_category(category);
    if (c < 0) return out;
    size_t r = sec_[kSecCategories].off + static_cast<size_t>(c) * kCategoryRecordSize;
    lo = u32(r + 16);
    hi = lo + u32(r + 20);
  }
  // Output budget: real metadata is ~1 KiB per guide, but a hostile pack may point every string of
  // 10000 records at kMaxMetaBytes strings. Stop materialising (still counting) past 16 MiB.
  size_t budget = 16u << 20;
  for (uint32_t d = lo; d < hi; ++d) {
    if (!part_of.empty() && str_at(doc_rec(d) + kDocPartOf) != part_of) continue;
    if (out.total >= offset && out.guides.size() < limit && budget > 0) {
      GuideMeta m;
      fill_meta(d, m);
      size_t sz = m.id.size() + m.title.size() + m.category.size() + m.summary.size() + m.source.size() +
                  m.source_title.size() + m.license.size() + m.attribution.size() + m.part_of.size() + 64;
      for (const std::string& t : m.tags) sz += t.size() + 8;
      budget = sz >= budget ? 0 : budget - sz;
      out.guides.push_back(std::move(m));
    }
    ++out.total;
  }
  return out;
}

bool GuideLibrary::find_doc(std::string_view id, uint32_t& doc) const {
  if (!valid_ || id.empty()) return false;
  size_t lo = 0, hi = n_docs_;
  const size_t base = sec_[kSecIdIndex].off;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    uint32_t d = u32(base + 4 * mid);
    if (d >= n_docs_) return false;
    int c = doc_id(d).compare(id);
    if (c == 0) { doc = d; return true; }
    if (c < 0) lo = mid + 1; else hi = mid;
  }
  return false;
}

bool GuideLibrary::body(uint32_t doc, std::string& out, bool check_crc) const {
  out.clear();
  if (!valid_ || doc >= n_docs_) return false;
  size_t r = doc_rec(doc);
  uint32_t bytes = u32(r + kDocBytes), off = u32(r + kDocBodyOff), len = u32(r + kDocBodyLen), crc = u32(r + kDocBodyCrc);
  const Ref& b = sec_[kSecBodies];
  if (off > b.len || len > b.len - off || bytes > kMaxGuideBytes) return false;
  if (!inflate_raw(pack_.sub(b.off + off, len), bytes, out)) return false;
  if ((check_crc && crc32_fast(ByteView(out)) != crc) || !utf8_valid(out)) { out.clear(); return false; }
  return true;
}

bool GuideLibrary::body_prefix(uint32_t doc, size_t max_bytes, std::string& out, size_t* work) const {
  out.clear();
  if (work) *work = 0;
  if (!valid_ || doc >= n_docs_) return false;
  size_t r = doc_rec(doc);
  uint32_t bytes = u32(r + kDocBytes), off = u32(r + kDocBodyOff), len = u32(r + kDocBodyLen);
  const Ref& b = sec_[kSecBodies];
  if (off > b.len || len > b.len - off || bytes > kMaxGuideBytes) return false;
  if (max_bytes >= bytes) {
    if (work) *work = static_cast<size_t>(len) + bytes;
    return body(doc, out, false);
  }
  // Input needed for max_bytes of output is at most ~9/8 of it (see load()); hand the decoder only
  // that slice so a hostile stream cannot make it chew through the whole body.
  size_t in_len = std::min<size_t>(len, max_bytes + max_bytes / 8 + 1024);
  if (work) *work = in_len + max_bytes;
  if (!inflate_raw_prefix(pack_.sub(b.off + off, in_len), bytes, max_bytes, out)) return false;
  out.resize(utf8_floor(out, out.size()));  // the cut may split a code point
  if (!utf8_valid(out)) { out.clear(); return false; }
  return true;
}

bool GuideLibrary::meta(std::string_view id, GuideMeta& out) const {
  uint32_t d;
  if (!find_doc(id, d)) return false;
  fill_meta(d, out);
  return true;
}

GetStatus GuideLibrary::get(std::string_view id, Guide& out) const {
  out = Guide();
  uint32_t d;
  if (!find_doc(id, d)) return GetStatus::kNotFound;
  fill_meta(d, out.meta);
  if (!body(d, out.markdown)) { out = Guide(); return GetStatus::kCorrupt; }
  uint32_t prev = u32(doc_rec(d) + kDocPrev), next = u32(doc_rec(d) + kDocNext);
  if (prev < n_docs_) out.prev = std::string(doc_id(prev));
  if (next < n_docs_) out.next = std::string(doc_id(next));
  return GetStatus::kOk;
}

// ---------------------------------------------------------------------------------------------
// Term dictionary

ByteView GuideLibrary::postings(const TermInfo& t) const {
  const Ref& ps = sec_[kSecPostings];
  if (t.post_off > ps.len || t.post_len > ps.len - t.post_off) return ByteView();
  return pack_.sub(ps.off + t.post_off, t.post_len);
}

bool GuideLibrary::scan_block(uint32_t block, TermVisitor visit, void* ctx, bool& stopped) const {
  stopped = false;
  if (block >= n_blocks_) return false;
  size_t e = sec_[kSecTermBlocks].off + 8 * static_cast<size_t>(block);
  uint32_t toff = u32(e), poff = u32(e + 4);
  const Ref& ts = sec_[kSecTerms];
  const Ref& ps = sec_[kSecPostings];
  if (toff >= ts.len || poff > ps.len) return false;
  ByteReader r(pack_.sub(ts.off + toff, ts.len - toff));
  uint32_t count = std::min<uint32_t>(kTermBlockSize, n_terms_ - block * kTermBlockSize);
  char buf[kMaxTermBytes];
  size_t len = 0;
  for (uint32_t k = 0; k < count; ++k) {
    if (k == 0) {
      uint64_t l = r.varint();
      if (l == 0 || l > kMaxTermBytes) return false;
      if (!r.copy(reinterpret_cast<uint8_t*>(buf), static_cast<size_t>(l))) return false;
      len = static_cast<size_t>(l);
    } else {
      uint64_t shared = r.varint(), suf = r.varint();
      if (!r.ok() || shared > len || suf > kMaxTermBytes - shared || shared + suf == 0) return false;
      if (!r.copy(reinterpret_cast<uint8_t*>(buf) + shared, static_cast<size_t>(suf))) return false;
      len = static_cast<size_t>(shared + suf);
    }
    uint64_t df = r.varint(), plen = r.varint();
    if (!r.ok() || df == 0 || df > n_docs_ || plen > ps.len - poff) return false;
    TermInfo info;
    info.df = static_cast<uint32_t>(df);
    info.post_off = poff;
    info.post_len = static_cast<uint32_t>(plen);
    if (!visit(ctx, std::string_view(buf, len), info)) { stopped = true; return true; }
    poff += static_cast<uint32_t>(plen);
  }
  return true;
}

bool GuideLibrary::block_first(uint32_t block, std::string& out) const {
  out.clear();
  bool stopped;
  auto v = [](void* c, std::string_view t, const TermInfo&) {
    static_cast<std::string*>(c)->assign(t.data(), t.size());
    return false;
  };
  return scan_block(block, v, &out, stopped) && stopped;
}

namespace {
// Index of the last block whose first term <= key (0 if none). Returns false on corruption.
template <class Lib>
bool locate_block(const Lib& lib, uint32_t n_blocks, std::string_view key, uint32_t& out,
                  bool (Lib::*first)(uint32_t, std::string&) const) {
  out = 0;
  uint32_t lo = 0, hi = n_blocks;
  std::string t;
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (!(lib.*first)(mid, t)) return false;
    if (std::string_view(t) <= key) { out = mid; lo = mid + 1; } else { hi = mid; }
  }
  return true;
}
}  // namespace

bool GuideLibrary::find_term(std::string_view term, TermInfo& out) const {
  if (!valid_ || n_blocks_ == 0 || term.empty() || term.size() > kMaxTermBytes) return false;
  uint32_t b;
  if (!locate_block(*this, n_blocks_, term, b, &GuideLibrary::block_first)) return false;
  struct Ctx { std::string_view want; TermInfo* out; bool found; } ctx{term, &out, false};
  auto v = [](void* c, std::string_view t, const TermInfo& info) {
    Ctx* x = static_cast<Ctx*>(c);
    int cmp = t.compare(x->want);
    if (cmp == 0) { *x->out = info; x->found = true; return false; }
    return cmp < 0;
  };
  bool stopped;
  if (!scan_block(b, v, &ctx, stopped)) return false;
  return ctx.found;
}

bool GuideLibrary::scan_prefix(std::string_view prefix, TermVisitor visit, void* ctx) const {
  if (!valid_ || n_blocks_ == 0) return true;
  uint32_t b;
  if (!locate_block(*this, n_blocks_, prefix, b, &GuideLibrary::block_first)) return false;
  struct Ctx { std::string_view prefix; TermVisitor visit; void* ctx; bool done; } x{prefix, visit, ctx, false};
  auto v = [](void* c, std::string_view t, const TermInfo& info) {
    Ctx* s = static_cast<Ctx*>(c);
    if (t.compare(0, s->prefix.size(), s->prefix) < 0) return true;           // before range
    if (t.substr(0, s->prefix.size()) != s->prefix) { s->done = true; return false; }  // past range
    if (!s->visit(s->ctx, t, info)) { s->done = true; return false; }
    return true;
  };
  // Hard cap on dictionary blocks visited per scan (131072 terms, 2.4x the real corpus's whole
  // dictionary): a hostile, unsorted dictionary must not turn every keystroke into a full scan.
  constexpr uint32_t kMaxScanBlocks = 4096;
  for (uint32_t end = b + std::min(kMaxScanBlocks, n_blocks_ - b); b < end && !x.done; ++b) {
    bool stopped;
    if (!scan_block(b, v, &x, stopped)) return false;
  }
  return true;
}

// ---------------------------------------------------------------------------------------------
// Deep verification and stats

bool GuideLibrary::verify(std::string* error) const {
  auto err = [&](const std::string& m) { if (error) *error = m; return false; };
  if (!valid_) return err("not loaded");
  const uint8_t* e = pack_.data() + kHdrSections + 12 * kSecBodies;
  if (crc32_fast(pack_.sub(sec_[kSecBodies].off, sec_[kSecBodies].len)) != rd32(e + 8))
    return err("bodies checksum mismatch");
  std::string md;
  for (uint32_t d = 0; d < n_docs_; ++d)
    if (!body(d, md)) return err("corrupt body: " + std::string(doc_id(d)));
  // Walk the whole dictionary and every posting list.
  uint32_t seen = 0;
  struct Ctx { const GuideLibrary* lib; uint32_t* seen; bool bad; std::string last; } ctx{this, &seen, false, std::string()};
  auto v = [](void* c, std::string_view t, const TermInfo& info) {
    Ctx* x = static_cast<Ctx*>(c);
    if (!x->last.empty() && !(std::string_view(x->last) < t)) { x->bad = true; return false; }
    x->last.assign(t.data(), t.size());
    ByteReader r(x->lib->postings(info));
    uint64_t doc = 0;
    for (uint32_t k = 0; k < info.df; ++k) {
      uint64_t gap = r.varint();
      doc = (k == 0) ? gap : doc + gap;
      if ((k > 0 && gap == 0) || doc >= x->lib->n_docs_) { x->bad = true; return false; }
      uint8_t mask = r.u8();
      if (mask == 0 || mask > 15) { x->bad = true; return false; }
      for (int f = 0; f < kFieldCount; ++f)
        if (mask & (1 << f)) r.varint();
    }
    if (!r.at_end()) { x->bad = true; return false; }
    ++*x->seen;
    return true;
  };
  for (uint32_t b = 0; b < n_blocks_; ++b) {
    bool stopped;
    if (!scan_block(b, v, &ctx, stopped) || ctx.bad) return err("corrupt term dictionary or postings");
  }
  if (seen != n_terms_) return err("term count mismatch");
  if (sem_.available() && !sem_.verify(error)) return false;
  return true;
}

PackStats GuideLibrary::stats() const {
  PackStats s;
  if (!valid_) return s;
  s.pack_bytes = pack_.size();
  s.docs = n_docs_;
  s.categories = n_cats_;
  s.terms = n_terms_;
  for (int i = 0; i < 8; ++i) s.section_bytes[i] = sec_[i].len;
  for (uint32_t d = 0; d < n_docs_; ++d) s.body_bytes += u32(doc_rec(d) + kDocBytes);
  s.semantic = info().semantic;
  s.sem_model_bytes = sem_.model_bytes();
  s.sem_passage_bytes = sem_.passage_table_bytes();
  s.sem_vector_bytes = sem_.vector_bytes();
  s.sem_guide_bytes = sem_.guide_vector_bytes();
  return s;
}

LibraryInfo GuideLibrary::info() const {
  LibraryInfo li;
  if (!valid_) { li.semantic.status = "no library loaded"; return li; }
  li.guides = n_docs_;
  li.categories = n_cats_;
  li.bytes = pack_.size();
  SemanticInfo& si = li.semantic;
  si.available = sem_.available();
  si.status = sem_status_;
  if (si.available) {
    si.model = std::string(sem_.model().name());
    si.dims = sem_.dims();
    si.bits = sem_.model().bits();
    si.vector_bits = sem_.bits();
    si.passages = sem_.passages();
    si.bytes = sem_.bytes();
  }
  std::vector<std::string_view> lic(n_docs_);
  for (uint32_t d = 0; d < n_docs_; ++d) lic[d] = str_at(doc_rec(d) + kDocLicense);
  std::sort(lic.begin(), lic.end());
  for (size_t i = 0; i < lic.size();) {
    size_t j = i;
    while (j < lic.size() && lic[j] == lic[i]) ++j;
    li.licences.emplace_back(std::string(lic[i]), static_cast<uint32_t>(j - i));
    i = j;
  }
  std::stable_sort(li.licences.begin(), li.licences.end(),
                   [](const auto& a, const auto& b) { return a.second > b.second; });
  return li;
}

}  // namespace ember::guides
