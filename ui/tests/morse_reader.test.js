// Morse camera reader (docs/RPC_API.md §12): whole-frame max-luma grids (lib/capture.js), the
// in-page camera reader's frame path, fallback and camera release (lib/capture_media.js with a fake
// camera), the reader's state line / ring (screens/morse.js) and the mock tracker.
import test from 'node:test';
import assert from 'node:assert/strict';
import { gridSizeFor, maxPoolLuma, FrameBatcher, confidenceWord, b64ToBytes, bytesToB64 } from '../js/lib/capture.js';
import MockBackend from '../js/backends/mock.js';
import { NativeCameraSim } from '../js/mock/camera_sim.js';
import { LightTracker } from '../js/mock/morse_rx.js';

// screens/morse.js imports the DOM-free helpers only at module level
const { cameraStateText, readerMeta, ringGeometry } = await import('../js/screens/morse.js');

// ------------------------------------------------------------------ grids
const frame = (w, h, fill = 20) => { const d = new Uint8ClampedArray(w * h * 4); for (let i = 0; i < d.length; i += 4) { d[i] = d[i + 1] = d[i + 2] = fill; d[i + 3] = 255; } return d; };
const setPx = (d, w, x, y, v = 255) => { const i = (y * w + x) * 4; d[i] = d[i + 1] = d[i + 2] = v; };

test('grid size: 64×36 at 16:9, 48×36 at 4:3, never more than 4096 cells', () => {
  assert.deepEqual(gridSizeFor(1280, 720), { w: 64, h: 36 });
  assert.deepEqual(gridSizeFor(640, 480), { w: 48, h: 36 });
  assert.deepEqual(gridSizeFor(720, 1280), { w: 36, h: 64 });
  for (const [w, h] of [[1920, 1080], [4000, 1000], [100, 3000], [1, 1], [0, 0]]) { const g = gridSizeFor(w, h); assert.ok(g.w * g.h <= 4096 && g.w >= 1 && g.h >= 1, w + 'x' + h); }
});

test('max-pooling: a single bright pixel decides its cell; the mean barely moves', () => {
  const W = 960, H = 540;
  for (const [x, y] of [[0, 0], [959, 539], [480, 270], [14, 15], [15, 14], [737, 61]]) {
    const d = frame(W, H);
    setPx(d, W, x, y);
    const { max, mean } = maxPoolLuma(d, W, H, 64, 36);
    const cell = Math.floor(y * 36 / H) * 64 + Math.floor(x * 64 / W);
    assert.equal(max[cell], 255, `pixel ${x},${y} → cell ${cell}`);
    assert.equal(max.filter(v => v === 255).length, 1, 'only that cell');
    assert.ok(mean[cell] < 25, 'one pixel among 225 hardly changes the mean');
  }
});

test('orientation: row by row from the top-left; mirroring flips left and right', () => {
  const W = 64, H = 36;
  const d = frame(W, H, 0);
  setPx(d, W, 1, 0, 200); // top row, near the left
  const plain = maxPoolLuma(d, W, H, 64, 36).max;
  assert.equal(plain[1], 200);
  const mirrored = maxPoolLuma(d, W, H, 64, 36, { mirror: true }).max;
  assert.equal(mirrored[62], 200);
  setPx(d, W, 0, 35, 90); // bottom-left
  assert.equal(maxPoolLuma(d, W, H, 64, 36).max[35 * 64], 90);
  // luma weights: pure green is brighter than pure blue
  const c = new Uint8ClampedArray([0, 255, 0, 255, 0, 0, 255, 255]);
  const g = maxPoolLuma(c, 2, 1, 2, 1).max;
  assert.ok(g[0] > g[1]);
});

