// EmberMesh guides — the offline guide library: loads a `guides.pack` (see guides/pack_format.h)
// and serves the `guides.*` RPC methods (docs/RPC_API.md) as plain C++ structs:
//
//   guides.categories -> GuideLibrary::categories()      {categories:[{id,title,count}], total}
//   guides.list       -> GuideLibrary::list(...)         {guides:[GuideMeta], total}
//   guides.get        -> GuideLibrary::get(id, out)      GuideMeta + {markdown, prev?, next?}
//   guides.search     -> GuideLibrary::search(...)       {results:[{id,title,category,score,snippet,terms,match,passage?}]}
//   guides.related    -> GuideLibrary::related(id, limit)  {results:[GuideMeta + {score}], weak?}
//   guides.ask        -> GuideLibrary::ask(...)          {passages:[{id,title,category,heading?,text,score}], weak?}
//   guides.info       -> GuideLibrary::info()            {guides, categories, bytes, semantic:{...}, licences}
//
// Semantic search (docs/RPC_API.md §7) is optional: it needs the pack's extension sections
// (pack_format.h). Without them, or if they are damaged, the library is keyword-only.
//
// Zero-copy: with copy=false the library reads the caller's bytes in place (they must stay valid
// and unchanged while loaded); nothing proportional to the corpus is copied to the heap. Bodies
// are decompressed on demand. Single-threaded use per instance (const methods keep no state, so
// concurrent const calls on one loaded instance are safe).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ember/bytes.h"
#include "guides/semantic.h"

namespace ember::guides {

struct GuideMeta {
  std::string id;
  std::string title;
  std::string category;
  std::string summary;
  std::vector<std::string> tags;
  std::string source;
  std::string source_title;
  std::string license;
  std::string attribution;
  uint32_t bytes = 0;      // size of the Markdown body
  std::string part_of;     // "" = not part of a book (omit in JSON)
  bool has_order = false;  // false = omit `order` in JSON
  int32_t order = 0;
};

struct CategoryInfo {
  std::string id;
  std::string title;
  uint32_t count = 0;
};

struct GuideList {
  std::vector<GuideMeta> guides;
  uint32_t total = 0;  // number of matches before offset/limit
};

struct Guide {
  GuideMeta meta;
  std::string markdown;
  std::string prev;  // id of the previous chapter in the same book, "" = none (omit in JSON)
  std::string next;  // id of the next chapter, "" = none
};

struct PassageText {
  std::string text;     // plain text, valid UTF-8, at most kPassageChars bytes ('…' marks a cut)
  std::string heading;  // "" = none
};
constexpr size_t kPassageChars = 600;

struct SearchResult {
  std::string id;
  std::string title;
  std::string category;
  double score = 0;
  std::string snippet;             // plain text, valid UTF-8, '…' marks elisions
  std::vector<std::string> terms;  // matched words as they appear (lowercased), for highlighting
  std::string match = "words";     // "words" | "meaning" | "both"
  bool has_passage = false;        // passage: the guide's best-matching passage (semantic only)
  PassageText passage;
};

// Hybrid ranking parameters (search.cpp). The defaults are the tuned values; the overload of
// search() that takes them exists for evaluation (guidepack eval) only.
struct HybridParams {
  double lambda_strong = 0.12;  // weight of meaning when the keyword match is strong
  double lambda_weak = 1.6;     // ... when it is weak (lay wording, typos, paraphrase)
  double guide_weight = 0.3;    // guide-vector cosine vs best-passage cosine in a guide's meaning score
  uint32_t semantic_pool = 60;  // guides taken from the meaning ranking into the candidate pool
};

// cosine (-1..1) -> similarity reported to the UI (0..1): linear between kSimLow (unrelated
// text) and kSimHigh (paraphrase), clamped.
constexpr double kSimLow = 0.40, kSimHigh = 0.80;
inline double calibrate_similarity(double cosv) {
  double s = (cosv - kSimLow) / (kSimHigh - kSimLow);
  return s < 0 ? 0 : (s > 1 ? 1 : s);
}

enum class SearchMode {
  kAuto,      // hybrid when the pack has a semantic section, else keyword
  kKeyword,   // keyword ranking only (guides.search semantic:false)
  kSemantic,  // meaning only (evaluation)
  kHybrid,    // keyword + meaning (falls back to keyword without a semantic section)
};

struct RelatedResult {
  GuideMeta meta;
  double score = 0;  // similarity 0..1
};
struct RelatedList {
  std::vector<RelatedResult> results;
  bool weak = false;  // even the best result is below kWeakScore
};

struct AskPassage {
  std::string id, title, category, heading, text;
  double score = 0;  // similarity 0..1
};
struct AskResult {
  std::vector<AskPassage> passages;
  bool weak = false;
};

// Similarity scores reported to the UI are calibrated cosines: 0 = unrelated, 1 = same meaning.
// Results whose best score is below kWeakScore are flagged weak (see docs/RPC_API.md §7).
constexpr double kWeakScore = 0.30;

struct SemanticInfo {
  bool available = false;
  std::string model;    // e.g. "potion-base-8M@bf8b056651a2"
  uint32_t dims = 0, bits = 0, vector_bits = 0, passages = 0;
  size_t bytes = 0;     // size of the semantic sections
  std::string status;   // "ok" or why semantic search is unavailable
};

struct LibraryInfo {
  uint32_t guides = 0, categories = 0;
  size_t bytes = 0;  // pack size
  SemanticInfo semantic;
  std::vector<std::pair<std::string, uint32_t>> licences;  // {licence, guides}, most used first
};

enum class LoadStatus {
  kOk = 0,
  kTooSmall = -1,
  kBadMagic = -2,
  kBadVersion = -3,
  kBadChecksum = -4,
  kBadLayout = -5,
  kTooLarge = -6,
};
const char* load_status_name(LoadStatus s);

enum class GetStatus { kOk, kNotFound, kCorrupt };

struct PackStats {
  size_t pack_bytes = 0;
  uint32_t docs = 0, categories = 0, terms = 0;
  size_t section_bytes[8] = {0};
  uint64_t body_bytes = 0;  // total uncompressed Markdown
  SemanticInfo semantic;
  size_t sem_model_bytes = 0, sem_passage_bytes = 0, sem_vector_bytes = 0, sem_guide_bytes = 0;
};

class GuideLibrary {
 public:
  GuideLibrary() = default;
  GuideLibrary(const GuideLibrary&) = delete;
  GuideLibrary& operator=(const GuideLibrary&) = delete;

