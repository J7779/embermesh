# Continuous integration and releases

Every push to `main` runs the workflows below on GitHub Actions; pull requests run the build, test and check workflows (not Release or Pages — see the table). They call
the same entry points a developer uses (`./build.sh`, and `build.ps1` on Windows), so a red job can
be reproduced locally with the command shown for it. No workflow needs a secret; the only optional
ones are the Android release key (see "Release signing").

| Workflow (badge name) | File | Jobs | Blocking | Produces |
|---|---|---|---|---|
| **Core** | `core.yml` | Linux GCC · Linux Clang ASan+UBSan · macOS Apple Clang · Windows MSVC · WebAssembly (wasm32 under Node) | all | test logs |
| **Desktop** | `desktop.yml` | host library + emberd + CTest on linux-x86_64, linux-arm64, macos-arm64, macos-x86_64, windows-x86_64 | all | `emberd-<platform>`, CTest JUnit |
| **Web** | `web.yml` | UI unit tests · build + headless-Chrome tests + smoke walks on Linux and macOS | all | `web-dist-linux`, `web-dist-macos`, logs, screenshots |
| **Pages** | `pages.yml` | deploy the site the Web workflow built and tested to GitHub Pages | – | the live site |
| **Android** | `android.yml` | JVM unit tests + debug/release APKs · instrumented tests on an emulator | emulator job is **not** | `android-apks`, test reports |
| **iOS** | `ios.yml` | simulator build + XCTest, unsigned device build | all | `.xcresult` bundle, logs |
| **Guides** | `guides.yml` | validate the corpus, build `guides.pack`, reproducibility, search checks, totals | all | `guides-pack`, reports |
| **Publish checks** | `checks.yml` | publishing gate + self-test · licences · required files and pins · actionlint/shellcheck/PowerShell/helper tests | all | gate logs |
| **Release** | `release.yml` | build every distributable, publish rolling `latest` or a versioned release | all | release assets |

Common rules: least-privilege `permissions` (read-only except the release publish job and the
Pages deploy job); on pull requests a newer push cancels the older run, on `main` nothing is ever
cancelled (one concurrency group per commit; Release and Pages serialise their publishing steps
without cancelling); every job has a timeout; matrices use `fail-fast: false`; every job writes a
Markdown summary (what was built, sizes, test counts per suite) and uploads its logs and reports
(14 days; release files 30 days). Each workflow ends with a **summary** job that fails if a
blocking job did not succeed, so the badge means "all blocking jobs passed".

## The workflows

### Core

The shared core alone (`-DEMBER_BUILD_DESKTOP=OFF -DEMBER_BUILD_TOOLS=OFF`), so a host or daemon
portability problem shows up in Desktop, not here. Each job builds `ember_tests` and runs the whole
doctest suite. The sanitizer job is Clang, Debug, `-DEMBER_SANITIZE=ON` with
`ASAN_OPTIONS=detect_leaks=1:abort_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1` (UBSan findings
fail the job). The WebAssembly job builds the same suite with Emscripten (`NODERAWFS`, so the
fixtures under `core/tests/data` load from the real file system) and runs it under Node.

### Desktop

`./build.sh desktop test --only native --strict`: builds core, guidepack, host library and emberd,
packs the guide library, stages `dist/desktop/`, then runs every CTest suite (core doctest,
`ember_host_tests`, `emberd_tests`, and on POSIX the multi-process `emberd_mesh.*` scenarios). Windows
runs `./build.ps1 desktop test` (Visual Studio generator, x64, `ctest -C Release`). The binary is
smoke-tested with `--version`/`--help` and uploaded.

### Web

