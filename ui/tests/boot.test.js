// Start-up animation: pure parts (timeline, variant choice, hand-over state machine, projection,
// geometry vs the real logo) and static checks of index.html / size budget.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { sample, ease, frameAt, TIMELINES, DEFAULTS, chooseVariant, readPrefs, createMachine, KEYS } from '../js/boot/timeline.js';
import { rotator, project, nodePos, NODES, SPOKES, FLAME, CORE, DIST } from '../js/boot/scene.js';

const UI = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const ROOT = path.resolve(UI, '..');
const near = (a, b, e = 1e-9) => assert.ok(Math.abs(a - b) <= e, `${a} ≈ ${b}`);
const q = s => new URLSearchParams(s);

test('easing and keyframe sampling', () => {
  for (const k of Object.keys(ease)) { near(ease[k](0), 0); near(ease[k](1), 1); }
  const keys = [[100, 0], [200, 10, 'l'], [300, 4]];
  assert.equal(sample(keys, 0), 0);
  assert.equal(sample(keys, 150), 5);
  assert.equal(sample(keys, 999), 4);
  assert.equal(sample(7, 123), 7);
});

test('timelines start from the static logo and settle exactly on it', () => {
  for (const v of ['full', 'short', 'reduced']) {
    const tl = TIMELINES[v];
    for (const t of [0, tl.T]) {
      const f = frameAt(tl, t);
      for (const k of ['yaw', 'pitch', 'roll', 'scatter', 'sep']) near(f[k], 0, 1e-9);
      assert.ok(f.spark === 0 || f.spark === 1, 'no spark in flight');
      for (const k of ['zoom', 'ring', 'spoke', 'flame']) near(f[k], 1, 1e-9);
      if (t === 0) { assert.equal(f.word, 0); assert.equal(f.glow, 0); assert.equal(f.dust, 0); assert.equal(f.lift, 0); }
      else assert.equal(f.word, 1, v + ' wordmark visible at the end');
    }
  }
  // reduced: no camera, particles, glow or lift at any time
  for (let t = 0; t <= TIMELINES.reduced.T; t += 25) {
    const f = frameAt(TIMELINES.reduced, t);
    for (const k of Object.keys(DEFAULTS)) if (k !== 'word') assert.equal(f[k], DEFAULTS[k], `${k}@${t}`);
  }
});

test('durations: full 2.0–2.6 s, short 1.0–1.4 s, hand-over keeps the total near 3 s', () => {
  assert.ok(TIMELINES.full.T >= 2000 && TIMELINES.full.T <= 2600);
  assert.ok(TIMELINES.short.T >= 1000 && TIMELINES.short.T <= 1400);
  assert.equal(TIMELINES.off.T, 0);
  for (const v of ['full', 'short', 'reduced']) assert.ok(TIMELINES[v].out <= 700 && TIMELINES[v].skipOut <= 320);
});

test('no strobing: the glow rises at most once and never flickers', () => {
  for (const v of ['full', 'short']) {
    let dir = 0, changes = 0, prev = frameAt(TIMELINES[v], 0).glow;
    for (let t = 10; t <= TIMELINES[v].T; t += 10) {
      const g = frameAt(TIMELINES[v], t).glow, d = Math.sign(Math.round((g - prev) * 1e6));
      if (d && d !== dir) { changes++; dir = d; }
      prev = g;
    }
    assert.ok(changes <= 4, `${v}: ${changes} glow direction changes`);
  }
});

