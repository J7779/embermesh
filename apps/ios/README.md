# EmberMesh for iPhone and iPad

The iOS shell is a Swift app (no third-party dependencies) that hosts the shared web UI (`ui/`) in a
`WKWebView` and runs the shared C++ core (`core/`) natively. It provides the hardware *endpoints*
(Bluetooth, Apple peer-to-peer, audio, TCP/WebSocket/UDP) and device services (GPS, camera QR,
flashlight, files, notifications). All protocol logic stays in the core (see `docs/ARCHITECTURE.md`).

- Bundle id `org.embermesh.app`, minimum **iOS 15.0** (iPhone 6s / SE 1st gen / 7 and newer, every iPad since 2015 that runs iOS 15).
- Swift 5 language mode, UIKit + WebKit. Core C API imported as the clang module `CEmber` (`CEmber/module.modulemap` → `core/include/ember/ember.h`).

## Layout

```
apps/ios/
  EmberMesh.xcodeproj         committed, hand-maintained; folders are "synchronized groups" so new
                              .swift files under EmberMesh/ and EmberMeshTests/ are picked up automatically
  build-core.sh               CMake build of core/ + host/ (hub) + native/ → build/ios/core/<sdk>/lib/libember_ios.a
  native/                     CMake project for iOS + ember_ios_hub.{h,cpp} (C wrapper embedding the host library's Hub)
  bundle-resources.sh         copies ui/ → App/www, Diagnostics/ → App/www-diag, guides.pack → App/guides.pack,
                              dist/web → App/hubweb (web app served by the phone hub)
  CEmber/module.modulemap     exposes ember.h + ember_ios_hub.h to Swift
  Config/Info.plist           permissions, background modes, Bonjour services, scenes
  Config/EmberMesh-multicast.entitlements   optional (only with Apple's multicast grant)
  Diagnostics/bridge-test.html              bridge test page (fallback start page)
  EmberMesh/App               AppDelegate/SceneDelegate, AppEnvironment (wiring, background)
  EmberMesh/Core              EmberNode (whole C API), NodeRunner (serial queue, adaptive tick)
  EmberMesh/Bridge            WKWebView host, embermesh:// scheme handler, JS bridge router
  EmberMesh/Host              HostRouter (host.*), LinkManager (core-driven link startup on node.ready)
  EmberMesh/Endpoints         ble_peer, ble, multipeer, audio, tcp, tcp_server, udp, websocket, ws_server (hub), rtl_tcp, loopback
  EmberMesh/Services          location, QR, files, flash/tone, notifications
  EmberMeshTests              XCTest (unit + simulator integration against the real static library)
```

The Xcode project lists **no core files**. The first build phase runs `build-core.sh $PLATFORM_NAME`,
which configures `apps/ios/native/` with CMake. That project adds `core/` (sources globbed by
`core/CMakeLists.txt`, all modules) and `host/` (`EMBER_HOST_AUDIO=OFF`, `EMBER_HOST_SERIAL=OFF`, no
tests) plus the hub wrapper, and the script merges the three static libraries into
`libember_ios.a` (linked with `-lember_ios -lc++`). New core modules are compiled automatically.

CMake ≥ 3.20 is needed (3.22 is known to work). The script
probes candidates from inside the repo with a 20 s timeout and uses the first that answers:
`$EMBER_CMAKE`, the one that worked last time (`build/ios/core/.cmake-path`), `cmake` on PATH,
`.toolchains/bin/cmake`, `.toolchains/cmake/bin/cmake`, Homebrew, CMake.app, the Android SDK's
cmake. All are probed by absolute path, so it works from the Xcode build phase (minimal PATH). A
cmake that hangs (for example one blocked by macOS privacy protection when the repo is inside a
protected folder such as ~/Documents) costs the 20 s probe once and is then skipped.

## Build, run, test

Everything below uses its own DerivedData under `build/ios/`. Run from the repo root.

```sh
# (optional) build the core alone, both SDKs, plus an XCFramework
apps/ios/build-core.sh all --xcframework

# simulator build
xcodebuild -project apps/ios/EmberMesh.xcodeproj -scheme EmberMesh \
  -destination 'platform=iOS Simulator,name=iPhone 17' -derivedDataPath build/ios/DerivedData build

# install + launch on a booted simulator
xcrun simctl boot "iPhone 17"            # or a UDID
xcrun simctl install booted build/ios/DerivedData/Build/Products/Debug-iphonesimulator/EmberMesh.app
xcrun simctl launch booted org.embermesh.app            # add -EmberDiagnostics to force the test page
xcrun simctl io booted screenshot /tmp/ember.png

# tests (unit + integration with the real libember_core.a + WKWebView bridge round trip)
xcodebuild -project apps/ios/EmberMesh.xcodeproj -scheme EmberMesh \
  -destination 'platform=iOS Simulator,name=iPhone 17' -derivedDataPath build/ios/DerivedData test

# generic device build (compile check, unsigned)
xcodebuild -project apps/ios/EmberMesh.xcodeproj -scheme EmberMesh -configuration Release \
  -destination 'generic/platform=iOS' -derivedDataPath build/ios/DerivedData CODE_SIGNING_ALLOWED=NO build
```

