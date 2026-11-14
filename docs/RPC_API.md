# EmberMesh RPC API

The contract between the shared UI (`ui/`), the platform host routers, and the core
(`ember_call`). Method names, parameter names and result shapes here are normative: the core, the
mock backend and every host router must agree with this file. Additive changes only.

## 1. Conventions

- Call: `call(method, params) → result`. Over the C API the envelope is
  `{"ok":true,"result":{…}}` or `{"ok":false,"error":{"code":"…","message":"…"}}`.
- Error codes: `bad_params`, `not_found`, `not_implemented`, `unsupported`, `busy`, `denied`,
  `locked`, `too_large`, `io_error`, `internal`.
- Every result is a JSON object (never a bare array/scalar). Unknown params are ignored; unknown
  result fields must be tolerated by callers.
- `node_id`: 16 lowercase hex chars. Message / bulletin ids: lowercase hex strings. Link ids:
  positive integers. Timestamps (`ts`, `*_ts`): Unix milliseconds. Durations: `*_s` seconds or
  `*_ms`. Frequencies: `*_hz`. Binary blobs: standard base64 in fields named `*_b64`.
- Conversation id (`conv`): `dm:<node_id>` for direct messages, `ch:<channel_id>` for channels.
- Events are delivered as `{"event":"<name>","data":{…}}`.
- Methods in the `host.*` namespace are handled by the platform host router (JS / Kotlin / Swift /
  C++), everything else by the core. The UI calls both through the same `backend.call`.

### Field schema (schema-driven forms)

Catalogue methods (`links.types`, `tools.list`, `host.info`) describe their inputs as arrays of:

```json
{"key":"baud","label":"Baud rate","type":"number|text|select|bool|textarea|password",
 "unit":"Hz","default":9600,"min":0,"max":1,"step":1,
 "options":[{"value":"afsk1200","label":"AFSK 1200 (APRS)"}],
 "help":"One line of help.","required":false,"advanced":false}
```

The UI renders a form from the schema and sends back `{key: value}` objects. Numbers are JSON
numbers, bools JSON bools, everything else strings.

## 2. Core methods

### node / identity

| Method | Params | Result |
|---|---|---|
| `node.info` | — | `{version, node_id, name, callsign, status, fingerprint:["word",…], card, platform, time_ms, time_source:"system"|"gps"|"mesh"|"manual", uptime_ms, locked, features:[…]}` |
| `node.set_profile` | `{name?, callsign?, status?}` | same as `node.info` |
| `node.set_time` | `{ts, source}` | `{time_ms}` |
| `identity.card` | — | `{card}` — `ember:` URI with id, name, public keys (for QR / copy-paste) |
| `identity.export` | `{passphrase}` | `{backup}` — passphrase-encrypted text backup of the identity |
| `identity.import` | `{backup, passphrase}` | same as `node.info` |

### contacts

`Contact = {node_id, name, callsign, verified, blocked, favorite, status, last_seen_ts, hops,
direct, via_link, rssi?, snr?, position?:Position, fingerprint:[…]}`
`Position = {lat, lon, alt?, accuracy?, ts}` (WGS-84 degrees, metres)

| Method | Params | Result |
|---|---|---|
| `contacts.list` | — | `{contacts:[Contact]}` |
| `contacts.add` | `{card}` or `{node_id, name?}` | `{contact}` |
| `contacts.update` | `{node_id, name?, verified?, blocked?, favorite?}` | `{contact}` |
| `contacts.remove` | `{node_id}` | `{}` |

### channels

`Channel = {id, name, encrypted, is_default, muted, unread, members_seen}`. The default channel
`Public` is cleartext, signed, and is the only chat traffic allowed on no-encryption links.

| Method | Params | Result |
|---|---|---|
| `channels.list` | — | `{channels:[Channel]}` |
| `channels.create` | `{name, passphrase?, random_key?=false}` (no passphrase → open cleartext channel; `random_key:true` → encrypted channel with a random key, shared only via `channels.invite`) | `{channel, passphrase_strength?:"weak"|"fair"|"strong"}` |
| `channels.invite` | `{id}` | `{invite}` — `ember:` URI |
| `channels.join` | `{invite}` or `{name, passphrase?}` | `{channel}` |
| `channels.update` | `{id, muted?}` | `{channel}` |
| `channels.leave` | `{id}` | `{}` |

### conversations / messages

`Conversation = {id, kind:"dm"|"channel", title, unread, muted, last_message?:Message, last_ts}`

`Message = {id, conv, from, from_name, outgoing, ts, received_ts, text, kind:"text"|"sos"|
"position"|"checkin"|"file"|"interop"|"system", status:"queued"|"sent"|"relayed"|"delivered"|
"read"|"failed"|"received", priority:"low"|"normal"|"high"|"emergency", encrypted, signed_ok,
hops, via_link, expires_ts?, position?:Position, file?:{name, mime, size, complete}, interop?:
{network, from, to}}`

| Method | Params | Result |
|---|---|---|
| `conversations.list` | — | `{conversations:[Conversation]}` (most recent first) |
| `conversations.clear` | `{conv}` | `{}` |
| `messages.list` | `{conv, before_ts?, limit?=50}` | `{messages:[Message], more}` (oldest first) |
| `messages.send` | `{conv, text, priority?="normal", want_ack?=true, ttl_hops?, expires_s?}` | `{message}` |
| `messages.retry` | `{id}` | `{message}` |
| `messages.mark_read` | `{conv}` | `{}` |
| `messages.delete` | `{id}` | `{}` |
| `messages.search` | `{query, limit?=50}` | `{messages:[Message]}` |
| `files.send` | `{conv, name, mime, data_b64}` | `{message}` (small files only; `too_large` beyond the limit) |
| `files.get` | `{id}` | `{name, mime, data_b64}` |
| `interop.send` | `{link_id, to?, text}` | `{message}` — plain text to non-Ember stations (APRS, Meshtastic, RTTY…) |

### emergency features

