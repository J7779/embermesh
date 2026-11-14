#!/usr/bin/env python3
"""Convert a Model2Vec static embedding model into EmberMesh's compact model file (EMBMODL1).

Offline, one-time step (see tools/embedding/README.md). Downloads the pinned upstream files from
the model's official Hugging Face repository (tools/embedding/models.json), verifies their
SHA-256, and writes one self-contained little-endian file that the C++ core reads zero-copy
(format: core/src/guides/embed_format.h):

  * the WordPiece vocabulary (sorted index for binary search, special-token ids),
  * the tokenizer's Unicode behaviour as compact run tables, derived by probing the reference
    tokenizer (`tokenizers` library) for every code point, so the C++ tokenizer reproduces it
    token for token without any Unicode library,
  * the token embedding table, quantised per row (symmetric, one u16 scale per row) to 4 or 8
    bits per weight, or kept as float32 (--bits 32, for measuring what quantisation costs).

Usage:
  convert.py --model potion-base-8M --bits 4 --out models/potion-base-8M/potion-base-8M-q4.emodel
             [--dims N] [--cache DIR] [--no-download]

Deterministic: the same inputs and options always give a byte-identical file.
"""
import argparse
import hashlib
import json
import os
import struct
import sys
import urllib.request
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))

MAGIC = b"EMBMODL1"
VERSION = 1
HEADER_SIZE = 96

# Normalisation run kinds (embed_format.h).
K_SAME, K_DROP, K_SPACE, K_PAD, K_DELTA, K_ALT, K_STR, K_HANGUL = range(8)
# Pre-tokenizer classes.
C_WORD, C_PUNCT, C_SPACE = 0, 1, 2


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def fetch(spec, cache, download):
    os.makedirs(cache, exist_ok=True)
    paths = {}
    for name, want in spec["files"].items():
        path = os.path.join(cache, name)
        if not (os.path.exists(path) and sha256(path) == want):
            if not download:
                sys.exit(f"missing or wrong {path} (run without --no-download)")
            url = f"https://huggingface.co/{spec['repo']}/resolve/{spec['commit']}/{name}"
            print(f"downloading {url}", file=sys.stderr)
            tmp = path + ".part"
            with urllib.request.urlopen(url, timeout=300) as r, open(tmp, "wb") as f:
                while True:
                    b = r.read(1 << 20)
                    if not b:
                        break
                    f.write(b)
            os.replace(tmp, path)
        got = sha256(path)
        if got != want:
            sys.exit(f"SHA-256 mismatch for {name}: got {got}, want {want}")
        paths[name] = path
    return paths


def hangul(cp):
    s = cp - 0xAC00
    l, v, t = 0x1100 + s // 588, 0x1161 + (s % 588) // 28, 0x11A7 + s % 28
    return chr(l) + chr(v) + (chr(t) if t != 0x11A7 else "")


