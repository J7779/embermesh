# EmberMesh wire protocol (version 2)

Normative description of what goes on the air / wire between EmberMesh nodes. Implementation:
`core/src/proto/` (encoding, security) and `core/src/mesh/` (behaviour). All multi-byte integers
are **big-endian** unless stated; `varint` is unsigned LEB128 (max 10 bytes); `str8` is a `u8`
length followed by that many bytes of UTF-8. Receivers MUST reject (drop, count, never crash)
anything that does not parse exactly as described, including non-zero reserved bits.

Design goals, in order: work over 1200-baud half-duplex radio with 60-byte MTUs; survive loss,
partitions, garbage and malicious peers; end-to-end security without infrastructure; never put
ciphertext on links where encryption is not allowed.

## 1. Layers

```
 link driver frame (KISS/AX.25/LoRa/UDP/WebSocket...; integrity checked by the driver)
   └─ mesh frame  = frame header + one of: packet | fragment | control      (hop-by-hop)
                      packet = header + body + auth tag                      (end-to-end, stored)
```

A **link** reports `LinkCaps` (`core/include/ember/link.h`): `mtu`, `bitrate_bps`, `broadcast`,
`half_duplex`, `encryption_allowed`, `requires_station_id`, `duty_cycle_permille`, `cost`. Links
with `mtu < 32` cannot carry the protocol.

## 2. Mesh frame

| Bytes | Field |
|---|---|
| 1 | `b0`: bits 7–6 version (`10` = 2; version 1 frames are rejected), bits 5–4 type (0 packet, 1 fragment, 2 control, 3 reserved), bit 3 `S` station id present, bits 2–0 reserved (0) |
| [S] 1 + n | station id: `u8 n` (1..16) + callsign `[A-Z0-9/-]` |
| 4 | `hop_src`: first 4 bytes of the transmitting node's id |
| rest | body (non-empty) |

Overhead: 5 bytes (+1+n with a station id). On links with `requires_station_id` **every** frame
carries the operator callsign; a node without a callsign MUST NOT transmit on such a link. Frames
whose `hop_src` equals our own are echoes and are ignored.

### 2.1 Fragments (type 1)

Any unit (a packet or a control message) that does not fit the link MTU is sent as fragments.
The *unit* is `u8 inner_type (0 packet / 2 control) || content`, max 4097 bytes.

| Bytes | Field |
|---|---|
| 2 | `frag_id` (per sender, per link) |
| 1 | `index` (< count) |
| 1 | `count` (2..128) |
| [index 0] 2 | `total_len` of the unit (2..4097) |
| rest | data (non-empty) |

Receivers reassemble per `(link, hop_src, frag_id)` with hard caps (16 reassemblies, 128 KiB per
link); the concatenation must equal `total_len`. A receiver missing fragments after an idle time
of `max(1.5 s, 3 × frame airtime)` sends at most two **FRAG_NACK** controls; the sender keeps
recently fragmented units for 120 s and resends missing fragments at most 3 times.

### 2.2 Control messages (type 2, hop-local, never relayed)

Body = `u8 ctrl` + payload.

**BEACON (1)** — `node_id[8] | u8 flags (bit0 relay, bit1 has_time) | [u64 mesh_time_ms | u8
time_quality] | varint store_count | u32 store_sum`. `short(node_id)` MUST equal `hop_src`.
`store_count/sum` summarise the packets this node offers for reconciliation *on this link*
(count, and sum mod 2³² of their sync ids). Sent every 30 s (≥100 kbit/s), 120 s (≥2400 bit/s)
or 600 s (slower), plus soon after the store changes or a new neighbour appears (minimum gaps
2 s / 10 s / 60 s).

**SYNC (2)** — `u32 target (hop short id) | u8 n (≤64) | n × item`, items:
* `DIG  = 1 | u8 bits | u32 prefix | 16 × (u16 count, u32 sum)` — digests of the 16 children
  (next 4 bits) of a prefix of the 32-bit sync-id space.
* `IDS  = 2 | u8 bits | u32 prefix | u8 k | k × u32 sync_id` — exact ids under a prefix.
* `WANT = 3 | u8 k | k × u32 sync_id` — please send these packets.

`bits` is a multiple of 4, ≤ 28, and `prefix` has its low `32-bits` bits zero; ids in an IDS item
must match its prefix. See §7.

**FRAG_NACK (3)** — `u32 target | u16 frag_id | u8 count | bitmap[(count+7)/8]` (bit i = missing).

## 3. Packet