The default is all core modules. `EMBER_MODULES=capi` (xcodebuild build setting or environment)
still builds just the C API for emergencies; `EMBER_SKIP_CORE_BUILD=1` reuses the last libraries.

`./build.sh ios` (simulator + unsigned device build into dist/ios/) and
`./build.sh test --device-tests` (runs this XCTest suite on a booted simulator) work unchanged.

**Test-only scheme argument:** the scheme's Test action passes `-XCTDisableAggressiveSymbolication YES`.
Without it, the first failing assertion hangs the whole run: XCTest symbolicates the call stack
in-process by opening the object files named in the debug map (absolute paths under the repo), and
in the simulator that `open()` can block forever when the repo is inside a macOS privacy-protected
folder (such as `~/Documents` or `~/Desktop`).

Logs: `xcrun simctl spawn booted log stream --predicate 'process == "EmberMesh"' | grep '\[ember\]'`.
In Debug builds the WebView is inspectable from Safari ▸ Develop.

### Web content

- `ui/` is copied to `<App>/www` at build time and served at `embermesh://app/` by a
  `WKURLSchemeHandler`: ES modules, `fetch()`, a stable origin and `window.isSecureContext === true`
  all work offline. If `ui/index.html` is missing the app starts with `Diagnostics/bridge-test.html`.
- `guides.pack` is taken from `$EMBER_GUIDES_PACK` (env or build setting) or the first of
  `build/guides.pack` (canonical, `./build.sh guides`), `build/guides/guides.pack`, `dist/guides.pack`,
  `guides/guides.pack`, `ui/guides.pack`. It is memory-mapped (`mmap`, read-only, pages loaded lazily
  and droppable under memory pressure) and handed to the core with `ember_guides_load(..., copy=0)`;
  it is also served at `embermesh://app/guides.pack`.
- `dist/web` (`./build.sh web`), when it contains the WASM build, is bundled as `hubweb/` and served
  by the phone hub so guests get a full node in their browser. A guide pack inside it that equals
  the app's pack is hard-linked, not stored twice. Without it, guests get the host library's built-in page.
- **No remote content**: a `WKContentRuleList` blocks every URL that is not `embermesh:`, `data:`, `blob:`
  or `about:`; navigation to http(s) is cancelled (a tapped link opens Safari instead).

### Bridge (docs/RPC_API.md §5)

JS → native: `window.webkit.messageHandlers.ember.postMessage(JSON.stringify({id, method, params}))`.
Native → JS: `window.__emberBridge.onReply({id, ok, result?, error?})` and `window.__emberBridge.onEvent({event, data})`.
`host.*` goes to the Swift `HostRouter`, everything else to `ember_call` on the node queue. A
document-start script buffers replies/events that arrive before `ui/js/backends/native.js` assigns
`window.__emberBridge` (assigning the object or its `onReply`/`onEvent` properties both flush it).
Extra events from the host: `host.endpoint.state` (`{link_id, connected, error?, kind, status?}`),
`host.location`, `host.hub.clients`. Platform extras follow the UI convention
`host.info.actions:[{id,title,description,method,params?}]`; iOS has one:
`{id:"ios.settings", method:"host.ios.open_settings"}` (opens the app's page in iOS Settings, where
Bluetooth / Local Network / Microphone / Camera / Location permissions are changed).

`ember:` links opened from another app (camera QR, Messages, AirDrop) are not sent to the UI. The
shell passes them to the core:
`contacts.add {card}`, and if that fails `channels.join {invite}`, then posts a local notification
with the result.

## Links: the core is the source of truth

There is no saved-link list in the iOS shell (a stray `links.json` is deleted on start). Per
docs/RPC_API.md "Link persistence (normative)":

- `host.link.create` validates the endpoint config, checks that the link type is compatible with the
  endpoint kind, opens the endpoint and calls `links.add {type, name, config, endpoint:{kind, config,
  autostart}}`; the core persists it (stable id).
