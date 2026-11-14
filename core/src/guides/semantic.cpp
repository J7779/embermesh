#include "guides/semantic.h"

#include <cmath>
#include <cstring>

#include "guides/deflate.h"
#include "guides/pack_format.h"

namespace ember::guides {

namespace {
uint32_t rd32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 | static_cast<uint32_t>(p[2]) << 16 |
         static_cast<uint32_t>(p[3]) << 24;
}
uint32_t rd16(const uint8_t* p) { return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8; }
float rdf(const uint8_t* p) {
  uint32_t u = rd32(p);
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}
}  // namespace

void SemanticIndex::clear() { *this = SemanticIndex(); }

bool SemanticIndex::load(ByteView pack, uint32_t ext_dir, uint32_t header_size, const std::vector<uint32_t>& doc_bytes,
                         std::string* why) {
  clear();
  auto fail = [&](const char* m) { clear(); if (why) *why = m; return false; };
  const size_t size = pack.size();
  const uint8_t* p = pack.data();
  if (ext_dir == 0) return fail("no semantic section");
  if (ext_dir < header_size || ext_dir > size || size - ext_dir < 16) return fail("bad extension directory");
  if (std::memcmp(p + ext_dir, kExtMagic, sizeof kExtMagic) != 0) return fail("bad extension directory");
  const uint32_t count = rd32(p + ext_dir + 8);
  if (count > kMaxExtSections || static_cast<uint64_t>(count) * kExtEntrySize > size - ext_dir - 16)
    return fail("bad extension directory");
  if (crc32_fast(pack.sub(ext_dir + 16, static_cast<size_t>(count) * kExtEntrySize)) != rd32(p + ext_dir + 12))
    return fail("extension directory checksum");
  struct Ent { size_t off = 0, len = 0; uint32_t crc = 0; bool have = false; };
  Ent model, pass, vec, gv;
  for (uint32_t i = 0; i < count; ++i) {
    const uint8_t* e = p + ext_dir + 16 + static_cast<size_t>(i) * kExtEntrySize;
    uint32_t tag = rd32(e), ver = rd32(e + 4), off = rd32(e + 8), len = rd32(e + 12), crc = rd32(e + 16);
    if (off < header_size || off > size || len > size - off) return fail("extension section out of range");
    Ent* t = tag == kTagSemModel ? &model : tag == kTagSemPassages ? &pass : tag == kTagSemVectors ? &vec
           : tag == kTagSemGuides ? &gv : nullptr;
    if (!t || t->have || ver != kSemVersion) continue;  // unknown / newer / duplicate: ignored
    t->off = off; t->len = len; t->crc = crc; t->have = true;
  }
  if (!model.have || !pass.have || !vec.have) return fail("semantic sections incomplete");
  pack_ = pack;
  // Model (validates itself; its table CRC is left to verify()).
  ModelStatus ms = model_.load(pack.sub(model.off, model.len));
  if (ms != ModelStatus::kOk) {
    clear();
    if (why) *why = std::string("model: ") + model_status_name(ms);
    return false;
  }
  model_sec_ = model.off;
  model_sec_len_ = model.len;
  model_crc_ = model.crc;
  // Passages: CRC-checked now (small), every record range-checked against its guide.
  if (crc32_fast(pack.sub(pass.off, pass.len)) != pass.crc) return fail("passage table checksum");
  if (pass.len < kSemPassageHeader) return fail("passage table too small");
  const uint32_t P = rd32(p + pass.off), N = rd32(p + pass.off + 4), flags = rd32(p + pass.off + 8);
  if (N != doc_bytes.size() || P > kMaxPassages) return fail("passage table counts");
  if (pass.len != kSemPassageHeader + 4ull * (N + 1) + static_cast<uint64_t>(P) * kSemPassageRecord)
    return fail("passage table size");
  const size_t firsts = pass.off + kSemPassageHeader, recs = firsts + 4ull * (N + 1);
  if (rd32(p + firsts) != 0 || rd32(p + firsts + 4ull * N) != P) return fail("passage table ranges");
  for (uint32_t d = 0; d < N; ++d) {
    uint32_t a = rd32(p + firsts + 4ull * d), b = rd32(p + firsts + 4ull * (d + 1));
    if (b < a || b > P) return fail("passage table ranges");
    for (uint32_t k = a; k < b; ++k) {
      const uint8_t* r = p + recs + static_cast<size_t>(k) * kSemPassageRecord;
      uint64_t off = rd32(r), len = rd16(r + 4), hl = rd16(r + 6), ho = rd32(r + 8);
      if (off + len > doc_bytes[d] || ho + hl > doc_bytes[d]) return fail("passage outside its guide");
    }
  }
  // Vectors: sizes must match exactly.
  auto check_vec = [&](const Ent& e, uint32_t want_count) {
    if (e.len < kSemVectorHeader) return false;
    uint32_t c = rd32(p + e.off), d = rd32(p + e.off + 4), b = rd32(p + e.off + 8), nb = rd32(p + e.off + 12);
    if (c != want_count || d != model_.dims() || !(b == 4 || b == 8 || b == 32)) return false;
    if (nb != (b == 4 ? 2u : 4u)) return false;
    uint64_t row = static_cast<uint64_t>(d) * b / 8;
    return e.len == kSemVectorHeader + static_cast<uint64_t>(c) * (row + nb);
  };
  if (!check_vec(vec, P)) return fail("passage vectors layout");
  dims_ = model_.dims();
  bits_ = rd32(p + vec.off + 8);
  norm_bytes_ = bits_ == 4 ? 2 : 4;
  if (gv.have) {
    if (!check_vec(gv, N) || rd32(p + gv.off + 8) != bits_) return fail("guide vectors layout");
    g_vec_ = gv.off + kSemVectorHeader;
    g_len_ = gv.len;
    g_sec_ = gv.off;
    g_crc_ = gv.crc;
  }
  n_pass_ = P;
  n_docs_ = N;
  flags_ = flags;
  pass_off_ = pass.off;
  pass_len_ = pass.len;
  vec_off_ = vec.off + kSemVectorHeader;
  vec_len_ = vec.len;
  vec_sec_ = vec.off;
  vec_crc_ = vec.crc;
  bytes_ = model.len + pass.len + vec.len + (gv.have ? gv.len : 0) + 16 + static_cast<size_t>(count) * kExtEntrySize;
  ok_ = true;
  return true;
}

