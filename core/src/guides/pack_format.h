// EmberMesh guides — the `guides.pack` binary format (version 1). Written by pack_builder.cpp
// (tools/guidepack), read zero-copy by library.cpp. Also documented in tools/guidepack/README.md.
//
// Design goals: load = validate a ~200 byte header + checksum the small metadata/index sections,
// then operate directly on the (possibly memory-mapped) bytes. Nothing proportional to the corpus
// is materialised on the heap; a guide body is decompressed only when opened or snippeted.
// Every multi-byte integer is little-endian; no field needs alignment (all reads are bytewise).
// All offsets are u32, so a pack is limited to < 4 GiB (kMaxPackBytes is far lower).
//
//   +---------------------------------------------------------------------------------------+
//   | Header (kHeaderSize = 176 bytes)                                                      |
//   |   0  magic "EMGUIDE1"                      8 bytes                                    |
//   |   8  version (= kPackVersion)              u32                                        |
//   |  12  header_size (= 176)                   u32                                        |
//   |  16  flags (0)                             u32                                        |
//   |  20  doc_count N                           u32                                        |
//   |  24  category_count C                      u32                                        |
//   |  28  term_count T                          u32                                        |
//   |  32  term_block_size B (= 32)              u32                                        |
//   |  36  section_count (= kSectionCount = 8)   u32                                        |
//   |  40  field_total_tokens[4]                 u64 x4  (title, tags, summary, body)       |
//   |  72  sections[8] {offset u32, length u32, crc32 u32}                                  |
//   | 168  extension directory offset (0 = none) u32  (optional semantic sections, below)    |
//   | 172  header_crc = CRC-32 of bytes 0..171   u32                                        |
//   +---------------------------------------------------------------------------------------+
//   | Sections, in this order, each inside [header_size, file size):                        |
//   |  0 STRINGS    UTF-8 string pool (deduplicated). A StrRef is {offset u32, length u32}. |
//   |  1 CATEGORIES C x 24 bytes: id StrRef, title StrRef, first_doc u32, doc_count u32.    |
//   |               Docs are stored grouped by category, so a category is a doc range.      |
//   |  2 DOCS       N x kDocRecordSize (128) bytes, in list order (category, book/title,    |
//   |               chapter order, title, id). See kDoc* offsets below.                     |
//   |  3 IDINDEX    N x u32 doc numbers sorted by id (bytewise) for get(id).                |
//   |  4 TERMBLOCKS ceil(T/B) x 8 bytes: {offset into TERMS u32, offset into POSTINGS u32}  |
//   |  5 TERMS      front-coded sorted term dictionary, B terms per block. First entry of a |
//   |               block: varint len, bytes, varint df, varint postings_len. Following:    |
//   |               varint shared_prefix, varint suffix_len, suffix bytes, varint df,       |
//   |               varint postings_len. A term's postings start at the block's postings    |
//   |               offset plus the postings_len of the terms before it in the block.       |
//   |  6 POSTINGS   per term, df entries in increasing doc order: varint doc_gap (doc -     |
//   |               previous doc, first entry = doc), u8 field mask (bit0 title, bit1 tags, |
//   |               bit2 summary, bit3 body), then one varint term frequency per set bit.   |
//   |  7 BODIES     concatenated raw-DEFLATE Markdown bodies (front matter removed).        |
//   +---------------------------------------------------------------------------------------+
//
// Checksums: header_crc covers the header; sections 0-6 are CRC-checked at load (they are small:
// metadata + index); BODIES has a section CRC that is only checked by GuideLibrary::verify(), and
// each body additionally carries the CRC-32 of its *uncompressed* text, checked whenever it is
// opened. Independently of checksums, every offset/length/varint is bounds-checked on use, so a
// hostile pack with valid checksums can at worst return wrong text, never over-read.
//
// Hostile-pack limits enforced by load() (security review H6; see the kMax* constants): every doc
// owns a disjoint BODIES range; uncompressed size <= kMaxBodyExpansion x compressed +
// kBodyExpansionSlack and compressed <= 9/8 uncompressed + 1 KiB; metadata strings <=
// kMaxMetaBytes; IDINDEX is a permutation sorted by strictly increasing id; <= kMaxDocs docs. The
// inflater refuses streams with more than size/1024 + 16 blocks. Search additionally works under
// per-query budgets (search.cpp), so no pack contents can make one query take seconds.
//
// Varints (security review L9): TERMS/POSTINGS varints are read with ember::ByteReader, which
// accepts non-canonical (overlong, e.g. 0x80 0x00 for 0) LEB128 up to 10 bytes. This is
// deliberately TOLERATED here: a varint in a pack is only ever a count, length or doc gap that is
// range-checked on use; packs are never hashed, signed or deduplicated by their encoding (the
// checksums cover the bytes as they are), so two encodings of one value cannot be confused for two
// different packs or smuggle a different value. The builder always writes canonical varints.
//
// Terms are produced by text.h (tokenize -> stop words -> stem); the index is only meaningful
// to a reader using the same tokenizer version, which is part of kPackVersion.
#pragma once

