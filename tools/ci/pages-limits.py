#!/usr/bin/env python3
"""Checks a site directory against GitHub Pages limits and prints a Markdown report.

  pages-limits.py DIR

Limits (GitHub documentation, "GitHub Pages limits"): a published site may be at most 1 GB; the
deployment artifact is a tar of the directory. GitHub does not publish a separate per-file limit
for sites deployed with Actions; 100 MiB (the limit for a file pushed to a repository) is used
here as the conservative per-file ceiling. Exit 1 if either is exceeded; warns below 25 % margin.
Pages sets its own Cache-Control (max-age=600) and custom headers cannot be configured, so long
cache lifetimes come from the app's own versioned service-worker caches instead.
"""
import os
import sys

SITE_LIMIT = 1_000_000_000
FILE_LIMIT = 100 * 1024 * 1024


def mib(n):
    return f"{n / 1048576:.1f} MiB"


def main(argv):
    if len(argv) != 1 or not os.path.isdir(argv[0]):
        print(__doc__, file=sys.stderr)
        return 2
    files = []
    for d, _, fs in os.walk(argv[0]):
        for f in fs:
            p = os.path.join(d, f)
            files.append((os.path.getsize(p), os.path.relpath(p, argv[0]).replace(os.sep, "/")))
    total = sum(s for s, _ in files)
    files.sort(reverse=True)
    big = files[0] if files else (0, "-")
    rc = 0
    print("### GitHub Pages limits")
    print()
    print("| | Size | Limit | Margin |")
    print("|---|---:|---:|---:|")
    print(f"| Site ({len(files)} files) | {mib(total)} | 1 GB | {100 - 100 * total / SITE_LIMIT:.0f} % |")
    print(f"| Largest file `{big[1]}` | {mib(big[0])} | 100 MiB | {100 - 100 * big[0] / FILE_LIMIT:.0f} % |")
    print()
    print("Largest files: " + ", ".join(f"`{n}` {mib(s)}" for s, n in files[:5]))
    for what, size, limit in (("site", total, SITE_LIMIT), ("largest file", big[0], FILE_LIMIT)):
        if size > limit:
            print(f"\n**{what} exceeds the GitHub Pages limit**")
            print(f"::error::{what} is {mib(size)}, over the GitHub Pages limit", file=sys.stderr)
            rc = 1
        elif size > 0.75 * limit:
            print(f"::warning::{what} is {mib(size)}, within 25 % of the GitHub Pages limit", file=sys.stderr)
    print()
    print("Pages serves every file with its own `Cache-Control: max-age=600`; custom headers cannot be set, so the app's "
          "versioned service-worker caches provide the long-lived offline copy.")
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
