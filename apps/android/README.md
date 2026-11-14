# EmberMesh for Android

Kotlin shell around the shared C++ core (`core/`, built with the NDK) and the shared web UI (`ui/`,
shown in a WebView). The app has no Google Play Services and no AndroidX at runtime. Its only
third-party library is ZXing core, vendored at `app/libs/zxing-core-3.5.3.jar` under Apache-2.0 (see
`app/libs/ZXING-LICENSE.txt`). Package `org.embermesh.app`. It runs on Android 5.0 (API 21) and later, and
targets API 37.

## Build

What you need: the Android SDK (platform 37, build-tools 36+, NDK 27.1.12297006, SDK CMake 3.22.1) and a
JDK 17+ (the one bundled with Android Studio is fine). The Gradle wrapper is committed.

```sh
cd apps/android
export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"   # macOS example
echo "sdk.dir=$HOME/Library/Android/sdk" > local.properties                       # or set ANDROID_HOME
./gradlew :app:assembleDebug            # app/build/outputs/apk/debug/app-debug.apk (arm64-v8a, armeabi-v7a, x86_64)
./gradlew :app:assembleRelease          # minified, signed with the debug key so it installs anywhere
./gradlew --offline ...                 # works once the Gradle cache is populated
```

Build properties (pass with `-P`):

