# guidepack

Host tool that validates the offline guide corpus (`guides/content/`, format in
`docs/ARCHITECTURE.md` §7) and packs it into `guides.pack`, the file every app bundles and loads
with `ember_guides_load()`. It reuses the shared core code in `core/src/guides/` (the same
tokenizer, stemmer and pack reader the apps run); only the file I/O lives here.

## Usage

```sh
guidepack validate <content_dir> [--no-warnings]       # exit 1 if any error
guidepack build    <content_dir> -o <guides.pack> [--model <file.emodel>] [--vector-bits 4|8|32]
                   [--passage-bytes N] [--strict] [-v]
guidepack stats    <pack>                               # sizes (incl. semantic sections), load + verify timing
guidepack search   <pack> <query...> [--category C] [--limit N] [--semantic|--no-semantic|--meaning-only]
guidepack ask      <pack> <question...> [--category C] [--limit N]   # extractive passages
guidepack related  <pack> <id> [--limit N]
guidepack get      <pack> <id>                          # metadata + Markdown + prev/next
guidepack eval     <pack> --queries <tsv> [--thresholds <file>] [--split tune|test|all] [-v]
```

- `--model` adds the semantic search sections (model, passages, vectors; see "Semantic search"
  below). The CMake target passes the committed model `models/potion-base-4M/potion-base-4M-q4.emodel`
  (cache variable `EMBER_GUIDES_MODEL`; empty = keyword-only pack). Without `--model` the pack is
  byte-identical to what earlier versions produced. `--vector-bits 8|32` and `--passage-bytes`
  exist for measurements.
- `search` defaults to hybrid ranking when the pack has a semantic section; `--no-semantic` is the
  keyword ranking, `--meaning-only` ranks by meaning alone.

- Files and directories whose name starts with `_` (e.g. `_sources.md`) or `.` are ignored.
- `validate` prints `file:line: error|warning: message` diagnostics. Errors: missing required
  front matter (`id title category summary source license attribution`), malformed front matter
  (unterminated block, duplicate keys, bad quotes/lists), `id` != path without `.md`, `category` !=
  directory, licence not in the allow-list (public domain, CC0, CC BY, CC BY-SA, OGL, MIT/BSD/
  Apache, GFDL... and anything NC/ND/"all rights reserved"/unknown is rejected), file > 64 KiB,
  invalid UTF-8 or control characters, raw HTML or images outside code, empty body, non-integer
  `order`, duplicate ids (case-insensitive). Warnings: missing `tags`/`source_title`/`retrieved`,
  file > 60 KB, unknown category or key, `part_of` without `order`, same `part_of`/`order` twice,
  non-Markdown files in the tree.
- `build` leaves files with errors out of the pack (and says so) so one bad file never breaks
  the platform builds; `--strict` makes it fail instead. Output is deterministic: the same input
  gives a byte-identical pack. The pack is self-checked (loaded) before it is written atomically.

## How builds obtain guides.pack

- Native CMake builds (`EMBER_BUILD_TOOLS=ON`, the default on desktop) build target
  `guides_pack` as part of `all`: `<build>/guides/guides.pack` from `EMBER_GUIDES_CONTENT_DIR`
  (default `guides/content`). It reruns when any `.md` under the corpus changes, and succeeds with
  an empty or missing corpus (empty, valid pack). In-tree consumers can read the path from
  `get_property(p GLOBAL PROPERTY EMBER_GUIDES_PACK_FILE)`.
- Cross builds (Android NDK, iOS, Emscripten) do not build host tools. Build the pack on the host
  first, then copy it into the app's assets:

  ```sh
  cmake -S . -B build/host -DEMBER_BUILD_DESKTOP=OFF && cmake --build build/host --target guides_pack
  # -> build/host/guides/guides.pack
  ```

Apps should memory-map (or keep in memory) the asset and call
`ember_guides_load(node, data, len, /*copy=*/0)` for zero-copy loading.

## Search quality (`guidepack eval`) — the CI check

```sh
guidepack eval <build>/guides/guides.pack \
    --queries tools/guidepack/eval/queries.tsv --thresholds tools/guidepack/eval/thresholds.txt
```

This is the one command CI runs after building the pack (also registered as the CTest test
`guides_search_quality`: `ctest -R guides_search_quality`). Exit status 0 = pass, 1 = a threshold
failed, 2 = bad input (for example a relevant guide id that is not in the pack). It prints MRR@20
and recall@1/5/10 (share of queries with a relevant guide in the top k) for keyword, meaning-only
and hybrid ranking, overall, per split and per query type; the hit rate of `ask`; mean latency;
and the exact-term guard (no exact-term query may rank worse with hybrid than with keywords).
`-v` prints every query's ranks.

`eval/queries.tsv` holds 195 queries (130 tuning, 65 held out) written the way stressed people type:
lay wording (63), paraphrases (33), questions (21), typos (21), synonyms (16) and exact terms such
as names, part numbers and document codes (41), each with the guides that answer it (judged by
hand, completed by pooling the top 3 of all three rankers). `eval/thresholds.txt` sets the minimums,
about 0.02–0.03 below the measured values; update both when the model, ranking or query set change.

Measured on the full corpus (5 490 guides) with the committed model:

