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

| `morse_rx::LightTracker` | `morse/light_tracker.h` | Whole-frame camera reader (§12): finds the keyed light in max/mean brightness grids (≤ 4096 cells), follows it, feeds `LightKeyer`. Described below. |

The node binds the RPC in `node/node_morse.cpp` and emits `morse.text` after every feed that
decoded something, and `morse.target` when the tracker's target changes.

## Finding the light anywhere in the frame (`LightTracker`)

Capture layers send one grid per camera frame; each cell is the **maximum** luma of the pixels it
covers, so a light of one pixel decides its cell (it shows only if it is brighter than the
brightest background pixel of that cell). Per frame, in O(cells), with every buffer allocated once
on the first frame (about 270 KiB per session) and nothing allocated afterwards:

1. **Global gain.** The median ratio of each cell (mean grid when given) to its own slow average
   (2.5 s) divides the frame: auto-exposure pumping and whole-scene changes cancel.
2. **Motion-tolerant change events.** A cell "switches on" when it is brighter than the 5×5
   neighbourhood maximum of *each* of the two previous frames (a steady light moving up to ~1 cell
   per frame — hand shake — never does), by more than 4 σ of that cell's own frame-to-frame noise
   (30th-percentile tracker: robust to the cell's own keying, and it absorbs the upward bias of
   max-pooled noise) and at least 5 levels, and when most of its brightness is new (noise riding on
   a steady lamp is not). "Switch-off" is the mirror image. More than 10 % of the cells at once is a
   scene change, not a light.
3. **Camera jumps and zoom.** On/off event pairs that share a displacement (a jerk) or lie on rays
   from the frame centre with a common ratio (a zoom step) move or rescale every track. Steady lights
   that have been lit for a while are motion anchors: when at least two agree, their per-frame
   motion also moves the tracks that are dark at that moment.
4. **Tracks** (at most 12). Each follows one spot: its signal is the brightest of the 3×3 cells
   around it minus a low percentile of a ring of cells just outside it and its bloom (local
   background, so straddling cell edges and blooming over neighbours do not matter); hysteresis
   keying between its own on/off envelopes; a gate that grows while it is dark (5 cells/s, 10 for
   the light being read, capped at 4 / 8 cells); its mark/space runs and the last ~5 s of its signal.
5. **Morse or not**, judged at the end of every mark over the recent runs (one-frame glitches are
   merged into their neighbours). *Evidence against keying* (a contradiction): ramps instead of
   steps, inconsistent mark brightness, marks more than ~6:1 apart or typically shorter than 1.5
   frames, no space as short as ~3.5 dots, a periodic beacon (≥ 75 % of the recent cycles and marks
   alike within the camera's quantisation), a lit level not well above the dark level (noise on a
   steady light), a large object. *Not enough evidence yet*: fewer than 4 marks, no dash and fewer
   than two element-length gaps, contrast below 6 σ or 8 levels. A track that has ever been
   contradicted must then pass three times in a row.
6. **Choosing.** The best keyed track (contrast and structure) is read; if a clearer one is about
   to qualify it waits up to 1.5 s. The light being read is kept while it is alive; it is replaced
   when it has been dark for the "lost" time (max(2 s, 7.5 dots)), when it stays lit for more than
   3 s, when it twice contradicts keying, or at once when a light of similar brightness starts
   blinking just after it went dark (the same light after a zoom step or a jerk). A tap pins the
   nearest track (or waits for one at that spot).
7. **Decoder signal.** The read track's samples go to `LightKeyer` through a soft limiter centred on
   the track's own hysteresis threshold (steady levels, intermediate values for partly exposed edge
   frames so crossings are still interpolated). On the first lock the track's history is replayed
   (preceded by a second of dark samples at the measured frame interval, so the decoder learns the
   interval before the first mark); after a hand-over only samples newer than those already fed.

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

### Whole-frame camera reader (`LightTracker`), measured in simulation

Scene model: `core/tests/morse/scene_sim.h` renders an 8-bit luma image per frame (512×288 for a
64×36 grid, i.e. 8×8 pixels per cell; one test uses 24×24 pixels per cell) with background texture,
read + shot noise, sub-pixel point lights with saturation and bloom, keyed lights integrated over
the exposure, mains flicker (100/120 Hz) integrated over the exposure, standby blinkers, moving
headlights, a lit person walking past, AE pumping and AE that follows the mean, hand shake
(random-walk velocity), jerks, the camera pointing away, zoom steps, timestamp jitter (separate from
the true exposure times) and dropped frames — then max/mean-pools it exactly as a host does. The
"box" below is the oracle: the section-6 region reader with a perfectly placed box and the exact
brightness of the light on the same frames. 10 seeds per row
(`-tc="tracker long: every*"`), hand keying with 5 % jitter, 5 % (3 % in the busy scenes) of
frames dropped:

