# EmberMesh link drivers (`core/src/link/`)

A **link driver** turns a dumb platform pipe (bytes or audio samples) into a mesh link. The contract
is [`ember/link.h`](../../include/ember/link.h); the operator manuals in
`guides/content/embermesh/` are written from this file.

| Type | What it drives | Endpoint kinds | Status |
|---|---|---|---|
| `raw` | Datagram media: Wi-Fi multicast, hub WebSocket, WebRTC, Multipeer, loopback | udp, websocket, ws_server, webrtc, multipeer, loopback | done, back-to-back tested |
| `stream` | Byte streams: USB/serial cable, TCP, BLE UART, phone↔phone BLE, Bluetooth SPP, Ebyte/HC-12/SiK/XBee radio modules | serial, tcp, tcp_server, ble, ble_peer, bt_spp, wifi_direct | done, back-to-back tested |
| `kiss` | KISS TNCs (hardware, Direwolf, Mobilinkd, radios with TNC, NinoTNC, LoRa KISS) + APRS | serial, tcp, ble, bt_spp | done, tested vs hand-built vectors |
| `audio` | Sound-card soft modem: AFSK1200/300, PSK2400/4800, Robust, CW/RTTY/PSK31/DTMF | audio, audio_pipe (desktop test pipe) | done, tested through the channel simulator |
| `meshtastic` | Stock Meshtastic device as a LoRa modem | serial, tcp, ble | done, emulator-tested |
| `rnode` | RNode firmware LoRa boards | serial, ble, bt_spp, tcp | done, emulator-tested |
| `lora_at` | REYAX RYLR998/498/896/406 AT-command LoRa modules | serial, ble, bt_spp, tcp | done, emulator-tested |
| `rtl_tcp` | Receive-only RTL-SDR via rtl_tcp | rtl_tcp, tcp | done, tested with synthetic IQ |

**None of the drivers has been tested on real hardware.** All tests use emulators, hand-built vectors
and the dsp channel simulator.

## For the node facade (using the registry)

- `ember::link_types_json()` returns the JSON array for `links.types` (wrap it as `{types:[…]}`).
  Each entry has `type`, `title`, `description`, `endpoint_kinds` and `config:[Field]`. The modem
  select of `audio`/`rtl_tcp` is built from `modem::modem_catalog()`, so it only lists modems that
  can be created.
- `ember::create_link(type, config_json, host, &err)`: `config_json` may be `""` or `{}` (all
  defaults). Missing or null keys get their defaults, wrong types or out-of-range values are
  rejected with a readable `err` like `"mtu: must be <= 8192"`, and unknown keys are ignored. Every
  type can be created from its defaults.
- `reconfigure(json)` takes the same schema. On error it returns false and keeps the old config.
  Drivers that talk to a device (kiss params, rnode, lora_at, meshtastic, rtl_tcp tuning) re-send
  their settings when the endpoint is connected.
- **States.** A new link is `Down`. Feed `on_io_state(true/false)` from `links.endpoint_state`.
  - `raw`, `stream`, `kiss` and `audio` go `Up` as soon as the endpoint opens.
  - `meshtastic`, `rnode`, `lora_at` and `rtl_tcp` stay `Connecting` until the device handshake
    completes, and go to `Error` (with `status.last_error`) if it fails. They retry on their own
    from `tick()`.
  - Data arriving also proves the endpoint is up.
- **`tick(now_ms)` every 20–50 ms** drives pacing, CSMA, retries and timeouts. `send()` also tries
  to transmit immediately.
- **`caps().mtu == 0` means the link cannot carry mesh frames**: `rtl_tcp` (receive-only) and
  `audio` in a keyboard mode (cw/rtty/psk31/dtmf, interop text only). `send()` then always returns
  false.
- **Callsign.** `kiss` and `audio` default to the placeholder callsign `N0CALL`. In amateur mode they
  refuse to transmit (`send()` false, `last_error` says why) until a real callsign is configured.
  Copy the node's callsign into the link config when creating AX.25 links.
- **Licence-free pseudo station id.** In licence-free mode (`licence: licence_free`) with the
  callsign left at `N0CALL`, `kiss` and `audio`/`rtl_tcp` transmit as a pseudo station id
  `QX` + 4 random letters + random `-SSID` (for example `QXKDTB-9`), drawn once from host entropy
  when the link is created and kept across `reconfigure`. It contains no digit, so it can never be
  a real ITU callsign (Q prefixes are not issued). `status.ax25.callsign` shows it. Amateur mode
  never uses it: there the driver still refuses to transmit until a real callsign is set.
- **Own-echo suppression.** A received AX.25 frame is dropped as our own echo (digipeater repeat,
  acoustic loop-back) only if its source is our callsign *and* its destination+info match one of
  the last 128 frames we sent. Another station using the same callsign is still heard.
- **`send()` returns false** for an empty frame, a frame larger than mtu, or a full queue
  (back-pressure; retry later). Queues are strict-priority (0 = highest). When a queue is full, a
  higher-priority frame evicts the newest lower-priority one (counted in `status.tx_evicted`).
  Frames sent while the endpoint is down are queued, up to the queue limit.
- **Events** (`link_event`):
  - `ptt {"on":bool}` from `audio`. The host keys the transmitter; it should delay releasing PTT
    by its own output buffer latency, because `on:false` is sent when the last sample is handed
    out, not when it has played.
  - `aprs_ack {"to","id","acked"}` from `kiss`/`audio` when an APRS message is acknowledged or
    given up.
- `status_json()` always contains `type`, `state`, `last_error?`, `tx_frames`, `tx_bytes`,
  `rx_frames`, `rx_bytes`, `rx_errors`, `tx_refused`, `tx_evicted`, `io_tx_bytes`, `io_rx_bytes`,
  `interop_rx`, `interop_tx`, `airtime_ms`, `last_rx_ms`, `queue`, `queue_bytes`, `caps`, plus
  driver-specific fields. These map onto the `stats` object of `Link` in RPC_API.md.

## `raw` — one datagram = one mesh frame

No header, no checksum (the medium provides integrity). The hub's `/ws` WebSocket carries exactly
this format.

