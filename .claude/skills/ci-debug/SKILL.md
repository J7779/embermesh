---
name: ci-debug
description: Diagnose a failing EmberMesh GitHub Actions run - read the run summary and logs, reproduce the job locally with the same ./build.sh command, use the tools/ci helpers, understand pinned versions and what hosted runners do and do not exercise, and update a pin safely. Use when a workflow is red, a badge fails, a CI helper or its tests change, or a toolchain or action version must be bumped.
---

# Debugging CI

Reference: `docs/CI.md` (every workflow, job, artifact, pin, and "Reproducing a job locally").

## 1. Read the run

```sh
gh run list --limit 10                        # recent runs and their status
gh run view <run-id>                          # jobs; the failing step
gh run view <run-id> --log-failed             # only the failed steps' logs
gh run download <run-id> -n <artifact>        # logs, reports, screenshots (kept 14 days)
```

Each job writes a Markdown summary (what was built, test counts per suite) and every workflow ends
with a `summary` job that fails when a blocking job failed: open the job it names, not the
summary job. The Android emulator job is **non-blocking** (`continue-on-error`): it can be red
while the badge is green.

## 2. Reproduce locally with the same command

| Workflow | Command |
|---|---|
| Core | `cmake -S . -B build/ci-core -DCMAKE_BUILD_TYPE=Release -DEMBER_BUILD_DESKTOP=OFF -DEMBER_BUILD_TOOLS=OFF && cmake --build build/ci-core --target ember_tests -j && build/ci-core/core/ember_tests`; sanitizers: `./build.sh test --sanitize --only native`; WebAssembly: `tools/ci/core-wasm-tests.sh` |
| Desktop | `./build.sh desktop test --only native --strict` (Windows: `.\build.ps1 desktop test`) |
| Web | `./build.sh desktop web && EMBER_REQUIRE_BROWSER=1 ./build.sh test --only js,web --strict && node ui/tests/smoke_chrome.mjs && node ui/tests/smoke_chrome.mjs --real auto` |
| Pages | `python3 tools/ci/check-relative-urls.py dist/web && python3 tools/ci/pages-limits.py dist/web`, sub-path smoke as in docs/CI.md |
| Android | `./build.sh guides android && ./build.sh test --only android --strict && (cd apps/android && ./gradlew :app:assembleRelease)` |
| iOS | `./build.sh guides ios && ./build.sh test --only ios --device-tests --strict` |
| Guides | `./build.sh guides && tools/ci/guides-report.sh build/host/tools/guidepack build/guides.pack && build/host/core/ember_tests -ts=guides` |
| Publish checks | `tools/check-publishable.sh --self-test && ./build.sh privacy && tools/ci/check-licences.sh && tools/ci/check-required-files.sh && tools/ci/check-pins.sh && python3 -m unittest discover -s tools/ci/tests` |

`CI=true` (set on runners) puts the publishing gate in `--ci` mode; locally run it without, so
your own identifiers are checked too. `EMBER_TEST_RESULTS=DIR` writes the JUnit/xcresult files the
CI summaries are made from (`tools/ci/report.py`).

## 3. The helpers in `tools/ci/`

`report.py` (summaries from doctest, CTest, node, JUnit, xcresult output), `needs-summary.py`
(the final summary job), `package.py` and `release-notes.py` (Release), `android-signing.sh`
(release key: prepare, verify, cleanup), `select-xcode.sh` (newest stable Xcode and a simulator),
`check-*.sh`/`check-relative-urls.py`/`pages-limits.py` (publish checks), `guides-report.sh` +
`guides-search-checks.tsv` (Guides), `core-wasm-tests.sh` (Core WASM). Their tests:
`python3 -m unittest discover -s tools/ci/tests`, with captured real outputs in
`tools/ci/tests/samples/` (`.txt` files, never `.log`, which the gate refuses). When you change a
parser, add a sample of the real output that broke it.

## 4. What hosted runners exercise — and what they do not

They build and test every target on Linux, macOS and Windows, run the WASM core under Node and
in Chrome, the Android unit tests and an x86_64 emulator (non-blocking), and the iOS simulator. They
do **not** exercise real radios, TNCs, LoRa boards, SDRs, sound cards, serial adapters,
Bluetooth between phones, real cameras or microphones, or Windows serial and audio. A green run
says nothing about hardware; do not claim otherwise.

## 5. Common causes

- Works locally, fails on CI: a missing file that is git-ignored (`tools/ci/check-required-files.sh`
  lists the ones the repo depends on; `git check-ignore -v <file>`), an untracked local fixture,
  or a toolchain newer locally than the pin.
- Publish checks: a new finding of the gate (playbook privacy-check), a `uses:` without a full SHA,
  emsdk or Android versions that differ between `.github/actions/*/action.yml` and the build files.
- Browser jobs: Chrome sandbox restrictions (the Ubuntu job lifts the AppArmor user-namespace
  limit), timeouts in e2e watchdogs (`EMBER_E2E_WATCHDOG_MS`), a missing `./build.sh web` output.
- Pages: Source not set to "GitHub Actions" (playbook fork-setup), an absolute URL in `dist/web`.
- Flaky emulator: re-run the job; it is non-blocking by design.

## 6. Updating a pin

- Emscripten: `apps/web/README.md` and the `version` default in `.github/actions/setup-emsdk/action.yml`
  together. Android: `apps/android/app/build.gradle.kts` and `.github/actions/setup-android/action.yml`
  together. `tools/ci/check-pins.sh` fails otherwise.
- An action: `git ls-remote https://github.com/<owner>/<action> refs/tags/<tag>`, then update the
  SHA and the `# vX.Y.Z` comment everywhere it is used, and the table in `docs/CI.md`.
- Keep `permissions` least-privilege; run actionlint/shellcheck (`docs/CI.md` "Publish checks").
- Update `AGENTS.md` and the playbooks if a command, path or version they name changed.
