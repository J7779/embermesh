// Accessibility plumbing shared by every screen, without touching the screens:
//   announce(text, {assertive})   screen-reader live region, rate limited (floods never spam)
//   initA11y(app)                 skip link, live regions, focus on route change (heading /
//                                 restore on back), modal sheets make the page inert and keep focus,
//                                 keyboard shortcuts (g + letter, ? for the list), event
//                                 announcements, haptic taps, html[data-route]
//   haptic(kind)                  host.haptic {kind} when the host has it and the user wants it
//   focusHeading(root), saveFocus(key), restoreFocus(key, root), trapFocus(box, e)
// Pure parts (createAnnouncer, shortcutTarget, focusDescriptor matching, SHORTCUTS) are unit
// tested in ui/tests/a11y.test.js.
import { getPrefs, onPrefsChange } from './prefs.js';

// ------------------------------------------------------------------------------ announcer (pure)
/**
 * Live-region announcer with flood control. write(text, assertive) does the DOM work.
 * - the same text within dedupeMs is dropped;
 * - at most maxPerWindow polite (maxAssertive assertive) announcements per windowMs; the rest are
 *   counted and summarised once the window has room again ("3 more updates").
 */
export function createAnnouncer({ write, now = () => Date.now(), schedule = (fn, ms) => setTimeout(fn, ms), windowMs = 10000, maxPerWindow = 3, maxAssertive = 2, dedupeMs = 5000 } = {}) {
  const recent = { polite: [], assertive: [] };
  const lastText = new Map();
  let dropped = 0;
  let summaryTimer = null;
  const st = {
    spoken: 0, suppressed: 0,
    announce(text, opts = {}) {
      const s = String(text == null ? '' : text).replace(/\s+/g, ' ').trim().slice(0, 300);
      if (!s) return false;
      const t = now();
      const kind = opts.assertive ? 'assertive' : 'polite';
      for (const k of ['polite', 'assertive']) recent[k] = recent[k].filter(x => t - x < windowMs);
      for (const [k, v] of lastText) if (t - v >= dedupeMs) lastText.delete(k);
      if (lastText.has(s)) { st.suppressed++; return false; }
      const cap = kind === 'assertive' ? maxAssertive : maxPerWindow;
      if (recent[kind].length >= cap) {
        st.suppressed++; dropped++;
        if (!summaryTimer) {
          const wait = Math.max(50, windowMs - (t - recent.polite[0] || 0));
          summaryTimer = schedule(() => {
            summaryTimer = null;
            const n = dropped; dropped = 0;
            if (n) { const tt = now(); recent.polite = recent.polite.filter(x => tt - x < windowMs); recent.polite.push(tt); st.spoken++; write(n === 1 ? '1 more update' : n + ' more updates', false); }
          }, wait);
        }
        return false;
      }
      recent[kind].push(t);
      lastText.set(s, t);
      st.spoken++;
      write(s, kind === 'assertive');
      return true;
    },
  };
  return st;
}

// ------------------------------------------------------------------------------ shortcuts (pure)
/** g + key → destination. Also listed in Settings → Accessibility and by pressing "?". */
export const SHORTCUTS = [
  ['c', '#/chats', 'Chats (Messages)'], ['p', '#/people', 'People'], ['s', '#/sos', 'SOS (Send for help)'],
  ['b', '#/board', 'Bulletins'], ['l', '#/links', 'Links (Connections)'], ['g', '#/guides', 'Guides'],
  ['i', '#/help', 'Instructions'], ['t', '#/tools', 'Tools'], ['n', '#/extras', 'Notes & lists'],
  ['o', '#/settings', 'Settings'], ['a', '#/settings/accessibility', 'Accessibility settings'], ['m', '#/more', 'More'],
];
export const SINGLE_KEYS = [['?', 'Show these shortcuts'], ['/', 'Search on this screen'], ['Esc', 'Close a dialog']];

/** Pure: the destination for the second key of a "g" sequence, or null. */
export function shortcutTarget(key) {
  const k = String(key || '').toLowerCase();
  const hit = SHORTCUTS.find(s => s[0] === k);
  return hit ? hit[1] : null;
}

/** Pure: true when a key event target is a text field (shortcuts must not steal typing). */
export function isTyping(el) {
  if (!el || !el.tagName) return false;
  const t = el.tagName.toLowerCase();
  if (t === 'textarea' || t === 'select' || el.isContentEditable) return true;
  if (t !== 'input') return false;
  return !['checkbox', 'radio', 'button', 'submit', 'reset', 'range', 'color', 'file'].includes(String(el.type || '').toLowerCase());
}