uint32_t SemanticIndex::first(uint32_t doc) const {
  if (!ok_ || doc > n_docs_) return n_pass_;
  return rd32(pack_.data() + pass_off_ + kSemPassageHeader + 4ull * doc);
}

PassageRec SemanticIndex::passage(uint32_t i) const {
  PassageRec r;
  if (!ok_ || i >= n_pass_) return r;
  const uint8_t* e = pack_.data() + pass_off_ + kSemPassageHeader + 4ull * (n_docs_ + 1) +
                     static_cast<size_t>(i) * kSemPassageRecord;
  r.off = rd32(e);
  r.len = rd16(e + 4);
  r.head_len = rd16(e + 6);
  r.head_off = rd32(e + 8);
  r.meta = r.len == 0;
  return r;
}

bool SemanticIndex::prepare_acc(const std::vector<int64_t>& acc, Query& q) const {
  q = Query();
  if (!ok_ || acc.size() != dims_) return false;
  if (bits_ == 32) {
    double ss = 0;
    for (int64_t a : acc) ss += static_cast<double>(a) * static_cast<double>(a);
    if (ss <= 0) return false;
    double inv = 1.0 / std::sqrt(ss);
    q.qf.resize(dims_);
    for (uint32_t i = 0; i < dims_; ++i) q.qf[i] = static_cast<double>(acc[i]) * inv;
    q.norm = 1.0;
  } else {
    q.q8.resize(dims_);
    uint32_t ss = quantize_levels(acc.data(), dims_, 127, q.q8.data());
    if (ss == 0) return false;
    q.norm = std::sqrt(static_cast<double>(ss));
  }
  q.ok = true;
  return true;
}

bool SemanticIndex::prepare_text(std::string_view text, Query& q) const {
  q = Query();
  if (!ok_) return false;
  std::vector<int64_t> acc(dims_, 0);
  if (model_.embed(text, acc.data()) == 0) return false;
  return prepare_acc(acc, q);
}