- On **every** `node.ready` (after `ember_create`, after unlock, duress unlock, wipe)
  `LinkManager.syncWithCore()` runs `links.list`, reopens the endpoint of each link from
  `link.endpoint {kind, config}` and reports `links.endpoint_state`. Endpoints already open with the
  same descriptor are kept; endpoints whose link disappeared (wipe) are closed; kinds this device
  cannot provide are reported as `connected:false` with the reason. A node that starts **locked**
  lists no links, so nothing opens until `security.unlock` (which re-emits `node.ready`).
- Compatible link types per endpoint kind come from the core's `links.types` (`endpoint_kinds`);
  `EndpointCatalog.linkTypes` is only a fallback before the catalogue is loaded. `ble_peer` therefore
  offers only `stream`; `tcp` offers stream, kiss, meshtastic, rnode, lora_at, rtl_tcp.

## Hub on the phone (`ws_server`, `host.hub.*`)

`host.hub.start {port?=8437}` creates a persisted `raw` link (preset `websocket`) whose endpoint is
`ws_server`; `host.hub.stop` removes it; `host.hub.status` → `{running, urls, clients, port, link_id}`;
`host.hub.clients` events. The endpoint embeds the **host library's Hub** (`host/src/host/hub.cpp`,
built for iOS with audio + serial off) through `native/ember_ios_hub.cpp`: one HTTP/WebSocket server on
its own thread serving the bundled web app at `/`, `/hub.json` (`{"embermesh_hub":true,"name":…,"ws":"/ws"}`,
`Cache-Control: no-store`), `/download/` (the single-file `embermesh.html`) and the `/ws` mesh bus
(binary, one mesh frame per message). The Swift shell keeps driving the node (`ember_host_create`
is not used because it must be the node's only `ember_poll` consumer); frames for the hub link are
handed to the wrapper, which broadcasts them to every guest. `/rpc` remote control is refused (no
admin token on the phone).

Personal Hotspot: turn it on, let guests join it, start the hub; the URL is usually
`http://172.20.10.1:8437/`. The hub runs only while the app is in the foreground (iOS suspends
sockets of background apps): use *keep awake* and a charger.

## Installing on a device without the App Store

1. Open `apps/ios/EmberMesh.xcodeproj` in Xcode, select the **EmberMesh** target ▸ *Signing & Capabilities*.
2. Tick *Automatically manage signing* and pick a **Team**. A free *Personal Team* (any Apple ID, added
   under Xcode ▸ Settings ▸ Accounts) works. If the bundle id is taken for your team, change it
   (e.g. `org.embermesh.app.yourname`).
   Command line: add `DEVELOPMENT_TEAM=ABCDE12345 -allowProvisioningUpdates` to `xcodebuild`.
3. Plug in the iPhone, enable *Developer Mode* (Settings ▸ Privacy & Security, iOS 16+), choose it as the run destination, Run.
4. First launch: Settings ▸ General ▸ VPN & Device Management ▸ trust your developer certificate.

Limits and alternatives:
- **Free Apple ID**: the app expires after **7 days** (re-run from Xcode to renew; data is kept), max 3 apps per device.
  Paid Apple Developer Program ($99/yr): 1-year provisioning, **Ad Hoc** builds for up to 100 registered
  devices per type, and **TestFlight** (90-day builds, needs internet to install).
- **AltStore / SideStore** re-sign with a free Apple ID and refresh the 7-day signature over Wi-Fi.
- **EU (iOS 17.4+)**: alternative app marketplaces / web distribution are possible for registered developers.
- **Before the grid goes down**: install and open the app once on every phone (permissions granted,
  guide library included). An expired free-signed app will not launch offline — prefer Ad Hoc/TestFlight builds for preparedness kits.
- No install possible at all? Join a **hub** from Safari (`http://<hub-ip>:8437/`): the web app runs the
  full node in the browser (WASM) — no installation, no signing.

## Permissions (what and why)

| Prompt | Used for |
|---|---|
| Bluetooth | phone↔phone chat (`ble_peer`), Bluetooth TNCs / Meshtastic (`ble`) |
| Local Network | Apple peer-to-peer (Multipeer), Bonjour discovery, TCP listener, hub WebSocket, UDP |
| Microphone | sound-modem links only, while such a link is on (nothing recorded or stored) |
| Camera | scanning QR contact cards / invites (`host.qr.scan`) |
| Location (While Using) | sharing your position in SOS / check-ins (GPS works without network) |
| Notifications | incoming messages while in the background, SOS alerts (asked the first time one is needed) |

Background modes: `bluetooth-central` + `bluetooth-peripheral` (BLE links keep working when the screen
is off, with state restoration), `audio` (only while a sound link is running — it is actively
listening/transmitting), `fetch` (`BGAppRefreshTask` every ≥15 min to flush store-and-forward).

What nearby devices see (privacy defaults): the app never advertises the iPhone's own name
(`UIDevice.name`, e.g. "<Owner>'s iPhone" on iOS 15). Unless the user sets a name on the link, the
Multipeer peer name and the Bonjour instance name of `tcp_server` are `EmberMesh-XXXX` (4 random hex
digits, generated once per install, `DefaultPeerName`); the `ble_peer` BLE local name is `Ember`; the
phone hub's `/hub.json` name is the user's own profile name or `EmberMesh hub`. The core's
`device_name` is the generic model (`iPhone` / `iPad`).

Storage: `Application Support/EmberMesh/` with `NSFileProtectionCompleteUntilFirstUserAuthentication`
(so Bluetooth wake-ups can read it while locked after first unlock), excluded from iCloud/computer
backups (identity keys never leave the phone — use the identity export for backups).

## Endpoints on iOS

| Kind | Status | Notes |
|---|---|---|
| `ble_peer` | implemented | GATT server (service `8e3b0001-…`, RX write/write-no-response, TX notify) **and** central at the same time; many peers; chunks to each peer's ATT size; per-peer flow control; peers' byte streams merged without mid-frame interleaving (time-gap arbiter, no extra header on the air — a plain byte stream as specified); state restoration and pending reconnects. Duplicate paths (A→B and B→A) are left to the mesh's duplicate filter. |
| `multipeer` | implemented | service `embermesh`; auto-invite with a token tie-break; one reliable message per core write; ≤ 7 peers. |
| `audio` | implemented | AVAudioEngine, `.playAndRecord` + `.measurement` (no AGC/echo cancel), voice processing off, input/output route choice, real hardware rates reported to the core, interruption / route-change / media-reset recovery. |
| `tcp`, `rtl_tcp` | implemented | Network.framework, host:port or Bonjour service name, reconnect with backoff. |
| `tcp_server` | implemented | NWListener, Bonjour `_embermesh._tcp` (TXT `ember=1`), `includePeerToPeer`, ≤16 clients — the entitlement-free LAN path. Scanning `tcp` finds these. |
| `websocket` | implemented | URLSessionWebSocketTask to `ws://hub:8437/ws`, binary frames, pings, reconnect. |
| `udp` | implemented, gated | NWConnectionGroup multicast `239.83.77.69:47474`. Works in the Simulator; on devices it needs Apple's **multicast entitlement** — `host.info` reports it unavailable unless the provisioning profile contains it. |
| `ble` | implemented (P1) | GATT central to one device. Supported services (exactly): **Nordic UART** `6e400001-b5a3-f393-e0a9-e50e24dcca9e` (we write RX `6e400002-…`, device notifies TX `6e400003-…`; KISS TNCs, serial bridges, RNode/REYAX behind a bridge); **BLE KISS API** `00000001-ba2a-46c9-ae49-01b0961f68bb` (Mobilinkd TNC3/TNC4: we write `00000002-…`, TNC notifies `00000003-…`; KISS byte stream, profile `kiss_ble`); **Meshtastic** `6ba1b218-15a8-461f-9fa8-5dcae273eafd` (ToRadio `f75c76d2-…` write, FromNum `ed9da18c-…` notify, FromRadio `2c55e69e-…` read until empty); **custom** service/write/notify UUIDs (byte stream). Meshtastic is adapted here: each FromRadio protobuf is delivered to the core as `0x94 0xC3 len_hi len_lo <protobuf>` and outgoing stream frames are unwrapped to one protobuf per ToRadio write, so the core's `meshtastic` link keeps its default `framing:"stream"` — do **not** set `framing:"ble"` with the iOS endpoint. |
| `loopback` | implemented | echoes bytes; for testing. |
| `serial` | unsupported | iOS gives apps no USB-serial access → use a BLE TNC/serial bridge or the audio path. |
| `bt_spp` | unsupported | Classic Bluetooth SPP is MFi-only on iOS → use BLE TNCs. |
| `wifi_direct` | unsupported | not on iOS → `multipeer` (Apple) / `ble_peer` (Android). |
| `ws_server` (hub) | implemented | host library Hub embedded (see "Hub on the phone"); port 8437; `raw` link. |
| `webrtc` | unsupported | would need a WebRTC stack. |

Services: `host.location.get/watch`, `host.qr.scan` (no camera in the Simulator → `unsupported`),
`host.file.save` (share sheet incl. Save to Files/AirDrop) / `host.file.pick`, `host.signal.flash`
(torch; white-screen fallback), `host.signal.tone`, `host.notify`, `host.keep_awake`, `host.hub.*`,
`host.ios.open_settings`. `host.webrtc.*` returns `unsupported`.

**`ble_peer` wire format (matches Android, checked against apps/android `BlePeerEndpoint.kt` /
`BleLogic.kt`):** service `8e3b0001-5d1f-4c6a-9b7e-0d2a6f1c4e10`, RX `…0002` (write / write without
response), TX `…0003` (notify). A plain byte stream: no chunk header, arbitrary chunking; the core
`stream` link does the framing and resynchronises. Both apps split each outgoing core write into
full-size chunks (negotiated ATT payload = MTU−3, clamped 20…512) followed by one short chunk.
Receivers merge several peers into the one link without interleaving (Android: a full-size chunk
holds the floor until a short chunk or idle; iOS: a time-gap arbiter) — neither puts anything extra
on the air, so they interoperate. Only `stream` is offered for `ble_peer` (never `raw`).

## Hardware notes

- **Audio to a radio**: Lightning/USB-C to 3.5 mm adapters with a **TRRS** cable (mic on the sleeve ring)
  work; the headset mic input expects ~10–50 mV, so use an attenuator (e.g. 10 kΩ/100 Ω divider) from
  the radio's speaker output and keep `tx_level` low enough not to overdrive the radio's mic input.
  USB audio interfaces (class compliant, e.g. CM108 dongles, Digirig in audio mode) work over USB-C or
  the Lightning camera adapter. Use VOX on the radio — iOS cannot key PTT on a serial line.
- **Acoustic coupling**: choose output *Loudspeaker*, keep the phone a few cm from the radio's speaker,
  quiet room. Expect only the slowest modem modes to work reliably.
- **Bluetooth headsets** are 8/16 kHz with heavy voice DSP: avoid them for modem audio.
- **BLE TNCs**: anything exposing Nordic UART (many ESP32/nRF52 KISS TNCs, BLE-serial bridges such as
  HM-10 clones via custom UUIDs `FFE0/FFE1/FFE1`), Meshtastic radios (pairing PIN may be requested by iOS).
  Mobilinkd TNC3/TNC4: choose *KISS TNC with the BLE KISS service* (`kiss_ble`), link type `kiss`.

## iOS limitations for off-grid use, and workarounds

- **Background**: iOS suspends apps. BLE links keep running (both roles) and wake the app; Multipeer,
  TCP/UDP/WebSocket stop shortly after the app leaves the screen. Workarounds: keep the app open with
  *keep awake* on a charger for relay duty; a running sound link keeps the app alive legitimately.
- **Background BLE advertising** moves the service UUID to Apple's "overflow area": other iPhones in the
  foreground still find us, **Android phones generally cannot** discover a backgrounded iPhone. Keep
  EmberMesh in the foreground when meeting Android users, or let the Android phone advertise (iOS can
  scan for it in the background).
- **Multicast** needs an Apple-granted entitlement → use `tcp_server` + Bonjour (`tcp` scan) or Multipeer.
- **Multipeer and our Bonjour LAN service share `_embermesh._tcp`** (the Multipeer service name
  `embermesh` maps to it): our scanner filters on TXT `ember=1`.
- No USB serial, no Classic Bluetooth SPP, no Wi-Fi Direct, no hotspot control from apps.
- Free signing expires after 7 days (see above). The **hub web app in Safari** is the universal fallback.

## Launch, logo, accessibility (RPC_API §8)

- **Assets from the vector logo.** `tools/make-logo-assets.py` translates `docs/assets/logo.svg`
  (paths, gradients, stroke opacity) into a hand-written PDF with no producer or date metadata:
  `Assets.xcassets/LaunchLogo.imageset/LaunchLogo.pdf`, with *preserve vector data*. It also renders
  the 1024 px app icons (light; dark on black; tinted grayscale) with `sips` and re-encodes them as
  plain RGB PNGs (IHDR/IDAT/IEND only). Rerun the script whenever the logo changes.
- **Launch screen → web boot animation.** `LaunchScreen.storyboard` shows the logo centred on
  #17110f, its side 30 % of the shorter screen side (min of two constraints, so it holds on every
  iPhone/iPad and orientation). `LaunchPlaceholderView` repeats exactly that over the web view from
  the first moment. The page tells the shell when its first frame is on screen: a document-end
  script posts `emberPainted` two animation frames after `DOMContentLoaded`. The placeholder is then
  removed instantly, with no fade (with or without Reduce Motion), so the animation appears to start
  from the still image. The page receives the logo position in CSS px of its viewport as
  `window.__emberHost.launchLogo = {x, y, size}` and `--launch-logo-x/-y/-size` on `<html>`. The web
  view sits inside the safe area, so this rect is not simply "centre of the viewport".
