// EmberMesh guides — splitting a guide's Markdown body into passages for semantic search.
//
// A passage is a byte range of the body (so the pack stores no extra text: a passage is read
// back by decompressing its guide). Rules:
//   * a heading starts a new section; passages never cross sections, and each passage records the
//     nearest heading above it (level 2+; the level-1 heading is the guide title);
//   * blocks (paragraphs, lists, tables, code blocks: runs of non-blank lines) are packed into a
//     passage while it stays within max_bytes; a block longer than max_bytes is cut at sentence
//     (else line, else word) boundaries, and each cut piece starts with the last sentence of the
//     previous piece when that sentence is short (overlap_bytes), so a statement split across
//     the cut is still found whole;
//   * a section's short tail (< min_bytes) joins the passage before it when that fits;
//   * boilerplate is skipped: sections titled Sources / References / Bibliography / External links
//     / See also / Further reading / Notes / Licence / Attribution / Credits / Footnotes, editorial
//     notes of the form "_( ... )_", and short "Source:" / "Licence:" / "Attribution:" lines;
//   * passages with fewer than 24 bytes of text are dropped.
// Deterministic; pure function of the input.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace ember::guides {

struct PassageSpan {
  uint32_t off = 0, len = 0;            // body byte range
  uint32_t head_off = 0, head_len = 0;  // heading text (without '#'), len 0 = none
};

struct PassageOptions {
  size_t max_bytes = 900;
  size_t min_bytes = 200;
  size_t overlap_bytes = 200;
};

std::vector<PassageSpan> split_passages(std::string_view md, const PassageOptions& opt = PassageOptions());

}  // namespace ember::guides