| Key | Default | Meaning |
|---|---|---|
| `preset` | `udp_lan` | `udp_lan` (mtu 1400, broadcast, cost 1), `websocket` (4096, broadcast, reliable, cost 1), `webrtc` (4096, point-to-point, reliable, cost 2), `multipeer` (4096, broadcast, reliable, cost 2), `loopback` (4096, cost 1), `custom` |
| `mtu`, `bitrate_bps`, `cost` | 0 | 0 = preset value |
| `broadcast`, `reliable` | `auto` | `auto` / `yes` / `no` |
| `queue_frames` | 64 | Frames kept while the endpoint is down |

Received datagrams of 1–8192 bytes are accepted, even above our own mtu (a peer may use a larger
preset). Empty and larger datagrams count as `rx_errors`. Caps: full duplex, encryption allowed,
no station id needed.

## `stream` — byte streams and transparent radio modules

**Wire formats.** The receiver always accepts plain frames, and FEC frames only when `fec` is on
(both ends of a link must agree on `fec`). The sender uses the configured one. Frames longer than
the link's mtu are dropped, so both ends must also use the same preset/mtu. Receive CPU is
budgeted: hostile input (forged headers at every offset) costs linear time, and candidates that
cannot be afforded are skipped (`status.rx_budget_skipped`).

- **plain**: `framing/stream.h`, i.e. `E5 4D type(0) len(2) hcrc(1) payload crc32(4)`, 10 bytes of
  overhead. It resynchronises after noise, dropped or inserted bytes and corrupt lengths, and
  survives arbitrary chunking.
  - This matters for the **phone↔phone BLE peer service**, which is a plain byte stream with
    arbitrary chunking and no chunk header. The `ble` preset relies on this resync.
- **fec**: sync word `7E 1B A5 D2` (matched with up to 3 bit errors) followed by a `fec/packet.h`
  envelope: RS-protected header, RS(…, nroots) blocks, CRC-32. It repairs up to nroots/2 byte
  errors per block, and `RxMeta.corrected_errors` reports the repairs.

| Key | Default | Meaning |
|---|---|---|
| `preset` | `cable` | see the table below |
| `mtu`, `bitrate_bps`, `pace_bps`, `cost` | 0 | 0 = preset value |
| `gap_ms` | -1 | Pause after each paced packet and after receiving (half-duplex turnaround); -1 = preset |
| `fec` | `auto` | Reed-Solomon envelope (`auto`/`yes`/`no`); `fec_roots` 16 (2..64) |
| `half_duplex`, `broadcast` | `auto` | Override the preset |
| `write_chunk` | 0 | Split endpoint writes into pieces of at most N bytes (small-buffer endpoints) |
| `amateur` | false | Radio is on an amateur band: `encryption_allowed=false`, `requires_station_id=true` |
| `duty_cycle_permille` | 1000 | Enforced over a sliding 1 h window |
| `queue_frames` | 32 | |

| Preset | mtu | bitrate | pacing | gap | FEC | half duplex | broadcast | cost | Use for |
|---|---|---|---|---|---|---|---|---|---|
| `cable` | 2048 | 115200 | – | 0 | no | no | no | 2 | USB/serial null-modem cable, USB OTG between phones |
| `tcp` | 4096 | 1 M | – | 0 | no | no | no (reliable) | 1 | TCP between nodes |
| `ble` | 512 | 20 k | – | 0 | no | no | no | 3 | BLE UART (Nordic UART service) adapters, phone↔phone BLE peer service |
| `bt_spp` | 2048 | 200 k | – | 0 | no | no | no (reliable) | 3 | Classic Bluetooth serial |
| `ebyte` | 200 | 2400 | 2400 bit/s | 150 ms | yes | yes | yes | 25 | Ebyte E22/E32 in transparent mode (air rate 2.4k default) |
| `hc12` | 200 | 4800 | 4800 bit/s | 80 ms | yes | yes | yes | 20 | HC-12 (FU3) |
| `sik` | 240 | 32 k | 32 kbit/s | 20 ms | no | no | no | 10 | SiK / 3DR telemetry radios (point-to-point TDM) |
| `xbee` | 200 | 9600 | 9600 bit/s | 20 ms | no | yes | yes | 8 | XBee transparent mode (9600 baud serial) |
| `custom` | 256 | 9600 | – | 0 | no | no | no | 10 | anything else |

**Wiring and hardware notes.**

- Ebyte E32/E22 need M0 = M1 = LOW (normal/transparent mode). Set the same air rate, channel and
  address on both modules with the vendor tool. Set `pace_bps` to the air data rate if you change it.
  These modules split long writes into 58-byte (E32) or 240-byte (E22) air packets, so use FEC on
  noisy links.
- HC-12: same channel and FU mode on both modules (`AT+FU3`, `AT+C001`). SET pin high for data.
- In the Americas, 433 MHz is an amateur band: turn on `amateur` and identify with your callsign.
- Pacing writes one packet, then waits for its time on air at `pace_bps` plus `gap_ms`. After
  receiving bytes, a half-duplex link also waits `gap_ms` before sending. `channel_busy()` reflects
  both.

## `kiss` — KISS TNCs and AX.25

**On air** (`payload: ax25`, the default) every mesh frame is one AX.25 UI frame:

```
dst EMBER-0 (C bit set) | src <callsign-SSID> | [digipeaters] | ctl 0x03 | PID 0xF0 | info = mesh frame
KISS: C0 <port<<4 | 0> <escaped AX.25 without FCS> C0
```

For example, mesh frame `01 C0 DB 42` from `N0CALL-7` is
`C0 00 8A 9A 84 8A A4 40 E0 9C 60 86 82 98 98 6F 03 F0 01 DB DC DB DD 42 C0`.

`payload: raw` puts the mesh frame directly into the KISS data frame, for licence-free LoRa KISS
modems that do not need AX.25.

