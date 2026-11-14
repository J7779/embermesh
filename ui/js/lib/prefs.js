// Appearance, motion and accessibility preferences (docs/RPC_API.md §8).
//
//   import { initPrefs, getPrefs, setPref, onPrefsChange, navItems } from './lib/prefs.js';
//   await initPrefs(backend);               // once, after createBackend()
//   setPref('simple_mode', true);           // saves to kv `ui` + local storage, re-applies
//   onPrefsChange((prefs, eff, changed) => …);   // returns unsubscribe
//   navItems(NAV)                           // destinations to show (Simple mode: the essentials)
//
// Storage: kv namespace `ui` (authoritative once the core is up) mirrored in local storage under
// `ember.ui.<key>` as JSON, so the start-up animation and the first paint can apply them before
// the core has started. Importing this module applies the mirrored values immediately.
//
// The effective state is reflected on <html>:
//   data-simple="true|false"   data-motion="full|reduced|off"   data-contrast="high|normal"
//   data-spacing="wide|normal" data-transparency="reduced|normal"  data-textsize="normal|large|xlarge"
//   style --text-scale: <number>
// and everything those attributes switch lives in css/a11y.css.
//
// Precedence (pure, see effectivePrefs): an explicit preference wins over the platform, except
// that low-power mode and Simple mode cap motion at "reduced", and the test hooks (?motion=off,
// navigator.webdriver) force motion off. `motion:"system"`, `high_contrast:null` follow the
// platform (host.info.accessibility, else the browser's media queries).

export const PREF_KEYS = ['simple_mode', 'motion', 'text_scale', 'high_contrast', 'boot_animation', 'haptics', 'reading_spacing', 'shortcuts'];
export const MIRROR_PREFIX = 'ember.ui.';
export const KV_NS = 'ui';
export const TEXT_SCALE_MIN = 0.85;
export const TEXT_SCALE_MAX = 1.6;
export const MOTIONS = ['system', 'full', 'reduced', 'off'];
export const BOOT_ANIMATIONS = ['full', 'short', 'off'];

/** Defaults. high_contrast null = follow the platform setting. */
export const PREF_DEFAULTS = Object.freeze({
  simple_mode: false, motion: 'system', text_scale: 1, high_contrast: null, boot_animation: 'full',
  haptics: true, reading_spacing: false, shortcuts: true,
});

/** Validates one value; returns the cleaned value or undefined when it is not acceptable. */
export function cleanPref(key, v) {
  switch (key) {
    case 'simple_mode': case 'haptics': case 'reading_spacing': case 'shortcuts':
      return typeof v === 'boolean' ? v : undefined;
    case 'high_contrast': return v === null || typeof v === 'boolean' ? v : undefined;
    case 'motion': return MOTIONS.includes(v) ? v : undefined;
    case 'boot_animation': return BOOT_ANIMATIONS.includes(v) ? v : undefined;
    case 'text_scale': {
      const n = typeof v === 'string' && v.trim() !== '' ? Number(v) : v;
      if (typeof n !== 'number' || !Number.isFinite(n)) return undefined;
      return Math.round(Math.min(TEXT_SCALE_MAX, Math.max(TEXT_SCALE_MIN, n)) * 100) / 100;
    }
    default: return undefined;
  }
}

/** Merge sources left to right (later wins), keeping only valid values. Pure. */
export function mergePrefs(...sources) {
  const out = { ...PREF_DEFAULTS };
  for (const s of sources) {
    if (!s || typeof s !== 'object') continue;
    for (const k of PREF_KEYS) {
      if (!(k in s)) continue;
      const v = cleanPref(k, s[k]);
      if (v !== undefined) out[k] = v;
    }
  }
  return out;
}

/**
 * Effective state from preferences + environment. Pure.
 * env: {reduceMotion, prefersContrast:'more'|'less'|'no-preference', forcedColors, reduceTransparency,
 *       lowPower, forceMotionOff, host:{reduce_motion?, high_contrast?, font_scale?, screen_reader?, bold_text?}}
 * Returns {simple, motion, contrast, textScale, spacing, transparency, follows:{motion, contrast}, sources:{…}}.
 */
