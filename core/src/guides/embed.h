// EmberMesh guides — the static token-embedding model (format: guides/embed_format.h): a WordPiece
// tokenizer that reproduces the model's reference tokenizer token for token, and text embedding
// as a sum of quantised token rows. No neural network runtime, no Unicode library, no floating
// point in the default (4/8-bit) path: everything below is integer arithmetic, so the packer and
// every app compute bit-identical vectors on every platform.
//
// Zero-copy: load() validates the model bytes and keeps pointers into them (they must stay valid
// while loaded). The only owned state is a 128-entry ASCII fast-path table.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ember/bytes.h"

namespace ember::guides {

enum class ModelStatus {
  kOk = 0,
  kTooSmall,
  kBadMagic,
  kBadVersion,
  kBadChecksum,
  kBadLayout,
  kUnsupported,
};
const char* model_status_name(ModelStatus s);

// Caps on work per text: tokens beyond these are ignored. Passages and queries are far below.
constexpr size_t kMaxEmbedTokens = 8192;
constexpr size_t kMaxEmbedBytes = 64u << 10;

class EmbedModel {
 public:
  EmbedModel() = default;
  ModelStatus load(ByteView data);
  void clear();
  bool loaded() const { return ok_; }

  uint32_t vocab_size() const { return vocab_; }
  uint32_t dims() const { return dims_; }
  uint32_t bits() const { return bits_; }
  std::string_view name() const { return name_; }
  size_t bytes() const { return data_.size(); }
  std::string_view token(uint32_t id) const;  // "" if out of range

  // WordPiece token ids of `text` (UTF-8; invalid bytes are dropped like the reference drops
  // U+FFFD), [UNK] removed as Model2Vec does. Appends at most max_ids ids. Returns the number
  // appended.
  size_t tokenize(std::string_view text, std::vector<uint32_t>& ids, size_t max_ids = kMaxEmbedTokens) const;

  // acc[0..dims) += row(id). Integer models add value * scale exactly; float models add
  // round(weight * 2^24) (fixed point), so the caller always works with int64 vectors.
  void add_row(uint32_t id, int64_t* acc) const;
  // acc (dims entries, zeroed by the caller) += every token of text. Returns the token count.
  size_t embed(std::string_view text, int64_t* acc) const;

  // CRC of the table (the part load() does not checksum).
  bool verify_table() const;

 private:
  struct Norm { uint32_t kind; uint32_t payload; };
  Norm norm_of(uint32_t cp, uint32_t& run_start) const;
  uint32_t class_of(uint32_t cp) const;
  bool lookup(const char* s, size_t n, uint32_t& id) const;
  void segment(std::string_view text, std::vector<uint32_t>& ids, size_t max_ids, std::string& word) const;
  void word_pieces(const std::string& word, size_t chars, std::vector<uint32_t>& ids, size_t max_ids) const;
  uint32_t rd(size_t off) const;

  ByteView data_;
  bool ok_ = false;
  uint32_t vocab_ = 0, dims_ = 0, bits_ = 0, unk_ = 0, max_word_chars_ = 0;
  uint32_t n_runs_ = 0, n_classes_ = 0, n_specials_ = 0;
  size_t runs_off_ = 0, strs_off_ = 0, strs_len_ = 0, classes_off_ = 0, specials_off_ = 0;
  size_t voff_off_ = 0, blob_off_ = 0, blob_len_ = 0, sorted_off_ = 0, scales_off_ = 0, table_off_ = 0;
  size_t row_bytes_ = 0;
  uint32_t table_crc_ = 0;
  std::string_view name_;
  // ASCII fast path: mapped byte (0 = dropped) and class, derived from the tables at load.
  uint8_t ascii_map_[128] = {};
  uint8_t ascii_cls_[128] = {};
};

// ---- vector quantisation (shared by the packer and the search) ------------------------------

// Symmetric quantisation of an int64 vector to `levels` (7 for 4 bits, 127 for 8 bits), rounding
// half away from zero with integer arithmetic. out receives dims int8 values. Returns sum of
// squares of the codes (0 for an all-zero vector).
uint32_t quantize_levels(const int64_t* acc, uint32_t dims, int levels, int8_t* out);

// Encodes acc as a stored vector of `bits` (4: two codes per byte, low nibble first, code + 8;
// 8: int8; 32: float32 of acc / max|acc|). Appends dims*bits/8 bytes to `vec` and the norm entry
// (4 bits: u16 sum of squares; 8 bits: u32 sum of squares; 32 bits: float32 L2 norm) to `norms`.
void encode_vector(const int64_t* acc, uint32_t dims, uint32_t bits, Bytes& vec, Bytes& norms);

}  // namespace ember::guides