// ------------------------------------------------------------------------------ focus helpers
/** A way to find "the same" control again after a screen re-renders. Pure on element-like objects. */
export function focusDescriptor(el) {
  if (!el || !el.tagName) return null;
  const href = el.getAttribute && el.getAttribute('href');
  if (href) return { href };
  if (el.id && !/^(sheet|id)-/.test(el.id)) return { id: el.id };
  const label = (el.getAttribute && el.getAttribute('aria-label')) || String(el.textContent || '').trim().slice(0, 80);
  return label ? { tag: el.tagName.toLowerCase(), label } : null;
}

function findByDescriptor(d, root) {
  if (!d || !root) return null;
  if (d.href) return [...root.querySelectorAll('a[href]')].find(a => a.getAttribute('href') === d.href) || null;
  if (d.id) return root.querySelector('#' + CSS.escape(d.id));
  return [...root.querySelectorAll(d.tag)].find(e => (e.getAttribute('aria-label') || String(e.textContent || '').trim().slice(0, 80)) === d.label) || null;
}

const visible = el => !!el && el.isConnected && (el.offsetParent !== null || getComputedStyle(el).position === 'fixed');

/** Move focus to the screen's heading (h1, else first h2, else #main) without scrolling. */
export function focusHeading(root = document.getElementById('main')) {
  if (!root) return null;
  const hd = [...root.querySelectorAll('h1, h2')].find(visible) || root;
  if (!hd.hasAttribute('tabindex')) hd.setAttribute('tabindex', '-1');
  try { hd.focus({ preventScroll: true }); } catch { hd.focus(); }
  return hd;
}

const saved = new Map();
export function saveFocus(key, el = document.activeElement) {
  const d = focusDescriptor(el);
  if (d) saved.set(key, d); else saved.delete(key);
  if (saved.size > 50) saved.delete(saved.keys().next().value);
}
export function restoreFocus(key, root = document.getElementById('main')) {
  const el = findByDescriptor(saved.get(key), root);
  if (!el || !visible(el)) return false;
  try { el.focus({ preventScroll: false }); } catch { el.focus(); }
  return document.activeElement === el;
}

const FOCUSABLE = 'a[href], button:not([disabled]), input:not([disabled]):not([type="hidden"]), select:not([disabled]), textarea:not([disabled]), summary, [tabindex]:not([tabindex="-1"])';
/** Keep Tab inside `box` (modal sheets). Returns true when it moved focus. */
export function trapFocus(box, e) {
  const els = [...box.querySelectorAll(FOCUSABLE)].filter(visible);
  if (!els.length) { e.preventDefault(); return true; }
  const first = els[0], last = els[els.length - 1];
  if (e.shiftKey && (document.activeElement === first || !box.contains(document.activeElement))) { last.focus(); e.preventDefault(); return true; }
  if (!e.shiftKey && (document.activeElement === last || !box.contains(document.activeElement))) { first.focus(); e.preventDefault(); return true; }
  return false;
}

// ------------------------------------------------------------------------------ DOM announcer
let regions = null;
function ensureRegions() {
  if (regions || typeof document === 'undefined' || !document.body) return regions;
  const mk = (live) => {
    const el = document.createElement('div');
    el.className = 'a11y-live';
    el.setAttribute('aria-live', live);
    el.setAttribute('aria-atomic', 'true');
    if (live === 'assertive') el.setAttribute('role', 'alert'); else el.setAttribute('role', 'status');
    el.dataset.a11yLive = live;
    document.body.appendChild(el);
    return el;
  };
  regions = { polite: mk('polite'), assertive: mk('assertive') };
  return regions;
}

const domAnnouncer = createAnnouncer({
  write(text, assertive) {
    const r = ensureRegions();
    if (!r) return;
    const el = assertive ? r.assertive : r.polite;
    // Clear first so repeating the same words is announced again; a short delay lets AT notice.
    el.textContent = '';
    setTimeout(() => { el.textContent = text; }, 60);
    if (typeof window !== 'undefined') (window.__emberAnnounced = window.__emberAnnounced || []).push({ text, assertive, ts: Date.now() });
  },
});