| Key | Default | Meaning |
|---|---|---|
| `callsign` | `N0CALL` | Operator callsign-SSID. Transmissions are refused while it is N0CALL in amateur mode |
| `licence` | `amateur` | `amateur` → `encryption_allowed=false`, `requires_station_id=true`; `licence_free` → both relaxed |
| `payload` | `ax25` | `ax25` / `raw` |
| `port` | 0 | KISS port 0–15; frames for other ports are ignored |
| `bitrate_bps` | 1200 | Radio data rate (1200 VHF, 9600 UHF, 300 HF); cost auto: ≥9600 → 25, ≥1200 → 40, else 80 |
| `path` | "" | Digipeaters for mesh frames (normally empty) |
| `aprs`, `aprs_path`, `aprs_auto_ack` | true, `WIDE1-1,WIDE2-1`, true | APRS interop |
| `set_params` | true | On connect send TXDELAY, P, SLOTTIME, TXTAIL, FULLDUPLEX (`txdelay_ms` 300, `persistence` 63, `slottime_ms` 100, `txtail_ms` 30, `full_duplex` false; sent in 10 ms units) |
| `init` | `none` | `kiss_on` sends `\rKISS ON\rRESTART\r` (TNC-2, Kantronics, Kenwood TH-D7x/TM-D710); `custom` sends `init_custom` (escapes `\r \n \t \\ \xHH`) |
| `exit_kiss` | false | Send `C0 FF C0` (leave KISS) when the link is destroyed |
| `mtu` | 256 | Info field size (32–2048; 256 = AX.25 N1) |
| `duty_cycle_permille`, `cost`, `queue_frames` | 1000, 0, 32 | |

**TX pacing.** After each frame the driver waits its estimated airtime (bytes at `bitrate_bps` +
TXDELAY + TXTAIL) before handing the TNC the next one, so TNC buffers do not overflow. The TNC
still does its own CSMA.

**APRS interop.** Received UI frames that are not addressed to `EMBER` are parsed as APRS.

- **Messages** (`:ADDRESSEE:text{id`) become `InteropText{from, to, text, network:"aprs"}`. This
  includes messages for other stations (monitoring) and bulletins (`to:"BLN1"`).
- Messages to our callsign are **acked automatically**. Copies heard again within 30 s (sender
  retries, digipeaters) are acked again but not shown twice.
- **Positions** become `"Position 49.05833, -72.02917 … - comment"`; **status** becomes `"Status: …"`.

`send_interop_text({to, text})` builds an APRS message from our callsign to `APZEMB`, using
`aprs_path`.

- Text over 67 characters is split into up to 4 messages.
- Each message is retried after 30, 60 and 120 s until acked. Reply-acks `{MM}AA` count as acks.
- The result arrives as an `aprs_ack` event.
- `to:""` sends an unacknowledged bulletin `BLN0`.
- Our own frames repeated by digipeaters are ignored.

Hardware notes:

- Direwolf: KISS TCP port 8001.
- BLE KISS TNCs: the `kiss` driver only needs a plain byte stream carrying KISS frames, with
  arbitrary chunking. Which GATT service carries that stream is the platform endpoint's business:
  - Nordic UART service (`6e400001-…`): many BLE serial bridges and DIY TNCs.
  - Mobilinkd TNC3/TNC4 do **not** use Nordic UART; they use their own BLE KISS service
    (`00000001-ba2a-46c9-ae49-01b0961f68bb`, write `00000002-…`, notify `00000003-…`, same suffix).
    Use the endpoint's custom-UUID profile (or a Mobilinkd profile if the platform offers one).
    A TNC3 can also be reached over Classic Bluetooth SPP (`bt_spp`) where the platform supports it.
- Kenwood TH-D72/D74 and TM-D710: enable KISS in the menu, or use `init: kiss_on`. Yaesu FTM-400
  has no KISS.
- NinoTNC: USB serial at 57600.

## `audio` — sound-card soft modem

The platform feeds `on_audio()` at any rate from 8 to 192 kHz and calls `pull_audio()` at any rate;
the TX and RX rates may differ. The receive and transmit modems are separate instances, each
created at its rate and recreated when the rate changes.

**Modems and on-air framing**

| Modem | bit/s | Framing | Notes |
|---|---|---|---|
| `afsk1200` (default) | 1200 | AX.25 UI as in `kiss` | Bell 202; interoperates with KISS TNC users and stock APRS gear (APRS interop included) |
| `afsk300` | 300 | AX.25 | HF packet (1600/1800 Hz); APRS interop |
| `psk2400`, `psk4800` | 2400, 4800 | AX.25 | audio cable only |
| `robust250/125/62/31` | 250…31 | AX.25 if amateur, else the bare mesh frame | MFSK + Viterbi + CRC; acoustic coupling, weak/HF signals; auto mtu 128; receiver `max_frame` = max(mtu, 128) + AX.25 overhead (see below) |
| `cw`, `rtty`, `psk31`, `dtmf` | – | text only | mtu 0; `send_interop_text` transmits text; received text becomes interop (`network` = modem name) after a line end, 200 characters or a 3 s pause |

| Key | Default | Meaning |
|---|---|---|
| `preset` | `custom` | `handheld_vox`, `handheld_ptt`, `acoustic`, `sound_card`, `custom`: fills modem, lead-in, tail, level and CSMA settings; explicit keys win (see "Handheld radios") |
| `modem` | `afsk1200` | see above |
| `callsign`, `licence`, `aprs`, `path`, `aprs_path`, `aprs_auto_ack` | as `kiss` | |
| `txdelay_ms` | 300 | Preamble after PTT (flags / sync tones). VOX and cheap handhelds need 300–600 |
| `txtail_ms` | 50 | Trailer before PTT release |
| `tx_level` | 0.5 | Peak output amplitude (0.01–1) |
| `vox_tone_ms`, `vox_tone_hz` | 0, 1000 | Wake-up tone before the preamble for VOX radios |
| `csma`, `persistence`, `slottime_ms` | true, 63, 100 | Carrier sense + p-persistence |
| `max_frames_per_tx` | 4 | Frames per key-up |
| `repeat` | 1 | Send each frame 1–3 times; the receiver drops copies seen within 3 s |
| `mtu` | 0 | 0 = auto (256 HDLC, 128 robust) |
| `modem_params` | "" | `key=value,…` passed to the modem (e.g. `wpm=15`, `mark_hz=1600`, `center_hz=1500`) |
| `duty_cycle_permille`, `cost`, `queue_frames` | 1000, 0 (auto from speed), 32 | |

