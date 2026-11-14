# EmberMesh shared UI (`ui/`)

One web UI for every platform: browser + WebAssembly core, Android/iOS WebViews + native core,
and remote control of a desktop daemon. Plain ES modules, HTML and CSS — **no framework, no
dependencies, no build step, no network requests to anything but the page's own origin**.

## Run it

```sh
cd ui && python3 -m http.server 8000
# open http://localhost:8000/?mock=1          demo with the in-browser mock backend
#      http://localhost:8000/?mock=1&fresh=1  first-run setup
#      http://localhost:8000/?mock=1&locked=1 lock screen (passphrase: demo)
#      http://localhost:8000/?mock=1&unsupported=guides.categories,files.send   graceful degradation
#      http://localhost:8000/?mock=1&torch=yes|ask|no|refused   Morse by light: flashlight states
#      http://localhost:8000/                 real WASM core (needs ui/wasm/ from the web build)
#      http://localhost:8000/?remote=1&token=…  control a desktop daemon over /rpc
```

The mock keeps state in `localStorage` (`embermesh.mock.v1`); clear site data to reset.

## Tests

```sh
node --test ui/tests/                         # pure logic: markdown (incl. XSS + ReDoS), formatters,
                                              # store, schema forms, morse, backend selection,
                                              # source lint (no innerHTML/eval/external URLs), CSP,
                                              # mock contract vs docs/RPC_API.md
node ui/tests/smoke_chrome.mjs --real auto     # same against the REAL WASM core (dist/web/wasm or
                                              # build/web/wasm + guide pack): first run, send, bulletin,
                                              # SOS, loopback link, guide search, tool, lock/unlock,
                                              # reload persistence (or --real http://host:port/ for a
                                              # served dist/web)
node ui/tests/smoke_chrome.mjs [--shots DIR]  # headless Chrome: every screen at 390×844 and 1280×800,
                                              # themes, interactions (send, SOS hold, link wizard,
                                              # tool run, search); fails on console errors, CSP
                                              # violations, failed requests, sideways overflow, XSS
```

The smoke test needs Chrome/Chromium (`CHROME=/path` to override) and Node ≥ 22.

## Structure

```
index.html            shell + strict CSP (see below)
css/app.css           all styles: tokens (type/space/radius/elevation/motion), themes dark (default,
                      OLED black) / light / night (red only), components, and a "Motion" section where
                      every animation is keyed off html[data-motion] (full / reduced = fades only /
                      off); low-power mode and a hidden page stop all of it (tests/motion.test.js)
js/lib/motion.js      motion level, screen transitions (CSS enter animation; View Transitions on
                      desktop-class screens), one-shot animation helpers; never delays an action
js/app.js             bootstrap, app context, hash router, status strip, nav, global events,
                      notifications, lock/setup gating
js/backend.js         createBackend(): picks + dynamically imports ./backends/<kind>.js (RPC_API.md §5)
js/backends/mock.js   full fake of the RPC surface (data in js/mock/*.js)
js/backends/{wasm,native,remote}.js, js/endpoints/   real backends and device endpoints (see apps/web/README.md)
js/lib/dom.js         h() element builder, null-safe append(), svg(), highlight() — text nodes only
js/lib/markdown.js    safe Markdown → AST → DOM (or escaped HTML for tests)
js/lib/schema.js      Field-schema value handling (pure);  js/lib/form.js renders it
js/lib/format.js      time/size/frequency/geo/airtime formatters (pure)
js/lib/core_spec.js   values from docs/RPC_API.md in one place (settings ranges/defaults) + pure
                      helpers for core results (hops, conv ids, legal mode, audio meter, catalogues,
                      host.info.actions); the mock uses the same table
js/lib/ratelimit.js   toast/notification limiter, unverified-sender test, strong passphrase
js/lib/about.js       licence data for Settings → About (app licence, third-party list, guide licence families)
js/lib/back.js        Android back button decision (window.__emberBridge.onBack)
js/lib/capture.js     Morse reader capture logic (pure): watched-region brightness, blinking-light
                      finder, sample batching, resampling, PCM base64, light advice
js/lib/capture_media.js  getUserMedia camera/microphone readers for morse.rx.feed; test hook
                      window.__emberCaptureHook = {camera(), mic()} returning MediaStreams
js/mock/morse_rx.js, radios.js, hardware.js   mock morse.rx.*, rf.radios, rf.hardware (§6)
js/mock/link_types.js snapshot of the real core's links.types (the mock offers the real drivers)
js/lib/store.js       tiny observable store
js/lib/morse.js       text → Morse → on/off timing; sending by light (§10): encodeText (prosigns <AR>,
                      skipped characters), buildSignal (pattern + per-character spans), speed limits
                      (torch 12 WPM default / 15 max, screen ≤ 7 WPM = ≤ 3 flashes/s) with the evidence
js/lib/ui.js          toasts, sheets/dialogs, QR rendering, hold-to-confirm, file pick/save
js/lib/icons.js       inline SVG icon paths
js/screens/*.js       chats, people (contacts/peers/radar/stats), sos, bulletins, links,
                      radio (Connect a radio wizard, #/links/radio), morse (send / read by camera /
                      read by sound, #/tools/morse; Send is morse_send.js: flashlight or screen via
                      host.signal.flash {mode}, preview, progress, Stop), help (Instructions, #/help, #/help/hardware),
                      hardware ("What you need" cards and matrix),
                      guides, tools, extras (notes/checklists/inventory), settings, setup,
                      lock, more
tests/                node:test unit tests + smoke_chrome.mjs
```