- **Never covering, never white.** If the start page is missing or fails, the diagnostic page is
  shown. Missing pages are detected through the scheme handler's 404 log, because WebKit gives
  custom-scheme responses to the delegate without a status. After 6 s the placeholder is removed
  whatever happened. The web view, its scroll view, the area under the page and the window are all
  #17110f and non-opaque from the first frame (cold start, rotation, return from background). Nothing
  native keeps animating after the hand-over: the placeholder is static, and its only timer (the 6 s
  reveal) is cancelled at first paint. There are no display links.
- **Status bar** follows the UI's `theme` setting, read from `settings.get` / `settings.set` replies
  on the bridge: `light` → dark text, `dark` / `night` → light text. The launch is always light text
  on #17110f.
- **`host.info.accessibility`** = `{reduce_motion, font_scale, screen_reader, high_contrast,
  bold_text}` from UIAccessibility. `host.accessibility` (same shape) is emitted when a platform
  notification actually changes a value. `font_scale` is the body-text size of the Dynamic Type
  category relative to the default (xS 0.82 … L 1.0 … XXXL 1.35, AX1 1.65 … AX5 3.12). The page is
  not scaled natively (no zoom, no `-apple-system-body`), so the UI applies
  `font_scale × text_scale` once. VoiceOver reads the web content directly; the placeholder is hidden
  from accessibility, and `screenChanged` is posted at the hand-over.