| | MRR | R@1 | R@5 | R@10 |
|---|---|---|---|---|
| keyword | 0.759 | 0.687 | 0.841 | 0.862 |
| meaning only | 0.680 | 0.600 | 0.774 | 0.815 |
| hybrid | **0.801** | **0.749** | **0.862** | **0.908** |
| hybrid, held-out split | 0.852 | 0.800 | 0.908 | 0.954 |

Per query type, quantisation variants and limits: `core/src/guides/README.md`.

## Performance (`guides_bench`)

```sh
cmake --build <build> --target guides_bench
<build>/tools/guides_bench <build>/guides/guides.pack tools/guidepack/eval/queries.tsv [repeats]
# WebAssembly (Emscripten SDK in .toolchains/emsdk; runs under its node):
em++ -std=c++17 -O3 -msimd128 -fno-exceptions -fno-rtti -Icore/include -Icore/src \
     core/src/guides/*.cpp tools/guidepack/bench.cpp -sALLOW_MEMORY_GROWTH=1 \
     -sINITIAL_MEMORY=134217728 -sNODERAWFS=1 -sENVIRONMENT=node -o guides_bench.js
node guides_bench.js <guides.pack> tools/guidepack/eval/queries.tsv
```

Reports load time and heap held after load, mean/p50/p90 latency per mode, the query embedding
cost and one full passage scan. Results and the phone estimate: `core/src/guides/README.md`.
Building the full pack with the model takes a few seconds longer than without (all ~20 M tokens
are embedded with the core's own integer code; no Python at build time).

## Pack format (version 1)

Authoritative description: `core/src/guides/pack_format.h`. Summary:

| Part | Contents |
|---|---|
| Header, 176 bytes | magic `EMGUIDE1`, version, counts (docs, categories, terms), per-field token totals (for BM25 length normalisation), a table of 8 sections `{offset, length, crc32}`, header CRC-32 |
| `STRINGS` | deduplicated UTF-8 string pool |
| `CATEGORIES` | `{id, title, first_doc, count}`; docs are stored grouped by category in list order |
| `DOCS` | fixed 128-byte record per guide: all GuideMeta fields as string refs, order, body offset/length/CRC, prev/next chapter, per-field token counts |
| `IDINDEX` | doc numbers sorted by id (binary search for `get`) |
| `TERMBLOCKS` + `TERMS` | sorted, front-coded term dictionary in blocks of 32 (binary search on block heads, prefix scans for search-as-you-type and typo tolerance) |
| `POSTINGS` | per term: doc gaps (varint), field mask, per-field term frequency |
| `BODIES` | each guide's Markdown body compressed independently with raw DEFLATE (zlib-compatible) |
| extension directory (optional) | header bytes 168..171 (reserved, never read by older readers) point to `EMGEXT01` + `{tag, version, offset, length, crc32}` entries for the semantic sections below |
| `SEMM` | the static embedding model file, verbatim (`core/src/guides/embed_format.h`) |
| `SEMP` | passages: per guide a passage range, per passage `{body offset, length, heading}` |
| `SEMV` / `SEMG` | 4-bit vectors (+ norms) per passage / per guide |

All integers are little-endian and read bytewise (no alignment). Loading checks the header CRC
and the CRCs of every section except `BODIES`, then validates the structure (every string ref,
category range, body extent, prev/next, id index entry). Bodies are CRC-checked when opened;
`stats` (`GuideLibrary::verify()`) checks everything. Every offset/varint is also bounds-checked
on use, so a hostile pack with valid checksums can at worst return wrong text, never over-read.

Limits for hostile packs (`pack_format.h`; the loader rejects a pack that breaks them, the
validator reports an error for a guide that would): body byte ranges must not overlap; a body may
expand at most 64:1 (+4 KiB) and be at most 1/8 (+1 KiB) larger compressed than uncompressed;
metadata strings (id, title, summary, joined tags, source, licence, attribution...) at most
8192 bytes; the id index must be sorted by strictly increasing id; at most 2^20 guides. The real
corpus is far inside all of them (ratio <= 5.8:1, longest string 792 bytes). Search works under
per-query budgets (re-rank reads at most 128 KiB per body and 16 MiB in total; 4 M posting
entries; 300 000 typo-candidate terms), which real queries never reach.
Changing the tokenizer or stemmer (`core/src/guides/text.cpp`) changes the index and requires a
version bump.

The semantic sections are additive and optional: the pack stays version 1, and readers that
predate them load it unchanged. A reader that knows them still treats them as optional — a missing,
truncated or corrupt semantic section (or an unknown section version) disables semantic search and
the library loads keyword-only. Their hostile-pack limits: at most 2^20 passages (bounds the per-
query scan), every passage inside its guide's body, vector sections exactly the declared size,
model limits in `embed_format.h`; the directory, passage table and model tables are CRC-checked at
load, the model table and vectors by `stats`/`verify()`. `guidepack stats` lists their sizes:

| Section (full corpus) | Bytes |
|---|---|
| model | 2 347 848 |
| passages (141 307) | 1 717 664 |
| passage vectors | 9 326 278 |
| guide vectors | 362 356 |
| total growth | 13 754 146 (46.9 MB → 60.7 MB) |