| Bytes | Field |
|---|---|
| 1 | `p0`: bits 7–5 `sec`, bits 4–3 `dst`, bits 2–1 `prio`, bit 0 `ack` |
| 1 | `kind` (≠ 0) |
| 1 | `hop`: bits 7–4 `ttl`, bits 3–0 `hops` — **mutable**, excluded from all authentication |
| 1 | `lim`: bits 7–4 `hop_limit` = the originator's `ttl` (authenticated), bits 3–0 reserved (0) |
| 4 | `pkt_id` (random, ≠ 0) |
| 8 | `origin` node id |
| 0 / 8 / 4 | destination: none (`dst`=0 broadcast), node id (`dst`=1), channel id (`dst`=2) |
| 4 | `created`: seconds since 2020-01-01T00:00:00Z, sender's mesh time |
| 2 | `lifetime` in minutes (≥ 1) |
| [ack] 8 | ack commitment (only with `dst`=node) |
| [salt] 8 | nonce salt (only with `sec` PairAead / ChanAead), see §4 |
| … | body |
| 0/16/64 | tag (by `sec`) |

`sec`: 0 None (ACK/WHOIS only), 1 Signed, 2 PairMac, 3 PairAead, 4 ChanAead. PairMac/PairAead
require `dst`=node; ChanAead requires `dst`=channel; `ack` requires `dst`=node. `prio`: 0 low,
1 normal, 2 high, 3 emergency. Max packet 4096 bytes. Header: 22 bytes broadcast, 30 DM, 38 DM
with ack commitment, +8 for AEAD modes (a typical encrypted, acknowledged DM: 46 + body + 16).
Receivers reject `ttl > hop_limit` and non-zero reserved bits in `lim`.

**Identity of a packet.** `wire_id` = BLAKE2b-64("ember-wire-v1" ‖ packet with hop byte zeroed)
— any modified copy is a different packet, so forged look-alikes cannot pre-empt genuine ones
in duplicate caches. `sync_id` = high 32 bits of `wire_id`. The message id seen by applications
is `origin ‖ pkt_id` (24 hex chars).

**TTL.** `ttl` is the number of further relays allowed. An originator sending with hop limit N
writes `ttl = hop_limit = N-1, hops = 0`. A receiver of `(t, h)` may forward only if `t > 0`, as
`(t-1, h+1)`; this also applies to reconciliation transfers (strict hop limit). Because the hop
byte is unauthenticated, `hop_limit` is authenticated: no copy may carry `ttl > hop_limit`, and a
duplicate with more hops left than the stored copy upgrades it (once per stored packet) only
while `ttl + hops <= hop_limit`. A forger can therefore never extend a packet beyond its
originator's limit (only re-start it from its own position within that limit). Relays may lower
`ttl` (§11).

**Expiry.** `expires = created + lifetime`, capped at now + 30 days. A `created` before
2024-01-01 (unset clock) or more than 1 h in the future is treated as "now". A packet that looks
expired by at most 24 h (clock disagreement with its sender) is accepted and kept for
min(lifetime, 1 h); older ones, and packets older than the maximum lifetime + 24 h, are dropped.
Stored packets expire against the node's **monotonic** clock (remaining lifetime fixed at
reception), so no change of mesh time — by the mesh, a GPS fix or an operator — can purge a store.

## 4. Security

Identity (`core/src/crypto/identity.h`): a 32-byte seed derives an Ed25519 signing key
(`BLAKE2b-256(key=seed, "ember-sign-v1")` as seed) and an X25519 key
(`BLAKE2b-256(key=seed, "ember-dh-v1")`). `node_id` = first 8 bytes of
BLAKE2b-256("ember-node-id-v1" ‖ sign_pk ‖ dh_pk). Fingerprint: 8 words (64 bits), one per byte
of BLAKE2b-256("ember-fp-v1" ‖ sign_pk ‖ dh_pk), from a fixed 256-word list (the 6-word
fingerprints of version 1 are a prefix). The QR contact card remains the strongest check.

Let `AD` = the packet header bytes (offset 0 up to the body) with the hop byte zeroed.

| sec | Construction |
|---|---|
| Signed | tag = Ed25519(sign_sk, "ember-sig-v1" ‖ AD ‖ body) |
| PairMac | tag = BLAKE2b-128(key=K_pair, "ember-mac-v1" ‖ AD ‖ body) |
| PairAead | body ‖ tag = XChaCha20-Poly1305(K_pair, nonce, AD, plaintext) |
| ChanAead | body ‖ tag = XChaCha20-Poly1305(K_chan, nonce, AD, plaintext ‖ Ed25519 sig over "ember-sig-v1" ‖ AD ‖ plaintext) |
| None | no tag (ACK, WHOIS) |