**Transmit sequence.**

1. When there is traffic, the driver waits while the receive modem reports DCD. A DCD older than
   500 ms without audio input is treated as stale.
2. Every slot it transmits with probability (persistence+1)/256.
3. It sends `ptt {"on":true}`. The following `pull_audio()` calls return the VOX tone (if set),
   then the modem's preamble, the frames and the tail, always exactly `max` samples, padded with
   silence.
4. The last block may be shorter. Then comes `ptt {"on":false}`, and `pull_audio()` returns 0
   again.
5. If nobody pulls audio within 2 s of PTT on, PTT is released and `last_error` explains why.
6. RX audio is ignored while transmitting.

**Level metering (stable, for UI meters).** `status` (top level, for `audio` and `rtl_tcp`) always
contains these flat fields; the names are normative (docs/RPC_API.md, "Audio link status fields"):

| Field | Type | Meaning |
|---|---|---|
| `rx_level_dbfs` | number | RMS receive level, dB re full-scale sine, ~0.3 s average, 0.1 dB steps, floor −120 |
| `rx_peak_dbfs` | number | Decaying peak (≈1 s), dBFS, floor −120 |
| `clipping` | bool | `clip_percent` > 0.1 |
| `clip_percent` | number | % of recent samples at full scale (0–100) |
| `snr_db` | number/null | Modem SNR of the last decoded frame (AFSK: eye SNR, robust: channel SNR in 3 kHz); null before the first |
| `dcd` | bool | Receive modem hears a carrier (false once audio has stopped for 500 ms) |
| `tx_active` | bool | Transmitting (PTT keyed); always false for `rtl_tcp` |
| `frames_ok`, `frames_bad` | integer | Modem frames with good checksum / failed checksum (bad also counts AX.25 rejects) |
| `rx_audio_age_ms` | integer/null | Time since the last receive audio block; null = never |
| `last_error` | string | `""` when none |
| `level_hint` | string | `no_audio` (none for 2 s), `too_quiet` (RMS < −50 dBFS), `ok`, `too_loud` (peak > −1 dBFS), `clipping` |

`status.modem_status` keeps the older diagnostic detail (`rx_clip_fraction`, `last_snr_db`, raw
modem counters, AX.25 status).

**Wiring.**

- Audio levels (canonical figures; platform READMEs should defer to this table). Mic inputs of
  phones/headsets expect roughly 10–50 mV, radio speaker/line outputs give 0.3–2 V:

  | Path | Attenuation | Example |
  |---|---|---|
  | Radio speaker / line out → computer **line in** | none (turn the radio volume down) | direct |
  | Radio speaker / line out → phone/headset **mic in** | 20:1 … 100:1 (26–40 dB) | 10 kΩ series, 100–470 Ω to ground (plus ~2.2 kΩ across mic and ground if the phone needs a "mic present" load) |
  | Device line/headphone out → radio **mic in** | about 100:1 (40 dB) + DC block | 10 kΩ series, 100 Ω to ground, 1 µF in series |
  | Device out → radio data/accessory (line-level) input | none … 10:1 | per radio manual |

  Then fine-tune in software: receive volume until the meter (`status.rx_level_dbfs`) sits around
  −30 … −10 dBFS with `clipping` false; transmit volume with `tx_level` until the far end decodes.
- PTT: the host acts on `link.ptt` events (serial RTS/DTR, CAT, a GPIO), or use the radio's VOX
  with `vox_tone_ms`.
- For acoustic coupling (phone held to the radio), use `robust62` and keep the phone 1–3 cm from
  the speaker.

**Robust false-header guard.** A noise burst that passes the robust header's CRC-8 makes the
receiver decode a payload of the claimed length, holding DCD (and so CSMA) busy for that airtime —
minutes at `robust31` with the modem's 1024-byte default. The driver therefore sets the modem's
`max_frame` to max(`mtu`, 128) + framing overhead (shown as `status.modem_status.max_frame`).
Everyone on a robust channel should use the same `mtu` (the default `0` = 128 is right for almost
everyone); `modem_params: max_frame=N` overrides it.

**Extra FEC.** The robust modes are already convolutionally coded with interleaving. The HDLC
modems discard frames that fail the FCS, so an outer FEC envelope would never see a damaged frame.
For lossy paths the driver offers `repeat` instead of an extra FEC layer.

## Handheld radios (Baofeng UV-5R and others)

Any FM voice radio can carry mesh traffic through the `audio` link. Pick the connection with the
`preset` config field; a preset fills the modem, lead-in, tail, level and channel-access settings,
and **any field given explicitly wins** (`{"preset":"acoustic","modem":"robust250"}` keeps
robust250). `custom` (the default) is exactly the historical defaults.

| `preset` | Use when | modem | VOX tone | txdelay | txtail | tx_level | persistence | slot | frames / key-up |
|---|---|---|---|---|---|---|---|---|---|
| `handheld_vox` | Handheld keyed by its own VOX from a K1/audio cable | afsk1200 | 400 ms @ 1200 Hz | 600 ms | 150 ms | 0.35 | 128 | 100 ms | 8 |
| `handheld_ptt` | PTT keyed by a serial RTS/DTR line or an AIOC / Digirig style interface | afsk1200 | – | 450 ms | 50 ms | 0.40 | 128 | 100 ms | 6 |
| `acoustic` | No cable: device speaker/mic held to the radio, operator presses PTT | robust125 | – | 1500 ms | 400 ms | 0.80 | 128 | 200 ms | 2 |
| `sound_card` | Mobile/base with a data port or USB audio (FT-991A, IC-7300, …) | afsk1200 | – | 300 ms | 50 ms | 0.30 | 63 | 100 ms | 4 |
| `custom` | Everything set by hand | afsk1200 | – | 300 ms | 50 ms | 0.50 | 63 | 100 ms | 4 |

CSMA is on in every preset. Why these numbers:

