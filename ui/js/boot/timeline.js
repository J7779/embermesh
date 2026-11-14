// Start-up animation: easing, keyframed timelines, variant choice, hand-over state machine (pure).
export const clamp01 = x => (x < 0 ? 0 : x > 1 ? 1 : x);
export const ease = {
  l: x => x,
  i: x => x * x * x,
  o: x => 1 - (1 - x) ** 3,
  io: x => (x < 0.5 ? 4 * x * x * x : 1 - (-2 * x + 2) ** 3 / 2),
  b: x => { const y = x - 1; return 1 + 2.2 * y * y * y + 1.2 * y * y; }, // small overshoot
};

/** keys = [[ms, value, ease?], …] or a constant; a key's ease shapes the segment arriving at it */
export function sample(keys, t) {
  if (typeof keys === 'number') return keys;
  if (t <= keys[0][0]) return keys[0][1];
  for (let i = 1; i < keys.length; i++) {
    const [t1, v1, e] = keys[i], [t0, v0] = keys[i - 1];
    if (t <= t1) return v0 + (v1 - v0) * (ease[e] || ease.io)(t1 > t0 ? (t - t0) / (t1 - t0) : 1);
  }
  return keys[keys.length - 1][1];
}

// camera yaw/pitch/roll (rad), zoom; nodes scattered into 3D sparks; ring/spoke drawing; spark run
// (nodes → ember); flame size; flame layer depth (parallax); glow; background sparks; wordmark; lift
export const DEFAULTS = { yaw: 0, pitch: 0, roll: 0, zoom: 1, scatter: 0, ring: 1, spoke: 1, spark: 0, flame: 1, sep: 0, glow: 0, dust: 0, word: 0, lift: 0 };

export const TIMELINES = {
  full: {
    T: 2400, ignite: 1850, out: 580, skipOut: 300,
    yaw: [[0, 0], [850, -0.62], [1550, 0.16], [2150, 0, 'o']],
    pitch: [[0, 0], [850, 0.42], [1550, -0.08], [2150, 0, 'o']],
    roll: [[0, 0], [850, -0.1], [1600, 0.03], [2150, 0, 'o']],
    zoom: [[0, 1], [850, 1.32], [1600, 1.06], [2200, 1, 'o']],
    scatter: [[0, 0], [800, 1], [1500, 0, 'b']],
    ring: [[0, 1], [380, 0, 'o'], [1000, 0], [1450, 1, 'o']],
    spoke: [[0, 1], [300, 0, 'o'], [1250, 0], [1520, 1, 'o']],
    spark: [[1480, 0], [1850, 1, 'i']],
    flame: [[0, 1], [520, 0.36], [1840, 0.36], [2150, 1, 'b']],
    sep: [[0, 0], [800, 1], [1700, 1], [2250, 0]],
    glow: [[0, 0], [600, 0.25], [1840, 0.3], [2000, 1, 'o'], [2400, 0.55]],
    dust: [[0, 0], [550, 1, 'o'], [1700, 0.7], [2400, 0.3]],
    word: [[1900, 0], [2400, 1, 'l']],
    lift: [[1150, 0], [2200, 1]],
  },
  short: {
    T: 1250, ignite: 720, out: 540, skipOut: 280,
    yaw: [[0, 0], [480, -0.34], [1050, 0]],
    pitch: [[0, 0], [480, 0.22], [1050, 0]],
    zoom: [[0, 1], [480, 0.94], [1050, 1]],
    scatter: [[0, 0], [450, 0.3], [950, 0, 'b']],
    spark: [[430, 0], [720, 1, 'i']],
    flame: [[0, 1], [300, 0.55], [700, 0.55], [960, 1, 'b']],
    sep: [[0, 0], [450, 1], [1050, 0]],
    glow: [[0, 0], [700, 0.3], [830, 0.9, 'o'], [1250, 0.55]],
    dust: [[0, 0], [400, 0.6, 'o'], [1250, 0.3]],
    word: [[700, 0], [1200, 1, 'l']],
    lift: [[350, 0], [1050, 1]],
  },
  reduced: { T: 700, out: 320, skipOut: 160, word: [[80, 0], [650, 1]] }, // finished logo, fades only
  off: { T: 0, out: 0, skipOut: 0 },
};