export function effectivePrefs(prefs, env = {}) {
  const p = mergePrefs(prefs);
  const host = env.host || {};
  const sysReduce = typeof host.reduce_motion === 'boolean' ? host.reduce_motion : !!env.reduceMotion;
  const sysHigh = typeof host.high_contrast === 'boolean' ? host.high_contrast || !!env.forcedColors : (env.prefersContrast === 'more' || !!env.forcedColors);
  let motion = p.motion === 'system' ? (sysReduce ? 'reduced' : 'full') : p.motion;
  let motionWhy = p.motion === 'system' ? (sysReduce ? 'system' : 'default') : 'preference';
  if (motion === 'full' && (env.lowPower || p.simple_mode)) { motion = 'reduced'; motionWhy = env.lowPower ? 'low_power' : 'simple_mode'; }
  if (env.forceMotionOff) { motion = 'off'; motionWhy = 'test'; }
  const contrast = (p.high_contrast === null ? sysHigh : p.high_contrast) ? 'high' : 'normal';
  return {
    simple: p.simple_mode,
    motion,
    contrast,
    textScale: p.text_scale,
    // effective size class: Simple mode adds 12.5 % on top of the scale
    textSize: (p.text_scale * (p.simple_mode ? 1.125 : 1)) >= 1.4 ? 'xlarge' : (p.text_scale * (p.simple_mode ? 1.125 : 1)) >= 1.15 ? 'large' : 'normal',
    spacing: p.reading_spacing ? 'wide' : 'normal',
    transparency: env.reduceTransparency || contrast === 'high' ? 'reduced' : 'normal',
    forcedColors: !!env.forcedColors,
    screenReader: !!host.screen_reader,
    follows: { motion: p.motion === 'system', contrast: p.high_contrast === null },
    system: { reduceMotion: sysReduce, highContrast: sysHigh, fontScale: Number(host.font_scale) || 1, boldText: !!host.bold_text },
    why: { motion: motionWhy },
  };
}

/**
 * Navigation destinations. Normally all items unchanged. In Simple mode only Chats, SOS, Guides,
 * Instructions and Settings (plus "More", which lists everything else) with plain-wording labels;
 * on phones Settings is reached from More so the bar keeps five large targets.
 * items: [{id, label, icon, wide?, narrow?, cls?}] (app.js NAV). Returns new objects; the original
 * label stays in `fullLabel`.
 */
export const SIMPLE_NAV = ['chats', 'sos', 'guides', 'help', 'settings', 'more'];
export const SIMPLE_LABELS = { chats: 'Messages', sos: 'Send for help', guides: 'Guides', help: 'How to use', settings: 'Settings', more: 'More' };
export function navItems(items, simple = current.prefs.simple_mode) {
  const list = Array.isArray(items) ? items : [];
  if (!simple) return list.slice();
  const byId = new Map(list.map(i => [i.id, i]));
  const out = [];
  for (const id of SIMPLE_NAV) {
    const it = byId.get(id);
    if (!it) continue;
    const n = { ...it, fullLabel: it.label, label: SIMPLE_LABELS[id] || it.label, simple: true };
    delete n.narrow; delete n.wide;
    if (id === 'settings') n.wide = true;       // phones: Settings is the first entry in More
    out.push(n);
  }
  return out;
}

// ------------------------------------------------------------------------------------ runtime
const hasDoc = typeof document !== 'undefined' && typeof window !== 'undefined';
const listeners = new Set();
const current = { prefs: { ...PREF_DEFAULTS }, eff: null, host: null, backend: null, lowPower: false };
let mqs = null;

function storage() { try { return hasDoc ? window.localStorage : null; } catch { return null; } }

/** Reads the local-storage mirror. */
export function readMirror(ls = storage()) {
  const out = {};
  if (!ls) return out;
  for (const k of PREF_KEYS) {
    try {
      const raw = ls.getItem(MIRROR_PREFIX + k);
      if (raw === null) continue;
      out[k] = JSON.parse(raw);
    } catch { /* bad JSON or blocked */ }
  }
  return out;
}

/** Writes the mirror (JSON values; high_contrast null = follow the platform). */
export function writeMirror(prefs, ls = storage()) {
  if (!ls) return;
  for (const k of PREF_KEYS) {
    try { ls.setItem(MIRROR_PREFIX + k, JSON.stringify(prefs[k] === undefined ? null : prefs[k])); } catch { /* full / blocked */ }
  }
}

