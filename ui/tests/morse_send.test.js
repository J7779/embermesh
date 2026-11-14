// Morse sending by light (docs/RPC_API.md §10): pattern building and speed limits (lib/morse.js),
// the browser host (endpoints/device.js flash/stop/detectSignal with a fake camera, document and
// clock), the routers, the mock backend and the Send screen's pure helpers.
import test from 'node:test';
import assert from 'node:assert/strict';
import {
  toMorse, morsePattern, patternDuration, dotMs, encodeText, buildSignal, lightWpm, maxFlashesPerSecond,
  MIN_WPM, TORCH_DEFAULT_WPM, TORCH_MAX_WPM, SCREEN_MAX_WPM, SCREEN_MIN_DOT_MS, PROSIGNS,
} from '../js/lib/morse.js';
import * as device from '../js/endpoints/device.js';
import { HostRouter } from '../js/endpoints/router.js';
import MockBackend from '../js/backends/mock.js';
import { torchState, primaryAction, resultText, torchErrorText } from '../js/screens/morse_send.js';

// ------------------------------------------------------------------ patterns and speeds
test('encodeText: letters, prosigns, SOS as one sign, skipped characters listed once', () => {
  const e = encodeText('Hi <AR> é~é');
  assert.deepEqual(e.words.map(w => w.map(c => c.ch)), [['H', 'I'], ['<AR>']]);
  assert.equal(e.words[1][0].code, PROSIGNS.AR);
  assert.deepEqual(e.skipped, ['É', '~']);
  assert.equal(e.morse, '.... .. / .-.-.');
  assert.deepEqual(encodeText(' sos ').words, [[{ ch: 'SOS', code: '...---...', prosign: true }]]);
  assert.deepEqual(encodeText('<ZZ>').skipped, ['<', '>'], 'unknown prosign: brackets skipped, letters kept');
  assert.equal(encodeText('<ZZ>').morse, '--.. --..');
  assert.deepEqual(encodeText('').words, []);
});

test('buildSignal: same timing as morsePattern, character spans, total duration', () => {
  for (const [t, w] of [['CQ CQ TEST', 12], ['sos', 7], ['73 de N0CALL', 15], ['a b', 3]]) {
    const s = buildSignal(t, w);
    assert.deepEqual(s.pattern, morsePattern(toMorse(t), w), t);
    assert.equal(s.durationMs, patternDuration(s.pattern));
    assert.equal(s.pattern.length % 2, 1, 'starts and ends with "on"');
    assert.equal(s.spans.at(-1).end, s.durationMs);
  }
  const s = buildSignal('ET', 12); // dot, letter gap, dash
  assert.deepEqual(s.pattern, [100, 300, 300]);
  assert.deepEqual(s.spans.map(x => [x.ch, x.start, x.end]), [['E', 0, 100], ['T', 400, 700]]);
  const w = buildSignal('E E', 12);
  assert.deepEqual(w.pattern, [100, 700, 100], 'word gap 7 units');
  assert.deepEqual(buildSignal('~', 12).pattern, []);
});

test('speeds: camera-friendly default, torch cap, screen clamp to ≤ 3 flashes per second', () => {
  assert.equal(TORCH_DEFAULT_WPM, 12);
  assert.equal(TORCH_MAX_WPM, 15);
  // ≥ 2.4 frames per dot at 30 fps for the fastest torch speed (decoder needs ~2, breaks at 1.8)
  assert.ok(dotMs(TORCH_MAX_WPM) / (1000 / 30) >= 2.4);
  assert.ok(dotMs(TORCH_DEFAULT_WPM) / (1000 / 30) >= 3);
  assert.ok(dotMs(SCREEN_MAX_WPM) >= SCREEN_MIN_DOT_MS);
  assert.equal(lightWpm(20, 'screen'), SCREEN_MAX_WPM);
  assert.equal(lightWpm(20, 'torch'), TORCH_MAX_WPM);
  assert.equal(lightWpm(1, 'torch'), MIN_WPM);
  assert.equal(lightWpm('x', 'torch'), TORCH_DEFAULT_WPM);
  assert.equal(lightWpm(6, 'screen'), 6);
  // the worst case for flash rate: a run of dots
  for (let w = MIN_WPM; w <= SCREEN_MAX_WPM; w++) assert.ok(maxFlashesPerSecond(buildSignal('HHHHH 55555 EISH', w).pattern) <= 3, w + ' WPM');
  assert.ok(maxFlashesPerSecond(buildSignal('HHHHH', SCREEN_MAX_WPM + 1).pattern) > 3, 'the clamp is needed');
  // every element within the host limits (10 ms .. 60 s)
  for (const w of [MIN_WPM, TORCH_MAX_WPM]) for (const v of buildSignal('PARIS 0123', w).pattern) assert.ok(v >= 10 && v <= 60000);
});