/** Speak `text` to screen readers. opts.assertive interrupts (SOS, failures). Rate limited. */
export function announce(text, opts = {}) { return domAnnouncer.announce(text, opts); }

// ------------------------------------------------------------------------------ haptics
let app = null;
let hapticState = 'unknown'; // unknown | host | vibrate | none
/** Whether this device can give tactile feedback (host.haptic or the Vibration API). */
export function hapticSupported(a = app) {
  const hi = a && a.store && a.store.get('host');
  if (hi && Array.isArray(hi.features) && hi.features.includes('haptic')) return true;
  if (hapticState === 'host') return true;
  return typeof navigator !== 'undefined' && typeof navigator.vibrate === 'function' && !!(window.matchMedia && window.matchMedia('(pointer: coarse)').matches);
}
const VIBRATE = { tap: 10, success: [20, 60, 20], warning: [40, 80, 40], error: [80, 60, 80] };
/** Tactile feedback; does nothing when switched off, unsupported or the motion level is off-limits. */
export async function haptic(kind = 'tap') {
  if (getPrefs().haptics === false || hapticState === 'none') return false;
  const k = VIBRATE[kind] ? kind : 'tap';
  if (app && hapticState !== 'vibrate') {
    try { await app.api.call('host.haptic', { kind: k }); hapticState = 'host'; return true; } catch (e) {
      if (!e || !['unsupported', 'not_implemented'].includes(e.code)) return false;
      hapticState = 'vibrate';
    }
  }
  try {
    if (typeof navigator !== 'undefined' && navigator.vibrate && hapticSupported()) { navigator.vibrate(VIBRATE[k]); return true; }
  } catch { /* ignore */ }
  hapticState = 'none';
  return false;
}