function query(q) { try { return hasDoc && window.matchMedia ? window.matchMedia(q) : null; } catch { return null; } }

function readEnv() {
  if (!hasDoc) return { host: current.host || {} };
  if (!mqs) {
    mqs = {
      reduce: query('(prefers-reduced-motion: reduce)'),
      more: query('(prefers-contrast: more)'),
      less: query('(prefers-contrast: less)'),
      forced: query('(forced-colors: active)'),
      transp: query('(prefers-reduced-transparency: reduce)'),
    };
    for (const m of Object.values(mqs)) {
      if (!m) continue;
      const f = () => apply();
      if (m.addEventListener) m.addEventListener('change', f); else if (m.addListener) m.addListener(f);
    }
  }
  let forceMotionOff = false;
  try { forceMotionOff = /[?&]motion=off\b/.test(location.search) || !!navigator.webdriver; } catch { /* ignore */ }
  return {
    reduceMotion: !!(mqs.reduce && mqs.reduce.matches),
    prefersContrast: mqs.more && mqs.more.matches ? 'more' : mqs.less && mqs.less.matches ? 'less' : 'no-preference',
    forcedColors: !!(mqs.forced && mqs.forced.matches),
    reduceTransparency: !!(mqs.transp && mqs.transp.matches),
    lowPower: document.documentElement.dataset.lowpower === '1',
    forceMotionOff,
    host: current.host || {},
  };
}

/** Reflect the effective state on <html>. Returns the effective state. */
function apply(changed = []) {
  const eff = effectivePrefs(current.prefs, readEnv());
  const prev = current.eff;
  current.eff = eff;
  if (hasDoc) {
    const r = document.documentElement;
    const set = (k, v) => { if (r.dataset[k] !== v) r.dataset[k] = v; };
    set('simple', String(eff.simple));
    set('motion', eff.motion);
    set('contrast', eff.contrast);
    set('spacing', eff.spacing);
    set('transparency', eff.transparency);
    set('textsize', eff.textSize);
    if (r.style.getPropertyValue('--text-scale') !== String(eff.textScale)) r.style.setProperty('--text-scale', String(eff.textScale));
  }
  const effChanged = !prev || ['simple', 'motion', 'contrast', 'textScale', 'textSize', 'spacing', 'transparency', 'forcedColors', 'screenReader'].some(k => prev[k] !== eff[k]);
  if (changed.length || effChanged) {
    for (const fn of [...listeners]) { try { fn(getPrefs(), eff, changed); } catch (e) { console.error('prefs listener failed', e); } }
  }
  return eff;
}

/** Current preferences (copy) plus `effective` (the state reflected on <html>). */
export function getPrefs() {
  return { ...current.prefs, effective: current.eff || effectivePrefs(current.prefs, readEnv()) };
}

/** Subscribe to preference or effective-state changes: fn(prefs, effective, changedKeys). */
export function onPrefsChange(fn) {
  listeners.add(fn);
  return () => listeners.delete(fn);
}

/** Set one preference: validates, applies, mirrors, saves to kv `ui` (best effort). */
export async function setPref(key, value) {
  if (!PREF_KEYS.includes(key)) throw Object.assign(new Error('unknown preference ' + key), { code: 'bad_params' });
  const v = cleanPref(key, value);
  if (v === undefined) throw Object.assign(new Error('invalid value for ' + key), { code: 'bad_params' });
  if (Object.is(current.prefs[key], v)) return v;
  current.prefs = { ...current.prefs, [key]: v };
  writeMirror(current.prefs);
  apply([key]);
  if (current.backend) {
    try { await current.backend.call('kv.set', { ns: KV_NS, key, value: v }); } catch { /* locked / no kv: the mirror keeps it */ }
  }
  return v;
}

async function loadKv(backend) {
  const out = {};
  await Promise.all(PREF_KEYS.map(async k => {
    try {
      const r = await backend.call('kv.get', { ns: KV_NS, key: k });
      // null = absent (high_contrast null = follow the platform, which is also the default)
      if (r && r.value !== null && r.value !== undefined) out[k] = r.value;
    } catch { /* locked or unavailable */ }
  }));
  return out;
}

