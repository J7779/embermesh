// Start-up animation. index.html paints the static logo (#ember-boot) before any script; this picks
// up from exactly that image, plays the 3D scene while the app loads underneath, and hands over when
// the minimum time has passed AND the app has rendered a screen — at once on a tap/key or when a
// dialog/toast appears — and at the hard cap. Self-contained (reads mirrored preferences itself).
// Hooks: ?noboot=1 / navigator.webdriver skip it; ?boot=full|short|reduced|off forces a variant;
// ?bootpause=<ms> freezes there (past the end: in the hand-over), window.__emberBoot.seek(ms).
import { chooseVariant, readPrefs, createMachine, frameAt, TIMELINES, KEYS, ease, clamp01 } from './timeline.js';
import { createRenderer } from './scene.js';

const CAP = 2900; // hand over by now even if the app is still loading (it has its own loading state)

if (typeof document !== 'undefined') {
  try { start(); } catch (e) { console.warn('boot screen:', e); const el = document.getElementById('ember-boot'); if (el) el.remove(); }
}

function start() {
  const doc = document, win = window, root = doc.getElementById('ember-boot');
  if (!root) return;
  const status = doc.getElementById('ember-boot-status'), main = doc.getElementById('main'), t0 = performance.now();
  let store = null;
  try { store = win.localStorage; } catch { /* blocked */ }
  const info = readPrefs(store, doc.documentElement.dataset || {}), query = new URLSearchParams(location.search);
  const appReady = () => !!(main && main.firstElementChild && !doc.getElementById('boot'));
  let osReduce = false;
  try { osReduce = win.matchMedia('(prefers-reduced-motion: reduce)').matches; } catch { /* old */ }
  let variant = chooseVariant({ query, webdriver: navigator.webdriver === true, prefs: info.prefs, lowPower: info.lowPower, osReduce, seen: info.seen, readyEarly: appReady(), late: t0 > 1200 });
  if (t0 > 3200 && !query.get('boot')) variant = 'none';
  const pq = query.get('bootpause'), pause = pq ? { t: +pq || 0 } : null;
  root.classList.add('eb-js'); // cancels the CSS fail-safe
  root.dataset.theme = info.theme;
  // native launch logo rectangle (iOS): ensure the CSS variables exist even if the host set only the object
  const ll = win.__emberHost && win.__emberHost.launchLogo;
  if (ll && +ll.size > 0 && !getComputedStyle(root).getPropertyValue('--launch-logo-size').trim()) {
    for (const k of ['x', 'y', 'size']) root.style.setProperty('--launch-logo-' + k, (+ll[k] || 0) + 'px');
  }
  let canvas = null, ctx = null;
  if (variant === 'full' || variant === 'short') {
    try { canvas = doc.createElement('canvas'); ctx = typeof Path2D === 'function' && canvas.getContext('2d'); } catch { ctx = null; }
    if (!ctx) { canvas = null; variant = 'reduced'; }
  }
  const tl = TIMELINES[variant] || TIMELINES.off, mach = createMachine({ min: tl.T, cap: CAP, out: tl.out, skipOut: tl.skipOut });
  const logo = root.querySelector('.eb-logo'), word = root.querySelector('.eb-word');
  const stats = { frames: 0, dt: [], draw: [], q: 0, dpr: 0 }, api = win.__emberBoot = { variant, active: true, reason: '', stats };
  const off = [], listen = (el, type, fn, o) => { el.addEventListener(type, fn, o); off.push(() => el.removeEventListener(type, fn, o)); };
  let raf = 0, safety = 0, mo = null, renderer = null, last = 0, target, L = null;

  function finish(reason) {
    if (!api.active) return;
    api.active = false;
    api.reason = reason || mach.m.reason;
    cancelAnimationFrame(raf); raf = 0;
    clearTimeout(safety);
    if (mo) mo.disconnect();
    off.forEach(f => f());
    if (canvas) canvas.width = canvas.height = 0;
    root.remove();
    if (status) status.remove();
    if (variant !== 'none') try { store.setItem(KEYS.seen, '1'); } catch { /* ignore */ }
    try { doc.dispatchEvent(new CustomEvent('ember:bootdone', { detail: { variant, reason: api.reason } })); } catch { /* old */ }
  }
  if (variant === 'none') return finish('none');
  if (status) status.textContent = 'Starting EmberMesh…';

  function layout() {
    const W = win.innerWidth || root.clientWidth, H = win.innerHeight || root.clientHeight, r = logo.getBoundingClientRect();
    // the SVG box paints snapped to whole CSS px: start from exactly where it is drawn
    L = { W, H, S: (r.width || 0.3 * Math.min(W, H)) / 128, cx: r.width ? Math.round(r.left) + r.width / 2 : W / 2, cy: r.width ? Math.round(r.top) + r.height / 2 : H / 2 };
    if (!canvas) return;
    // capped device pixel ratio and pixel count, lower on lower quality levels
    let dpr = Math.min(win.devicePixelRatio || 1, [2, 1.5, 1][stats.q]);
    const budget = [3.2e6, 1.8e6, 0.9e6][stats.q];
    if (W * H * dpr * dpr > budget) dpr = Math.max(0.75, Math.sqrt(budget / (W * H)));
    L.dpr = stats.dpr = dpr;
    canvas.width = Math.round(W * dpr);
    canvas.height = Math.round(H * dpr);
  }

  // the header's own logo, if visible: where the hand-over lands
  function findTarget() {
    const r = (doc.querySelector('.strip .brand .brand-mark') || root).getBoundingClientRect();
    return { mark: r.width && r.width < 200 && r.bottom > 0 && r.top < L.H && r.right > 0 && r.left < L.W ? { x: r.left + r.width / 2, y: r.top + r.height / 2, size: r.width } : null };
  }

  function frame(now) {
    raf = 0;
    if (!api.active) return;
    if (!L) layout();
    if (appReady()) mach.ready();
    let t, p = null, clock;
    if (pause) {
      t = Math.min(pause.t, tl.T); clock = pause.t;
      if (pause.t > tl.T && tl.out) { p = clamp01((pause.t - tl.T) / tl.out); if (target === undefined) target = findTarget(); }
    } else {
      const m = mach.tick(now);
      if (m.phase === 'done') return finish();
      t = m.t; clock = m.el;
      if (m.phase === 'out') {
        p = m.p;
        if (!root.classList.contains('eb-out')) { root.classList.add('eb-out'); target = canvas && findTarget(); }
      }
    }
    const d0 = performance.now();
    if (canvas) {
      if (!renderer) renderer = createRenderer(ctx, info.theme);
      renderer.draw({ f: frameAt(tl, t), ...L, clock, ignite: tl.ignite || 0, out: p, q: stats.q, target });
      canvas.style.opacity = p == null ? '' : String(1 - clamp01((p - 0.7) / 0.3));
      root.classList.add('eb-live');
    } else {
      // calm: the static logo itself, the wordmark fades in, then all fades out
      if (word) word.style.opacity = String(ease.io(frameAt(tl, t).word));
      root.style.opacity = p == null ? '' : String(1 - ease.io(p));
    }
    // measure the first frames; step quality down (particles, resolution) when slow
    stats.frames++;
    if (last) { stats.dt.push(+(now - last).toFixed(2)); stats.draw.push(+(performance.now() - d0).toFixed(2)); }
    last = now;
    const n = stats.dt.length;
    if (canvas && !pause && n >= 14 && stats.q < 2 && !(n % 12)) {
      const med = a => a.slice(-12).sort((x, y) => x - y)[6];
      if (med(stats.dt) > 22 || med(stats.draw) > 9) { stats.q++; layout(); }
    }
    raf = requestAnimationFrame(frame);
  }

  const skip = why => {
    if (!api.active || pause) return;
    mach.skip(performance.now(), why);
    if (!raf) raf = requestAnimationFrame(frame);
  };
  if (variant === 'off') { raf = requestAnimationFrame(() => finish('off')); return; } // one static frame
  if (canvas) { canvas.setAttribute('aria-hidden', 'true'); root.appendChild(canvas); }

  // any input skips; a dialog or toast (an SOS, an error) ends it too
  const pe = 'PointerEvent' in win, sk = () => skip('skip');
  listen(root, pe ? 'pointerdown' : 'mousedown', sk);
  if (!pe) listen(root, 'touchstart', sk, { passive: true });
  listen(win, 'keydown', sk, true);
  listen(win, 'resize', () => { root.classList.add('eb-free'); layout(); });
  listen(doc, 'ember:ready', () => mach.ready());
  listen(doc, 'visibilitychange', () => {
    const n = performance.now();
    if (doc.visibilityState === 'hidden') { mach.pause(n); cancelAnimationFrame(raf); raf = 0; }
    else { mach.resume(n); if (mach.m.ready) mach.skip(n, 'resume'); if (!raf) raf = requestAnimationFrame(frame); }
  });
  if (typeof MutationObserver === 'function') {
    mo = new MutationObserver(recs => {
      if (appReady()) mach.ready();
      for (const r of recs) for (const el of r.addedNodes) if (el.nodeType === 1 && (r.target.id === 'toasts' || /\b(flash-)?overlay\b/.test(el.className))) skip('interrupt');
    });
    for (const el of [main, doc.body, doc.getElementById('toasts')]) if (el) mo.observe(el, { childList: true });
  }
  if (pause) api.seek = ms => { pause.t = +ms || 0; target = undefined; };
  else safety = setTimeout(() => finish('safety'), CAP + 1800);
  api.skip = sk;
  // a native launch screen covers the page for a few frames after first paint: hold the still
  // image until the host says it is gone ('ember:launch-removed') or ~140 ms
  let hold = ll && !pause ? 140 : 0;
  listen(win, 'ember:launch-removed', () => { hold = 0; });
  mach.start(t0);
  const begin = now => {
    raf = 0;
    if (!api.active) return;
    if (hold && now - t0 < hold) { raf = requestAnimationFrame(begin); return; }
    if (mach.m.phase === 'play') mach.start(now);
    frame(now);
  };
  raf = requestAnimationFrame(begin);
}
