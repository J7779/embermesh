---
id: embermesh/building-from-source
title: "EmberMesh 32: Building EmberMesh from Source"
category: embermesh
tags: [embermesh, build, source, compile, build.sh, cmake, emscripten, android, ios, raspberry pi, toolchain, tests, offline]
summary: "For whoever inherits the code: rebuilding every EmberMesh app from this repository with ./build.sh, its targets and options, the toolchains each target needs, where the results go, and how to keep everything rebuildable without internet."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 32
---

# Building EmberMesh from Source

Everything in EmberMesh (the mesh core, the computer hub `emberd`, the web app, the Android and
iPhone apps and this guide library) is built from one repository with one script, `./build.sh`, run
from the repository's top folder. This chapter is for whoever has to rebuild or fix it.

## Fastest path

```sh
./build.sh doctor        # what toolchains were found, what this machine can build
./build.sh all           # everything this machine can build (skips the rest, with a note)
./build.sh test          # run every test suite this machine can run
```

At the end the script prints a summary of what it built and where.

## Targets

`./build.sh <target>... [options]` (several targets may be given at once):

| Target | Builds | Result |
|---|---|---|
| `doctor` | Nothing: shows which toolchains were found and what can be built | |
| `core` | The shared C++ core, host tools and host library (native) | `build/host/` |
| `guides` | Packs `guides/content` into the guide library every app bundles | `build/guides.pack` |
| `desktop` | `emberd` (hub, radio gateway, terminal client), plus the guide pack and the deploy scripts | `dist/desktop/` |
| `web` | The WebAssembly app, the installable web app and the single-file HTML | `dist/web/`, `dist/web/embermesh.html` |
| `android` | The Android APK | `dist/android/` |
| `ios` | The iOS app: simulator build and an unsigned device build | `dist/ios/simulator/`, `dist/ios/device/` |
| `all` | guides + desktop + web + android + ios, skipping targets whose toolchain is missing | all of the above |
| `test` | Every automated test suite that can run here | report |
| `clean` | Removes `build/` and `dist/` (toolchains in `.toolchains/` are kept) | |

**Order matters for phone hubs:** the Android app bundles `dist/web` so that a phone hub can serve
the full web app to guests. Build `web` before `android` (`all` does this). Without it, a phone
hub's guests see the interface but cannot run a node.

## Options

| Option | Effect |
|---|---|
| `--debug` | Debug build (default: Release); native builds go to `build/host-debug/` |
| `--sanitize` | AddressSanitizer + UndefinedBehaviorSanitizer for native builds and tests (implies `--debug`; `build/host-sanitize/`) |
| `--jobs N` | Parallel jobs (default: number of processors) |
| `--device-tests` | With `test`: also run the Android emulator / iOS simulator tests (they must be available) |
| `--no-guides` | Do not bundle the guide library (smaller, faster builds) |
| `--inline-guides` | `web`: put the whole guide library inside the single-file HTML (about 80.4 MiB instead of about 3.2 MiB) |
| `--strict` | With `all`: fail instead of skipping targets whose toolchain is missing |

Environment overrides: `EMBER_CMAKE` (path to a working CMake), `JAVA_HOME`, `ANDROID_HOME`,
`EMSDK_DIR` (Emscripten SDK), `IOS_SIMULATOR` (simulator name or UDID).

Examples:

```sh
./build.sh desktop web              # a computer hub with the full web app
./build.sh android ios              # just the phone apps
./build.sh web --inline-guides      # one self-contained HTML file with the whole library inside
./build.sh test --sanitize          # core + host tests under sanitizers, plus the JavaScript suites
```

## What each target needs

| Target | Toolchain |
|---|---|
| `core`, `guides`, `desktop` | A C++17 compiler and CMake 3.20 or newer. The script also finds a CMake installed in `.toolchains/bin/`. No other libraries. Raspberry Pi OS: `sudo apt install cmake g++`. |
| `web` | The Emscripten SDK in `.toolchains/emsdk` (install once, needs internet; see `apps/web/README.md`), Node 22 or newer |
| `android` | Android SDK (platform 37, build-tools 36+, NDK 27.1.12297006, SDK CMake 3.22.1) and JDK 17+; the Gradle wrapper is in the repository |
| `ios` | macOS with Xcode |
| `test` | As the targets it tests; the JavaScript suites need Node, the browser tests Chrome |

`./build.sh doctor` says which of these it found.

Status of the builds themselves: everything was built and tested on macOS. The Linux
and Raspberry Pi builds of `emberd` use the same code but were not compiled in the latest round;
the Windows code has never been compiled. If a Windows build is needed, see
`apps/desktop/deploy/windows/README.md` (Visual Studio 2022, same CMake commands).

## Building pieces by hand

The script only calls the normal tools, so each piece can also be built by hand:

```sh
cmake -S . -B build/desktop -DCMAKE_BUILD_TYPE=Release     # core + emberd
cmake --build build/desktop -j
apps/web/build.sh --guides build/guides.pack               # web app (Emscripten SDK required)
cd apps/android && ./gradlew :app:assembleDebug            # Android APK
```

Use a separate build folder for each experiment (for example `build/<yourname>`).

## The guide library

The guides (including this manual) are Markdown files in `guides/content/<category>/`. The
format is described in `docs/ARCHITECTURE.md` (section 7). After editing, check them with the packer
built by `core`:

```sh
build/host/tools/guidepack validate guides/content     # errors and warnings per file
./build.sh guides                                      # repack build/guides.pack
```

Files with errors are left out of the pack, so one bad file never breaks an app build.

## Keeping it rebuildable without internet

The first build of each target may need the internet to fetch toolchains. After that everything
builds offline. To be able to rebuild after the internet is gone, keep on the same stick or disk:

- the repository itself (with `.toolchains/`, which holds the Emscripten SDK, about 1.5 GB, and any
  CMake you installed there);
- for Android: the Android SDK folder and the Gradle cache (`./gradlew --offline` works once the
  cache is filled);
- for iPhone: a Mac with Xcode (the app cannot be built anywhere else);
- the built results in `dist/`, so people can be given working apps even if nothing can be rebuilt.

## Where to read more

| Document | About |
|---|---|
| `docs/ARCHITECTURE.md` | How the pieces fit, rules for code, the guide format |
| `docs/PROTOCOL.md` | What goes on the air (wire format, security, time, admission control) |
| `docs/RPC_API.md` | The interface between the app screens, the platform shells and the core |
| `core/src/link/README.md` | Every radio and link driver in detail |
| `apps/*/README.md` | Each platform, including an honest list of what is proven and what is only compiled |
