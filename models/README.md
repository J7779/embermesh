# models — the static embedding model for semantic guide search

`potion-base-4M/potion-base-4M-q4.emodel` (2 347 848 bytes) is the only model. `guidepack` packs it
into `guides.pack` (section SEMM) so every app carries it; nothing reads it at run time from this
directory.

## What it is

A *static* text-embedding model: one 128-dimensional vector per WordPiece token (29 528 tokens).
The vector of a sentence is the sum of its token vectors, compared by cosine similarity. There is
no neural network to run, so it works in the dependency-free C++ core, in WebAssembly and on old
phones in microseconds per query. It is **potion-base-4M** by MinishLab, a Model2Vec model
distilled from the sentence-transformer `BAAI/bge-base-en-v1.5` and then trained with Tokenlearn;
the weighting of frequent words is built into the vectors.

Our file is a conversion, not the upstream file: the token table is quantised to 4 bits per weight
(one u16 scale per token), and the tokenizer is stored as compact tables derived from the
upstream `tokenizer.json` (format: `core/src/guides/embed_format.h`).

| | |
|---|---|
| Upstream | https://huggingface.co/minishlab/potion-base-4M |
| Revision | commit `9b3cff412d30be9ae8603fe10224c224f3401869` |
| Licence | MIT (model card metadata `license: mit` at that commit; Hugging Face API `cardData.license = "mit"`, tag `license:mit`). Text: `potion-base-4M/LICENSE` |
| Teacher model | `BAAI/bge-base-en-v1.5`, MIT (Hugging Face model card) |
| Converted by | `tools/embedding/convert.py --model potion-base-4M --bits 4` (deterministic) |
| SHA-256 of this file | `6c28a8ee5e2a0ad13540d140e2053c7ded08dfd4f586e33b3abd839e0ddff87d` |

SHA-256 of the upstream files used (also pinned in `tools/embedding/models.json`):

| File | SHA-256 |
|---|---|
| `model.safetensors` | `8a7140edd17ffab30ddcff1135eda127df57abfae856203dbd7b3d061295c31a` |
| `tokenizer.json` | `e67e803f624fb4d67dea1c730d06e1067e1b14d830e2c2202569e3ef0f70bb50` |
| `config.json` | `90a28f209637bea10a440b69c1b41b72a24c4cb7d378030172f89c671c254c80` |
| `README.md` (model card) | `f0d13fe7a9aa2369a71b14eb4e1fa11dfa27498713e1fc600165b4b9cbc53705` |

## Why this model

Measured on our corpus and query set (`tools/guidepack/README.md`, "Search quality"): the 8M
variant (256 dimensions) ranks only slightly better (hybrid MRR 0.810 vs 0.801) but doubles the
passage vectors (+25 MB pack instead of +13.8 MB); the 2M variant (64 dimensions) is clearly worse
(0.776); the retrieval-tuned 32M model, cut to 128 or 256 dimensions, was no better in hybrid
ranking and its larger vocabulary makes the model file 2-4 times bigger. 4-bit weights cost
nothing measurable against float weights.

## Regenerating

See `tools/embedding/README.md`. In short: create `.toolchains/pyvenv-embedding`, install the
pinned libraries, run `convert.py`; the output must have the SHA-256 above. If you change the model,
also regenerate `core/tests/data/guides/tokenizer_parity.tsv`, re-run `guidepack eval` and update
`tools/guidepack/eval/thresholds.txt`.
