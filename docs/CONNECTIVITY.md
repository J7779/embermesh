<!-- SPDX-License-Identifier: MIT -->
# EmberMesh — connectivity without configuration: Nearby devices and the internet relay

Status: design, revision 2 (after an independent adversarial review; normative where it says MUST).
Contract: [RPC_API.md §11](RPC_API.md#11-nearby-devices-and-the-internet-relay-additive); wire
format: [PROTOCOL.md §12](PROTOCOL.md#12-internet-relay-framing). Core implementation:
`core/src/proto/relay.*` (derivations, sealing), `core/src/framing/mqtt.*` (MQTT 3.1.1 codec),
`core/src/link/relay_link.*` (link type `relay` and its per-session child links),
`core/src/link/stream_link.cpp` (preset `nearby`: the hello), `core/src/node/node_connectivity.cpp`
(settings, consent, sessions, auto links), `core/src/mesh` (per-link scope, fallback, beacon and
push budgets).

## 1. Problem

Two people add each other as contacts and nothing flows: a mesh needs links, and on phones nothing
creates one by itself. On iPhones the UDP multicast LAN endpoint needs an Apple entitlement the
public build does not have. Phones on LTE or satellite internet sit behind carrier NAT and can only
make outbound connections. EmberMesh runs no servers and has no accounts.

* **Nearby devices** (on by default after first-run setup, with a plain disclosure): devices on the
  same Wi-Fi or local network find each other and link up with no configuration and no special
  entitlement. Automatic Bluetooth links are deferred (§3.5); Bluetooth stays a link the user adds by
  hand.
* **Internet relay** (off until the user turns it on): contacts reach each other at any distance,
  when they cannot be reached otherwise, through public or private MQTT brokers over TLS WebSocket,
  every frame sealed end to end on topics derived from secrets only the parties hold.

## 2. Threat model

| Actor | Capabilities assumed |
|---|---|
| Passive observer on the same Wi-Fi / LAN | sees mDNS, ARP, packet addresses and sizes; can connect to any listening port |
| Active attacker on the same Wi-Fi | all of the above; answers mDNS with forged records; opens many connections; sends garbage; spoofs unauthenticated mesh control frames (beacons) |
| A web page in a LAN user's browser | can send HTTP requests (including POST bodies) to any LAN or loopback address and port |
| Relay operator (broker) | sees client IP addresses, connection times, every topic subscribed and published, message sizes and timing; can drop, delay, reorder, duplicate, inject, retain and replay |
| Other clients of a public broker | **assumed able to subscribe to wildcards** and so to watch every topic name, size and timing (not IP addresses, not content). Verified 2026-11-14: broker.emqx.io refuses `#` but accepts `+` and `+/+`; test.mosquitto.org accepts `#`; broker.hivemq.com refuses `#`, `+` and `+/+`. |
| Internet path observer | with TLS: the broker's host name (SNI/DNS), connection times and sizes |
| A contact | holds the pair secret: can read and inject on the pair session (as on any link) |
| Someone who seizes a device | holds its identity key, hence every pair and channel secret |

Assets: content and authenticity (end-to-end already: PROTOCOL §4); who talks to whom; the user's
identity, IP address and location over time; availability.

Out of scope: hiding from a relay *that* an IP address uses EmberMesh; global traffic analysis; a
device compromised while in use.

## 3. Nearby devices

### 3.1 What exists and what changes

Today Android's `tcp_server` advertises `_embermesh._tcp` (TXT `proto=tcp`) and iOS's advertises
with TXT `ember=1 link=stream v=1` but browses only `ember=1`, so the two never see each other;
the desktop host has no mDNS; nothing links up by itself. New on every native platform: **DNS-SD
`_embermesh._tcp` + TCP + the `stream` link driver with preset `nearby`**, started automatically
when Nearby is active (§3.9). It needs no entitlement: iOS Network.framework (`NWListener` with
`NWListener.Service`, `NWBrowser`; `NSLocalNetworkUsageDescription` and `NSBonjourServices`
already present), Android `NsdManager`, desktop a minimal built-in mDNS responder/browser (§3.10).
UDP multicast stays where it works. In v1 automatic Nearby is Wi-Fi/LAN only (§3.5).

### 3.2 Service record

* Service `_embermesh._tcp.local.`; port = the listener's port (any free port).
* Instance `EmberMesh-<token>`; SRV target host `EmberMesh-<token>.local.` on the desktop responder
  (never the system host name). iOS and Android use the system's responder, which announces the
  device's own mDNS host name — often derived from the device name; that is a platform limitation,
  stated in the UI text ("devices on this Wi-Fi can see …").
* `token` = 8 lowercase hex digits from `connectivity.get` (`nearby.token`), random per node start
  and renewed on every network change (`connectivity.set {nearby_renew_token:true}` is not needed:
  the core renews when the host calls `connectivity.network_changed`).
* TXT exactly: `ember=1`, `v=1`, `link=stream`, `t=<token>`. Browsers ignore entries without
  `ember=1` and `v=1` and take the token from `t`.
* A/AAAA: IPv4 addresses of the interface and IPv6 link-local addresses only (no global or EUI-64
  addresses); link-local addresses are used with their zone id.
* **What a passive observer learns from the advertisement:** an EmberMesh device at this IP/MAC
  address, its port, a token that changes at every start or network change, and (iOS/Android) the
  device's mDNS host name. **What anyone who connects learns:** what every mesh neighbour sees —
  the node's id, display name, callsign and public keys (ANNOUNCE), and its Public traffic. These
  are stable identifiers: sensors at several places can recognise the same person. This is why
  Nearby is disclosed plainly before it starts (§3.9).

### 3.3 The hello (wire contract for every host and the core)

Every nearby TCP connection starts with a binary hello in each direction, sent by the core's
`stream` driver (preset `nearby`) as soon as the endpoint opens:

`45 4D 4E 42 01 00 00 00` ("EMNB", version 1, three reserved zero bytes).

The first 8 bytes received MUST be exactly this (reserved bytes MUST be zero) and MUST arrive
within 2 s, otherwise the core marks the link failed and removes it (`link.removed reason:
"hello_failed"`), and the host closes the socket. Any other first bytes — `GET `, `POST`, `OPTI`,
`PUT `, `HEAD`, TLS, anything — fail at once. Only after the hello are stream frames accepted. A
browser request therefore cannot inject frames (it cannot make the first bytes "EMNB…").

### 3.4 Who dials, limits, duplicates

* Every node listens and browses. For a discovered instance with token `T` (own token `M`): the
  node with the smaller token (bytewise on the hex strings) dials; the other waits 8 s for an
  inbound connection and then dials too (one-sided discovery).
* Outbound (normative): only addresses on the interface's own subnet or link-local (IPv4 RFC 1918
  / 169.254/16 within the interface prefix; IPv6 fe80::/10 with zone, ULA within the interface
  prefix); at most 4 addresses per instance (IPv6 link-local first, then IPv4, 250 ms apart); at
  most 4 new nearby dials per minute in total.
* Inbound (normative): the listener binds only to the local-network interface(s) (never to cellular
  interfaces or `[::]` on all interfaces); accepts only on-link/private source addresses as above;
  at most 2 connections per source IPv4 address or IPv6 /64, at most 4 new connections per minute
  per source.
* Each connection becomes a core link: `links.add {type:"stream", config:{preset:"nearby"},
  persist:false, auto:"nearby", auto_role:"dial"|"accept", auto_key}` with
  `auto_key = "<token>@<address>"` when dialing and `"@<address>"` when accepting. The core refuses
  (`busy`) a second dial link with the same `auto_key` while the first is not down.
* At most 16 nearby links. When full, the core evicts the oldest nearby link whose neighbour has not
  proven its identity (below) instead of refusing (`link.removed reason:"evicted"`); if every link
  is proven, the new one is refused (`busy`).
* **Proven identity** of a nearby link: a verified, self-certifying ANNOUNCE received on that link
  with `hops = 0` and `hop_src` = the first 4 bytes of its origin, or a pairwise-authenticated packet
  received directly from its origin (`hops = 0`). Unauthenticated beacons never count.
* **Duplicates:** if two proven nearby links lead to the same node, the core keeps the one dialed by
  the node with the smaller node id (both ends compute the same answer) and removes the other
  (`reason:"duplicate"`). Links that are not nearby links are never removed by this rule.
* Names: a nearby link is named "Nearby device" until proven, then "Nearby: <display name>" (the
  proven neighbour's announced name, sanitised, ≤ 32 bytes).

### 3.5 Bluetooth LE (deferred: not in v1)

**v1 decision:** automatic Nearby links are same Wi-Fi/LAN only, on every platform. Bluetooth stays
a link the user adds by hand (`ble_peer`), with no automatic discovery. Reasons: Android cannot
advertise an arbitrary local name without renaming the owner's Bluetooth adapter; iOS cannot put
service data in its advertisement; and the existing `ble_peer` endpoint is one shared link per
device, which cannot carry a per-link hello or the per-link bookkeeping (§3.4).

**Future design (specified, not built):**

* A **BLE token**: 4 random bytes, independent of the mDNS token, renewed at least every 10 minutes
  and on every advertising restart.
* Android puts the token in **manufacturer-specific or service data in the scan response** (with the
  peer service UUID `8e3b0001-…` in the advertisement); the adapter name is never touched. iOS, which
  allows only a local name and service UUIDs, sets the **local name** `EM` + 8 lowercase hex digits.
  Scanners accept either form.
* **One link per BLE connection** (not one shared `ble_peer` link): each GATT connection becomes its
  own `stream` link with preset `nearby`, `auto:"nearby"`, `auto_key = "<ble token>@ble"`, so the
  hello (§3.3), the proven-identity and duplicate rules and the limits (§3.4) apply unchanged. A BLE
  link duplicating a proven Wi-Fi link to the same node is the one removed (lower `caps.cost` kept).
* Scanning in windows (10 s every 60 s) while Nearby is active and the app is in the foreground;
  the smaller BLE token dials.
* When it ships, the disclosure (§3.9) and `nearby.notice` add Bluetooth range, and the Bluetooth OS
  prompts move to the first-run consent.

### 3.6 Untrusted networks

On public Wi-Fi strangers' devices become neighbours, as on a shared radio channel. Everything they
send meets the existing trust classes and budgets (PROTOCOL §11). In addition: the limits of §3.4;
unproven links are evicted first; the known-but-unverifiable budget has a global cap across all
links (2400/min, burst 4800) so 16 links cannot multiply it; nearby links never get more trust than
radio neighbours; contacts are recognised only by their keys. The UI shows the number of nearby
devices.

**Later (specified, not built):** "Only on networks I approve" — the host remembers approved
networks by a salted hash of the network's identity (SSID + gateway MAC), and Nearby over Wi-Fi
runs only on those.

### 3.7 Battery

Advertise/browse only while Nearby is active and the app is in the foreground (iOS always; Android
also while its foreground service runs with "keep running" on); stop when the network goes away;
restart with a new token on a network change. Desktop always. Idle TCP links cost nothing beyond
mesh beacons.

### 3.8 Lock, wipe, duress

Hosts MUST NOT advertise, browse, dial or accept before `connectivity.get` reports
`ready:true` (setup completed **and** the node unlocked). On `security.lock`, wipe or a duress unlock
the core removes every automatic link (nearby and relay, `reason:"locked"` / `"wipe"`) and reports
`ready:false` before any new identity is up; hosts close the endpoints and stop discovery. A wipe
resets every `connectivity.*` setting to its default (consent off). The duress snapshot carries the
`connectivity.*` settings over unchanged (without relay passwords), so a decoy looks like the real
profile; a decoy has no contacts or private channels and so subscribes to no real topic.

### 3.9 Disclosure and the switch

`connectivity.nearby` (default true) is effective only when `connectivity.setup_done` is true.
First-run setup — the "Connect to people" card on the "Your identity" step, applied by "Start using
EmberMesh" — shows, before anything starts:

> "Nearby devices: find other EmberMesh users on this Wi-Fi or local network and connect
> automatically. Devices on the same network can see your name and your EmberMesh identity."

and the UI then calls `connectivity.set {setup_done:true, nearby:<choice>}`; the OS permission
prompt (iOS Local Network) is asked at that moment; Android needs no runtime permission for
DNS-SD on the local network. Upgraded installs (`setup_done` absent, existing identity) get a one-time notice with
the same text and the switch before anything starts. Settings → "Nearby devices" turns it off: the
core removes nearby links (`reason:"nearby_off"`) and hosts stop.

### 3.10 Desktop mDNS

A minimal mDNS (RFC 6762) responder and DNS-SD (RFC 6763) browser in `host/`: one multicast socket
on 224.0.0.251/ff02::fb port 5353 with address reuse (coexists with the system responder), answers
PTR/SRV/TXT/A/AAAA for our one instance only, queries `_embermesh._tcp.local.`, parses responses
with the core's hostile-input rules (bounded names and counts, no compression loops, no
allocations sized by input). Rejected: Avahi/Bonjour client libraries (runtime dependency, three
APIs).

### 3.11 Browsers

A web page cannot discover or accept LAN connections. The web app keeps automatic join of the hub
that served it, WebRTC pairing by code/QR, and gains the internet relay (browsers have `wss://`).
A web app served by a hub sends that hub's LAN address or host name as the WebSocket `Origin` to
relays — a browser behaviour that cannot be removed; the single-file app (`file://`) sends
`Origin: null`.

## 4. Internet relay

### 4.1 Choice

**MQTT 3.1.1 over TLS WebSocket (`wss://`, subprotocol `mqtt`), QoS 0 only**, against a user-editable
list of relays. The core implements the client: CONNECT (empty client id, clean session),
SUBSCRIBE/UNSUBSCRIBE, PUBLISH QoS 0, PINGREQ, DISCONNECT and their acks — no QoS 1/2, retain, will
or sessions — with the bounds of §4.13, fuzzed like every other parser.

Verified from a development machine on **2026-11-14** (anonymous CONNECT with an empty and with a
random client id, subprotocol `mqtt`, 4352-byte QoS 0 round trip on a random topic):

| Broker | Works | `#` | `+` / `+/+` | Default |
|---|---|---|---|---|
| `wss://broker.hivemq.com:8884/mqtt` (HiveMQ) | yes | refused | refused | **enabled** |
| `wss://broker.emqx.io:8084/mqtt` (EMQX) | yes | refused | **accepted** (thousands of other topics visible) | listed, disabled |
| `wss://test.mosquitto.org:8081/mqtt` (Eclipse Mosquitto) | yes | **accepted** | not tested | listed, disabled |

Also tried the same day, not usable over WebSocket from here: `mqtt.eclipseprojects.io:443`,
`broker.mqttdashboard.com:8884`, `public.mqtthq.com:8084`. **Rule:** only a broker verified to refuse
both `#` and `+` is enabled by default; brokers that allow wildcard subscriptions are listed but
disabled, and their entry says why (`Relay.note`). Privacy wins over redundancy: with one default
relay, reaching contacts over the internet depends on that one free service. The user's own hub
(§4.12) is the robust option; more relays can be enabled or added in Settings → Internet relay.

These are free test services with no guarantee; their terms may not cover this use and they may
block or vanish. The design still assumes metadata can be watched on any public broker (a broker
may change its policy). Multi-level topics (`<8 hex>/<24 hex>`) were considered as a cheap way to
dodge a root `+`, and rejected: EMQX accepts `+/+` as well, so it would hide nothing. With a single
default relay one company sees the default users' social graph (stated in the notice). A publicly
reachable `emberd` hub is a relay with no third party (§4.12).

Alternatives considered:

| Option | Why not (now) |
|---|---|
| WebRTC data channels, relay for signalling | Fails behind two carrier NATs without TURN (infrastructure). Later upgrade for bulk data when a direct path exists. |
| Nostr relays | Needs secp256k1 Schnorr on every event (second curve, new vendored code); relays index and keep events publicly; JSON on the hot path. Possible later as a second relay kind. |
| Tor onion services | Nothing embeddable and maintained for iOS/WASM; heavy; slow. Users may run the app over an OS-level VPN or Tor to hide their IP. |
| Our own server | The project runs no infrastructure. |
| HTTP polling services | Battery/data cost; same metadata exposure. |

### 4.2 Sessions (one engine link per topic root)

A `relay` link is one MQTT connection to one broker. It is **only a transport**. Every session — one
per contact pair, one per relayed private channel — is its own engine link (a *child* of the relay
link, ids from 0x40000000 upward, never persisted) with its own scope (§4.7), beacons, digests,
fragment id space and frame caps. A contact therefore never receives a beacon digest, sync id or
fragment of my traffic with anyone else. `links.list` shows the relay link with
`status.sessions` and `status.session_link_ids`; `mesh.stats`/`peer.seen` may name child link ids.

### 4.3 Keys and topics (precise: PROTOCOL §12)

* Pair: `R = BLAKE2b-256(key = K_pair, "ember-relay-pair-v1")`. Channel (only if relayed, §4.8):
  `R = BLAKE2b-256(key = K_chan, "ember-relay-chan-v1")`.
* `host` = the broker's canonical `hostname:port` (lower case; default port 443). Topics, keys and the
  AD are bound to it: a topic or frame seen on one broker is useless on another.
* Epoch `e` = floor(Unix time in ms, taken from the node's mesh clock / 86 400 000) — UTC days.
* `topic(e) = hex(BLAKE2b-128(key = R, "ember-relay-topic-v1" ‖ u32be e ‖ u8 |host| ‖ host))`,
  `K_frame(e) = BLAKE2b-256(key = R, "ember-relay-aead-v1" ‖ u32be e ‖ u8 |host| ‖ host)`.
* Subscribe `topic(E)`; also `topic(E+1)` during the last 3 hours of day E and `topic(E−1)` during
  its first 3 hours. Publish on `topic(E)`. Clocks may disagree by up to 3 hours.
* What rotation protects: topics change daily, so an observer who sees only publishes (a wildcard
  watcher) cannot tell that two days' topics belong together. **It does not protect against the
  broker**: the overlap window and a client's reconnecting set of topics from new IP addresses let the
  operator chain a user's topics across days and networks — a history of IP addresses, and so of
  approximate locations. Stated in the notice.

### 4.4 Sealing

MQTT payload = `u8 1 ‖ nonce[24] ‖ XChaCha20-Poly1305(K_frame(e), nonce, AD, P)`:
`P = u64be sent_ms ‖ u16be len ‖ frame ‖ zeros` padded to 128/256/512/1024/2048/4096 bytes;
`nonce = BLAKE2b-192(key = K_frame(e), "ember-relay-nonce-v1" ‖ rnd16 ‖ u64be sent_ms ‖ P)` with 16
fresh random bytes (hedged: a weak RNG never repeats a nonce for different plaintexts);
`AD = "ember-relay-v1" ‖ u8 1 ‖ u8 |host| ‖ host ‖ topic`. Sizes leak only the class; files leak
their size to ≈ 4 KiB through the number of fragments, and message timing and count are visible.

### 4.5 What the relay and observers learn

| Observer | Learns | Cannot learn |
|---|---|---|
| Relay operator | your IP address (so roughly where you are), when you are online, how many topics you subscribe (≈ contacts + relayed channels), the size class and time of every message, **which other users you exchange messages with** (IP addresses sharing a topic), and — by chaining topics and reconnects — all of this across days and networks | content, names, node ids, keys, which contact a topic is |
| Anyone watching a public broker (wildcards) | topic names, sizes and timing | IP addresses, content |
| Internet path observer | the broker's host name, times and sizes | topics, content |
| Your contacts | when you come online (beacons on your pair session) | — |
| Someone who learns a topic | can publish junk to it (§4.6) | read or inject |

### 4.6 Replay, injection, flooding

* Injection needs `K_frame(e)`: impossible for the broker or a watcher. On a pair session the frame's
  `hop_src` MUST be the contact's short id. My own frames echoed back by the broker (MQTT 3.1.1 has
  no "no local") are recognised by `hop_src` = mine and dropped **before** any replay state changes.
* Replay state is kept per **(topic, `hop_src`)** and across all relay links (so a frame recorded on
  one broker and injected on another is caught): the highest `sent_ms` seen and the nonces of the last
  30 minutes; a known nonce or a `sent_ms` more than 10 minutes below that sender's highest is
  dropped. At most 64 senders are tracked per topic (a channel), the least recent evicted; the nonce
  memory is capped at 16 384 entries per node. A `sent_ms` outside [start(e) − 1 day,
  start(e) + 2 days) is dropped (bounds a member with a wrong clock, a spoofed system clock and delayed
  replays). Replay state is not persisted; after a restart, replayed packets are still caught by the
  mesh's persisted replay memory and duplicate cache, and replayed hop-local controls are bounded by
  the small push budget of sessions (§4.9).
* PUBLISH packets from the broker with RETAIN=1, DUP=1 or QoS ≠ 0 are dropped (counted): a third party
  cannot plant retained frames for replay on every reconnect.
* Flooding: at most 50 messages/s per topic and 200/s per relay link. **Circuit breaker:** a topic
  with more than 20 seal failures or rate drops in a minute is unsubscribed for 1, 2, 4 … up to 60
  minutes (`status.topics_paused`), then tried again. A broker can always deny service; two relays and
  the other links are the answer. Flood bytes are still downloaded until the breaker trips.

### 4.7 Scope: what may cross a session

A pair session (contact P) carries, in both directions, only: packets between us and P (DMs, acks,
receipts, files); ANNOUNCE and SOS broadcasts originated by us or by P; hop-local controls. A
channel session carries only that channel's packets plus ANNOUNCE and WHOIS broadcasts (so members
who are not contacts can learn each other's keys), under the unknown-origin budgets. Digests cover
only in-scope packets. Received packets outside the scope are dropped. **Nothing received over a
relay is ever forwarded to an amateur-radio link** (`requires_station_id`), in v1 not even SOS: the
store marks such packets and neither flooding nor reconciliation puts them on those links. There is
no "bridge everything" option in v1.

### 4.8 Channels

Passphrase channels are **not** relayed by default: anyone can precompute topics for common names and
weak passphrases and subscribe on the default brokers. A private channel is relayed only after a
per-channel opt-in (`channels.update {relay:true}`), which the core accepts only when the channel has
a random key (created with `random_key` or joined from an invite that says so) or its passphrase
was rated `strong` when it was created or joined. Stated in the notice and in SECURITY.md.

### 4.9 Fallback, beacons, data

* **The relay is a fallback:** a pair session sends nothing — not even beacons — while the contact has
  been heard directly (`hops = 0`) on another, non-relay link within the last **3 minutes**. When the
  contact goes out of reach, the session resumes and reconciles.
* Sessions report `bitrate_bps` 64 000, beacon every 30 minutes and on connect, plus at most one extra
  beacon per 2 minutes after the store changed; reconciliation pushes at most 8 packets per round.
* Data when idle (measured in `relay M4: a session beacons about every 30 minutes, not like a LAN`:
  12 frames per idle hour on one session, both ends together, with the node's default 30-minute
  re-announcement): per end and hour ≈ 4 beacons (128-byte class, ≈ 205 B with MQTT framing) and 2
  announcements (256-byte class, ≈ 333 B), sent once, received from the contact, and received again
  as the broker's echo of our own (MQTT 3.1.1 has no "no local") ≈ 4.5 KB/h per session per relay,
  plus ≈ 8 B of PINGREQ/PINGRESP every ≈ 4 minutes per relay. 20 contacts, one relay: ≈ 90 KB/h
  (≈ 2 MB/day) while the app is open; nothing while it is closed. TLS/WebSocket framing adds a few
  percent. A session whose contact is reachable directly sends nothing (§4.9 fallback).
* One WebSocket per enabled relay; several relays at once are fine (the mesh deduplicates).
* MQTT keepalive random in 200…280 s per connection; empty client id; no User-Agent identifying
  EmberMesh; WebSocket messages ≤ 8 KiB.
* Reconnect (host): exponential backoff 2 s … 5 min, ±20 % jitter, reset after 2 minutes connected.
* Phones keep relays only while the app is in the foreground (Android: also while its foreground
  service runs). No push: a message for a phone whose app is closed waits in the sender's store
  until both are online.

### 4.10 Consent and the notice

The internet relay is the first feature that contacts third-party servers. It is **off** until the
user turns it on, at first run or when adding a contact. One switch; then it works for every contact
(and every channel the user opted in, §4.8). **Nothing is sent before consent:** the core refuses
`relay` links (`denied`) and the relay driver never connects while `connectivity.internet` is false
or `ready` is false. The notice is produced by the core (`connectivity.get` → `internet.notice`)
from the enabled relays, e.g. with the defaults:

> "Also send messages to your contacts over the internet when they cannot be reached nearby. They go
> through 1 relay server run by others (broker.hivemq.com) — free services with no guarantee unless
> you added your own. The relays, and anyone watching them, cannot read your direct messages. The
> relays can see your IP address (and so roughly where you are), when you are online, roughly how
> many contacts you have and which other users you exchange messages with, and can follow this across
> days and networks. Your contacts can see when you come online. Private channels go over the relay
> only if you turn it on for a channel with a strong key. With one relay, reaching your contacts over
> the internet depends on that one free service; your own hub is the robust option, and more relays
> can be added under Settings > Connections."

### 4.11 TLS and certificates

* Relay URLs MUST be `wss://`; certificate chain and host name MUST be validated against the
  platform's trust store; there is no "insecure" option for relays. Plain `ws://` is accepted only for
  a relay the user added with `allow_plaintext: true`, and only for a LAN, link-local or `.local`
  host; never as a fallback. The core enforces this on `connectivity.set` and on `links.add` (it
  refuses a relay link whose endpoint config has `insecure` or a `ws://` URL without
  `allow_plaintext`); hosts enforce it in the endpoint too (relay-ness = `protocols` contains `mqtt`).
* Android: `SSLSocketFactory` + hostname verification; iOS: `URLSessionWebSocketTask` with an
  ephemeral session; web: the browser.
* Desktop host: vendored **Mbed TLS 3.6.2** (Apache-2.0), client-only TLS 1.2 (TLS 1.3 needs PSA crypto,
  deferred), trust anchors from the OS store (macOS Security framework, Windows
  `CertOpenSystemStore("ROOT")`, Linux the distribution CA bundle); `SSL_CERT_FILE` replaces the OS
  anchors on every platform. Verification cannot be turned off. The desktop reports
  `websocket_tls:true` and `"relay"`; a build without TLS reports neither and never uses plaintext.

### 4.12 The hub as a relay

`emberd` gains a `/mqtt` endpoint on **its own listener and port** (no other routes, not the hub
port): a minimal MQTT 3.1.1 broker reusing `core/src/framing/mqtt.*`. It accepts only topic filters
and topics of exactly 32 lowercase hex characters (no `#`, `+`, `$…`), denies PUBLISH elsewhere,
ignores retain, will and QoS > 0, and caps: 200 clients, 4 per source address, 256 subscriptions
and 4 KiB per message per client, per-client message rates. Beyond 256 subscriptions it answers
SUBACK 0x80 and the client reports those sessions `refused` (≈ 250 contacts and channels per device). It has no TLS:
reach it through a TLS reverse proxy that forwards only `/mqtt`, or opt in to plaintext for a LAN
address.

### 4.13 MQTT client bounds (normative for the core driver)

Remaining length ≤ 4 bytes and minimal; packets > 4400 bytes refused before buffering; ≤ 8 KiB per
WebSocket message and ≤ 64 KiB unparsed in total; PUBLISH topics exactly 32 lowercase hex and
currently subscribed, else counted and dropped; from the server only CONNACK (once, after CONNECT),
SUBACK/UNSUBACK for outstanding ids, PINGRESP, PUBLISH QoS 0 without RETAIN/DUP — anything else
closes the session; a SUBACK 0x80 marks that topic refused, never for good: it is asked for again
after 1, 2, 4 … 60 minutes and on every new connection (one hostile or transient refusal must not
cut a contact off); CONNACK within 20 s, SUBACK within 30 s,
PINGRESP within the keepalive, or the session closes. Fuzz targets: client decoder and the
broker-side parser.

### 4.14 Key compromise and blocking

All pair and channel secrets derive from the static identity key (PROTOCOL §4, no forward secrecy).
Someone who seizes a device can therefore (a) decrypt all relay traffic of that node any broker or
watcher recorded, past and future; (b) compute every future topic, and with a broker's help watch
every contact's IP and online times until they delete the contact; (c) impersonate any contact to
that node (key-compromise impersonation, an existing limitation now reachable globally). Former
contacts keep the pair secret: the core stops publishing and subscribing for blocked and deleted
contacts at once. **Future work:** an authenticated rendezvous re-key (ephemeral X25519 over the
PairAead path replacing `R`, ratcheted per epoch with old values erased) to give forward secrecy
for past epochs.

## 5. Per-platform requirements (contract: RPC_API §11)

* **All hosts:** start nothing before `connectivity.get.ready`; follow `connectivity.changed`; create
  and remove relay links; pass `auto`/`auto_role`/`auto_key` through; close endpoints on `link.removed`;
  call `connectivity.network_changed` on network changes; report `features` `"nearby"`/`"relay"` and
  `websocket_tls`.
* **iOS:** NWListener/NWBrowser per §3.2–3.4 (bind to Wi-Fi; on-link filtering), hello is sent by the
  core; `URLSessionWebSocketTask(with:protocols:)` with an ephemeral session; Local Network prompt
  at the first-run consent. (Automatic BLE deferred, §3.5.)
* **Android:** NsdManager per §3.2–3.4; websocket: `Sec-WebSocket-Protocol`, refuse `insecure` and
  plaintext for relay sockets, no identifying User-Agent, 8 KiB message cap. (Automatic BLE deferred, §3.5.)
* **Desktop:** mDNS (§3.10); mbedTLS (§4.11); `/mqtt` broker on its own port (§4.12).
* **Web:** relay links via `WebSocket(url, ["mqtt"])`; UI per RPC_API §11.

## 6. Self-review (attacker's view, revision 2)

* *Wi-Fi attacker impersonating a contact:* no — contacts are recognised by keys, and duplicate
  removal acts only on proven identities and only on nearby links.
* *Filling nearby slots:* bounded (16 links, per-address and per-minute limits); unproven links are
  evicted first; a contact that proves itself always gets a slot unless 16 proven devices are present.
* *Web page injecting frames into the listener:* refused by the hello (§3.3).
* *Tracking by stable identity over Nearby:* real; disclosed before it starts; off-able; "approved
  networks" mode specified for later.
* *Broker linking days and networks:* real; stated in the notice (§4.3, §4.5).
* *A contact learning my other contacts through sync:* impossible — one engine link per session (§4.2).
* *Channel member with a wrong clock:* replay state per sender; epoch bounds (§4.6).
* *Weak channel passphrases:* not relayed unless random-key or strong, opt-in (§4.8).
* *Retained-frame replay:* retain/DUP/QoS>0 dropped; per-sender windows; small push budget.
* *Internet traffic onto amateur radio:* never (§4.7).
* *Unset clock:* a node without a sane clock (before 2024-01-01) does not connect to relays
  (`mqtt_state:"waiting_for_time"`); a node whose only contacts are over the relay cannot get time
  from them and needs the system clock, GPS or a radio/nearby neighbour first.
* *Key compromise:* §4.14.
* *Traffic analysis:* sizes padded to six classes; timing and counts visible; file sizes leak to ≈ 4 KiB.