def probe_tokenizer(tok_path):
    """Run the reference normalizer and pre-tokenizer on every code point."""
    from tokenizers import Tokenizer  # imported here: only this step needs it

    tok = Tokenizer.from_file(tok_path)
    cfg = json.load(open(tok_path, encoding="utf-8"))
    norm, pre, model = cfg["normalizer"], cfg["pre_tokenizer"], cfg["model"]
    if norm.get("type") != "BertNormalizer" or pre.get("type") != "BertPreTokenizer" or model.get("type") != "WordPiece":
        sys.exit("unsupported tokenizer: need BertNormalizer + BertPreTokenizer + WordPiece")
    if model.get("continuing_subword_prefix") != "##":
        sys.exit("unsupported continuing_subword_prefix")
    n, p = tok.normalizer, tok.pre_tokenizer

    def kind(cp):
        if 0xD800 <= cp <= 0xDFFF:
            return (K_DROP, 0)
        c = chr(cp)
        o = n.normalize_str(c)
        if o == c:
            return (K_SAME, 0)
        if o == "":
            return (K_DROP, 0)
        if o == " ":
            return (K_SPACE, 0)
        if o == " " + c + " ":
            return (K_PAD, 0)
        if 0xAC00 <= cp <= 0xD7A3:
            if o != hangul(cp):
                sys.exit(f"Hangul U+{cp:04X} does not follow the standard decomposition")
            return (K_HANGUL, 0)
        if len(o) == 1:
            return (K_DELTA, ord(o) - cp)
        return (K_STR, o)

    kinds = [kind(cp) for cp in range(0x110000)]
    # Run-length encode; K_ALT covers alternating upper/lower case pairs (delta on even offsets).
    runs, strs = [], bytearray()
    cp = 0
    while cp < 0x110000:
        k, v = kinds[cp]
        if k == K_STR:
            b = v.encode("utf-8")
            if len(b) > 255 or len(strs) > (1 << 20) - 256:
                sys.exit("normalisation string table overflow")
            runs.append((cp, K_STR, (len(strs) << 8) | len(b)))
            strs += b
            cp += 1
            continue
        end = cp + 1
        while end < 0x110000 and kinds[end] == (k, v):
            end += 1
        if k == K_DELTA and end == cp + 1:
            # Try an alternating run: cp+2i -> delta, cp+2i+1 -> same.
            e = cp
            while e + 1 < 0x110000 and kinds[e] == (K_DELTA, v) and kinds[e + 1] == (K_SAME, 0):
                e += 2
            if e - cp >= 4:
                runs.append((cp, K_ALT, v & 0xFFFFFF))
                cp = e
                continue
        runs.append((cp, k, (v & 0xFFFFFF) if k in (K_DELTA,) else 0))
        cp = end
    # Merge consecutive identical non-string runs (possible after ALT splitting).
    merged = []
    for r in runs:
        if merged and merged[-1][1] == r[1] and merged[-1][2] == r[2] and r[1] not in (K_STR, K_ALT):
            continue
        merged.append(r)
    runs = merged

    def klass(c):
        r = [x[0] for x in p.pre_tokenize_str("a" + c + "a")]
        if r == ["a" + c + "a"]:
            return C_WORD
        if r == ["a", c, "a"]:
            return C_PUNCT
        if r == ["a", "a"]:
            return C_SPACE
        sys.exit(f"unexpected pre-tokenizer split for U+{ord(c):04X}: {r}")

    classes = []
    prev = None
    for cp in range(0x110000):
        k = C_WORD if 0xD800 <= cp <= 0xDFFF else klass(chr(cp))
        if k != prev:
            classes.append((cp, k))
            prev = k
    specials = [t["id"] for t in cfg.get("added_tokens", []) if t.get("special")]
    return runs, bytes(strs), classes, model, specials


def quantise(table, bits, clip_search):
    v, d = table.shape
    if bits == 32:
        return table.astype("<f4").tobytes(), None
    levels = 7 if bits == 4 else 127
    amax = np.abs(table).max(axis=1)
    amax[amax == 0] = 1e-12
    best_scale = amax / levels
    if clip_search:
        best_err = np.full(v, np.inf)
        for a in np.linspace(0.5, 1.0, 26):
            s = amax * a / levels
            q = np.clip(np.rint(table / s[:, None]), -levels, levels)
            err = ((q * s[:, None] - table) ** 2).sum(axis=1)
            better = err < best_err
            best_err[better] = err[better]
            best_scale[better] = s[better]
    smax = best_scale.max()
    scale_q = np.maximum(1, np.rint(best_scale / smax * 65535)).astype(np.int64)
    eff = scale_q * (smax / 65535)  # the scale the reader actually applies (up to a common factor)
    codes = np.clip(np.rint(table / eff[:, None]), -levels, levels).astype(np.int64)
    if bits == 8:
        data = codes.astype("<i1").tobytes()
    else:
        nib = (codes + 8).astype(np.uint8)  # 1..15
        data = (nib[:, 0::2] | (nib[:, 1::2] << 4)).astype(np.uint8).tobytes()
    return data, scale_q.astype("<u2").tobytes()


