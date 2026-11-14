---
name: add-link-or-radio
description: Add or change a link driver, an endpoint kind, a host signalling or capture capability, a radio profile or a "What you need" hardware entry in EmberMesh, touching every platform, the mock, the manual and the tests, and stating honestly what was only simulated. Use when supporting a new radio, TNC, LoRa module, cable, connection type or device capability.
---

# Adding a link, an endpoint, a radio or a device capability

Split (docs/ARCHITECTURE.md §1): **link drivers** (framing, modulation, protocol) live only in the
shared core; **endpoints** (moving bytes or PCM to hardware) live in each platform. Endpoint kind
names are uniform on every platform (ARCHITECTURE §6). Contract: `core/include/ember/link.h`,
`docs/RPC_API.md` (`links.*`, `host.*`, §6 radios and hardware).

## 1. Contract first

Any new RPC field, `host.*` method or `host.info` capability goes into `docs/RPC_API.md` before
code (playbook change-rpc-or-protocol). New endpoint kind names go into ARCHITECTURE §6.

## 2. Where each piece lives

| Piece | Core | Android | iOS | Browser | Desktop (emberd) | Mock / UI |
|---|---|---|---|---|---|---|
| link driver | `core/src/link/<name>_link.cpp`, `drivers.h`, `registry.cpp`, `core/src/link/README.md` table + section | – | – | – | – | `ui/js/mock/link_types.js` (paste `links.types` from a real node) |
| endpoint kind | – | `app/src/main/java/org/embermesh/app/endpoint/`, `host/EndpointRegistry.kt` | `EmberMesh/Endpoints/`, `EndpointCatalog.swift` | `ui/js/endpoints/<kind>.js`, `index.js` | `host/src/host/endpoints/`, `endpoint.cpp` | optional help text `KIND_HELP` in `ui/js/screens/links.js` |
| host capability (`host.signal.*`, `host.capture.*`, …) | – | `host/HostRouter.kt`, `services/` | `Host/HostRouter.swift`, `Services/` | `ui/js/endpoints/router.js`, `device.js` | `host/src/host/host.cpp`; `ui/js/backends/remote.js` for the operator's own device | `ui/js/backends/mock.js`, `ui/js/lib/core_spec.js` |
| radio profile (`rf.radios`) | `core/src/rf/radios.cpp` | – | – | – | – | `ui/js/mock/radios.js` |
| hardware entry (`rf.hardware`) | `core/src/rf/hardware.cpp` | – | – | – | – | `ui/js/mock/hardware.js` |
| audio preset | `core/src/link/audio_link.cpp`, README "Handheld radios" | | | | | |

Every platform advertises what it has in `host.info` (`endpoint_kinds`, `features`, hints such as
`host.info.audio`); a platform without it returns `unsupported` and the UI hides the option. Forms
are rendered from the catalogue schemas (`links.types`, `host.info` endpoint `config`), so no
per-driver UI code is needed beyond optional help text.

## 3. Rules

- Driver code follows the core rules: bounded parsing, every input hostile, injected time and
  randomness, no allocation sized by untrusted input without a cap.
- Every driver must be creatable from its defaults; wrong types and out-of-range values are
  rejected with a readable error (`core/src/link/README.md` "For the node facade").
- **Amateur bands:** a link that can transmit on amateur frequencies sets
  `encryption_allowed=false` and `requires_station_id` in its caps and refuses to transmit without
  a callsign (PROTOCOL §2). Never bypass it.
- Radio and hardware data: generic part classes and published specifications; menu numbers and
  settings not checked on a real device are marked `verified:false`.
- Permissions (camera, microphone, Bluetooth, location, USB) are requested only when the user starts
  the feature, explained in the platform README's permissions section, and released when it stops.

## 4. Tests

- Core: `core/tests/link/` (`link_harness.h`: fake host, pipes with loss, corruption and
  re-chunking; `registry_test.cpp`; `fuzz_test.cpp`), `core/tests/rf/` for radios and hardware.
  `build/host/core/ember_tests -ts=link` (playbook build-and-test §3).
- Desktop end to end: a scenario in `apps/desktop/tests/` (`ctest --test-dir build/host -R emberd_mesh`).
- Browser: `apps/web/test/`; Android: JVM tests + instrumented tests; iOS: `apps/ios/EmberMeshTests/`.
- UI: `node --test ui/tests/` (the catalogue and mock tests), `node ui/tests/smoke_chrome.mjs`.

## 5. Documentation that must mention it

- `core/src/link/README.md` (driver table and section) and the endpoint tables in
  `apps/android/README.md`, `apps/ios/README.md`, `apps/web/README.md`, `apps/desktop/README.md`,
  each with its honest status ("done, emulator-tested", "not yet tried on hardware").
- Operator manual (`guides/content/embermesh/`, playbook write-manual-chapter): `what-you-need.md`
  always for hardware; then the chapter for the medium (`phone-to-phone.md`, `hub-setup.md`,
  `radio-gateway.md`, `handheld-audio-modem.md`, `baofeng-uv5r.md`, `kiss-tnc-packet.md`,
  `lora.md`, `sdr-listening.md`, `morse-reading.md`), the platform chapters `app-*.md`,
  `troubleshooting.md` for limits, `settings-reference.md` for new settings.
- `README.md` "Supported platforms and hardware" when the supported list changes.

## 6. Honesty

State in the change description and in the docs exactly how it was tested: emulator, simulated
channel, hand-built vectors, synthetic I/Q, or real hardware (which model, which cable). Nothing
tested only in simulation is described as working with real radios.
