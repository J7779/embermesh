// EmberMesh guides — text utilities shared by the pack builder and the search engine:
// UTF-8 validation, Unicode-tolerant tokenizer with case/accent folding, English stop words,
// a light English stemmer (Porter step 1), and Markdown → plain text conversion.
//
// The SAME tokenizer + stemmer runs at index time (guidepack) and at query time (GuideLibrary), so
// any change here changes ranking and requires rebuilding packs (bump kPackVersion if the index
// semantics change incompatibly).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ember::guides {

// Strict UTF-8 validation (no overlongs, no surrogates, <= U+10FFFF). On failure *bad_offset (if
// given) receives the offset of the first invalid byte.
bool utf8_valid(std::string_view s, size_t* bad_offset = nullptr);

// Decodes one code point at s[i]; advances i. Invalid sequences yield U+FFFD and advance one byte.
uint32_t utf8_next(std::string_view s, size_t& i);
void utf8_append(std::string& out, uint32_t cp);
// Largest n' <= n such that s[0..n') does not end in the middle of a UTF-8 sequence.
size_t utf8_floor(std::string_view s, size_t n);

constexpr size_t kMaxTokenBytes = 48;

struct Token {
  std::string text;     // normalized (lowercased, accent-folded, apostrophes removed), unstemmed
  uint32_t begin = 0;   // byte range in the source text
  uint32_t end = 0;
  bool joined = false;  // concatenation of a hyphenated compound ("j-pole" -> "jpole")
};

// Splits text into word tokens. Words are runs of letters/digits (any non-ASCII letter counts);
// apostrophes inside words are dropped ("won't" -> "wont", trailing "'s" removed), digit groups
// keep '.' ("146.52"), hyphenated compounds emit their parts plus the joined form. Single ASCII
// letters are dropped; tokens longer than kMaxTokenBytes are dropped. Stop words are NOT removed
// here (see is_stop_word). `max_tokens` caps the output (0 = unlimited).
void tokenize(std::string_view text, std::vector<Token>& out, size_t max_tokens = 0);

bool is_stop_word(std::string_view normalized);

// Light English stemmer: Porter (1980) step 1a/1b/1c on pure a-z words longer than 3 letters;
// anything else is returned unchanged. "bleeding"->"bleed", "purifies"/"purified"->"purifi".
std::string stem(std::string_view normalized);

// normalized token -> index term (stemmed). Returns "" if the token is a stop word.
std::string index_term(std::string_view normalized);

// Markdown -> plain text for indexing and snippets: drops heading/list/quote markers, table pipes
// and separator rows, code fences, emphasis characters, link targets and bare URLs. Line structure
// is kept ('\n').
std::string markdown_to_plain(std::string_view md);

// Lowercase ASCII copy.
std::string ascii_lower(std::string_view s);
// Simple lowercase for ASCII, Latin-1, Latin Extended-A, Greek and Cyrillic (accents kept).
std::string utf8_lower(std::string_view s);

}  // namespace ember::guides