def pad4(b):
    return b + b"\0" * (-len(b) % 4)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--model", required=True, help="key in models.json, e.g. potion-base-8M")
    ap.add_argument("--bits", type=int, choices=(4, 8, 32), default=4)
    ap.add_argument("--dims", type=int, default=0, help="keep only the first N dimensions (0 = all)")
    ap.add_argument("--no-clip-search", action="store_true", help="scale rows by max |w| (no MSE clipping search)")
    ap.add_argument("--cache", default=os.path.join(HERE, "..", "..", ".toolchains", "embedding-downloads"))
    ap.add_argument("--no-download", action="store_true")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    spec = json.load(open(os.path.join(HERE, "models.json"), encoding="utf-8"))[a.model]
    paths = fetch(spec, os.path.join(a.cache, a.model), not a.no_download)
    config = json.load(open(paths["config.json"], encoding="utf-8"))
    if not config.get("normalize", False):
        sys.exit("model output is not normalised; unsupported")

    from safetensors.numpy import load_file

    table = load_file(paths["model.safetensors"])["embeddings"].astype(np.float64)
    runs, strs, classes, wp, specials = probe_tokenizer(paths["tokenizer.json"])
    vocab = wp["vocab"]
    V = len(vocab)
    if V != table.shape[0] or V > 65535:
        sys.exit(f"vocabulary size {V} does not match table {table.shape}")
    if a.dims:
        if a.dims % 8 or a.dims > table.shape[1]:
            sys.exit("--dims must be a multiple of 8 and <= the model's dimensions")
        table = table[:, : a.dims]
    D = table.shape[1]
    toks = [None] * V
    for t, i in vocab.items():
        toks[i] = t.encode("utf-8")
    if any(t is None for t in toks):
        sys.exit("vocabulary ids are not dense")
    unk = vocab[wp["unk_token"]]

    offsets, blob = [], bytearray()
    for t in toks:
        offsets.append(len(blob))
        blob += t
    offsets.append(len(blob))
    order = sorted(range(V), key=lambda i: toks[i])
    data, scales = quantise(table, a.bits, not a.no_clip_search)

    name = f"{a.model}@{spec['commit'][:12]}".encode("utf-8")
    body = bytearray()
    body += pad4(name)
    body += pad4(b"".join(struct.pack("<I", s) for s in specials))
    body += b"".join(struct.pack("<II", s, (k << 24) | v) for s, k, v in runs)
    body += pad4(strs)
    body += b"".join(struct.pack("<II", s, k) for s, k in classes)
    body += b"".join(struct.pack("<I", o) for o in offsets)
    body += pad4(bytes(blob))
    body += pad4(b"".join(struct.pack("<H", i) for i in order))
    if scales is not None:
        body += pad4(scales)
    table_off = HEADER_SIZE + len(body)
    total = table_off + len(data)

    hdr = bytearray(HEADER_SIZE)
    hdr[0:8] = MAGIC
    fields = [VERSION, HEADER_SIZE, V, D, a.bits, unk, wp.get("max_input_chars_per_word", 100), 1,
              len(runs), len(strs), len(classes), len(blob), len(name), len(specials), total,
              zlib.crc32(bytes(body)), table_off, zlib.crc32(data)]
    struct.pack_into("<" + "I" * len(fields), hdr, 8, *fields)
    struct.pack_into("<I", hdr, 92, zlib.crc32(bytes(hdr[:92])))
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    tmp = a.out + ".part"
    with open(tmp, "wb") as f:
        f.write(hdr)
        f.write(body)
        f.write(data)
    os.replace(tmp, a.out)
    print(f"{a.out}: {a.model} V={V} D={D} bits={a.bits} norm_runs={len(runs)} class_runs={len(classes)} "
          f"bytes={total} sha256={sha256(a.out)}")


if __name__ == "__main__":
    main()
