#!/usr/bin/env bash
# Selects the newest stable Xcode installed on a macOS runner (/Applications/Xcode*.app) and makes
# sure an iOS simulator runtime and at least one iPhone simulator are available for it, so that
# build.sh's simulator choice (first booted iPhone, else the first available one) always finds one.
#
#   tools/ci/select-xcode.sh            (uses sudo xcode-select; meant for CI runners)
#   XCODE_APP=/Applications/Xcode_16.4.app tools/ci/select-xcode.sh   pick a specific one
set -euo pipefail

app="${XCODE_APP:-}"
if [[ -z "$app" ]]; then
  best=""; bestv=""
  for a in /Applications/Xcode*.app; do
    [[ -d "$a" ]] || continue
    case "$a" in *[Bb]eta*|*_RC*|*Release_Candidate*) continue ;; esac
    v="$(defaults read "$a/Contents/Info" CFBundleShortVersionString 2>/dev/null || true)"
    [[ -n "$v" ]] || continue
    if [[ -z "$bestv" ]] || [[ "$(printf '%s\n%s\n' "$bestv" "$v" | sort -t. -k1,1n -k2,2n -k3,3n | tail -1)" == "$v" ]]; then best="$a"; bestv="$v"; fi
  done
  app="$best"
fi
[[ -n "$app" && -d "$app" ]] || { echo "no Xcode found in /Applications" >&2; exit 1; }
if [[ "$(xcode-select -p 2>/dev/null)" != "$app/Contents/Developer" ]]; then sudo xcode-select -s "$app/Contents/Developer"; fi
xcodebuild -version

if ! xcrun simctl list runtimes available 2>/dev/null | grep -q '^iOS '; then
  echo "no iOS simulator runtime for this Xcode; downloading one"
  xcodebuild -downloadPlatform iOS
fi
if ! xcrun simctl list devices available 2>/dev/null | grep -Eq '^ +iPhone '; then
  rt="$(xcrun simctl list runtimes available | sed -n 's/^iOS .* - \(com\.apple\.CoreSimulator\.SimRuntime\.iOS-[0-9-]*\)$/\1/p' | tail -1)"
  dt="$(xcrun simctl list devicetypes | sed -n 's/^\(iPhone [^(]*\) (\(com\.apple\.CoreSimulator\.SimDeviceType\.[^)]*\))$/\2/p' | tail -1)"
  [[ -n "$rt" && -n "$dt" ]] || { echo "cannot create an iPhone simulator (runtime '$rt', device type '$dt')" >&2; exit 1; }
  xcrun simctl create "CI iPhone" "$dt" "$rt"
fi
xcrun simctl list runtimes available | grep '^iOS ' || true
echo "first available iPhone: $(xcrun simctl list devices available | grep -E '^ +iPhone ' | head -1 | sed 's/ ([0-9A-F-]*).*//; s/^ *//')"