test('batching: at most 8 frames or 100 ms per feed, never reordered', () => {
  const b = new FrameBatcher(8, 100);
  const m = new Uint8Array(4);
  let out = null;
  for (let i = 0; i < 7; i++) assert.equal(b.add(1000 + i * 5, 2, 2, m), null);
  out = b.add(1035, 2, 2, m);
  assert.equal(out.frames.length, 8);
  assert.equal(b.add(1035, 2, 2, m), null, 'same time: dropped');
  assert.equal(b.add(1020, 2, 2, m), null, 'older: dropped');
  assert.equal(b.add(1040, 2, 2, m), null);
  out = b.add(1140, 2, 2, m);
  assert.equal(out.frames.length, 2, '100 ms span closes a batch');
  assert.ok(out.frames[0].t_ms < out.frames[1].t_ms);
  assert.equal(b.flush(), null);
  b.add(2000, 2, 2, new Uint8Array([1, 2, 3, 250]), new Uint8Array([1, 1, 1, 1]));
  const f = b.flush().frames[0];
  assert.deepEqual([...b64ToBytes(f.max_b64)], [1, 2, 3, 250]);
  assert.ok(f.mean_b64);
  assert.deepEqual({ w: f.w, h: f.h }, { w: 2, h: 2 });
});

// ------------------------------------------------------------------ the reader's words and ring
test('reader state line for every target state; words, not percentages', () => {
  assert.match(cameraStateText(null, false), /press Start reading/);
  assert.equal(cameraStateText({ target: { state: 'searching' } }, true), 'Looking for a blinking light…');
  assert.equal(cameraStateText({ target: { state: 'locked' } }, true), 'Reading the light');
  assert.equal(cameraStateText({ target: { state: 'lost' } }, true), 'Lost the light — keep the camera on it');
  assert.equal(cameraStateText({}, true), 'Looking for a blinking light…', 'old core, nothing read yet');
  assert.equal(cameraStateText({ chars: 3 }, true), 'Reading the light', 'old core reading');
  assert.equal(readerMeta({ wpm: 11.6, target: { state: 'locked', confidence: 0.8 } }, true), '12 WPM · signal good');
  assert.equal(readerMeta({ wpm: 0, target: { state: 'searching', confidence: 0 } }, true), '');
  assert.equal(readerMeta({ wpm: 10 }, true), '10 WPM', 'no target: no signal word');
  assert.equal(readerMeta({ wpm: 12, snr_db: 5, tone_hz: 652 }, false), '12 WPM · signal weak · tone 652 Hz');
  assert.ok(!/%/.test(readerMeta({ wpm: 12, target: { state: 'locked', confidence: 0.2 } }, true)));
  assert.deepEqual(['weak', 'fair', 'good', ''].map((_, i) => confidenceWord([0.1, 0.5, 0.9, null][i])), ['weak', 'fair', 'good', '']);
});

test('ring: the host spec — centred on x/y, diameter max(size × width, 28), only locked (dimmed lost)', () => {
  assert.equal(ringGeometry(null, 400, 300), null);
  assert.equal(ringGeometry({ state: 'searching', x: 0.5, y: 0.5, confidence: 0 }, 400, 300), null);
  const g = ringGeometry({ state: 'locked', x: 0.5, y: 0.5, size: 0.2 }, 400, 300);
  assert.deepEqual([g.x + g.d / 2, g.y + g.d / 2, g.d, g.lost], [200, 150, 80, false]);
  assert.equal(ringGeometry({ state: 'locked', x: 0.5, y: 0.5, size: 0.01 }, 400, 300).d, 28, 'never smaller than 28');
  assert.equal(ringGeometry({ state: 'lost', x: 0.1, y: 0.9, size: 0.02 }, 400, 300).lost, true);
});