test('variant: full by default on every platform; OS reduce motion, early/seen/late/low power never change it', () => {
  const base = { query: q(''), prefs: {} };
  assert.equal(chooseVariant(base), 'full');
  // inputs that used to shorten it are no longer part of the decision
  for (const extra of [{ seen: true }, { readyEarly: true }, { late: true }, { lowPower: true }, { seen: true, readyEarly: true, late: true, lowPower: true }]) {
    assert.equal(chooseVariant({ ...base, ...extra }), 'full', JSON.stringify(extra));
  }
  // only the person's choices change it
  assert.equal(chooseVariant({ ...base, prefs: { boot_animation: 'full' } }), 'full');
  assert.equal(chooseVariant({ ...base, prefs: { boot_animation: 'short' } }), 'short');
  assert.equal(chooseVariant({ ...base, prefs: { boot_animation: 'off' } }), 'off');
  // the OS reduce-motion setting does not change it (owner decision; the rest of the app follows it)
  assert.equal(chooseVariant({ ...base, osReduce: true }), 'full');
  assert.equal(chooseVariant({ ...base, osReduce: true, lowPower: true, prefs: { motion: 'system' } }), 'full');
  assert.equal(chooseVariant({ ...base, osReduce: true, prefs: { motion: 'full' } }), 'full');
  assert.equal(chooseVariant({ ...base, osReduce: true, prefs: { boot_animation: 'short' } }), 'short');
  assert.equal(chooseVariant({ ...base, prefs: { motion: 'reduced' } }), 'reduced');
  assert.equal(chooseVariant({ ...base, prefs: { motion: 'off' } }), 'off');
  assert.equal(chooseVariant({ ...base, prefs: { simple_mode: true } }), 'reduced');
  assert.equal(chooseVariant({ ...base, prefs: { simple_mode: true, motion: 'full' } }), 'reduced');
  assert.equal(chooseVariant({ ...base, prefs: { boot_animation: 'off', motion: 'reduced' } }), 'off');
  assert.equal(chooseVariant({ ...base, query: q('?motion=off') }), 'off');
  // automation and ?noboot skip it; ?boot= forces a variant even under automation
  assert.equal(chooseVariant({ ...base, webdriver: true }), 'none');
  assert.equal(chooseVariant({ ...base, query: q('?noboot=1&boot=full') }), 'none');
  for (const v of ['full', 'short', 'reduced', 'off']) assert.equal(chooseVariant({ ...base, webdriver: true, osReduce: true, query: q('?boot=' + v) }), v);
  assert.equal(chooseVariant({ ...base, query: q('?boot=bogus') }), 'full');
});

test('preferences read from mirrored local storage with safe defaults', () => {
  const mem = o => ({ getItem: k => (k in o ? o[k] : null) });
  assert.deepEqual(readPrefs(mem({})), { prefs: {}, theme: 'dark' });
  const blocked = { getItem() { throw new Error('SecurityError'); } };
  assert.deepEqual(readPrefs(blocked), { prefs: {}, theme: 'dark' });
  assert.deepEqual(readPrefs(null), { prefs: {}, theme: 'dark' });
  const r = readPrefs(mem({
    'ember.ui.boot_animation': '"short"', 'ember.ui.motion': '"reduced"', 'ember.ui.simple_mode': 'true', 'ember.ui.text_scale': '1.2',
    [KEYS.theme]: JSON.stringify({ theme: 'night', low_power: true }), 'embermesh.ui.boot_seen': '1',
  }));
  assert.deepEqual(r, { prefs: { boot_animation: 'short', motion: 'reduced', simple_mode: true }, theme: 'night' });
  assert.equal(readPrefs(mem({ 'ember.ui.motion': '{broken' })).prefs.motion, '{broken', 'non-JSON value kept as text, then ignored');
  assert.equal(readPrefs(mem({ [KEYS.theme]: '{"theme":"neon"}' })).theme, 'dark');
  // html[data-simple] counts; html[data-motion] is the EFFECTIVE motion (folds in low power): ignored
  const d = readPrefs(mem({ 'ember.ui.motion': '"full"' }), { motion: 'reduced', simple: '1', theme: 'light', lowpower: '1' });
  assert.deepEqual(d, { prefs: { motion: 'full', simple_mode: true }, theme: 'light' });
  assert.equal(chooseVariant({ query: q(''), prefs: readPrefs(mem({}), { motion: 'reduced', lowpower: '1' }).prefs }), 'full', 'low power alone keeps the full animation');
});

test('settings written by lib/prefs.js (ember.ui.* JSON keys) pick the variant', () => {
  const mem = o => ({ getItem: k => (k in o ? o[k] : null) });
  const pick = (o, extra = {}) => chooseVariant({ query: q(''), prefs: readPrefs(mem(o)).prefs, ...extra });
  assert.equal(pick({}), 'full');
  assert.equal(pick({ 'ember.ui.boot_animation': '"short"' }), 'short');
  assert.equal(pick({ 'ember.ui.boot_animation': '"off"' }), 'off');
  assert.equal(pick({ 'ember.ui.boot_animation': '"full"', 'embermesh.ui.boot_seen': '1' }), 'full', 'the old "seen" marker is ignored');
  assert.equal(pick({ 'ember.ui.motion': '"reduced"' }), 'reduced');
  assert.equal(pick({ 'ember.ui.motion': '"off"' }), 'off');
  assert.equal(pick({ 'ember.ui.motion': '"system"' }, { osReduce: true }), 'full', 'Follow system + OS reduce motion: still full');
  assert.equal(pick({ 'ember.ui.motion': '"full"' }, { osReduce: true }), 'full');
  assert.equal(pick({ 'ember.ui.simple_mode': 'true', 'ember.ui.motion': '"full"' }), 'reduced');
  assert.equal(pick({ 'ember.ui.simple_mode': 'false' }), 'full');
  assert.equal(pick({ 'embermesh.ui.theme': '{"theme":"dark","low_power":true}' }), 'full', 'low-power mode does not change it');
});