- **VOX** needs 200–600 ms of audio before the transmitter keys, then the far receiver's squelch
  needs another ~150 ms. The 1 s lead-in (tone + HDLC flags) lets both open before the data. All
  queued frames go out back to back in one key-up (up to 8), so the audio never pauses and VOX stays
  held. The lower level keeps the very sensitive mic input of a K1 cable from overdriving.
- **PTT via an interface** keys in ~100–150 ms, but cheap receivers open their squelch slowly. 450
  ms of flags covers both with margin.
- **Acoustic coupling** is band-limited, echoes and picks up room noise, and a human presses PTT
  0.5–1 s after the app asks ("ptt" event). The robust FEC modem tolerates the echo; the 1.5 s lead-in
  covers the operator's reaction.
- **Data ports** key fast and pass flat audio, so the defaults already work there; the preset mainly
  lowers the level for line inputs.

**Measured** through the radio models in `dsp/radio_channel.h`
(`core/tests/link/handheld_test.cpp`, 16 kHz, licence-free AX.25 frames):

| Model (parameters) | preset: frames delivered | default (`{}`): frames delivered |
|---|---|---|
| VOX handheld (VOX attack 300–600 ms, hang 500 ms, squelch 150 ms + 200 ms tail, pre/de-emphasis, limiter, 300–3000 Hz, AGC, FM SNR 22 dB) | 8/8 single messages; 80/80 in 4-frame bursts; 18/18 in 6-frame bursts with one VOX opening per burst | 0/8 single messages (start clipped); 58/80 in bursts (each burst's first frame lost) |
| PTT handheld (key-up 150 ms, squelch opening 250 ms) | 8/8; 80/80 | 0/8; 56/80 |
| Acoustic coupling (speaker/mic band 400–4000 Hz, reverb, room noise, operator reaction 0.5–0.9 s) | 3/3; 16/16 | 0/3; 0/16 |
| Data port (key-up 60 ms, squelch 80 ms, flat audio) | 8/8; 80/80 | 8/8; 80/80 |

**Connecting a Baofeng UV-5R** (and other K1 two-pin handhelds). `rf.radios` holds the per-radio
settings shown by the "Connect a radio" wizard. For the UV-5R / BF-F8HP the menu numbers are taken
from the UV-5R series menu definitions: MENU 0 SQL low but closed, 2 TXP LOW, 3 SAVE OFF,
4 VOX 1–3, 5 WN to match the other stations, 7 TDR OFF, 8 BEEP OFF, 9 TOT ≥ 60 s,
10–13 tones OFF, 14 VOICE OFF, 19 PTT-ID OFF, 39 ROGER OFF. Numbering can differ on other
firmware, so check the name on the display. Other radios carry generic wording unless `verified`
is true.

- **Cable.** The K1 jack is a 3.5 mm + 2.5 mm pair. Ready-made "APRS-K1" cables, or a USB interface
  for K1 radios (AIOC, Digirig with a K1 lead), give a sound card plus a PTT line.
- **Level.** Keep the phone/computer output low (start around 30 %): the mic input is very
  sensitive. Use the interface's isolation, ferrites and low power against hum and RF feedback.
- **Radios with a built-in TNC.** Kenwood TH-D72/D74/D75 and TM-D710 have a KISS TNC: use the `kiss`
  link (USB, or Bluetooth serial on the TH-D74/D75) instead of `audio`.
- **Licence-free radios.** FRS/GMRS/PMR446 blister-pack radios and CB have no data jack: use
  `acoustic`, and check local rules on data.

## `rtl_tcp` — receive-only SDR

The endpoint is a TCP connection to `rtl_tcp -a <ip> -p 1234`.

1. On connect the driver writes 5-byte tuner commands (u8 cmd + u32 big-endian):
   - `02` sample rate
   - `01` centre frequency = `frequency_hz − tune_offset_hz`
   - `05` ppm
   - `03` gain mode (0 auto / 1 manual), `04` gain in tenths of dB
   - `08 0` RTL AGC off
2. It goes `Connecting` and expects the 12-byte server header (`RTL0`, tuner type, gain count).
   With the header it goes `Up`; with anything else it goes `Error` ("not an rtl_tcp server").
3. Unsigned 8-bit IQ is NBFM-demodulated with `dsp/nbfm.h`: channel at +offset, 12.5 kHz/3 kHz
   deviation or 25 kHz/5 kHz, audio at 24 kHz. The audio is fed to the selected modem, with the same
   framing as `audio`.
4. Mesh frames and APRS traffic are delivered. **It never transmits** (mtu 0, no APRS acks).

| Key | Default | Meaning |
|---|---|---|
| `frequency_hz` | 144390000 | 144.390 MHz = APRS North America (144.800 MHz Europe) |
| `sample_rate` | `240000` | `240000`, `1024000`, `2048000` |
| `tune_offset_hz` | 50000 | Keeps the channel away from the DC spike |
| `gain_db` | 0 | 0 = automatic gain |
| `ppm` | 0 | Dongle crystal correction |
| `channel_bw` | `12500` | `12500` / `25000` |
| `modem`, `callsign`, `licence`, `aprs`, `modem_params` | as `audio` | |

Status: frequency, rate, gain, tuner name (R820T, E4000, …), IQ byte count, channel power, and the
same modem/level metrics as `audio`. A $20 RTL-SDR dongle is enough to monitor a net or an APRS
channel.

## `meshtastic` — stock Meshtastic device as a LoRa modem

Uses an unmodified Meshtastic radio (Heltec, T-Beam, RAK WisBlock, T-Echo, Station G2 …) as a
long-range LoRa modem. The driver talks the same client API the Meshtastic apps and Python CLI use.
It does not change the device's radio settings. You set region, preset and channels with the normal
Meshtastic app, and Ember then uses one channel of that device.

**Endpoint kinds:** `serial` (USB, 115200 baud 8N1), `tcp` (Wi-Fi/Ethernet devices, port **4403**),
`ble` (see below).

### Config

| Key | Default | Meaning |
|---|---|---|
| `framing` | `stream` | `stream` = a byte stream with the Meshtastic stream framing (0x94 0xC3 len): USB serial, TCP, **and BLE through a platform endpoint that adapts the GATT protocol (recommended)**. `ble` = one bare protobuf per `on_io_bytes` / `link_io_write` (only for endpoints that pass GATT values through unchanged). |
| `channel` | `0` | Channel slot on the device (0 = primary). Every Ember node must use the same slot, and the channel name and key must be the same on every device. |
| `hop_limit` | `3` | Meshtastic hop limit (1–7) for Ember packets. |
| `modem_preset` | `auto` | Used only to estimate speed and airtime for scheduling. `auto` reads it from the device (LONG_FAST if unknown). |
| `region` | `auto` | Sets the duty-cycle limit. EU_868 / EU_433 / UA_433: 10 %. UA_868: 1 %. Other regions: none. `auto` reads it from the device. |
| `licensed` | `false` | Set this if the device runs Meshtastic licensed (ham) mode. The link then has `encryption_allowed=false` and `requires_station_id=true`. |
| `text_bridge` | `true` | Bridges Meshtastic text chat on this channel, and direct messages to this device, as interop text (`network:"meshtastic"`), in both directions. |
| `heartbeat_s` | `60` | Keep-alive interval (ToRadio.heartbeat). *(advanced)* |
| `handshake_timeout_s` | `10` | A connect attempt is abandoned after this long with nothing received. *(advanced)* |
| `queue_frames` | `16` | Packets the driver holds while waiting for the device. *(advanced)* |
| `cost` | `0` | Routing cost. 0 = default 20. *(advanced)* |

### Caps

- `mtu` = 233 (Meshtastic `DATA_PAYLOAD_LEN`).
- `bitrate_bps` = SF·BW/2^SF·4/CR of the preset in use. LONG_FAST 1074, MEDIUM_FAST 3515, SHORT_TURBO ≈ 21875, LONG_SLOW 183.
- `broadcast`, `half_duplex`, not `reliable`.
- `encryption_allowed` = !licensed, `requires_station_id` = licensed.
- `duty_cycle_permille` from the region, `cost` 20.

### Device setup checklist

1. Flash normal Meshtastic firmware. Set the **region** first, because the device will not transmit until it is set.
2. Use the same channel (name + PSK) and the same modem preset on every device that should carry Ember traffic.
   A dedicated secondary channel keeps Ember packets away from people's chat apps. Set `channel` to its slot number.
3. Serial: plug in by USB and leave the serial module at its defaults (the API mode is automatic).
   Don't run the Meshtastic app over the same USB port at the same time.
4. TCP: the device must be on Wi-Fi/Ethernet. Connect to `<device-ip>:4403`. Only one TCP client can be connected at a time.
5. BLE: what the core driver expects is the same as for serial/TCP — **a plain byte stream using
   the Meshtastic stream framing** (`0x94 0xC3 len_hi len_lo <protobuf>`), with the default
   `framing:"stream"`. Adapting the Meshtastic GATT protocol to that stream is the platform
   endpoint's job (the iOS `ble` endpoint's Meshtastic profile does exactly this):
   - service `6ba1b218-15a8-461f-9fa8-5dcae273eafd`
   - core → device: strip the 4-byte stream header of each frame the core writes and write the
     bare protobuf to ToRadio `f75c76d2-129e-4dad-a1dd-7866124401e7` (frames may span or share
     `link_io_write` buffers; reassemble by the length field)
   - device → core: after every FromNum `ed9da18c-a800-4f66-a670-aa7547e34453` notification, read
     FromRadio `2c55e69e-4993-11ed-b878-0242ac120002` until it returns empty, and pass each value to
     the core as `0x94 0xC3 len_hi len_lo` + value
   - A plain Nordic-UART `ble` endpoint is *not* compatible (Meshtastic has no UART service).
   - Legacy alternative: an endpoint that passes GATT values through unchanged (one protobuf per
     write/read) can use `framing:"ble"` instead. Do not combine an adapting endpoint with
     `framing:"ble"`.