| Method | Params | Result |
|---|---|---|
| `sos.start` | `{text?, position?:Position, interval_s?}` | `{active:true, since_ts}` — repeating top-priority beacon, relayed by everyone |
| `sos.stop` | — | `{active:false}` |
| `sos.status` | — | `{active, since_ts?, text?, sent, acked_by:[{node_id,name,ts}]}` |
| `sos.list` | — | `{alerts:[{node_id, name, text, position?, first_ts, last_ts, hops, acked}]}` |
| `sos.ack` | `{node_id}` | `{}` — tell the sender someone heard them |
| `checkin.send` | `{status:"ok"|"need_help"|"injured"|"moving"|"sheltering", note?, position?}` | `{}` |
| `checkin.list` | — | `{checkins:[{node_id, name, status, note, position?, ts}]}` (latest per node) |
| `position.set` | `Position` | `{}` — the host pushes GPS fixes here |
| `position.get` | — | `{position?:Position}` |
| `position.share` | `{conv}` | `{message}` |
| `bulletins.list` | `{category?}` | `{bulletins:[Bulletin]}` |
| `bulletins.post` | `{category, title, text, position?, expires_s?}` | `{bulletin}` |
| `bulletins.delete` | `{id}` | `{}` |

`Bulletin = {id, from, from_name, ts, expires_ts, category:"water"|"food"|"medical"|"shelter"|
"hazard"|"trade"|"missing"|"transport"|"info", title, text, position?:Position, mine}` — a
persistent notice that floods the mesh and is kept until it expires.

### mesh

| Method | Params | Result |
|---|---|---|
| `mesh.peers` | — | `{peers:[{node_id, name, callsign, direct, hops, last_seen_ts, links:[{link_id, rssi?, snr?, last_rx_ts}], position?}]}` |
| `mesh.stats` | — | `{stored_messages, stored_bytes, relayed, dropped_duplicates, dropped_ttl, tx_frames, rx_frames, rx_bad, uptime_ms, per_link:[{link_id, airtime_ms, duty_used_permille, queue}]}` |
| `mesh.ping` | `{node_id}` | `{sent:true}` then event `mesh.pong` |
| `mesh.announce` | — | `{}` — send our node announcement now |
| `netplan.get` | — | `{plan?:NetPlan, heard:[NetPlan]}` |
| `netplan.set` | `NetPlan` | `{plan}` — announced periodically so others know where to tune |

`NetPlan = {from?, from_name?, ts?, entries:[{label, freq_hz, mode:"afsk1200"|…|"voice", tone?, schedule?, notes?}]}`

NetPlan `mode` names (additive clarification): exactly the modem catalogue names —
`afsk1200`, `afsk300`, `psk2400`, `psk4800`, `robust250`, `robust125`, `robust62`, `robust31`,
`cw`, `rtty`, `psk31`, `dtmf` — plus `voice` (default when empty), `lora`, `meshtastic`, `other`.
`netplan.get` result gains `modes:[…]` (that list, for the UI select). `netplan.set` normalises
old / loose spellings (`bpsk31`, `PSK-31` → `psk31`; `Morse` → `cw`; `AFSK 1200` → `afsk1200`;
case, spaces, `-`, `_` ignored); other strings are stored unchanged. Heard plans are reported with
the same normalisation.

### links

`Link = {id, type, name, state:"down"|"connecting"|"up"|"error", error?, endpoint:{kind,…},
config:{…}, caps:{mtu, bitrate_bps, broadcast, half_duplex, reliable, encryption_allowed,
requires_station_id, duty_cycle_permille, cost}, stats:{tx_frames, rx_frames, tx_bytes, rx_bytes,
rx_errors, airtime_ms, last_rx_ts, queue}, status:{…driver specific…}}`

| Method | Params | Result |
|---|---|---|
| `links.types` | — | `{types:[{type, title, description, endpoint_kinds:[…], config:[Field]}]}` |
| `links.list` | — | `{links:[Link]}` |
| `links.add` | `{type, name?, config, endpoint?:{kind,…}, persist?=true}` | `{link}` — `endpoint` is opaque to the core, stored and echoed |
| `links.configure` | `{id, config?, name?, endpoint?:{kind,…}}` | `{link}` — `endpoint` (additive) replaces the stored opaque descriptor as a whole, same id, persisted, emits `link.state` |
| `links.remove` | `{id}` | `{}` |
| `links.endpoint_state` | `{id, connected, error?}` | `{}` — host tells the core the endpoint opened/closed |

**Region-seeded link defaults (additive):** for link types with a `region` select (`rnode`,
`lora_at`), `links.types` reports as that field's `default` the preset matching the node's
`region` setting (US → `us915`, EU/UK/ZA/CEPT countries → `eu868`, AU/NZ/BR/South America →
`au915`, IN → `in865`, JP/KR/AS → `as923` (new preset), unknown e.g. `OTHER` → unchanged
`eu868`), and `links.add` stores that value into a config without `region`. An explicit `region`
is never changed; if the seeded value makes an otherwise valid config fail, the link is created
exactly as before. `meshtastic` (`region:"auto"`/device codes) is not affected.

Normally the UI does not call `links.add` directly; it calls `host.link.create` (below), which
opens the endpoint and then calls `links.add`.

**Link persistence (normative):** the core is the single source of truth for links. `links.add`
persists the link definition (type, name, config and the opaque `endpoint` descriptor) unless
`persist:false` is passed, and link ids are stable across restarts. When a node starts, persisted
links come back in state `down`. The host router's only job at startup is: call `links.list`, and
for every link whose `endpoint.kind` it can provide, reopen the endpoint from `endpoint.config`
and report `links.endpoint_state {id, connected:true}` (or `connected:false, error`). Hosts keep no
separate list of saved links. `links.remove` deletes the persisted definition.

**Interop conversations:** text heard from non-Ember stations (APRS, Meshtastic, RTTY, PSK31, CW)
is stored as messages of kind `interop` in conversations with id `io:<network>` (e.g. `io:aprs`),
`Conversation.kind = "interop"`. `interop.send` replies into the same conversation.

**Audio link status fields (normative, stable names; additive).** For links of type `audio` and
`rtl_tcp`, `Link.status` always contains these flat, top-level fields, intended for a level meter
(poll `links.list` ~2–4 times a second while the meter is visible). Numbers are rounded to 0.1.