| Scene | exact | CER | box CER | lock (median / worst) |
|---|---|---|---|---|
| 1–2 px light, dark room, 30 fps, 12 WPM | 10/10 | 0 % | 0 % | 1.4 / 1.5 s |
| same, 30 fps, 15 WPM | 8/10 | 2.1 % | 0.7 % | 1.1 / 1.3 s |
| same, 30 fps, 5 WPM | 10/10 | 0 % | 0 % | 2.7 / 2.9 s |
| same, 24 fps, 12 WPM | 9/10 | 0.7 % | 0.7 % | 1.4 / 1.6 s |
| same, 24 fps, 15 WPM (~1.9 frames per dot, the decoder's limit) | 5/10 | 7.9 % | 2.1 % | 1.2 / 3.2 s |
| same, 24 fps, 5 WPM | 10/10 | 0 % | 0 % | 2.7 / 3.0 s |
| same, 60 fps, 15 WPM | 10/10 | 0 % | 0 % | 1.1 / 1.1 s |
| night street clutter (lamps, 100 Hz flicker, headlights, walker, AE pumping), 30 fps | 10/10 | 0 % | 0 % | 1.2 / 1.3 s |
| same, 24 fps | 9/10 | 1.2 % | 0.6 % | 1.2 / 1.7 s |
| hand shake ~4 cells/s RMS + 5-cell jerks | 9/10 | 0.7 % | 0 % | 1.4 / 1.6 s |
| camera away for 1.5 s mid-message | — | 12.6 % | 11.5 % | 1.2 / 1.3 s |
| zoom 1×→2×→5×→1× mid-message (24 characters) | 0/10 | 17.9 % | — | 1.2 / 2.8 s |
| two keyed lights (reads the clearer) | 10/10 | 0 % | 0 % | 1.2 / 1.3 s |
| saturated torch blooming over 3–4 cells, AE following | 8/10 | 1.4 % | 0 % | 1.5 / 1.6 s |
| daylight, textured 110–160 background, light +80 luma | 7/10 | 12.9 % | 0 % | 1.7 s / one never |

- **No keyed light, no text**: 60 scenes (dark room with high-gain noise for 60 s, and night
  clutter with flicker, two standby blinkers, traffic, a walker, AE pumping, shake and jerks for
  40 s), 10 seeds × 24/30/60 fps, about 51 minutes of video: **0 characters, 0 locks**
  (`-tc="tracker long: no text*"`).
- **Smallest light** (one pixel centred, dark room, read noise σ 2.5 + shot noise, 30 fps,
  12 WPM, 6 seeds): locks from **+20–25 luma** over the background; reads as well as the
  perfect box from **+35–40 luma** (the cell's max rises ~25 levels above its max-pooled
  background, ~10 σ of the read noise). At 24 fps: +40 luma for 4/6. Daylight: a 2-pixel light
  must stand ~25 levels above the brightest background pixel of its cell (here +80 luma over the
  texture) — 5/6 lock, 3/6 exact; +60 luma: 2/6 lock. Max-pooling is honest about this: a light
  that is not brighter than the brightest pixel of its cell is invisible.
- **Time to lock**: four marks with Morse structure (six for a weak light): about 1.1–1.5 s after
  keying starts at 12–15 WPM, 2.7–3 s at 5 WPM; the first character appears at the same moment
  (the history is replayed).
- **Where it is weaker than a perfect box**: the first letter at 15 WPM in ~1 of 10 runs (the
  decoder's cold start: "TEST" → "NST"); at 24 fps / 15 WPM; a zoom step costs 1–2 characters
  (the light may be mid-element; in a dark scene the reader finds it again on the ray from the
  centre or by hand-over), and in 1 of 10 runs zooming back out made it latch onto a lamp for ~3 s;
  around a dropout the characters sent while away are lost and one at each edge may be garbled.
- **Cost** (`-tc="tracker long: cost*"`, busy night scene at 60 fps, tracker + decoder per frame):
  64×36 grid ~27 µs natively (a recent arm64 laptop, Release build) and ~34–41 µs in the wasm32 build under Node;
  64×64 ~47 / ~57–69 µs. Memory ~270 KiB per session, allocated on the first frame; the tracker does
  not allocate afterwards (the shared decoder `MorseTiming` still builds a few small temporary
  vectors while a character is in progress).