- **`host.haptic {kind: tap|success|warning|error}`** uses UIImpactFeedbackGenerator (light) and
  UINotificationFeedbackGenerator. It is `unsupported` on iPad / Mac (no Taptic Engine). The
  `haptic` feature is advertised only where supported.
- **Measured in the simulator:**
  - The placeholder is removed at first paint 0.45–0.65 s after the controller appears.
  - A 3D CSS animation in our web view ran 180 frames with median 17 ms, p95 17 ms, max 19 ms, and
    no late frames. The simulator draws with the Mac's GPU, so this only shows the shell neither
    throttles nor stalls the page; it is not a device figure.
  - Cold-start screenshots: no white frame. The screenshots are ~400 ms apart, so a single frame
    could be missed; the colours of every native layer are asserted in tests.

## Morse reader: camera and microphone (RPC_API §6)

Both capture paths exist on iOS; `host.info.features` advertises `getusermedia`, `capture_mic_pcm`
and, on devices with a camera, `capture_camera_luma`.

- **getUserMedia in the WKWebView** works on our `embermesh://app` origin (`isSecureContext` is true).
  `WebViewController` implements the iOS 15 `requestMediaCapturePermissionFor` delegate and grants
  camera/microphone to our own origin only (`MediaCapturePolicy`). WebKit's per-page prompt is
  skipped; iOS still shows its one-time Camera / Microphone prompt (usage strings mention the Morse
  reader). `allowsInlineMediaPlayback` keeps the `<video playsinline>` preview in the page.
