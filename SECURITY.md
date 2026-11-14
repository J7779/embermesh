<!-- SPDX-License-Identifier: MIT -->
# Security policy

## Reporting a vulnerability

Please report vulnerabilities **privately** through GitHub's private vulnerability reporting:
open the repository's **Security** tab and choose **Report a vulnerability**. Do not open a public
issue, pull request or discussion for a problem that could put people at risk before a fix is
available.

Helpful details: the component (core, a platform app, the hub, the web app, the guide packer),
the version or commit, how to reproduce it (a crafted packet, frame, file or guide pack is ideal),
and what an attacker gains. Reports are acknowledged as soon as possible, and fixes are released
through the normal builds; you will be credited in the fix unless you prefer not to be.

Only the latest commit on `main` (and the rolling `latest` pre-release built from it) is
supported.

## What EmberMesh protects, in brief

EmberMesh assumes that **anyone in radio or network range can listen, transmit, replay and forge
traffic**, and that every input — radio noise, frames, files, guide packs, QR codes — may be
hostile.

- **Confidentiality and authenticity:** direct messages and private channels are end-to-end
  encrypted and authenticated (X25519, XChaCha20-Poly1305, Ed25519, BLAKE2b, Argon2id via
  Monocypher). Public traffic is signed. Acknowledgements are self-authenticating and cannot be
  forged by relays.
- **Abuse by strangers:** new identities are free, so packets from unknown or unverifiable senders
  get strict rate and storage budgets, are relayed only a short distance, and can never cancel or
  replace verified state (an SOS, a bulletin, a read receipt). Replays are remembered across
  restarts.
- **Time:** a device with a working clock never lets the mesh change it; clock-less devices accept
  time only from authenticated, agreeing sources.
- **Robustness:** every parser is bounded, rejects malformed input and caps allocations; the
  core has no exceptions, no threads and no OS calls outside storage.
- **On the device:** storage encrypted with a passphrase, a duress passphrase, panic wipe. Hubs
  require an admin token for remote control and never announce the device's own name.
- **Amateur-band links** carry only cleartext signed traffic with the operator's callsign, as the
  rules require.

**Known limits:** no forward secrecy for direct messages; traffic metadata (who talks to whom, when,
from where) is visible to anyone in range; passphrase channels can be attacked offline with weak
passphrases; verification only means something for contacts you verified yourself; the hub uses
plain HTTP/WebSocket on the local network; the code has not had an independent security audit.

The full threat model and the mechanisms are in [docs/PROTOCOL.md](docs/PROTOCOL.md) (§4 Security,
§9 Mesh time, §10 Bounds, §11 Admission control), the hub's in
[apps/desktop/README.md](apps/desktop/README.md#security-notes), and the user-facing advice in the
[security chapter](guides/content/embermesh/security.md) of the operator manual.