// ------------------------------------------------------------------ in-page camera with a fake browser
function fakeBrowser() {
  const listeners = { doc: new Map(), win: new Map() };
  const on = m => (k, f) => { if (!m.has(k)) m.set(k, new Set()); m.get(k).add(f); };
  const off = m => (k, f) => { if (m.has(k)) m.get(k).delete(f); };
  const tracks = [];
  const applied = [];
  globalThis.document = {
    visibilityState: 'visible',
    createElement: () => ({ width: 0, height: 0, getContext: () => ({ drawImage() {}, getImageData: (x, y, w, h) => { const d = new Uint8ClampedArray(w * h * 4); d[0] = d[1] = d[2] = 250; return { data: d }; } }) }),
    addEventListener: on(listeners.doc), removeEventListener: off(listeners.doc),
  };
  const mkStream = () => {
    const t = { stopped: false, stop() { this.stopped = true; }, getSettings: () => ({ width: 1280, height: 720 }), getCapabilities: () => ({ zoom: { min: 1, max: 4 }, exposureCompensation: { min: -2, max: 2 } }), applyConstraints: async c => { applied.push(c.advanced[0]); } };
    tracks.push(t);
    return { getTracks: () => [t], getVideoTracks: () => [t] };
  };
  globalThis.window = { __emberCaptureHook: { camera: () => mkStream() }, addEventListener: on(listeners.win), removeEventListener: off(listeners.win) };
  let cb = null;
  const video = {
    videoWidth: 1280, videoHeight: 720, muted: false, srcObject: null, setAttribute() {}, play: async () => {},
    requestVideoFrameCallback: f => { cb = f; return 1; }, cancelVideoFrameCallback: () => { cb = null; },
  };
  const fire = (where, type) => { for (const f of [...((where === 'doc' ? listeners.doc : listeners.win).get(type) || [])]) f({ type }); };
  const tick = meta => { const f = cb; cb = null; if (f) f(performance.now(), meta); };
  return { tracks, applied, video, fire, tick, listeners };
}

test('in-page camera: frames from requestVideoFrameCallback times, rear camera, torch off, exposure biased down', async () => {
  const fb = fakeBrowser();
  const { CameraReader } = await import('../js/lib/capture_media.js?fb=1');
  const batches = [];
  const r = new CameraReader({ video: fb.video, onBatch: b => batches.push(b) });
  await r.start();
  assert.deepEqual(fb.applied.slice(0, 2), [{ torch: false }, { focusMode: 'continuous' }]);
  assert.ok(fb.applied.some(c => c.exposureCompensation < 0), 'exposure biased down');
  for (let i = 0; i < 8; i++) fb.tick({ captureTime: 5000 + i * 10, mediaTime: 99 }); // 100 fps: 8 frames fit in 100 ms
  assert.equal(batches.length, 1);
  const f = batches[0].frames;
  assert.equal(f.length, 8);
  assert.equal(f[0].t_ms, 5000, 'capture time, not a timer');
  assert.deepEqual([f[0].w, f[0].h], [64, 36]);
  assert.equal(b64ToBytes(f[0].max_b64)[0], 250, 'the bright pixel at the top-left reached cell 0');
  assert.deepEqual(r.zoomSteps(), [1, 2]);
  assert.ok(r.costMs().frames === 8);
  r.stop();
  assert.ok(fb.tracks.every(t => t.stopped));
});

test('fallback for a core without frames: the old watched-region levels', async () => {
  const fb = fakeBrowser();
  const { CameraReader } = await import('../js/lib/capture_media.js?fb=2');
  const batches = [];
  const r = new CameraReader({ video: fb.video, onBatch: b => batches.push(b) });
  await r.start();
  r.setMode('levels');
  for (let i = 0; i < 40; i++) fb.tick({ mediaTime: 1 + i / 30 });
  assert.ok(batches.length >= 1);
  assert.ok(Array.isArray(batches[0].levels) && Array.isArray(batches[0].times_ms));
  assert.equal(batches[0].frames, undefined);
  r.stop();
});

test('the camera is released on stop, when the page is hidden, on pagehide and on restart', async () => {
  for (const how of ['stop', 'hidden', 'pagehide', 'restart']) {
    const fb = fakeBrowser();
    const { CameraReader } = await import('../js/lib/capture_media.js?fb=3' + how);
    const stopped = [];
    const r = new CameraReader({ video: fb.video, onBatch() {}, onStopped: why => stopped.push(why) });
    await r.start();
    if (how === 'stop') r.stop();
    if (how === 'hidden') { document.visibilityState = 'hidden'; fb.fire('doc', 'visibilitychange'); }
    if (how === 'pagehide') fb.fire('win', 'pagehide');
    if (how === 'restart') await r.start();
    assert.ok(fb.tracks[0].stopped, how + ': first camera released');
    assert.equal(fb.video.srcObject === null || how === 'restart', true);
    if (how === 'hidden' || how === 'pagehide') { assert.deepEqual(stopped, [how]); assert.equal(fb.listeners.doc.get('visibilitychange').size, 0); }
    if (how === 'restart') { r.stop(); assert.ok(fb.tracks[1].stopped); }
  }
});