* `nonce` = BLAKE2b-192("ember-nonce-v2" ‖ AD), where AD includes the header's 8-byte `salt` =
  BLAKE2b-64(key=K, "ember-salt-v2" ‖ u8 len ‖ entropy ‖ header-with-zero-salt ‖ plaintext),
  `entropy` = 16 fresh random bytes. The nonce is random per packet, and even with a repeated
  header and a broken RNG two different plaintexts never share a nonce (hedged / SIV-style).
* `K_pair` = BLAKE2b-256(key = X25519(my_sk, their_pk), "ember-pair-v1" ‖ min(pk_a, pk_b) ‖
  max(pk_a, pk_b)) — the same in both directions. All-zero X25519 results are rejected.
* `K_chan` = Argon2id(passphrase, salt "ember-channel-v1:" ‖ name, 8 MiB, 3 passes, 1 lane);
  `name` is normalised (ASCII lower case, whitespace trimmed/collapsed, ≤32 bytes). Channel id =
  u32 of BLAKE2b-32(key=K_chan, "ember-chan-id-v1"). Open (cleartext) channels use id = u32 of
  BLAKE2b-32("ember-open-channel-v1:" ‖ name) and `sec` Signed.
* Public channel = `dst` broadcast, `sec` Signed.

Which mode is used: DMs (text, file, app, position) PairAead; read receipts, ping, pong PairMac;
SOS-ack Signed to a node; ACK None; channels ChanAead or Signed; broadcasts Signed (WHOIS None).
There is no forward secrecy (static-static DH); this is a documented limitation.

**Verification.** Announcements are self-certifying (§5) and always verified. Relays verify
signed packets whenever they know the origin's key and drop failures; when a key arrives later,
stored packets from that origin are verified then (forgeries removed). A recipient that lacks the
sender's key holds the packet (≤64, ≤5 min) and broadcasts a WHOIS; if the key arrived meanwhile
the packet is verified, otherwise signed cleartext is delivered marked *unauthenticated* —
except anything that cancels or withdraws (SOS cancel, bulletin delete, read receipt), which is
dropped. An active SOS from an unknown sender is delivered at once, flagged unauthenticated and
rate-limited (§11). An authenticated copy of a message id later supersedes an unauthenticated
delivery of the same id.

**Legal mode.** On links with `encryption_allowed = false` only packets with `sec` ∈ {None,
Signed, PairMac} (cleartext, possibly authenticated) may be transmitted — including relays,
fragments and reconciliation, whose digests also cover only those packets. PairAead/ChanAead are
never queued for such links. Together with §2's station id this is the amateur-radio rule.

**Replay.** Duplicate cache of wire ids (8192), expiry, the ack commitments below, and a replay
memory of every authenticated delivery keyed by the exact message id (no lossy hashing) until its
expiry + 24 h, persisted across restarts (≤ 32768, evicting the soonest to expire;
unauthenticated deliveries use a separate 4096-entry FIFO and never displace them). Verified acks
(matching a packet we hold or sent) and unverified acks are kept apart (16384 / 4096, FIFO), so
junk acks cannot flush real ones. Hash tables indexed by message ids use SipHash-2-4 with a
per-node random key.

**Shared identities.** Two devices restored from the same identity backup are one node: each
drops the other's packets as its own echoes, so nothing originated by one is relayed or delivered
through the other. Use one identity per device.

**Acknowledgements.** A sender requesting an ack puts `commit = BLAKE2b-64("ember-ackc-v1" ‖ s)`
in the header, with `s = BLAKE2b-64(key=K_pair, "ember-ack-v1" ‖ origin ‖ pkt_id)`. Only the
recipient (and sender) can compute `s`. The ACK packet carries `s` in cleartext; any relay holding
the original verifies `commit` and purges its copy, so acks are self-authenticating and cannot be
forged by third parties.

## 5. Payload kinds (body after decryption)

`pos` = `u8 flags (bit0 alt, bit1 accuracy) | i32 lat×1e7 | i32 lon×1e7 | [i16 alt_m] | [u16
acc_m]`. `text` = textpack blob (§8) filling the rest of the body.

