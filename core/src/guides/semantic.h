// EmberMesh guides — the semantic part of a loaded pack (extension sections, pack_format.h):
// the embedding model, passages and their vectors, and brute-force integer vector scoring.
//
// Zero-copy over the pack bytes; the only heap use is per query (query vector, one float score
// per passage). Brute force is deliberate: the corpus has ~1e5 passages, and one linear pass of
// integer dot products is faster on a phone than any index we could afford to store.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ember/bytes.h"
#include "guides/embed.h"

namespace ember::guides {

struct PassageRec {
  uint32_t off = 0, len = 0, head_off = 0, head_len = 0;
  bool meta = false;  // the guide's metadata passage (title, tags, summary): no body range
};

class SemanticIndex {
 public:
  // Parses and validates the extension sections. On any problem returns false with *why set; the
  // index is then empty. doc_bytes[d] = uncompressed body size of doc d (for range checks).
  bool load(ByteView pack, uint32_t ext_dir, uint32_t header_size, const std::vector<uint32_t>& doc_bytes,
            std::string* why);
  void clear();
  bool available() const { return ok_; }
  bool has_guide_vectors() const { return ok_ && g_vec_ != 0; }

  const EmbedModel& model() const { return model_; }
  uint32_t passages() const { return n_pass_; }
  uint32_t dims() const { return dims_; }
  uint32_t bits() const { return bits_; }
  size_t bytes() const { return bytes_; }
  size_t model_bytes() const { return model_.bytes(); }
  size_t passage_table_bytes() const { return pass_len_; }
  size_t vector_bytes() const { return vec_len_; }
  size_t guide_vector_bytes() const { return g_len_; }

  uint32_t first(uint32_t doc) const;  // passages of doc d are [first(d), first(d+1))
  PassageRec passage(uint32_t p) const;

  // A query vector in the form the scorer needs.
  struct Query {
    std::vector<int8_t> q8;   // 4/8-bit stores: int8 codes
    std::vector<double> qf;   // float store: unit vector
    double norm = 0;          // |q8|
    bool ok = false;
  };
  bool prepare_text(std::string_view text, Query& q) const;
  bool prepare_acc(const std::vector<int64_t>& acc, Query& q) const;
  // The stored vector of passage p (or guide d) as a query, for "more like this".
  bool prepare_guide(uint32_t doc, Query& q) const;

  // Cosine similarity of q with passages [lo, hi) into out[0 .. hi-lo).
  void score_passages(const Query& q, uint32_t lo, uint32_t hi, float* out) const;
  // Cosine similarity of q with every guide vector (out has doc_count entries).
  void score_guides(const Query& q, float* out) const;

  bool verify(std::string* why) const;

 private:
  float cosine(const Query& q, size_t vec_base, size_t norm_base, uint32_t i) const;
  void decode(size_t vec_base, uint32_t i, std::vector<int64_t>& out) const;

  ByteView pack_;
  EmbedModel model_;
  bool ok_ = false;
  uint32_t n_pass_ = 0, n_docs_ = 0, dims_ = 0, bits_ = 0, norm_bytes_ = 0, flags_ = 0;
  size_t pass_off_ = 0, pass_len_ = 0;  // SEMP
  size_t vec_off_ = 0, vec_len_ = 0;    // SEMV codes base
  size_t g_vec_ = 0, g_len_ = 0;        // SEMG codes base (0 = absent)
  uint32_t vec_crc_ = 0, g_crc_ = 0, model_crc_ = 0;
  size_t vec_sec_ = 0, g_sec_ = 0, model_sec_ = 0, model_sec_len_ = 0;
  size_t bytes_ = 0;
};

}  // namespace ember::guides