| Field | Type | Meaning |
|---|---|---|
| `rx_level_dbfs` | number | Receive audio RMS level, dB relative to a full-scale sine, ~0.3 s average. Floor `-120` (also when no audio has been received). Good range ≈ −30…−10. |
| `rx_peak_dbfs` | number | Decaying peak level (≈1 s decay), dBFS; floor `-120`; `0` = full scale. |
| `clipping` | bool | Input is clipping now (`clip_percent` > 0.1). |
| `clip_percent` | number | Percentage (0–100) of recent samples at full scale. |
| `snr_db` | number \| null | SNR reported by the modem for the last decoded frame; `null` until one is decoded. |
| `dcd` | bool | Data carrier detect: the receive modem hears a signal (false when audio has stopped arriving). |
| `tx_active` | bool | The link is transmitting (PTT keyed). Always false for `rtl_tcp`. |
| `frames_ok` | integer | Frames the modem decoded with a good checksum (counters restart when the receive modem is recreated: reconfigure or a new sample rate). |
| `frames_bad` | integer | Frames that failed checksum/decoding (modem + AX.25 layer). |
| `rx_audio_age_ms` | integer \| null | Milliseconds since the last block of receive audio arrived; `null` = never. |
| `last_error` | string | Last driver error, `""` when none. |
| `level_hint` | string | One of `no_audio` (nothing received for 2 s), `too_quiet` (RMS < −50 dBFS), `ok`, `too_loud` (peak > −1 dBFS), `clipping`. |

The older nested `status.modem_status` object stays for diagnostics; meters should use the fields
above. For `rtl_tcp` the levels are those of the demodulated audio.

### guides

| Method | Params | Result |
|---|---|---|
| `guides.categories` | — | `{categories:[{id, title, count}], total}` |
| `guides.list` | `{category?, part_of?, offset?=0, limit?=100}` | `{guides:[GuideMeta], total}` |
| `guides.get` | `{id}` | `GuideMeta + {markdown, prev?, next?}` |
| `guides.search` | `{query, category?, limit?=20}` | `{results:[{id, title, category, score, snippet, terms:[…]}]}` |

`GuideMeta = {id, title, category, summary, tags, source, source_title, license, attribution,
bytes, part_of?, order?}`

### tools (calculators and utilities — schema driven)

| Method | Params | Result |
|---|---|---|
| `tools.list` | — | `{tools:[{id, title, category, description, inputs:[Field]}]}` |
| `tools.run` | `{id, inputs:{…}}` | `{outputs:[{key, label, value, unit?}], text?, notes?:[…]}` |
| `rf.bands` | `{region?, service?}` | `{services:[{id, name, region, license, max_power_w?, notes, channels:[{name, freq_hz, bw_hz?, notes?}]}]}` — `region` accepts any settings code or ISO 3166 country code; result gains `matched_regions:[…]` (e.g. `"BR"` → `["BR","R2","INTL"]`): worldwide services are always included, plus the IARU-region band plans and any country data; unknown codes (`OTHER`) get worldwide + all three IARU regions. |
| `tools.qr` | `{text, ecc?="M"}` | `{size, rows:["0101…",…]}` — QR matrix, 1 = dark |

Tool categories: `antenna`, `rf`, `power`, `navigation`, `signal`, `survival`, `crypto`.
`text` is preformatted monospace output (build diagrams, Morse, tables).

### sneakernet bundles

| Method | Params | Result |
|---|---|---|
| `bundle.export` | `{scope:"all"|"conv"|"for_node", conv?, node_id?, since_ts?, max_bytes?}` | `{data_b64, count, bytes}` |
| `bundle.import` | `{data_b64}` | `{imported, duplicates, for_me}` |

A bundle is a file of signed mesh packets; carrying it on a USB stick or showing it as QR codes is
equivalent to a (very slow) link.

### settings / app storage / security

| Method | Params | Result |
|---|---|---|
| `settings.get` | — | `{settings:{…}}` |
| `settings.set` | `{settings:{key:value,…}}` | `{settings:{…}}` |
| `kv.get` | `{ns, key}` | `{value}` (null if absent) |
| `kv.set` | `{ns, key, value}` | `{}` — any JSON value; used by UI features (notes, checklists, inventory, station profiles) |
| `kv.delete` | `{ns, key}` | `{}` |
| `kv.list` | `{ns, prefix?}` | `{keys:[…]}` |
| `security.status` | — | `{locked, has_passphrase, has_duress}` |
| `security.set_passphrase` | `{old?, new}` (`new:""` removes) | `{}` — encrypts storage at rest |
| `security.unlock` | `{passphrase}` | `{unlocked}` — entering the duress passphrase silently wipes |
| `security.lock` | — | `{}` |
| `security.set_duress` | `{passphrase}` | `{}` |
| `security.wipe` | `{confirm:"ERASE"}` | `{}` — destroys identity, keys and all stored data |

Known settings keys: `region` (`"US"|"EU"|"UK"|"AU"|"JP"|…`), `relay_enabled` (bool),
`store_forward_days` (number), `beacon_interval_s`, `low_power` (bool), `position_sharing`
(`"off"|"manual"|"auto"`), `default_ttl_hops`, `units` (`"metric"|"imperial"`), `theme`
(`"dark"|"light"|"night"`), `links_autostart` (host-managed array of saved link definitions).
Also `read_receipts` (bool, default true: `messages.mark_read` sends read receipts for DMs).
Defaults: `region:"US"`, `relay_enabled:true`, `store_forward_days:3` (1..30), `beacon_interval_s:1800`
(0 = never, else 60..86400), `low_power:false`, `position_sharing:"manual"`, `default_ttl_hops:7`
(1..15; large meshes need more), `units:"metric"`, `theme:"dark"`. A known key with a wrong
type/range fails the whole call with `bad_params` (nothing applied); unknown keys are stored and
echoed (≤ 64 KiB in total).

### Security review changes (protocol v2; additive for the RPC)

- **Fingerprints** (`node.info`, contacts) are 8 words (64 bits); the first 6 are the old ones.
- **`links.list` / `links.add` `config`** reports the effective values the driver runs with (e.g.
  the profile callsign the node copies into kiss/audio links instead of `N0CALL`).
- **Unverified senders.** Messages, bulletins (`signed_ok` field added to Bulletin) and SOS alerts
  from senders whose key is unknown are flagged `signed_ok:false`. Unverified SOS alerts live in a
  separate table of at most 32 (they never evict verified alerts; at most ~12/min arrive); an
  unverified cancel never clears a verified alert; only the verified author can withdraw a
  bulletin; bulletins from unverified senders are evicted first. A later authenticated copy of the
  same message id replaces the unverified one (`message.updated`).
- **History is never deleted for strangers.** When the conversation table is full, only a DM /
  interop conversation the user never wrote in, with a non-contact, may be forgotten (unverified
  ones first); otherwise the new stranger's message is not stored. Message eviction for the history
  quota takes channel chatter first, then DMs with strangers, then the rest.
