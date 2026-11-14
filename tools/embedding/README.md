# tools/embedding — converting the static embedding model

One-time, offline tools that turn an openly licensed Model2Vec static embedding model into the
compact file the core reads (`models/<name>/*.emodel`, format in
`core/src/guides/embed_format.h`). **Builds never run these**: the converted model is committed,
and `guidepack` (C++) is all a build needs. Python and the network are only needed to regenerate
the model or the tokenizer parity fixture.

| File | What it does |
|---|---|
| `models.json` | pinned upstream sources: repository, commit, SHA-256 of every file used, licence |
| `convert.py` | downloads (or reuses) the pinned files, verifies SHA-256, probes the reference tokenizer for every Unicode code point, quantises the token table, writes the `.emodel` file |
| `make_tokenizer_fixture.py` | writes `core/tests/data/guides/tokenizer_parity.tsv`: sentences from the corpus plus Unicode edge cases with the token ids the reference tokenizer produces |

## Reproducing the committed model

Library versions used (pin them; newer versions should give the same bytes, but these are the
ones the committed file was made with): Python 3.14, numpy 2.5.4, safetensors 0.8.0,
tokenizers 0.23.3.

```sh
python3 -m venv .toolchains/pyvenv-embedding
.toolchains/pyvenv-embedding/bin/pip install numpy==2.5.4 safetensors==0.8.0 tokenizers==0.23.3
.toolchains/pyvenv-embedding/bin/python -I tools/embedding/convert.py \
    --model potion-base-4M --bits 4 \
    --out models/potion-base-4M/potion-base-4M-q4.emodel
# downloads into .toolchains/embedding-downloads/potion-base-4M/ (git-ignored), verifies
# SHA-256, prints the output's SHA-256 (compare with models/README.md)
.toolchains/pyvenv-embedding/bin/python -I tools/embedding/make_tokenizer_fixture.py \
    --tokenizer .toolchains/embedding-downloads/potion-base-4M/tokenizer.json \
    --content guides/content --out core/tests/data/guides/tokenizer_parity.tsv
```

The output is deterministic (same inputs and options, same bytes). Downloaded files are data:
the scripts parse them (JSON, safetensors) and never execute anything from them; run Python with
`-I` so nothing from the download directories is imported.

Options worth knowing for measurements: `--bits 8` / `--bits 32` (8-bit or float table, to see
what 4 bits cost), `--dims N` (keep the first N dimensions; the potion models are PCA-ordered),
`--no-clip-search` (scale each row by its maximum instead of the MSE-optimal clipping).

## How the conversion works

* **Tokenizer tables.** The model's tokenizer is BERT's uncased WordPiece (BertNormalizer with
  lower-casing and accent stripping, BertPreTokenizer, `##` continuation pieces, 100-character
  word limit). Instead of re-implementing Unicode normalisation, `convert.py` runs the reference
  normaliser and pre-tokenizer on every code point and stores the result as run tables (2 932
  normalisation runs, 327 character-class runs, 5 KiB of multi-character outputs). The C++
  tokenizer (`core/src/guides/embed.cpp`) interprets them; Hangul syllables are decomposed
  algorithmically. Parity is proven token for token by the fixture test.
* **Quantisation.** Each token row is quantised symmetrically to 4 bits (values -7..7) with one
  u16 scale per row (a common factor is dropped: cosine similarity does not need it). The scale is
  chosen per row to minimise squared error (a search over clipping factors 0.5..1.0). Rows are
  added as exact integers at run time, so every platform computes identical vectors.
