// Sound-card transmit path (endpoints/audio_worklet.js, endpoints/audio.js): the worklet's
// low-water mark and drain report, and the PTT release that waits for it — these key a real
// transmitter, so every bound is checked: release after the last sample, never stuck on.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import audio, { spBufferSize } from '../js/endpoints/audio.js';

const RATE = 48000;

/** The real worklet processor in a vm, with a clock and a port whose messages are collected. */
function worklet() {
  const src = fs.readFileSync(new URL('../js/endpoints/audio_worklet.js', import.meta.url), 'utf8');
  const sent = [];
  const clock = { frames: 0 };
  let Cls = null;
  const ctx = {
    AudioWorkletProcessor: class { constructor() { this.port = { onmessage: null, postMessage: m => sent.push(m) }; } },
    registerProcessor: (_n, c) => { Cls = c; },
  };
  Object.defineProperty(ctx, 'sampleRate', { get: () => RATE });
  Object.defineProperty(ctx, 'currentTime', { get: () => clock.frames / RATE });
  vm.runInNewContext(src, ctx);
  const p = new Cls();
  const out = new Float32Array(128);
  return {
    p, sent, clock, out,
    msg: m => p.port.onmessage({ data: m }),
    quantum() { p.process([[]], [[out]]); clock.frames += 128; },
  };
}

test('worklet: asks for more below 250 ms queued and tops up to 500 ms', () => {
  const w = worklet();
  w.quantum();
  const ask = w.sent.find(m => m.need !== undefined);
  assert.equal(ask.need, RATE * 0.5, 'empty queue: asks for twice the low-water mark');
  w.msg({ pcm: new Float32Array(RATE * 0.5).fill(0.5), more: true });
  w.sent.length = 0;
  // plays down to 250 ms without asking, then asks once for the difference to 500 ms
  let before = 0, asked = null;
  for (let i = 0; i < 2000 && !asked; i++) { before = w.p.queued; w.quantum(); asked = w.sent.find(m => m.need !== undefined); }
  assert.ok(asked);
  assert.ok(before >= RATE * 0.25 && w.p.queued < RATE * 0.25, 'asked on crossing 250 ms (' + before + ' → ' + w.p.queued + ')');
  assert.equal(asked.need, RATE * 0.5 - w.p.queued);
  assert.equal(w.p.st.underruns, 0);
});

test('worklet: survives a 200 ms main-thread round trip without a gap (120 ms did not)', () => {
  const w = worklet();
  const pending = [];
  const delay = Math.round(0.2 * RATE); // 100 ms each way
  let produced = 0;
  for (let q = 0; q < 300; q++) {
    for (const m of w.sent.splice(0)) if (m.need !== undefined) pending.push({ at: w.clock.frames + delay, n: m.need });
    while (pending.length && pending[0].at <= w.clock.frames) {
      const { n } = pending.shift();
      produced += n;
      w.msg({ pcm: new Float32Array(n).fill(0.5), more: true });
    }
    w.quantum();
  }
  assert.ok(produced > 0);
  // the first ask waits a full round trip on an empty queue (that is before any audio): count after it
  const st = w.p.st;
  assert.ok(st.played > RATE * 0.5);
  assert.equal(st.underrun_samples, 0, JSON.stringify(st));
});

test('worklet: the drain report comes only once the last queued sample has been rendered', () => {
  const w = worklet();
  w.msg({ pcm: new Float32Array(1000).fill(0.5), more: false });
  w.msg({ drain: 7 });
  const drained = () => w.sent.filter(m => m.drained !== undefined);
  for (let i = 0; i < 7; i++) { w.quantum(); assert.equal(drained().length, 0, 'still queued after quantum ' + i); }
  w.quantum(); // 8 × 128 = 1024 ≥ 1000
  assert.equal(w.p.queued, 0);
  assert.deepEqual(drained().map(m => m.drained), [7]);
  assert.equal(drained()[0].t, (7 * 128) / RATE, 'stamped with the render time of the last sample');
  w.quantum();
  assert.equal(drained().length, 1, 'reported once');
  // already empty: answered at the next quantum
  w.msg({ drain: 8 });
  w.quantum();
  assert.deepEqual(drained().map(m => m.drained), [7, 8]);
});