// ------------------------------------------------------------------------------ route tracking
function parseRoute(hash = location.hash) {
  const raw = String(hash || '').replace(/^#\/?/, '').split('?')[0];
  const parts = raw.split('/').filter(Boolean);
  return { name: parts[0] || 'chats', sub: parts[1] || '', depth: parts.length, key: '#/' + raw };
}

function setRouteAttr() {
  const r = parseRoute();
  const root = document.documentElement;
  if (root.dataset.route !== r.name) root.dataset.route = r.name;
  if (root.dataset.subroute !== r.sub) root.dataset.subroute = r.sub;
  return r;
}

/** Waits until #main shows a new screen (a heading appears or content settles), max `ms`. */
function whenRendered(main, ms = 2500) {
  return new Promise(resolve => {
    let done = false;
    const finish = () => { if (done) return; done = true; obs.disconnect(); clearTimeout(t1); clearTimeout(t2); resolve(); };
    let t2 = 0;
    const settle = () => { clearTimeout(t2); t2 = setTimeout(finish, 250); };
    const obs = new MutationObserver(() => { if (main.querySelector('h1')) settle(); });
    obs.observe(main, { childList: true, subtree: true });
    const t1 = setTimeout(finish, ms);
    if (main.querySelector('h1') && !main.querySelector('#boot')) settle();
  });
}

// ------------------------------------------------------------------------------ init
let installed = false;
/**
 * Install everything once. `app` is the app context from app.js (api, store, backend, nav).
 * Idempotent; returns a small API for tests.
 */
export function initA11y(a) {
  app = a || app;
  if (installed || typeof document === 'undefined') return api;
  installed = true;
  ensureRegions();
  installSkipLink();
  setRouteAttr();
  installRouteFocus();
  installDialogGuard();
  installShortcuts();
  installHapticTaps();
  if (app && app.backend) installEventAnnouncements(app);
  installInterim(app);
  return api;
}

function installSkipLink() {
  if (document.querySelector('.skip-link')) return;
  const a = document.createElement('a');
  a.className = 'skip-link';
  a.href = '#main';
  a.textContent = 'Skip to content';
  a.addEventListener('click', e => { e.preventDefault(); focusHeading(); });
  document.body.insertBefore(a, document.body.firstChild);
  const main = document.getElementById('main');
  if (main && !main.hasAttribute('tabindex')) main.setAttribute('tabindex', '-1');
}

let lastKey = null;
function installRouteFocus() {
  lastKey = parseRoute().key;
  // Remember the focused control continuously so it is known when the hash changes.
  let lastFocus = null;
  document.addEventListener('focusin', e => { if (document.getElementById('main')?.contains(e.target)) lastFocus = e.target; }, true);
  window.addEventListener('hashchange', async () => {
    const prevKey = lastKey;
    if (lastFocus && prevKey) saveFocus(prevKey, lastFocus);
    const r = setRouteAttr();
    lastKey = r.key;
    const main = document.getElementById('main');
    if (!main) return;
    const before = document.activeElement;
    await whenRendered(main);
    if (r.key !== lastKey) return; // navigated again meanwhile
    if (document.querySelector('.overlay')) return;
    const ae = document.activeElement;
    // The screen moved focus itself (e.g. the chat composer): respect it.
    if (ae && ae !== before && ae !== document.body && main.contains(ae) && !/^H[12]$/.test(ae.tagName)) return;
    if (saved.has(r.key) && restoreFocus(r.key, main)) return;
    focusHeading(main);
  });
}

function installDialogGuard() {
  const appEl = () => document.getElementById('app');
  const topOverlay = () => { const o = document.querySelectorAll('.overlay'); return o.length ? o[o.length - 1] : null; };
  let opener = null;
  const sync = () => {
    const ov = topOverlay();
    const el = appEl();
    if (!el) return;
    if (ov && !el.hasAttribute('inert')) {
      const ae = document.activeElement;
      opener = ae && !ov.contains(ae) && ae !== document.body ? ae : opener;
      el.setAttribute('inert', '');
    } else if (!ov && el.hasAttribute('inert')) {
      el.removeAttribute('inert');
      // The sheet tried to give focus back while the page was still inert: do it now.
      const ae = document.activeElement;
      if (opener && opener.isConnected && (!ae || ae === document.body)) { try { opener.focus({ preventScroll: true }); } catch { /* ignore */ } }
      opener = null;
    }
  };

  new MutationObserver(sync).observe(document.body, { childList: true });
  document.addEventListener('focusin', e => {
    const ov = topOverlay();
    if (!ov || ov.contains(e.target) || e.target.closest?.('.toasts, .skip-link')) return;
    const f = [...ov.querySelectorAll(FOCUSABLE)].find(visible);
    if (f) f.focus();
  });
  // Tab handling for sheets that do not trap themselves; Escape closes via the sheet's own handler.
  document.addEventListener('keydown', e => {
    if (e.key !== 'Tab') return;
    const ov = topOverlay();
    if (ov && !ov.contains(document.activeElement)) trapFocus(ov.querySelector('[role="dialog"]') || ov, e);
  });
}

let gPending = 0;
function installShortcuts() {
  document.addEventListener('keydown', e => {
    if (e.defaultPrevented || e.ctrlKey || e.metaKey || e.altKey) return;
    if (getPrefs().shortcuts === false) return;
    if (isTyping(e.target) || document.querySelector('.overlay')) return;
    if (document.getElementById('app')?.hasAttribute('inert')) return;
    const nav = document.getElementById('nav');
    if (nav && nav.hidden) return; // lock screen / first-run setup
    const k = e.key;
    if (gPending && Date.now() - gPending < 1500) {
      gPending = 0;
      const to = shortcutTarget(k);
      if (to) { e.preventDefault(); if (location.hash === to && app) app.nav(to); else location.hash = to; }
      return;
    }
    gPending = 0;
    if (k === 'g') { gPending = Date.now(); return; }
    if (k === '?') { e.preventDefault(); showShortcuts(); return; }
    if (k === '/') {
      const s = document.querySelector('#main input[type="search"]:not([disabled])');
      if (s) { e.preventDefault(); s.focus(); }
    }
  });
}

/** The shortcut list (also linked from Settings → Accessibility). */
export async function showShortcuts() {
  const { sheet } = await import('./ui.js');
  const { h } = await import('./dom.js');
  const dl = h('dl', { class: 'kbd-list' },
    SHORTCUTS.map(([k, , label]) => [h('dt', null, h('kbd', null, 'g'), ' ', h('kbd', null, k)), h('dd', null, label)]),
    SINGLE_KEYS.map(([k, label]) => [h('dt', null, h('kbd', null, k)), h('dd', null, label)]));
  sheet('Keyboard shortcuts', h('div', { class: 'stack' },
    h('p', { class: 'muted' }, 'Press g, then the letter. Shortcuts do nothing while you are typing in a text field. You can turn them off in Settings → Accessibility.'), dl));
}

function installHapticTaps() {
  document.addEventListener('click', e => {
    const b = e.target && e.target.closest && e.target.closest('.btn, .tabbar a, .item');
    if (b && e.isTrusted && e.detail > 0 && hapticSupported()) haptic('tap');
  }, true);
}

function installEventAnnouncements(a) {
  const b = a.backend;
  const on = (ev, fn) => { try { b.on(ev, d => { try { fn(d); } catch (e) { console.error(e); } }); } catch { /* ignore */ } };
  let sosActive = null;
  let acks = 0;
  on('sos.updated', d => {
    if (!d) return;
    const act = !!d.active;
    if (sosActive !== null && act !== sosActive) {
      announce(act ? 'Your SOS is on. It repeats until you stop it.' : 'Your SOS is off.', { assertive: true });
      haptic(act ? 'success' : 'tap');
    }
    const n = (d.acked_by || []).length;
    if (act && n > acks) announce(n === 1 ? '1 person heard your SOS' : n + ' people heard your SOS', { assertive: true });
    sosActive = act; acks = n;
  });
  on('sos.received', d => { if (d && d.signed_ok !== false && d.verified !== false) haptic('warning'); });
  on('message.status', d => {
    if (!d || d.conv !== a.currentConv) return;
    if (d.status === 'failed') { announce('Message not sent.', { assertive: true }); haptic('error'); }
    else if (d.status === 'delivered') announce('Message delivered.');
  });
  on('link.state', d => {
    const l = d && d.link;
    if (!l || !l.name) return;
    if (l.state === 'up') announce('Connection ' + l.name + ' is working.');
    else if (l.state === 'error' || l.state === 'down') announce('Connection ' + l.name + ' stopped.');
  });
  on('security.locked', () => announce('EmberMesh is locked.'));
}

// ------------------------------------------------------------------------------ INTERIM wiring
// Until settings.js / setup.js carry the hooks themselves (see the report in ui/README.md →
// Accessibility), put the Accessibility entry at the top of Settings, render
// #/settings/accessibility, and offer Simple mode on the first setup screen. Each step does
// nothing once the owning screen provides the element itself.
let interimCleanup = null;
function installInterim(a) {
  const main = document.getElementById('main');
  if (!main) return;
  let busy = false;
  const check = async () => {
    if (busy) return;
    busy = true;
    try {
      const r = parseRoute();
      const nav = document.getElementById('nav');
      const setupEl = main.querySelector('.setup');
      if (setupEl && setupEl.querySelector('#setup-name') && !setupEl.querySelector('.simple-offer')) {
        const { simpleOffer } = await import('../screens/accessibility.js');
        const card = setupEl.querySelector('.card');
        if (card && !setupEl.querySelector('.simple-offer')) card.after(simpleOffer());
      }
      if (nav && nav.hidden) return;
      const h1 = main.querySelector('.page-head h1');
      if (r.name !== 'settings' || !h1) return;
      if (r.sub === 'accessibility' && !main.querySelector('.a11y-page') && h1.textContent.trim() === 'Settings') {
        const mod = await import('../screens/accessibility.js');
        if (parseRoute().sub !== 'accessibility') return;
        while (main.firstChild) main.removeChild(main.firstChild);
        main.scrollTop = 0;
        interimCleanup = (await mod.default(a, { rest: ['accessibility'], name: 'settings' }, main)) || null;
        focusHeading(main);
      } else if (!r.sub && !main.querySelector('.a11y-entry')) {
        const { settingsEntry } = await import('../screens/accessibility.js');
        const head = main.querySelector('.page-head');
        if (head && !main.querySelector('.a11y-entry')) head.after(settingsEntry());
      }
    } catch (e) { console.error('a11y interim', e); } finally { busy = false; }
  };
  let raf = 0;
  new MutationObserver(() => { if (!raf) raf = requestAnimationFrame(() => { raf = 0; check(); }); }).observe(main, { childList: true, subtree: true });
  window.addEventListener('hashchange', () => { if (interimCleanup) { try { interimCleanup(); } catch { /* ignore */ } interimCleanup = null; } });
  check();
}

const api = { announce, focusHeading, saveFocus, restoreFocus, haptic, showShortcuts };

// Re-evaluate haptic availability when preferences change (e.g. turned back on).
onPrefsChange((p, _e, changed) => { if (changed && changed.includes('haptics') && p.haptics && hapticState === 'none') hapticState = 'unknown'; });