| Kind | Body |
|---|---|
| 0x01 TEXT | `u8 flags (bit0 reply)` [`origin[8] u32 pkt_id`] `text` (≤2000 bytes) |
| 0x02 ACK | `u8 n (1..16)` × (`u32 pkt_id`, `s[8]`); `dst` = original sender |
| 0x03 READ | `u8 n (1..64)` × `u32 pkt_id` |
| 0x04 ANNOUNCE | `sign_pk[32] dh_pk[32] u8 flags(bit0 relay) u8 caps str8 name(≤32) str8 callsign(≤16) str8 status(≤64)`; must be Signed broadcast, `origin` must equal node_id(keys), verified with the enclosed key |
| 0x05 POSITION | `pos` |
| 0x06 SOS | `u8 flags (bit0 active, bit1 pos) u16 seq` [`pos`] `text`; active=0 cancels |
| 0x07 SOS_ACK | `u32 sos_pkt_id (0 = any)` `text` |
| 0x08 CHECKIN | `u8 status (0 ok,1 need_help,2 injured,3 moving,4 sheltering) u8 flags(bit0 pos)` [`pos`] `text` |
| 0x09 BULLETIN | `u8 category (0 water,1 food,2 medical,3 shelter,4 hazard,5 trade,6 missing,7 transport,8 info) u8 flags (bit0 pos, bit1 delete)` [`u32 delete_pkt_id`] [`pos`] `str8 title` `text`; expiry = packet lifetime |
| 0x0A FILE | `u32 file_id u16 index u16 count (≤1024)` [index 0: `u32 total str8 name str8 mime hash[16]` (BLAKE2b-128 of file)] `data`; chunk *i* uses `pkt_id = file_id + i` (0 → 1); only chunk 0 requests an ack, sent once the whole file is verified |
| 0x0B NETPLAN | `u8 n (≤16)` × (`str8 label, varint freq_hz, str8 mode, str8 tone, str8 schedule, str8 notes`) |
| 0x0C PING / 0x0D PONG | `u32 nonce` / `u32 nonce u8 ping_hops` |
| 0x0E WHOIS | `target node_id[8]`; target answers with a fresh ANNOUNCE, others holding its announce re-send the stored copy (randomised delay, suppressed if heard) |
| 0x0F APP | `u16 app_type` + opaque bytes (for later app features) |

NETPLAN `mode` is free text (≤ 64 bytes); senders should use the canonical names — the modem
catalogue names (`afsk1200`, `afsk300`, `psk2400`, `psk4800`, `robust250`, `robust125`, `robust62`,
`robust31`, `cw`, `rtty`, `psk31`, `dtmf`) or `voice`, `lora`, `meshtastic`, `other`. Receivers map
old spellings (`bpsk31` → `psk31`, `morse` → `cw`, case / separators ignored) and keep unknown
values as they are.

Unknown kinds are relayed and stored but not delivered. **Replaceable kinds** keep only the
newest (by `created`, then `pkt_id`) per origin in stores: ANNOUNCE, CHECKIN, NETPLAN, SOS,
and POSITION (per origin and destination) — but an unverified packet never replaces a verified
one, an unverified SOS cancel replaces nothing, and a `created` more than 10 min in the future
never wins. ANNOUNCE flags: bit0 relay, bits 7–4 the sender's clock quality when it signed
(§9), bits 3–1 reserved (0).

Default lifetimes: announce 7 d, position/SOS/SOS-ack/read 1 d, check-in/netplan 2 d,
ping/pong/whois 10 min, everything else the configured store-forward time (3 d).

## 6. Forwarding

* **Managed flooding.** A new, valid, forwardable packet (not addressed only to us) is scheduled
  on every up link where it is allowed. On the link it arrived on (broadcast media only) the
  delay is `f·W + U(0, W)` with `W = max(40 ms, 3 × airtime)` and `f` ∈ [0,1] from SNR
  (`(snr+5)/20`) or RSSI (`(rssi+110)/60`): weak receivers — far from the sender — relay first.
  A pending relay is cancelled after hearing 2 further copies (suppression). Bridging to other
  links uses a short jitter. Point-to-point links never echo back.
* **Priority queues** (strict, 0 highest): 0 SOS/emergency, 1 ACK/READ/PONG/FRAG_NACK,
  2 direct messages, 3 channel/public text and check-ins, 4 announce/position/bulletin/netplan/
  beacons, 5 low priority, 6 reconciliation and file chunks. Per-link queue cap 64 KiB; lower
  classes are dropped first; queued frames go stale after 10–30 min.
* **Airtime.** `airtime = bytes·8/bitrate + key-up` (250 ms ≤2400 bit/s, 50 ms ≤20 kbit/s,
  2 ms otherwise, half-duplex only). A token bucket of `duty/1000 × elapsed` (capacity 10 min of
  budget) enforces `duty_cycle_permille`; the engine paces slow links itself (one frame in the
  driver at a time) so priority holds, and respects carrier sense.
