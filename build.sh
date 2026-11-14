#!/usr/bin/env bash
# EmberMesh — one build script for everything.
#
#   ./build.sh <target>... [options]
#
# Targets
#   doctor     show which toolchains were found and what can be built on this machine
#   core       build the shared C++ core + host tools (native)
#   guides     pack guides/content into build/guides.pack (bundled by every app)
#   desktop    build emberd (hub + radio gateway + terminal client)  -> dist/desktop/
#   web        build the WebAssembly app, PWA and single-file HTML   -> dist/web/
#   android    build the Android APK                                 -> dist/android/
#   ios        build the iOS app (simulator + unsigned device build) -> dist/ios/
#   all        guides + desktop + web + android + ios (skips what this machine cannot build)
#   test       run every automated test suite that can run on this machine (includes privacy)
#   privacy    check that nothing identifying the machine or its owner would be published
#   clean      remove build/, dist/ and Android build intermediates (toolchains in .toolchains/ are kept);
#              run it after moving or renaming the checkout
#
# Options
#   --debug          Debug build (default Release)
#   --sanitize       ASan + UBSan for native builds and tests (implies --debug)
#   --jobs N         parallel jobs (default: number of CPUs)
#   --device-tests   with `test`: also run Android emulator / iOS simulator tests (must be available)
#   --no-guides      do not bundle the guide library (smaller, faster builds)
#   --inline-guides  web: embed the guide library inside the single-file HTML
#   --strict         with `all`: fail instead of skipping targets whose toolchain is missing;
#                    with `test`: fail when a selected suite cannot run here
#   --only LIST      with `test`: run only these suites (comma-separated):
#                    native (core + host + emberd ctest), privacy, js (ui/tests), web, android, ios
#
# Environment overrides: EMBER_CMAKE, JAVA_HOME, ANDROID_HOME, EMSDK_DIR, IOS_SIMULATOR (name or UDID)
# EMBER_TEST_RESULTS=DIR (with `test`): also write ctest JUnit XML and the iOS .xcresult bundle there
#
# Examples
#   ./build.sh all                 everything this machine can build
#   ./build.sh android ios         just the phone apps
#   ./build.sh web --inline-guides one self-contained HTML file with the whole library inside
#   ./build.sh test --sanitize     core + host tests under sanitizers, plus the JS suites
#   ./build.sh test --only ios --device-tests   just the iOS simulator tests
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

BUILD_TYPE=Release
SANITIZE=OFF
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
DEVICE_TESTS=0
WITH_GUIDES=1
INLINE_GUIDES=0
STRICT=0
ONLY=""
TARGETS=()

usage() { sed -n '2,40p' "$0" | sed 's/^# \{0,1\}//'; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug) BUILD_TYPE=Debug ;;
    --sanitize) SANITIZE=ON; BUILD_TYPE=Debug ;;
    --jobs) shift; JOBS="${1:?--jobs needs a number}" ;;
    --jobs=*) JOBS="${1#*=}" ;;
    --device-tests) DEVICE_TESTS=1 ;;
    --no-guides) WITH_GUIDES=0 ;;
    --inline-guides) INLINE_GUIDES=1 ;;
    --strict) STRICT=1 ;;
    --only) shift; ONLY="${1:?--only needs a list of suites}" ;;
    --only=*) ONLY="${1#*=}" ;;
    -h|--help|help) usage; exit 0 ;;
    -*) echo "unknown option: $1 (try --help)" >&2; exit 2 ;;
    *) TARGETS+=("$1") ;;
  esac
  shift