Routes are hash based (`#/chats/dm:<id>`, `#/guides/read/<id>`, `#/links/add?kind=ble`, …) so the
app works from `file://`, inside WebViews and when inlined into one HTML file. Each screen module
exports `default async function render(app, params, root)` and may return a cleanup function;
`app.on(event, fn)` subscriptions are removed automatically when the route changes.

## How backends plug in

`createBackend()` follows the selection order of `docs/RPC_API.md` §5: native bridge present →
`native`; `?remote=1` → `remote`; `?mock=1` → `mock`; else `wasm`. Every backend module has
`export default class { kind; async init(); async call(method, params); on(event, fn) }`. `call`
rejects with an `Error` carrying `.code`.

The UI talks only through `app.api.call()` / `app.api.try()`:

* `not_implemented` / `unsupported` mark the method unavailable (`app.supports(m)`) and the
  feature is hidden or shows a short "not available yet" note instead of breaking the page.
* `locked` from any non-`security.*` call switches to the lock screen.
* Device features are gated on `host.info.features` (`hub`, `qr_scan`, `location`, `flash`,
  `tone`, `file`, `notify`, `webrtc`) with fallbacks: `<input type=file>`/download link when there
  is no `file`, on-screen flash and WebAudio tone when there is no `flash`/`tone`, paste boxes when
  there is no `qr_scan`.
* Forms for links, endpoints and tools are rendered from the catalogues (`host.info`
  endpoint kinds, `links.types`, `tools.list`) — nothing per-driver is hard-coded except optional
  extra help text (`KIND_HELP` in `screens/links.js`).

## Security

* Everything from the mesh, guides and catalogues is treated as hostile. The UI never assigns
  `innerHTML`/`outerHTML`, never uses `eval`/`new Function`, and builds DOM with text nodes
  (enforced by `tests/misc.test.js`).
* `markdown.js` supports headings, emphasis, lists (nested, task), tables with alignment, code,
  blockquotes, rules and links. Raw HTML is shown as text, images become `[image: alt]`, links get
  no `href` (only `data-href`): internal guide links navigate in-app, external ones open a sheet
  showing the address with a copy button, script-like URLs are rendered inert. Input size, nesting
  depth and table width are capped; regexes are written to avoid catastrophic backtracking
  (tested with pathological inputs).
* CSP (meta tag in `index.html`):
  `default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; style-src 'self'; img-src 'self' data: blob:;
  connect-src 'self' ws: wss: blob: data:; worker-src 'self' blob:; object-src 'none'; base-uri 'none';
  form-action 'none'; frame-src 'none'`. Inline styles are set only through the CSSOM (allowed).
  **Single-file packaging** must replace `'self'` in `script-src`/`style-src` with the sha256
  hashes of the inlined script/style (or a nonce). Very old WebViews (Chrome < 95) do not know
  `'wasm-unsafe-eval'`; the native backends do not need it.

## UI-side conventions (not RPC methods)

* kv namespaces used: `ui` (`setup_done`, `guide_bookmarks`, `guide_recent`, `reader_scale`),
  `notes` (one key per note), `checklists` (one key per list), `inventory` (`items`), `stations`
  (station profiles), `tool_inputs` (last inputs per tool id).
* First run = `node.info.name` is empty.
* `hops`: 0 = heard directly, null = unknown. `via_link` 0 = none.
* Interop text lives in `io:<network>` conversations (kind `interop`); replies use `interop.send`
  on a chosen link, or `messages.send` (core picks the link the network was last heard on).
* `node.ready` (after unlock, duress unlock, wipe) reloads host.info, node.info, settings and the
  current screen.
* Android back button: `window.__emberBridge.onBack()` is added to the bridge object (never
  replaced); it closes the top dialog / goes back a screen / to Chats and returns true, or false
  when native should handle it.