* **End-to-end reliability.** Ack-requesting packets are retransmitted at 20 s, 40 s, 80 s, …
  (cap 30 min, 6 tries) and the store keeps them until acked or expired. A recipient seeing a
  duplicate of something it already acknowledged sends a fresh ACK (≤ every 15 s).

### 6.1 Slow shared channels (half duplex below 20 kbit/s)

Airtime estimates used for budgets and scheduling on these links include ~20 bytes of link-layer
framing per frame, 5 % HDLC bit stuffing and a 350 ms key-up (60 ms above 2400 bit/s), so the
duty-cycle token bucket (§ duty cycle) never lets the real transmit time exceed the budget.

Behaviour only — no wire change; old and new nodes interoperate. Applies to links whose caps say
`half_duplex` and `bitrate_bps < 20000` (sound-card AFSK/robust modes, KISS TNCs, LoRa), unless
`MeshConfig::slow_channel_etiquette` is off. Let `A` = airtime of a 128-byte frame on the link
(≈1.1 s at 1200 bit/s, ≈4.4 s at 250 bit/s).

* **Listen-before-talk holdoff.** After hearing any frame from another station, a node hands
  nothing new to its driver until `heard + base + r·span`, with one uniform `r ∈ [0,1)` drawn per
  frame heard: SOS/emergency `0.25A + r·0.5A`; acks, direct and channel messages `A + r·2A`;
  beacons, announcements, WHOIS, reconciliation and everything else `3A + r·4A`. The station that
  just transmitted is not held (its continuation gets the channel), listeners that react to the
  same frame are spread out instead of keying up together inside the modems' carrier-detect
  latency, and user traffic always goes before housekeeping.
* **No reactive WHOIS for a beacon.** A stranger's beacon no longer triggers a WHOIS on these links
  (it still does on fast links). Keys are asked for only when something from the stranger actually
  needs verifying.
* **Announcement piggy-backed on a first message.** When a node originates user traffic (text,
  position, check-in, bulletin, SOS, file, app, ping) and it has not announced yet, or has heard a
  new neighbour on a slow link since its last announcement, or announced more than 30 min ago (at
  most once per 10 s), it first queues its signed announcement at the message's priority, so both
  go out in one key-up and receivers verify the message at once. The self-certifying signature
  check and the admission budgets for new identities (§11) are unchanged; a scheduled start-up
  announcement is skipped if one was sent within the last minute.
* **Reply to a new neighbour.** Stations may be switched on in any order, so the first time a node
  hears a neighbour on such a link it schedules one announcement of its own at `U(A, 5A)` later
  (several newcomers coalesce into one; then the holdoff above applies, as housekeeping). It is
  skipped if the node announced after that neighbour was first heard, or if every neighbour heard
  since its last announcement has already proven it holds our keys (it sent us a pairwise-
  authenticated packet directly, `hops = 0` from its own origin), and sent at most once per minute
  per link. This replaces the beacon-triggered WHOIS round trip.
* **Relaying on one shared channel.** A pending relay on the link the packet arrived on is cancelled
  after hearing one further copy (instead of two), and is not scheduled at all when the sender is
  the only neighbour heard on that link (it already has the packet).
* **WHOIS answered even right after announcing.** A WHOIS for us within 30 s of our last
  announcement (normally ignored to bound abuse) is answered on these links by re-sending that
  same stored announcement, at most every 10 s per link: the asker evidently missed it in a
  collision. No new packet is ever originated for it.
* **Fast repair of a fresh message.** For each of its own fresh user packets of priority ≤ 3 on
  such a link, a node waits `3A + U(0, 3A)` after the packet went on air for evidence that a
  neighbour holds it (a relayed or re-sent copy heard, or — with a single neighbour — a beacon
  whose digest equals ours). Without evidence it sends the packet again (preceded by the
  announcement that was piggy-backed on it, once), at most 2 times, only while a neighbour has been
  heard on the link (a repair waits up to 10 min for the first neighbour), at most 8 pending
  repairs per link. Everything else is left to reconciliation (§7).

