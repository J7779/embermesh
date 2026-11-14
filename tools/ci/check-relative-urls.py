#!/usr/bin/env python3
"""Checks that a built static site works under a sub-path (https://<owner>.github.io/<repo>/).

  check-relative-urls.py DIR

Every reference from the site to its own files must be relative: a root-absolute URL ("/wasm/...")
would load from https://<owner>.github.io/wasm/... instead. Checked:
  *.html               src= href= action= poster= data= attributes, and url(...) in inline styles
  *.css                url(...)
  *.webmanifest        id, start_url, scope, icons[].src, screenshots[].src, shortcuts[].url
  *.js / *.mjs         string literals that start with "/" followed by one of the site's own
                       top-level files or folders (as found in DIR), e.g. '/sw.js', "/data/..."
Hub endpoints (/ws, /rpc, /download/, /hub.json on a hub) are served by emberd at its own root and
are not part of the static site, so they are allowed. Prints file:line findings; exit 1 if any.
"""
import json
import os
import re
import sys

ALLOWED_ROOT = ("/ws", "/rpc", "/download/")


def rel(path, base):
    return os.path.relpath(path, base).replace(os.sep, "/")


def line_of(text, pos):
    return text.count("\n", 0, pos) + 1


def main(argv):
    if len(argv) != 1 or not os.path.isdir(argv[0]):
        print(__doc__, file=sys.stderr)
        return 2
    base = argv[0]
    tops = sorted(e for e in os.listdir(base) if not e.startswith("."))
    own = "|".join(re.escape(t) for t in tops)
    js_re = re.compile(r"""(['"`])/(?!/)(%s)(?=[/'"`?#])""" % own) if own else None
    attr_re = re.compile(r"""\b(src|href|action|poster|data)\s*=\s*(["']?)(/[^"'\s>]*)""", re.I)
    css_re = re.compile(r"""url\(\s*(["']?)(/[^)"']*)""", re.I)
    findings = []
    nfiles = 0
    for d, dirs, files in os.walk(base):
        dirs[:] = sorted(dirs)
        for f in sorted(files):
            p = os.path.join(d, f)
            ext = os.path.splitext(f)[1].lower()
            if ext not in (".html", ".htm", ".css", ".js", ".mjs", ".webmanifest", ".json"):
                continue
            nfiles += 1
            with open(p, "rb") as fh:
                text = fh.read().decode("utf-8", "replace")
            r = rel(p, base)
            if ext in (".html", ".htm"):
                for m in attr_re.finditer(text):
                    url = m.group(3)
                    if url.startswith(ALLOWED_ROOT):
                        continue
                    findings.append(f"{r}:{line_of(text, m.start())}: root-absolute {m.group(1)}=\"{url}\"")
            if ext in (".html", ".htm", ".css"):
                for m in css_re.finditer(text):
                    findings.append(f"{r}:{line_of(text, m.start())}: root-absolute url({m.group(2)})")
            if ext in (".js", ".mjs", ".html", ".htm") and js_re:
                for m in js_re.finditer(text):
                    findings.append(f"{r}:{line_of(text, m.start())}: root-absolute reference to the site's own /{m.group(2)}")
            if ext == ".webmanifest" or f == "manifest.json":
                try:
                    man = json.loads(text)
                except ValueError as e:
                    findings.append(f"{r}:1: manifest is not JSON: {e}")
                    continue
                urls = [(k, man.get(k)) for k in ("id", "start_url", "scope")]
                urls += [("icons[].src", i.get("src")) for i in man.get("icons", []) if isinstance(i, dict)]
                urls += [("screenshots[].src", i.get("src")) for i in man.get("screenshots", []) if isinstance(i, dict)]
                urls += [("shortcuts[].url", i.get("url")) for i in man.get("shortcuts", []) if isinstance(i, dict)]
                for k, u in urls:
                    if isinstance(u, str) and (u.startswith("/") or re.match(r"^[a-z]+://", u)):
                        findings.append(f"{r}:1: manifest {k} must be relative, is \"{u}\"")
    for x in findings:
        print(x)
    print(f"relative URLs: {nfiles} files checked under {base}, {len(findings)} problem(s)", file=sys.stderr)
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