void SemanticIndex::decode(size_t base, uint32_t i, std::vector<int64_t>& out) const {
  out.assign(dims_, 0);
  const size_t row = static_cast<size_t>(dims_) * bits_ / 8;
  const uint8_t* v = pack_.data() + base + static_cast<size_t>(i) * row;
  if (bits_ == 4) {
    for (uint32_t j = 0; j < dims_ / 2; ++j) {
      out[2 * j] = static_cast<int64_t>(v[j] & 15) - 8;
      out[2 * j + 1] = static_cast<int64_t>(v[j] >> 4) - 8;
    }
  } else if (bits_ == 8) {
    for (uint32_t j = 0; j < dims_; ++j) out[j] = static_cast<int8_t>(v[j]);
  } else {
    for (uint32_t j = 0; j < dims_; ++j) {
      double f = rdf(v + 4ull * j);
      if (!(std::fabs(f) <= 1.0)) f = 0;
      out[j] = static_cast<int64_t>(std::llround(f * 1048576.0));
    }
  }
}

bool SemanticIndex::prepare_guide(uint32_t doc, Query& q) const {
  q = Query();
  if (!has_guide_vectors() || doc >= n_docs_) return false;
  std::vector<int64_t> acc;
  decode(g_vec_, doc, acc);
  return prepare_acc(acc, q);
}

float SemanticIndex::cosine(const Query& q, size_t base, size_t norm_base, uint32_t i) const {
  const size_t row = static_cast<size_t>(dims_) * bits_ / 8;
  const uint8_t* v = pack_.data() + base + static_cast<size_t>(i) * row;
  const uint8_t* nb = pack_.data() + norm_base + static_cast<size_t>(i) * norm_bytes_;
  if (bits_ == 32) {
    double dot = 0;
    for (uint32_t j = 0; j < dims_; ++j) {
      double f = rdf(v + 4ull * j);
      if (!(std::fabs(f) <= 1.0)) f = 0;
      dot += q.qf[j] * f;
    }
    double n = rdf(nb);
    if (!(n > 0) || !(n < 1e6)) return 0;
    return static_cast<float>(dot / n);
  }
  int32_t dot = 0;
  const int8_t* qq = q.q8.data();
  if (bits_ == 4) {
    for (uint32_t j = 0; j < dims_ / 2; ++j) {
      uint32_t b = v[j];
      dot += qq[2 * j] * (static_cast<int32_t>(b & 15) - 8) + qq[2 * j + 1] * (static_cast<int32_t>(b >> 4) - 8);
    }
  } else {
    for (uint32_t j = 0; j < dims_; ++j) dot += qq[j] * static_cast<int32_t>(static_cast<int8_t>(v[j]));
  }
  uint32_t ss = bits_ == 4 ? rd16(nb) : rd32(nb);
  if (ss == 0) return 0;
  return static_cast<float>(static_cast<double>(dot) / (q.norm * std::sqrt(static_cast<double>(ss))));
}

void SemanticIndex::score_passages(const Query& q, uint32_t lo, uint32_t hi, float* out) const {
  if (!ok_ || !q.ok || hi > n_pass_ || lo >= hi) return;
  const size_t row = static_cast<size_t>(dims_) * bits_ / 8;
  const size_t norms = vec_off_ + static_cast<size_t>(n_pass_) * row;
  for (uint32_t i = lo; i < hi; ++i) out[i - lo] = cosine(q, vec_off_, norms, i);
}

void SemanticIndex::score_guides(const Query& q, float* out) const {
  if (!has_guide_vectors() || !q.ok) return;
  const size_t row = static_cast<size_t>(dims_) * bits_ / 8;
  const size_t norms = g_vec_ + static_cast<size_t>(n_docs_) * row;
  for (uint32_t d = 0; d < n_docs_; ++d) out[d] = cosine(q, g_vec_, norms, d);
}

bool SemanticIndex::verify(std::string* why) const {
  auto err = [&](const char* m) { if (why) *why = m; return false; };
  if (!ok_) return err("semantic index not loaded");
  if (!model_.verify_table() || crc32_fast(pack_.sub(model_sec_, model_sec_len_)) != model_crc_)
    return err("semantic model checksum mismatch");
  if (crc32_fast(pack_.sub(vec_sec_, vec_len_)) != vec_crc_) return err("passage vectors checksum mismatch");
  if (g_vec_ && crc32_fast(pack_.sub(g_sec_, g_len_)) != g_crc_) return err("guide vectors checksum mismatch");
  return true;
}

}  // namespace ember::guides