- **Native sources** (`Services/CaptureService.swift`), the more robust path:
  - `host.capture.start {kind:"camera_luma", session, rect?, roi?, camera?}`: AVCaptureSession at
    ≤ 640×480 and up to 60 fps. Auto exposure/focus meters for 0.8 s, then exposure, focus and white
    balance are **locked**, so auto-exposure "breathing" can't swamp a flashlight. The ROI mean is
    taken from the Y plane of 420f frames with real frame timestamps, and batched about every
    100 ms into `morse.rx.feed {session, times_ms, levels}`. A native preview with the ROI outlined
    is placed over the web view at `rect` (CSS px = web-view points); `host.capture.update` moves it
    or changes the ROI.
  - `host.capture.start {kind:"mic_pcm", session, sample_rate?=8000}`: microphone → mono Int16 at
    the session rate (boxcar decimator) → `morse.rx.feed {session, pcm_b64}` about every 100 ms. If a
    sound link is running it **shares that link's input tap** (`AudioEndpoint.addInputListener`)
    instead of starting a second engine. Otherwise it runs its own AVAudioEngine with the same
    `.playAndRecord` / `.measurement` session. `host.signal.tone` does not switch the session to
    playback while a capture is running. `sample_rate` is an additive iOS param; the contract's
    default is 8000.
  - If the core rejects a feed (session closed after 60 s idle, `morse.rx.stop`), capture stops and
    the shell emits `host.capture.state {kind, running:false, error}`.
