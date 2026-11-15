// Sample-time twin of the real-time audio tests (e2e_audio.test.mjs; docs/CI.md): the same
// cross-wired pair of nodes, but on a virtual sound card (lib/virtual_audio.mjs) whose clock is the
// sample counter. Real core (WASM in Node), real audio endpoint (ui/js/endpoints/audio.js, created
// by the host router through host.link.create), real AudioWorklet processor code; the timers the
// endpoint uses for the PTT hang time are mocked and advance with the audio. Asserts, in samples:
//   - B decodes A's Public message from A's output
//   - the chunks the app schedules are contiguous: no underrun while the core is mid-transmission
//   - serial-RTS PTT is high from before the first transmitted sample to after the last one, and
//     drops after every burst
// Runs on every host (needs only the WASM build): the result does not depend on the host's speed.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { wasmModuleUrl, ui, haveWasm, NO_WASM } from './helpers.mjs';
import { VirtualAudio } from './lib/virtual_audio.mjs';

const skip = haveWasm() ? false : (process.env.EMBER_REQUIRE_BROWSER === '1' ? false : NO_WASM);
const LINK_CFG = { modem: 'afsk1200', licence: 'licence_free', csma: false, persistence: 255, txdelay_ms: 150, txtail_ms: 50, aprs: false };

async function pair(t, { worklet, latencyMs }) {
  if (!haveWasm()) throw new Error(NO_WASM);
  const { default: WasmBackend } = await import(ui('backends/wasm.js'));
  const va = new VirtualAudio({ latencyMs });
  // fake Web Serial port for A's RTS PTT: every level change stamped with the sample clock
  const rts = [];
  const port = { getInfo: () => ({}), async open() {}, async close() {}, async setSignals(s) { if ('requestToSend' in s) rts.push({ on: s.requestToSend, frame: va.frames }); } };
  const serial = { getPorts: async () => [port], requestPort: async () => port, addEventListener() {}, removeEventListener() {} };
  let epA = null;
  const envA = va.env({ worklet, serial });
  const envB = va.env({ worklet, mic: va.outputOf(() => epA && epA.node) });
  const mk = async (env, name) => {
    const b = new WasmBackend({ moduleUrl: wasmModuleUrl(), storage: 'memory', hub: false, guides: false, restoreLinks: false, env, log: { info() {}, warn() {}, error: console.error, debug() {}, log() {} } });
    await b.init();
    await b.call('node.set_profile', { name });
    return b;
  };
  const A = await mk(envA, 'Radio A');
  const B = await mk(envB, 'Radio B');
  t.after(async () => { t.mock.timers.reset(); await A.destroy().catch(() => {}); await B.destroy().catch(() => {}); });
  await new Promise((r) => setTimeout(r, 50)); // first ticks on real timers
  t.mock.timers.enable({ apis: ['setTimeout', 'setInterval', 'Date'], now: Date.now() });
  va.timers = t.mock.timers;
  // host.link.create opens the endpoint (getUserMedia, worklet module, PTT port): drive the clock meanwhile
  const settle = async (p) => { let r, e, done = false; p.then((x) => { r = x; done = true; }, (x) => { e = x; done = true; }); await va.run(10, () => done); if (e) throw e; if (!done) throw new Error('timed out on the virtual clock'); return r; };
  const la = (await settle(A.call('host.link.create', { name: 'Radio A', endpoint: { kind: 'audio', config: { ptt: 'serial_rts', tx_level: 80 } }, link: { type: 'audio', config: { ...LINK_CFG, callsign: 'TEST1' } } }))).link;
  const lb = (await settle(B.call('host.link.create', { name: 'Radio B', endpoint: { kind: 'audio', config: { ptt: 'vox', rx_gain: 1 } }, link: { type: 'audio', config: { ...LINK_CFG, callsign: 'TEST2' } } }))).link;
  epA = A.router.links.get(la.id).endpoint;
  const epB = B.router.links.get(lb.id).endpoint;
  await settle(B.call('contacts.add', { card: (await settle(A.call('node.info'))).card }));
  return { va, A, B, epA, epB, la, lb, rts, settle };
}