done
[[ ${#TARGETS[@]} -eq 0 ]] && { usage; exit 2; }

# ---------------------------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------------------------
if [[ -t 1 ]]; then B=$'\033[1m'; G=$'\033[32m'; Y=$'\033[33m'; R=$'\033[31m'; N=$'\033[0m'; else B=; G=; Y=; R=; N=; fi
step() { echo "${B}==> $*${N}"; }
ok()   { echo "${G}ok${N}  $*"; }
warn() { echo "${Y}warning:${N} $*" >&2; }
die()  { echo "${R}error:${N} $*" >&2; exit 1; }

SUMMARY=()
note() { SUMMARY+=("$1"); }

# Run a command with a time limit without needing GNU timeout (not present on stock macOS).
with_timeout() { local secs="$1"; shift; perl -e 'alarm shift; exec @ARGV or exit 127' "$secs" "$@"; }

HOST_OS="$(uname -s)"
case "$HOST_OS" in MINGW*|MSYS*|CYGWIN*) EXE=.exe ;; *) EXE= ;; esac

HOST_BUILD="$ROOT/build/host"
[[ "$BUILD_TYPE" == Debug ]] && HOST_BUILD="$ROOT/build/host-debug"
[[ "$SANITIZE" == ON ]] && HOST_BUILD="$ROOT/build/host-sanitize"
GUIDES_PACK="$ROOT/build/guides.pack"

# --- CMake --------------------------------------------------------------------------------------
# Some CMake builds can hang when a parent directory of the repository is unreadable or is waiting
# on an OS permission prompt, so every candidate is probed from inside the repo with a time limit
# and the first one that answers is used for the whole build (it is also put first on PATH so the
# per-platform scripts pick it up).
CMAKE=""
CTEST=""
find_cmake() {
  [[ -n "$CMAKE" ]] && return 0
  local candidates=()
  [[ -n "${EMBER_CMAKE:-}" ]] && candidates+=("$EMBER_CMAKE")
  local onpath; onpath="$(command -v cmake 2>/dev/null || true)"
  [[ -n "$onpath" ]] && candidates+=("$onpath")
  [[ -x "$ROOT/.toolchains/bin/cmake" ]] && candidates+=("$ROOT/.toolchains/bin/cmake")
  local sdk; sdk="$(find_android_sdk || true)"
  if [[ -n "$sdk" && -d "$sdk/cmake" ]]; then
    local c
    for c in "$sdk"/cmake/*/bin/cmake; do [[ -x "$c" ]] && candidates+=("$c"); done
  fi
  local c rc
  mkdir -p "$ROOT/build"
  for c in ${candidates[@]+"${candidates[@]}"}; do
    # `--version` alone does not touch the source tree, so also make CMake resolve the -S/-B paths
    # the way a real configure does (-N only loads the cache; "no cache" is a fine, fast answer).
    # A probe that is killed by the alarm (status 142) means this CMake hangs on this machine.
    (cd "$ROOT" && with_timeout 20 "$c" --version >/dev/null 2>&1) || { warn "cmake at $c did not respond; trying the next one"; continue; }
    rc=0
    (cd "$ROOT" && with_timeout 30 "$c" -N -S "$ROOT" -B "$ROOT/build/.cmake-probe" >/dev/null 2>&1) || rc=$?
    if [[ $rc == 142 || $rc == 14 ]]; then
      warn "cmake at $c hangs while resolving this repository's path; trying the next one"
      continue
    fi
    CMAKE="$c"; break
  done
  rm -rf "$ROOT/build/.cmake-probe"
  [[ -n "$CMAKE" ]] || return 1
  # Shim directory so sub-scripts that call plain `cmake` / `ctest` get the working one.
  local shim="$ROOT/build/.shim"
  mkdir -p "$shim"
  local dir; dir="$(dirname "$CMAKE")"
  ln -sf "$CMAKE" "$shim/cmake"
  if [[ -x "$dir/ctest" ]]; then ln -sf "$dir/ctest" "$shim/ctest"; fi
  if [[ -x "$dir/ninja" ]] && ! command -v ninja >/dev/null 2>&1; then ln -sf "$dir/ninja" "$shim/ninja"; fi
  export PATH="$shim:$PATH"
  CTEST="$shim/ctest"; [[ -x "$CTEST" ]] || CTEST="$(command -v ctest || true)"
  return 0
}
need_cmake() { find_cmake || die "no working CMake (>= 3.20) found. Install CMake or set EMBER_CMAKE=/path/to/cmake."; }

# --- Android --------------------------------------------------------------------------------------
find_android_sdk() {
  local d
  for d in "${ANDROID_HOME:-}" "${ANDROID_SDK_ROOT:-}" "$HOME/Library/Android/sdk" "$HOME/Android/Sdk" \
           "${LOCALAPPDATA:-/nonexistent}/Android/Sdk"; do
    [[ -n "$d" && -d "$d/platforms" ]] && { echo "$d"; return 0; }
  done
  return 1
}
find_java_home() {
  if [[ -n "${JAVA_HOME:-}" && -x "$JAVA_HOME/bin/java" ]]; then echo "$JAVA_HOME"; return 0; fi
  local d
  for d in "/Applications/Android Studio.app/Contents/jbr/Contents/Home" \
           "$HOME/Applications/Android Studio.app/Contents/jbr/Contents/Home" \
           /opt/android-studio/jbr /usr/local/android-studio/jbr "$HOME/android-studio/jbr"; do
    [[ -x "$d/bin/java" ]] && { echo "$d"; return 0; }
  done
  if [[ -x /usr/libexec/java_home ]]; then
    d="$(/usr/libexec/java_home 2>/dev/null || true)"
    [[ -n "$d" && -x "$d/bin/java" ]] && { echo "$d"; return 0; }
  fi
  if command -v java >/dev/null 2>&1 && java -version >/dev/null 2>&1; then
    d="$(cd "$(dirname "$(command -v java)")/.." && pwd)"; echo "$d"; return 0
  fi
  return 1
}
can_android() { find_android_sdk >/dev/null && find_java_home >/dev/null && [[ -x "$ROOT/apps/android/gradlew" ]]; }

# --- iOS / Web ------------------------------------------------------------------------------------
can_ios() { [[ "$HOST_OS" == Darwin ]] && command -v xcodebuild >/dev/null 2>&1 && [[ -d "$ROOT/apps/ios/EmberMesh.xcodeproj" ]]; }
EMSDK_DIR="${EMSDK_DIR:-$ROOT/.toolchains/emsdk}"
can_web() { [[ -f "$EMSDK_DIR/emsdk_env.sh" && -f "$ROOT/apps/web/build.sh" ]]; }

ios_simulator_id() {
  local want="${IOS_SIMULATOR:-}"
  if [[ "$want" =~ ^[0-9A-Fa-f-]{36}$ ]]; then echo "$want"; return 0; fi
  local line
  if [[ -n "$want" ]]; then
    line="$(xcrun simctl list devices available 2>/dev/null | grep -F "    $want (" | head -1 || true)"
  else
    # Prefer a simulator that is already booted, otherwise the first available iPhone.
    line="$(xcrun simctl list devices available 2>/dev/null | grep -E 'iPhone .*\(Booted\)' | head -1 || true)"
    [[ -z "$line" ]] && line="$(xcrun simctl list devices available 2>/dev/null | grep -E '^ +iPhone ' | head -1 || true)"
  fi
  [[ -n "$line" ]] || return 1
  echo "$line" | grep -Eo '[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}' | head -1
}

# --- build lock -----------------------------------------------------------------------------------
# Two builds sharing build/ at the same time corrupt each other's archives. A second ./build.sh
# waits for the first instead (stale locks from killed runs are detected by pid and removed).
LOCK_DIR="$ROOT/build/.build.lock"
LOCK_HELD=0
acquire_lock() {
  [[ $LOCK_HELD == 1 || "${EMBER_BUILD_LOCKED:-}" == 1 ]] && return 0
  mkdir -p "$ROOT/build"
  local waited=0 owner
  while ! mkdir "$LOCK_DIR" 2>/dev/null; do
    owner="$(cat "$LOCK_DIR/pid" 2>/dev/null || true)"
    if [[ -n "$owner" ]] && ! kill -0 "$owner" 2>/dev/null; then
      rm -rf "$LOCK_DIR"; continue          # owner is gone: stale lock
    fi
    if [[ $waited == 0 ]]; then echo "another build.sh (pid ${owner:-?}) is running; waiting for it to finish..."; fi
    sleep 2; waited=$((waited + 2))
    [[ $waited -ge 3600 ]] && die "gave up waiting for the build lock ($LOCK_DIR)"
  done
  echo $$ > "$LOCK_DIR/pid"
  LOCK_HELD=1
  export EMBER_BUILD_LOCKED=1               # nested invocations (sub-scripts) do not re-lock
  trap 'rm -rf "$LOCK_DIR"' EXIT
}

# ---------------------------------------------------------------------------------------------
# targets
# ---------------------------------------------------------------------------------------------
DID_CONFIGURE=0
configure_host() {
  [[ $DID_CONFIGURE == 1 ]] && return 0
  need_cmake
  step "Configuring native build ($BUILD_TYPE$([[ $SANITIZE == ON ]] && echo ', sanitizers')) in ${HOST_BUILD#$ROOT/}"
  "$CMAKE" -S "$ROOT" -B "$HOST_BUILD" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DEMBER_SANITIZE="$SANITIZE" \
    -DEMBER_BUILD_TESTS=ON -DEMBER_BUILD_TOOLS=ON -DEMBER_BUILD_DESKTOP=ON >/dev/null
  DID_CONFIGURE=1
}

DID_CORE=0
do_core() {
  [[ $DID_CORE == 1 ]] && return 0
  configure_host
  step "Building core, tools, host library and desktop daemon"
  "$CMAKE" --build "$HOST_BUILD" -j "$JOBS"
  DID_CORE=1
  ok "native build: ${HOST_BUILD#$ROOT/}"
}

DID_GUIDES=0
do_guides() {
  [[ $DID_GUIDES == 1 || $WITH_GUIDES == 0 ]] && return 0
  configure_host   # only the packer and the core are compiled for this target, not the daemon
  step "Packing the offline guide library"
  "$CMAKE" --build "$HOST_BUILD" --target guides_pack -j "$JOBS" >/dev/null
  local built="$HOST_BUILD/guides/guides.pack"
  [[ -f "$built" ]] || built="$(find "$HOST_BUILD" -name guides.pack -type f 2>/dev/null | head -1)"
  [[ -n "$built" && -f "$built" ]] || die "guides_pack target did not produce guides.pack"
  cp "$built" "$GUIDES_PACK"
  DID_GUIDES=1
  local n; n="$(find "$ROOT/guides/content" -name '*.md' -not -name '_*' 2>/dev/null | wc -l | tr -d ' ')"
  ok "guides.pack: $(du -h "$GUIDES_PACK" | cut -f1 | tr -d ' ') from $n guide files"
  note "guides   build/guides.pack"
}

do_desktop() {
  do_core
  [[ $WITH_GUIDES == 1 ]] && do_guides
  local bin; bin="$(find "$HOST_BUILD" -type f -name "emberd$EXE" 2>/dev/null | head -1)"
  [[ -n "$bin" ]] || die "emberd was not built (is apps/desktop present?)"
  step "Staging dist/desktop"
  mkdir -p "$ROOT/dist/desktop"
  # Replace, never overwrite in place: macOS kills a signed binary whose file changed under its
  # cached code signature ("Killed: 9" on the next start).
  rm -f "$ROOT/dist/desktop/emberd$EXE"
  cp "$bin" "$ROOT/dist/desktop/"
  [[ -f "$GUIDES_PACK" && $WITH_GUIDES == 1 ]] && cp "$GUIDES_PACK" "$ROOT/dist/desktop/guides.pack"
  [[ -d "$ROOT/apps/desktop/deploy" ]] && { rm -rf "$ROOT/dist/desktop/deploy"; cp -R "$ROOT/apps/desktop/deploy" "$ROOT/dist/desktop/deploy"; }
  ok "dist/desktop/emberd$EXE  (serves dist/web if present, otherwise ui/)"
  note "desktop  dist/desktop/emberd$EXE   run it, then open http://localhost:8437/"
}

do_web() {
  can_web || die "Emscripten SDK not found at $EMSDK_DIR — see apps/web/README.md (Toolchain) to install it into .toolchains/emsdk"
  need_cmake
  local args=()
  if [[ $WITH_GUIDES == 1 ]]; then
    do_guides
    args+=(--guides "$GUIDES_PACK")
    [[ $INLINE_GUIDES == 1 ]] && args+=(--inline-guides)
  fi
  [[ "$BUILD_TYPE" == Debug ]] && args+=(--debug)
  step "Building the WebAssembly app"
  EMSDK_DIR="$EMSDK_DIR" bash "$ROOT/apps/web/build.sh" ${args[@]+"${args[@]}"}
  ok "dist/web/ (static site + PWA) and dist/web/embermesh.html (single file)"
  note "web      dist/web/               serve with any static server, or open dist/web/embermesh.html from disk"
}

do_android() {
  local sdk java
  sdk="$(find_android_sdk)" || die "Android SDK not found (set ANDROID_HOME)"
  java="$(find_java_home)" || die "no JDK found (set JAVA_HOME; Android Studio ships one)"
  [[ $WITH_GUIDES == 1 ]] && do_guides
  step "Building the Android app (JDK: $java)"
  local gradle_args=(":app:assembleDebug" "--console=plain" "-q")
  [[ $WITH_GUIDES == 1 && -f "$GUIDES_PACK" ]] && gradle_args+=("-PemberGuidesPack=$GUIDES_PACK")
  (cd "$ROOT/apps/android" && JAVA_HOME="$java" ANDROID_HOME="$sdk" ANDROID_SDK_ROOT="$sdk" ./gradlew "${gradle_args[@]}")
  mkdir -p "$ROOT/dist/android"
  local apk found=0
  while IFS= read -r apk; do
    cp "$apk" "$ROOT/dist/android/"; found=1
    ok "dist/android/$(basename "$apk")  ($(du -h "$apk" | cut -f1 | tr -d ' '))"
  done < <(find "$ROOT/apps/android/app/build/outputs/apk/debug" -name '*.apk' 2>/dev/null)
  [[ $found == 1 ]] || die "Gradle finished but no APK was produced"
  note "android  dist/android/*.apk      install with: adb install -r dist/android/<apk>   (or copy to the phone and open it)"
}

do_ios() {
  can_ios || die "iOS builds need macOS with Xcode"
  need_cmake
  [[ $WITH_GUIDES == 1 ]] && do_guides
  local cfg=Release; [[ "$BUILD_TYPE" == Debug ]] && cfg=Debug
  local dd="$ROOT/build/ios/DerivedData"
  local common=(-project "$ROOT/apps/ios/EmberMesh.xcodeproj" -scheme EmberMesh -configuration "$cfg" -quiet)
  [[ $WITH_GUIDES == 1 && -f "$GUIDES_PACK" ]] && common+=("EMBER_GUIDES_PACK=$GUIDES_PACK")
  mkdir -p "$ROOT/dist/ios"

  step "Building the iOS app for the simulator"
  xcodebuild "${common[@]}" -destination 'generic/platform=iOS Simulator' -derivedDataPath "$dd" build
  local app; app="$(find "$dd/Build/Products" -maxdepth 2 -name 'EmberMesh.app' -path '*iphonesimulator*' 2>/dev/null | head -1)"
  if [[ -n "$app" ]]; then
    rm -rf "$ROOT/dist/ios/simulator"; mkdir -p "$ROOT/dist/ios/simulator"; cp -R "$app" "$ROOT/dist/ios/simulator/"
    ok "dist/ios/simulator/EmberMesh.app"
  fi

  step "Building the iOS app for devices (unsigned)"
  xcodebuild "${common[@]}" -destination 'generic/platform=iOS' -derivedDataPath "$dd-device" CODE_SIGNING_ALLOWED=NO build
  app="$(find "$dd-device/Build/Products" -maxdepth 2 -name 'EmberMesh.app' -path '*iphoneos*' 2>/dev/null | head -1)"
  if [[ -n "$app" ]]; then
    rm -rf "$ROOT/dist/ios/device"; mkdir -p "$ROOT/dist/ios/device"; cp -R "$app" "$ROOT/dist/ios/device/"
    ok "dist/ios/device/EmberMesh.app (unsigned — sign with your own team to install; see apps/ios/README.md)"
  fi
  note "ios      dist/ios/simulator/EmberMesh.app   install with: xcrun simctl install booted dist/ios/simulator/EmberMesh.app"
}

run_suite() {  # run_suite <name> <command...>
  local name="$1"; shift
  step "Tests: $name"
  if "$@"; then ok "$name"; TEST_PASS+=("$name"); else warn "$name FAILED"; TEST_FAIL+=("$name"); fi
}
want() { [[ -z "$ONLY" || ",$ONLY," == *",$1,"* ]]; }   # want <suite>: selected by --only (default: all)

do_test() {
  TEST_PASS=(); TEST_FAIL=(); TEST_SKIP=()
  local results="${EMBER_TEST_RESULTS:-}" s
  for s in ${ONLY//,/ }; do case "$s" in native|privacy|js|web|android|ios) ;; *) die "--only: unknown suite '$s' (native, privacy, js, web, android, ios)" ;; esac; done
  [[ -n "$results" ]] && { mkdir -p "$results"; results="$(cd "$results" && pwd)"; }
  if want native; then
    do_core
    local junit=(); [[ -n "$results" ]] && junit=(--output-junit "$results/ctest.xml")
    run_suite "core + host (ctest)" "$CTEST" --test-dir "$HOST_BUILD" --output-on-failure -j "$JOBS" ${junit[@]+"${junit[@]}"}
  fi
  want privacy && run_suite "privacy (publishable files)" bash "$ROOT/tools/check-publishable.sh"

  if ! want js && ! want web; then
    :
  elif command -v node >/dev/null 2>&1; then
    [[ -d "$ROOT/ui/tests" ]] && want js && run_suite "shared UI (node --test)" bash -c "cd '$ROOT' && node --test ui/tests/"
    if [[ -d "$ROOT/apps/web/test" ]] && want web; then
      if [[ -f "$ROOT/build/web/wasm/ember.mjs" || -d "$ROOT/dist/web" ]]; then
        run_suite "web platform (node --test)" bash -c "cd '$ROOT' && node --test apps/web/test/"
      else
        TEST_SKIP+=("web platform (run ./build.sh web first)")
      fi
    fi
  else
    TEST_SKIP+=("JavaScript suites (node not found)")
  fi

  if ! want android; then
    :
  elif can_android; then
    local sdk java; sdk="$(find_android_sdk)"; java="$(find_java_home)"
    run_suite "Android unit tests" bash -c "cd '$ROOT/apps/android' && JAVA_HOME='$java' ANDROID_HOME='$sdk' ./gradlew :app:testDebugUnitTest --console=plain -q"
    if [[ $DEVICE_TESTS == 1 ]]; then
      if "$sdk/platform-tools/adb" devices 2>/dev/null | grep -qE '\sdevice$'; then
        run_suite "Android emulator/device tests" bash -c "cd '$ROOT/apps/android' && JAVA_HOME='$java' ANDROID_HOME='$sdk' ./gradlew :app:connectedDebugAndroidTest --console=plain -q"
      else
        TEST_SKIP+=("Android emulator tests (no device/emulator connected — start one first)")
      fi
    fi
  else
    TEST_SKIP+=("Android (SDK or JDK not found)")
  fi

  if ! want ios; then
    :
  elif ! can_ios; then
    [[ -n "$ONLY" ]] && TEST_SKIP+=("iOS (needs macOS with Xcode)")
  else
    if [[ $DEVICE_TESTS == 1 ]]; then
      local sim xcres=()
      if [[ -n "$results" ]]; then rm -rf "$results/ios.xcresult"; xcres=(-resultBundlePath "$results/ios.xcresult"); fi
      if sim="$(ios_simulator_id)" && [[ -n "$sim" ]]; then
        run_suite "iOS simulator tests" xcodebuild -project "$ROOT/apps/ios/EmberMesh.xcodeproj" -scheme EmberMesh \
          -destination "platform=iOS Simulator,id=$sim" -derivedDataPath "$ROOT/build/ios/DerivedData" -quiet ${xcres[@]+"${xcres[@]}"} test
      else
        TEST_SKIP+=("iOS simulator tests (no simulator available)")
      fi
    else
      TEST_SKIP+=("iOS simulator tests (pass --device-tests)")
    fi
  fi

  echo
  echo "${B}Test summary${N}"
  local s
  for s in ${TEST_PASS[@]+"${TEST_PASS[@]}"}; do echo "  ${G}pass${N}  $s"; done
  for s in ${TEST_SKIP[@]+"${TEST_SKIP[@]}"}; do echo "  ${Y}skip${N}  $s"; done
  for s in ${TEST_FAIL[@]+"${TEST_FAIL[@]}"}; do echo "  ${R}FAIL${N}  $s"; done
  [[ ${#TEST_FAIL[@]} -eq 0 ]] || exit 1
  [[ $STRICT == 1 && ${#TEST_SKIP[@]} -gt 0 ]] && die "--strict: ${#TEST_SKIP[@]} selected suite(s) could not run"
  return 0
}

do_privacy() {
  [[ -f "$ROOT/tools/check-publishable.sh" ]] || die "tools/check-publishable.sh not found"
  step "Checking that nothing identifying would be published"
  bash "$ROOT/tools/check-publishable.sh" || die "privacy check failed (see findings above)"
}

do_doctor() {
  echo "${B}EmberMesh build environment${N}  ($HOST_OS $(uname -m))"
  if find_cmake; then echo "  cmake     $CMAKE ($("$CMAKE" --version | head -1 | sed 's/cmake version //'))"; else echo "  cmake     ${R}not found / not responding${N}"; fi
  local cxx; cxx="$(command -v c++ || command -v clang++ || command -v g++ || true)"
  echo "  C++       ${cxx:-${R}not found${N}}"
  echo "  node      $(command -v node || echo 'not found (needed for web packaging and JS tests)')"
  if can_web; then echo "  emsdk     $EMSDK_DIR"; else echo "  emsdk     not installed -> web target unavailable (see apps/web/README.md)"; fi
  local sdk java
  if sdk="$(find_android_sdk)"; then echo "  android   $sdk"; else echo "  android   SDK not found -> android target unavailable"; fi
  if java="$(find_java_home)"; then echo "  java      $java"; else echo "  java      not found -> android target unavailable"; fi
  if can_ios; then echo "  xcode     $(xcodebuild -version 2>/dev/null | tr '\n' ' ')"; else echo "  xcode     not available -> ios target unavailable (needs macOS + Xcode)"; fi
  echo "  guides    $(find "$ROOT/guides/content" -name '*.md' -not -name '_*' 2>/dev/null | wc -l | tr -d ' ') files in guides/content"
}

do_all() {
  local t
  [[ $WITH_GUIDES == 1 ]] && do_guides
  do_desktop
  for t in web android ios; do
    if "can_$t"; then "do_$t"
    elif [[ $STRICT == 1 ]]; then die "$t toolchain not available (--strict)"
    else warn "skipping $t: toolchain not available on this machine (./build.sh doctor for details)"; note "$t      SKIPPED (toolchain not available)"
    fi
  done
}

for t in "${TARGETS[@]}"; do
  case "$t" in doctor|clean|privacy) ;; *) acquire_lock ;; esac
  case "$t" in
    doctor)  do_doctor ;;
    core)    do_core ;;
    guides)  do_guides ;;
    desktop) do_desktop ;;
    web)     do_web ;;
    android) do_android ;;
    ios)     do_ios ;;
    all)     do_all ;;
    test)    do_test ;;
    privacy) do_privacy ;;
    clean)   step "Removing build/, dist/ and the Android build intermediates"
             # The Gradle/CMake intermediates under apps/android cache absolute paths: remove them too,
             # so a checkout that was moved or renamed builds again after `clean`.
             rm -rf "$ROOT/build" "$ROOT/dist" "$ROOT/apps/android/app/.cxx" "$ROOT/apps/android/app/build" \
                    "$ROOT/apps/android/build" "$ROOT/apps/android/.gradle" "$ROOT/apps/android/.kotlin"
             ok "clean" ;;
    *) die "unknown target: $t (try --help)" ;;
  esac
done

if [[ ${#SUMMARY[@]} -gt 0 ]]; then
  echo
  echo "${B}Built${N}"
  for s in "${SUMMARY[@]}"; do echo "  $s"; done
fi