// ------------------------------------------------------------------ mock tracker and contract
test('mock tracker: searching → locked → lost → locked again on a drifting light', () => {
  const sim = new NativeCameraSim({ feed() {}, after() {}, text: 'CQ', wpm: 10 });
  sim.t0 = 0;
  const tr = new LightTracker();
  const states = [];
  for (let t = 0; t < 9000; t += 33) { tr.push(t, 48, 36, sim.frame(t)); const s = tr.view().state; if (states.at(-1) !== s) states.push(s); }
  assert.deepEqual(states, ['searching', 'locked', 'lost', 'locked'], 'the light goes dark for 5 s, then keys again')
  const v = tr.view();
  assert.ok(v.x > 0 && v.x < 1 && v.y > 0 && v.y < 1);
  assert.equal(v.candidates <= 1, true, 'the steady lamp is not a candidate');
  assert.equal(v.pinned, false);
});

test('mock tracker: lost after max(2 s, 7.5 dots), searching again after about 10 s', () => {
  const sim = new NativeCameraSim({ feed() {}, after() {}, text: 'CQ', wpm: 10 });
  sim.t0 = 0;
  const tr = new LightTracker();
  let t = 0, lockedAt = -1;
  for (; t < 4000 && lockedAt < 0; t += 33) { tr.push(t, 48, 36, sim.frame(t)); if (tr.view(120).state === 'locked') lockedAt = t; }
  assert.ok(lockedAt >= 0);
  // the light goes dark: a flat frame
  const dark = new Uint8Array(48 * 36).fill(10);
  const seen = {};
  for (let q = t; q < t + 14000; q += 100) { tr.push(q, 48, 36, dark); const s = tr.view(400).state; if (!(s in seen)) seen[s] = q - t; }
  assert.ok(seen.lost >= 2900 && seen.lost <= 3200, 'lost after 7.5 × 400 ms: ' + seen.lost);
  assert.ok(seen.searching >= 9800 && seen.searching <= 10400, 'searching again after ~10 s: ' + seen.searching);
});

test('state text: a waiting pin, locked, lost, searching', () => {
  assert.equal(cameraStateText({ target: { state: 'searching', pinned: true } }, true), 'Waiting for a light where you tapped… Tap Automatic to search everywhere.');
  assert.equal(cameraStateText({ target: { state: 'searching', pinned: false } }, true), 'Looking for a blinking light…');
  assert.equal(cameraStateText({ target: { state: 'locked', pinned: true } }, true), 'Reading the light');
});