let inited = null;
let boundTo = null;
/**
 * Load (mirror → kv `ui` → host hints → media queries), apply and keep in sync. Safe to call more
 * than once (re-reads kv, e.g. after unlock). Never rejects.
 */
export function initPrefs(backend) {
  if (backend) current.backend = backend;
  const b = current.backend;
  const run = (async () => {
    ensureStylesheet();
    current.prefs = mergePrefs(current.prefs, readMirror());
    apply();
    if (!b) return getPrefs();
    try {
      const hi = await b.call('host.info', {});
      if (hi && hi.accessibility && typeof hi.accessibility === 'object') current.host = { ...hi.accessibility };
    } catch { /* no host.info */ }
    const kv = await loadKv(b);
    const before = current.prefs;
    current.prefs = mergePrefs(before, kv);
    const changed = PREF_KEYS.filter(k => !Object.is(before[k], current.prefs[k]));
    writeMirror(current.prefs);
    // Preferences chosen while the core was locked live only in the mirror: push them to kv.
    for (const k of PREF_KEYS) if (!(k in kv) && current.prefs[k] !== null && !Object.is(current.prefs[k], PREF_DEFAULTS[k])) b.call('kv.set', { ns: KV_NS, key: k, value: current.prefs[k] }).catch(() => {});
    apply(changed);
    if (boundTo !== b) {
      boundTo = b;
      try {
        b.on('host.accessibility', d => { if (d && typeof d === 'object') { current.host = { ...(current.host || {}), ...d }; apply(['host']); } });
        b.on('node.ready', () => { initPrefs(); });
      } catch { /* backend without events */ }
    }
    return getPrefs();
  })().catch(e => { console.error('prefs', e); return getPrefs(); });
  inited = inited || run;
  return run;
}

/** Test hook: replace host hints (as a host.accessibility event would). */
export function _setHostHints(h) { current.host = h ? { ...h } : null; return apply(['host']); }
/** Test hook: reset module state. */
export function _reset() { current.prefs = { ...PREF_DEFAULTS }; current.host = null; current.backend = null; current.eff = null; inited = null; boundTo = null; listeners.clear(); }

// ------------------------------------------------------------------------- stylesheet fallback
const PROBE = '--a11y-css';
/**
 * css/a11y.css must be linked by index.html. Until it is, add it here: a <link> next to the
 * page (http/https/file), or — in the single-file build, where every asset is embedded — a
 * constructable stylesheet from the embedded copy (CSSOM, allowed by the CSP).
 */
export function ensureStylesheet() {
  if (!hasDoc || document.querySelector('link[data-a11y-css], link[href$="a11y.css"]')) return;
  const single = window.__EMBER_SINGLE__;
  if (single && single.assets) {
    const a = single.assets['css/a11y.css'];
    if (!a || !('adoptedStyleSheets' in document) || typeof CSSStyleSheet !== 'function') return;
    try {
      if (getComputedStyle(document.documentElement).getPropertyValue(PROBE).trim()) return; // inlined already
      const b64 = a.b64 !== undefined ? a.b64 : (document.getElementById(a.el) || {}).textContent || '';
      const bin = atob(b64.replace(/\s+/g, ''));
      const bytes = new Uint8Array(bin.length);
      for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
      const sheet = new CSSStyleSheet();
      sheet.replaceSync(new TextDecoder().decode(bytes));
      document.adoptedStyleSheets = [...document.adoptedStyleSheets, sheet];
    } catch (e) { console.warn('a11y.css not applied', e); }
    return;
  }
  try {
    if (getComputedStyle(document.documentElement).getPropertyValue(PROBE).trim()) return;
  } catch { /* ignore */ }
  const link = document.createElement('link');
  link.rel = 'stylesheet';
  link.setAttribute('data-a11y-css', '');
  link.href = new URL('../../css/a11y.css', import.meta.url).href;
  document.head.appendChild(link);
}

// Apply the mirrored preferences as soon as the module is evaluated (first paint).
if (hasDoc) {
  try {
    current.prefs = mergePrefs(readMirror());
    apply();
    ensureStylesheet();
    // Low-power mode is set by app.js as html[data-lowpower]; follow it.
    if (typeof MutationObserver === 'function') new MutationObserver(() => apply()).observe(document.documentElement, { attributes: true, attributeFilter: ['data-lowpower'] });
  } catch (e) { console.error(e); }
}