export function frameAt(tl, t) {
  const o = {};
  for (const k in DEFAULTS) o[k] = tl[k] === undefined ? DEFAULTS[k] : sample(tl[k], t);
  return o;
}

export const KEYS = { prefs: 'ember.ui.', theme: 'embermesh.ui.theme' };

/**
 * The person's own choices, mirrored in local storage by lib/prefs.js (RPC_API.md §8) as JSON under
 * `ember.ui.<name>`, and html[data-simple]. Not html[data-motion]: that is the effective motion,
 * which also folds in low-power mode — and low power does not change the start-up animation.
 */
export function readPrefs(store, ds = {}) {
  const val = k => { let r = null; try { r = store ? store.getItem(k) : null; } catch { /* blocked */ } if (r == null) return null; try { return JSON.parse(r); } catch { return r; } };
  const prefs = {};
  for (const k of ['boot_animation', 'motion', 'simple_mode']) { const v = val(KEYS.prefs + k); if (v !== null) prefs[k] = v; }
  if (ds.simple === '1' || ds.simple === 'true') prefs.simple_mode = true;
  const lk = Object(val(KEYS.theme)), th = ['dark', 'light', 'night'];
  return { prefs, theme: th.includes(lk.theme) ? lk.theme : th.includes(ds.theme) ? ds.theme : 'dark' };
}

/**
 * 'none' | 'off' (one static frame) | 'reduced' | 'short' | 'full'. Full on every platform and every
 * page load unless the person chose otherwise IN THE APP: boot_animation short/off, Motion explicitly
 * Reduced/Off (not "Follow system"), Simple mode — or a test hook. The OS reduce-motion setting, low
 * power, being ready early, seen before or a late start do not change it. o = {query, webdriver, prefs}
 */
export function chooseVariant(o) {
  const q = k => (o.query && o.query.get ? o.query.get(k) : null), p = o.prefs || {}, forced = q('boot');
  if (q('noboot') === '1') return 'none';
  if (['full', 'short', 'reduced', 'off'].includes(forced)) return forced;
  if (o.webdriver) return 'none';
  const motion = q('motion') || p.motion;
  if (p.boot_animation === 'off' || motion === 'off') return 'off';
  if (motion === 'reduced' || p.simple_mode === true) return 'reduced';
  return p.boot_animation === 'short' ? 'short' : 'full';
}

/**
 * Hand-over: idle → play → hold (timeline done, app not ready) → out → done. Hands over when min has
 * elapsed AND the app is ready, on skip, or at the cap. Hidden time (pause/resume) does not count.
 */
export function createMachine({ min, cap = 2900, out = 0, skipOut = 0 }) {
  const m = { phase: 'idle', t: 0, el: 0, p: 0, ready: false, reason: '', start: 0, outStart: 0, outDur: out, paused: 0 };
  const begin = (now, why, dur) => Object.assign(m, { phase: 'out', reason: why, outStart: now, outDur: dur });
  return {
    m,
    start(now) { m.phase = 'play'; m.start = now; },
    ready() { m.ready = true; },
    skip(now, why = 'skip') { if (/idle|play|hold/.test(m.phase)) begin(now, why, skipOut); },
    pause(now) { if (!m.paused) m.paused = now; },
    resume(now) { if (m.paused) { const d = now - m.paused; m.start += d; m.outStart += d; m.paused = 0; } },
    tick(now) {
      if (m.paused || m.phase === 'idle' || m.phase === 'done') return m;
      m.el = now - m.start;
      if (m.phase !== 'out') {
        m.t = Math.min(m.el, min);
        if (m.el >= cap) begin(now, 'cap', skipOut);
        else if (m.el >= min && m.ready) begin(now, 'ready', out);
        else if (m.el >= min) m.phase = 'hold';
      }
      if (m.phase === 'out') {
        m.p = m.outDur > 0 ? clamp01((now - m.outStart) / m.outDur) : 1;
        if (m.p >= 1) m.phase = 'done';
      }
      return m;
    },
  };
}