* Legal (amateur) mode in the link wizard is predicted from the driver's own config fields
  (`licence:"amateur"`, `amateur:true`, `licensed:true`); on link cards from `caps`. A missing
  callsign is asked for and saved with `node.set_profile`.
* Endpoint fields whose key ends in `link_id` (e.g. Android `ptt_link_id`) are shown as a picker of
  existing links. Changing a link's endpoint settings re-creates it (host.link.remove + create)
  after confirmation; driver settings use links.configure.
* Platform extras from `host.info.actions` (RPC_API §3) appear in Settings → This device.
* Morse reading (RPC_API §6): camera brightness or microphone PCM → `morse.rx.feed`. Native
  `host.capture.*` is preferred for the camera when the host lists `capture_camera_luma` (a switch
  offers the in-page camera when it also lists `getusermedia`), and used for sound only when the page
  cannot capture. `host.capture.state {running:false}` returns the reader to idle with the error.
* Connect a radio reads `host.info.audio` hints generically (`serial_ptt`, `presets_supported`,
  `presets_unsupported:{preset: reason}`) to hide connection methods a host cannot do.
* The guides home waits for the library while `guides.categories` reports 0 guides and shows
  progress from an optional host event `host.guides.progress {loaded, total}` (bytes).
* Notifications/toasts are rate-limited per source and overall; SOS alerts from unverified senders
  are grouped below verified ones and never trigger system notifications.
* Sneakernet bundles as QR codes: base64 split into frames `EMB1 <i>/<n> <data>` (~700 chars).
* Station profiles fill any tool input whose key starts with `tx_`/`rx_` and contains
  `power`/`gain`/`height`/`loss` (feedline loss computed from cable type, length and frequency).

## Launch screen

Native apps show a static launch screen before the web view exists. It must be identical to the
first frame of the start-up animation (`index.html` `#ember-boot`, `css/boot.css`,
`js/boot/`), which then animates from exactly that image:

* **Background:** `#17110f` (sRGB), full screen, every theme. (The web view, its scroll view and the
  window behind it should be `#17110f` too, so nothing white or black shows in between.)
* **Artwork:** `docs/assets/logo.svg` — its full 128 × 128 viewBox. Its background disc is the same
  colour as the background, so it is invisible; the drawing is the hexagon of six nodes and links
  with the flame (nodes span 86/128 of the box width).
* **Size:** the 128-unit box is **30 % of the shorter side** of the area the page will occupy
  (`min(width, height) × 0.30`), in every orientation; no minimum or maximum.
* **Position:** the box is **centred horizontally and vertically** in that area (box centre at 50 %
  / 50 %). No wordmark, no text, no shadow.
* **Exact placement:** when the native launch logo cannot be at the page's centre (e.g. the web
  view sits inside the safe area while the launch screen covers the whole display), pass the logo's
  rectangle in CSS px of the page viewport before the first paint, as
  `window.__emberHost.launchLogo = {x, y, size}` (top-left corner and side) **and** the CSS
  variables `--launch-logo-x`, `--launch-logo-y`, `--launch-logo-size` (with `px`) on `<html>`,
  from a document-start script. The page then draws its first frame exactly over the native logo;
  after a rotation it falls back to the centred rule. iOS does this; Android should use the same
  names.
* **Hand-over:** remove the native placeholder instantly (no fade) once the page has painted, and
  in the same moment tell the page: `window.__emberHost.launchRemoved = true` and
  `window.dispatchEvent(new Event('ember:launch-removed'))` (iOS and Android do both). The page
  holds the still logo until then (at most 1.5 s without the signal), so the animation's frame 0 is
  the first frame anyone sees — never drawn behind the native screen, never "late".

**The animation is the same on every platform** (docs/RPC_API.md §8): every page load — a cold start
of a native app, a browser visit, a reload — plays the **full** animation (2.4 s + ~0.6 s hand-over).
Returning from the background does not replay it (the page is not reloaded). Being ready early,
having seen it before, a late start, low-power mode and the platform's reduce-motion setting do not
change it (owner decision; the rest of the UI follows the platform setting; the animation has no
flashing). The app loads underneath and the hand-over waits for the animation. Only choices made in
the app do: `boot_animation` `short` (1.25 s) / `off` (one still frame); Motion explicitly `reduced`
→ `reduced` (still logo, the name fades in), explicitly `off` → `off`; Simple mode → `reduced`. Any
tap/click/key skips it at once, so does a dialog or toast; a 2.9 s cap counts from its first frame.
The Accessibility screen states this next to the setting.
Where no 2D canvas exists (very old or locked-down web views) the `reduced` variant is shown.

