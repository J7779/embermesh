#!/usr/bin/env bash
# Checks that the toolchain versions pinned in .github/ match the ones the build files and the
# platform READMEs require, so CI cannot drift from local builds:
#   Emscripten   .github/actions/setup-emsdk/action.yml  ==  apps/web/README.md ("emsdk install X")
#   Android      .github/actions/setup-android/action.yml installs the NDK, SDK CMake and platform
#                that apps/android/app/build.gradle.kts asks for (ndkVersion, cmake version, compileSdk)
#   actions      every `uses:` in .github/ is pinned to a full commit SHA (40 hex) with a version comment
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
bad=0
err() { echo "error: $*" >&2; bad=$((bad + 1)); }

readme_em="$(sed -n 's/.*emsdk install \([0-9][0-9.]*\).*/\1/p' apps/web/README.md | head -1)"
action_em="$(awk '/^  version:/ { f = 1 } f && /default:/ { gsub(/[^0-9.]/, "", $2); print $2; exit }' .github/actions/setup-emsdk/action.yml)"
[[ -n "$readme_em" && "$readme_em" == "$action_em" ]] || err "Emscripten: apps/web/README.md says '${readme_em:-?}', .github/actions/setup-emsdk pins '${action_em:-?}'"

g=apps/android/app/build.gradle.kts
a=.github/actions/setup-android/action.yml
ndk="$(sed -n 's/^ *ndkVersion *= *"\([^"]*\)".*/\1/p' "$g")"
cmk="$(awk '/cmake *\{/ { f = 1 } f && /version *=/ { gsub(/[^0-9.]/, "", $0); print; exit }' "$g")"
sdk="$(sed -n 's/^ *compileSdk *= *\([0-9]*\).*/\1/p' "$g")"
grep -q "ndk;$ndk" "$a" || err "Android NDK: $g wants $ndk, $a does not install ndk;$ndk"
grep -q "cmake;$cmk" "$a" || err "Android CMake: $g wants $cmk, $a does not install cmake;$cmk"
grep -Eq "platforms;android-$sdk(\.0)?\"" "$a" || err "Android SDK: $g compiles against $sdk, $a does not install platforms;android-$sdk"

while IFS= read -r line; do
  file="${line%%:*}"; rest="${line#*:}"; n="${rest%%:*}"; use="${rest#*:}"
  ref="$(printf '%s' "$use" | sed -n 's/.*uses: *[^@ ]*@\([^ #]*\).*/\1/p')"
  case "$use" in *"uses: ./"*) continue ;; esac
  if ! [[ "$ref" =~ ^[0-9a-f]{40}$ ]]; then err "$file:$n: action not pinned to a commit SHA: $(echo "$use" | sed 's/^ *//')"
  elif ! [[ "$use" =~ \#\ *v[0-9] ]]; then err "$file:$n: pinned action without a '# vX.Y.Z' comment"; fi
done < <(grep -rn -E '^\s*(-\s*)?uses:' .github)

echo "pins: Emscripten ${action_em:-?}, NDK ${ndk:-?}, CMake ${cmk:-?}, android-${sdk:-?}; $bad problem(s)"
[[ $bad -eq 0 ]]