- **Replay.** A delivered authenticated message is never delivered again (also after
  `messages.delete`, `conversations.clear` or a restart) until its expiry + 1 day.
- **Mesh time** (`time_source:"mesh"`) is only adopted by a node without a usable clock; a sane
  system clock, GPS or manual time is never changed by the mesh (docs/PROTOCOL.md §9).
- **`identity.import`** rejects backups demanding more than 64 MiB / 8 passes of Argon2id; node
  config `kdf_mem_kib` / `kdf_passes` are capped likewise. New config key `max_conversations`
  (default 5000).
- **`channels.join` / `channels.create`** return `busy` if another channel already uses the same
  32-bit id with a different key (instead of silently returning that channel).

- **Tampered storage.** `security.status` gains `tampered:true` and `tamper_message` (additive)
  when the node finds its storage manipulated: an unexpected plaintext store generation next to
  the encrypted one (the encrypted data is still opened normally with the passphrase, and the
  plaintext copy is removed), an encrypted store that is missing / replaced by a plaintext one
  (the node keeps an "encrypted" marker file in `storage_dir`, outside the store), or an encrypted
  header with hostile key-derivation parameters (> 64 MiB or > 8 passes). In the last two cases the
  node stays `locked` and `security.unlock` returns `denied` with the reason; nothing is wiped or
  reset automatically — `security.wipe` remains the user's explicit choice. Encrypted stores
  written before this version (store format v1) open with their passphrase and are migrated.

**Duress unlock (normative).** `security.unlock` with the duress passphrase is indistinguishable
from a normal unlock of a lightly used device:
- it takes the same work (both passphrase derivations always run) and returns the same result;
- the identity, contacts, messages, channels other than Public and kv data are
  destroyed and a fresh identity is created;
- the non-secret presentation state is carried over: profile `name`, `callsign`, `status`, and the
  settings `region`, `units`, `theme`, `position_sharing`, `relay_enabled`, `low_power`,
  `read_receipts`, `store_forward_days`, `default_ttl_hops`, `beacon_interval_s` (a snapshot kept,
  encrypted under a key derived from the duress passphrase, in the store header; refreshed when the
  profile or settings change while a duress passphrase is set); persisted links are kept;
- the duress passphrase becomes the storage passphrase (`locked`/`has_passphrase` as before; a
  later unlock with it behaves the same), and `security.status.has_duress` stays `true` (a
  phantom, non-working duress slot), as on the real device;
- events: exactly those of a normal unlock (`node.ready`); `storage.dirty` follows at the normal
  rate-limited time.
Stores written before this version keep working; their duress slot gains the presentation
snapshot the next time `security.set_duress` is called (until then a duress unlock behaves as
before, with an empty profile).

### Core behaviour notes (clarifications; additive)

- **Ids.** The Public channel has id `public` (conversation `ch:public`); other channel ids are 8
  lowercase hex chars. Message / bulletin ids are 24 hex chars. `hops` (Message, Contact, peers) counts
  relays: 0 = heard directly; `null` when unknown.
- **Errors.** Unknown method → `not_implemented`; a `host.*` method reaching the core →
  `unsupported`; a send before the node has a clock (no `now_ms` in the config and no `ember_tick`
  yet) → `busy`; wrong passphrase (`security.unlock`, `security.set_passphrase.old`,
  `identity.import`) → `denied`; strings over their limit → `too_large`.
- **Locked state.** While locked only `node.info`, `node.set_time`, `position.set`,
  `security.status|unlock|wipe`, `links.types|list|endpoint_state`, `guides.*`, `tools.*`,
  `rf.bands` and `tools.qr` work; everything else returns `locked`. `node.info` then has empty
  identity fields (`node_id:""`, `fingerprint:[]`, `card:""`) and `security.status.has_duress` is
  false. A node started on an encrypted store has no links until unlocked; after `security.lock`
  (runtime lock) it keeps relaying and storing. Content events (`message.*`, `conversation.*`,
  `contact.*`, `sos.*` …) are not emitted while locked; `link.*`, `node.ready`, `security.locked`,
  `storage.dirty` and `time.updated` are.
- **`node.ready`** is emitted after `ember_create` and again after a successful unlock, a duress
  unlock and `security.wipe`. Hosts re-run their startup step (`links.list` → reopen endpoints) on
  every `node.ready`. A duress unlock leaves a fresh identity whose storage is protected by the
  duress passphrase. `security.wipe` also removes links; link ids are never reused.
- **Messages.** `messages.list` returns messages with `ts < before_ts`; messages sharing the oldest
  timestamp of a page are never split across pages (a page may exceed `limit`). Received SOS and
  check-ins also appear as messages of kind `sos` / `checkin` in `ch:public`. `messages.send` to an
  `io:<network>` conversation replies as plain text on the link that network was last heard on.
  Message gains `reply_to?` (id); Conversation gains `encrypted`. `conversations.list` lists every
  channel, also when empty. Files: at most 128 KiB; `file.progress` is reported for incoming files.