// ------------------------------------------------------------------ fake browser for the host
function fakeEnv({ ua = 'Mozilla/5.0 (Linux; Android 14) Mobile', torch = true, gum = 'ok', jitter = 0, wake = true } = {}) {
  let now = 1000;
  const timers = [];
  const log = { gum: 0, tracks: [], torch: [], wake: 0, released: 0, constraints: null };
  const listeners = { doc: new Map(), win: new Map() };
  const add = (m) => (k, f) => { if (!m.has(k)) m.set(k, new Set()); m.get(k).add(f); };
  const rem = (m) => (k, f) => { if (m.has(k)) m.get(k).delete(f); };
  const el = () => {
    const e = { style: {}, dataset: {}, children: [], attrs: {}, removed: false, setAttribute(k, v) { this.attrs[k] = v; }, appendChild(c) { this.children.push(c); return c; }, remove() { this.removed = true; doc.body.children = doc.body.children.filter(x => x !== this); }, focus() {} };
    Object.defineProperty(e.style, 'cssText', { set(v) { for (const p of v.split(';')) { const [k, val] = p.split(':'); if (k && val) this[k.trim()] = val.trim(); } }, get() { return ''; } });
    return e;
  };
  const doc = {
    visibilityState: 'visible',
    documentElement: { dataset: {} },
    body: { children: [], appendChild(c) { this.children.push(c); return c; } },
    createElement: () => el(),
    addEventListener: add(listeners.doc), removeEventListener: rem(listeners.doc),
  };
  const mkTrack = () => {
    const t = {
      stopped: false,
      stop() { this.stopped = true; },
      getCapabilities: () => (torch ? { torch: true } : { facingMode: ['environment'] }),
      applyConstraints(c) { log.torch.push([now, c.advanced[0].torch]); return Promise.resolve(); },
    };
    log.tracks.push(t);
    return t;
  };
  const env = {
    isSecureContext: true,
    document: doc,
    navigator: {
      userAgent: ua,
      mediaDevices: {
        getUserMedia: async (c) => {
          log.gum++; log.constraints = c;
          if (gum === 'denied') throw Object.assign(new Error('denied'), { name: 'NotAllowedError' });
          if (gum === 'nocam') throw Object.assign(new Error('none'), { name: 'NotFoundError' });
          const v = mkTrack();
          return { getTracks: () => [v], getVideoTracks: () => [v] };
        },
      },
      wakeLock: wake ? { request: async () => { log.wake++; return { release() { log.released++; } }; } } : undefined,
    },
    performance: { now: () => now },
    setTimeout: (fn, ms) => { const t = { at: now + Math.max(0, ms) + jitter, fn }; timers.push(t); return t; },
    clearTimeout: (t) => { const i = timers.indexOf(t); if (i >= 0) timers.splice(i, 1); },
    addEventListener: add(listeners.win), removeEventListener: rem(listeners.win),
  };
  env.window = env;
  const flush = () => new Promise(r => setImmediate(r));
  /** Runs fake time until `p` settles (or `until(now)` is true). */
  async function run(p, until) {
    let done = false;
    p.then(() => { done = true; }, () => { done = true; });
    for (let i = 0; i < 20000 && !done; i++) {
      await flush();
      if (done) break;
      if (until && until(now)) return;
      timers.sort((a, b) => a.at - b.at);
      const t = timers.shift();
      if (!t) continue;
      now = Math.max(now, t.at);
      t.fn();
    }
    return p;
  }
  const fire = (where, type) => { for (const f of [...((where === 'doc' ? listeners.doc : listeners.win).get(type) || [])]) f({ type }); };
  return { env, log, run, fire, doc, now: () => now, listeners };
}

const allStopped = (log) => log.tracks.every(t => t.stopped);
const lastTorch = (log) => (log.torch.length ? log.torch.at(-1)[1] : null);

