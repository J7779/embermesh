# EmberMesh Morse receive (`core/src/morse/`)

Real-time Morse transcription for `morse.rx.*` (docs/RPC_API.md §6): camera brightness of a
flashlight, lamp or blinking screen (`source:"light"`), or microphone audio (`source:"audio"`).

| Piece | File | What it does |
|---|---|---|
| `modem::MorseTiming` | `modem/morse_timing.h` | Shared back end: key transitions → text. Speed learned from the marks (two-class split on a log scale), element/character/word gaps learned from the spaces (Farnsworth works), short drop-outs bridged and glitches dropped (both bounded in absolute terms), characters emitted as soon as their gap is long enough, prosigns as `<XX>`, undecodable groups as `□`, a `pending` element string and a timing `quality` 0..1. |
| `modem::CwReceiver` | `modem/cw_rx.h` | Tone front end at 8 kHz: mixer + narrow envelope filter, adaptive floor/peak keying with hysteresis and a noise squelch, tone search and drift tracking (300–1500 Hz). The keyboard-mode `cw` modem uses the same receiver, so both decode identically. |
| `morse_rx::LightKeyer` | `morse/rx.h` | Brightness front end: state-dependent envelope tracking (auto-exposure pumping, ambient drift, hand shake), hysteresis threshold, contrast squelch relative to a robust noise estimate (25th percentile of frame-to-frame change), an edge-sharpness gate (slow exposure ramps never key), sub-frame interpolation of crossings, bridging of drop-outs up to ~1.6 frame intervals. Out-of-order and absurd timestamps are dropped or re-based. |
| `morse_rx::AudioDecoder` | `morse/rx.h` | s16 PCM at 8–192 kHz → resampler → `CwReceiver`. |
| `morse_rx::Session(s)` | `morse/rx.h` | Transcript (last 4000 characters), counters, at most 4 sessions, 60 s idle expiry, feed limits (20 000 levels / 4 MiB PCM per call). |

The node binds the RPC in `node/node_morse.cpp` and emits `morse.text` after every feed that
decoded something.

Characters sharing a pattern with a prosign are written as the prosign by the receiver:
`+` → `<AR>`, `=` → `<BT>`, `(` → `<KN>`, `&` → `<AS>` (the CW modem keeps the characters).

## Measured ranges

Test models: `core/tests/morse/morse_sim.h` (hand keying from the send side
`ember::morse::timing`, camera with jitter / drops / exposure / auto exposure / drift / 100 Hz
flicker / noise / level wander, audio with AWGN in a 2500 Hz bandwidth, drift, QSB and clicks).

- **Light** (phone camera model, 28-character message, 3 seeds): CER ≤ 5 % from about
  **2 frames per dot** (15 fps: up to ~9 WPM; 24 fps: ~15 WPM; 30 fps: ~20 WPM; 60 fps: 25 WPM
  and beyond). Below ~1.8 frames per dot it breaks down quickly.
- **Audio**: CER ≤ 10 % down to about **2–5 dB SNR** (2500 Hz bandwidth) from 5 to 40 WPM;
  exact copy at 10 dB except occasionally the first character while the speed is being learned.
- Hand keying: dash:dot 3:1–5:1 with 15 % jitter ≤ 10 % CER; 2:1 with 15 % jitter ≤ 20 %
  (the classes overlap). A 2.7× speed change mid-message costs about one character.

Regenerate with
`./ember_tests -ts=morse -tc="*long*" --no-skip` (light range table, audio SNR at 10 % CER,
full character table).
