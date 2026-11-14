#!/usr/bin/env bash
# Checks that the files the repository depends on exist and are publishable: present in the
# working tree, and not ignored by .gitignore (an ignored file would silently be left out of the
# repository; `git check-ignore` must report none of them).
#
#   tools/ci/check-required-files.sh [--root DIR]
#
# Exit 1 if any required file is missing or ignored. The list is below; globs that match nothing
# are an error unless marked optional (models/ exists only once the embedding model is committed).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
[[ "${1:-}" == --root ]] && ROOT="$(cd "${2:?--root needs a directory}" && pwd)"
cd "$ROOT"

# <glob> [optional]
required=(
  "build.sh"
  "build.ps1"
  "LICENSE"
  "AGENTS.md"
  "CLAUDE.md"
  ".claude/settings.json"
  ".claude/skills/*/SKILL.md"
  "tools/check-publishable.sh"
  "tools/publishable-allow.txt"
  "tools/publishable-landmarks.txt"
  ".gitignore"
  ".gitattributes"
  ".github/workflows/*.yml"
  ".github/actions/*/action.yml"
  "tools/ci/*.sh"
  "tools/ci/*.py"
  "apps/android/gradlew"
  "apps/android/gradlew.bat"
  "apps/android/gradle/wrapper/gradle-wrapper.jar"
  "apps/android/gradle/wrapper/gradle-wrapper.properties"
  "apps/ios/EmberMesh.xcodeproj/project.pbxproj"
  "apps/ios/EmberMesh.xcodeproj/xcshareddata/xcschemes/EmberMesh.xcscheme"
  "core/tests/data/node/v1-encrypted-store/data-2.ekv"
  "models/* optional"
)
problems=0; checked=0
for entry in "${required[@]}"; do
  glob="${entry% optional}"; optional=0; [[ "$entry" == *" optional" ]] && optional=1
  shopt -s nullglob
  # shellcheck disable=SC2206  # the glob is expanded on purpose
  matches=($glob)
  shopt -u nullglob
  existing=(); for f in ${matches[@]+"${matches[@]}"}; do [[ -e "$f" ]] && existing+=("$f"); done
  matches=(${existing[@]+"${existing[@]}"})
  if [[ ${#matches[@]} -eq 0 ]]; then
    [[ $optional == 1 ]] && continue
    echo "error: $glob: required file is missing" >&2; problems=$((problems + 1)); continue
  fi
  for f in "${matches[@]}"; do
    if [[ -d "$f" ]]; then
      # A directory (models/): every file in it must be publishable.
      while IFS= read -r -d '' g; do
        checked=$((checked + 1))
        if git check-ignore -q --no-index -- "$g"; then echo "error: $g: required file is ignored by git ($(git check-ignore -v --no-index -- "$g" | cut -f1))" >&2; problems=$((problems + 1)); fi
      done < <(find "$f" -type f -print0)
      continue
    fi
    checked=$((checked + 1))
    if git check-ignore -q --no-index -- "$f"; then
      echo "error: $f: required file is ignored by git ($(git check-ignore -v --no-index -- "$f" | cut -f1))" >&2; problems=$((problems + 1))
    fi
  done
done
echo "required files: $checked checked, $problems problem(s)"
[[ $problems -eq 0 ]]
