# All apps on one mesh

`all_together.mjs` is the end-to-end test that runs every EmberMesh app on **one** mesh at the
same time. The per-app tests only check each app against emberd on its own (pairwise).

```
node tools/e2e/all_together.mjs                       # browser + Android + iOS
node tools/e2e/all_together.mjs --skip ios            # e.g. on a machine without Xcode
node tools/e2e/all_together.mjs --sequential --keep   # phones one after the other, keep logs
```

## What it runs

1. **One emberd hub** (`dist/desktop/emberd`, or `build/desktop/apps/desktop/emberd`) on a free
   port, with a temporary data dir under the OS temp directory. It is named `Hub`, serves
   `dist/web`, and runs with `--local-no-token --no-udp`. It binds to `127.0.0.1`: the Android
   emulator reaches the host's loopback as `10.0.2.2`. If you pass a different `--host-ip` (for a
   real phone), it binds to `0.0.0.0` instead.
2. **Browser guest**: a headless-Chrome page at `http://127.0.0.1:<port>/`. This is the real WASM
   core. It auto-joins through `/hub.json` + `/ws`, completes first run through the DOM as
   `Browser guest`, sends a Public message, and stays connected until the end. The helpers are
   shared with `apps/desktop/tests/hub_guests_chrome.mjs` and live in
   `apps/desktop/tests/chrome_guest.mjs`.
3. **Android**: `apps/android/scripts/e2e_emberd.py --attach-port <port> --transports ws --nonce
   <nonce> --expect-public "browser public <nonce>"` runs the instrumented
   `org.embermesh.app.EmberdInteropTest` against that same emberd (`ws://10.0.2.2:<port>/ws`).
   `--expect-public` passes the instrumentation argument `expect_public`.
4. **iOS**: `TEST_RUNNER_EMBERD_PORT=<port> TEST_RUNNER_EXPECT_PUBLIC="browser public <nonce>" xcodebuild … -only-testing:EmberMeshTests/EmberdInteropTests/testWebSocketToEmberd test`
   runs in the simulator against that same emberd (`ws://127.0.0.1:<port>/ws`). xcodebuild builds
   first if needed, using `build/ios/DerivedData` like `./build.sh`.

When it is given an expected text, each device test waits up to 20 s for it in the app's own Public
channel. It reports `interop expect-public: ok (from <node_id>, signed_ok <bool>)`, or `… timeout`
and fails.

The Android and iOS tests run at the same time by default. Use `--sequential` to run them one
after the other. Either way, emberd and the browser guest stay up for the whole run.

## What it checks

Each check prints one line, `ok` or `FAIL`. Things the test cannot prove print as `n/a`.

- Each device test passes against the shared emberd. This covers Public and DMs both ways with
  delivery receipts, as in the pairwise tests.
- emberd's Public channel holds the Public message from Android (`android->emberd public <nonce>`),
  from iOS (`ios→emberd websocket <tag>`; the test picks the tag) and from the browser guest
  (`browser public <nonce>`), each with `outgoing:false`.
- **Phone → hub → browser:** the browser guest's Public channel, read through its RPC, holds the
  Android message and the iOS message, from the same node ids emberd saw, with `signed_ok:true`.
  The message must arrive within `--relay-timeout` (45 s) after the phone tests finish, and the
  check line prints the delay. A recipient that does not have the sender's key holds a signed
  message for up to 5 minutes, then delivers it marked unauthenticated (docs/PROTOCOL.md,
  "Verification"). Such a late or unauthenticated delivery is a `FAIL`.
- **Browser → hub → phone:** the Android app's and the iOS test node's own Public channels
  received `browser public <nonce>` from the browser guest's node id, with `signed_ok true`. Each
  device test reports this through its `interop expect-public` line.
- The browser guest's hub link stayed up for the whole run (polled every 1.5 s).
- emberd lists the browser, Android and iOS nodes in `mesh.peers`, at some point during the run or
  at the end. The phones' test nodes may disconnect when their tests finish.
- Informational only: the most guests on the hub at once. For Android, whether the app's final
  `mesh.peers` (from logcat) includes the browser guest.

Checks that depend on the browser guest print `n/a` when it is skipped with `--skip browser`.

The run ends with `RESULT PASS|FAIL`, the apps that took part and those that were skipped,
unavailable or failed. The exit code is 0 only if every check passed. A skipped app is reported as
skipped, never as passed. A missing device or simulator for an app you did not `--skip` is a
`FAIL`.

## Options

| Option | Default | |
|---|---|---|
| `--skip android\|ios\|browser` | none | repeatable or comma-separated |
| `--serial S` | `$ANDROID_SERIAL` or `emulator-5554` | adb device |
| `--simulator NAME\|UDID` | `$IOS_SIMULATOR`, else the booted iPhone, else the first iPhone | |
| `--sequential` | off | run the phone tests one after the other |
| `--keep` | off | keep the temp dir: emberd data, `emberd.log`, `chrome.log`, `android.log`, `ios.log`, `ios.xcresult`. It is also kept when the run fails |
| `--host-ip IP` | `10.0.2.2` | the host address as seen from the Android device |
| `--emberd PATH`, `--web DIR`, `--chrome PATH` | see above | |
| `--android-timeout S`, `--ios-timeout S`, `--relay-timeout S` | 420, 1800, 45 | |

## Prerequisites

- Node ≥ 22, Chrome or Chromium, and a built `dist/` (`./build.sh all`).
- Android: an emulator or device online, and both APKs installed:
  `cd apps/android && ./gradlew installDebug installDebugAndroidTest`. `connectedAndroidTest`
  uninstalls them again when it finishes.
- iOS: Xcode and an available iPhone simulator.
- Headless Chrome, adb and the simulator need to run outside any command sandbox.