Code: `js/boot/timeline.js` (timelines, variant choice, hand-over state machine — pure),
`js/boot/scene.js` (3D renderer on a 2D canvas), `js/boot/boot.js` (DOM glue). Test hooks:
`?noboot=1` and `navigator.webdriver` skip it, `?boot=full|short|reduced|off` forces a variant (also
under automation), `?bootpause=<ms>` freezes at that time (past the end: into the hand-over) with
`window.__emberBoot.seek(ms)` for stills. Read-only diagnostics: `window.__emberBoot = {variant,
active, reason, canvas, osReduce, at:{module, frame0, removed?, ready?, out?, done?}}`. It reads the person's
choices from local storage (`ember.ui.<name>` JSON keys written by `js/lib/prefs.js`, theme from
`embermesh.ui.theme`) and stores nothing (an old `embermesh.ui.boot_seen` marker is removed).

## Accessibility and Simple mode

Preferences (docs/RPC_API.md §8) live in `js/lib/prefs.js`: kv namespace `ui` keys `simple_mode`,
`motion`, `text_scale`, `high_contrast`, `boot_animation` plus the UI-only `haptics`,
`reading_spacing`, `shortcuts`; mirrored as JSON in local storage under `ember.ui.<key>`
(`high_contrast` null = follow the platform). The effective state is reflected on `<html>`:
`data-simple`, `data-motion` (full/reduced/off after combining the preference, the platform's
reduce-motion setting or `host.info.accessibility`, low-power mode and Simple mode),
`data-contrast`, `data-textsize`, `data-spacing`, `data-transparency` and `--text-scale`;
`js/lib/a11y.js` adds `data-route`/`data-subroute`. Everything those switch is in
`css/a11y.css` (load after app.css: `<link rel="stylesheet" href="css/a11y.css">`; until index.html
links it, prefs.js adds it itself, or adopts the embedded copy in the single-file build).

```js
import { initPrefs, getPrefs, setPref, onPrefsChange, navItems } from './lib/prefs.js';
import { initA11y, announce, haptic } from './lib/a11y.js';
import { initSimple } from './lib/simple.js';
await initPrefs(backend); initA11y(app); initSimple(app);
onPrefsChange(() => buildNav());     // nav = navItems(NAV)
announce('Message not sent.', { assertive: true });   // rate limited live region
```

* Screen code marks secondary controls with `data-secondary`; in Simple mode they are hidden until
  the screen's "More options" button (added by simple.js) is pressed. Nothing is removed.
* Use rem for font sizes (they follow `--text-scale`), never fixed heights on text containers,
  and the class `motion-decorative` on purely decorative animation.
* Keyboard: g + letter jumps to a destination (c chats, p people, s SOS, b bulletins, l links,
  g guides, i instructions, t tools, n notes, o settings, a accessibility, m more), ? lists them,
  / focuses the screen's search. Off in Settings → Accessibility.
* Audit: `node ui/tests/a11y_audit.mjs [--shots DIR] [--json FILE] [--quick]` (headless Chrome,
  mock backend) compares counts per screen and rule with `tests/a11y_baseline.json` and fails on
  any increase; `--update-baseline` after fixes.

### Plain wording (Simple mode)

Simple mode replaces these exact labels; the operator manual should use the same words when it
describes Simple mode (source: `GLOSSARY` in js/lib/simple.js).

| Normal | Simple mode |
|---|---|
| Chats | Messages |
| SOS | Send for help |
| Emergency | Send for help |
| Links | Connections |
| Links & radios | Connections |
| Bulletins | Notice board |
| Instructions | How to use |
| Survival library | Guides |
| People & mesh | People |
| Tools | Radio tools |
| Safety check-in | Tell people how you are |
| SOS alerts received | Calls for help nearby |
| Roll call (latest check-in per person) | Who has said how they are |
| Smart search | Search by meaning |
| Region & display | Region and screen |
| Mesh & relaying | Network and passing on messages |
| Sneakernet bundles | Carry messages by hand |
| Fingerprint words | Safety words |
| Low-power mode (no animation, fewer refreshes) | Battery saver (no animation, fewer updates) |
| Share this device | Let others connect through this device |
| Net plan (where to tune) | Where to tune the radio |
| Mesh statistics | Network numbers |
| Peers | Nearby devices |
| Radar | Map of nearby devices |
| Contacts | People I know |
| Post | Write a notice |
| Show my card | Show my contact code |
| Back up identity | Save a backup of my identity |
| Set passphrase | Set a password |
| Change passphrase | Change password |
| Panic wipe | Erase everything |
| Duress passphrase | Emergency password |
| I am safe — stop SOS | I am safe now — stop calling for help |
| Send emergency | Yes, send as emergency |
| Stop SOS | Yes, stop |
| Send as emergency? | Send as an emergency? |
| Stop SOS? | Stop calling for help? |
| Remove passphrase? | Remove the password? |
| Replace identity? | Replace who you are on this device? |
