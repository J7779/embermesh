// Motion helpers. All motion is optional decoration on top of a UI that works without it:
// nothing here ever delays a state change, and every helper is a no-op when motion is off.
//
// Level comes from <html data-motion="full|reduced|off"> (set by lib/prefs.js from the §8
// `motion` preference) and is forced to "off" by low-power mode (data-lowpower="1") or a hidden
// page. Without the attribute: off under automation (navigator.webdriver, ?motion=off), reduced when
// the platform asks for reduced motion, otherwise full. CSS keys off the same attributes
// (css/app.css "Motion" section); JS-driven animation (Web Animations API) must go through here.
//
// Imports without a DOM (unit tests): every browser global is guarded.

const LEVELS = ['off', 'reduced', 'full'];
const doc = typeof document !== 'undefined' ? document : null;
const win = typeof window !== 'undefined' ? window : null;

/** Durations (ms) and easings; the CSS tokens in app.css use the same values. */
export const DUR = { press: 90, fast: 140, base: 220, slow: 320, sheet: 380 };
export const EASE = {
  out: 'cubic-bezier(.2, .8, .2, 1)',
  in: 'cubic-bezier(.4, 0, 1, 1)',
  std: 'cubic-bezier(.4, 0, .2, 1)',
  spring: 'cubic-bezier(.34, 1.4, .64, 1)',
};

/** Pure: the effective level for the given environment. Exported for tests. */
export function resolveLevel({ attr, lowPower, hidden, webdriver, query, reducedMedia } = {}) {
  if (lowPower || hidden) return 'off';
  if (LEVELS.includes(attr)) return attr;
  if (webdriver || /(?:^|[?&])motion=off(?:&|$)/.test(query || '')) return 'off';
  return reducedMedia ? 'reduced' : 'full';
}

export function motionLevel() {
  if (!doc) return 'off';
  const ds = doc.documentElement.dataset;
  return resolveLevel({
    attr: ds.motion,
    lowPower: ds.lowpower === '1',
    hidden: doc.visibilityState === 'hidden',
    webdriver: !!(win && win.navigator && win.navigator.webdriver),
    query: win && win.location ? win.location.search : '',
    reducedMedia: !!(win && win.matchMedia && win.matchMedia('(prefers-reduced-motion: reduce)').matches),
  });
}

/** True when the current level is at least `min` ('reduced' or 'full'). */
export function allows(min = 'full') { return LEVELS.indexOf(motionLevel()) >= LEVELS.indexOf(min); }

/**
 * Pure: route direction from the previous and next hash and the visit stack. "back" for history
 * back to the previous entry or a shallower path in the same section, "fwd" for deeper, "tab" for
 * a switch between sections (shown as a cross-fade, never a slide). Exported for tests.
 */
