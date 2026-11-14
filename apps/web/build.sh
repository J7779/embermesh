#!/usr/bin/env bash
# Builds the EmberMesh web app: WASM core + static site/PWA (dist/web/) + single file
# (dist/web/embermesh.html). Works offline once .toolchains/emsdk is installed (see README.md).
#
#   apps/web/build.sh [--guides PATH] [--inline-guides] [--modules "base;capi"] [--debug]
#                     [--skip-wasm] [--no-single] [--no-simd]
# Env: EMBER_GUIDES_PACK=PATH, EMBER_INLINE_GUIDES=1, EMBER_MODULES=..., EMSDK_DIR=...
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$ROOT/build/web"
EMSDK_DIR="${EMSDK_DIR:-$ROOT/.toolchains/emsdk}"
MODULES="${EMBER_MODULES:-all}"
BUILD_TYPE=MinSizeRel
SKIP_WASM=0
SIMD=1
PKG_ARGS=()
[[ -n "${EMBER_GUIDES_PACK:-}" ]] && PKG_ARGS+=(--guides "$EMBER_GUIDES_PACK")
[[ "${EMBER_INLINE_GUIDES:-0}" == 1 ]] && PKG_ARGS+=(--inline-guides)

while [[ $# -gt 0 ]]; do
  case "$1" in
    --guides) PKG_ARGS+=(--guides "$2"); shift 2 ;;
    --inline-guides) PKG_ARGS+=(--inline-guides); shift ;;
    --modules) MODULES="$2"; shift 2 ;;
    --debug) BUILD_TYPE=Debug; shift ;;
    --skip-wasm) SKIP_WASM=1; shift ;;
    --no-simd) SIMD=0; shift ;;
    --no-single) PKG_ARGS+=(--no-single); shift ;;
    -h|--help) sed -n '2,9p' "$0"; exit 0 ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done

if [[ $SKIP_WASM == 0 ]]; then
  if [[ ! -f "$EMSDK_DIR/emsdk_env.sh" ]]; then
    echo "Emscripten SDK not found at $EMSDK_DIR — see apps/web/README.md (Toolchain)." >&2
    exit 1
  fi
  # shellcheck disable=SC1091
  source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
  # No absolute source paths in the output (assert messages, __FILE__, DWARF of --debug builds): map
  # the repository root to ".".
  PREFIX_MAP="-ffile-prefix-map=$ROOT=. -fdebug-prefix-map=$ROOT=."
  configure_and_build() { # <build dir> <extra C/C++ flags> [cmake args...]
    local dir="$1" flags="$2"; shift 2
    local gen=()
    if [[ -f "$dir/CMakeCache.txt" ]]; then
      # Keep whatever generator the build dir was configured with (cmake refuses to switch).
      local prev; prev="$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "$dir/CMakeCache.txt")"
      [[ -n "$prev" ]] && gen=(-G "$prev")
    elif command -v ninja >/dev/null 2>&1; then
      gen=(-G Ninja)
    fi
    emcmake cmake -S "$ROOT" -B "$dir" ${gen[@]+"${gen[@]}"} \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DEMBER_MODULES="$MODULES" -DEMBER_BUILD_WEB=ON \
      -DCMAKE_C_FLAGS="$PREFIX_MAP $flags" -DCMAKE_CXX_FLAGS="$PREFIX_MAP $flags" "$@" >/dev/null
    cmake --build "$dir" --target ember_web -j "$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
  }
  # Baseline module (runs everywhere; also the single file's) ...
  configure_and_build "$BUILD_DIR" "" -DEMBER_WEB_VARIANT=
  # ... and a WebAssembly SIMD module for the site / PWA, picked at load time after a feature test
  # (ui/js/backends/wasm.js). --no-simd skips it.
  if [[ $SIMD == 1 ]]; then
    # -O3 (Release): measured, -msimd128 at -Os does not vectorise the search loops; see README §8
    SIMD_TYPE=Release; [[ $BUILD_TYPE == Debug ]] && SIMD_TYPE=Debug
    BUILD_TYPE=$SIMD_TYPE configure_and_build "$ROOT/build/web-simd" "-msimd128" -DEMBER_WEB_VARIANT=simd
    cp "$ROOT/build/web-simd/wasm/ember-simd.mjs" "$ROOT/build/web-simd/wasm/ember-simd.wasm" "$BUILD_DIR/wasm/"
  else
    rm -f "$BUILD_DIR/wasm/ember-simd.mjs" "$BUILD_DIR/wasm/ember-simd.wasm"
  fi
fi

# Node: prefer the one on PATH, fall back to the copy bundled with emsdk.
NODE="$(command -v node || true)"
if [[ -z "$NODE" ]]; then NODE="$(ls -d "$EMSDK_DIR"/node/*/bin/node 2>/dev/null | head -1)"; fi
"$NODE" "$ROOT/apps/web/tools/package.mjs" --wasm "$BUILD_DIR/wasm" --out "$ROOT/dist/web" "${PKG_ARGS[@]+"${PKG_ARGS[@]}"}"
