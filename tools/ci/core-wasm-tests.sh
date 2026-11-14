#!/usr/bin/env bash
# Builds the core doctest suite (ember_tests) as wasm32 with Emscripten and runs it under Node.
#
#   tools/ci/core-wasm-tests.sh [build-dir] [-- doctest args...]
#
# Uses the same Emscripten SDK as the web app (EMSDK_DIR, default .toolchains/emsdk; see
# apps/web/README.md). NODERAWFS gives the tests the real file system, so the fixtures under
# core/tests/data load exactly as in the native build. Node: `node` on PATH (>= 22), else the
# copy bundled with emsdk. Exit status is the test binary's.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="$ROOT/build/core-wasm"
if [[ $# -gt 0 && "$1" != "--" ]]; then BUILD="$1"; shift; fi
[[ "${1:-}" == "--" ]] && shift
EMSDK_DIR="${EMSDK_DIR:-$ROOT/.toolchains/emsdk}"
[[ -f "$EMSDK_DIR/emsdk_env.sh" ]] || { echo "Emscripten SDK not found at $EMSDK_DIR (see apps/web/README.md)" >&2; exit 1; }
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1

GEN=()
if [[ -f "$BUILD/CMakeCache.txt" ]]; then
  PREV_GEN="$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "$BUILD/CMakeCache.txt")"
  [[ -n "$PREV_GEN" ]] && GEN=(-G "$PREV_GEN")
elif command -v ninja >/dev/null 2>&1; then
  GEN=(-G Ninja)
fi
emcmake cmake -S "$ROOT" -B "$BUILD" ${GEN[@]+"${GEN[@]}"} -DCMAKE_BUILD_TYPE=Release \
  -DEMBER_BUILD_TESTS=ON -DEMBER_BUILD_TOOLS=OFF -DEMBER_BUILD_DESKTOP=OFF -DEMBER_BUILD_WEB=OFF \
  "-DCMAKE_EXE_LINKER_FLAGS=-sALLOW_MEMORY_GROWTH=1 -sNODERAWFS=1 -sSTACK_SIZE=8388608 -sMAXIMUM_MEMORY=4GB" >/dev/null
cmake --build "$BUILD" --target ember_tests -j "$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

NODE="$(command -v node || true)"
[[ -n "$NODE" ]] || NODE="$(ls -d "$EMSDK_DIR"/node/*/bin/node 2>/dev/null | head -1)"
echo "running $BUILD/core/ember_tests.js under $("$NODE" --version)"
cd "$BUILD/core"
exec "$NODE" ember_tests.js "$@"
