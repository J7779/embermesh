// Morse capture helpers (ui/js/endpoints/capture.js): feed batching / back-pressure and the
// capture wake lock, with fakes. The end-to-end proof is in e2e_morse.test.mjs.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { ui, sleep } from './helpers.mjs';

const { FeedBatcher, CaptureWake, b64encode, b64decode } = await import(ui('endpoints/capture.js'));
const { KeepAwake } = await import(ui('endpoints/device.js'));

function slowSender(ms) {
  const sent = [];
  const send = (p) => { sent.push(p); return new Promise((r) => setTimeout(() => r({ chars: sent.length }), ms)); };
  return { sent, send };
}

test('FeedBatcher: one feed in flight per session, later feeds merged (light, both forms)', async () => {
  const { sent, send } = slowSender(30);
  const b = new FeedBatcher(send);
  const all = [];
  all.push(b.feed({ session: 1, t_ms: 0, dt_ms: 33, levels: [0.1, 0.2] }));
  all.push(b.feed({ session: 1, t_ms: 66, dt_ms: 33, levels: [0.3] }));
  all.push(b.feed({ session: 1, times_ms: [99, 132], levels: [0.4, 0.5] }));
  all.push(b.feed({ session: 2, t_ms: 0, dt_ms: 10, levels: [1] }));
  const res = await Promise.all(all);
  assert.equal(sent.length, 3, 'session 1: first alone, the next two merged; session 2 separate');
  assert.deepEqual(sent[0], { session: 1, times_ms: [0, 33], levels: [0.1, 0.2] });
  const merged = sent.find((p) => p.session === 1 && p.levels.length === 3);
  assert.deepEqual(merged, { session: 1, times_ms: [66, 99, 132], levels: [0.3, 0.4, 0.5] });
  assert.equal(res[1], res[2], 'merged callers share the reply');
  assert.equal(b.stats.merged, 1);
});

test('FeedBatcher: audio PCM concatenated; backlog capped while congested (oldest dropped)', async () => {
  const { sent, send } = slowSender(5);
  let congested = true;
  const b = new FeedBatcher(send, { congested: () => congested, maxAudioBytes: 1000, maxLightSamples: 10, pollMs: 5 });
  const pcm = (n, v) => b64encode(new Uint8Array(n).fill(v));
  const ps = [];
  for (let i = 0; i < 10; i++) ps.push(b.feed({ session: 'a', pcm_b64: pcm(200, i) }));
  for (let i = 0; i < 30; i++) ps.push(b.feed({ session: 'l', times_ms: [i], levels: [i / 30] }));
  await sleep(20);
  assert.equal(sent.length, 0, 'nothing sent while the transport is congested');
  congested = false;
  await Promise.all(ps);
  const a = sent.find((p) => p.session === 'a');
  const bytes = b64decode(a.pcm_b64);
  assert.equal(bytes.length, 1000, 'audio backlog capped');
  assert.equal(bytes[0], 5, 'oldest chunks dropped first');
  assert.equal(bytes.at(-1), 9);
  const l = sent.find((p) => p.session === 'l');
  assert.deepEqual(l.times_ms, [20, 21, 22, 23, 24, 25, 26, 27, 28, 29]);
  assert.equal(b.stats.dropped_samples, 500 + 20);
});

test('FeedBatcher: errors reject every merged caller; the session keeps working', async () => {
  let fail = true;
  const b = new FeedBatcher(async () => { await sleep(5); if (fail) throw Object.assign(new Error('x'), { code: 'not_found' }); return { ok: 1 }; });
  const p1 = b.feed({ session: 1, levels: [1], t_ms: 0, dt_ms: 1 });
  const p2 = b.feed({ session: 1, levels: [1], t_ms: 1, dt_ms: 1 });
  await assert.rejects(p1, { code: 'not_found' });
  fail = false;
  assert.deepEqual(await p2, { ok: 1 });
});

test('CaptureWake: screen held while a session is open; released on stop, gone session, idle', async () => {
  const calls = [];
  const ka = { want: async (h, on) => { calls.push([h, on]); } };
  const w = new CaptureWake(() => ka, { idleMs: 40 });
  w.observe('morse.rx.start', { source: 'light' }, { session: 3 });
  w.observe('morse.rx.start', { source: 'audio' }, { session: 4 });
  w.observe('morse.rx.feed', { session: 3 }, { chars: 0 });
  w.observe('morse.rx.stop', { session: 3 }, { transcript: '' });
  w.observe('morse.rx.feed', { session: 4 }, null, { code: 'not_found' });
  w.observe('morse.rx.start', {}, { session: 5 });
  await sleep(120);
  w.observe('messages.send', {}, {});
  assert.deepEqual(calls, [['capture:3', true], ['capture:4', true], ['capture:3', false], ['capture:4', false], ['capture:5', true], ['capture:5', false]]);
  w.close();
});

test('KeepAwake: independent holders (user switch + captures), re-acquired when visible again', async () => {
  const locks = [];
  const listeners = {};
  const env = {
    isSecureContext: true,
    document: { visibilityState: 'visible', addEventListener: (n, f) => { listeners[n] = f; }, removeEventListener: (n) => { delete listeners[n]; } },
    navigator: { wakeLock: { request: async () => { const l = { released: false, addEventListener: (n, f) => { l.onrelease = f; }, release: async () => { l.released = true; } }; locks.push(l); return l; } } },
  };
  const k = new KeepAwake(env);
  await k.set(true);
  await k.want('capture:1', true);
  assert.equal(locks.length, 1);
  await k.set(false);
  assert.equal(locks[0].released, false, 'capture still holds it');
  locks[0].onrelease(); // the browser drops the lock when the page is hidden
  listeners.visibilitychange();
  await sleep(1);
  assert.equal(locks.length, 2, 're-acquired on visibility');
  await k.want('capture:1', false);
  assert.equal(locks[1].released, true);
  assert.equal(listeners.visibilitychange, undefined);
  await assert.rejects(new KeepAwake({ isSecureContext: false, navigator: {} }).want('x', true), { code: 'unsupported' });
});
