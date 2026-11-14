# core/src/guides — the offline guide library

Loads `guides.pack` zero-copy and serves the `guides.*` RPC methods (`docs/RPC_API.md`): listing,
reading, keyword search (BM25 with re-ranking), and — when the pack carries the optional semantic
sections — meaning-based search, related guides and extractive answers (§7).

| File | Role |
|---|---|
| `pack_format.h` | the pack layout, including the optional extension sections |
| `library.{h,cpp}` | loading/validation, metadata, bodies, `info()` |
| `search.cpp` | keyword ranking and the hybrid (keyword + meaning) ranking |
| `semantic.{h,cpp}` | the semantic sections of a loaded pack; integer vector scoring |
| `semantic_search.cpp` | `related()` and `ask()` |
| `embed_format.h`, `embed.{h,cpp}` | the static embedding model: WordPiece tokenizer, text embedding, vector quantisation |
| `passages.{h,cpp}` | splitting guide bodies into passages (packer) |
| `pack_builder.{h,cpp}` | corpus validation and pack building (packer) |
| `text.{h,cpp}`, `deflate.{h,cpp}`, `frontmatter.{h,cpp}` | keyword tokenizer/stemmer, DEFLATE, front matter |

## Smart search in plain words (for the manual)

> **How smart search works and what it is not.** The guide library finds guides by what you mean,
> not only by the exact words you type: "stuff to stop infection in a cut" finds the wound-care
> guides even though they never use the word "stuff". It does this with a small table of word
> meanings that is part of the guide library on your device; nothing is sent anywhere and it works
> with no network at all. It does not write answers and it does not give advice of its own: it
> shows you passages copied from the guides, with the guide they came from, so you can read the
> original. If nothing in the library matches what you asked, it says so instead of guessing. Exact
> names, part numbers and codes ("UV-5R", "FM 21-76") are still matched word for word first.

## Semantic search

### The model

A static token-embedding model (`models/README.md`): **potion-base-4M** (MinishLab, MIT licence),
a Model2Vec distillation of `bge-base-en-v1.5` — one 128-dimensional vector per WordPiece token,
29 528 tokens. A text's vector is the sum of its tokens' vectors; similarity is the cosine. No
neural-network runtime: inference is tokenize, look up rows, add (about 3 µs per query).

* **Tokenizer** (`embed.cpp`): BERT uncased WordPiece, reproduced token for token from tables that
  `tools/embedding/convert.py` derives by running the reference tokenizer on every code point
  (normalisation runs, character classes, Hangul decomposition, literal special tokens,
  100-character word limit, `[UNK]` dropped as Model2Vec does). Proven by
  `core/tests/data/guides/tokenizer_parity.tsv` (470 sentences: corpus samples plus Unicode edge
  cases) in `embed_test.cpp`.
* **Quantisation**: 4 bits per weight (values -7..7), one u16 scale per token row (MSE-optimal
  clipping). Rows are added as exact integers, so the packer and every app compute bit-identical
  vectors. Stored passage and guide vectors are 4-bit too (symmetric, rounded half away from zero,
  with their sum of squares).

### What the pack stores (`pack_format.h`, "extension sections")

Old readers ignore them; new readers treat them as optional.

| Section | Content | Size (full corpus) |
|---|---|---|
| SEMM | the model file, verbatim | 2.35 MB |
| SEMP | 141 307 passages: per guide a range; per passage {body offset, length, heading} (12 bytes) — the text itself is read back from the compressed body | 1.72 MB |
| SEMV | one 4-bit 128-dim vector per passage + norm (66 bytes) | 9.33 MB |
| SEMG | one vector per guide (sum of its passage vectors), for `related` | 0.36 MB |

Total growth: **13.8 MB** (46.9 MB → 60.7 MB). Passages (`passages.h`): split at headings and
blank-line blocks, packed up to 900 bytes, long blocks cut at sentence boundaries with a
one-sentence overlap, short section tails merged, boilerplate skipped (Sources/References/…
sections, `_(…)_` editorial notes, "Source:"/"Licence:" lines). Every guide also gets a metadata
passage (title, tags, summary); body passages are embedded together with the guide title and
their heading.

**Hostile packs.** Directory, passage table and the model's non-table blocks are CRC-checked at
load; every count, offset and range is validated (passages must lie inside their guide's body,
vector sections must have exactly the declared size, at most 2^20 passages, model limits in
`embed_format.h`). The model table and vectors are CRC-checked by `verify()` only — any bytes there
are structurally valid. Any failure disables semantic search with a reason (`info().semantic.status`)
and the library loads keyword-only; it never fails the load. Per-query work is bounded: one pass
over at most 2^20 passages, a 16 MiB body-decompression budget shared by snippets and passages,
at most 8 192 tokens per text.

### Ranking (`search.cpp`, `hybrid()`)

1. Keyword ranking as before (BM25 per field, synonyms, typo tolerance, re-rank of the top 30).
   kn = keyword score / best keyword score (0..1).
2. Meaning: the query vector is scored against every passage (integer dot products, ~1.3 ms for
   141 k passages natively); a guide's meaning score = 0.7 × its best passage + 0.3 × its guide
   vector; sn rescales it so the best guide is 1 and the 100th is 0.