test('mock morse.rx: frames feed, target, pinning, old core rejects frames', async () => {
  const b = new MockBackend({ persist: false, speed: 0.01, autoplay: false, latency: 0, params: new URLSearchParams('') });
  const { session } = await b.call('morse.rx.start', { source: 'light' });
  const sim = new NativeCameraSim({ feed() {}, after() {}, text: 'CQ', wpm: 10 });
  sim.t0 = 0;
  const events = [];
  b.on('morse.target', d => events.push(d.state));
  let st;
  for (let t = 0; t < 8000; t += 100) {
    const frames = [0, 33, 66].map(o => ({ t_ms: t + o, w: 48, h: 36, max_b64: bytesToB64(sim.frame(t + o)) }));
    st = await b.call('morse.rx.feed', { session, frames });
  }
  assert.ok(st.target && ['locked', 'lost'].includes(st.target.state));
  assert.match(st.transcript, /CQ/);
  assert.ok(events.includes('locked'));
  assert.equal(st.target.pinned, false);
  // a tap where no light blinks: the pin waits, the state stays searching
  const tapped = await b.call('morse.rx.target', { session, x: 0.02, y: 0.98 });
  assert.equal(tapped.target.pinned, true);
  assert.equal(tapped.target.state, 'searching');
  assert.equal(cameraStateText(tapped, true), 'Waiting for a light where you tapped… Tap Automatic to search everywhere.');
  const pe = [];
  b.on('morse.target', d => pe.push(d));
  const auto = await b.call('morse.rx.target', { session, auto: true });
  assert.equal(auto.target.pinned, false);
  assert.ok(pe.length && pe.at(-1).pinned === false && Number.isInteger(pe.at(-1).candidates), 'the event carries candidates and pinned');
  await assert.rejects(b.call('morse.rx.target', { session, x: 2, y: 0 }), { code: 'bad_params' });
  await assert.rejects(b.call('morse.rx.target', { session, x: 0.5, y: 0.5, auto: true }), { code: 'bad_params' });
  // feed limits: 1–64 frames, each one checked before any is used
  const one = { t_ms: 1, w: 1, h: 1, max_b64: 'AA==' };
  await b.call('morse.rx.feed', { session, frames: new Array(64).fill(one) });
  await assert.rejects(b.call('morse.rx.feed', { session, frames: new Array(65).fill(one) }), { code: 'too_large' });
  await assert.rejects(b.call('morse.rx.feed', { session, frames: [] }), { code: 'bad_params' });
  await assert.rejects(b.call('morse.rx.feed', { session, frames: [one, { t_ms: 2, w: 2, h: 1, max_b64: 'AA==' }] }), { code: 'bad_params' });
  await assert.rejects(b.call('morse.rx.feed', { session, frames: [{ t_ms: 1, w: 100, h: 41, max_b64: '' }] }), { code: 'too_large' });
  // a session fed with frames refuses levels, and one call never carries both
  await assert.rejects(b.call('morse.rx.feed', { session, times_ms: [1], levels: [0.5] }), { code: 'bad_params' });
  await b.call('morse.rx.reset', { session });
  await assert.rejects(b.call('morse.rx.feed', { session, times_ms: [1], levels: [0.5] }), { code: 'bad_params' });
  const s3 = (await b.call('morse.rx.start', { source: 'light' })).session;
  await assert.rejects(b.call('morse.rx.feed', { session: s3, frames: [one], times_ms: [1], levels: [0.5] }), { code: 'bad_params' });
  await b.call('morse.rx.feed', { session: s3, times_ms: [1], levels: [0.5] });
  b.close();
  const old = new MockBackend({ persist: false, speed: 0.01, autoplay: false, latency: 0, params: new URLSearchParams('morseframes=0') });
  const s2 = (await old.call('morse.rx.start', { source: 'light' })).session;
  await assert.rejects(old.call('morse.rx.feed', { session: s2, frames: [{ t_ms: 1, w: 1, h: 1, max_b64: 'AA==' }] }), { code: 'bad_params' }, 'an old core: the UI falls back to levels');
  const lv = await old.call('morse.rx.feed', { session: s2, times_ms: [1, 2], levels: [0.1, 0.9] });
  assert.equal(lv.target, undefined, 'no ring for an old core');
  old.close();
});

test('mock native camera: host.capture.start feeds the tracker; stop ends it', async () => {
  const b = new MockBackend({ persist: false, speed: 0.02, autoplay: false, latency: 0, params: new URLSearchParams('nativecam=1') });
  assert.ok((await b.call('host.info')).features.includes('capture_camera_luma'));
  const { session } = await b.call('morse.rx.start', { source: 'light' });
  await b.call('host.capture.start', { kind: 'camera_luma', session, rect: { x: 0, y: 0, w: 300, h: 200 } });
  await new Promise(r => setTimeout(r, 400));
  const st = await b.call('morse.rx.status', { session });
  assert.ok(st.target && st.target.state !== undefined);
  await b.call('host.capture.stop', { kind: 'camera_luma' });
  assert.ok(!b._cam);
  b.close();
});