test('host limits: bad pattern and bad mode → bad_params', async () => {
  const { env } = fakeEnv();
  for (const p of [[], [5], [100, 60001], new Array(4097).fill(100), new Array(11).fill(60000), 'x', [NaN]]) {
    await assert.rejects(device.flash(env, { pattern_ms: p }), { code: 'bad_params' }, JSON.stringify(p).slice(0, 40));
  }
  await assert.rejects(device.flash(env, { pattern_ms: [100], mode: 'laser' }), { code: 'bad_params' });
  await assert.rejects(device.tone(env, { pattern_ms: [5] }), { code: 'bad_params' });
  assert.equal(device.checkPattern([10, 60000, 10]).length, 3);
});

test('detectSignal: "ask" only where a rear camera may exist; desktop says "no" and never opens a camera', async () => {
  assert.deepEqual(device.detectSignal(fakeEnv().env), { torch: 'ask', screen: true });
  const iPad = fakeEnv({ ua: 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)' }).env;
  assert.equal(device.detectSignal(iPad).torch, 'no');
  iPad.navigator.maxTouchPoints = 5;
  assert.equal(device.detectSignal(iPad).torch, 'ask');
  const desk = fakeEnv({ ua: 'Mozilla/5.0 (X11; Linux x86_64)' });
  assert.equal(device.detectSignal(desk.env).torch, 'no');
  const r = await desk.run(device.flash(desk.env, { pattern_ms: [100, 100, 100] }));
  assert.equal(r.used, 'screen');
  assert.equal(r.fallback, 'no_torch');
  assert.equal(desk.log.gum, 0, 'no camera request on a desktop');
  await assert.rejects(desk.run(device.flash(desk.env, { pattern_ms: [100], mode: 'torch' })), { code: 'unsupported' });
  assert.equal(desk.log.gum, 0);
  const insecure = fakeEnv().env; insecure.isSecureContext = false;
  assert.equal(device.detectSignal(insecure).torch, 'no');
  const nomedia = fakeEnv().env; delete nomedia.navigator.mediaDevices;
  assert.equal(device.detectSignal(nomedia).torch, 'no');
});

test('torch present: rear camera only, no audio, torch keyed, off and every track stopped at the end', async () => {
  const f = fakeEnv();
  const pattern = [100, 100, 300];
  const r = await f.run(device.flash(f.env, { pattern_ms: pattern }));
  assert.equal(r.used, 'torch');
  assert.equal(r.stopped, false);
  assert.equal(r.fallback, undefined);
  assert.equal(r.timing.edges, 3);
  assert.deepEqual(f.log.constraints, { video: { facingMode: 'environment' }, audio: false });
  assert.deepEqual(f.log.torch.map(x => x[1]), [true, false, true, false]);
  assert.equal(lastTorch(f.log), false);
  assert.ok(allStopped(f.log));
  assert.equal(f.doc.body.children.length, 0, 'no screen overlay for the torch');
  assert.equal(f.log.wake, 1); assert.equal(f.log.released, 1);
  assert.equal(f.listeners.doc.get('visibilitychange').size, 0, 'listeners removed');
});

test('torch absent: auto falls back to the screen (said why), remembered as "no"; torch mode → unsupported', async () => {
  const f = fakeEnv({ torch: false });
  const r = await f.run(device.flash(f.env, { pattern_ms: [100, 100, 100] }));
  assert.equal(r.used, 'screen');
  assert.equal(r.fallback, 'no_torch');
  assert.ok(allStopped(f.log), 'the camera opened to look for a torch is closed again');
  assert.equal(device.detectSignal(f.env).torch, 'no');
  const gum = f.log.gum;
  await f.run(device.flash(f.env, { pattern_ms: [100] }));
  assert.equal(f.log.gum, gum, 'not asked again this session');
  await assert.rejects(f.run(device.flash(f.env, { pattern_ms: [100], mode: 'torch' })), { code: 'unsupported' });
  const g = fakeEnv({ gum: 'nocam' });
  await assert.rejects(g.run(device.flash(g.env, { pattern_ms: [100], mode: 'torch' })), { code: 'unsupported' });
});

test('permission refused: auto → screen with fallback "permission_denied"; torch mode → permission_denied', async () => {
  const f = fakeEnv({ gum: 'denied' });
  const r = await f.run(device.flash(f.env, { pattern_ms: [100, 100, 100] }));
  assert.equal(r.used, 'screen');
  assert.equal(r.fallback, 'permission_denied');
  assert.equal(device.detectSignal(f.env).torch, 'ask', 'a refusal is not "no torch": the user can still allow it');
  await assert.rejects(f.run(device.flash(f.env, { pattern_ms: [100], mode: 'torch' })), { code: 'permission_denied' });
});

test('screen mode: overlay lit and dark by the pattern, removed at the end; night theme flashes red', async () => {
  const f = fakeEnv();
  const seen = [];
  const p = device.flash(f.env, { pattern_ms: [100, 100, 100], mode: 'screen' });
  await f.run(p, () => { const el = f.doc.body.children[0]; if (el) seen.push(el.style.background); return false; });
  const r = await p;
  assert.equal(r.used, 'screen');
  assert.equal(f.log.gum, 0, 'screen mode never touches the camera');
  assert.ok(seen.includes('#fff') && seen.includes('#000'));
  assert.equal(f.doc.body.children.length, 0);
  const n = fakeEnv();
  n.doc.documentElement.dataset.theme = 'night';
  const lit = [];
  const q = device.flash(n.env, { pattern_ms: [100], mode: 'screen' });
  await n.run(q, () => { const el = n.doc.body.children[0]; if (el) lit.push(el.style.background); return false; });
  await q;
  assert.ok(lit.includes('#f00') && !lit.includes('#fff'));
});

test('stop mid-pattern: resolves stopped, torch off, every track stopped; stop is a no-op otherwise', async () => {
  assert.deepEqual(device.signalStop(), {});
  const f = fakeEnv();
  const p = device.flash(f.env, { pattern_ms: [100, 100, 100, 100, 100, 100, 100] });
  await f.run(p, (t) => t >= 1250);
  device.signalStop();
  const r = await f.run(p);
  assert.equal(r.stopped, true);
  assert.equal(r.used, 'torch');
  assert.ok(r.timing.edges < 7);
  assert.equal(lastTorch(f.log), false);
  assert.ok(allStopped(f.log));
});

test('page hidden or closed mid-pattern: the light goes off and the camera is released', async () => {
  for (const how of ['hidden', 'pagehide']) {
    const f = fakeEnv();
    const p = device.flash(f.env, { pattern_ms: new Array(21).fill(100) });
    await f.run(p, (t) => t >= 1450);
    if (how === 'hidden') { f.doc.visibilityState = 'hidden'; f.fire('doc', 'visibilitychange'); } else f.fire('win', 'pagehide');
    const r = await f.run(p);
    assert.equal(r.stopped, true, how);
    assert.equal(lastTorch(f.log), false, how);
    assert.ok(allStopped(f.log), how);
    assert.equal(f.log.released, 1, how);
  }
});

test('a second flash stops the first', async () => {
  const f = fakeEnv();
  const p1 = device.flash(f.env, { pattern_ms: new Array(11).fill(100) });
  await f.run(p1, (t) => t >= 1300);
  const p2 = device.flash(f.env, { pattern_ms: [100] });
  const r1 = await f.run(p1);
  assert.equal(r1.stopped, true);
  const r2 = await f.run(p2);
  assert.equal(r2.stopped, false);
  assert.ok(allStopped(f.log));
});

test('timing: deadlines from the pattern start — late timers never accumulate drift', async () => {
  const f = fakeEnv({ jitter: 7 });
  const pattern = new Array(41).fill(100);
  const p = device.flash(f.env, { pattern_ms: pattern });
  const r = await f.run(p);
  const on = f.log.torch.filter((x, i) => i < pattern.length).map(x => x[0]);
  const t0 = on[0];
  on.forEach((t, i) => { assert.ok(t - (t0 + i * 100) <= 7.001, `edge ${i} late by ${t - (t0 + i * 100)} ms`); });
  assert.ok(r.timing.max_late_ms <= 7);
});

test('routers: host.signal.stop, host.info.signal and the "torch" feature', async () => {
  const f = fakeEnv();
  const r = new HostRouter({ core: { call: async () => ({}) }, env: f.env, endpoints: [], log: { warn() {}, error() {}, log() {} } });
  assert.deepEqual(await r.handle('host.signal.stop', {}), {});
  assert.ok(r.features().includes('torch'));
  assert.ok(r.features().includes('flash'));
  await assert.rejects(r.handle('host.signal.flash', { pattern_ms: [1] }), { code: 'bad_params' });
  const d = fakeEnv({ ua: 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)' });
  const rd = new HostRouter({ core: { call: async () => ({}) }, env: d.env, endpoints: [], log: { warn() {}, error() {}, log() {} } });
  assert.ok(!rd.features().includes('torch'), 'a desktop does not offer a flashlight');
});

// ------------------------------------------------------------------ mock backend
const mock = (torch) => new MockBackend({ persist: false, speed: 0.001, autoplay: false, latency: 0, params: new URLSearchParams(torch ? 'torch=' + torch : '') });

test('mock: host.info.signal and features per torch switch', async () => {
  for (const [t, want, feat] of [['yes', 'yes', true], ['ask', 'ask', true], ['no', 'no', false], ['refused', 'ask', true], [null, 'ask', true]]) {
    const b = mock(t);
    const hi = await b.call('host.info');
    assert.equal(hi.signal.torch, want, String(t));
    assert.equal(hi.features.includes('torch'), feat, String(t));
    b.close();
  }
});

test('mock: host.signal.flash honours mode, limits, stop and reports used/fallback', async () => {
  const p = [100, 100, 300];
  let b = mock('yes');
  assert.equal((await b.call('host.signal.flash', { pattern_ms: p })).used, 'torch');
  assert.equal((await b.call('host.signal.flash', { pattern_ms: p, mode: 'screen' })).used, 'screen');
  await assert.rejects(b.call('host.signal.flash', { pattern_ms: [5] }), { code: 'bad_params' });
  await assert.rejects(b.call('host.signal.flash', { pattern_ms: p, mode: 'x' }), { code: 'bad_params' });
  b.close();
  b = mock('no');
  const r = await b.call('host.signal.flash', { pattern_ms: p });
  assert.deepEqual([r.used, r.fallback], ['screen', 'no_torch']);
  await assert.rejects(b.call('host.signal.flash', { pattern_ms: p, mode: 'torch' }), { code: 'unsupported' });
  b.close();
  b = mock('refused');
  const rr = await b.call('host.signal.flash', { pattern_ms: p });
  assert.deepEqual([rr.used, rr.fallback], ['screen', 'permission_denied']);
  await assert.rejects(b.call('host.signal.flash', { pattern_ms: p, mode: 'torch' }), { code: 'permission_denied' });
  b.close();
  b = new MockBackend({ persist: false, speed: 1, autoplay: false, latency: 0, params: new URLSearchParams('torch=yes') });
  const long = b.call('host.signal.flash', { pattern_ms: [60000] });
  await new Promise(res => setTimeout(res, 20));
  assert.deepEqual(await b.call('host.signal.stop'), {});
  const lr = await long;
  assert.equal(lr.stopped, true);
  assert.deepEqual(await b.call('host.signal.stop'), {}, 'no-op when idle');
  b.close();
});

// ------------------------------------------------------------------ Send screen helpers
test('Send screen: primary action and messages for every torch state and result', () => {
  assert.equal(torchState({ signal: { torch: 'yes' } }), 'yes');
  assert.equal(torchState({ signal: { torch: 'ask' } }), 'ask');
  assert.equal(torchState({ signal: { torch: 'no' } }), 'no');
  assert.equal(torchState({ features: ['flash'] }), 'no', 'hosts before §10: screen first');
  assert.deepEqual(primaryAction('yes'), { light: 'torch', label: 'Flash with flashlight' });
  assert.deepEqual(primaryAction('ask'), { light: 'torch', label: 'Flash with flashlight' });
  assert.deepEqual(primaryAction('no'), { light: 'screen', label: 'Flash with screen' });
  assert.equal(resultText({ used: 'torch', stopped: false }, 'torch'), 'Sent with the flashlight.');
  assert.equal(resultText({ used: 'screen' }, 'screen'), 'Sent with the screen.');
  assert.equal(resultText({}, 'torch'), 'Sent.', 'old host: used unknown');
  assert.equal(resultText({ stopped: true, used: 'torch' }, 'torch'), 'Stopped.');
  assert.match(resultText({ used: 'screen', fallback: 'no_torch' }, 'torch'), /no flashlight/);
  assert.match(resultText({ used: 'screen', fallback: 'permission_denied' }, 'torch'), /camera permission/);
  assert.match(torchErrorText({ code: 'permission_denied' }), /camera permission/);
  assert.match(torchErrorText({ code: 'unsupported' }), /No flashlight/);
  assert.match(torchErrorText({ code: 'io_error', message: 'busy' }), /busy/);
});
