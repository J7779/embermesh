# EmberMesh web platform (WASM)

Runs the shared C++ core as WebAssembly in any modern browser, on any OS, with direct
device-to-device links and no server. Three ways to ship it, built from the same files:

| Output | What it is | Use |
|---|---|---|
| `dist/web/` | static site + PWA (service worker precaches everything) | any dumb HTTP server; the hub serves it at `http://<ip>:8437/` |
| `dist/web/embermesh.html` | one self-contained file (all JS, CSS, WASM; guide pack optional) | double-click from a USB stick / SD card, `file://`, no server — how the app spreads offline |
| `build/web/wasm/` | `ember.mjs` + `ember.wasm`, `ember.single.mjs` | the raw modules (tests, other embedders) |

## 1. Toolchain (once, needs internet; nothing installed system-wide)

```sh
cd <repo>
git clone --depth 1 https://github.com/emscripten-core/emsdk.git .toolchains/emsdk
.toolchains/emsdk/emsdk install 6.0.13
.toolchains/emsdk/emsdk activate 6.0.13
```

`.toolchains/` is git-ignored. Also needed: CMake ≥ 3.20 and Node ≥ 22 (`node` on PATH; the build
falls back to the Node bundled inside emsdk). After this, every build and test works offline.
Keep a copy of `.toolchains/emsdk` (≈1.5 GB) on the same stick as the repo if you want to rebuild
after the internet is gone.

## 2. Build

```sh
apps/web/build.sh                                  # WASM (all core modules) + dist/web + embermesh.html
apps/web/build.sh --guides path/to/guides.pack     # also ship the guide library (dist/web/data/)
apps/web/build.sh --guides g.pack --inline-guides  # ... and embed it in embermesh.html (can be tens of MB)
apps/web/build.sh --modules "capi"                 # only some core modules (while others are broken)
apps/web/build.sh --skip-wasm                      # repackage only (UI changes)
apps/web/build.sh --debug                          # -O0 -g + assertions
```

Env equivalents: `EMBER_GUIDES_PACK=…`, `EMBER_INLINE_GUIDES=1`, `EMBER_MODULES=…`, `EMSDK_DIR=…`.

What `build.sh` does:

1. `emcmake cmake -S . -B build/web -DCMAKE_BUILD_TYPE=MinSizeRel` → the root CMake includes
   `apps/web/CMakeLists.txt`, which links `ember_core` into two ES modules (`ember.mjs`+`.wasm`, and
   `ember.single.mjs` with the wasm embedded as base64). Flags: no pthreads, `-Os`, no exceptions,
   `ALLOW_MEMORY_GROWTH`, emmalloc, 512 KiB stack, `MODULARIZE`+`EXPORT_ES6`, IDBFS linked in.
   Exports: every function in `ember.h`, `malloc`/`free`, `HEAPU8/HEAP16/HEAP32/HEAPU32`,
   `UTF8ToString`, `FS`, `IDBFS`, plus `ember_web_out_*` layout probes.
2. `node apps/web/tools/package.mjs` (zero npm dependencies, deterministic output) copies `ui/`
   (minus test files) to `dist/web/`, adds `wasm/`, `data/guides.pack`, `hub.json`, the PWA files
   (`manifest.webmanifest`, `icons/`, `pwa.js`, `sw.js`), injects the manifest, favicon (SVG + 32 px
   PNG), Apple touch icon, theme colour and `pwa.js` into `index.html`, builds `embermesh.html`, and
   writes `wasm/build.json` = `{version, build, guides, guides_hash, guides_bytes, total_bytes,
   files:[{path, size, sha256}]}` — the manifest an offline install is verified against. Every URL
   is relative, so the same `dist/web` works at `/`, under `/<repo>/` (GitHub Pages) and from a hub.
   If `ui/index.html` does not exist a placeholder page with an RPC console is used.

Icons: `node apps/web/tools/make-icons.mjs` (needs Chrome; run when `docs/assets/logo.svg`
changes) renders `apps/web/pwa/icons/`: 192/512 px "any", maskable (full-bleed `#17110f`, logo in the
80 % safe zone), monochrome (white glyph), 180 px Apple touch icon (opaque), `favicon.svg`,
`favicon-32.png`. PNGs keep only IHDR/IDAT/IEND (no text, time, EXIF or colour profile). Theme and
background colour `#17110f` = the start-up screen's first frame, so an installed app's splash screen
(icon on that colour) flows into it without a flash.

## 3. Test

```sh
./build.sh desktop web                                  # WASM + dist/web (with build/guides.pack) + emberd
node --test apps/web/test/                               # everything below
EMBER_REQUIRE_BROWSER=1 node --test apps/web/test/       # same, but a missing build/Chrome FAILS instead of skipping
```

