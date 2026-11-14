---
name: ui-change
description: Change the shared EmberMesh web UI (ui/) that every app uses - no build step, the mock backend, preferences, accessibility, motion and Simple mode rules, security lints, tests and headless-Chrome smoke walks, and what the Android, iOS, web and desktop hosts expect from it. Use when editing anything under ui/, adding a screen or a UI for a new RPC or host method.
---

# Changing the shared UI

Reference: `ui/README.md` (structure, backends, security, launch screen, accessibility, Simple
mode wording), `docs/RPC_API.md` §5 (backend selection), §8 (appearance, motion, accessibility).

## 1. Run it

```sh
cd ui && python3 -m http.server 8000
# http://localhost:8000/?mock=1           demo data (mock backend, state in localStorage)
# http://localhost:8000/?mock=1&fresh=1   first-run setup;  &locked=1 lock screen (passphrase: demo)
# http://localhost:8000/?mock=1&unsupported=guides.categories,files.send   graceful degradation
```

No framework, no npm, no build step: edit `ui/` and reload. Never edit the copies under
`apps/android/app/build/`, `build/` or `dist/`.

## 2. Rules that keep every app working

- **One UI for all hosts.** No platform forks of UI files. Talk only through `app.api.call()` /
  `app.api.try()`; gate features on `host.info.features` / documented `host.info` fields and on
  `app.supports(method)` (`not_implemented` / `unsupported` hide the feature or show a short note).
  A new host capability needs its contract in `docs/RPC_API.md` first and an implementation (or
  `unsupported`) in every host plus the mock (playbook change-rpc-or-protocol).
- **Mock backend** (`ui/js/backends/mock.js`, data in `ui/js/mock/`): implements every RPC method
  and emits only documented events (`ui/tests/mock_contract.test.js`). Mock data is fictional
  (placeholder callsigns `N0CALL`/`QX0ABC`, landmark positions, neutral names).
- **Security** (`ui/tests/misc.test.js`, CSP in `index.html`): no `innerHTML`/`outerHTML`, no
  `eval`/`new Function`, DOM from text nodes (`js/lib/dom.js` `h()`), Markdown only through
  `js/lib/markdown.js`, no external URLs, no CDN, no fonts or requests to anything but the page's
  own origin. Everything from the mesh, guides and catalogues is hostile.
- **Relative URLs only** (no leading `/`), so the same files work from `file://`, inside WebViews,
  under `/<repository>/` on Pages, in the single-file build and from a hub. Routes are hash based.
- **Accessibility and motion** (`js/lib/prefs.js`, `js/lib/a11y.js`, `css/a11y.css`): rem font
  sizes, no fixed heights on text, `data-secondary` on secondary controls (Simple mode hides them
  behind "More options"), `motion-decorative` on decorative animation, every animation keyed off
  `html[data-motion]` (full / reduced / off; low-power mode stops it), screen-reader labels,
  `announce()` for live messages. Simple mode uses the exact words of the `GLOSSARY` in
  `js/lib/simple.js` (table in `ui/README.md`); the manual must use the same words.
- **Rate limits**: toasts and notifications through `js/lib/ratelimit.js`; unverified senders never
  trigger system notifications.
- **Privacy**: never prefill a name from the device; show the user what a pairing code or a
  shared card reveals before they share it; request camera, microphone or location only when the
  user starts the feature and stop the stream when the screen or feature ends.
- **What native hosts expect**: the bridge object `window.__emberBridge` (Android adds `onBack()`;
  never replace the object), `window.__emberHost.launchLogo` and the `--launch-logo-*` CSS
  variables for the launch hand-over, the `ember:launch-removed` event, `#17110f` as the first
  frame. Android copies `ui/` to `assets/ui/` (Gradle property `emberUiDir`), iOS to
  `<App>/www` (`apps/ios/bundle-resources.sh`), the web build packages it into `dist/web/`
  (`apps/web/tools/package.mjs`, without `ui/tests`), emberd serves `dist/web` or `ui/`.

## 3. Test

```sh
node --test ui/tests/                         # logic, lints, CSP, mock contract
node ui/tests/smoke_chrome.mjs                # every screen, phone + desktop size, mock backend
node ui/tests/smoke_chrome.mjs --real auto    # real WASM core (after ./build.sh web)
node ui/tests/boot_chrome.mjs                 # start-up animation (if boot/ or index.html changed)
node ui/tests/a11y_audit.mjs                  # fails on any increase vs ui/tests/a11y_baseline.json
apps/web/build.sh --skip-wasm && node --test apps/web/test/   # repackage UI into dist/web, browser e2e
```

Smoke walks fail on console errors, CSP violations, failed requests, sideways overflow and XSS.
Use `--update-baseline` on the accessibility audit only after a real improvement, never to accept a
regression. Then rebuild the platforms you can (`./build.sh android ios`) so their bundles pick up
the change; native UI glue changes need their platform suites (playbook build-and-test).

## 4. Screenshots and docs

Screenshots only from the mock backend: `node ui/tests/smoke_chrome.mjs --shots DIR`. The README
images in `docs/assets/screenshots/` are approved binaries (allow-list) and must stay PNGs without
metadata. Update `guides/content/embermesh/screens-tour.md`, `settings-reference.md` or
`accessibility.md` when labels, screens or settings change (playbook write-manual-chapter).