### Protocol, as the driver uses it

- **Framing (serial/TCP):** `0x94 0xC3 len_hi len_lo <protobuf>`, len ≤ 512.
  - Bytes outside frames are the device's debug log. They are ignored, and the last log line appears in `status.device_log`.
  - A bad start byte or a length > 512 makes the driver resynchronise.
- **Handshake:**
  1. On connect the driver writes 32 × `0xC3` (wakes the serial API) and waits 100 ms.
  2. It then sends `ToRadio{want_config_id = random nonce}`.
  3. The device streams `my_info`, `node_info`…, `config`…, `metadata`, then `config_complete_id = nonce`. At that point the link goes **Up**.
  4. If an attempt gets nothing for `handshake_timeout_s`, the driver retries. After 3 attempts the link goes to **Error**, and it retries every 30 s.
  5. `FromRadio.rebooted` restarts the handshake.
- **Sending:** `ToRadio.packet = MeshPacket{to = 0xFFFFFFFF, channel, hop_limit, id = random, priority, decoded = Data{portnum = PRIVATE_APP (256), payload = mesh frame}}`.
  - Ember priority 0 maps to HIGH(100), 1–2 to RELIABLE(70), 3–6 to DEFAULT(64), 7 to BACKGROUND(10).
  - Writes are paced by the LoRa time-on-air of the preset (16-symbol preamble, explicit header, CRC), with the duty-cycle budget kept over a sliding 1 h window.
  - When the device reports `QueueStatus.free = 0`, the driver waits for free space, or for 10 s.
- **Receiving:**
  - PRIVATE_APP packets on the configured channel are delivered with RSSI/SNR in `RxMeta`.
  - Our own packets, packets still encrypted (another channel or key), other ports and other channels are counted and dropped.
  - TEXT_MESSAGE_APP becomes `InteropText{from:"!xxxxxxxx", to:"" | "!xxxxxxxx", network:"meshtastic"}`.