| Property | Default | Meaning |
|---|---|---|
| `emberModules` | `all` | Core modules to compile. Use `-PemberModules=capi` while other modules are broken. |
| `emberUiDir` | `<repo>/ui` | Web UI copied to `assets/ui/`. If it has no `index.html`, the app opens the diagnostics page. |
| `emberGuidesPack` | first existing file among `build/guides.pack`, `dist/guides.pack`, `guides/guides.pack`, `build/guides/guides.pack` | Copied to `assets/guides.pack`. It is stored uncompressed and memory-mapped (`ember_guides_load(copy=0)`). |
| `emberWebDir` | `<repo>/dist/web` | The web build (WASM core + UI) the phone hub serves to guests, copied to `assets/web/` with only what differs from `assets/ui` (WASM, its `index.html`, PWA files; the hub rebuilds the web root as ui + web), without its `data/guides.pack` (the hub serves the APK's own pack there) and without `embermesh.html`. Absent → the hub serves `assets/ui` (no WASM: guests see the UI but cannot run a node). Build it first with `./build.sh web`. |
| `emberHub` | `true` | Embed the host library's hub (`host/`) for `host.hub.*` and the `ws_server` endpoint. |
| `emberRoot` | `../..` | Repository root. |

The native library is built by `app/src/main/cpp/CMakeLists.txt`. It calls `add_subdirectory()` on the
repository root CMake (only `ember_core` for Android) and on `host/` with `EMBER_HOST_AUDIO=OFF`,
`EMBER_HOST_SERIAL=OFF`, `EMBER_HOST_TESTS=OFF`, and links `libember_jni.so` (16 KB page aligned,
`-Wall -Wextra -Wshadow -Werror` for our sources). The hub uses the host library's hub-only C API
(`host/include/ember_hub.h`), which leaves the node to the Kotlin runner; the host library itself
handles minSdk 21 (no `getifaddrs` before API 24, no `getrandom` before API 28).

APK sizes measured (all three ABIs, web build for the hub): with a 41 MB / 4,999-guide pack,
release 47.1 MB; with an 8.8 MB / 864-guide pack, release 13.0 MB and debug 16.1 MB (clean build).
The size grows with the guide pack you bundle. Contents (compressed): `guides.pack` (stored uncompressed so it can be
mapped; 4-byte aligned in the APK, which is all the core's byte-wise reader needs), `libember_jni.so` 1.3–1.8 MB per ABI (compressed, extracted at install, because the APK is
passed phone to phone), dex 1.4 MB debug / 0.2 MB release, WASM core for hub guests 0.6 MB, UI
0.3 MB. `ui/tests` is not shipped. An incremental debug build can be several MB larger than these
figures (AGP's incremental packaging leaves unused space in debug APKs). For passing the app on, use
the release APK (`./gradlew :app:assembleRelease`; see "Signing").

Native libraries are built with `-ffile-prefix-map` so no absolute path of the building machine ends
up in them (checked with `strings` on the unstripped library); release APKs embed no VCS state.

## Test

```sh
./gradlew :app:testDebugUnitTest            # JVM tests: USB serial encodings, BLE chunking/arbiter, WebSocket
                                            # codec, bridge routing, LinkManager (core-owned links, node.ready sync,
                                            # locked start, PTT targets + release delay, endpoint reconfigure,
                                            # link types from links.types), assets, ...
$ANDROID_HOME/emulator/emulator -avd <avd> -no-snapshot-save &   # or plug in a phone with USB debugging
./gradlew :app:connectedDebugAndroidTest    # instrumented tests against the REAL core (see below)
./gradlew :app:installDebug :app:installDebugAndroidTest
scripts/e2e_emberd.py --transports ws,tcp   # app <-> desktop emberd over real sockets (see below)
scripts/measure_idle.py --seconds 60        # CPU time and wakeups of the running app, per thread
```

Note: `connectedDebugAndroidTest` uninstalls the app afterwards (its data is gone).

### What is proven on an API 37 arm64 emulator (Android 17) and how

| Claim | Evidence |
|---|---|
| Real core over JNI: fresh identity has `name:""`, `node.set_profile`, Unicode across JNI, errors (`not_implemented`, `unsupported` for `host.*` in the core, `locked`, `denied`) | `RealCoreTest`, `NativeCoreTest` |
| Link persistence is the core's: a link made with `host.link.create` comes back after a restart from `links.list` → endpoint reopened → `links.endpoint_state`; nothing in `filesDir` | `RealCoreTest.identityPersistsAcrossProcessRestart` (also run as two separate `am instrument` processes with `-e persist_phase write` / `check`) |
| Locked start: passphrase + `security.lock`; a node restarted on the encrypted store is locked with no identity and no links; wrong passphrase → `denied`; unlock → `node.ready` → links reopened and `up` | `RealCoreTest.freshIdentityProfileAndLockUnlockWithLinkRestore` |
| Two real nodes in one process, wired by a virtual datagram cable through LinkManager/HostRouter: encrypted DM delivered, `message.status` delivered, Public channel back | `RealCoreTest.twoNodesExchangeDmWithDeliveryReceipt` |
| Guides: an 8.8 MB pack is mapped from the APK (`ember_guides_load(copy=0)`), 20 categories, 864 guides, search and `guides.get` work | `RealCoreTest.guidesMemoryMappedFromApk`; manual UI check |
| `host.info`: link types per endpoint kind come from the core's `links.types` (tcp has meshtastic/rnode/lora_at/rtl_tcp, serial has lora_at, ble has meshtastic/rnode, ble_peer is `stream` only); `actions`; `hub` feature | `RealCoreTest.hostInfoLinkTypesFromCore` |
| Phone hub: `host.hub.start` serves the web build (WASM verified by magic number), `/data/guides.pack`, `/hub.json`, the APK at `/download/EmberMesh.apk`; a guest node joins `ws://…/ws` and a DM to the phone's node is delivered; `host.hub.stop` | `HubTest`; manual check from a desktop through `adb forward`, with desktop `emberd` as a guest exchanging Public messages with the phone |
| Audio modem through the real `AudioEndpoint` RX/TX loops: node A's AFSK1200 TX audio, carried in real time by a virtual cable, is decoded by node B (SNR ≈ 20 dB, 0 bad frames); PTT keyed on a serial link's RTS and released after the output latency | `AudioModemTest.afskFrameCrossesTheAudioEndpointLoops` |
| Real `AudioRecord`/`AudioTrack` on the device open, capture (the emulator's mic is silent) and play a modem transmission | `AudioModemTest.hardwareAudioPathOpensCapturesAndPlays` |
| Cross-platform over real sockets with the desktop daemon: Public both ways, DM both ways, delivery receipt — over `websocket` (emberd hub `/ws`) and `tcp` (emberd `tcp_server`) | `scripts/e2e_emberd.py` + `EmberdInteropTest` |
| UDP multicast: app → emberd works through the emulator NAT; emberd → app does not (the emulator does not deliver inbound multicast) — untested between two real phones | `e2e_emberd.py --transports udp` |
| UI: first-run setup, Public message, guides, hub start/URLs/stop driven through the WebView | manual UI check (no crash/ANR in logcat) |
| Morse receive inputs: `getUserMedia` in the WebView (own origin only, after the runtime permission) delivers camera frames the page samples for brightness (≈13 fps on the emulator's static virtual scene) and a live 48 kHz microphone track | `BridgeInstrumentedTest.webViewCameraAndMicCapture` |
| Native capture sources `capture_camera_luma` (Camera2, 640×480 YUV, ≈30 fps, exposure + white balance locked after 1.5 s, torch off, ROI luma from the Y plane, sensor timestamps) and `capture_mic_pcm` (AudioRecord, UNPROCESSED where available, resampled to the session rate) run on the emulator and call `morse.rx.feed` | `MorseCaptureTest.realCameraDeliversLumaFrames`, `realMicDeliversPcm` |
| The core decodes a generated blinking light (SOS SOS, 10 WPM, 30 fps, ±2 ms frame jitter, exposure noise → "SOS SOS", 9.7 WPM estimated) and a keyed 700 Hz tone at 48 kHz (CQ CQ, 15 WPM → "CQ CQ", resampled to 8 kHz) fed through those sources' own frame/PCM paths | `MorseCaptureTest.syntheticBlinkingLightDecodes`, `syntheticToneDecodes` |
| Morse microphone capture while an audio-modem link holds the microphone: both keep receiving audio | `MorseCaptureTest.micCaptureCoexistsWithAudioLink` |
| Morse sending by light (RPC_API §10): `host.signal.flash` resolves with `{used, stopped, timing}`; modes auto/torch/screen; `host.signal.stop` ends a flash in 1–3 ms with the torch off; a second flash stops the first; limits → `bad_params`; the screen flash restores brightness, keep-screen-on and the window foreground exactly, also when the app goes to the background mid-flash; with this app's camera reader holding the camera, `auto` uses the screen and `torch` returns `busy`. On the emulator's virtual torch (its only flash unit is on the front camera, so the tests opt in to it; production requires a back camera): mean edge lateness 0.2–0.5 ms, worst 2–10 ms, drift ≤ 1.3 ms over "PARIS PARIS" at 5/12/20 WPM, `setTorchMode` ≈ 1 ms per call. Tone sending is sample-accurate | `LightSendingTest`, `MorseCaptureTest.flashKeyingTiming`, `LightSenderTest` (JVM: drift, stop, limits, modes, camera busy) |
| Handheld on VOX through the virtual cable (VOX model: envelope detector, the first 250 ms of every transmission clipped, 400 ms hang) with the core's `handheld_vox` preset (400 ms VOX tone, 600 ms lead-in): frames decode | `AudioModemTest.handheldVoxPresetSurvivesVoxClipping` |
| Big guide packs mapped from the APK, never copied: the real 41 MB pack (4999 guides) — release APK 47.1 MB, cold start 0.50–0.66 s, native heap 2.4 MB, `guides.categories` 0.5 ms, search 38 ms; a synthetic 49.9 MB pack (5316 guides) — release APK 54 MB, cold start 0.80–0.95 s, search 48 ms. The hub copies the pack once into its web root for guests (as much extra storage as the pack). Passing a 47 MB APK over Bluetooth takes minutes; the hub's `/download/EmberMesh.apk` over Wi-Fi is the fast way | manual check + `RealCoreTest.guidesMemoryMappedFromApk` with `-PemberGuidesPack=<50 MB pack>` |

Not exercised anywhere: real BLE (central/peripheral/peer, Meshtastic and Mobilinkd BLE profiles),
USB serial chips, USB sound cards (AIOC, Digirig), Bluetooth SPP, Wi-Fi Direct, Bluetooth SCO, real
radios, a real camera pointed at a real lamp, the native camera preview overlay's placement, a phone
hotspot. These are compiled and unit-tested only
(protocol encoders, chunking, arbiters).

Install and run by hand: `adb install -r app/build/outputs/apk/debug/app-debug.apk`, then
`adb shell am start -n org.embermesh.app/.ui.MainActivity`.

To open the bridge diagnostics page, run
`adb shell am start -a org.embermesh.app.DIAGNOSTICS -n org.embermesh.app/.ui.MainActivity`, or
long-press the launcher icon and choose **Diagnostics**. The page calls every `host.*` service and lets
you create test links. Its source is `app/src/main/assets/ember/bridge-test.html`.

## Architecture

| Piece | File(s) |
|---|---|
| JNI binding for all of `ember.h` | `cpp/ember_jni.cpp`, `core/EmberNative.kt`, `core/EmberNode.kt`. Converts strings to real UTF-8; `poll` returns a byte header plus payload; guides are mapped from an asset or a file. |
| Node thread | `core/NodeRunner.kt`. Calls tick, then drains the outbox. Sleeps for `ember_next_wakeup_ms`, but uses about 25 ms for 3 s after any activity, and at most 20 ms while an audio link is open. |
| Runtime singleton | `EmberRuntime.kt`. Node and identity are stored in `filesDir/ember`; entropy comes from `SecureRandom`. Also handles events, notifications, wake locks and USB hot-plug. |
| Foreground service | `service/MeshService.kt`. Service type `connectedDevice`, plus `microphone` / `location` while those are in use. Persistent notification with **Share app** and **Quit** actions. Restarts after being killed (`START_STICKY`). |
| WebView host | `ui/MainActivity.kt` and `ui/AssetServer.kt`. Assets are served at `https://appassets.androidplatform.net/`, which is a secure context; every other URL gets a 403. File and content access are off. |
| Bridge (RPC_API §5) | `bridge/BridgeDispatcher.kt` and `bridge/EmberBridge.kt`. `window.EmberNative.postMessage` goes to the core or the host router; replies and events return through `window.__emberBridge.onReply/onEvent`. |
| Host router (RPC_API §3) | `host/HostRouter.kt`, `host/LinkManager.kt`, `host/EndpointRegistry.kt` |
| Phone hub | `hub/PhoneHub.kt`, `hub/HubEndpoint.kt`, `hub/HubNative.kt`, `cpp/hub_jni.cpp` (host library `Hub`) |
| Endpoints | `endpoint/<kind>/…`, registered in `endpoint/Endpoints.kt` |
| Device services | `services/AndroidDeviceServices.kt`, `QrScanActivity.kt`, `SharedFileProvider.kt` |

How links work (RPC_API "Link persistence", normative):

- The core is the only store of links. `host.link.create` opens the endpoint and calls `links.add`
  with the opaque descriptor `endpoint:{kind, config, autostart?}`; the app keeps no list of its own
  (a stray `filesDir/links.json` is deleted on start, and the app never writes `links_autostart`).
- On EVERY `node.ready` (start, unlock, duress unlock, wipe) the runtime calls `links.types` (link
  types per endpoint kind) and `links.list`, reopens every endpoint whose kind it provides and reports
  `links.endpoint_state`. Endpoints whose link vanished are closed. A node that starts locked lists no
  links; the `node.ready` after the unlock brings them up.
- A link that drops reconnects with exponential backoff (2 s up to 60 s). Plugging in a USB device
  retries serial links immediately. Links that could not open because a runtime permission is missing
  are retried (with the permission prompt) when the UI comes to the front.
- `host.link.configure {id, name?, config?, endpoint?:{config}}` edits a link after creation: link
  config and name go to `links.configure` (same id). A changed endpoint config is applied in place if
  the core stores the new descriptor (endpoints that can, e.g. audio TX level/PTT, change live);
  the current core does not, so the link is recreated (new id, result `replaced:<old id>`). The UI's
  own remove + create works too.

### Android extensions to RPC_API

These are additive; the shared UI may ignore them.

- `host.info.actions` (UI convention, RPC_API §3): `share_app`, `save_apk` (`host.app.share`),
  `start_on_boot` (`host.android.settings.set`, title shows the current state), `battery_optimization`
  (`host.android.battery_optimization`).
- `host.hub.start {port?}` / `host.hub.stop` / `host.hub.status` → `{running, urls, clients, port,
  web_root:"web"|"ui-only"|"builtin", link_id?, error?}`; event `host.hub.clients`. Feature `hub`.
- `host.link.configure` (above). Feature `link_configure`.
- `host.capture.start/update/stop` (RPC_API §6) for `camera_luma` and `mic_pcm` (features
  `capture_camera_luma`, `capture_mic_pcm`); extras: `sample_rate` for `mic_pcm` (the session's rate,
  default 8000) and `host.capture.status` (frames, fps, exposure lock, level, samples, feeds, last
  error). The camera preview is a native view over the page at the CSS-pixel `rect`.
- `host.signal.stats` → `{flash: timing}` of the last `host.signal.flash` (the same object as its
  result's `timing`: the standard `max_late_ms, mean_late_ms, edges` plus `abs_p95_ms, abs_max_ms,
  call_mean_ms, call_max_ms, total_error_ms, light`).
- `host.endpoints.scan {kind:"audio"}` ("Connect a radio"): the phone's likely radio interfaces with
  complete configs — `{id, name, detail, config:{input_device, output_device, ptt, ptt_line?},
  link:{type:"audio", config:{preset}}}`: AIOC (one composite USB device: sound card + CDC serial,
  PTT on DTR), Digirig-style USB sound card + CP2102 (PTT on RTS), other USB sound card + serial
  adapter (RTS), USB sound card alone or headset-jack cable (VOX, `handheld_vox`), and the built-in
  speaker/microphone for acoustic coupling (`acoustic`).

- `host.app.share {mode:"share"|"save"}`: passes the installed APK on through the share sheet
  (Bluetooth, Quick Share, file managers) or saves it with SAF. Feature string: `share_app`.
- `host.file.save` accepts `mode:"share"` to open the share sheet instead of the SAF "save as" dialog.
- `host.android.settings.get/set`: `start_on_boot`, `run_in_background`, `keep_cpu_awake`
  (`auto|always|never`), `notify_messages`. The result also reports `ignoring_battery_optimizations` and
  the guides status.
- `host.android.battery_optimization`: opens the system screen that exempts the app from battery
  optimisation.
- `host.link.status`: per-link endpoint status (connected, error, bytes, driver details).
- Events:
  - `host.open_uri {uri}`: an `ember:` link was opened.
  - `host.open {target}`: a notification was tapped.
  - `host.app.visible`
- Back button: if the page defines `window.__emberBridge.onBack()` and it returns `true`, the page has
  handled back itself. Otherwise the WebView goes back in its history, or the app moves to the
  background while the node keeps running.

## Endpoints

| Kind | Status | Notes |
|---|---|---|
| `ble_peer` | done, not yet tried between real phones | GATT server and advertiser on service `8e3b0001-…`, running at the same time as a scanner/central. Supports many peers, MTU 517, and write-without-response in chunks of (MTU − 3). A per-peer stream arbiter stops bytes from different peers interleaving. A token in the advert breaks the tie when two phones would connect to each other. |
| `udp` | done | Multicast `239.83.77.69:47474` joined on every IPv4 interface, including the hotspot interface (`ap0`/`swlan0`/`wlan1`) and tethering. Holds a MulticastLock plus a Wi-Fi lock. Optional unicast peers. |
| `audio` | modem path emulator-tested, not yet tried with a real radio | Captures with `UNPROCESSED` (or `VOICE_RECOGNITION`) and turns AGC, noise suppression and echo cancellation off. Low-latency AudioTrack. You can choose the input and output device: jack, USB, Bluetooth SCO or built-in. Reports the actual sample rate and output latency. Handles audio focus. PTT (`ptt`): a picker of "VOX / none", every open serial connection (`link:<id>`, keys its RTS or DTR — `ptt_line`, `ptt_invert`) and every attached USB serial adapter (`usb:<device>`, opened only for PTT). PTT release is held for the output latency + 40 ms after the core's `link.ptt on:false`. TX level and PTT change live. |
| `serial` | done, not yet tried with real chips | Our own drivers for CDC-ACM, CP210x, CH340/CH341, FTDI (including H-type chips) and PL2303 (H/HX/TA/TB/HXN). Handles the USB permission prompt and hot-plug. Keys PTT with RTS or DTR on `link.ptt`. |
| `ble` | done, not yet tried on hardware | Central for Nordic-UART-style peripherals (UUIDs can be changed), `profile:"kiss_ble"` (Mobilinkd TNC3/TNC4 BLE KISS API `00000001-ba2a-…`), or `profile:"meshtastic"`: the Meshtastic Bluetooth API (service `6ba1b218-…`; ToRadio write, FromNum notify, FromRadio read until empty), adapted like the iOS app: every FromRadio protobuf reaches the core as `94 C3 len <protobuf>` and the core's stream frames are written one protobuf per ToRadio write, so the `meshtastic` link keeps `framing:"stream"`. Scan marks Meshtastic radios and pre-selects the profile. |
| `bt_spp` | done, not yet tried on hardware | RFCOMM SPP. Tries a secure socket, then an insecure one, then channel 1. Scanning lists paired devices plus devices found by discovery. |
| `tcp`, `tcp_server`, `websocket` | done | Our own RFC 6455 client (`ws://` and `wss://`). The TCP server advertises `_embermesh._tcp`. NSD scanning works for `tcp` and `websocket`. |
| `wifi_direct` | partial | Group owner listens on TCP 47476 and the client connects to it. Only lightly tested; the emulator has no P2P. |
| `loopback` | done | Echoes frames back; used for tests. |
| `ws_server` (hub) | done, emulator-tested | The host library's HTTP + WebSocket hub (`host/include/ember_hub.h`, the hub-only C API) behind `cpp/hub_jni.cpp`, on its own native loop thread, as one datagram endpoint under a core `raw` link (all guests share it; the mesh engine relays). Port 8437, bound to 0.0.0.0, so it is reachable on the phone's own hotspot and on any Wi-Fi it joined (hotspot addresses listed first). Serves `assets/web` (extracted once per app version to `filesDir/hub-web`), `/hub.json`, `/download/EmberMesh.apk`; `/rpc` remote control is refused on phones. `ember_host.h` is not used: it must be the only `ember_poll` consumer, but the Kotlin node runner drives the node. |
| `webrtc`, `multipeer`, `rtl_tcp` | not available | The `host.webrtc.*` methods return `unsupported`. |

Device services:

- `host.location.*`: GPS through LocationManager; works with no network.
- `host.qr.scan`: camera plus ZXing.
- `host.file.save/pick`: Storage Access Framework or the share sheet; picks are limited to 32 MB.
- `host.signal.flash` / `host.signal.stop` (RPC_API §10, Morse sending by light):
  - **Torch** through `CameraManager.setTorchMode` only — no CAMERA permission, no camera session —
    on the back camera's flash unit (`host.info.signal.torch` is `"yes"` only then, else `"no"`;
    Android never needs `"ask"`). While any camera is open (this app's QR scanner or Morse camera
    reader, or another app; seen through the torch callback) the torch is busy: `auto` uses the
    screen, `torch` returns `busy`. The app never competes for the camera.
  - **Screen**: a white full-window overlay at maximum brightness, only while the app is in front;
    brightness, keep-screen-on and the window foreground are saved and restored exactly. The light
    hides the page's own Stop button, so a tap anywhere stops a screen flash (the tap is not passed
    to the page).
  - **Result**: `{used, stopped, timing:{max_late_ms, mean_late_ms, edges, …}, fallback?}` — `fallback`
    is `"no_torch"` or `"busy"` when `auto` used the screen although a torch was wanted.
  - **Timing**: edges are deadlines on `SystemClock.elapsedRealtimeNanos()` from the pattern start, on
    a dedicated URGENT_DISPLAY thread (a late edge shortens the next interval, no drift); each switch
    call starts early by half its measured duration. `timing` reports mean and worst lateness.
  - **The light always goes off**: at the end, on `host.signal.stop`, a new flash or tone, an error,
    `onStop` (background, screen off, lock), the page reloading or its renderer dying, and runtime
    shutdown. Nothing about the pattern is logged or stored; page console output reaches logcat only
    in debuggable builds.
  - Real-phone torch latency (`setTorchMode` takes about 1 ms on the emulator) and the lamp's own
    rise time are not measured.
- `host.signal.tone`: sine tone with ramps.
- `host.notify`, `host.keep_awake`.

## Sideloading (no app store)

1. On the phone that has the app, open **Share app**. It is in the persistent notification, under a
   long-press on the launcher icon, and available to the UI as `host.app.share`. Send the APK by
   Bluetooth, Quick Share / Nearby Share, a USB stick (save it with `mode:"save"`), or a laptop.
2. On the receiving phone, open the `.apk` from a file manager and allow **Install unknown apps** for
   that app when Android asks. On Android 7 and older, enable *Settings → Security → Unknown sources*.
3. From a computer: `adb install EmberMesh.apk`.

### Signing

Copies made from one build (or one builder's key) carry the same signature, so updates install over
older copies made from that key. Two copies built on different machines do NOT: Android refuses
to update an app signed with another key (uninstall first, which deletes its data).

**Without a release key** (the default) the release build is signed with the building machine's own
debug key (`~/.android/debug.keystore`, generated by the Android tools on each computer). That key
identifies the builder's machine and is not secret-grade.

**With a release key** the release build is signed with it when all four settings are present, read
from the environment first and from Gradle properties of the same name second (for example in
`~/.gradle/gradle.properties`, never in the repository):

| Setting | Value |
|---|---|
| `EMBER_ANDROID_KEYSTORE` | path to a PKCS12 (or JKS) keystore outside the repository |
| `EMBER_ANDROID_KEYSTORE_PASSWORD` | keystore password |
| `EMBER_ANDROID_KEY_ALIAS` | alias of the key in it |
| `EMBER_ANDROID_KEY_PASSWORD` | key password (for PKCS12 usually the same as the keystore password) |

Gradle prints one line saying which key signs release builds. If the settings are present but wrong
(no such file, wrong password, unknown alias) the build fails; it never falls back to the debug key.
If only some are set, it says which are missing and uses the debug key. Signature schemes: v1 (for
Android 5–6), v2 and v3.

Create a key once and keep it (and a backup) outside the repository — losing it means future builds
can no longer update installed copies:

```sh
keytool -genkeypair -storetype PKCS12 -keystore /path/to/release.p12 -alias embermesh \
  -keyalg RSA -keysize 4096 -validity 10000 -dname "CN=EmberMesh"
```

Build a release APK with it:

```sh
export EMBER_ANDROID_KEYSTORE=/path/to/release.p12
export EMBER_ANDROID_KEY_ALIAS=embermesh
read -rs EMBER_ANDROID_KEYSTORE_PASSWORD && export EMBER_ANDROID_KEYSTORE_PASSWORD
export EMBER_ANDROID_KEY_PASSWORD="$EMBER_ANDROID_KEYSTORE_PASSWORD"
./gradlew :app:assembleRelease
$ANDROID_HOME/build-tools/<version>/apksigner verify --print-certs app/build/outputs/apk/release/app-release.apk
```

The CI release workflow does the same with repository secrets (see `docs/CI.md`, "Release signing").
No keystore is part of this repository and none must ever be committed (`.gitignore` and
`tools/check-publishable.sh` refuse `*.jks`, `*.p12`, `*.keystore` and `keystore.properties`).

## Permissions and why

| Permission | Why |
|---|---|
| `INTERNET` | All sockets: UDP/TCP/WebSocket links, the hub (no internet is needed; this is the socket permission) |
| `ACCESS_NETWORK_STATE`, `ACCESS_WIFI_STATE` | Pick interfaces for multicast and list hub addresses (hotspot first) |
| `CHANGE_WIFI_STATE` | Wi-Fi Direct and Wi-Fi locks for LAN links |
| `CHANGE_WIFI_MULTICAST_STATE` | Multicast lock: LAN discovery / UDP mesh `239.83.77.69` |
| `ACCESS_LOCAL_NETWORK` (runtime, Android 17+) | **Every Wi-Fi/LAN link and the hub.** Apps targeting API 37 cannot reach or be reached on LAN addresses without it — connections just time out (on the emulator, TCP/WebSocket to the host fail until it is granted). Requested when a LAN link or the hub is created; links blocked by it are retried with the prompt when the app is opened. |
| `NEARBY_WIFI_DEVICES` (runtime, 13+, `neverForLocation`) | Wi-Fi Direct |
| `BLUETOOTH`, `BLUETOOTH_ADMIN` (≤ Android 11) | Bluetooth on old Android |
| `BLUETOOTH_SCAN` (`neverForLocation`), `BLUETOOTH_CONNECT`, `BLUETOOTH_ADVERTISE` (runtime, 12+) | BLE phone-to-phone mesh, BLE TNCs / Meshtastic, Bluetooth SPP |
| `ACCESS_FINE_LOCATION`, `ACCESS_COARSE_LOCATION` (runtime) | GPS position for SOS and check-ins (only when the user shares it); also needed for Bluetooth / Wi-Fi scans on Android 11 and older |
| `RECORD_AUDIO` (runtime), `MODIFY_AUDIO_SETTINGS` | Audio soft modem, Morse audio receive, Bluetooth SCO routing |
| `CAMERA` (runtime) | QR scanning and Morse light receive only. Sending Morse with the torch uses `setTorchMode`, which needs no permission, and never asks for this one |
| `VIBRATE` | Notification vibration for messages and SOS |
| `POST_NOTIFICATIONS` (runtime, 13+) | New messages, SOS alerts, the "relaying" notification |
| `WAKE_LOCK` | Keep relaying with the screen off while a link needs the CPU |
| `FOREGROUND_SERVICE`, `FOREGROUND_SERVICE_CONNECTED_DEVICE` | The mesh service that keeps links up in the background |
| `FOREGROUND_SERVICE_MICROPHONE` | An audio-modem link in the background |
| `FOREGROUND_SERVICE_LOCATION` | Position sharing in the background. *Store policy: Google Play restricts foreground-service location to approved use cases.* |
| `RECEIVE_BOOT_COMPLETED` | Optional start on boot (`start_on_boot`, off by default) |
| `REQUEST_IGNORE_BATTERY_OPTIMIZATIONS` | Lets the app open the exemption dialog. *Store policy: Google Play allows it only for specific app categories; a store build may need to drop it and only link to the settings screen.* |

**Measured cost** (emulator, app in background, `scripts/measure_idle.py`): cold start to first frame
≈ 0.4–1.1 s (`am start -W` TotalTime; 0.4 s on a fresh install's first run). Idle with no links: the
node thread wakes every ~4–5 s (15 wakeups/min, the core's own idle cadence), ~0.03 % of one core for
the whole process. With the hub running and no guests: 0.28 node wakeups/s, process ≈ 2.4 wakeups/s,
no measurable CPU. A UI RPC ticks the node once; only received link traffic opens the fast
cadence, and WebView timers pause in the background. The core sometimes keeps
answering `ember_next_wakeup_ms()==0` (seen for ~40 s after start with a hub link); the runner then
falls back to 20 ms instead of spinning. A `tcp_server`/`wifi_direct` link adds a 30 ms arbiter
timer.

**Entropy:** `ember_create` gets 64 bytes from `SecureRandom` (the OS CSPRNG); 32 more bytes are
mixed in with `ember_add_entropy` on every `node.ready` and every 15 minutes.

**Battery advice:**

- Nothing native animates after start-up: the launch overlay and its timeout are gone once the web
  view has drawn.
- Exempt EmberMesh from battery optimisation (`host.android.battery_optimization`).
- On Xiaomi, Huawei, OPPO/Realme, Samsung and other aggressive brands, also allow *autostart* and
  *no restrictions* in the vendor's battery settings (see dontkillmyapp.com when you have a
  connection).
- The app holds a partial wake lock only while a link that needs the CPU awake is up (USB serial,
  audio, UDP/TCP/WebSocket, the hub, SPP) or when `keep_cpu_awake=always`. A BLE-only mesh relies on the
  Bluetooth stack to wake the phone.

## Launch screen, icons and accessibility

**Launch → web boot animation without a flash.** The logo (`docs/assets/logo.svg`) on `#17110f`,
the logo's 128-unit box at 30 % of the shorter side of the web view and centred in it — the size and
place of the web boot animation's first frame (ui/README.md "Launch screen"; `LOGO_FRACTION` and
`tools/gen_icons.py` hold the numbers).

1. Before Android 12 the window background is the colour plus the logo (`drawable-v23/splash_window`,
   colour only on 5.x); from 12 the system splash shows `drawable-v31/splash_icon` (no icon animation).
2. In the activity's first frame a native overlay (`ui/LaunchHandover.kt`) shows the same logo; the
   system splash is removed at that moment without a fade.
3. When the web view has committed its first visible frame (`onPageCommitVisible`) the overlay is
   removed in the same frame the page's first frame is drawn and the page gets `ember:launch-removed` (so the
   animation starts at once). A 6 s timeout removes it in any case. Nothing native
   (views, timers, animators) remains after the hand-over.

The page gets the logo's square before its first paint, with the same names as on iOS:
`window.__emberHost.launchLogo = {x, y, size}` and `--launch-logo-x/-y/-size` on `<html>` (CSS px of
the page viewport; the web view sits inside the system-bar insets). The asset server adds a
same-origin `<script src="/__ember/launch-logo.js">` at the end of `<head>` of the UI's `index.html`
(the CSP allows no inline script); the values are refreshed on rotation and inset changes.

The web view's background is `#17110f` from the start, so there is no white frame even before the
page has applied a light theme. System-bar icons follow the page's theme (`data-theme` /
`theme-color`, watched in the page and reported through `EmberNative.appearance`): light icons on dark
pages, dark icons on light pages.

Cold start from the launcher, measured on an API 37 emulator (ms since process start): activity
created 280–680, first app frame 360–820, system splash removed 510–910, web view's first frame 520–910,
overlay removed 555–935; `Displayed` +480–1000 ms. Screenshots taken during launches show only the
brand background with the logo until the hand-over, never a white frame (`LaunchAndAccessibilityTest`).
On Android 13+ a launch that does not come from the launcher (notification, other app) gets the
system's icon-less splash in the same colour.

**Icons** are generated from the logo by `tools/gen_icons.py` (vector drawables; legacy PNGs written
by Pillow without text/EXIF chunks): adaptive launcher icon (brand background, mesh + ember
foreground, monochrome layer for themed icons), legacy `mipmap-*dpi` PNGs, and a white notification
silhouette.

**Accessibility.** `host.info.accessibility = {reduce_motion, font_scale, screen_reader,
high_contrast, bold_text}` from the system ("Remove animations", font size, a touch-exploring service
such as TalkBack, "High contrast text" where readable, "Bold text" on 12+), and `host.accessibility`
whenever any of them changes while the app runs. Font size is applied once: the web view's text
zoom is pinned to 100 % and the UI multiplies `font_scale` with its own `--text-scale`. Pinch-zoom
works where the page's viewport allows it (no zoom buttons). The web content is navigable with
TalkBack; the launch overlay is hidden from accessibility services.

**Haptics.** `host.haptic {kind:"tap"|"success"|"warning"|"error"}` → `{performed}` uses the view's
haptic feedback constants (KEYBOARD_TAP, CONFIRM/REJECT on 11+, LONG_PRESS), which honour the
system's touch-feedback setting; `unsupported` only on devices without a vibrator. Feature `haptic`.

**Smoothness.** The activity is hardware-accelerated and the web view keeps the default layer type
(a hardware layer would add an extra off-screen copy of every web frame). Frame statistics on the
emulator (software-composited GPU, busy host): cold start 26 frames, median 17 ms; eight tab
transitions 92 frames, median 17 ms, 90th percentile 27 ms, 99th 36 ms, 24 % janky. Real devices were
not measured.

## Privacy defaults

Nothing that reaches other devices carries the phone's or owner's name unless the user typed it:

- Profile name: empty until the user enters one at first run (the core's first-run state).
- Hub `/hub.json` `name`: the user's profile name, else `EmberMesh hub <first 4 hex of the node id>`.
- BLE peer advertising: service UUID + a random tie-break token; the Bluetooth device name is never
  included (scan response empty). Other phones list us as "EmberMesh phone".
- TCP server NSD (`_embermesh._tcp`) service name: `EmberMesh-<random 4 hex>` per start.
- The core is created with `device_name:"Android"` (no model, no user-set device name).
- Outside the app's control: Android itself shows the phone's Wi-Fi Direct name to Wi-Fi Direct peers
  and the Bluetooth name to devices it pairs with (Bluetooth SPP / BLE GAP); both are system settings.

## Hardware notes

- **Radio interfaces (Connect a radio)**:
  - **AIOC** (All-In-One-Cable): one USB device with a sound card and a CDC serial port. Pick it as
    the audio input/output and as the PTT adapter (`usb:1209:7388…`, DTR). The serial driver claims
    only the CDC interfaces, so Android's USB audio keeps the sound card and both work at once
    (unit-tested on an AIOC-style descriptor; not tried with the real cable).
  - **Digirig Mobile**: USB sound card + CP2102 serial behind a built-in hub: PTT on RTS.
  - A PTT-only USB adapter asks for USB permission itself the first time it keys.
  - The audio link's status shows the input level 5 times a second (`rx_level_dbfs`, `rx_clipping`)
    for setting the radio volume.

- **USB OTG**:
  - The phone must support USB host.
  - Use an OTG adapter or a USB-C to USB-A/micro cable; a plain charging cable will not work.
  - Radios and boards that draw a lot of power may need a powered hub or a Y-cable.
  - Plugging a device in opens EmberMesh, and the system asks once for permission. Tick "use by
    default".
- **Supported serial chips**:
  - FTDI FT232R/BM/FT2232/FT4232/FT232H/FT-X
  - Silicon Labs CP2102/CP2104/CP2105 (port 0)/CP2108
  - WCH CH340/CH341
  - Prolific PL2303 HX/TA/TB/HXN (G series). Counterfeit "PL2303HXA" chips may refuse to work.
  - Any CDC-ACM device: Arduino, RP2040, nRF52 (RAK4631, T-Echo), ESP32-S2/S3 native USB, STM32,
    Mobilinkd TNC3/4, many radios' USB ports.
  - Meshtastic and RNode boards normally use 115200 baud; most KISS TNCs use 9600 or 57600.
- **Some ESP32 boards reset** when DTR/RTS change. If yours does, turn off "Assert DTR" (advanced).
- **PTT**:
  - A radio interface cable that keys with RTS or DTR: in the audio connection's settings pick it under
    **Push-to-talk** — either the serial connection already open to it (e.g. the radio's CAT/data
    cable) or the bare USB serial adapter ("PTT only"). Choose RTS or DTR under the advanced options.
  - Otherwise pick **VOX / none** and use the radio's VOX (set `vox_tone_ms` on the link if needed).
  - A serial link used directly by the core (KISS etc.) still keys with its own `ptt_line`.
- **Audio cable wiring (TRRS headset jack, CTIA standard)**:
  - Tip = left out, ring 1 = right out, ring 2 = ground, sleeve = mic in.
  - Levels follow the canonical table in `core/src/link/README.md` (audio → Wiring):
  - Radio speaker → phone mic: 20:1–100:1 (26–40 dB), e.g. 10 kΩ series and 100–470 Ω to ground,
    plus about 2.2 kΩ across mic and ground so the phone detects a headset.
  - Phone earphone out → radio mic: about 100:1 (40 dB) plus a DC block, e.g. 10 kΩ series, 100 Ω to
    ground, 1 µF in series.
  - Then set the receive volume until the link's `rx_level_dbfs` sits around −30 … −10 dBFS without
    clipping, and the transmit level (`tx_level` / "Transmit level") until the far end decodes.
  - Phones without a jack need a USB-C audio adapter or a USB sound card (CM108-style dongles work).
    Bluetooth headsets work through SCO at 8/16 kHz, which is bandwidth-limited.
  - Acoustic coupling: phone speaker held to the radio mic, radio speaker to the phone mic. Raise
    `tx_gain` and use a quiet room.

## Troubleshooting

- **The diagnostics page shows instead of the UI**: `ui/index.html` was not in the repository at
  build time. Rebuild, or pass `-PemberUiDir=`.
- **"native library failed to load"**: the APK has no `.so` file for this CPU. The debug build includes
  arm64-v8a, armeabi-v7a and x86_64; add `x86` to `abiFilters` for very old emulators.
- **BLE finds nobody**:
  - Bluetooth must be on, and on Android 11 and older Location must be on too.
  - Both phones need the app open, or the service running.
  - Some old phones cannot advertise (central-only mode; see the link status).
- **UDP sees nobody on a hotspot**: some hotspots isolate clients. Add `unicast_peers`, or have the
  hotspot phone also run EmberMesh, since multicast is joined on the hotspot interface.
- **USB device not listed**: check `adb shell lsusb` or the scan result. Unknown VID/PID pairs fall back
  to CDC-ACM only when the device has a CDC interface. Report the VID/PID.
- **The mesh stops with the screen off**:
  - Exempt the app from battery optimisation.
  - Keep "run in background" on.
  - Check the vendor's autostart settings.
- **Logs**: `adb logcat -s EmberRuntime EmberLinks EmberNode EmberUI EmberWeb MeshService`.