// ------------------------------------------------------------------ PTT release in the endpoint
/**
 * An open audio endpoint with fake Web Audio and a fake serial RTS line. Timers are node:test's
 * mocked ones; the audio clock follows them (ac.currentTime = mocked ms / 1000).
 */
function endpoint(t, { mode = 'worklet', latency = 0 } = {}) {
  t.mock.timers.enable({ apis: ['setTimeout', 'Date'], now: 0 });
  const rts = [];
  const posted = [];
  const ep = audio.create({ ptt: 'serial_rts' }, { env: globalThis, log: { warn() {}, log() {}, error() {} } });
  // the audio clock follows the wall clock unless a test holds it back (clock.lag s) or stops it (clock.frozen s)
  const clock = { lag: 0, frozen: null };
  ep.ac = { get currentTime() { return clock.frozen !== null ? clock.frozen : Date.now() / 1000 - clock.lag; }, state: 'running', outputLatency: latency, baseLatency: 0, close: async () => {} };
  ep.sampleRate = RATE;
  ep.mode = mode;
  ep.node = mode === 'worklet' ? { port: { postMessage: m => posted.push(m) }, disconnect() {} } : { disconnect() {} };
  ep.pttPort = { setSignals: async s => { rts.push({ on: s.requestToSend, ms: Date.now() }); }, close: async () => {} };
  const tick = async ms => { for (let i = 0; i < ms; i++) { t.mock.timers.tick(1); await Promise.resolve(); await Promise.resolve(); } };
  const lastRts = () => (rts.length ? rts.at(-1).on : false);
  return { ep, rts, posted, tick, lastRts, clock };
}

test('PTT (worklet): released only after the worklet reports its queue empty, plus the tail', async t => {
  const { ep, rts, posted, tick, lastRts } = endpoint(t, { latency: 0.02 });
  await ep.setPtt(true);
  ep._notePlayout(RATE * 0.5); // half a second scheduled from the main thread
  const off = ep.setPtt(false);
  await tick(1);
  const req = posted.find(m => m.drain !== undefined);
  assert.ok(req, 'asks the worklet to report its queue empty');
  // the prediction (500 ms + 20 ms latency + 30 ms tail) passes: the worklet still plays (delivery delay)
  await tick(800);
  assert.equal(lastRts(), true, 'held past the main-thread prediction while the worklet still plays');
  ep._onWorklet({ drained: req.drain + 1, t: 0.8 }); // a stale report is ignored
  await tick(100);
  assert.equal(lastRts(), true);
  ep._onWorklet({ drained: req.drain, t: 0.9 });
  await tick(49);
  assert.equal(lastRts(), true, 'tail: output latency 20 ms + 30 ms');
  await tick(2);
  await off;
  assert.equal(lastRts(), false);
  const releasedAt = rts.at(-1).ms;
  assert.ok(releasedAt >= 950 && releasedAt <= 952, 'released at ' + releasedAt);
  assert.equal(ep.bursts.length, 1);
  assert.ok(ep.bursts[0].off_audio >= ep.bursts[0].play_end, 'released after the last sample played');
  assert.equal(ep.bursts[0].drained_audio, 0.9);
});

test('PTT (worklet): a worklet that never reports still releases (prediction + 1 s, at most 6 s)', async t => {
  const { ep, rts, tick, lastRts } = endpoint(t);
  await ep.setPtt(true);
  ep._notePlayout(RATE * 0.5);
  const off = ep.setPtt(false);
  await tick(1500);
  assert.equal(lastRts(), true);
  await tick(40);
  await off;
  assert.equal(lastRts(), false, 'released by the hard bound');
  assert.ok(rts.at(-1).ms <= 0.53 * 1000 + 1000 + 1);
  // an absurd prediction is still capped
  await ep.setPtt(true);
  ep._notePlayout(RATE * 60);
  const t0 = Date.now();
  const off2 = ep.setPtt(false);
  await tick(6001);
  await off2;
  assert.equal(lastRts(), false);
  assert.ok(rts.at(-1).ms - t0 <= 6000, 'never more than 6 s after the core dropped PTT');
});