- **Sending text** (`send_interop_text`): `to` is `""` (broadcast) or `"!xxxxxxxx"` (direct message, `want_ack`). The text is cut to 200 bytes of UTF-8.
- **Status:** `node_id`, `node_num`, `long_name`, `short_name`, `firmware_version`, `nodes_known`, `modem_preset`, `region`,
  `last_rssi_dbm`, `last_snr_db`, `device_queue_free/max`, `handshakes`, `heartbeats`, `framing_errors`, `decode_errors`,
  `ignored_packets`, `encrypted_packets`, `own_packets`, `device_log`, `caps`, plus the common counters.

### Limits and notes

- Meshtastic routers rebroadcast Ember packets like any other packet, so Ember's own flooding sits on top of Meshtastic's.
  Keep `hop_limit` low (1–3) on dense meshes.
- Meshtastic encrypts every channel with its PSK. To the radio, Ember's payload is opaque.
- Only tested against the scripted emulator in `core/tests/link/meshtastic_test.cpp`. It has not been tested against real hardware.

## LoRa region defaults (`rnode`, `lora_at`)

The drivers default to `eu868`. The node (core/src/node) adjusts this to its `region` setting
(`link::lora_region_for_setting`): `links.types` reports the matching preset as the `region`
field's `default`, and `links.add` stores it into a config that does not set `region` (so a later
change of the setting never retunes an existing radio; if the seeded region makes an otherwise
valid config fail — e.g. an explicit `frequency_hz` for Europe — the link is created with the
driver default as before). Mapping: EU/CEPT countries, UK, ZA → `eu868`; US, CA, MX, Central
America/Caribbean → `us915`; AU, NZ, BR and South America → `au915` (917 MHz is inside both
915–928 and 902–928 MHz); IN → `in865`; JP, KR, AS/SE Asia → `as923`; anything else (e.g.
`OTHER`) keeps `eu868`. Explicit `region` values always win.

## `rnode` — RNode LoRa radio

Uses a LoRa board running the open **RNode firmware** (markqvist/RNode_Firmware: LilyGO T-Beam,
LoRa32, T3S3, Heltec LoRa32, RAK4631, homebrew RNodes) as a long-range radio. The app sets the
frequency, bandwidth, spreading factor, coding rate and power on the board every time it connects;
nothing is stored on the board.

**Endpoints:** `serial` (USB, **115200 baud 8N1**), `ble` / `bt_spp` (boards with Bluetooth enabled in
the firmware), `tcp` (boards on Wi-Fi). Flash the firmware once with `rnodeconf --autoinstall`; a
board that answers "EEPROM locked" is not provisioned yet.