Unit tests (Node only, fakes): `webrtc_blob`, `framing`, `singlefile`, `endpoints`, `router` (incl. the
normative startup flow, legacy `links_autostart` migration, hub link re-adding), `backends`,
`wasm_binding` (both built modules in Node), `dist` (packaged output, single-file bootstrap).

End-to-end, real headless Chrome + the real WASM core (`test/lib/e2e.mjs`, driven over the DevTools
protocol, no npm). Each page runs the real UI; tests drive the page's node via `__emberWasmBackend`:

| file | proves |
|---|---|
| `browser.test.mjs` | site over a dumb HTTP server: node answers, no console errors, endpoint kinds; service worker installs; **reload with the network cut off** keeps the same identity and the guide library; hub guest auto-connect; `embermesh.html` from `file://` incl. the second-tab temporary node |
| `e2e_mesh.test.mjs` | two tabs (`?profile=b` = second store on the same origin) joined by `loopback`: first run (`name:""`), DM A→B **delivered** then **read**, Public both ways, SOS (`sos.received`, `sos.list`, kind `sos` message), settings; **reload both** → identity, profile, messages+status, contacts, settings and links (same ids, reopened by the startup step, no duplicates) survive, a DM works again; `host.link.remove` stays removed. **WebRTC**: manual offer/answer blobs between two pages, DM delivered/read + Public over the data channel. **Hub**: starts `dist/desktop/emberd` (temp data dir, spare port, `--web-root dist/web`) and joins two guest pages through it (falls back to a Node relay that mimics `/hub.json` + `/ws` when emberd is missing; `EMBER_HUB=relay` forces it, `EMBER_EMBERD=path` picks another binary); the run prints `# hub used: …` |
| `e2e_library.test.mjs` | real guide pack: `guides.categories/search/get/list`; `tools.run` (dipole 146 MHz = 0.975 m, Ohm's law); `rf.bands` (FRS plan); the **single file with the pack inlined** (built into a temp dir) from `file://`: node, guide search/open, tool, and **loopback between two `file://` tabs** |
| `e2e_morse.test.mjs` | camera Morse from a generated `.y4m` blinking light (fake camera) and from another page's `host.signal.flash` streamed over WebRTC video; sound Morse from a keyed-tone WAV (fake microphone); capture (camera frame readback, microphone, audio endpoint) on http://localhost and in the single file, refused with the documented reason on a plain-http hub page (`--host-resolver-rules` maps `hub.test`); `handheld_vox` links through a VOX model (300 ms attack clipped, 500 ms hold) with DMs delivered both ways; `handheld_ptt` with serial RTS; `rf.radios`. Tests that need a core feature the build lacks are reported as TODO (pending), never as passed; `EMBER_PRESET_FALLBACK=1` runs the radio tests with equivalent explicit settings |
| `e2e_morse.test.mjs` (UI) | the UI's Morse reader (`#/tools/morse?tab=camera`) with the fake camera: the UI capture library + core → on-screen transcript |
| `e2e_remote.test.mjs` | operator page (`?remote=1`) against emberd: `morse.rx.feed` at 30/s, round trip p50/p95, and on a throttled 300 ms link the feeds merge and the backlog stays bounded |
| `e2e_bigpack.test.mjs` | `EMBER_BIG_PACK=<pack>` (default `build/guides.pack`): streamed load with progress, WASM memory ≈ pack + 12 MiB, cached, offline reload from the Cache API, main-thread stall |
| `e2e_offline.test.mjs` | site under `/embermesh/`: fresh load → offline_ready → server gone → cold restart of the same profile → same identity, guide opens, search, message queued; pack download cut at 45 % → not ready → resumes with ranges → complete; update A→B (UI change, same pack: not re-downloaded) → used after restart, A deleted after B booted → C (new pack) via "Restart to update"; truncated B never activated, fixed B downloaded on an explicit check; a B that cannot start is rolled back; upgrade from the previous release's service worker |
| `e2e_boot.test.mjs` | start-up screen in the site, the single file and the offline PWA: static logo is the first paint, node answers during the animation, hands over by itself, a click skips it; reduced motion → calm variant + `data-motion="reduced"` + `host.accessibility`; Simple mode survives a reload |
| `e2e_audio.test.mjs` | `audio` link with the real AFSK1200 modem: node A transmits through its endpoint (core → `ember_audio_tx` → AudioWorklet), the worklet output is routed (MediaStreamAudioDestinationNode) into node B's endpoint as its microphone (AudioWorklet capture → `ember_audio_rx`); B **decodes A's Public message**. Worklet reports **0 underruns** while the core was mid-transmission and played every sample; `link.ptt` on/off fired; serial-RTS PTT (fake Web Serial port) stayed keyed for the whole audio and dropped after it played out. Second test: a WAV made by the core's modulator in Node decodes in the page |

**Microphone in headless Chrome.** In a normal terminal `getUserMedia({audio})` works with
`--use-fake-device-for-media-stream`; in some locked-down environments it never
resolves and a pending request wedges that browser's audio. The audio test probes this once in a
throw-away browser: `real` → real `getUserMedia` (node A hears Chrome's fake beep, the WAV test uses
`--use-file-for-fake-audio-capture`); otherwise the endpoint gets a Web Audio `MediaStream` instead
(the WAV via `decodeAudioData`). On macOS the fake capture device can only read the WAV with
`--disable-features=AudioServiceSandbox`; without it the "microphone" is silent (the test checks
the RX level first and says so). PTT is checked per transmission (`AudioEndpoint.bursts`): each
release happens after that burst's last sample has played (≈60 ms after, = output latency + 30 ms);
back-to-back transmissions stay keyed.

**No hangs.** Every CDP call settles (reply, timeout, or Chrome exit), Chrome runs in its own process
group that is killed with all helpers on close and on process exit, cleanups are individually
time-boxed, hooks have timeouts, and each e2e file has a watchdog (`EMBER_E2E_WATCHDOG_MS`, default
10 min) that kills the browsers and fails the file. `EMBER_E2E_DEBUG=1` logs cleanups and live handles.

## 4. How the page runs the core (`ui/js/backends/wasm.js`)

- Loads `../../wasm/ember.mjs` (relative to the module; `opts.moduleUrl` / `window.EMBER_WASM_URL`
  override). 64 bytes from `crypto.getRandomValues` → `ember_create`.
- **Storage**: IDBFS mounted at `/ember` and populated (`syncfs(true)`) *before* `ember_create`
  (`storage_dir:"/ember"`). Flushed (`syncfs(false)`, serialised) right after `ember_create` (a
  first run's identity), on every `storage.dirty` (leading edge, then at most every 400 ms) and on
  `pagehide` / tab hidden. `navigator.storage.persist()` is requested.
  A Web Lock (`embermesh-storage`) makes the first tab own the store; further tabs of the same
  origin run a temporary in-memory node with its own identity (`host.info.storage:"memory"` +
  `storage_note`) instead of corrupting it. No IndexedDB → in-memory, with a note.
- **Loop**: `ember_tick(now)` → drain `ember_poll` (events → listeners, `LINK_TX` → endpoint) →
  re-arm after `ember_next_wakeup_ms` (4 ms … 60 s). Any input (RPC call, link bytes, audio) ticks on
  the next microtask. A tiny blob Worker timer backs up the page timer because browsers throttle page
  timers in background tabs (≥1 s, up to 1/min in Chrome) far more than worker timers; arriving
  WebSocket/WebRTC/serial data also wakes the page. Phones still freeze background tabs eventually:
  use `host.keep_awake` and keep the tab visible on a relay.
- **Audio**: `getUserMedia` is given 30 s (`media_timeout_ms` endpoint config overrides); a refusal
  (`denied`) or no answer (`io_error`) fails `host.link.create` and removes the link again.
- **Actions** (`host.info.actions`): "Download single-file copy to share" (`host.web.download_single_file`,
  http(s) only) and "Install as app" (`host.web.install`, when the browser fired `beforeinstallprompt`).
- **Startup**: `node.ready` comes out of the first tick → router startup step (below) → hub join.
- **Audio**: AudioWorklet where allowed; on a `file://` page (single file) Chrome refuses worklet
  modules from `blob:` URLs, so the endpoint uses a 4096-frame ScriptProcessor there
  (`endpoint.mode` = `worklet` | `scriptprocessor`; both proven with the real modem).
- **Guides** (big packs): loaded in the background — `init()` does not wait. The response is
  streamed straight into one WASM heap buffer sized from `Content-Length` (no intermediate
  ArrayBuffer, so memory is ~1x the pack) and handed to `ember_guides_load(copy=0)`. Events
  `guides.progress {received,total,source}`, `guides.loaded {bytes,ms,source}`, `guides.error`,
  `guides.cached`; `host.info.guides` mirrors the state (`none|loading|loaded|error`). Over http(s)
  the pack lives in its own content-addressed Cache API cache `embermesh-guides-<hash>` (filled by
  the page while it streams, served cache-first by the service worker, kept across app updates
  that do not change the library); it is **not** in the app-shell precache, so installing or
  updating the PWA does not download tens of MB. `build.json` carries `guides_hash`/`guides_bytes`.
- **Morse capture** (RPC_API §6): `morse.rx.*` go to the core; while a session is open the screen
  wake lock is held (`KeepAwake` holders: the user switch + one per session; released on
  `morse.rx.stop`, when the core says the session is gone, or after 65 s without a feed).
  `host.info.features` gains `camera` / `microphone`; `unavailable_features.camera|microphone` say
  why not (plain-http hub page → "needs a secure page … download the single-file copy from the hub").
  The browser captures with `getUserMedia` itself, so the native `capture_*` features are never
  advertised by the web host (the remote backend strips the daemon's).
- **Remote backend + `morse.rx.feed`**: `FeedBatcher` (`ui/js/endpoints/capture.js`) keeps one feed
  per session in flight; feeds made meanwhile are merged (light → `times_ms`, audio → concatenated
  PCM), the backlog is capped (4000 light samples / 20 s of 8 kHz audio, oldest dropped), and nothing
  is sent while the socket has >256 KB buffered or is down.
- `host.*` methods → `ui/js/endpoints/router.js`; everything else → `ember_call`.
- Debug handle: `window.__emberWasmBackend` (first instance; used by the browser tests).

### Hub guest mode — detection convention (for hub implementers)

When the page is served over `http(s):`, `wasm.js` fetches **`hub.json` at the site root**
(next to `index.html`, `cache: no-store`). `dist/web/hub.json` ships as `{"embermesh_hub": false}`
so a dumb static server answers "not a hub" without a 404. **A hub must serve that path itself**
(instead of the file) with:

```json
{"embermesh_hub": true, "name": "Camp North", "ws": "/ws", "version": "0.1.0", "node_id": "…"}
```

`Content-Type: application/json`, `Cache-Control: no-store`. `ws` may be a path or an absolute
`ws(s)://` URL (default `/ws`). The browser then adds a `websocket` endpoint + `raw` link to it
(`links.add persist:false`; re-detected on every load and re-added after unlock/wipe; an existing core
link with the same URL is reused). The service worker never caches `/ws`, `/rpc`, `/download/` and fetches `hub.json`
network-first. URL overrides: `?hub=ws://host:8437/ws` forces a hub, `?nohub=1` disables detection.

### Links: the core is the only saved-link list (RPC_API "Link persistence")

`host.link.create` opens the endpoint, then `links.add {type, name, config, endpoint:{kind, config},
persist}`; `persist` is true unless `autostart:false` or the kind cannot be reopened (WebRTC needs a
new pairing; hub links are re-detected each load). The router keeps no list. On **every** core
`node.ready` (create, unlock, duress unlock, wipe) it runs its startup step: `links.list` → close
endpoints of links that disappeared → for each link with an `endpoint.kind`, reopen it from
`endpoint.config` without prompting (serial via `navigator.serial.getPorts()` matched by USB VID/PID,
BLE via `getDevices()`) → `links.endpoint_state {connected}` (or `connected:false, error` — also for
kinds this browser cannot provide and for WebRTC) → re-add the hub link if a hub was detected. A node
started locked has no links; startup simply runs again after unlock. A `link.state` with
`removed:true` releases the endpoint. Old `settings.links_autostart` / `localStorage` entries are
migrated into core links once and then cleared. `link.ptt` → `endpoint.setPtt` (serial RTS/DTR;
the audio endpoint holds PTT until the audio queued in its worklet + `outputLatency` has played).

`?profile=<name>` (or `opts.profile`) gives a separate IndexedDB store (`/ember-<name>`), lock and
identity on the same origin (two-node demos, tests); without it a second tab gets a temporary node.

### Offline installs and updates (RPC_API §9; `apps/web/pwa/sw.js`, `ui/js/endpoints/offline.js`)

Hosted on https (e.g. GitHub Pages under `/<repo>/`) the app makes itself independent of the web:

- **What is stored**: on the first visit (and again on `appinstalled`), once the node is up, the
  page downloads every file listed in `wasm/build.json` into `embermesh-app-<build>` (each checked
  against its size and sha256), stores the manifest next to them, and keeps the guide library in
  `embermesh-guides-<hash>` as 4 MiB chunks (the copy the page loads anyway is teed into it, so it
  is fetched once). A pack download that breaks off keeps its complete chunks and resumes with HTTP
  range requests (GitHub Pages and most servers support them; without ranges it streams again and
  skips what is stored). The pack is sealed only after its size and sha256 match the manifest; the
  build is `offline_ready` only when every file and every chunk is in. `navigator.storage.persist()`
  is requested and reported honestly (`persistent`); before downloading, `storage.estimate()` is
  checked and a shortage is reported with the number of MB to free.
- **Serving**: the service worker holds no build data. It serves the build named `active` in the
  shared state record (`embermesh-state/state.json`) — all files of one build, never a mix —
  assembles `data/guides.pack` from its chunks, falls back to the network for anything else, and to
  the stored `index.html` for navigations (unknown paths in the scope redirect to the app root).
  Cold start, reload, guides, search and the node's data all work with the network physically gone.
- **Updates**: at start, every 6 hours and when the browser comes back online, the page fetches
  `wasm/build.json?network=…` (network only, 6 s timeout; offline: nothing happens, no retries). A
  newer build is downloaded completely into a **new** cache in the background (the pack only if its
  hash changed), verified, re-checked against the site, and marked `pending`. The worker switches to
  it on the next start (a navigation with no other window of the app open) or at once via "Restart to
  update" (`host.app.check_update {apply:true}`). The new build is on trial until its node has
  started; only then are the previous build's caches deleted. A build that fails to start (the page
  reports it, or it was started twice without confirming) is rolled back to the previous one and
  marked failed; automatic checks retry it after an hour, an explicit check at once. A corrupt or
  partial download is never activated.
- **Worker upgrades**: `skipWaiting` + `clients.claim` are safe because the worker logic is
  build-independent; the previous release's per-version precache (`embermesh-<version>`) keeps
  serving as a fallback until a complete build has replaced it, then it is deleted (tested).
- **Not available**: on a plain-http hub page (`http://<ip>:8437/`, not a secure context: no service
  worker; the status says so — use the single file from the hub instead) and in the single file
  (it is already self-contained; with the pack inlined it has the library too).
- **Status / controls**: `host.app.offline_status|make_offline|check_update`, event
  `host.app.offline`, and the actions "Download everything for offline use", "Check for updates",
  "Restart to update" with the state in their descriptions.
- **Per browser** (from the browsers' documented behaviour; only Chrome is tested here):
  Chrome/Edge (desktop, Android) offer "Install app" (`beforeinstallprompt` → "Install as app"
  action); stored data of an origin is "best effort" and can be evicted under storage pressure
  unless `persist()` is granted (installed apps and frequently used sites usually are). Safari on
  iOS/iPadOS: "Share → Add to Home Screen"; Safari may delete script-written storage of a website
  not used for about a week when it is *not* added to the Home Screen — the status shows a warning
  then; a Home Screen app has its own storage. Firefox: works as a website; no install prompt on
  desktop. Nothing is ever fetched from a third party; WebRTC pairing, radios (Web Serial/Bluetooth),
  the audio modem and every core feature keep working offline; what needs a network is only what
  inherently does (a hub, WebSocket links, update checks).

### Start-up screen, motion, Simple mode (RPC_API §8)

The UI's start-up screen (`js/boot/`, `css/boot.css`), motion system and accessibility preferences
(`lib/prefs.js`, `css/a11y.css`) are packaged like every other UI file (site, offline cache, single
file with its CSP rewritten). The first paint is the static logo in every packaging; the core starts
loading in parallel and answers while the animation runs. Under automation (`navigator.webdriver`)
the UI skips the animation; `e2e_boot.test.mjs` presents itself as a normal browser to test it.
`host.info.accessibility` reports `prefers-reduced-motion`, `prefers-contrast` and `forced-colors`
(`host.accessibility` on change); `host.haptic` uses the Vibration API (Android Chrome; only after a
user gesture; elsewhere `unsupported`).

## 5. Other backends

- `native.js` — Android `window.EmberNative.postMessage(str)` / iOS
  `window.webkit.messageHandlers.ember.postMessage(str)`; replies and events arrive at
  `window.__emberBridge.onReply/onEvent` (object or JSON string). If native code puts an array of
  early events in `window.__emberBridge.queued` before the page loads, they are delivered on init.
- `remote.js` — `ws(s)://<host>/rpc?token=…` (`?remote=1&host=…&token=…&tls=1`; host defaults to the
  page's host, else `localhost:8437`). Reconnects with backoff; calls made while disconnected wait up
  to 30 s; close codes 4401/4403/1008 = bad token → `denied`. Emits `remote.state {connected}`.
  QR scan, file save/pick, notify, keep-awake, flash and tone run in *this* browser.

## 6. Endpoints (`ui/js/endpoints/`)

Common interface (`base.js`): `prepare()` (device pickers; needs the user's click) · `open()` ·
`close()` · `send(bytes)` · `onData(cb)` · `onState(cb)` · `setPtt(on)` · `describe()`; module
descriptor with `detect(env)` → `{available, reason}`, Field-schema `config`, `linkTypes`, `scan`.
`host.info.endpoint_kinds[*].link_types` comes from core `links.types` (`endpoint_kinds`); the static
lists below are only a fallback for a core without `links.types`. With the real core: websocket,
webrtc, loopback → raw; serial, ble → stream, kiss, meshtastic, rnode, lora_at; audio → audio.

| kind | transport | link types | notes |
|---|---|---|---|
| `websocket` | WebSocket client, 1 message = 1 frame | raw | reconnects; https pages cannot open `ws://` (reason shown) |
| `webrtc` | RTCPeerConnection data channel | raw | manual pairing via `host.webrtc.*`; blob `ember:rtc:<base64url>` ≈ 100–180 chars (QR-friendly): ICE ufrag/pwd, DTLS fingerprint, ≤8 candidates (IPv4/IPv6/mDNS), CRC-16. Full ICE gathering first, no STUN needed on a LAN/hotspot, optional user STUN. `accept_offer` also returns `link` |
| `serial` | Web Serial, byte stream | kiss, meshtastic, rnode, lora_at, stream | baud/bits/parity/stop/flow, DTR/RTS levels, PTT on RTS or DTR (`link.ptt`), invert |
| `ble` | Web Bluetooth central, byte stream | stream, kiss, raw | Ember peer service, Nordic UART, or custom UUIDs; chunked writes (default 20 B), auto-reconnect |
| `audio` | getUserMedia (EC/NS/AGC off) + AudioWorklet → `ember_audio_rx`; `ember_audio_tx` → output | audio | input/output device (output via `AudioContext.setSinkId`), TX level, PTT = VOX or serial RTS/DTR |
| `loopback` | BroadcastChannel between tabs | raw | demos and two-node tests (also works in Node) |

Device services (`device.js`): `host.qr.scan` (BarcodeDetector + camera overlay),
`host.file.save` (download link; `share:true` uses the share sheet) / `host.file.pick`,
`host.location.get/watch` (pushes `position.set`, emits `host.location`), `host.signal.flash`
(full-screen, tap to stop; `torch:true` tries the camera torch), `host.signal.tone` (click-free Web
Audio), `host.keep_awake` (Wake Lock, re-acquired when visible), `host.notify` (via the service
worker when present). `host.hub.*` → `unsupported` (browsers cannot listen). `host.info` adds
`unavailable_features {feature: reason}`, `storage`, `secure_context`, `single_file`, `hub`.

### Browser capability matrix

| | Chrome/Edge desktop | Chrome Android | Firefox | Safari macOS | iOS (all browsers) |
|---|---|---|---|---|---|
| core, storage, websocket, webrtc, loopback, file, tone, flash | ✓ | ✓ | ✓ | ✓ | ✓ |
| serial (Web Serial) | ✓ | – | – | – | – |
| ble (Web Bluetooth) | ✓ (Linux: flag) | ✓ | – | – | – |
| audio modem | ✓ | ✓ | ✓ | ✓ | ✓ |
| audio output device choice | ✓ | – | – | – | – |
| qr_scan (BarcodeDetector) | macOS/ChromeOS only | ✓ | – | – | – |
| location, keep_awake | ✓ | ✓ | ✓ | ✓ | ✓ |
| notify | ✓ | ✓ | ✓ | ✓ | installed PWA only |
| torch flash | – | ✓ | – | – | – |
| camera / microphone capture (Morse reader) | ✓ | ✓ | ✓ | ✓ | ✓ (secure page) |
| radio PTT on a serial line (`handheld_ptt`) | ✓ | – | – | – | – |

Only the Chrome desktop column is exercised by the tests; the others follow the APIs' documented
support. `host.info.audio` reports the PTT methods (`vox`, `manual`, `serial` with Web Serial) and
`presets_unsupported` with the reason (no Web Serial → `handheld_ptt`; insecure page → all).

**Secure context matters**: microphone, camera, serial, BLE, location, wake lock, notifications and
the service worker need HTTPS, `localhost`, or `file://` (the single file). A hub page at
`http://192.168.x.x:8437/` is *not* secure: there the app works (WebSocket to the hub, WebRTC,
loopback, files, tone, flash) and every other capability reports `available:false` with that reason.
Use the single file (or the native apps) to get radios/audio on a phone.

## 7. Single file internals (`apps/web/tools/singlefile.mjs`)

Browsers refuse ES-module imports from `file://`. Each module is embedded as text
(`<script type="text/x-ember-module">`); a classic bootstrap turns them into `blob:` URLs and installs
an import map from canonical URLs `https://embermesh.invalid/<path>`. Module sources are rewritten:
relative specifiers → canonical, `import.meta.url` → canonical, `import(x)` →
`__emberImport(<module url>, x)` (computed specifiers work). Non-JS assets are base64 in one JSON
block and served by a `fetch()` shim (canonical or document-relative URLs; unknown canonical URLs
404 locally). Stylesheets → `<style>`, icons/images → `data:`. If the page has a CSP `<meta>`, its
`script-src` gets `blob:`, the bootstrap nonce (given to the import map) and sha256 hashes of the
inline scripts; `style-src` gets the style hashes. Text that the HTML parser treats specially
(`</script`, `<!--`) is escaped as `\x3C`.

## 8. Sizes (all core modules)

| artifact | size |
|---|---|
| `ember.wasm` (baseline, `-Os`) | 1 463 KiB raw, 591 KiB gzip -9 |
| `ember-simd.wasm` (`-msimd128 -O3`, site/PWA only) | 1 834 KiB raw, 708 KiB gzip -9 |
| `ember.mjs` loader | 63 KiB (each module has its own) |
| `embermesh.html` without the guide pack | 3.2 MiB (baseline module only) |
| `embermesh.html` with the 58 MiB pack inlined (`--inline-guides`) | 80.4 MiB (works; slow to copy and open on old phones) |
| `dist/web` total | ≈4.5 MiB + the pack (55.5 MiB / 5333 guides with the semantic section) |
| offline install (everything, `storage.estimate`) | 62.4 MiB |
| start-up screen, motion, Simple mode (UI files) | +127 KiB (+44 KiB gzip) in site, cache and single file |
| PWA icons | +78 KiB in site and cache; +12 KiB (favicons + touch icon as `data:`) in the single file |
| offline-install code (`offline.js`, `sha256.js`, `sw.js`) | +38 KiB |

**WebAssembly SIMD** (decided by measurement): the site and PWA ship a second core module built with
`-msimd128 -O3`; `wasm.js` loads it when `WebAssembly.validate` accepts a SIMD probe (the
wasm-feature-detect test, inlined) and falls back to the baseline module if the engine lacks SIMD,
`?nosimd=1` is given, or the SIMD module fails to load (`host.info.wasm_simd` says which runs).
Measured in headless Chrome with the 58 MiB pack, `guides.search` (hybrid) median per query: SIMD
11.7–13.0 ms, baseline 15.9–19.6 ms (≈ −26 %); in Node, hybrid 16.2 vs 22.1 ms, keyword-only 12.1 vs
13.9 ms. `-msimd128` at `-Os` gave no gain (the loop vectoriser stays off), hence `-O3` for that module.
Cost: +1.8 MiB (+0.7 MiB gzip) per offline install and per update that changes the core. The single
file stays baseline-only (it must run in old web views; +2.4 MiB base64 would not pay off there).
`apps/web/build.sh --no-simd` builds only the baseline module.

Guide packs and the single file:

- `./build.sh web` ships the pack as `dist/web/data/guides.pack` and does **not** inline it into
  `embermesh.html` (the single file then has no library and spreads easily). `--inline-guides`
  embeds it as base64: about 1.34 x the pack.
- Hub download: one file, not split. It is served with `Content-Length` (progress), cached
  content-addressed in the browser (downloaded once per library version, survives app updates and
  offline reloads), and loaded with ~1x memory. The emberd hub also answers HTTP `Range` requests
  (see apps/desktop/README.md); an interrupted offline install resumes with ranges.
- Measured with the real 57.9 MiB / 5490-guide pack (`e2e_bigpack.test.mjs`, served at 8 MiB/s like a
  hub over Wi-Fi): download + load 8.2–11.3 s with ~100 progress events; node.info and messages.send
  answer in <10 ms meanwhile (guides empty until the pack is in); longest main-thread stall 14–19 ms;
  WASM memory 71 MiB (pack + ~13 MiB); reload from the Cache API 69–89 ms.
- Offline install from localhost (`e2e_offline.test.mjs`): node interactive after 0.25–1.3 s,
  everything stored and verified (62.3 MiB) after 0.7–3 s; cold start with the server gone: node
  answers after 0.2–0.4 s, guides from the device.
- First paint (`e2e_boot.test.mjs`): the static logo at 40–80 ms in the site, the single file and
  the offline PWA (28–44 ms without the start-up screen, `?noboot=1`); the core starts in parallel and
  answers `node.info` in <1 ms while the animation runs; app interactive ~170–200 ms (one run under
  heavy machine load: 0.9 s).

Defaults held up with the real core: 16 MiB initial memory + growth, 512 KiB stack, IDBFS mounted
and populated before `ember_create`.

## 9. Proven vs unproven

Proven by the tests above (headless Chrome on a desktop OS): WASM node with all modules; IndexedDB
persistence across reloads (identity, messages + delivery/read status, contacts, settings, links);
the normative link startup flow; loopback (http and file://), WebRTC manual pairing on one machine,
hub guests through the real emberd; guides from the real pack (fetched and inlined); tools and band
plans; offline PWA reload; the audio modem chain from `ember_audio_tx` through AudioWorklet playout
and Web Audio capture into `ember_audio_rx` with gapless playout and PTT timing.
Morse receive: camera (Chrome's fake camera playing a generated blinking-light video, and another
page's screen flash streamed as WebRTC video) and sound (fake microphone playing a keyed tone) decode
to the right text in the core, including through the UI's Morse reader screen; capture works under
the site's and the single file's CSP and is refused with a reason on a plain-http hub page. Radio
presets: `handheld_vox` through a modelled VOX (300 ms clipped) and `handheld_ptt` with serial RTS,
Public messages both ways. Big packs (27–55 MiB) stream in the background with ~1x memory and come
back offline from the cache; `morse.rx.feed` through the remote backend at 30/s.

Not proven: a physical camera, microphone/sound card and speaker/radio output (only Chrome's fake
capture devices and Web Audio, see §3); a real radio's VOX (only a model); real lighting/flicker and
a hand-held phone camera for Morse;
real Web Serial / Web Bluetooth devices (only fakes; PTT on a real RTS line); WebRTC across two
devices / NAT / mDNS on a real LAN; Firefox, Safari and mobile browsers (the capability matrix in §6
is from the APIs' documented support, not from runs); background-tab throttling on phones; hub
guests over a real Wi-Fi hotspot; IndexedDB eviction under storage pressure.

## 10. Implementation notes

- Audio links: the AX.25 carrier drops frames whose source equals its own callsign. Since core
  protocol v2, licence-free audio links get a per-node pseudo station id instead of `N0CALL`, so
  default nodes hear each other; the audio tests still set explicit callsigns per station.
- `host.link.reconnect` and a `link.state` whose `link.endpoint` differs from the open endpoint
  (`links.configure {endpoint}`) reopen the endpoint from the descriptor stored in the core.
- Nodes that talk to each other (WASM pages, emberd) must be built from the same core/ tree once the
  wire format changes: rebuild both (`./build.sh desktop web`) before debugging dropped frames.
- Verified against the real core: `links.add/list/remove/endpoint_state`, `links.types`
  `endpoint_kinds` names, `storage.dirty`, `link.ptt {link_id, on}`, `ember_audio_tx` (exactly `max`
  samples mid-transmission, fewer on the last call, 0 idle), store files under `/ember` (IDBFS),
  guide pack zero-copy, 512 KiB stack (raise `EMBER_WEB_STACK_SIZE` if deep JSON recursion overflows).
- `apps/web/build.sh` reuses the CMake generator a build dir was configured with, so a `build/web`
  configured with Makefiles keeps working when ninja appears on PATH.
- `ember_tick` takes `uint64_t`: passed as a JS BigInt (`WASM_BIGINT` is the default).
- Generated output is deterministic; `wasm/build.json.build` and the SW cache name are a content hash.

## 11. Privacy notes

- **Build output** embeds no machine information: `wasm/build.json` is `{version, build, guides,
  guides_hash, guides_bytes}` (`build` and the service-worker cache name are content hashes), no
  timestamps, no source maps. Sources are compiled with `-ffile-prefix-map=<repo>=.`
  (`apps/web/build.sh` and the top-level CMake), so even `--debug` builds name files relative to the
  repository — still, publish only release builds (`--debug` adds DWARF and assertions).
- **WebRTC pairing codes** (`ember:rtc:…`, shown as QR or text) contain, by design: the ICE
  username fragment and password and the DTLS certificate fingerprint of this pairing (single-use),
  and up to 8 ICE candidates — host candidates with this device's **LAN IP addresses** (or Chrome's
  random `.local` mDNS names instead), and, only if the user configured a STUN server, the device's
  **public IP address and port** as seen by that server. No identity keys, names or messages. Anyone
  who sees the code learns those addresses; the code is useless after the pairing (or 15 minutes).
- **Contact cards / channel invites** (`ember:` URIs) carry the node id, name and public keys.
- `host.info` reports capabilities only; it does not read device labels. Audio device names appear
  only in `host.endpoints.scan` results the user asked for, and are stored only if the user picks one
  (as the device id in the link's endpoint config, inside this browser's storage).
