---
name: change-rpc-or-protocol
description: Change the EmberMesh RPC API (core methods, host.* methods, events, host.info fields) or the wire protocol safely across the contract docs, core, every host router, the mock backend, the UI, the manual and tests, and decide when a protocol version bump is needed. Use when adding or changing an RPC method, parameter, result field, event, host capability, packet field, payload kind or bound.
---

# Changing the RPC API or the wire protocol

Contracts: `docs/RPC_API.md` (normative for core, mock, every host router and the UI; "Additive
changes only") and `docs/PROTOCOL.md` (normative for everything on the air, version 2). Both are
shared contracts: say so explicitly in the change description.

## 1. Contract first

Write the change into the contract **before** the code: method, params, result, errors, events,
defaults and bounds, in the section it belongs to (new features get a new numbered section,
marked "(additive)", like §6–§10). Rules from RPC_API §1: results are JSON objects, unknown params
are ignored, callers tolerate unknown result fields, error codes come from the fixed list,
`host.*` methods a platform lacks return `unsupported` and the UI gates the feature on
`host.info.features` (or a documented `host.info` field). Prefer new optional fields and new
methods over changing the meaning of existing ones. Never remove or rename.

## 2. Then every implementation, in one change

| Part | Where |
|---|---|
| core method | `core/src/node/` (dispatch in `node_rpc.cpp`, feature files `node_*.cpp`), tests in `core/tests/node/` (`node_rpc_test.cpp`, fuzz in `node_fuzz_test.cpp`) |
| C API (rare) | `core/include/ember/ember.h` (additive), `core/src/capi/`, `core/tests/capi/` |
| `host.*` on Android | `apps/android/app/src/main/java/org/embermesh/app/host/HostRouter.kt` (+ `services/`, `endpoint/`), JVM tests `./gradlew :app:testDebugUnitTest` |
| `host.*` on iOS | `apps/ios/EmberMesh/Host/HostRouter.swift` (+ `Services/`, `Endpoints/`), XCTest in `apps/ios/EmberMeshTests/` |
| `host.*` in the browser | `ui/js/endpoints/router.js` (+ `device.js`, endpoint modules), tests in `apps/web/test/` |
| `host.*` on the desktop | `host/src/host/host.cpp` (emberd), tests in `host/tests/`; the operator page's device-local methods run in the browser (`ui/js/backends/remote.js`) |
| mock backend | `ui/js/backends/mock.js` and `ui/js/mock/*.js` — `ui/tests/mock_contract.test.js` checks it implements every method in §2/§3 and emits only §4 events |
| shared values | `ui/js/lib/core_spec.js` (ranges, defaults, helpers shared by UI and mock) |
| UI | `ui/js/screens/*.js` (playbook ui-change) |
| docs | the operator manual chapter that describes the feature (playbook write-manual-chapter), platform READMEs ("extensions to RPC_API", "Proven vs …" tables) |

A host-method feature (for example "Morse sending by light", RPC_API §10) touches every row:
contract, Android, iOS, web endpoints, desktop/remote, mock, UI, manual, tests. A platform that
cannot do it returns `unsupported` or reports the limit through `host.info`; it is never silently
faked.

## 3. Wire protocol changes

- Receivers MUST reject anything that does not parse exactly (introduction of PROTOCOL.md): non-zero
  reserved bits, bad lengths, out-of-range values. Update §10 "Bounds" for every new limit.
- **Compatible** (no version change): a new payload kind or control type that old nodes drop as
  unknown, a new optional section that old readers ignore, tighter receiver bounds.
- **Needs a version decision** (`kVersion` in `core/src/proto/packet.h`, frame `b0` bits 7–6, and
  the title of `docs/PROTOCOL.md`): any change to the frame or packet header layout, to what is
  authenticated, to `wire_id`/`sync_id`, or anything an old node would misparse or mis-verify.
  Version 1 frames are rejected today, so a bump splits the mesh; document the migration and get
  agreement first.
- Security-relevant changes (§4, §9, §11) also follow the playbook security-review.
- `guides.pack` has its own format version (`core/src/guides/pack_format.h`); tokenizer or stemmer
  changes require a pack version bump (`tools/guidepack/README.md`).

## 4. Tests

- Core: doctest in `core/tests/<module>/`: happy path, every boundary, malformed, truncated and
  bit-flipped input, round trip encode/decode, known vectors.
- Interop: old/new behaviour against stored fixtures where they exist (`core/tests/data/`).
- Every host: its unit tests; the UI: `node --test ui/tests/` (includes the mock contract test).
- End to end where the feature crosses processes: `ctest --test-dir build/host -R emberd_mesh`,
  `node --test apps/web/test/`, device suites (`./build.sh test --device-tests`).

## 5. Verify and report

```sh
./build.sh test --only native,privacy,js
./build.sh test --sanitize --only native
```

plus the platform suites you touched (playbook build-and-test). Report which hosts implement the
change, which return `unsupported`, and what was tested only in simulation. Update `AGENTS.md`
and any playbook that names a changed method, path or command in the same change.
