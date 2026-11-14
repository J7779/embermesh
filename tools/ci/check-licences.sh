#!/usr/bin/env bash
# Licence and attribution checks for publishing.
#
#   tools/ci/check-licences.sh [--root DIR]
#
#   1. the project's own LICENSE exists at the repository root and is not empty
#   2. every vendored third-party component keeps its licence text next to it
#   3. every guide category has a _sources.md, and every guide records source, licence and
#      attribution in its front matter with a licence that permits redistribution and modification
#      (guidepack validate enforces the same allow-list in the Guides workflow; this is a fast
#      second line that needs no build)
# Prints file:line style findings, a Markdown summary when GITHUB_STEP_SUMMARY is set, and exits 1
# on any finding.
set -euo pipefail
export LC_ALL=C

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
[[ "${1:-}" == --root ]] && ROOT="$(cd "${2:?--root needs a directory}" && pwd)"
cd "$ROOT"
problems=0
report() { echo "error: $*" >&2; problems=$((problems + 1)); }

# --- 1. project licence ----------------------------------------------------------------------------
lic=""
for f in LICENSE LICENSE.md LICENSE.txt LICENCE LICENCE.md COPYING; do [[ -s "$f" ]] && { lic="$f"; break; }; done
[[ -n "$lic" ]] || report "LICENSE: the project has no licence file at the repository root (add one before publishing)"

# --- 2. vendored third-party code -------------------------------------------------------------------
# <component>|<licence file, or a file whose header carries the licence>|<pattern it must contain>
third_party=(
  "Monocypher|core/third_party/monocypher/LICENCE.md|BSD|CC0"
  "doctest|core/third_party/include/doctest.h|MIT"
  "miniaudio|host/third_party/miniaudio/LICENSE|public domain|MIT"
  "ZXing core|apps/android/app/libs/ZXING-LICENSE.txt|Apache License"
)
declare -a tp_rows=()
for e in "${third_party[@]}"; do
  IFS='|' read -r name file pat1 pat2 <<< "$e"
  pat="$pat1${pat2:+|$pat2}"
  if [[ ! -s "$file" ]]; then
    report "$file: licence of $name is missing"; tp_rows+=("| $name | \`$file\` | missing |")
  elif ! grep -Eiq -- "$pat" "$file"; then
    report "$file: does not look like the $name licence (expected /$pat/)"; tp_rows+=("| $name | \`$file\` | unexpected text |")
  else
    tp_rows+=("| $name | \`$file\` | ok |")
  fi
done
# Any other vendored directory must carry a licence file too.
for d in core/third_party/* host/third_party/*; do
  [[ -d "$d" ]] || continue
  case "$d" in core/third_party/include) continue ;; esac
  has=0
  for f in "$d"/*; do case "$(basename "$f" | tr 'A-Z' 'a-z')" in licen[cs]e*|copying*|unlicense*) has=1 ;; esac; done
  [[ $has == 1 ]] || report "$d: vendored directory without a LICENSE/COPYING file"
done

# --- 3. guide library -------------------------------------------------------------------------------
nguides=0; ncats=0
if [[ -d guides/content ]]; then
  for c in guides/content/*/; do
    c="${c%/}"; ncats=$((ncats + 1))
    [[ -s "$c/_sources.md" ]] || report "$c/_sources.md: category has no source list"
  done
  # One awk pass over every guide's front matter.
  while IFS= read -r line; do report "$line"; done < <(
    find guides/content -name '*.md' -not -name '_*' -not -path '*/_*' -print0 | xargs -0 awk '
      FNR == 1 { if (prev != "") done(prev); prev = FILENAME; fm = 0; inside = 0; delete k; lic = "" }
      FNR == 1 && $0 == "---" { inside = 1; next }
      inside && $0 == "---" { inside = 0; fm = 1; next }
      inside { if (match($0, /^[a-z_]+:/)) { key = substr($0, 1, RLENGTH - 1); val = substr($0, RLENGTH + 1); sub(/^[ \t]+/, "", val); k[key] = val; if (key == "license") lic = tolower(val) } }
      function done(f) {
        if (!fm) { print f ":1: no front matter"; return }
        if (k["source"] == "") print f ":1: missing source"
        if (k["license"] == "") print f ":1: missing license"
        if (k["attribution"] == "") print f ":1: missing attribution"
        if (lic ~ /(^|[^a-z])(nc|nd)([^a-z]|$)|noncommercial|non-commercial|noderiv|no deriv|all rights reserved|proprietary/) print f ":1: licence does not permit redistribution and modification: " k["license"]
      }
      END { if (prev != "") done(prev) }'
  )
  nguides="$(find guides/content -name '*.md' -not -name '_*' -not -path '*/_*' | wc -l | tr -d ' ')"
fi

# --- report -------------------------------------------------------------------------------------
if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
  {
    echo "### Licences and attribution"
    echo
    if [[ -n "$lic" ]]; then echo "- Project licence: \`$lic\`"; else echo "- Project licence: **missing** (add LICENSE at the repository root)"; fi
    echo "- Guide library: $nguides guides in $ncats categories checked for source, licence and attribution"
    echo
    echo "| Third-party component | Licence file | Status |"
    echo "|---|---|---|"
    printf '%s\n' "${tp_rows[@]}"
    echo
    echo "**$problems problem(s)**"
  } >> "$GITHUB_STEP_SUMMARY"
fi
echo "licence check: ${lic:-no project LICENSE}; ${#third_party[@]} third-party components; $nguides guides in $ncats categories; $problems problem(s)"
[[ $problems -eq 0 ]]
