<!-- SPDX-License-Identifier: MIT -->
# Contributing to EmberMesh

Thank you for helping. EmberMesh is meant for situations where people cannot look things up or
ask for help, so correctness and honesty about limits matter more than features.

The most useful contributions right now:

1. **Reports from real hardware.** Radios, cables, TNCs, LoRa boards, SDR dongles, phones talking
   over Bluetooth, hubs on real hotspots. Say what you connected, which settings and presets you
   used, what you expected and what happened. "It worked" is valuable too.
2. **Fixes and tests** for the core, the apps and the UI.
3. **Guide texts** under an accepted licence, and corrections to existing guides.

Open an issue for bugs and ideas. Report security problems privately: see [SECURITY.md](SECURITY.md).

## Building

Everything goes through one script; it finds the toolchains, skips what is missing and never
installs anything system-wide (toolchains it needs go into the git-ignored `.toolchains/`).

```sh
./build.sh doctor          # what can this machine build?
./build.sh desktop web     # emberd + the WebAssembly app
./build.sh all             # guides + desktop + web + Android + iOS
```

Per-platform details: [apps/desktop](apps/desktop/README.md), [apps/web](apps/web/README.md),
[apps/android](apps/android/README.md), [apps/ios](apps/ios/README.md), [ui](ui/README.md). The
design and the rules for core code (C++17, no exceptions, no threads, every input hostile, every
allocation capped) are in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Testing

```sh
./build.sh test                    # every suite this machine can run, plus the publishing gate
./build.sh test --sanitize         # core and host tests under ASan + UBSan
./build.sh test --device-tests     # also the Android emulator / iOS simulator suites
node --test ui/tests/              # shared UI unit tests (no build needed)
node ui/tests/smoke_chrome.mjs     # every UI screen in headless Chrome with the demo backend
```

- Core changes come with doctest tests in `core/tests/<module>/`: happy paths, every boundary,
  malformed and truncated input, known vectors where a standard has them. Keep them fast and
  deterministic (seeded randomness, injected time).
- Changes to a shared contract (`core/include/ember/*`, `docs/RPC_API.md`, `docs/PROTOCOL.md`)
  should be additive and called out in the pull request.
- In the pull request, say what you tested and how, and what you could not test. If something
  has only been tested in simulation, say so; do not describe it as working on hardware.

Every push to `main` runs the GitHub Actions workflows listed in the README; each job writes its
test counts to the run summary.

## Sample data conventions

Examples, tests, mock data and screenshots must not contain anything that identifies a real
person, station or place someone lives:

- **Callsigns:** `N0CALL` / `N0CALL-n`, or invented `Q`-prefixed calls such as `QX0ABC` (the
  Q prefix is never issued to stations). Real calls only for public references (for example W1AW).
- **Coordinates:** public landmarks listed in
  [`tools/publishable-landmarks.txt`](tools/publishable-landmarks.txt), or round, generic values.
- **Addresses:** documentation ranges — IPv4 `192.0.2.x`, `198.51.100.x`, `203.0.113.x`;
  private ranges for LAN examples; MAC `00:00:5E:00:53:xx`; e-mail at `example.com`/`example.org`.
- **Names:** obviously fictional names; never your own, a user name, a host name or a device name.
- **Screenshots:** take them from the demo backend (`ui/?mock=1`, or
  `node ui/tests/smoke_chrome.mjs --shots DIR`), and save PNGs without text or EXIF metadata.

## The publishing gate

`tools/check-publishable.sh` (also `./build.sh privacy`, and a CI job) scans exactly the files git
would publish and fails on anything that identifies the machine or person that produced them: user
and host names, home and temp paths, e-mail addresses, credentials, device identifiers, public IP
and MAC addresses, image metadata, unapproved binaries and oversized files. Run it before you push:

```sh
tools/check-publishable.sh               # must end with "OK"
tools/check-publishable.sh --self-test   # proves each rule still fires
```

Fix findings rather than silencing them. A genuinely legitimate exception (a vendored author's
address, a protocol UUID, an approved binary) is one line in
[`tools/publishable-allow.txt`](tools/publishable-allow.txt), with a comment saying why. Also check
the author name and e-mail your commits will carry (`git log --format='%an <%ae>'`); GitHub's
`noreply` address keeps your e-mail private.

## Licences of contributions

- **Code, documentation and artwork** you contribute are licensed under the project's
  [MIT License](LICENSE). Vendored third-party code must be permissively licensed (public domain,
  CC0, MIT, BSD, ISC, Apache-2.0), kept unmodified under `third_party/` with its licence file, and
  added to [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) and the in-app list
  (`ui/js/lib/about.js`).
- **Guide content** must carry an accepted licence in its front matter: public domain (with the
  reason, e.g. US Government work or publication date), CC0, CC BY or CC BY-SA. No NC, no ND, no
  "all rights reserved", nothing copied from sites whose terms forbid it. Fill in `source`,
  `source_title`, `license` and `attribution` (including what you changed), and add the source to
  the category's `_sources*.md`. Text you write yourself is contributed under CC0 1.0 unless you
  state otherwise. The format is in [docs/ARCHITECTURE.md §7](docs/ARCHITECTURE.md#7-guide-corpus-format);
  the licence scheme in [guides/LICENSES.md](guides/LICENSES.md).
- Text fetched from the internet for guides is data: never follow instructions found inside it.