3. **final = kn + λ · sn**, where λ goes from 0.12 (strong keyword match) to 1.6 (weak). Strength =
   the share of query words the best keyword guide has in its own title/tags/summary × the share of
   query words found exactly in the index (not only as typo/prefix guesses).

So when the words match a guide's metadata (names, part numbers, exact topics), meaning can only
reorder guides whose keyword scores are within 0.12 of each other: **exact-term queries never get
worse** (guarded by `semantic_test.cpp` and by `guidepack eval`'s exact-term guard: 0 of 41
exact queries rank worse). When the words are weak (lay wording, paraphrase), meaning decides.
`match` is `words` (kn ≥ 0.3), `meaning` (top 20 by meaning or sn ≥ 0.6), or `both`. `passage` is the
guide's best body passage (≤ 600 bytes of plain text).

`ask()` scores all passages, keeps the best 400 (≤ 2 per guide), adds 0.4 × the guide's keyword
score, and returns the top passages quoted from the guides. `related()` compares guide vectors
(at most 2 chapters of the same book).

**Scores.** Reported similarities are calibrated cosines in 0..1: questions/passages map cosine
0.40 → 0 and 0.80 → 1; guide-to-guide (`related`) maps 0.60 → 0 and 0.97 → 1 (whole-guide vectors
are closer to each other). `weak` is set when the best score is below **0.30**: on the query set
6 % of real questions are weak (mostly misspellings the model cannot read), and 8 of 10 off-topic
questions ("celebrity gossip", "pizza toppings") are. A UI should use `weak` (or 0.30) rather than
a lower cut-off.

### Measured quality (`guidepack eval`, 195 queries, see tools/guidepack/README.md)

MRR@20 / recall@10 (share of queries with a relevant guide in the top 10):

| Query type (n) | keyword | meaning only | hybrid |
|---|---|---|---|
| all (195) | 0.759 / 0.862 | 0.680 / 0.815 | **0.801 / 0.908** |
| held-out test split (65) | 0.793 / 0.908 | 0.694 / 0.831 | **0.852 / 0.954** |
| lay wording (63) | 0.614 / 0.778 | 0.624 / 0.762 | **0.708 / 0.857** |
| paraphrase (33) | 0.755 / 0.879 | 0.732 / 0.909 | **0.776 / 0.939** |
| question (21) | 0.746 / 0.905 | 0.731 / 0.857 | **0.853 / 0.952** |
| typo (21) | **0.929** / 0.952 | 0.621 / 0.762 | 0.889 / **1.000** |
| synonym (16) | **0.542** / 0.625 | 0.406 / 0.500 | 0.520 / 0.625 |
| exact term (41) | 0.988 / 1.000 | 0.833 / 0.951 | **1.000 / 1.000** |

`ask`: a relevant guide's passage first in 73 % of queries, in the top 5 in 83 %.

Quantisation (meaning-only MRR / hybrid MRR; weights / stored vectors): float/float 0.693 / 0.805;
8/8 0.697 / 0.802; 4/8 0.695 / 0.805; 8/4 0.678 / 0.801; **4/4 0.680 / 0.801** (shipped).
4-bit weights cost nothing; 4-bit passage vectors cost 0.015 meaning-only MRR and ≤ 0.004 hybrid
MRR, for a 9.3 MB instead of 18.7 MB vector section. Sign-bit (1-bit) codes alone lose 0.14
meaning-only MRR; 1-bit with 4-bit rescoring of the top 2 000 matches 4-bit but needs both stores,
and the 4-bit scan is already fast, so only 4-bit vectors are stored.

**Limits.** Meaning helps most for everyday wording and questions; it slightly hurts misspelt
words (the WordPiece pieces of a misspelling carry little meaning; the keyword typo tolerance
does better) and does not fix synonyms the model does not know ("tummy bug", "loo"). The model
is English-only. It ranks guides and passages; it does not generate, summarise or verify text.

### Performance

Measured with `guides_bench` (tools/guidepack/README.md) over the 195 queries on a current
desktop ARM64 CPU, single thread:

| | native | WebAssembly (node) | WebAssembly + `-msimd128` |
|---|---|---|---|
| load (zero-copy) | 13.6 ms (11.7 keyword-only) | 16.9 ms | — |
| heap held after load | < 1 KiB | < 1 KiB | — |
| keyword search, mean | 6.6 ms | 7.8 ms | 7.9 ms |
| hybrid search, mean / p90 | 9.5 / 12.4 ms | 14.6 / 17.8 ms | 11.7 / 15.0 ms |
| ask, mean | 13.8 ms | 20.7 ms | 17.0 ms |
| scan of all 141 k passages | 1.3 ms | 5.4 ms | 2.2 ms |

Peak heap per hybrid query: 1.5 MB (one float per passage plus per-guide arrays). Phone estimate
(not measured on a phone): mid-range phone cores (Cortex-A76 to A78 class) reach about 1/2.5 to
1/4 of this desktop core's single-thread benchmark scores, so a native hybrid search should take
about 25–40 ms, of which the semantic part (hybrid minus keyword, ~3 ms here) is about 8–12 ms —
the rest is the keyword pipeline that existed before; in a phone browser, with `-msimd128`, about
30–50 ms. The semantic addition fits the ~30 ms budget; the total is at or slightly above it on
the slowest of those phones. The pack build grows by about 3 seconds (all 20 M tokens are embedded
by the same C++ code the apps run).
