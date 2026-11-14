---
name: build-and-test
description: Build any EmberMesh target (core, guides, desktop emberd, web/WASM, Android, iOS) with ./build.sh and pick and run the right test suites, including sanitizers, device tests, headless-Chrome walks and the all-apps mesh test. Use when building, testing, verifying a change, or when a toolchain is missing or a build hangs.
---

# Build and test

All commands run from the repository root. `./build.sh --help` is the authoritative list.

## 1. What can this machine build?

```sh
./build.sh doctor
```

| Target | Output | Needs |
|---|---|---|
| `core` | `build/host/` (core, `guidepack`, host library, `emberd`, test binaries) | C++17 compiler, CMake >= 3.20 |
| `guides` | `build/guides.pack` | same |
| `desktop` | `dist/desktop/emberd` (+ `guides.pack`, `deploy/`) | same |
| `web` | `dist/web/`, `dist/web/embermesh.html`, `build/web/wasm/` | Emscripten 6.0.13 in `.toolchains/emsdk` (install: `apps/web/README.md` §1), Node >= 22 |
| `android` | `dist/android/*.apk` (debug) | Android SDK (platform 37, NDK 27.1.12297006, SDK CMake 3.22.1), JDK 17+ (`apps/android/README.md`) |
| `ios` | `dist/ios/simulator/`, `dist/ios/device/` (unsigned) | macOS + Xcode |
| `all` | all of the above that can be built here | `--strict` fails instead of skipping |

Options: `--debug`, `--sanitize` (ASan + UBSan, implies `--debug`, separate `build/host-sanitize`),
`--jobs N`, `--no-guides`, `--inline-guides` (web). Environment: `EMBER_CMAKE`, `JAVA_HOME`,
`ANDROID_HOME`, `EMSDK_DIR`, `IOS_SIMULATOR` (name or UDID), `EMBER_TEST_RESULTS=DIR`.
Toolchains go into the git-ignored `.toolchains/`, never system-wide. Windows (MSVC):
`.\build.ps1 desktop test` (targets `core`, `guides`, `desktop`, `test`, `clean`; `-Only native,privacy`).

## 2. Pick the suites for what you changed

| You changed | Run at least |
|---|---|
| `core/` | `./build.sh test --only native,privacy`, then `./build.sh test --sanitize --only native` |
| one core module, fast loop | single-module build below, then the full native run |
| `host/`, `apps/desktop/` | `./build.sh desktop test --only native --strict` |
| `ui/` | `node --test ui/tests/` and `node ui/tests/smoke_chrome.mjs` (playbook ui-change) |
| `apps/web/`, the WASM binding | `./build.sh desktop web && EMBER_REQUIRE_BROWSER=1 ./build.sh test --only js,web --strict` |
| `apps/android/` | `./build.sh guides android && ./build.sh test --only android --strict`; emulator below |
| `apps/ios/` | `./build.sh guides ios && ./build.sh test --only ios --device-tests --strict` |
| `guides/content/`, `tools/guidepack/` | playbook add-guide (validate, pack, search checks) |
| anything | `tools/check-publishable.sh` must print OK (playbook privacy-check) |

`./build.sh test` runs every suite this machine can run (native ctest, privacy, js, web, android,
ios) and prints pass/skip/FAIL per suite; `--only LIST` selects, `--strict` turns a skip into a
failure, `--device-tests` adds the Android emulator and iOS simulator suites.

## 3. Native details

```sh
# One module (separate build dir per configuration; list the module's dependencies first):
cmake -S . -B build/<dir> -DCMAKE_BUILD_TYPE=Debug -DEMBER_SANITIZE=ON \
      -DEMBER_MODULES="base;<deps>;<module>" -DEMBER_BUILD_DESKTOP=OFF -DEMBER_BUILD_TOOLS=OFF
cmake --build build/<dir> -j 8 && ./build/<dir>/core/ember_tests -ts=<module>

# After ./build.sh core: one doctest suite, or a subset of the CTest entries
build/host/core/ember_tests -ts=link
ctest --test-dir build/host -R emberd_mesh --output-on-failure    # multi-process mesh scenarios
```

`EMBER_TEST_LOGS=1` prints daemon logs; `EMBER_KEEP_TEST_DIRS=1` keeps their data directories
(they hold node identities: delete them, never commit them). Module dependencies are listed in
the module READMEs (for example `core/src/link/README.md` "Tests").

## 4. Browser, phones, all apps

```sh
node --test ui/tests/                         # UI unit tests, no build needed
node ui/tests/smoke_chrome.mjs [--shots DIR]  # every screen, mock backend, headless Chrome
node ui/tests/smoke_chrome.mjs --real auto    # same against the real WASM core (after ./build.sh web)
node ui/tests/boot_chrome.mjs                 # start-up animation
node ui/tests/a11y_audit.mjs --quick          # accessibility counts vs ui/tests/a11y_baseline.json
node --test apps/web/test/                    # web unit + end-to-end (needs ./build.sh desktop web)
node apps/desktop/tests/hub_guests_chrome.mjs # two browser guests + operator page against emberd
./build.sh test --only android --device-tests # start an emulator first (adb devices shows it)
node tools/e2e/all_together.mjs [--skip ios]  # hub + browser + Android + iOS on one mesh
```

`all_together.mjs` needs `./build.sh all`, an emulator with both APKs installed
(`cd apps/android && ./gradlew installDebug installDebugAndroidTest`) and an iPhone simulator; a
missing app you did not `--skip` is a FAIL. `--keep` keeps its temp dir (emberd data, logs): do not
commit anything from it.

## 5. Known quirks (generic)

- **A build that hangs at CMake**: some systems block access to parent folders of the checkout
  (privacy protection, a pending permission prompt). `build.sh` and `apps/ios/build-core.sh` probe
  each CMake with a timeout and skip one that hangs; point `EMBER_CMAKE` at one that works.
- **Moved or renamed checkout**: Gradle/CMake caches hold absolute paths. Run `./build.sh clean`.
- **"another build.sh is running"**: builds share `build/` and wait for each other's lock; a stale
  lock from a killed run is removed automatically. Do not delete `build/` under a running build.
- **Headless Chrome, adb and simulators** must be able to launch: they fail inside restrictive
  command sandboxes and some containers. `CHROME=/path` selects the browser.
- **`connectedDebugAndroidTest` uninstalls the app** afterwards (its data is gone).
- A suite reported **skip** did not run. Never report it as passed; say what was skipped and why.
- Do not edit generated copies (`apps/android/app/build/…`, `build/`, `dist/`); edit the source
  (`ui/`, `core/`, …) and rebuild.

## 6. Report

State the exact commands, the pass/skip/FAIL summary, and what could not run on this machine.