- **Sending by light** (RPC_API §10): `host.signal.flash {pattern_ms, mode?}` resolves only when
  the pattern has finished or was stopped, with `{used:"torch"|"screen", stopped, timing:{worst_late_ms,
  mean_late_ms, edges}}`. `host.signal.stop` stops a flash or tone at once, and a second flash/tone
  stops the running one. Limits: ≤ 4,096 entries of 10 ms – 60 s, ≤ 10 min in total, otherwise
  `bad_params`; an empty pattern is `bad_params` too. `host.info.signal = {torch:"yes"|"no",
  screen:true}`, with the `torch` feature when "yes". iOS never reports "ask".
  - **Torch present:** `auto`/`torch` use it.
  - **No torch** (simulator, iPads without a flash): `auto` → screen, `torch` → `unsupported`.
  - **Camera in use by this app** (QR scanner or Morse camera reader): `auto` → screen, `torch` →
    `busy`, and the torch is never touched.
  - **Torch refused by the system** (overheated, lock failed): `auto` → screen, `torch` → `busy` /
    `permission_denied`.
  - **Torch control:** `AVCaptureDevice.lockForConfiguration` + `torchMode`, held for the whole
    pattern. No camera authorisation is requested, and no capture session is started or read. Apple
    documents the authorisation requirement for capture sessions (`AVCaptureDeviceInput`), not for
    torch control; flashlight apps work without the camera permission. Only a real iPhone can
    confirm that no prompt appears on current iOS.
  - **Screen fallback:** a full-window cover, white for on, black for off, at full brightness with
    the idle timer held off. Brightness and idle-timer state are restored exactly afterwards. A tap on
    the cover stops the flash, because the UI's Stop button is underneath it.
  - **The light always goes off:** at pattern end, on stop, on error, on willResignActive,
    background and screen lock (protected data becoming unavailable), and when the page reloads,
    navigates away, its web content process dies, or the web view controller goes away.
  - **Nothing about the pattern or text is logged or stored.**
  - **Timing:** edge *k* is due at start + Σ previous durations on the monotonic clock, on a
    dedicated thread. Waits are interruptible until 3 ms before the deadline, then finished with
    `mach_wait_until`. Measured in the simulator at 20 and 5 WPM: worst 2.1 ms late, mean
    1.1–1.6 ms. A late edge shortens the next interval, so lateness never accumulates; this is
    tested with a virtual clock. The torch LED's own switching latency, and screen edges (shown at
    the next display refresh, ≤ 16.7 ms), are not included.

## Connecting a radio (sound link)

- Routes: TRRS headset-jack adapter (`wired`), USB-C / Lightning audio interfaces (`usb`, class
  compliant), the built-in mic + loudspeaker for acoustic coupling. Bluetooth headsets are **not
  suitable** for data: HFP is 8/16 kHz mono with voice processing (AGC, noise suppression, echo
  cancelling) that destroys AFSK/PSK tones, adds 100–200 ms of variable latency, and A2DP has no
  microphone at all. The option is still listed, labelled as unsuitable.
- **PTT:** iPhone/iPad apps have no serial RTS/DTR or GPIO lines, so a cable cannot key a radio.
  The audio endpoint offers `ptt: vox | manual`, and `host.info.audio` says
  `{ptt:["vox","manual"], serial_ptt:false, presets_unsupported:{handheld_ptt:"…"}}`, so the wizard
  can hide `handheld_ptt` and explain. Use `handheld_vox` (VOX on the radio; the preset's 300 ms
  lead-in covers the VOX attack), `acoustic` (operator holds PTT) or `sound_card` (an interface with
  its own VOX). Radios with a Bluetooth KISS TNC use the `ble` endpoint (`nus` or `kiss_ble`
  profile) with link type `kiss`.
- Level metering comes from the core link status (`rx_level_dbfs`, `rx_peak_dbfs`, `level_hint`,
  `dcd`, `snr_db`); the endpoint status adds the actual input/output route, rates and latencies.

## Guide library at scale

