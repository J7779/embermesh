// EmberMesh guides — corpus validation (ARCHITECTURE §7) and `guides.pack` building.
//
// Pure functions over in-memory file contents: tools/guidepack does the directory walk and file
// I/O; the unit tests drive these directly. The output of build_pack() is deterministic: the same
// set of guides always produces byte-identical packs (no timestamps, no unordered containers).
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "ember/bytes.h"
#include "guides/library.h"
#include "guides/passages.h"

namespace ember::guides {

struct SourceGuide {
  GuideMeta meta;        // meta.bytes is recomputed by build_pack
  std::string markdown;  // body without front matter
};

struct Diagnostic {
  std::string file;  // path relative to the content dir, e.g. "water/boiling.md"
  int line = 0;      // 1-based, 0 = whole file
  bool error = true; // false = warning
  std::string message;
};

struct SourceFile {
  std::string path;     // relative to the content dir, '/' separators
  std::string content;  // raw bytes
};

struct ValidateOptions {
  size_t max_bytes = 64 * 1024;   // hard limit per file (error)
  size_t warn_bytes = 60 * 1000;  // §7 "keep each file under ~60 KB" (warning)
};

// Licence allow-list: public domain, CC0, CC BY, CC BY-SA (any version), and other licences that
// permit redistribution and modification for any purpose (US Gov works, OGL, MIT/BSD/Apache, GFDL,
// Unlicense, WTFPL). Rejects anything NC / ND / "all rights reserved" / unknown.
bool license_allowed(std::string_view license, std::string* why = nullptr);

// Validates one guide file. Appends diagnostics; returns true (and fills out) if it has no errors.
bool parse_guide_file(const SourceFile& file, SourceGuide& out, std::vector<Diagnostic>& diags,
                      const ValidateOptions& opt = ValidateOptions());

// Validates a whole corpus: every file, plus cross-file checks (duplicate ids, chapter order
// collisions within part_of). Files whose path has a component starting with '_' or that do not
// end in ".md" are skipped (the caller may also pre-filter). Returns the number of errors; `ok`
// receives the guides that passed (error-free files) in path order.
size_t validate_corpus(const std::vector<SourceFile>& files, std::vector<SourceGuide>& ok,
                       std::vector<Diagnostic>& diags, const ValidateOptions& opt = ValidateOptions());

// True if a relative path should be ignored by the packer (a component starts with '_' or '.').
bool path_ignored(std::string_view rel_path);

// Human title for a category id ("medical" -> "Medical"); unknown ids are title-cased.
std::string category_title(std::string_view id);
// Position of a category in the canonical §7 order; unknown categories sort after, by id.
int category_rank(std::string_view id);
bool category_known(std::string_view id);

// Builds a pack. Guides with duplicate ids keep the first occurrence. Never fails for valid input
// (an empty vector yields a valid empty pack).
Bytes build_pack(const std::vector<SourceGuide>& guides);

// Semantic search sections (pack_format.h, "extension sections"). With an empty model the pack is
// keyword-only and byte-identical to build_pack(guides).
struct SemanticBuildOptions {
  ByteView model;                  // an EMBMODL1 file (models/), copied into the pack verbatim
  uint32_t vector_bits = 4;        // stored passage/guide vectors: 4, 8 or 32 (float, measurements)
  PassageOptions passages;
  bool title_in_passages = true;   // embed each passage together with its guide's title
};
struct SemanticBuildStats {
  uint32_t passages = 0;
  uint64_t tokens = 0;
  size_t model_bytes = 0, passage_bytes = 0, vector_bytes = 0, guide_vector_bytes = 0;
};
// Returns an empty Bytes and sets *error if the model cannot be loaded.
Bytes build_pack(const std::vector<SourceGuide>& guides, const SemanticBuildOptions& sem,
                 SemanticBuildStats* stats = nullptr, std::string* error = nullptr);

// Test helper: recomputes every section checksum and the header checksum in place so that fuzz
// tests can exercise the structural validation behind the checksums. Returns false if the
// header/section table itself is out of range.
bool repair_pack_checksums(Bytes& pack);

}  // namespace ember::guides