async function transmit(t, opts) {
  const { va, A, B, epA, rts, settle } = await pair(t, opts);
  assert.equal(epA.mode, opts.worklet ? 'worklet' : 'scriptprocessor');
  const nodeA = epA.node;
  // what A's speaker played, per quantum: first and last frame with signal, and RTS at each of them
  let first = -1, last = -1;
  const unkeyed = [];
  const tape = (frame) => {
    const out = nodeA.lastOut;
    if (!out) return;
    let any = false;
    for (let i = 0; i < out.length; i++) if (out[i] !== 0) { any = true; break; }
    if (!any) return;
    if (first < 0) first = frame;
    last = frame + out.length;
    if (!epA.ptt && !epA._burst && unkeyed.length < 5) unkeyed.push(frame);
  };
  const text = `sample time ${opts.worklet ? 'worklet' : 'scriptprocessor'} ${opts.latencyMs} ms`;
  await settle(A.call('messages.send', { conv: 'ch:public', text }));
  let got = null, n = 0;
  const idle = () => !epA.ptt && !epA._burst && !epA._pttCancel;
  const t0 = va.currentTime;
  await va.run(30, async () => {
    if (++n % 64) return false; // poll every ~170 ms of audio
    if (!got) got = (await B.call('messages.list', { conv: 'ch:public' })).messages.find((m) => m.text === text) || null;
    return !!got && idle() && va.currentTime - t0 > 1;
  }, tape);
  const st = opts.worklet ? { ...nodeA.proc.st } : null;
  t.diagnostic(`${text}: decoded ${got ? 'yes' : 'no'} after ${(va.currentTime - t0).toFixed(2)} s of audio; tx ${epA.txSamples} samples, first ${first}, last ${last}; worklet ${JSON.stringify(st)}; RTS ${JSON.stringify(rts)}; bursts ${JSON.stringify(epA.bursts.map((b) => ({ samples: b.samples, on: b.on_audio, off: b.off_audio, play_end: b.play_end })))}`);
  assert.ok(got, 'B decoded A\'s message from A\'s output, in sample time');
  assert.equal(got.from, (await A.call('node.info')).node_id);
  assert.ok(epA.txSamples > va.rate * 0.2, 'core produced audio');
  if (st) {
    assert.equal(st.underruns, 0, `contiguous chunks: no underrun while transmitting (${JSON.stringify(st)})`);
    assert.equal(st.played, epA.txSamples, 'every scheduled sample was played');
  }
  // PTT in samples: keyed before the first sample, released after the last, low at the end
  // (the line is driven low once when the endpoint opens)
  while (rts.length && !rts[0].on) rts.shift();
  assert.ok(rts.length >= 2, `RTS changes ${JSON.stringify(rts)}`);
  assert.deepEqual(unkeyed, [], 'no audio played while PTT was released');
  const ons = rts.filter((r, i) => r.on && (i === 0 || !rts[i - 1].on));
  assert.equal(ons.length, epA.bursts.length, 'one key-up per transmission');
  assert.ok(rts[0].frame <= first, `RTS up at frame ${rts[0].frame}, first sample at ${first}`);
  assert.equal(rts.at(-1).on, false, 'RTS low at the end');
  assert.ok(rts.at(-1).frame >= last, `RTS down at frame ${rts.at(-1).frame}, last sample played at ${last}`);
  for (const b of epA.bursts) assert.ok(b.off_audio >= b.play_end - 1e-9, `released at ${b.off_audio} s, last sample of the burst at ${b.play_end} s`);
}

test('audio playout in sample time (worklet): contiguous chunks, PTT held from first to last sample, B decodes (twin of the cross-wired test)', { skip, timeout: 120000 }, (t) => transmit(t, { worklet: true, latencyMs: 3 }));
test('audio playout in sample time (ScriptProcessor, the single file): PTT held from first to last sample, B decodes (twin of the single-file test)', { skip, timeout: 120000 }, (t) => transmit(t, { worklet: false, latencyMs: 3 }));