test('PTT (worklet): close and a new key-up while a release is pending', async t => {
  const { ep, rts, posted, tick, lastRts } = endpoint(t);
  await ep.setPtt(true);
  ep._notePlayout(RATE * 0.2);
  const off = ep.setPtt(false);
  await tick(5);
  // the core keys up again before the drain: the line never drops, the pending release is dropped
  await ep.setPtt(true);
  await off;
  const req = posted.find(m => m.drain !== undefined);
  ep._onWorklet({ drained: req.drain, t: 0.01 });
  await tick(2000);
  assert.equal(lastRts(), true, 'still keyed for the second transmission');
  assert.equal(rts.filter(r => !r.on).length, 0, 'never dropped between the two');
  // close while a release waits: PTT off at once
  ep._notePlayout(RATE * 1);
  ep.setPtt(false);
  await tick(5);
  assert.equal(lastRts(), true);
  const before = Date.now();
  const closing = ep.close();
  await tick(1);
  await closing;
  assert.equal(lastRts(), false, 'close releases immediately');
  assert.ok(rts.at(-1).ms - before <= 1);
});

test('PTT: a release with nothing keyed applies at once (the line is driven low on open)', async t => {
  const { ep, rts, posted } = endpoint(t);
  await ep.setPtt(false);
  assert.deepEqual(rts.map(r => r.on), [false]);
  assert.equal(posted.length, 0, 'no drain wait');
});

test('PTT (ScriptProcessor): each buffer plays one buffer after its callback, so the hold counts it', async t => {
  const { ep, rts, tick, lastRts } = endpoint(t, { mode: 'scriptprocessor' });
  await ep.setPtt(true);
  // one callback at t = 0 filled a 4096-frame buffer with 4096 samples: it plays 85–171 ms
  ep._notePlayout(4096, 4096);
  const off = ep.setPtt(false);
  const end = Math.ceil(((2 * 4096) / RATE + 0.03) * 1000);
  await tick(end - 1);
  assert.equal(lastRts(), true);
  await tick(2);
  await off;
  assert.equal(lastRts(), false);
  assert.ok(rts.at(-1).ms >= ((2 * 4096) / RATE) * 1000, 'after the last sample of the buffer played');
});

test('PTT (ScriptProcessor): an audio clock that fell behind the wall clock holds PTT until the last sample has played', async t => {
  const { ep, tick, lastRts, rts, clock } = endpoint(t, { mode: 'scriptprocessor' });
  await ep.setPtt(true);
  ep._notePlayout(4096, 4096);
  const off = ep.setPtt(false);
  const end = Math.ceil(((2 * 4096) / RATE + 0.03) * 1000);
  clock.lag = 0.2; // the device underran for 200 ms after the release was timed
  await tick(end + 1);
  assert.equal(lastRts(), true, 'the timer fired, the audio clock is 200 ms short: still keyed');
  await tick(197);
  assert.equal(lastRts(), true);
  await tick(5);
  await off;
  assert.equal(lastRts(), false);
  assert.ok(rts.at(-1).ms >= end + 199, 'released once the audio clock reached the end of the last sample');
});

test('PTT (ScriptProcessor): an audio clock that stops still releases, at most 6 s after the core dropped PTT', async t => {
  const { ep, tick, lastRts, rts, clock } = endpoint(t, { mode: 'scriptprocessor' });
  await ep.setPtt(true);
  ep._notePlayout(4096, 4096);
  clock.frozen = 0;
  const off = ep.setPtt(false);
  await tick(5990);
  assert.equal(lastRts(), true);
  await tick(20);
  await off;
  assert.equal(lastRts(), false);
  assert.ok(rts.at(-1).ms <= 6005, `released at ${rts.at(-1).ms} ms`);
});

test('PTT (ScriptProcessor): an audio context that is not running releases at the predicted time', async t => {
  const { ep, tick, lastRts, clock } = endpoint(t, { mode: 'scriptprocessor' });
  await ep.setPtt(true);
  ep._notePlayout(4096, 4096);
  const off = ep.setPtt(false);
  clock.frozen = 0.05;
  ep.ac.state = 'suspended';
  await tick(Math.ceil(((2 * 4096) / RATE + 0.03) * 1000) + 1);
  await off;
  assert.equal(lastRts(), false);
});

