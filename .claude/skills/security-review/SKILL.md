---
name: security-review
description: Checklist for EmberMesh changes that touch cryptography, parsing of radio, network, file, QR or guide-pack input, storage, the hub HTTP/WebSocket surface, device permissions, optical or audio signalling, or GitHub workflows - threat model pointers, hostile-input tests, the amateur-band rule, secrets handling and private vulnerability reporting. Use before finishing any such change or when reviewing one.
---

# Security review checklist

Threat model: `SECURITY.md` (summary and known limits), `docs/PROTOCOL.md` §4 Security, §9 Mesh
time, §10 Bounds, §11 Admission control; hub: `apps/desktop/README.md` "Security notes" and
"Privacy notes"; user-facing: `guides/content/embermesh/security.md`. Assume anyone in radio or
network range can listen, transmit, replay and forge, and every input may be hostile.

## 1. Cryptography and identity

- [ ] Only the vendored Monocypher primitives through the wrappers in `core/src/crypto/`
      (X25519, Ed25519, XChaCha20-Poly1305, BLAKE2b, Argon2id). No home-made primitives, modes,
      MACs or random number generators; randomness is injected entropy, never `rand()`.
- [ ] Nonces never repeat for a key (PROTOCOL §4 salts); keys and passphrases are wiped after use
      where the wrappers do so; comparisons of secrets are constant time.
- [ ] What is authenticated did not shrink; a change to it is a protocol version decision
      (playbook change-rpc-or-protocol).
- [ ] Unverified or unknown senders still cannot cancel, replace or evict verified state (SOS,
      bulletins, receipts, history) and still get the strict budgets of PROTOCOL §11.

## 2. Hostile input (radio, network, files, QR codes, guide packs)

- [ ] Parsed with `ByteReader` or explicit bounds checks; no length field trusted; every
      allocation sized by input is capped; no throwing std functions; reserved bits must be zero.
- [ ] New limits written into PROTOCOL §10 (or the pack limits in `core/src/guides/pack_format.h`).
- [ ] Tests for empty, 1 byte, max, max+1, truncated, bit-flipped and random input. Extend the
      existing fuzz-style doctest suites where they fit: `core/tests/link/fuzz_test.cpp`,
      `core/tests/node/node_fuzz_test.cpp`, `core/tests/store/kv_fuzz_test.cpp`,
      `core/tests/guides/fuzz_test.cpp`, `core/tests/guides/hostile_pack_test.cpp`,
      `core/tests/fec/fec_fuzz_test.cpp`. Seeded randomness only.
- [ ] `./build.sh test --sanitize --only native` passes (ASan + UBSan).
- [ ] UI: no HTML sinks, Markdown only through `ui/js/lib/markdown.js`, regexes without
      catastrophic backtracking (`ui/tests/`).

## 3. Storage and the device

- [ ] Data at rest stays under the passphrase encryption (`core/src/store/`, `store_crypto.cpp`);
      duress passphrase and panic wipe still cover new data; crash safety tests
      (`core/tests/store/kv_crash_test.cpp`, `kv_fault_test.cpp`) still pass.
- [ ] Nothing new is written outside the app's data directory; nothing sensitive goes to logs.

## 4. Hub HTTP / WebSocket surface (`host/`, `apps/desktop/`)

- [ ] `/rpc` and the console still require the admin token; guests only reach `/`, `/ws`,
      `/download/`, `/hub.json`; `/hub.json` reveals nothing new (no OS user, host or device name).
- [ ] Per-guest budgets, connection caps, header and path limits unchanged or tightened; tests in
      `host/tests/` (`http_test.cpp`, `ws_test.cpp`, `hub_test.cpp`) and the `emberd_mesh` flood and
      garbage scenarios still pass.
- [ ] The hub is plain HTTP on a local network: never present it as confidential.

## 5. Permissions and signalling

- [ ] Camera, microphone, location, Bluetooth, USB permissions are requested only at the moment of
      use (for example the camera that a browser needs to drive a torch), released immediately
      when the feature stops, and explained in the platform README's permissions section.
- [ ] Optical and audio signalling (Morse by torch, screen or tone; acoustic coupling) and
      everything sent in cleartext can be read by anyone in line of sight or earshot. The UI and
      the manual never present it as private.
- [ ] Defaults stay anonymous: no OS user, host, device or Bluetooth name in anything broadcast;
      pairing codes and cards say what they reveal (LAN addresses, node id, public keys).
- [ ] No telemetry, analytics, remote logging, update pings or third-party requests.

## 6. Amateur bands

- [ ] Links flagged `encryption_allowed=false` / `requires_station_id` carry only cleartext signed
      traffic with the operator's callsign; ciphertext is never forwarded onto them; no callsign,
      no transmit (PROTOCOL §2, ARCHITECTURE §4 "Legal mode"). Tests cover the new path.

## 7. Workflows and supply chain

- [ ] `permissions: contents: read` unless the job publishes; `uses:` pinned to a full commit SHA
      with a version comment (`tools/ci/check-pins.sh`); no new third-party action receives a secret.
- [ ] Secrets only in steps guarded to `main`/`v*` tags, never on `pull_request` runs; never echoed,
      uploaded or written to the workspace (`tools/ci/android-signing.sh`).
- [ ] No `pull_request_target`, no untrusted input (titles, branch names) interpolated into `run:`.
- [ ] Vendored code is permissive, unmodified, with its licence file, listed in
      `THIRD_PARTY_LICENSES.md` and `ui/js/lib/about.js`; `tools/ci/check-licences.sh` passes.
- [ ] actionlint and shellcheck pass (`docs/CI.md` "Publish checks").

## 8. Reporting a vulnerability

Never in a public issue, pull request or discussion. Use the repository's **Security** tab ->
**Report a vulnerability** (`SECURITY.md`), with the component, commit, a reproduction (a crafted
packet, frame, file or pack is ideal) and the impact. When you find one while working on something
else, stop and report it privately; do not push a revealing test or commit message first.

## 9. Report

List which boxes apply, what was tested (including sanitizers and fuzz suites), and the known
limits that remain.
