#!/usr/bin/env bash
# Validates the guide corpus, inspects a built guides.pack and runs the search smoke checks; prints
# a Markdown report on stdout (for $GITHUB_STEP_SUMMARY) and diagnostics on stderr.
#
#   tools/ci/guides-report.sh <guidepack binary> <guides.pack> [content dir]
#
# Fails (exit 1) on any validation error or failed search check; warnings are counted, not fatal.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
GP="${1:?usage: guides-report.sh <guidepack> <guides.pack> [content dir]}"
PACK="${2:?usage: guides-report.sh <guidepack> <guides.pack> [content dir]}"
CONTENT="${3:-$ROOT/guides/content}"
CHECKS="$ROOT/tools/ci/guides-search-checks.tsv"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/guides-report.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
fail=0

# --- validate ---------------------------------------------------------------------------------
rc=0; "$GP" validate "$CONTENT" > "$WORK/validate.txt" 2>&1 || rc=$?
cat "$WORK/validate.txt" >&2
vline="$(grep -E 'guide file\(s\):' "$WORK/validate.txt" | tail -1 || true)"
errors="$(grep -c ': error: ' "$WORK/validate.txt" || true)"
warnings="$(grep -c ': warning: ' "$WORK/validate.txt" || true)"
[[ $rc -ne 0 ]] && fail=1

# --- pack statistics ----------------------------------------------------------------------------
"$GP" stats "$PACK" > "$WORK/stats.txt" 2>&1 || { cat "$WORK/stats.txt" >&2; echo "guidepack stats failed" >&2; fail=1; }
guides="$(sed -n 's/^guides: *\([0-9]*\) in \([0-9]*\) categories.*/\1/p' "$WORK/stats.txt")"
cats="$(sed -n 's/^guides: *\([0-9]*\) in \([0-9]*\) categories.*/\2/p' "$WORK/stats.txt")"
terms="$(sed -n 's/.* categories, \([0-9]*\) index terms.*/\1/p' "$WORK/stats.txt")"
bytes="$(wc -c < "$PACK" | tr -d ' ')"
verify="$(sed -n 's/^verify: *//p' "$WORK/stats.txt")"
case "$verify" in ok*) ;; *) echo "pack verification: ${verify:-missing}" >&2; fail=1 ;; esac

# --- search smoke checks ------------------------------------------------------------------------
: > "$WORK/search.md"; npass=0; nfail=0
while IFS=$'\t' read -r query want; do
  [[ -z "$query" || "$query" == \#* ]] && continue
  # shellcheck disable=SC2086  # the query is split into words on purpose (guidepack takes words)
  "$GP" search "$PACK" $query --limit 5 > "$WORK/q.txt" 2>&1 || true
  ids="$(sed -n -E 's/^ *[0-9]+\. \[[^]]*\] ([^ ]+) .*/\1/p' "$WORK/q.txt")"
  if printf '%s\n' "$ids" | grep -Eq -- "$want"; then
    npass=$((npass + 1)); r="pass"
  else
    nfail=$((nfail + 1)); r="**FAIL**"; fail=1
    echo "search check failed: \"$query\" (want /$want/ in the top 5), got:" >&2; cat "$WORK/q.txt" >&2
  fi
  printf '| `%s` | `%s` | %s | %s |\n' "$query" "${want//|/\\|}" "$(printf '%s' "$ids" | head -1)" "$r" >> "$WORK/search.md"
done < "$CHECKS"

# --- report -------------------------------------------------------------------------------------
mib="$(awk -v b="$bytes" 'BEGIN { printf "%.1f", b / 1048576 }')"
echo "### Guide library"
echo
echo "| | |"
echo "|---|---|"
echo "| Guides in pack | ${guides:-?} |"
echo "| Categories | ${cats:-?} |"
echo "| Index terms | ${terms:-?} |"
echo "| guides.pack | $mib MiB ($bytes bytes) |"
echo "| Verify | ${verify:-?} |"
echo "| Validation | ${vline:-?} |"
echo "| Errors / warnings | $errors / $warnings |"
echo
echo "<details><summary>Guides per category</summary>"
echo
echo "| Category | Guides | Title |"
echo "|---|---:|---|"
sed -n '/^categories:/,$p' "$WORK/stats.txt" | sed '1d' | awk 'NF >= 2 { id = $1; n = $2; $1 = ""; $2 = ""; sub(/^ +/, ""); printf "| %s | %s | %s |\n", id, n, $0 }'
echo
echo "</details>"
echo
echo "#### Search smoke checks: $npass passed, $nfail failed"
echo
echo "| Query | Expected id in top 5 | Top result | Result |"
echo "|---|---|---|---|"
cat "$WORK/search.md"
if [[ $errors -gt 0 ]]; then
  echo
  echo "<details><summary>Validation errors</summary>"
  echo
  echo '```'
  grep ': error: ' "$WORK/validate.txt" | head -200
  echo '```'
  echo "</details>"
fi
exit $fail
