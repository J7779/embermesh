#!/bin/bash
# Builds the native libraries for iOS with CMake: the shared C++ core (core/), the host library's
# hub (host/, audio + serial off) and the small hub wrapper in apps/ios/native/, merged into ONE
# static library per SDK.
#
#   apps/ios/build-core.sh [iphoneos|iphonesimulator|all] [--xcframework] [--modules "base;capi"]
#
# Output (default root: <repo>/build/ios/core, override with EMBER_CORE_BUILD_DIR):
#   <root>/iphoneos/lib/libember_ios.a               arm64 device
#   <root>/iphonesimulator/lib/libember_ios.a        arm64 + x86_64 simulator
#   <root>/EmberCore.xcframework                     (with --xcframework, both slices + headers)
#
# The core sources are discovered by core/CMakeLists.txt (glob), so new core modules are picked up
# automatically: the Xcode project never lists core files. Xcode runs this script as the first build
# phase of the EmberMesh target with the platform it is building for (see project.pbxproj); you can
# also run it by hand before xcodebuild.
#
# CMake selection: the first candidate that answers `cmake --version` within 20 s (run from the
# repo) wins: $EMBER_CMAKE, the one that worked last time (<root>/.cmake-path), cmake on PATH, <repo>/.toolchains/bin/cmake, .toolchains/cmake/bin/cmake,
# Homebrew / CMake.app, the Android SDK's cmake. A cmake that hangs (e.g. blocked on a parent
# directory) is skipped. Xcode build phases run with a minimal PATH, so all of these are probed
# by absolute path. CMake >= 3.20 is required (3.22 is known to work).
#
# Environment:
#   EMBER_CMAKE            use this cmake (still probed)
#   EMBER_MODULES          core modules ("all" by default, or a ;-list such as "base;capi" for a faster partial build)
#   EMBER_CORE_BUILD_DIR   output root
#   EMBER_CORE_CONFIG      CMake build type (Release default; Debug when Xcode CONFIGURATION=Debug)
#   IOS_DEPLOYMENT_TARGET  minimum iOS (15.0)
#   EMBER_SIM_ARCHS        simulator architectures (default "arm64;x86_64")
#   EMBER_SKIP_CORE_BUILD  =1: do not run CMake, reuse the libraries already in the output root
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
cd "$REPO"   # never let cmake start from (or walk up into) a directory outside the repo

PLATFORMS="all"
XCFRAMEWORK=0
MODULES="${EMBER_MODULES:-all}"
while [ $# -gt 0 ]; do
  case "$1" in
    iphoneos|iphonesimulator|all) PLATFORMS="$1" ;;
    --xcframework) XCFRAMEWORK=1 ;;
    --modules) shift; MODULES="$1" ;;
    -h|--help) sed -n '2,32p' "$0"; exit 0 ;;
    *) echo "build-core.sh: unknown argument $1" >&2; exit 2 ;;
  esac
  shift
done
[ "$PLATFORMS" = "all" ] && PLATFORMS="iphoneos iphonesimulator"
LIBNAME=libember_ios.a

if [ "${EMBER_SKIP_CORE_BUILD:-0}" = "1" ]; then
  for p in $PLATFORMS; do
    lib="${EMBER_CORE_BUILD_DIR:-$REPO/build/ios/core}/$p/lib/$LIBNAME"
    [ -f "$lib" ] || { echo "error: EMBER_SKIP_CORE_BUILD=1 but $lib does not exist" >&2; exit 1; }
    echo "note: reusing prebuilt $lib (EMBER_SKIP_CORE_BUILD=1)"
  done
  exit 0
fi

OUT="${EMBER_CORE_BUILD_DIR:-$REPO/build/ios/core}"
DEPLOY="${IOS_DEPLOYMENT_TARGET:-15.0}"
CONFIG="${EMBER_CORE_CONFIG:-}"
if [ -z "$CONFIG" ]; then
  if [ "${CONFIGURATION:-Release}" = "Debug" ]; then CONFIG=Debug; else CONFIG=Release; fi