  // Replaces the current library. copy=false: zero-copy (see above). On failure the library is
  // left empty (every query returns nothing) and the status says why.
  LoadStatus load(ByteView pack, bool copy);
  void clear();
  bool loaded() const { return valid_; }

  uint32_t total() const { return n_docs_; }
  std::vector<CategoryInfo> categories() const;

  // Filters are optional ("" = any). Ordered by category, then book (part_of) / title, chapter
  // order, title. limit is clamped to [0, 10000].
  GuideList list(std::string_view category, std::string_view part_of, uint32_t offset, uint32_t limit) const;

  GetStatus get(std::string_view id, Guide& out) const;
  // Metadata only (no decompression).
  bool meta(std::string_view id, GuideMeta& out) const;

  // Ranked full-text search. Empty/garbage queries return no results. limit clamped to [1, 200].
  std::vector<SearchResult> search(std::string_view query, std::string_view category, uint32_t limit) const;
  std::vector<SearchResult> search(std::string_view query, std::string_view category, uint32_t limit,
                                   SearchMode mode, const HybridParams& params = HybridParams()) const;
  // Guides closest in meaning to `id` (empty without a semantic section). limit clamped to [1, 50].
  RelatedList related(std::string_view id, uint32_t limit) const;
  // Passages that best answer a question, across the library (or one category). limit [1, 20].
  AskResult ask(std::string_view question, std::string_view category, uint32_t limit) const;
  LibraryInfo info() const;
  bool semantic_available() const { return valid_ && sem_.available(); }
  const SemanticIndex& semantic() const { return sem_; }

  // Deep check: BODIES checksum and every body decompresses to its recorded size/CRC.
  bool verify(std::string* error = nullptr) const;
  PackStats stats() const;

  // Internal accessors (used by search.cpp; not part of the RPC surface).
  struct TermInfo {
    uint32_t df = 0;
    uint32_t post_off = 0;
    uint32_t post_len = 0;
  };
  bool find_term(std::string_view term, TermInfo& out) const;
  // Calls visit(ctx, term, info) for each dictionary term starting with prefix, in order, until it
  // returns false. Returns false if the dictionary is corrupt.
  using TermVisitor = bool (*)(void* ctx, std::string_view term, const TermInfo& info);
  bool scan_prefix(std::string_view prefix, TermVisitor visit, void* ctx) const;
  // Raw postings bytes of a term (bounds already checked).
  ByteView postings(const TermInfo& t) const;

 private:
  friend class Searcher;
  friend class SemanticSearcher;
  struct Ref { uint32_t off = 0, len = 0; };

  uint32_t u32(size_t off) const;
  std::string_view str_at(size_t rec_off) const;  // StrRef at absolute offset
  size_t doc_rec(uint32_t doc) const { return docs_off_ + static_cast<size_t>(doc) * 128u; }
  void fill_meta(uint32_t doc, GuideMeta& m) const;
  bool find_doc(std::string_view id, uint32_t& doc) const;
  // check_crc=false skips the CRC (snippets); UTF-8 validity and all bounds are always checked.
  bool body(uint32_t doc, std::string& out, bool check_crc = true) const;
  // First max_bytes of the body (whole body if smaller; CRC not checked), cut to a code point
  // boundary. *work (may be null) receives the compressed + uncompressed bytes the call may touch.
  bool body_prefix(uint32_t doc, size_t max_bytes, std::string& out, size_t* work) const;
  std::string_view doc_id(uint32_t doc) const;
  std::string_view doc_title(uint32_t doc) const;
  uint32_t doc_category(uint32_t doc) const { return u32(doc_rec(doc) + 72); }
  uint32_t field_len(uint32_t doc, int f) const { return u32(doc_rec(doc) + 108 + 4u * static_cast<unsigned>(f)); }
  int find_category(std::string_view id) const;
  // Decodes block entries in order; visit returning false stops. `stopped` tells the caller.
  // Returns false on corruption.
  bool scan_block(uint32_t block, TermVisitor visit, void* ctx, bool& stopped) const;
  bool block_first(uint32_t block, std::string& out) const;

  // Text of a stored passage (plain, cut to kPassageChars at a word boundary). md = the guide body.
  PassageText passage_text(uint32_t doc, uint32_t passage, const std::string& md) const;
  void load_semantic();

  SemanticIndex sem_;
  std::string sem_status_;
  Bytes own_;
  ByteView pack_;
  bool valid_ = false;
  uint32_t n_docs_ = 0, n_cats_ = 0, n_terms_ = 0, n_blocks_ = 0;
  Ref sec_[8];
  size_t docs_off_ = 0;
  double avg_len_[4] = {1, 1, 1, 1};
};

}  // namespace ember::guides
