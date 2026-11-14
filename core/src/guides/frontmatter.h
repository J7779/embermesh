// EmberMesh guides — parser for the YAML-style front matter of guide Markdown files (ARCHITECTURE §7).
//
// Supported subset (everything the corpus format needs, nothing more):
//   - optional UTF-8 BOM, LF or CRLF line endings
//   - opening line "---", closing line "---" (or "...")
//   - `key: value` with key [A-Za-z0-9_-]+; plain, "double-quoted" (with \" \\ \n \t \uXXXX
//     escapes) or 'single-quoted' ('' escape) scalars; ` #` starts a comment in plain scalars
//   - flow lists `[a, "b, c", d]` and block lists (following lines `  - item`)
//   - plain scalars continued on following indented lines (folded with a space)
//   - `>` / `|` block scalars (indented lines; folded / literal)
// Errors are reported with 1-based line numbers; parsing continues past most errors so that a
// validator can report everything at once.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ember::guides {

struct FmValue {
  std::string key;
  std::string scalar;              // scalar value (for lists: empty)
  std::vector<std::string> list;   // list items
  bool is_list = false;
  int line = 0;                    // line of the key
};

struct FmDiag {
  int line = 0;
  std::string message;
};

struct FrontMatter {
  bool present = false;            // file starts with a "---" line
  bool terminated = false;         // closing "---" found
  std::vector<FmValue> values;     // in file order (duplicates are reported and the first kept)
  size_t body_offset = 0;          // byte offset of the body in the original text
  int body_line = 1;               // 1-based line number of the first body line
  std::vector<FmDiag> errors;

  const FmValue* find(std::string_view key) const;
  // Scalar value of key or "" (lists are joined with ", ").
  std::string get(std::string_view key) const;
};

FrontMatter parse_front_matter(std::string_view text);

}  // namespace ember::guides