Measured with real audio links through a summed half-duplex channel
(`core/tests/mesh/mesh_air_test.cpp`, "air long …"; before = `slow_channel_etiquette` off). First
public message between strangers, delivered to every node, median / p90 / max:
AFSK1200 2 nodes 5.9/7.8/27.9 s → 3.8/4.9/5.2 s; 3 nodes 31/125/244 s → 4.4/6.5/7.0 s; 5 nodes
62/142/145 s → 3.9/6.2/7.0 s; with the first copy destroyed: 2 nodes 27/134/144 s → 14/18/20 s,
3 nodes 47/142/268 s → 13/16/21 s. robust250: 2 nodes 24 → 20 s, 3 nodes 100 → 21 s, 5 nodes
122 → 23 s (median). Overlapping transmissions drop to ≈0. Stations switched on in random order up
to a minute apart all hold each other's keys after (median / max) AFSK1200 18/49 s (2 nodes),
53/67 s (3), 82/203 s (5); robust250 58/79 s (2), 140/171 s (3), 292/559 s (5). A node originated
at most 4 announcements in 30 min doing so. With a 10 % duty-cycle limit no node used more than
23 s of its 360 s hourly budget.

## 7. Store-carry-forward and reconciliation

Every node stores every storable packet (all but PING/PONG/WHOIS; DMs to itself that it
acknowledges are replaced by the ACK) until expiry, within a quota (default 4 MiB / 5000
packets). Eviction order: expired, then lowest value class (file < position < check-in <
bulletin/netplan < text < announce < ack < SOS; emergency/high priority raise, own undelivered
traffic is kept longest), oldest first.

When a beacon's `(count, sum)` differs from ours, the node with the lower `hop_src` (or either,
after 2 sync gaps) starts a round with `DIG(bits=0)`. For each differing child a receiver answers
`IDS` if it holds ≤16 ids there (or at 28 bits), else a deeper `DIG`. On `IDS` a node pushes the
packets the peer lacks (as ordinary packet frames, priority 6, ≤48 per message) and replies
`WANT` for the ones it lacks. An empty SYNC reply means "nothing differs". Rounds repeat every
sync gap (4 s / 20 s / 120 s by speed class) while a mismatch persists; rounds that were answered
but moved nothing back off exponentially (≤30 min). Steady-state cost: one beacon per interval.

Value classes put packets from unknown origins below everything else, and they may occupy at
most 10 % of the quota (count and bytes), displacing only each other (§11).

Reconciliation is bounded per neighbour: SYNC is only answered for neighbours that sent a beacon,
at most 240 (fast) / 30 (slower) SYNC messages per minute (burst 60 / 16), 16 DIG items and 512
ids per message, and 64 KiB / 16 KiB / 4 KiB of pushed packets per round (fast / medium / slow).
The store is indexed by sync id, so a WANT for ids we lack costs a hash lookup. After a node
originates a message it advertises its new digest early (post-transmission quiet time ≥ 2 s, then
a beacon even inside the normal gap, at most every 15 s), and a node whose neighbour advertises
more packets than it holds starts a round at once — a broadcast lost to a collision is recovered
in seconds on slow shared channels.

**Bundles** (sneakernet: files, QR codes): `"EMB1" | varint count | count × (varint len |
packet) | u32 CRC-32 of everything before`. Importing a bundle is equivalent to receiving each
packet from a link (validated, de-duplicated, stored, delivered, then flooded on).

## 8. Text compression (textpack)

`u8 method` then: 0 = raw bytes; 1 = `varint length` + MSB-first bitstream of a fixed canonical
Huffman code over bytes (built deterministically from the static English/chat frequency table in
`core/src/base/textpack.cpp`, max code length 15). The encoder picks the smaller. Decoders reject
lengths beyond their limit, truncated streams and more than 7 bits of padding.

## 9. Mesh time

Each node keeps `mesh = monotonic host clock + offset` and a quality 0 (GPS/manual) … 8 (sane
system clock) … 15 (unknown, e.g. a clock before 2024).

* **Local clocks win.** GPS / manual time and a sane system clock (quality < 15) are never moved by
  the mesh. Host clock steps backwards are applied once; timers use the monotonic clock.
* **Evidence is authenticated.** Only the `created` time of a signed ANNOUNCE heard directly from
  its origin (origin's short id = the frame's `hop_src`, `hops` = 0), with the clock quality the
  origin signed into it, is a time sample. BEACON time fields are informational only and MUST NOT
  set clocks (they are unauthenticated).
* **Multi-origin agreement.** A node without a usable clock (quality 15) adopts the median of the
  largest ±60 s cluster of the best-quality fresh samples (30 min): from pinned contacts if any
  (one suffices), otherwise at least two distinct origins forming a strict majority of that class.
  Its quality becomes max(9, best + 1); later corrections are slewed (≤ 1 s per minute of uptime,
  at least 1 s per sample). Values outside 2024–2100 are rejected; saved offsets that put a sane
  host clock outside that range are discarded.
* **Robust expiry.** See §3: mesh time can never purge stored data, and fresh traffic is accepted
  across up to 24 h of clock disagreement.

