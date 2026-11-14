# Sources: embermesh (EmberMesh Operator Manual)

Every file in this directory is an **original work** written for EmberMesh by the EmberMesh
contributors and dedicated to the public domain under **CC0-1.0**
(https://creativecommons.org/publicdomain/zero/1.0/). No text was copied from other publications.
You may copy, translate, print, adapt and redistribute it for any purpose without asking.

All chapters share these front-matter values:

- `source: original`
- `source_title: EmberMesh Operator Manual`
- `license: CC0-1.0`
- `attribution: EmberMesh contributors`
- `part_of: embermesh/manual` (chapter order in `order`)

## What the chapters were written from

The manual describes the software as it exists in this repository:

- `docs/ARCHITECTURE.md`, `docs/RPC_API.md`, `docs/PROTOCOL.md` (protocol version 2), `build.sh`
- `core/src/link/README.md` and the link driver sources (field names, defaults, presets)
- `core/src/modem/` headers and `PERFORMANCE.md` (measured modem thresholds, simulator only)
- `core/src/tools/` and `core/src/rf/` (tool titles, antenna dimensions, band-plan data, cable
  loss models); all worked antenna numbers were produced by running the app's own design code
- `core/src/node/` (setting ranges and defaults actually enforced by the node), `ui/js/lib/core_spec.js`
- `apps/desktop/README.md` and `apps/desktop/deploy/`, `apps/android/README.md`,
  `apps/ios/README.md`, `apps/web/README.md`, `ui/README.md`, `ui/js/screens/` (screen names and
  button labels)

Radio facts not taken from the code come from the public-domain and CC-licensed texts already in
`guides/content/radio/`, or are general knowledge marked as approximate where they are not exact.

## Honesty note

EmberMesh has been proven end to end only between real programs on a single computer:
desktop daemons, Chrome tabs, the Android emulator and the iOS simulator, over sockets and virtual
audio paths, with the radio drivers checked against emulators. **No real radio, TNC, LoRa board,
SDR dongle, Bluetooth link, USB-serial adapter or microphone capture has been exercised**, the
Linux and Windows builds of the desktop daemon have not been compiled in that round, and Firefox
and Safari are untested. The manual says so wherever a procedure depends on that, and explains how
to verify a link yourself ([Troubleshooting](troubleshooting.md) has the full list).

Revised for protocol version 2 and the integrated apps (eight-word fingerprints,
channel keys, unverified senders, the time model, duress unlock, level meter, phone hubs, platform
actions, region-seeded LoRa defaults, `./build.sh`).