// ------------------------------------------------------------------ ScriptProcessor buffer
test('ScriptProcessor buffer: at least 160 ms (a callback may run that late), power of two', () => {
  assert.equal(spBufferSize(48000), 8192);
  assert.equal(spBufferSize(44100), 8192);
  assert.equal(spBufferSize(16000), 4096);
  assert.equal(spBufferSize(96000), 16384);
  for (const r of [16000, 22050, 44100, 48000]) assert.ok(spBufferSize(r) / r >= 0.16, r + ' Hz');
});

test('ScriptProcessor endpoint: opens with that buffer, receives whole blocks, PTT held one buffer past the callback', async t => {
  const created = [];
  const rx = [];
  let txLeft = 0;
  const rts = [];
  const port = { getInfo: () => ({}), async open() {}, async close() {}, async setSignals(s) { rts.push({ on: s.requestToSend, ms: Date.now() }); } };
  class AC {
    constructor() { this.sampleRate = RATE; this.state = 'running'; this.outputLatency = 0; this.baseLatency = 0; this.destination = {}; }
    get currentTime() { return Date.now() / 1000; }
    async resume() {} async close() {}
    createMediaStreamSource() { return { connect() {}, disconnect() {} }; }
    createScriptProcessor(size, i, o) { const sp = { bufferSize: size, i, o, connect() {}, disconnect() {}, onaudioprocess: null }; created.push(sp); return sp; }
  }
  const env = Object.create(globalThis);
  const own = o => { for (const [k, v] of Object.entries(o)) Object.defineProperty(env, k, { value: v, writable: true, configurable: true, enumerable: true }); };
  own({
    AudioContext: AC, AudioWorkletNode: undefined, isSecureContext: true, location: { protocol: 'file:' }, document: undefined,
    navigator: { mediaDevices: { getUserMedia: async () => ({ getTracks: () => [{ stop() {} }] }) }, serial: { getPorts: async () => [port], requestPort: async () => port } },
  });
  const ep = audio.create({ ptt: 'serial_rts' }, {
    env, log: { warn() {}, log() {}, error() {} },
    audioRx: pcm => rx.push(pcm.length),
    audioTx: n => { const k = Math.min(n, txLeft); txLeft -= k; return new Int16Array(k).fill(1000); },
  });
  await ep.open();
  assert.equal(ep.mode, 'scriptprocessor');
  assert.equal(created.length, 1);
  assert.equal(created[0].bufferSize, 8192, '8192 frames at 48 kHz');
  const sp = created[0];
  const block = () => { const inp = new Float32Array(8192), out = new Float32Array(8192); sp.onaudioprocess({ inputBuffer: { getChannelData: () => inp }, outputBuffer: { getChannelData: () => out } }); return out; };
  t.mock.timers.enable({ apis: ['setTimeout', 'Date'], now: 0 });
  const tick = async ms => { for (let i = 0; i < ms; i++) { t.mock.timers.tick(1); await Promise.resolve(); await Promise.resolve(); } };
  block();
  assert.deepEqual(rx, [8192], 'microphone capture through the same node: whole blocks');
  await ep.setPtt(true);
  txLeft = 6000; // the last 6000 samples of a frame, in one callback at t = 0
  const out = block();
  assert.equal(out[0] !== 0 && out[5999] !== 0 && out[6000] === 0, true);
  const off = ep.setPtt(false);
  // that buffer plays after the one playing now: from 8192 to 14192 frames; then the 30 ms tail
  const endMs = (8192 + 6000) / RATE * 1000;
  await tick(Math.floor(endMs));
  assert.equal(rts.at(-1).on, true, 'held while the last buffer still plays');
  await tick(Math.ceil(endMs + 30 - Math.floor(endMs)) + 1);
  await off;
  assert.equal(rts.at(-1).on, false);
  assert.ok(rts.at(-1).ms >= endMs, 'released after the last sample: ' + rts.at(-1).ms + ' ≥ ' + endMs.toFixed(1));
  assert.ok(rts.at(-1).ms <= endMs + 32);
  await ep.close();
});