- **SOS.** `sos.ack` accepts an optional `text`. Alerts in `sos.list` gain `signed_ok` (SOS is
  delivered even when the sender's key is unknown); cancelled alerts are removed. SOS from blocked
  contacts is still shown; everything else from blocked contacts is ignored.
- **Misc.** `mesh.ping` to a node whose keys are unknown still returns `{sent:true}`. `links.list`
  Link gains `persist`; `links.remove` emits a final `link.state` with `state:"down", removed:true`.
  `bundle.import` result gains `rejected`. `mesh.stats`, `node.info` (`app_version`, `device_name`,
  `storage:"memory"|"disk"`, `storage_error?`), `tools.qr` (`version`) and `rf.bands` (`notes`,
  `regions`) carry extra fields.

## 3. Host methods (platform host router)

| Method | Params | Result |
|---|---|---|
| `host.info` | — | `{platform:"web"|"android"|"ios"|"desktop", app_version, backend:"wasm"|"native"|"remote"|"mock", endpoint_kinds:[EndpointKind], features:[…]}` |
| `host.endpoints.scan` | `{kind, timeout_ms?}` | `{devices:[{id, name, detail?, rssi?, config:{…}}]}` — discovered devices; `config` pre-fills the endpoint form |
| `host.link.create` | `{name?, endpoint:{kind, config:{…}}, link:{type, config:{…}}, autostart?=true}` | `{link}` — opens the endpoint, calls `links.add`, wires I/O |
| `host.link.remove` | `{id}` | `{}` — closes the endpoint and calls `links.remove` |
| `host.link.reconnect` | `{id}` | `{link}` |
| `host.hub.start` | `{port?=8437}` | `{running:true, urls:[…], clients}` — serve the web app + `/ws` to nearby devices |
| `host.hub.stop` / `host.hub.status` | — | `{running, urls:[…], clients}` |
| `host.webrtc.create_offer` | `{name?}` | `{pairing_id, offer}` — text blob to show as QR / copy |
| `host.webrtc.accept_offer` | `{offer}` | `{pairing_id, answer}` |
| `host.webrtc.accept_answer` | `{pairing_id, answer}` | `{link}` |
| `host.location.get` | — | `Position` (also pushes `position.set` to the core) |
| `host.location.watch` | `{enabled}` | `{}` |
| `host.qr.scan` | — | `{text}` — camera QR scanner (`unsupported` if no camera/decoder) |
| `host.file.save` | `{name, mime, data_b64}` | `{}` — save/share a file (bundles, backups) |
| `host.file.pick` | `{accept?}` | `{name, mime, data_b64}` |
| `host.signal.flash` | `{pattern_ms:[on,off,on,…], mode?}` | `{used, stopped, timing?}` — flashlight/screen Morse (section 10) |
| `host.signal.stop` | — | `{}` — stop a running flash or tone (section 10) |
| `host.signal.tone` | `{pattern_ms:[…], freq_hz?=700}` | `{}` — audible Morse |
| `host.notify` | `{title, body}` | `{}` |
| `host.keep_awake` | `{enabled}` | `{}` |

`EndpointKind = {kind, title, description, available, unavailable_reason?, scannable,
config:[Field], link_types:[…compatible core link types…], default_link_type}`

`host.link.create` also accepts `persist?=true` (`autostart:false` means the same as
`persist:false`: the link is kept only until restart) and, on the desktop host, `host_key?`: if a
link whose `endpoint.host_key` equals it already exists, that link is reused instead of adding
another (idempotent creation from config files and command-line flags). `host.hub.stop` removes the
hub's `ws_server` link.

Any `host.*` method a platform lacks returns error `unsupported`; the UI hides or disables the
feature based on `host.info.features` (strings such as `"hub"`, `"qr_scan"`, `"location"`,
`"flash"`, `"tone"`, `"file"`, `"notify"`, `"webrtc"`).

**Platform actions (additive, optional).** Platform-specific extras the shared UI knows nothing
about (Android: share the app, start on boot, battery optimisation; desktop: open a folder…) are
listed in `host.info.actions:[{id, title, description?, method, params?, confirm?, checked?}]` and
shown by the UI in Settings → "This device". `method` must be a `host.*` method of the same
platform; the UI calls it with `params` (default `{}`) and toasts `result.message` if present.
`confirm` (text) asks before calling. `checked` (bool) makes the action a switch: the UI calls
`method` with `{...params, enabled: <new state>}` and re-reads `host.info` to show the state. Ids
are stable strings; the UI ignores malformed entries and non-`host.*` methods. Example:
`{"id":"start_on_boot","title":"Start on boot","method":"host.android.settings.set","params":{"key":"start_on_boot"},"checked":false}`.

## 4. Events

| Event | Data |
|---|---|
| `node.ready` | `{}` |
| `message.new` | `{message}` |
| `message.status` | `{id, conv, status}` |
| `message.updated` | `{message}` — (additive) an authenticated copy replaced an unverified message with the same id (`signed_ok` false → true; text may change) |
| `conversation.updated` | `{conversation}` |
| `contact.updated` | `{contact}` |
| `channel.updated` | `{channel}` |
| `peer.seen` | `{node_id, name, link_id, hops, rssi?, snr?}` |
| `link.state` | `{link}` |
| `link.activity` | `{link_id, dir:"tx"|"rx", bytes}` (rate limited) |
| `link.ptt` | `{link_id, on}` — host keys/unkeys the transmitter (serial RTS/DTR, CAT…) |
| `sos.received` | `{node_id, name, text, position?, ts, hops}` |
| `sos.updated` | same shape as `sos.status` |
| `checkin.received` | `{node_id, name, status, note, position?, ts}` |
| `bulletin.new` | `{bulletin}` |
| `netplan.heard` | `NetPlan` |
| `mesh.pong` | `{node_id, rtt_ms, hops}` |
| `file.progress` | `{id, received, total}` |
| `time.updated` | `{time_ms, source}` |
| `storage.dirty` | `{}` — persisted state changed (WASM host flushes IDBFS) |
| `security.locked` | `{}` |
| `host.endpoint.state` | `{link_id, connected, error?}` |
| `host.hub.clients` | `{clients}` |
| `host.location` | `Position` |

## 5. Transport of RPC between UI and backends

UI code only sees the Backend interface (`ui/js/backend.js`):

```js
const backend = await createBackend();          // picks the implementation
const info = await backend.call('node.info');   // rejects with Error{code,message} on failure
const off = backend.on('message.new', data => {…});   // returns unsubscribe; '*' receives (event, data)
backend.kind                                     // "wasm" | "native" | "remote" | "mock"
```

Each implementation is an ES module with `export default class` exposing `async init()`,
`async call(method, params)`, `on(event, handler)`, `kind`:

| Module | Selected when |
|---|---|
| `ui/js/backends/native.js` | `window.EmberNative` (Android) or `window.webkit.messageHandlers.ember` (iOS) exists |
| `ui/js/backends/remote.js` | URL has `?remote=1` (optionally `&token=…`) — control a desktop daemon's node |
| `ui/js/backends/mock.js` | URL has `?mock=1` — in-browser fake for UI development/tests |
| `ui/js/backends/wasm.js` | otherwise — the WebAssembly core running in this page |

**Native bridge (Android + iOS WebView):** JS → native: `postMessage(JSON.stringify({id, method,
params}))` on `window.EmberNative` (Android `@JavascriptInterface`) or
`window.webkit.messageHandlers.ember` (iOS). Native → JS:
`window.__emberBridge.onReply({id, ok, result?, error?})` and
`window.__emberBridge.onEvent({event, data})`, both invoked via evaluateJavascript. The native host
router answers `host.*` itself and forwards everything else to `ember_call`.

**Remote (desktop daemon):** WebSocket `ws://<host>:8437/rpc?token=<admin token>`; text frames.
Request `{id, method, params}` → reply `{id, ok, result?, error?}`; server pushes `{event, data}`.
Scripts can use `POST /rpc` instead: body `{method, params, id?}` with `Authorization: Bearer
<token>` (or `?token=`), returning `{ok, result|error, id?}`.

**Hub mode for guests:** a browser that loads the UI from a hub at `http://<ip>:8437/` runs its
own WASM node (own identity, end-to-end crypto) and automatically adds a `websocket` endpoint +
`raw` link to `ws://<same host>/ws`.

## 6. Morse receive, handheld radios (additive)

### Morse receive — camera light or audio, transcribed in real time

One decoder in the core serves every app. The UI (or a native host capture source) feeds it either
brightness samples from a camera looking at a flashlight / lamp / blinking screen, or microphone
audio, and gets text back as it is decoded. The decoder estimates speed by itself (about 3–25 WPM
for light, limited by camera frame rate; about 5–40 WPM for audio), tolerates hand-keyed timing,
camera auto-exposure drift, frame jitter, flicker, noise and tone drift, and never needs a link.

| Method | Params | Result |
|---|---|---|
| `morse.rx.start` | `{source:"light"|"audio", sample_rate? (audio, Hz, default 8000), wpm_hint?, tone_hz? (audio; default auto-detect 300–1500 Hz)}` | `{session, source}` |
| `morse.rx.feed` | light: `{session, t_ms, dt_ms, levels:[…]}` (uniformly spaced samples starting at `t_ms`) or `{session, times_ms:[…], levels:[…]}` (per-sample times, e.g. camera frame timestamps); `levels` are brightness 0..1 of the watched region. audio: `{session, pcm_b64}` — mono signed 16-bit little-endian at the session's `sample_rate` | `MorseStatus` |
| `morse.rx.status` | `{session}` | `MorseStatus` |
| `morse.rx.reset` | `{session}` | `{}` — clears transcript and speed estimate |
| `morse.rx.stop` | `{session}` | `{transcript}` |

`MorseStatus = {transcript (decoded text so far, last ≤ 4000 chars), chars (total characters decoded
since start/reset — compare with your last value to find what is new), wpm (estimated, 0 = unknown),
on (bool: a mark is being received right now), level (0..1 normalised signal strength for a meter),
threshold (0..1, light), quality (0..1 confidence in the timing lock), pending (the dots/dashes of
the character currently being received, e.g. "-.-"), tone_hz? and snr_db? (audio)}`

Event `morse.text {session, source, text, wpm}` is emitted whenever new characters are decoded
(prosigns in angle brackets, e.g. `<SOS>`, `<AR>`; an undecodable element group as `□`). At most 4
sessions; a session with no feed for 60 s is closed. Times are milliseconds on any monotonic origin.

**Capture.** The shared UI captures with `getUserMedia` wherever the page has it (browser, desktop
operator page, Android WebView, iOS WKWebView): the camera preview is a `<video>` element on
screen, the UI averages brightness over the watched region for every frame and calls
`morse.rx.feed`; for audio it resamples microphone PCM to the session rate. Hosts must therefore
grant camera and microphone permission to their web view. A host whose web view cannot capture
advertises native sources instead in `host.info.features` — `"capture_camera_luma"` and/or
`"capture_mic_pcm"` — and implements:

| Method | Params | Result |
|---|---|---|
| `host.capture.start` | `{kind:"camera_luma"|"mic_pcm", session, rect?:{x,y,w,h} (CSS px where the native camera preview must be shown), roi?:{x,y,w,h} (0..1 fractions of the frame to watch; default the centre fifth), camera?:"back"|"front"}` | `{}` — the host captures and calls `morse.rx.feed` itself |
| `host.capture.update` | `{kind, rect?, roi?}` | `{}` |
| `host.capture.stop` | `{kind}` | `{}` |

The UI then follows progress through `morse.text` events and `morse.rx.status`. `host.capture.start`
for `mic_pcm` also accepts `sample_rate?` (default 8000; must equal the session's rate). If a native
capture stops by itself (permission revoked, device lost, interruption) the host emits
`host.capture.state {kind, running:false, error}`. When a host advertises `capture_camera_luma` the
UI should prefer it over `getUserMedia` for reading light, because native capture can lock exposure,
focus and white balance; hosts that support both also list `"getusermedia"` in `features`.

### Radio profiles and handheld presets

| Method | Params | Result |
|---|---|---|
| `rf.radios` | `{query?}` | `{radios:[Radio]}` |

`Radio = {id, maker, model, kind:"handheld"|"mobile"|"base", bands:[…], connector (e.g. "K1 2-pin
(3.5 mm + 2.5 mm)"), vox, data_port, builtin_tnc, bluetooth, power_w:[…], verified (bool: settings
below were checked against the maker's manual), settings:[{label, value, how}] (what to set on the
radio and the menu path/number to do it), cable_notes, notes, recommended:{preset, link:{type,
config:{…}}}}` — a small offline database (Baofeng UV-5R family and other common handhelds and
mobiles, plus generic entries) used by the "Connect a radio" wizard.

The core `audio` link type has a `preset` config field (see `links.types`): `handheld_vox` (radio
keyed by its own VOX from the phone/computer audio — longer lead-in so the first bits are not
clipped), `handheld_ptt` (PTT keyed by a serial RTS/DTR line or an interface such as an AIOC or
Digirig), `acoustic` (no cable: device speaker ↔ radio microphone, radio speaker ↔ device
microphone, operator presses PTT), `sound_card` (base/mobile radio with a data port) and `custom`.
A preset fills the modem, lead-in/tail, level and CSMA settings; any field given explicitly wins.

### What you need — hardware for each connection

| Method | Params | Result |
|---|---|---|
| `rf.hardware` | `{platform?:"android"|"ios"|"web"|"desktop", method?}` | `{methods:[HardwareMethod]}` |

`HardwareMethod = {id, title, what_it_connects (e.g. "phone ↔ handheld radio audio"), link_type,
preset?, platforms:[…where this method works…], platform_notes:{android?, ios?, web?, desktop?},
items:[{name (a generic description of the part, e.g. "USB-C OTG adapter", "K1 two-pin radio audio
cable (3.5 mm + 2.5 mm)", "USB sound-card + serial-PTT radio interface (AIOC / Digirig class)",
"USB serial adapter with a CP210x, CH340/CH341, FTDI or PL2303 chip"), why, required (bool),
alternatives:[…]}], steps_summary, manual_chapter (guide id), proven:"emulated"|"untested hardware"}`
— an offline, structured shopping/packing list: for every way of connecting something to a phone
or computer (radio by cable with VOX, radio with a PTT interface, radio with no cable, Bluetooth
TNC, serial/KISS TNC, Meshtastic board, RNode, AT-command LoRa module, transparent serial radio,
SDR dongle, phone-to-phone, hub hotspot, sneakernet, Morse by light/sound) it says exactly which
devices, cables and adapters are needed on each platform. The "Connect a radio" wizard shows the
matching entry as its "You will need" step, and the Instructions section renders the whole list;
the operator manual has the same content in prose (`embermesh/what-you-need`). Only generic part
classes and chip families the software actually supports are named; nothing here has been tested on
real hardware unless `proven` says otherwise.

### Host capability hints used by the radio wizard and the library screens

- `host.info.audio = {ptt:[…methods this host can key, e.g. "vox","manual","serial","hid","rigctld"…],
  serial_ptt (bool), presets_supported:[…], presets_unsupported:{preset: reason}}` — optional. The
  "Connect a radio" wizard reads it generically: a connection method whose preset is unsupported is
  shown disabled with the host's reason (for example iOS has no serial PTT line).
- Event `host.guides.progress {loaded, total}` — optional. A host that loads the guide pack after the
  app has started (the browser, where the pack is downloaded) emits it while loading so the library
  screens can show progress; the app must be usable before the pack has finished loading, and
  `guides.*` simply return an empty library until `ember_guides_load` has been called.
- `Link` (`links.list`, `links.add`, `links.configure`, `link.state`) adds `preset?` (the active driver
  preset, e.g. `handheld_vox`) and `overrides` (only the config keys the user set explicitly); `config`
  is the effective configuration the driver runs with: preset values applied for keys not in
  `overrides`, the profile callsign substituted for `N0CALL`, and the callsign actually transmitted
  (e.g. a licence-free `QX…` pseudo id).
- `Link` also adds `blocked?:"no_callsign"` while the link requires a station id (amateur band) and
  the profile has no callsign, so nothing is transmitted on it; `status.last_error` then reads "Set
  your callsign to transmit on this link (amateur band)" (unless the driver reports its own error),
  and `link.state` is emitted when the condition starts and when it clears.

## 7. Semantic search — a small local model (additive)

Guide search combines keyword ranking with meaning-based ranking from a small text-embedding model
that runs entirely on the device, inside the core, with no network and no extra runtime. The model
(static token embeddings, quantized to 4 bits per weight) and a precomputed vector for every guide
passage are stored in `guides.pack`, so every app gets them with the library. A pack without the
semantic section still works; search is then keyword-only. There is no generative model: results are
always passages of the guides themselves, never generated text.

| Method | Params | Result |
|---|---|---|
| `guides.search` | existing params plus `semantic?=true` (set false for keyword-only) | existing result; each item gains `match:"words"|"meaning"|"both"` and `passage?:{text, heading?}` — the best-matching passage of that guide (plain text, ≤ 600 chars) |
| `guides.related` | `{id, limit?=8}` | `{results:[GuideMeta + {score}]}` — guides closest in meaning to the given guide (empty when the pack has no semantic section) |
| `guides.ask` | `{question, category?, limit?=5}` | `{passages:[{id, title, category, heading?, text, score}]}` — the passages that best answer a natural-language question, across the whole library (extractive: text is quoted from the guides) |
| `guides.info` | — | `{guides, categories, bytes, semantic:{available, model?, dims?, bits?, passages?}, licences:[{name, count}]}` |

Ranking must never get worse than keyword search for exact-term queries (names, part numbers,
callsigns): the keyword score dominates when the query terms match strongly, and the semantic score
decides among weak or zero keyword matches (paraphrases, lay wording, typos, synonyms).

**Scores (semantic methods).** `score` in `guides.ask`, `guides.related` and the `passage` ranking is a
similarity in the range 0..1 (1 = same meaning; values are comparable across queries). `guides.ask`
and `guides.related` also return `weak:true` at the top level when even the best result is below the
library's confidence threshold, so a UI can say "no good passage found" instead of showing poor
matches; results are still returned, best first.

**Implementation notes (core/src/guides/README.md).**
- `guides.search`: `semantic:false` gives exactly the keyword ranking. With the default, a result's
  `match` is `"words"` when its keyword score is at least 0.3 of the best one, `"meaning"` when it
  is among the 20 best by meaning (or close to the best) without such a keyword score, `"both"`
  otherwise; `passage` is present when the pack has a semantic section. `score` stays a relative
  ranking value (not 0..1). Guides found only by meaning have a `snippet` taken from their passage.
- Similarity calibration: `guides.ask` and `passage` ranking map cosine 0.40 → 0 and 0.80 → 1;
  `guides.related` maps 0.60 → 0 and 0.97 → 1 (whole-guide vectors are closer to each other). The
  confidence threshold for `weak` is **0.30** for both. Measured: 6 % of the evaluation questions
  are weak (mostly misspellings), and 8 of 10 off-topic questions. UIs should use `weak` (or 0.30)
  as the "no good passage" cut-off; 0.20 lets some off-topic matches through.
- `guides.ask` returns at most 2 passages per guide; `limit` is clamped to 1..20 and
  `guides.related` `limit` to 1..50 (at most 2 chapters of the same book). Without a semantic
  section both return an empty list (`ask` with `weak:true`).
- `guides.info`: `bytes` is the size of the loaded pack; `semantic.bits` is the bits per model
  weight (4); `licences` counts guides per exact licence string, most used first. While no pack is
  loaded it returns zeros and `semantic.available:false`.

## 8. Appearance, motion and accessibility (additive)

These are preferences of the shared UI. They are stored with `kv.set` in namespace `ui` (and mirrored
in the page's local storage so they can be applied before the core has started):

| Key | Values | Meaning |
|---|---|---|
| `simple_mode` | bool (default false) | Simple mode: larger text and touch targets, fewer choices per screen, plain wording, only the essential destinations in the navigation (Chats, SOS, Guides, Instructions, Settings), no decorative motion. Every feature stays reachable from "More". |
| `motion` | `"system"` (default) / `"full"` / `"reduced"` / `"off"` | Animation level. `system` follows the platform's reduce-motion setting. Low-power mode and Simple mode force at most `reduced`. |
| `text_scale` | number 0.85–1.6 (default 1) | Text size multiplier, on top of the platform's own font scaling. |
| `high_contrast` | bool (default false, or the platform's setting) | Stronger borders and text contrast in every theme. |
| `boot_animation` | `"full"` (default) / `"short"` / `"off"` | The start-up animation. Never delays the app: it ends as soon as the app is ready, on any tap or key press, and after a hard time limit. |

A host may report platform accessibility state in `host.info.accessibility = {reduce_motion?,
font_scale?, screen_reader?, high_contrast?, bold_text?}` and emit `host.accessibility` with the same
shape when it changes; where it is absent the UI uses the browser's media queries
(`prefers-reduced-motion`, `prefers-contrast`, `forced-colors`). Optional host method
`host.haptic {kind:"tap"|"success"|"warning"|"error"}` gives tactile feedback; hosts without it
return `unsupported`.

Test hooks: `?noboot=1` skips the start-up animation, `?motion=off` forces motion off, and both are
automatic under browser automation (`navigator.webdriver`), so automated walks stay deterministic.

## 9. Hosted web app and fully offline installs (additive)

The web app is published as a static site (GitHub Pages) from every `main` commit. Installed as a
PWA ("Add to Home Screen" / "Install app") it must then work with no network at all: the service
worker downloads EVERY resource to the device — the UI, the WebAssembly core and the whole guide
library — keeps it in persistent storage, and serves the app from there from then on. When a
network is available it checks for a newer build in the background, downloads it completely, and
switches to it on the next start (never mid-session, and never to a partly downloaded build); when
there is no network nothing changes.

Host methods of the browser host (other hosts return `unsupported`):

| Method | Params | Result |
|---|---|---|
| `host.app.offline_status` | — | `{installed (running as an installed PWA), offline_ready (every resource is stored), stored_bytes, total_bytes, version, build, persistent (storage will not be evicted), update:{available, version?, downloaded_bytes?, total_bytes?, ready (downloaded, applies on next start)}, last_check_ts?}` |
| `host.app.make_offline` | — | `{}` — download everything now (also done automatically after install); progress via `host.app.offline` events |
| `host.app.check_update` | `{apply?:false}` | `host.app.offline_status` result — checks now; `apply:true` restarts into a fully downloaded update |

Event `host.app.offline {stored_bytes, total_bytes, offline_ready, update?}` reports progress. The
same controls appear in Settings → "This device" through `host.info.actions` ("Download everything
for offline use", "Check for updates", "Restart to update"), with the current state in each
action's `description`.

## 10. Morse sending by light (additive)

The Morse screen lets the user type text and flash it — with the device's flashlight (torch) when
it has one, with the screen otherwise — for another device to read with its camera (section 6,
`morse.rx.*` with `source:"light"`). Timing comes from the UI (`ui/js/lib/morse.js`:
`toMorse()` → `morsePattern(code, wpm)` → `pattern_ms`), so every platform flashes the same
pattern.

| Method | Params | Result |
|---|---|---|
| `host.signal.flash` | `{pattern_ms:[on,off,on,…], mode?="auto"}` | `{used:"torch"\|"screen", stopped:bool, timing?:{…}}` |
| `host.signal.stop` | — | `{}` |

`mode`:

- `"auto"` (default; what older UIs send by omitting it): the torch if the device has one that the
  host can switch, otherwise a full-screen flash.
- `"torch"`: the torch only; error `unsupported` when there is none, `permission_denied` when the
  platform needs a permission for it and the user refused.
- `"screen"`: the screen only.

`host.signal.flash` resolves when the pattern has finished or was stopped (`stopped:true`), never
before. A second `host.signal.flash` or `host.signal.tone` while one is running stops the running
one first. `host.signal.stop` stops a running flash or tone at once and is a no-op otherwise.
Limits every host enforces: at most 4,096 entries, each 10 ms – 60,000 ms, total at most 10 minutes
(`bad_params` otherwise). `timing` is optional and platform-specific (for example the largest
deviation from the requested edges, in ms) — the UI may show it, tests may assert on it.

`host.info` gains `signal: {torch: "yes" | "no" | "ask", screen: true}`. `"ask"` means the host
cannot know before the user grants a permission (a browser exposes the torch only through the
camera). `features` keeps `"flash"` (the host can flash something) and gains `"torch"` when
`signal.torch` is `"yes"` or `"ask"`.

Rules for hosts — these are requirements, not suggestions:

- **The light always goes off.** The torch is switched off and the screen restored when the pattern
  ends, on `host.signal.stop`, on any error, when the app goes to the background or the screen
  locks, and when the page is hidden or closed.
- **Least privilege, at the moment of use.** Nothing is requested at start-up for this feature. A
  host that needs the camera permission to reach the torch (browsers) asks only when the user
  starts a torch flash, opens the camera stream only for the duration of the flash, never reads,
  records, stores or transmits frames from it, and stops the track as soon as the flash ends. Hosts
  that can switch the torch without the camera (Android `setTorchMode`, iOS torch) must not request
  the camera for sending.
- **Nothing leaves the device and nothing is kept.** The typed text is not sent to the mesh, not
  logged and not stored by the host; the UI keeps it only in the page (no history).
- **Timing.** Edges are scheduled against a monotonic clock from the start of the pattern (no
  accumulated drift): a late edge shortens the next interval instead of delaying everything.

Rules for the UI:

- It says plainly that light is not private: anyone who can see it can read it, and it is not
  encrypted.
- Full-screen flashing can trigger seizures in people with photosensitive epilepsy. Screen flashes
  are limited to at most three flashes per second (a dot of at least 170 ms, about 7 words per
  minute), are never started automatically, and show a warning before the first one; with reduced
  motion or Simple mode the warning must be confirmed each time. The torch is a small source and
  may be faster, up to the fastest speed the camera reader decodes reliably.
- The default speed is one the camera reader of another device decodes reliably at 30 frames per
  second; the screen shows the progress, the time remaining and a Stop button that works at once.

Old hosts that ignore `mode` and return `{}` remain valid: the UI treats a missing `used` as
unknown and a missing `host.signal.stop` (`unsupported` / `not_implemented`) as "cannot stop the
torch early" and says so.