#include <cstddef>
#include <cstdint>

namespace ember::guides {

constexpr char kPackMagic[8] = {'E', 'M', 'G', 'U', 'I', 'D', 'E', '1'};
constexpr uint32_t kPackVersion = 1;
constexpr size_t kHeaderSize = 176;
constexpr uint32_t kSectionCount = 8;
constexpr uint32_t kTermBlockSize = 32;
constexpr size_t kMaxPackBytes = 1u << 30;           // 1 GiB, refused beyond
constexpr uint32_t kMaxGuideBytes = 8u << 20;        // per uncompressed body
constexpr uint32_t kMaxTermBytes = 64;
// Load-time limits for hostile packs (security review H6). Packs from tools/guidepack are far
// inside them (measured on the 864-guide corpus: 864 docs, max ratio 5.8:1, max string 792 B).
constexpr uint32_t kMaxDocs = 1u << 20;              // docs per pack
constexpr uint32_t kMaxMetaBytes = 8192;             // any metadata string (id, title, tags...)
constexpr uint32_t kMaxBodyExpansion = 64;           // uncompressed <= 64 x compressed + slack
constexpr uint32_t kBodyExpansionSlack = 4096;
constexpr int kFieldCount = 4;                       // title, tags, summary, body

enum Section : uint32_t {
  kSecStrings = 0,
  kSecCategories = 1,
  kSecDocs = 2,
  kSecIdIndex = 3,
  kSecTermBlocks = 4,
  kSecTerms = 5,
  kSecPostings = 6,
  kSecBodies = 7,
};

// Header field offsets.
constexpr size_t kHdrVersion = 8;
constexpr size_t kHdrHeaderSize = 12;
constexpr size_t kHdrFlags = 16;
constexpr size_t kHdrDocCount = 20;
constexpr size_t kHdrCategoryCount = 24;
constexpr size_t kHdrTermCount = 28;
constexpr size_t kHdrTermBlockSize = 32;
constexpr size_t kHdrSectionCount = 36;
constexpr size_t kHdrFieldTotals = 40;
constexpr size_t kHdrSections = 72;
constexpr size_t kHdrCrc = 172;

constexpr size_t kCategoryRecordSize = 24;

// DOCS record layout (128 bytes). StrRefs are 8 bytes {offset, length} into STRINGS.
constexpr size_t kDocRecordSize = 128;
constexpr size_t kDocId = 0;            // StrRef
constexpr size_t kDocTitle = 8;         // StrRef
constexpr size_t kDocSummary = 16;      // StrRef
constexpr size_t kDocTags = 24;         // StrRef: tags joined with '\n'
constexpr size_t kDocSource = 32;       // StrRef
constexpr size_t kDocSourceTitle = 40;  // StrRef
constexpr size_t kDocLicense = 48;      // StrRef
constexpr size_t kDocAttribution = 56;  // StrRef
constexpr size_t kDocPartOf = 64;       // StrRef (length 0 = none)
constexpr size_t kDocCategory = 72;     // u32 category index
constexpr size_t kDocFlags = 76;        // u32 bit0: has order
constexpr size_t kDocOrder = 80;        // i32 chapter order
constexpr size_t kDocBytes = 84;        // u32 uncompressed Markdown size
constexpr size_t kDocBodyOff = 88;      // u32 offset into BODIES
constexpr size_t kDocBodyLen = 92;      // u32 compressed length
constexpr size_t kDocBodyCrc = 96;      // u32 CRC-32 of uncompressed Markdown
constexpr size_t kDocPrev = 100;        // u32 previous chapter doc number or kNone
constexpr size_t kDocNext = 104;        // u32 next chapter doc number or kNone
constexpr size_t kDocFieldLens = 108;   // u32 x4 token counts (title, tags, summary, body)
// 124..127 reserved (0)
constexpr uint32_t kNone = 0xFFFFFFFFu;
constexpr uint32_t kDocFlagHasOrder = 1;

// ---------------------------------------------------------------------------------------------
// Optional extension sections (semantic search). Additive: the pack stays version 1 and readers
// that predate them ignore them.
//
// Header bytes 168..171, "reserved" for older readers (which never read them), hold the offset of
// an extension directory (0 = none); the header CRC covers it. The directory and the sections it
// lists live after BODIES:
//
//   directory  "EMGEXT01", u32 count (<= kMaxExtSections), u32 CRC-32 of the entries, then
//              count x {u32 tag, u32 version, u32 offset, u32 length, u32 crc32}
//   SEMM v1    the static embedding model, verbatim (guides/embed_format.h)
//   SEMP v1    passages: u32 passage_count P, u32 doc_count N (== header doc_count), u32 flags
//              (bit0: the first passage of every guide is its metadata: title, tags, summary),
//              u32 reserved; (N+1) x u32 first passage of each doc (ascending, last == P);
//              P x 12-byte records {u32 body offset, u16 length, u16 heading length,
//              u32 heading body offset}; a metadata passage has length 0.
//   SEMV v1    passage vectors: u32 count (== P), u32 dims (== model dims), u32 bits (4, 8, 32),
//              u32 norm_bytes (2, 4, 4); count x dims*bits/8 bytes of codes (embed.h
//              encode_vector), then count x norm_bytes norms.
//   SEMG v1    guide vectors, same layout with count == N: the sum of a guide's passage vectors.
//
// A reader treats every extension as optional: any error in it (bad CRC on the small parts,
// impossible sizes, wrong version) disables semantic search with a reason and the library loads
// keyword-only; it never fails the load. The directory, SEMP and the model's non-table blocks are
// CRC-checked at load; the model table, SEMV and SEMG are only CRC-checked by verify() (like
// BODIES: any bytes there are structurally valid and can only change scores).
constexpr size_t kHdrExtDir = 168;
constexpr char kExtMagic[8] = {'E', 'M', 'G', 'E', 'X', 'T', '0', '1'};
constexpr uint32_t kMaxExtSections = 16;
constexpr uint32_t kExtEntrySize = 20;
constexpr uint32_t fourcc(char a, char b, char c, char d) {
  return static_cast<uint32_t>(static_cast<uint8_t>(a)) | static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8 |
         static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16 | static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24;
}
constexpr uint32_t kTagSemModel = fourcc('S', 'E', 'M', 'M');
constexpr uint32_t kTagSemPassages = fourcc('S', 'E', 'M', 'P');
constexpr uint32_t kTagSemVectors = fourcc('S', 'E', 'M', 'V');
constexpr uint32_t kTagSemGuides = fourcc('S', 'E', 'M', 'G');
constexpr uint32_t kSemVersion = 1;
constexpr uint32_t kSemPassageHeader = 16;
constexpr uint32_t kSemPassageRecord = 12;
constexpr uint32_t kSemVectorHeader = 16;
constexpr uint32_t kSemFlagMetaPassage = 1;
// Hostile-pack bound: the semantic scan of one query touches every passage once, so the passage
// count bounds query time (2^20 x 128 bytes ~ 100 ms on a desktop; the real corpus has ~1/8 of it).
constexpr uint32_t kMaxPassages = 1u << 20;

}  // namespace ember::guides