Residual: a clock-less node surrounded only by sybil identities can be given a wrong time; it
cannot pass it on to nodes with clocks, and its store is unaffected.

## 10. Bounds (receiver side)

Neighbours 64/link, reassemblies 16/link, pending-verification 64, peers 2048 (pinned contacts
never evicted, keyless first), per-origin rate 120 packets/min (burst 300), admission budgets
(§11), 400 frames/s/link and 200 per transmitter, 8 concurrent
incoming files ≤128 KiB, outgoing tracking 2048, WHOIS ≤10/min. Every parser rejects trailing
bytes, bad lengths and invalid UTF-8.

## 11. Admission control (unknown and unverified senders)

Self-certifying identities are free to make, so "authenticated" says only that a packet comes from
*some* key. Every received packet gets a trust class:

* **verified** — signature / MAC / AEAD checked by us, a self-certifying ANNOUNCE, or an ACK whose
  commitment matches a packet we hold or sent;
* **known** — the origin's key is known but we cannot check this packet (someone else's DM or
  channel, an ACK for a packet we never held);
* **unknown** — no key for the origin (anything it claims is unverifiable).

Rules:

* Budgets (token buckets, per minute / burst): unknown origins 30/30 per neighbour (`hop_src`),
  60/60 per fast link (20/20 on links < 100 kbit/s), 120/120 globally; known-unverifiable 600/1200
  per fast link (120/240 slower). New identities (first ANNOUNCE of a key) 30/100 per link and
  60/200 globally. Over budget: dropped (not stored, not relayed). Bundles are local input: only
  the storage share applies.
* Unknown-origin packets are relayed with `ttl` lowered to at most 2 (SOS) or 1 (anything else),
  stored in the lowest value class within a 10 % quota share, and create no peer-table entries.
  When the origin's key becomes known they are verified and promoted (or removed).
* Unverified packets never replace or cancel verified state (§5, §4): no SOS cancel, bulletin
  withdrawal or read receipt from an unverified sender takes effect.
* SOS from strangers: delivered at once, flagged unauthenticated, at most 6/min per link and
  12/min in total; nodes keep them in a separate small alert table so they cannot crowd out
  verified alerts (node layer, docs/RPC_API.md).
* Peer table: keyless entries are evicted first, pinned contacts never. The node never deletes a
  conversation the user wrote in, or one with a contact, to make room for strangers; when only
  such conversations remain, a new stranger's conversation is refused.
* Per-link frame cap 400/s, and 200/s per transmitting `hop_src`.

## 12. Internet relay framing

Links of type `relay` carry mesh frames over MQTT 3.1.1 (QoS 0) through a broker (design and threat
model: [CONNECTIVITY.md §4](CONNECTIVITY.md#4-internet-relay)). The broker is untrusted, and on public
brokers anyone may be able to watch topic names, sizes and timing: every frame is sealed, and topics
are derived from secrets only the parties hold.

**Sessions.** A relay link is one MQTT connection. Each *session* — one per contact pair, one per
relayed private channel — is its own mesh link with its own scope (below), beacons, digests,
fragment ids and caps.

**Roots.** Pair: `R = BLAKE2b-256(key = K_pair, "ember-relay-pair-v1")`. Private channel (relayed only
after a per-channel opt-in, only for random-key or strong-passphrase channels):
`R = BLAKE2b-256(key = K_chan, "ember-relay-chan-v1")`. Domain-separated from every other use of the
keys (§4).

**Broker binding.** `host` = the broker's canonical `hostname:port` (lower case, IPv6 without brackets,
default port 443 for `wss://`, 80 for `ws://`, ≤ 200 bytes); `H = u8 |host| ‖ host`.

**Epochs and topics.** `e = floor(unix_ms / 86 400 000)`, Unix time from the node's mesh clock (UTC
days). `topic(e)` = 32 lowercase hex digits of `BLAKE2b-128(key = R, "ember-relay-topic-v1" ‖ u32be e ‖ H)`.
A node subscribes to `topic(E)`, also to `topic(E+1)` during the last 3 hours of day E and to
`topic(E-1)` during its first 3 hours, and publishes on `topic(E)`. A node without a sane clock (before
2024-01-01) neither subscribes nor publishes.

**Message** (the MQTT PUBLISH payload): `u8 version (1) ‖ nonce[24] ‖ ct ‖ tag[16]`, where