| Key | Default | Meaning |
|---|---|---|
| `region` | `eu868` (driver); the node seeds it from its `region` setting | Legal preset: `eu868` (869.525 MHz, 10 % duty cycle, 14 dBm), `eu868_1pct` (868.1 MHz, 1 %), `eu433` (433.775 MHz, 10 %, 10 dBm), `us915` (914.875 MHz), `au915` (917 MHz), `in865` (866 MHz), `as923` (923.2 MHz, 10 %, 13 dBm: JP/KR/SG/TH…; listen-before-talk where required is the firmware's job), `custom` |
| `frequency_hz` | 0 | Centre frequency; 0 = region default. Must lie in the region's band (use `custom` to override; RNode range 137 MHz–3 GHz) |
| `bandwidth_hz` | `125000` | 7.8 / 10.4 / 15.6 / 20.8 / 31.25 / 41.7 / 62.5 / 125 / 250 / 500 kHz |
| `sf` | 9 | Spreading factor 5–12 (7 fast … 12 longest range) |
| `cr` | 5 | Coding rate 4/5 … 4/8 (5–8) |
| `tx_power_dbm` | -1 | -1 = region default; up to 30 for boards with a PA. Check your legal limit |
| `st_alock_pct` / `lt_alock_pct` | 0 | Firmware airtime caps (15 s / 1 h, %). `lt_alock` 0 = set automatically from the region duty cycle |
| `duty_cycle_permille` | 0 | Driver-enforced limit (1 h sliding window); 0 = region value |
| `mtu` | 254 | 254 = one LoRa packet; up to 508 (firmware splits into two packets) |
| `amateur` | false | Amateur-licence operation: no encryption, station id required |
| `cost`, `queue_frames`, `detect_timeout_ms` | 20, 32, 2000 | Advanced |

**All stations must use the same frequency, bandwidth, SF and CR.**

**Caps:** mtu 254 (≤508), bitrate = SF·BW/2^SF·4/CR (SF9/125 kHz/4:5 = 1757 bit/s), half duplex,
broadcast, unreliable, duty cycle from region, encryption allowed unless `amateur`, cost 20.

**Handshake / states:** endpoint up → *connecting*: sends `C0 08 73 C0 50 00 C0 48 00 C0 49 00 C0`
(detect + firmware/platform/MCU queries, as Reticulum). No `C0 08 46 C0` answer within the timeout
→ retried 3× → *error* ("no RNode answered"), retried every 15 s. Then FREQUENCY, BANDWIDTH,
TXPOWER, SF, CR, [ST_ALOCK], [LT_ALOCK], RADIO_STATE=1 are sent; the board echoes what it applied
and the link goes *up* only if frequency (±100 Hz), bandwidth, power, SF, CR match and the radio is on
(otherwise *error*: "RNode rejected the TX power setting …"). A reboot report (`55 F8`), a radio-off
report or an init-radio error restarts the handshake. Changing radio settings with `reconfigure`
reconfigures the board live.

**Flow control:** one packet in flight; released by the firmware's READY (`0x0F`) or, for firmware
without flow control, after the computed time on air + 300 ms. TX-failed / queue-full errors are
counted (`tx_failed`) and the next frame goes out.

**Status:** device firmware/platform/MCU, radio settings, last RSSI/SNR, airtime and channel load
(short/long), noise floor, battery, temperature, device RX/TX counters, device errors.
RSSI = byte − 157 dBm, SNR = signed byte / 4 dB; both go into RxMeta of the following packet.

**On air:** one mesh frame per RNode DATA packet, unchanged (the firmware adds its own 1-byte
header). It can share a channel with Reticulum users on identical settings, but the packets are not
Reticulum packets; each side discards the other's traffic.

Example: Europe, defaults → `C0 01 33 D3 E6 08 C0` (869 525 000 Hz), `C0 02 00 01 E8 48 C0`
(125 kHz), `C0 03 0E C0` (14 dBm), `C0 04 09 C0`, `C0 05 05 C0`, `C0 0C 03 E8 C0` (10.00 %),
`C0 06 01 C0`.

**Legal notes:** EU 868 MHz sub-bands have duty-cycle limits (1 % or 10 %) and ERP limits (25 mW,
500 mW at 869.4–869.65 MHz); EU 433 MHz 10 mW, 10 %. In the Americas 433 MHz is an amateur band —
enable `amateur` and use your callsign. The driver enforces the duty cycle but not antenna gain/ERP.

## `lora_at` — AT-command LoRa module (REYAX)

For cheap AT-command modules: **REYAX RYLR998 / RYLR498** (and clones) and the older
**RYLR896 / RYLR406**, on a USB-serial adapter or a microcontroller's UART.

**Wiring:** module TXD → adapter RXD, RXD → TXD, GND, VDD **3.3 V only** (RYLR998 is not 5 V
tolerant; use a level shifter with 5 V adapters). Serial **115200 baud 8N1** (factory default; change
with `AT+IPR`). Endpoints: `serial`, `ble` / `bt_spp` (serial bridges), `tcp` (serial servers).

| Key | Default | Meaning |
|---|---|---|
| `model` | `rylr998` | `rylr998` (RYLR998/498) or `rylr896` (RYLR896/406) — sets valid ranges |
| `region` | `eu868` (driver); seeded by the node | Same presets as `rnode` (frequency, power, duty cycle) |
| `band_hz` | 0 | Frequency (`AT+BAND`), 0 = region default; must be in the region's band unless `custom` |
| `sf` | 9 | RYLR998: 5–11 (≤ 9 at 125 kHz, ≤ 10 at 250 kHz, ≤ 11 at 500 kHz); RYLR896: 7–12 |
| `bw` | `7` (125 kHz) | Code 0–9 = 7.8 … 500 kHz; RYLR998 supports only 7, 8, 9 |
| `cr` | 1 | 1–4 = 4/5 … 4/8 |
| `preamble` | -1 | -1 = module default (12 / 4). RYLR998: 4–24 only with network 18, otherwise 12; RYLR896: 4–7 |
| `network_id` | -1 | -1 = module default (18 / 0). RYLR998 3–15 or 18; RYLR896 0–16 |
| `address` | 0 | This module's address 0–65535 |
| `dest_address` | 0 | Destination; 0 = broadcast (normal) |
| `power_dbm` | -1 | -1 = region default (clamped to module max: RYLR998 22, RYLR896 15). CE: ≤ 14 dBm |
| `password` | "" | Optional 8-hex-digit AES password (`AT+CPIN`) |
| `interop` | true | Show plain text from ordinary modules |
| `amateur`, `duty_cycle_permille`, `cost`, `queue_frames`, `cmd_timeout_ms` | false, 0, 20, 16, 1000 | As for `rnode` |

**Initialisation** (each step waits for its answer, 1 s timeout, retried): `AT` (3 tries; any reply
proves the module is there), `AT+VER?` (optional), `AT+ADDRESS?` and `AT+NETWORKID?` — the
flash-stored values are written (`AT+ADDRESS=…`, `AT+NETWORKID=…`) **only if different**, to spare the
module's flash — then `AT+BAND=…`, `AT+PARAMETER=SF,BW,CR,PRE`, `AT+CRFOP=…`, [`AT+CPIN=…`], and
verification with `AT+PARAMETER?` / `AT+BAND?`. Any `+ERR=n` or mismatch → *error* with the
command and the code in `last_error`; the whole sequence is retried every 10 s. `+READY` (module
rebooted) re-initialises. Three unanswered `AT+SEND` in a row re-initialise too.

**Sending:** one `AT+SEND` in flight; after `+OK` the driver waits for the computed time on air
(payload + 8 bytes module overhead) + 50 ms before the next one. `+ERR` on a send is counted
(`tx_failed`) and the frame dropped (the mesh retries).

**On air / wire format:** the AT channel is text and `<len>` counts characters, so a mesh frame is
sent as `EM1:` + base64(frame) → at most 240 characters → **MTU 177 bytes**. Example: frame
`01 02 03` → `AT+SEND=0,8,EM1:AQID`. Receptions `+RCV=<addr>,<len>,<data>,<rssi>,<snr>` are parsed
by length (data may contain commas or even line breaks); RSSI/SNR go into RxMeta. Any payload not
starting with `EM1:` is foreign text → interop message (network `lora_at`, from = sender address);
`send_interop_text` sends plain text to `to` (an address) or `dest_address`.

**Line resync:** a corrupt `+RCV=` whose length field claims more data than the line holds is
first assumed to contain a line break in its payload; but as soon as a following line is itself a
module line (`+OK`, `+ERR=…`, `+RCV=…`, `+READY`…), the partial reception is dropped (counted in
`rx_errors` and `status.rx_resyncs`) and that line is handled normally, so no real line is lost.

**Caps:** mtu 177, bitrate from SF/BW/CR (SF9/125 kHz/4:5 = 1757 bit/s), half duplex, broadcast,
unreliable, duty cycle from region, encryption allowed unless `amateur`, cost 20.

**Status:** phase, module version, address, network, frequency, SF/BW/CR/preamble, power, last
RSSI/SNR/sender, tx_failed, tx_timeouts, module_errors, reinits, flash_writes.

## Tests

`core/tests/link/` (doctest `TEST_SUITE("link")`), harness in `link_harness.h`: `FakeHost` with an
injected clock and seeded PRNG, `Pipe` (re-chunking, loss, bit corruption, delay), `Pair` (two
drivers back to back).

```sh
cmake -S . -B build/link -DEMBER_MODULES="base;framing;fec;dsp;modem;link" \
  -DCMAKE_BUILD_TYPE=Debug -DEMBER_SANITIZE=ON -DEMBER_BUILD_DESKTOP=OFF -DEMBER_BUILD_TOOLS=OFF
cmake --build build/link -j 8 && ./build/link/core/ember_tests -ts=link
```
