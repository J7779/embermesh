#!/usr/bin/env python3
"""Final gate of a workflow: report every job's result and fail if a required one did not succeed.

  NEEDS='${{ toJSON(needs) }}' needs-summary.py TITLE [--optional JOB]...

Every job in NEEDS is required unless named with --optional (a non-blocking job, reported but not
counted). Writes a Markdown table to $GITHUB_STEP_SUMMARY (stdout when unset); exit 1 when a
required job's result is anything but "success" (failure, cancelled or skipped).
"""
import json
import os
import sys

ICON = {"success": "✅", "failure": "❌", "cancelled": "⛔", "skipped": "⏭️"}


def main(argv):
    if not argv:
        print(__doc__, file=sys.stderr)
        return 2
    title, optional, i = argv[0], set(), 1
    while i < len(argv):
        if argv[i] == "--optional" and i + 1 < len(argv):
            optional.add(argv[i + 1])
            i += 2
        else:
            print(f"unknown argument {argv[i]}", file=sys.stderr)
            return 2
    try:
        needs = json.loads(os.environ.get("NEEDS", "") or "{}")
    except ValueError as e:
        print(f"NEEDS is not JSON: {e}", file=sys.stderr)
        return 2
    lines = [f"## {title}", "", "| Job | Result | Blocking |", "|---|---|---|"]
    bad = []
    for job in sorted(needs):
        result = needs[job].get("result", "unknown")
        blocking = job not in optional
        lines.append(f"| {job} | {ICON.get(result, '')} {result} | {'yes' if blocking else 'no'} |")
        if blocking and result != "success":
            bad.append(f"{job} ({result})")
    for job in sorted(optional - set(needs)):
        lines.append(f"| {job} | not run | no |")
    lines.append("")
    lines.append("**All required jobs succeeded.**" if not bad else "**Required jobs not successful:** " + ", ".join(bad))
    text = "\n".join(lines) + "\n"
    path = os.environ.get("GITHUB_STEP_SUMMARY")
    if path:
        with open(path, "a", encoding="utf-8") as f:
            f.write(text)
    print(text)
    return 1 if bad or not needs else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