* `K_frame(e) = BLAKE2b-256(key = R, "ember-relay-aead-v1" ‖ u32be e ‖ H)`;
* `P = u64be sent_ms ‖ u16be len ‖ frame[len] ‖ zero bytes`, `|P|` the smallest of 128, 256, 512,
  1024, 2048, 4096 that holds `10 + len` (so `1 ≤ len ≤ 4086`; session mtu 4000);
* `nonce = BLAKE2b-192(key = K_frame(e), "ember-relay-nonce-v1" ‖ rnd16 ‖ u64be sent_ms ‖ P)`, `rnd16`
  = 16 fresh random bytes (hedged);
* `AD = "ember-relay-v1" ‖ u8 1 ‖ H ‖ topic` (the 32 ASCII characters);
* `ct ‖ tag = XChaCha20-Poly1305(K_frame(e), nonce, AD, P)`.

Receivers MUST drop a message whose version is not 1, whose `|P|` is not a class or not the smallest
class for its `len`, whose tag fails, whose `len` is 0 or exceeds `|P| - 10`, or whose padding is not
all zero. The frame is an ordinary mesh frame (§2).

**Receiver rules.** PUBLISH packets with RETAIN, DUP or QoS ≠ 0 are dropped. Try `K_frame` of the
epoch the topic belongs to. Drop frames whose `hop_src` is our own (broker echo) before any state
changes. On a pair topic `hop_src` MUST equal the first 4 bytes of the contact's node id. Drop
`sent_ms` outside [start(e) − 1 day, start(e) + 2 days). Replay state per (topic, `hop_src`): drop a
`sent_ms` more than 600 000 ms below that sender's highest; drop a nonce seen in the last 30 minutes
(≤ 16 384 nonces per link, ≤ 64 senders per topic). At most 50 messages/s per topic and 200/s per link;
a topic with more than 20 failures or rate drops in a minute is unsubscribed for 1, 2, 4 … 60 minutes.

**Scope.** A pair session carries only packets between us and the contact, ANNOUNCE/SOS broadcasts
originated by either, and hop-local controls; a channel session only that channel's packets and
ANNOUNCE/WHOIS broadcasts. Digests cover only in-scope packets; out-of-scope packets received on a
session are dropped. A pair session sends nothing while its contact was heard directly (hops 0) on
another non-relay link within 3 minutes. Sessions beacon every 30 minutes (and at most every 2
minutes after the store changed) and push at most 8 packets per reconciliation round. Packets
received over a relay are never transmitted on `requires_station_id` links (stored with a flag that
excludes them from flooding and reconciliation there). Relay links are never amateur links.

**Test vectors** (`K_pair` = bytes 00 01 … 1f, `K_chan` = bytes 20 21 … 3f, `host` =
`broker.example.org:8084`; produced by an independent implementation — hashlib BLAKE2b and an
XChaCha20-Poly1305 checked against RFC 8439 §2.8.2 and draft-irtf-cfrg-xchacha A.3.1):

| Value | Result |
|---|---|
| `R` (pair) | `f2ed9d1556599423401b6154e15a81c09c13353b10f68fa2656e4f980d456451` |
| `R` (channel) | `56525207dbb953de803f3d1f7093580b532e71484e08bf53f4de058ca2834acd` |
| `e` for unix_ms 1763078400000 | `20406` |
| pair `topic(20406)` / `topic(20407)` | `b61f9b3d043471379e905d4aefbe672c` / `9e8c34977c9269c6d0dd31226b015feb` |
| pair `topic(20406)` with host `broker.example.org:443` | `65132cb6b60682b977ed2036b0bf23e1` |
| channel `topic(20406)` / `topic(20407)` | `0a9ec8333ccee061f6f92007dea7837f` / `af251244b409e280a7c9b444aaf1fa1f` |
| pair `K_frame(20406)` | `ad93f89b41b4e5a1caf5ac9506e10b5dfc67a74a1e0c0304aa45ea3492b1f124` |
| pair `K_frame(20407)` | `0368ee5df4a47c0fc44677b6430f45735418219c2776c02782d8a0985467654a` |
| message: pair, e 20406, `rnd16` = a0 a1 … af, `sent_ms` 1763078400123, frame `80 01 02 03 04 55` | 169 bytes, `nonce` = `941851c18244f015e3fb407c67ac3afbe0314c04e8e8de5e`, BLAKE2b-256 of the message = `a438a9455b4cd6489058c38d0083277a946675723ca6f30d801678e52dc88d0e` (full hex in `core/tests/proto/relay_test.cpp`) |

Implementation and tests: `core/src/proto/relay.*`, `core/src/link/relay_link.*`,
`core/tests/proto/relay_test.cpp`, `core/tests/link/relay_test.cpp`.