- **UI unit tests**: `./build.sh test --only js --strict` (`node --test ui/tests/`).
- **Build + browser tests** (Linux and macOS): `./build.sh desktop web` (emberd is needed by the hub
  and remote tests), then `EMBER_REQUIRE_BROWSER=1 ./build.sh test --only web --strict`
  (`node --test apps/web/test/` in the runner's Google Chrome; a missing build or browser fails
  instead of skipping), then the UI smoke walks `node ui/tests/smoke_chrome.mjs --shots …` (mock
  backend, screenshots uploaded) and `--real auto` (the real WASM core). On Ubuntu 24.04 the job
  lifts the AppArmor restriction on unprivileged user namespaces that Chrome's sandbox needs.

### Pages

Triggered when the Web workflow completes successfully for a push to `main` (and by hand:
Actions → Pages → Run workflow redeploys the newest successful Web run). It downloads that run's
`web-dist-linux` artifact — the exact `dist/web` that was tested: UI, WASM core, `data/guides.pack`,
PWA manifest and service worker, and `embermesh.html` as a download — adds `.nojekyll`, and before
deploying checks:

- every reference from the site to its own files is relative (`tools/ci/check-relative-urls.py`:
  HTML attributes, CSS `url()`, manifest `id`/`start_url`/`scope`/icons, JS string literals naming
  the site's own files), because the app lives under `https://<owner>.github.io/<repository>/`;
- the GitHub Pages limits (`tools/ci/pages-limits.py`): the site must stay under 1 GB and, as a
  conservative per-file ceiling, every file under 100 MiB. Today: about 50 MiB in total (95 %
  margin), the largest file is `data/guides.pack` at about 45 MiB (55 % margin);
- the UI smoke walk against the real core with the site served from a sub-directory
  (`http://127.0.0.1:8765/<repository>/`), exactly as Pages serves it.

Then it deploys with the official `configure-pages` / `upload-pages-artifact` / `deploy-pages`
actions to the `github-pages` environment; the job summary and the environment show the URL.
Pages sends its own `Cache-Control: max-age=600` and does not allow custom headers; the app's
versioned service-worker caches keep the long-lived offline copy.

**One-time setup (a repository admin):** Settings → Pages → Build and deployment → Source:
**GitHub Actions**. The workflow cannot change this setting; until it is set, the first run fails
at "Configure Pages" and its summary says so. After the first deployment, also put the URL in the
repository's About box (gear icon → Website → "Use your GitHub Pages website").

### Android

`./build.sh guides android` (debug APK), `./build.sh test --only android --strict`
(`:app:testDebugUnitTest`), then `./gradlew :app:assembleRelease`. On pushes to `main` in a
repository that has the release-key secrets, the release build is signed with that key and named
`embermesh-android-release.apk` (see "Release signing"). Otherwise — pull requests, forks, a
repository without the secrets — it is signed with the debug key the Android tools create on the
runner, as `app/build.gradle.kts` does for every builder; that key is thrown away with the runner,
so the file is called `embermesh-android-release-ci-debug-signed.apk` and a later build cannot be
installed over it without uninstalling first. The job summary shows which key signed it and the
certificate's SHA-256 (`apksigner verify --print-certs`).

The **instrumented tests** (`:app:connectedDebugAndroidTest`, API 35 x86_64 Google APIs emulator
with KVM, emulated cameras) run as a separate, **non-blocking** job (`continue-on-error`, and the
summary job ignores it): hosted emulators are a well-known source of flaky failures (cold boots,
shared-runner KVM, emulated camera and microphone used by several of these tests), and the suite was
proven on an API 37 arm64 emulator locally, not on this image. It always runs and reports; make it
blocking by removing `continue-on-error` and the `--optional emulator` argument once it has been
green for a while.

### iOS

On `macos-26` with the newest stable Xcode installed (`tools/ci/select-xcode.sh`, which also makes
sure an iPhone simulator exists): `./build.sh guides ios` (simulator build and an unsigned
`generic/platform=iOS` device build with `CODE_SIGNING_ALLOWED=NO`), then
`./build.sh test --only ios --device-tests --strict` (the XCTest suite on the first available
iPhone simulator; xcodebuild boots it). Results come from the `.xcresult` bundle, which is
uploaded. No archive, no `.ipa`, no signing.

### Guides

`./build.sh guides` builds `guidepack` and the pack; `tools/ci/guides-report.sh` runs
`guidepack validate` (fails on any error, counts warnings), `guidepack stats` (guides, categories,
index terms, pack size, verification) and the search smoke checks in
`tools/ci/guides-search-checks.tsv` (a relevant guide in the top 5 for each query). A second
`guidepack build` must produce a byte-identical pack. The core's guides test suite
(`ember_tests -ts=guides`, including the ranking tests in `core/tests/guides/search_rank_test.cpp`)
is the search-quality check.

### Publish checks

- `tools/check-publishable.sh --self-test` and `./build.sh privacy` (see "The publishing gate on CI").
- `tools/ci/check-licences.sh`: root `LICENSE`; the licence text of every vendored component
  (Monocypher, doctest, miniaudio, ZXing); a `_sources.md` per guide category; `source`, `license`
  and `attribution` on every guide, with no NC/ND/all-rights-reserved licence.
- `tools/ci/check-required-files.sh`: the files the repository depends on exist and are not
  git-ignored (`git check-ignore`): `build.sh`, `build.ps1`, `LICENSE`, `AGENTS.md`, `CLAUDE.md`,
  `.claude/settings.json` and the playbooks `.claude/skills/*/SKILL.md`, the gate and its allow-lists,
  every workflow and composite action, the CI helpers, the Gradle wrapper jar and properties, the
  Xcode project and scheme, the encrypted-store test fixture, and `models/` once it exists.
- `tools/ci/check-pins.sh`: the Emscripten version in `.github/actions/setup-emsdk` equals
  `apps/web/README.md`; the NDK, SDK CMake and platform in `.github/actions/setup-android` equal
  `apps/android/app/build.gradle.kts`; every action is pinned to a commit SHA with a version comment.
- actionlint (with shellcheck on `run:` blocks), shellcheck `-S warning` on every shell script, a
  PowerShell parse of every `.ps1`, and `python3 -m unittest discover -s tools/ci/tests` (the
  report parsers against outputs captured from real runs, packaging, release notes, URL checks).

### Release

On every push to `main`, and on tags `v*`:

| Job | Output |
|---|---|
| guides | `guides.pack` |
| web | `embermesh-<v>-web.zip` (static site + PWA), `embermesh.html` (single file, no library), `embermesh-with-library.html` (single file with the library inlined, about 1.34 × the pack; dropped with a warning if it ever approaches the 2 GiB asset limit) |
| desktop × 5 | `embermesh-<v>-emberd-linux-x86_64.tar.gz`, `-linux-arm64.tar.gz`, `-macos-arm64.tar.gz`, `-macos-x86_64.tar.gz`, `-windows-x86_64.zip` |
| android | `embermesh-<v>-android-debug.apk`, and `embermesh-<v>-android-release.apk` (signed with the project's release key) or, without the release-key secrets, `embermesh-<v>-android-release-ci-debug-signed.apk` (throw-away key); all ABIs, hub serves the web build, library bundled |
| publish | `SHA256SUMS.txt`, the release notes, and the release itself |

Each desktop package holds `emberd` (or `emberd.exe`), `guides.pack` and `web/` next to it (emberd
finds both automatically), `deploy/` (systemd, launchd, Task Scheduler), `LICENSE`,
`THIRD_PARTY_LICENSES.md` when present, `licenses/` (Monocypher, miniaudio), `README.txt` and
`BUILDINFO.txt` (version, commit, workflow run). Archives are reproducible: sorted entries, time
stamps set to the commit time. The web job rebuilds the pack through `./build.sh web` and checks it
is byte-identical to the released `guides.pack`.

`<v>` is `project(EmberMesh VERSION …)` from `CMakeLists.txt` plus `-dev.<commit>` for `main`
builds, or the tag without its `v`.

**Where to download:**

- the rolling pre-release: `../../releases/tag/latest` (Releases → "latest"), replaced by every
  commit on `main`; only the newest commit may replace it (a slower, older run leaves it alone);
- versioned releases: Releases → `vX.Y.Z`;
- any run: Actions → Release → the run → Artifacts (`release-*`, and `embermesh-<v>` with
  everything in one zip), kept 30 days.

Verify downloads with `sha256sum -c SHA256SUMS.txt --ignore-missing`
(`shasum -a 256 -c …` on macOS).

## Cutting a versioned release

1. Set the version in the top-level `CMakeLists.txt`: `project(EmberMesh VERSION X.Y.Z …)` (the
   Android `versionName`/`versionCode` and every package follow it). Commit and push to `main`; wait
   for the workflows to be green.
2. Tag that commit and push the tag:
   ```sh
   git tag -a vX.Y.Z -m "EmberMesh X.Y.Z"
   git push origin vX.Y.Z
   ```
3. The Release workflow checks that the tag matches the CMake version (it fails otherwise), builds
   everything from the tag and publishes the release `vX.Y.Z` with the notes and checksums. A tag
   with a suffix (`vX.Y.Z-rc.1`) becomes a pre-release. Re-running the workflow for the same tag
   replaces the assets.

With the release-key secrets set (next section) the release APK is signed with the project's key,
so it installs as an update over earlier releases; without them it is signed with a throw-away CI
key and named `…-release-ci-debug-signed.apk`.

## Release signing

The Android release APK is signed with the project's own key when the repository has these
**secrets** (Settings → Secrets and variables → Actions → Secrets):

| Secret | Value |
|---|---|
| `ANDROID_KEYSTORE_BASE64` | the PKCS12 (or JKS) keystore, base64-encoded |
| `ANDROID_KEYSTORE_PASSWORD` | keystore password |
| `ANDROID_KEY_ALIAS` | key alias |
| `ANDROID_KEY_PASSWORD` | key password |

and, optionally, this **variable** (Variables tab; certificate digests are public):

| Variable | Value |
|---|---|
| `ANDROID_CERT_SHA256` | the signing certificate's SHA-256 (hex, colons optional). A release-key build whose certificate differs fails; with the variable set, a Release run without the secrets fails instead of shipping a throw-away-key APK |

Create the key as in `apps/android/README.md` ("Signing"), keep it and a backup outside the
repository, then (with `gh` logged in to the repository):

```sh
base64 < /path/to/release.p12 | tr -d '\n' | gh secret set ANDROID_KEYSTORE_BASE64
gh secret set ANDROID_KEYSTORE_PASSWORD      # prompts; paste the password
gh secret set ANDROID_KEY_ALIAS --body embermesh
gh secret set ANDROID_KEY_PASSWORD           # prompts
keytool -list -v -keystore /path/to/release.p12 -storetype PKCS12 | grep 'SHA256:'
gh variable set ANDROID_CERT_SHA256 --body '<SHA256 from keytool, with or without colons>'
```

How the workflows use them (Android `build` job and Release `android` job):

- Only runs on `main` (Android) or on `main` and `v*` tags (Release) ask for the secrets; pull
  requests never do (and GitHub does not give secrets to pull requests from forks).
- `tools/ci/android-signing.sh prepare` checks that all four are set (some but not all: the job
  fails), decodes the keystore into `$RUNNER_TEMP/android-signing/` (mode 600, never the
  workspace), and the four `EMBER_ANDROID_*` variables are set for the Gradle step only. Nothing is
  echoed; no third-party action receives them.
- `tools/ci/android-signing.sh cleanup` deletes the keystore afterwards (`if: always()`).
- `tools/ci/android-signing.sh verify` prints the certificate (`apksigner verify --print-certs`)
  into the summary and compares it with `ANDROID_CERT_SHA256` when that is set. The release notes
  say which key signed the APK and give the certificate SHA-256.

Locally the same four settings, as environment variables or Gradle properties, sign
`./gradlew :app:assembleRelease` (`apps/android/README.md`, "Signing").

## The publishing gate on CI

`tools/check-publishable.sh` also discovers the identifiers of the machine it runs on (user name,
home path, host name, git identity, network, simulators…). On a GitHub runner those are generic
words such as `runner`, so `--ci` (automatic when `CI=true`) skips that one rule. Every generic
rule stays active: home and temp paths of any user, the e-mail allow-list, credentials and tokens,
device ids, MAC and public IP addresses, process wording, forbidden files, binaries, image metadata
and large files, plus `EMBER_PUBLISH_EXTRA_IDS`. `--local` forces the normal mode. The self-test
covers both: in local mode it plants machine identifiers and also runs a `--ci` pass over the same
tree. Run the gate locally (normal mode) before publishing anything: `./build.sh privacy`.

Owner-approved public identity: the git `user.name` (the display name only) may appear in a
copyright statement — `Copyright (c) 2026 <name>`, `© 2026 <name>` — in the files listed as
`identity path` in `tools/publishable-allow.txt` (LICENSE, README.md, THIRD_PARTY_LICENSES.md,
CONTRIBUTING.md and the UI's About text), when that line carries no other identifier of the machine.
The same name anywhere else, or outside a copyright statement, and the e-mail address, its local
part, a GitHub handle, the OS user name, host name or paths in any line, still fail. The self-test
proves both sides.

## Reproducing a job locally

| Job | Command |
|---|---|
| Core (native) | `cmake -S . -B build/ci-core -DCMAKE_BUILD_TYPE=Release -DEMBER_BUILD_DESKTOP=OFF -DEMBER_BUILD_TOOLS=OFF && cmake --build build/ci-core --target ember_tests -j && build/ci-core/core/ember_tests` |
| Core (sanitizers) | the same with `-DCMAKE_BUILD_TYPE=Debug -DEMBER_SANITIZE=ON` (or `./build.sh test --sanitize --only native` for core + host) |
| Core (WebAssembly) | `tools/ci/core-wasm-tests.sh` (needs `.toolchains/emsdk`, see `apps/web/README.md`) |
| Desktop | `./build.sh desktop test --only native --strict`; Windows: `.\build.ps1 desktop test` |
| Web | `./build.sh desktop web && EMBER_REQUIRE_BROWSER=1 ./build.sh test --only js,web --strict && node ui/tests/smoke_chrome.mjs && node ui/tests/smoke_chrome.mjs --real auto` |
| Pages checks | `python3 tools/ci/check-relative-urls.py dist/web && python3 tools/ci/pages-limits.py dist/web`; sub-path smoke: copy `dist/web` to `build/pages-root/embermesh/`, `python3 -m http.server 8765 --directory build/pages-root`, then `node ui/tests/smoke_chrome.mjs --real http://127.0.0.1:8765/embermesh/` |
| Android | `./build.sh guides android && ./build.sh test --only android --strict && (cd apps/android && ./gradlew :app:assembleRelease)`; emulator: start one, then `./build.sh test --only android --device-tests` |
| iOS | `./build.sh guides ios && ./build.sh test --only ios --device-tests --strict` |
| Guides | `./build.sh guides && tools/ci/guides-report.sh build/host/tools/guidepack build/guides.pack && build/host/core/ember_tests -ts=guides` |
| Publish checks | `tools/check-publishable.sh --self-test && ./build.sh privacy && tools/ci/check-licences.sh && tools/ci/check-required-files.sh && tools/ci/check-pins.sh && python3 -m unittest discover -s tools/ci/tests` |
| Release packages | `python3 tools/ci/package.py desktop --platform macos-arm64 --version 0.1.0 --emberd dist/desktop/emberd --web dist/web --guides build/guides.pack --out build/release` |

`build.sh test` options used by CI: `--only native,privacy,js,web,android,ios` selects suites,
`--strict` turns a suite that cannot run (missing toolchain, no simulator) into a failure, and
`EMBER_TEST_RESULTS=DIR` also writes CTest JUnit XML (`ctest.xml`) and the iOS result bundle
(`ios.xcresult`) there. The summaries are produced by `tools/ci/report.py` (doctest, CTest, node,
JUnit and xcresult parsers).

## Pinned versions

| What | Version | Where |
|---|---|---|
| Linux runners | `ubuntu-24.04`, `ubuntu-24.04-arm` | all workflows |
| macOS runners | `macos-15` (arm64), `macos-15-intel` (x86-64); iOS on `macos-26` with its newest stable Xcode | Desktop, Core, Web, Release; iOS |
| Windows runner | `windows-2025`, MSVC through CMake's Visual Studio generator | Core, Desktop, Release |
| Emscripten | 6.0.13 (emsdk tag of the same name), cached in `.toolchains/emsdk` | `.github/actions/setup-emsdk` |
| Node | 26 (LTS) | `actions/setup-node` |
| JDK | Temurin 21 | `.github/actions/setup-android` |
| Android | platform `android-37.0`, build-tools 37.0.0 and 36.0.0, NDK 27.1.12297006, SDK CMake 3.22.1, Gradle from the wrapper (9.8.2), AGP 9.5.0 | `.github/actions/setup-android`, `apps/android` |
| Emulator | API 35, `google_apis`, x86_64 | Android |
| Chrome | the runner image's Google Chrome | Web, Pages |
| actionlint / shellcheck | 1.7.12 / 0.11.0, SHA-256 verified | Publish checks |

Actions (all pinned to the commit of the release tag named in the comment):

| Action | Version | Commit |
|---|---|---|
| actions/checkout | v7.0.1 | `3d3c42e5aac5ba805825da76410c181273ba90b1` |
| actions/setup-node | v7.1.0 | `949feb2413d6458794dcd2491c4babbbce0c15c1` |
| actions/setup-java | v6.3.0 | `d1374f8ac74474200d16ef8ecee22c7ae7a51880` |
| actions/cache | v6.1.0 | `55cc8345863c7cc4c66a329aec7e433d2d1c52a9` |
| actions/upload-artifact | v7.0.2 | `cf430e030ddbb5b0abf93d22962f4752f3646cd9` |
| actions/download-artifact | v8.0.2 | `9000827ccba6bdab643e8b6fd33ac0654aef8333` |
| gradle/actions/setup-gradle | v6.4.1 | `8da46b539cd8fe6ab27ca75128919a18d757604c` (open-source `basic` cache provider) |
| reactivecircus/android-emulator-runner | v2.38.0 | `a421e43855164a8197daf9d8d40fe71c6996bb0d` |
| actions/configure-pages | v6.0.0 | `45bfe0192ca1faeb007ade9deae92b16b8254a0d` |
| actions/upload-pages-artifact | v5.0.0 | `fc324d3547104276b827a68afc52ff2a11cc49c9` |
| actions/deploy-pages | v5.0.1 | `368f82528645a54fb793d4d04e342629a3f51346` |

Releases are published with the preinstalled `gh` CLI and the run's `GITHUB_TOKEN`.

Caches: emsdk (per OS/arch/version, about 1 GB), Gradle (saved on `main` only). The C++ builds are
not cached: a clean core build takes a few minutes and a stale cache would hide problems.

## Updating a pin

- Emscripten: change `apps/web/README.md` and the `version` default in
  `.github/actions/setup-emsdk/action.yml` together (check-pins fails otherwise).
- Android: change `apps/android/app/build.gradle.kts` and `.github/actions/setup-android/action.yml`
  together.
- An action: look up the commit of the new release tag in the action's repository
  (`git ls-remote https://github.com/<owner>/<action> refs/tags/<tag>`) and update the SHA and the
  `# vX.Y.Z` comment everywhere it is used.