A 46.8 MB pack (5,202 guides; the real corpus replicated 6×, built with `tools/guidepack`) was
tested through the same mmap + `ember_guides_load(copy=0)` path. Load took 25 ms; `phys_footprint`
went 21.7 → 21.7 MB after load and 21.8 MB after 10 searches + 1 get. Searches averaged 39 ms (max
53 ms), `guides.get` 0.4 ms. App cold start with it bundled (Release, simulator): guides loaded at
296 ms, page loaded at 882 ms (vs 255 / 745 ms with an 8.8 MB pack). The app is 68 MB with it. The
hub serves the web app's own pack (`/data/guides.pack` from dist/web); it is hard-linked to the
app's pack only when they are identical, i.e. when dist/web was built from the same pack. Run with
`TEST_RUNNER_EMBER_BIG_PACK=<pack> xcodebuild … -only-testing:EmberMeshTests/GuidePackScaleTests test`.

## Proven vs compiled-only (real core, verified in the iOS simulator)

Proven in the simulator (`xcodebuild … test`, plus the opt-in UI smoke test):
- Real core, all modules, plus host library hub, built for simulator (arm64+x86_64) and device (arm64, unsigned).
- Fresh identity (`name:""`) → profile → persistence across restart / app relaunch → passphrase,
  locked start, wrong passphrase `denied`, unlock (re-emits `node.ready`), runtime lock.
- Link persistence in the core: create → restart → `node.ready` → reopened with the same id and `up`;
  locked start opens nothing until unlock; `host.link.remove` deletes it.
- Two nodes, back-to-back endpoints, links created with `host.link.create`: Public both ways, DM
  end-to-end encrypted with `delivered` receipt.
- Phone hub: `host.hub.start` → `/hub.json`, `/` (dist/web), `/wasm/ember.wasm`; a guest node joins over
  the `websocket` endpoint; Public + DM with receipt through the hub; the hub comes back after restart; stop.
- Against the desktop daemon `emberd` on the same Mac (`TEST_RUNNER_EMBERD_PORT`, `TEST_RUNNER_EMBERD_TCP_PORT`):
  `websocket` (`ws://127.0.0.1:<port>/ws`), `tcp` (to an emberd `tcp_server` link) and `udp`
  multicast: Public both ways and DMs both ways with delivery receipts at both ends, checked
  through emberd's `POST /rpc`.
- Audio: a simulated two-way radio channel between two nodes' AudioEndpoints: each side's modem output
  (pulled by NodeRunner from `ember_audio_tx` into the endpoint ring) is rendered by the same render
  function the `AVAudioSourceNode` uses (A 48 kHz, B 44.1 kHz Float32), resampled and fed to the other
  side's input-tap function (Float32 → Int16 → `ember_audio_rx`); B decodes A's AFSK1200 Public message
  (10/10 runs, ~3–5 s of channel time). Keys are exchanged first and messages are sent on an idle channel:
  otherwise B holds an unknown sender's signed message until WHOIS → announce completes, and a collision
  with a keyed half-duplex receiver is only repaired by sync (120 s gap below 2400 bd) — mesh behaviour,
  not the audio path. `AudioBurstDiagnostics` (opt-in, `TEST_RUNNER_EMBER_AUDIO_BURST=1`) checks
  multi-frame key-ups. **Not exercised:** a running AVAudioEngine, AVAudioSession routing, real
  speaker/microphone, interruptions and route changes.
- Guides: bundled `guides.pack` memory-mapped with `copy=0` (verified with an 864-guide pack in 20
  categories): search + open.
- UI smoke (real UI in the app's WKWebView): first-run setup, send in Public, guide search and
  open, screenshots, and relaunch shows the persisted profile and message.

Morse/radio (simulator):
- getUserMedia on `embermesh://`: API present, both requests go through our permission delegate,
  video (WebKit's simulated camera) and audio return live tracks.
- Native camera path: synthetic 420f `CMSampleBuffer`s through `CameraLumaCapture.process`.
- Native mic path: synthetic 48 kHz buffers through `MicPcmCapture.process`, including sharing a
  sound link's input.
- Flash scheduler timing and the screen-flash fallback.
- `handheld_vox` preset over the two-way channel with a 300 ms VOX attack model.
- Decoding through the real `morse.rx`: `testCameraLumaDecodesWithRealCore` and
  `testMicPcmDecodesWithRealCore` feed synthetic camera frames and microphone audio through those
  paths into the core's decoder (they skip with a message on a core built without `morse.rx`).
- **Not exercised:** a real camera (AVCaptureSession, exposure lock, preview layer), the torch, real
  microphone capture through AVAudioEngine, real radios / VOX / cables.

Compiled only (needs hardware): `ble_peer`, `ble` (NUS / BLE KISS / Meshtastic), `multipeer`, real
audio I/O, `tcp_server` Bonjour discovery between devices, Personal Hotspot hub on a phone, background modes.
