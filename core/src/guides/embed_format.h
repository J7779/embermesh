// EmberMesh guides — the static embedding model file (EMBMODL1). Written once, offline, by
// tools/embedding/convert.py (committed under models/); copied verbatim into guides.pack as the
// SEMM section by tools/guidepack; read zero-copy by guides/embed.cpp.
//
// A "static" model is one vector per WordPiece token. The vector of a text is the sum (cosine
// makes it equivalent to the mean) of the vectors of its tokens, so inference needs no neural
// network runtime: tokenize, look up rows, add. Every multi-byte integer is little-endian and is
// read bytewise; blocks start on 4-byte boundaries (relative to the file start).
//
//   +--------------------------------------------------------------------------------------+
//   | Header (kModelHeaderSize = 96 bytes)                                                 |
//   |   0 magic "EMBMODL1"   8 version (1)   12 header_size (96)                           |
//   |  16 vocab_size V (<= 65535)   20 dims D (multiple of 8, <= 1024)                     |
//   |  24 bits per weight (4, 8 or 32 = float, the last only for measurements)             |
//   |  28 unk_id   32 max_word_chars (WordPiece: longer words are [UNK])   36 flags (1)    |
//   |  40 norm_runs R   44 norm_str_bytes   48 class_runs C   52 vocab_bytes               |
//   |  56 name_len   60 special_count S   64 total_size   68 body_crc (CRC-32 of the       |
//   |     blocks between the header and the table)   72 table_offset   76 table_crc        |
//   |  80..91 reserved (0)   92 header_crc = CRC-32 of bytes 0..91                         |
//   +--------------------------------------------------------------------------------------+
//   | name            name_len bytes UTF-8 ("potion-base-8M@<commit>"), padded to 4        |
//   | specials        S x u32 token ids matched literally before normalisation ([CLS]...)  |
//   | norm runs       R x {u32 first code point, u32 kind << 24 | payload}, ascending;      |
//   |                 a run covers [first, next first). Kinds: SAME, DROP (removed), SPACE |
//   |                 (-> ' '), PAD (-> ' ' c ' ', CJK), DELTA (-> cp + signed 24-bit      |
//   |                 payload), ALT (even offsets from first -> DELTA, odd -> SAME), STR   |
//   |                 (one code point -> norm_strs[payload >> 8, + (payload & 255)]),      |
//   |                 HANGUL (canonical syllable decomposition)                            |
//   | norm strs       norm_str_bytes UTF-8, padded to 4                                    |
//   | class runs      C x {u32 first code point, u32 class}: 0 word, 1 punctuation (split  |
//   |                 off as its own word), 2 whitespace (separates words)                 |
//   | vocab offsets   (V+1) x u32 into the vocab blob (token i = blob[off[i], off[i+1]))   |
//   | vocab blob      vocab_bytes UTF-8 (continuation pieces carry the "##" prefix), pad 4 |
//   | sorted          V x u16 token ids in bytewise order of their strings, padded to 4    |
//   | scales          V x u16 per-row scale (bits 4 and 8 only), padded to 4               |
//   | table           at table_offset: V rows of D weights. 4 bits: two per byte, low      |
//   |                 nibble first, value = nibble - 8 in [-7, 7]; 8 bits: int8 in         |
//   |                 [-127, 127]; weight = value * scale[row] (common factor dropped).    |
//   |                 32 bits: float32.                                                    |
//   +--------------------------------------------------------------------------------------+
//
// The norm/class tables are the reference tokenizer's behaviour (BertNormalizer with lower-
// casing and accent stripping, BertPreTokenizer) sampled for every code point by convert.py, so
// the C++ tokenizer needs no Unicode database. Validation (embed.cpp) checks every count,
// offset and table before use; the table CRC is checked by verify() only (like guide bodies:
// any table bytes are structurally valid, a flipped bit can only change a score).
#pragma once

#include <cstddef>
#include <cstdint>

namespace ember::guides {

constexpr char kModelMagic[8] = {'E', 'M', 'B', 'M', 'O', 'D', 'L', '1'};
constexpr uint32_t kModelVersion = 1;
constexpr size_t kModelHeaderSize = 96;
constexpr uint32_t kModelMaxVocab = 65535;
constexpr uint32_t kModelMaxDims = 1024;
constexpr uint32_t kModelMaxWordChars = 200;
constexpr uint32_t kModelMaxNormRuns = 1u << 16;
constexpr uint32_t kModelMaxClassRuns = 1u << 16;
constexpr uint32_t kModelMaxSpecials = 64;
constexpr uint32_t kModelMaxTokenBytes = 256;

enum NormKind : uint32_t {
  kNormSame = 0,
  kNormDrop = 1,
  kNormSpace = 2,
  kNormPad = 3,
  kNormDelta = 4,
  kNormAlt = 5,
  kNormStr = 6,
  kNormHangul = 7,
};
enum CharClass : uint32_t { kClsWord = 0, kClsPunct = 1, kClsSpace = 2 };

// Header field offsets.
constexpr size_t kMhVersion = 8, kMhHeaderSize = 12, kMhVocab = 16, kMhDims = 20, kMhBits = 24, kMhUnk = 28,
                 kMhMaxWordChars = 32, kMhFlags = 36, kMhNormRuns = 40, kMhNormStrBytes = 44, kMhClassRuns = 48,
                 kMhVocabBytes = 52, kMhNameLen = 56, kMhSpecials = 60, kMhTotal = 64, kMhBodyCrc = 68,
                 kMhTableOffset = 72, kMhTableCrc = 76, kMhHeaderCrc = 92;

}  // namespace ember::guides