fi

# --- find a cmake that actually answers -----------------------------------------------------------
with_timeout() { local secs="$1"; shift; /usr/bin/perl -e 'alarm shift; exec @ARGV or exit 127' "$secs" "$@"; }
cmake_ok() {
  local c="$1"
  [ -n "$c" ] && [ -x "$c" ] || return 1
  local v
  v="$(cd "$REPO" && with_timeout 20 "$c" --version 2>/dev/null | head -n 1)" || return 1
  # cmake version 3.22.1-g37088a8  ->  3 22
  local maj min
  maj="$(echo "$v" | sed -n 's/^cmake version \([0-9]*\)\.\([0-9]*\).*/\1/p')"
  min="$(echo "$v" | sed -n 's/^cmake version \([0-9]*\)\.\([0-9]*\).*/\2/p')"
  [ -n "$maj" ] || return 1
  if [ "$maj" -lt 3 ] || { [ "$maj" -eq 3 ] && [ "$min" -lt 20 ]; }; then
    echo "note: skipping $c ($v is older than 3.20)" >&2
    return 1
  fi
  return 0
}
# The cmake that worked last time is tried right after EMBER_CMAKE, so a hanging cmake earlier on
# PATH costs the 20 s probe only once.
CACHE_FILE="$OUT/.cmake-path"
CANDIDATES=()
[ -n "${EMBER_CMAKE:-}" ] && CANDIDATES+=("$EMBER_CMAKE")
[ -f "$CACHE_FILE" ] && CANDIDATES+=("$(cat "$CACHE_FILE")")
ONPATH="$(command -v cmake 2>/dev/null || true)"
[ -n "$ONPATH" ] && CANDIDATES+=("$ONPATH")
CANDIDATES+=("$REPO/.toolchains/bin/cmake" "$REPO/.toolchains/cmake/bin/cmake"
             /opt/homebrew/bin/cmake /usr/local/bin/cmake /Applications/CMake.app/Contents/bin/cmake)