export function routeDirection(prev, next, stack) {
  const parts = h => String(h || '').replace(/^#\/?/, '').split('?')[0].split('/').filter(Boolean);
  const a = parts(prev), b = parts(next);
  if (prev == null) return 'none';
  if (stack && stack.length > 1 && stack[stack.length - 2] === next) return 'back';
  if ((a[0] || 'chats') !== (b[0] || 'chats')) return 'tab';
  if (b.length > a.length) return 'fwd';
  if (b.length < a.length) return 'back';
  return 'tab';
}

let activeVT = null;
/** Ends a running screen transition immediately (a new navigation interrupts the old one). */
export function finishScreenTransition() {
  if (activeVT) { try { activeVT.skipTransition(); } catch { /* finished */ } activeVT = null; }
}


/**
 * View Transitions only on desktop-class screens (wide, fine pointer). Measured on a phone-sized
 * page with 4x CPU throttling, the snapshot costs one ~130 ms frame per navigation, while the CSS
 * enter animation keeps every frame at 16.7 ms — so phones and tablets use the CSS path.
 */
export function useViewTransitions() {
  if (!doc || typeof doc.startViewTransition !== 'function' || !win || !win.matchMedia) return false;
  return win.matchMedia('(min-width: 900px) and (pointer: fine)').matches;
}

/** Longest the View Transition may hold the old frame while the new screen renders (ms). */
export const VT_CAP_MS = 120;
let enterTimer = 0;

/**
 * Runs `run()` (clears #main and renders the next screen into it; returns a promise) wrapped in a
 * screen transition, and returns run()'s promise. Nothing ever waits for the animation:
 *  - full + View Transitions API (desktop-class screens, see useViewTransitions): the old frame stays up until the new screen has rendered or
 *    VT_CAP_MS passed (whichever is first), then slides/fades to the live new screen; taps go to
 *    the new screen at once (app.css: ::view-transition { pointer-events: none }).
 *  - full without the API, or reduced: #main[data-enter=<dir>] makes the new content enter with a
 *    CSS animation (reduced: a short fade) while it renders.
 *  - off / low power / hidden page: just run().
 * `dir`: fwd | back | tab | none (see routeDirection).
 */
export function screenTransition(run, main, dir) {
  finishScreenTransition();
  const level = motionLevel();
  const root = doc && doc.documentElement;
  if (level === 'off' || !root || !main) return Promise.resolve().then(run);
  root.dataset.navDir = level === 'reduced' ? 'fade' : dir || 'tab';
  if (level === 'full' && dir !== 'none' && useViewTransitions()) {
    let p = null;
    try {
      const vt = doc.startViewTransition(() => {
        p = Promise.resolve().then(run);
        return Promise.race([p.catch(() => {}), new Promise(r => setTimeout(r, VT_CAP_MS))]);
      });
      activeVT = vt;
      const done = () => { if (activeVT === vt) activeVT = null; };
      vt.finished.then(done, done);
      vt.ready.catch(() => { /* skipped */ });
      return vt.updateCallbackDone.then(() => p, () => p || run());
    } catch { /* fall back to CSS */ }
  }
  clearTimeout(enterTimer);
  main.dataset.enter = dir || 'tab';
  enterTimer = setTimeout(() => { delete main.dataset.enter; }, DUR.slow + 400);
  return Promise.resolve().then(run);
}

/** Adds a one-shot animation class; removes it when the animation ends (or after `ms`). */
export function playClass(el, cls, ms = 700) {
  if (!el || !allows('reduced')) return;
  el.classList.remove(cls);
  // Restart without a forced layout: the class goes on in the next frame.
  const raf = win && win.requestAnimationFrame ? win.requestAnimationFrame.bind(win) : f => setTimeout(f, 16);
  raf(() => {
    el.classList.add(cls);
    const off = () => { el.classList.remove(cls); el.removeEventListener('animationend', onEnd); };
    const onEnd = e => { if (e.target === el) off(); };
    el.addEventListener('animationend', onEnd);
    setTimeout(off, ms);
  });
}

/**
 * Web Animations API wrapper gated on the motion level. `min` is the lowest level that plays it
 * ('full' for decoration, 'reduced' for essential state-change fades). Returns the Animation or
 * null. Callers never await it.
 */
export function animate(el, keyframes, opts = {}, min = 'full') {
  if (!el || typeof el.animate !== 'function' || !allows(min)) return null;
  try { return el.animate(keyframes, { duration: DUR.base, easing: EASE.out, ...opts }); } catch { return null; }
}

/** Short opacity+scale pulse (status counters, activity lamps). */
export function pulse(el, scale = 1.25) {
  return animate(el, [{ transform: 'scale(1)', opacity: 1 }, { transform: 'scale(' + scale + ')', opacity: .85 }, { transform: 'scale(1)', opacity: 1 }], { duration: DUR.slow, easing: EASE.std });
}

/**
 * Removes `el` after its leave animation (class `leaving`), or at once when motion is off. The
 * element stops taking pointer events immediately, so it never blocks the next action.
 */
export function leave(el, ms = DUR.base) {
  if (!el || !el.parentNode) return;
  if (!allows('reduced')) { el.remove(); return; }
  el.classList.add('leaving');
  el.setAttribute('aria-hidden', 'true');
  el.style.pointerEvents = 'none';
  let done = false;
  const go = () => { if (!done) { done = true; el.remove(); } };
  el.addEventListener('animationend', e => { if (e.target === el) go(); });
  setTimeout(go, ms + 80);
}

/**
 * Removes `el` now and lets a visual copy animate out in its place (class `from` → `to` plus
 * `leaving`). Use it where other code reacts to the element's presence (an `.overlay` makes the
 * page inert in lib/a11y.js): the real element is gone at once, only its picture lingers. The
 * copy is inert, hidden from assistive tech and has no ids or form names.
 */
export function ghostLeave(el, from, to, ms = DUR.base, inner = null) {
  if (!el || !el.parentNode) return;
  if (!allows('reduced') || typeof el.cloneNode !== 'function') { el.remove(); return; }
  const g = el.cloneNode(true);
  for (const n of [g, ...g.querySelectorAll('[id],[name]')]) { n.removeAttribute('id'); n.removeAttribute('name'); }
  // Inner classes other code queries (e.g. `.sheet`) are renamed too: [[from, to], …].
  for (const [a, b] of inner || []) for (const n of g.querySelectorAll('.' + a)) { n.classList.remove(a); n.classList.add(b); }
  g.classList.remove(from);
  g.classList.add(to, 'leaving');
  g.setAttribute('aria-hidden', 'true');
  g.setAttribute('inert', '');
  g.style.pointerEvents = 'none';
  el.parentNode.insertBefore(g, el);
  el.remove();
  let done = false;
  const go = () => { if (!done) { done = true; g.remove(); } };
  g.addEventListener('animationend', e => { if (e.target === g) go(); });
  setTimeout(go, ms + 80);
}

/** Smoothly counts a number from its current value to `to` (text only; ~0.3 s; full motion). */
export function countTo(el, to, fmt = v => String(Math.round(v))) {
  if (!el) return;
  const from = Number(el.dataset.value);
  el.dataset.value = String(to);
  if (!allows('full') || !Number.isFinite(from) || from === to || !win || !win.requestAnimationFrame) { el.textContent = fmt(to); return; }
  const t0 = performance.now();
  const step = t => {
    if (el.dataset.value !== String(to)) return; // superseded
    const p = Math.min(1, (t - t0) / DUR.slow);
    const e = 1 - Math.pow(1 - p, 3);
    el.textContent = fmt(from + (to - from) * e);
    if (p < 1) win.requestAnimationFrame(step);
  };
  win.requestAnimationFrame(step);
}

let installed = false;
/**
 * Page-level wiring: pauses every CSS animation while the page is hidden (data-hidden on <html>),
 * and, only when lib/prefs.js did not set data-motion, applies the automation/platform fallback so
 * CSS and JS agree.
 */
export function installMotion({ fallback = false } = {}) {
  if (!doc || installed) return;
  installed = true;
  const root = doc.documentElement;
  const vis = () => {
    if (doc.visibilityState === 'hidden') { root.dataset.hidden = '1'; finishScreenTransition(); } else delete root.dataset.hidden;
  };
  doc.addEventListener('visibilitychange', vis);
  vis();
  if (fallback && !root.dataset.motion) {
    const q = win && win.location ? win.location.search : '';
    const auto = (win && win.navigator && win.navigator.webdriver) || /(?:^|[?&])motion=off(?:&|$)/.test(q);
    if (auto) root.dataset.motion = 'off';
  }
}