test('hand-over with the app ready long before the animation ends: plays the full minimum', () => {
  const T = TIMELINES.full.T;
  const m = createMachine({ min: T, cap: 2900, out: TIMELINES.full.out, skipOut: TIMELINES.full.skipOut });
  m.ready(); // native core ready before frame 0
  m.start(1000); // frame 0 = first visibility, however late after navigation
  for (let t = 1000; t < 1000 + T; t += 16) assert.notEqual(m.tick(t).phase, 'out', `still playing at ${t - 1000} ms`);
  const s = m.tick(1000 + T);
  assert.equal(s.phase, 'out'); assert.equal(s.reason, 'ready'); assert.equal(s.t, T);
  assert.equal(m.tick(1000 + T + TIMELINES.full.out).phase, 'done');
});

test('hand-over: waits for both the minimum time and the app', () => {
  const m = createMachine({ min: 1000, cap: 2900, out: 500, skipOut: 300 });
  m.start(0);
  assert.equal(m.tick(400).phase, 'play');
  m.ready();
  assert.equal(m.tick(900).phase, 'play', 'ready early still plays to the minimum');
  const s = m.tick(1000);
  assert.equal(s.phase, 'out'); assert.equal(s.reason, 'ready'); assert.equal(s.outDur, 500);
  near(m.tick(1250).p, 0.5);
  assert.equal(m.tick(1500).phase, 'done');
});

test('hand-over: holds while the app loads, then the hard cap', () => {
  const m = createMachine({ min: 1000, cap: 2900, out: 500, skipOut: 300 });
  m.start(100);
  assert.equal(m.tick(1200).phase, 'hold');
  assert.equal(m.m.t, 1000, 'timeline clock stops at its end');
  assert.equal(m.tick(2900).phase, 'hold');
  const s = m.tick(3000);
  assert.equal(s.phase, 'out'); assert.equal(s.reason, 'cap'); assert.equal(s.outDur, 300);
  assert.equal(m.tick(3300).phase, 'done');
  // ready during hold hands over at once
  const h = createMachine({ min: 1000 }); h.start(0); h.tick(1500); h.ready();
  assert.equal(h.tick(1516).reason, 'ready');
});

test('hand-over: any input skips immediately, from any moment', () => {
  for (const at of [0, 50, 700, 1600]) {
    const m = createMachine({ min: 1000, out: 500, skipOut: 300 });
    m.start(0); m.tick(at);
    m.skip(at);
    const s = m.tick(at + 1);
    assert.equal(s.phase, 'out'); assert.equal(s.reason, 'skip');
    assert.equal(m.tick(at + 300).phase, 'done');
  }
  const m = createMachine({ min: 1000, out: 500, skipOut: 300 });
  m.start(0); m.tick(1000); m.ready(); m.tick(1001);
  m.skip(1100); // already handing over: no restart
  assert.equal(m.m.reason, 'ready');
  // off variant: zero-length hand-over completes on the first tick
  const off = createMachine({ min: 0 }); off.start(0); off.ready();
  assert.equal(off.tick(0).phase, 'done');
});

test('hand-over: hidden time does not count', () => {
  const m = createMachine({ min: 1000, cap: 2900 });
  m.start(0); m.tick(500);
  m.pause(500);
  assert.equal(m.tick(5000).phase, 'play', 'paused: no progress');
  m.resume(5000);
  assert.equal(m.tick(5100).t, 600);
  assert.equal(m.tick(5500).phase, 'hold');
});