for sdk in "${ANDROID_HOME:-}" "${ANDROID_SDK_ROOT:-}" "$HOME/Library/Android/sdk"; do
  [ -n "$sdk" ] && [ -d "$sdk/cmake" ] || continue
  for c in "$sdk"/cmake/*/bin/cmake; do [ -x "$c" ] && CANDIDATES+=("$c"); done
done
CMAKE=""
SEEN=" "
for c in "${CANDIDATES[@]}"; do
  case "$SEEN" in *" $c "*) continue ;; esac
  SEEN="$SEEN$c "
  [ -x "$c" ] || continue
  if cmake_ok "$c"; then CMAKE="$c"; break; fi
  echo "warning: cmake at $c did not answer --version within 20 s (hung or broken); trying the next one" >&2
done
if [ -z "$CMAKE" ]; then
  echo "error: no working CMake >= 3.20 found. Install one (brew install cmake, CMake.app, or put it in .toolchains/bin) or set EMBER_CMAKE" >&2
  exit 1
fi
echo "using $CMAKE ($("$CMAKE" --version | head -n 1))"
mkdir -p "$OUT" && echo "$CMAKE" > "$CACHE_FILE"

DEV_DIR="${DEVELOPER_DIR:-$(xcode-select -p)}"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
# Run CMake in a clean environment: Xcode exports dozens of variables (SDKROOT, ARCHS, CC ...) that
# would otherwise leak into the configure step. PATH holds only system tools + the chosen cmake.
CLEAN_ENV=(env -i PATH="/usr/bin:/bin:/usr/sbin:/sbin:$(dirname "$CMAKE")" HOME="$HOME" DEVELOPER_DIR="$DEV_DIR")

build_one() {
  local sdk="$1" archs sysroot
  case "$sdk" in
    iphoneos) archs="arm64" ;;
    iphonesimulator) archs="${EMBER_SIM_ARCHS:-arm64;x86_64}" ;;
  esac
  # CMake 3.22 resolves CMAKE_OSX_SYSROOT=<sdk name> itself, but an absolute path avoids any
  # xcrun lookups from inside the clean environment.
  sysroot="$(DEVELOPER_DIR="$DEV_DIR" xcrun --sdk "$sdk" --show-sdk-path 2>/dev/null || echo "$sdk")"
  local bdir="$OUT/$sdk/cmake-$CONFIG"
  local mark="$bdir/.ember-config"
  local want="v2|$archs|$DEPLOY|$MODULES|$CONFIG|$sysroot|$CMAKE"
  if [ -f "$mark" ] && [ "$(cat "$mark")" != "$want" ]; then rm -rf "$bdir"; fi
  mkdir -p "$bdir" "$OUT/$sdk/lib"
  if [ ! -f "$bdir/CMakeCache.txt" ] || [ ! -f "$mark" ]; then
    (cd "$REPO" && "${CLEAN_ENV[@]}" "$CMAKE" -S "$HERE/native" -B "$bdir" -G "Unix Makefiles" \
      -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_SYSROOT="$sysroot" \
      -DCMAKE_OSX_ARCHITECTURES="$archs" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOY" \
      -DCMAKE_BUILD_TYPE="$CONFIG" \
      -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
      -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO \
      -DEMBER_MODULES="$MODULES" \
      > "$bdir/configure.log" 2>&1) || { cat "$bdir/configure.log" >&2; echo "error: CMake configure failed for $sdk" >&2; rm -f "$mark"; exit 1; }
    echo "$want" > "$mark"
  fi
  (cd "$REPO" && "${CLEAN_ENV[@]}" "$CMAKE" --build "$bdir" --target ember_core ember_host ember_ios_hub -j "$JOBS") \
    || { echo "error: native build failed for $sdk (if a core module is mid-edit, retry; EMBER_MODULES=capi builds the C API only)" >&2; exit 1; }
  local libs=()
  local n
  for n in libember_core.a libember_host.a libember_ios_hub.a; do
    local f; f="$(find "$bdir" -name "$n" -type f | head -n 1)"
    [ -n "$f" ] || { echo "error: $n not produced for $sdk" >&2; exit 1; }
    libs+=("$f")
  done
  local tmp="$bdir/$LIBNAME.tmp"
  rm -f "$tmp"
  DEVELOPER_DIR="$DEV_DIR" xcrun libtool -static -no_warning_for_no_symbols -o "$tmp" "${libs[@]}"
  # Only replace the copy when it changed so Xcode does not relink needlessly.
  if ! cmp -s "$tmp" "$OUT/$sdk/lib/$LIBNAME"; then mv "$tmp" "$OUT/$sdk/lib/$LIBNAME"; else rm -f "$tmp"; fi
  rm -f "$OUT/$sdk/lib/libember_core.a"   # pre-hub name; never link a stale copy
  echo "built $OUT/$sdk/lib/$LIBNAME ($archs, modules: $MODULES, $CONFIG)"
}

for p in $PLATFORMS; do build_one "$p"; done

if [ "$XCFRAMEWORK" = 1 ]; then
  HDR="$OUT/headers"
  rm -rf "$HDR" "$OUT/EmberCore.xcframework"
  mkdir -p "$HDR/ember"
  cp "$REPO"/core/include/ember/ember.h "$HDR/ember/"
  cp "$HERE"/native/ember_ios_hub.h "$HDR/"
  cat > "$HDR/module.modulemap" <<'EOF'
module CEmber {
  header "ember/ember.h"
  header "ember_ios_hub.h"
  link "c++"
  export *
}
EOF
  args=()
  for p in $PLATFORMS; do args+=(-library "$OUT/$p/lib/$LIBNAME" -headers "$HDR"); done
  xcodebuild -create-xcframework "${args[@]}" -output "$OUT/EmberCore.xcframework" >/dev/null
  echo "built $OUT/EmberCore.xcframework"
fi