test('projection: perspective with depth, exact logo scale on the z=0 plane', () => {
  const S = 117 / 128, cam = { cx: 195, cy: 422, F: S * DIST, dist: DIST };
  const p = project([43, -25, 0], cam);
  near(p[0], 195 + 43 * S); near(p[1], 422 - 25 * S); near(p[2], S); near(p[3], DIST);
  const far = project([43, -25, 100], cam), close = project([43, -25, -100], cam);
  assert.ok(far[2] < S && close[2] > S && far[0] < p[0] && close[0] > p[0], 'farther is smaller and nearer the centre');
  // dolly: half the distance doubles the scale
  near(project([10, 0, 0], { ...cam, dist: DIST / 2 })[2], 2 * S);
  const R = rotator(0.7, -0.4, 0.2), v = R(10, -20, 30);
  near(Math.hypot(...v), Math.hypot(10, -20, 30), 1e-9);
  assert.deepEqual(rotator(0, 0, 0)(1, 2, 3), [1, 2, 3]);
  // depth ordering follows the rotation: after a quarter turn about y, +x points away
  const q4 = rotator(Math.PI / 2, 0, 0);
  assert.ok(q4(50, 0, 0)[2] < 0 && q4(-50, 0, 0)[2] > 0);
});

test('scene geometry is the logo: nodes, spokes and flame paths match docs/assets/logo.svg and index.html', () => {
  for (let i = 0; i < 6; i++) {
    const n = nodePos(i, 0);
    near(n[0], NODES[i][0], 1e-9); near(n[1], NODES[i][1], 1e-9); near(n[2], 0, 1e-9);
    const s = nodePos(i, 1);
    assert.ok(Math.hypot(s[0], s[1]) > 70 && Math.abs(s[2]) > 30, 'scattered into depth');
  }
  for (const file of [path.join(ROOT, 'docs/assets/logo.svg'), path.join(UI, 'index.html')]) {
    if (!fs.existsSync(file)) continue;
    const svg = fs.readFileSync(file, 'utf8');
    const circles = [...svg.matchAll(/<circle cx="(\d+)" cy="(\d+)" r="5\.5"/g)].map(m => [+m[1] - 64, +m[2] - 64]);
    assert.deepEqual(circles, NODES, file);
    assert.ok(svg.includes(FLAME) && svg.includes(CORE), file + ': flame paths');
    const spokes = [...svg.matchAll(/M(\d+) (\d+) L(\d+) (\d+)(?= M| ?")/g)].slice(-6).map(m => [+m[3] - 64, +m[4] - 64]);
    assert.deepEqual(spokes, SPOKES, file + ': spokes');
  }
});

test('index.html: static first frame, CSP unchanged, boot module separate from the app', () => {
  const html = fs.readFileSync(path.join(UI, 'index.html'), 'utf8');
  assert.match(html, /<div id="ember-boot" aria-hidden="true">/);
  assert.match(html, /id="ember-boot-status" role="status"/);
  assert.match(html, /<link rel="stylesheet" href="css\/boot\.css">/);
  assert.match(html, /<script type="module" src="js\/boot\/boot\.js" async><\/script>/);
  assert.doesNotMatch(html, /\sstyle=|<style/);
  const csp = /http-equiv="Content-Security-Policy" content="([^"]+)"/.exec(html)[1];
  assert.equal(csp, "default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; style-src 'self'; img-src 'self' data: blob:; font-src 'self'; media-src 'self' blob: data:; connect-src 'self' ws: wss: blob: data:; worker-src 'self' blob:; object-src 'none'; base-uri 'none'; form-action 'none'; frame-src 'none'; manifest-src 'self'");
  // the overlay is never focusable
  const boot = /<div id="ember-boot"[\s\S]*?<\/div>\s*<\/div>|<div id="ember-boot"[\s\S]*?<p class="eb-sr"/.exec(html)[0];
  assert.doesNotMatch(boot, /tabindex|<button|<a |<input/);
});

// target ~25 KB; hard ceiling 30 KB (readable source, no build step; ~10 KB gzipped)
test('size budget: boot module + styles stay small', () => {
  const files = ['js/boot/boot.js', 'js/boot/scene.js', 'js/boot/timeline.js', 'css/boot.css'];
  const total = files.reduce((n, f) => n + fs.statSync(path.join(UI, f)).size, 0);
  assert.ok(total < 30 * 1024, `${total} bytes`);
});

test('boot module imports without a DOM and does nothing', async () => {
  const m = await import('../js/boot/boot.js');
  assert.deepEqual(Object.keys(m), []);
});
