// The `audio` endpoint with the real soft modem, in real headless Chrome:
//
//  1. Cross-wired Web Audio loopback in one page: node A (the page's node) transmits through its
//     real audio endpoint (core modulator -> ember_audio_tx -> AudioWorklet playout); the worklet's
//     output is routed through a MediaStreamAudioDestinationNode into node B's audio endpoint as if
//     it were B's microphone (getUserMedia -> AudioWorklet capture -> ember_audio_rx -> demodulator).
//     B must decode A's Public message. Also checks: worklet playout had no underruns while the
//     core was mid-transmission (the whole test needs a host that passes the real-time probe; its
//     sample-time twin is audio_playout.test.mjs), link.ptt on/off fired, and serial-RTS PTT (a fake Web Serial port)
//     was keyed for the whole transmission and released only after the audio had played out.
//  2. Fake microphone: a WAV produced by the core's own modulator (in Node) is fed to Chrome with
//     --use-file-for-fake-audio-capture; the page's audio link (real getUserMedia path) must decode it.
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { DIST_DIR, wasmModuleUrl, ui, haveWasm } from './helpers.mjs';
import { e2eSkip, startBrowser, nodePage, realErrors, tmpDir } from './lib/e2e.mjs';
import { runCleanups, HOOK, realtimeSkip } from './lib/e2e.mjs';
import { startServer } from './lib/static_server.mjs';

const skip = e2eSkip([!haveWasm() && 'WASM build missing']);
const cleanups = [];
after(async () => { await runCleanups(cleanups); }, HOOK);

const MEDIA_FLAGS = ['--use-fake-ui-for-media-stream', '--use-fake-device-for-media-stream', '--autoplay-policy=no-user-gesture-required'];
// Robust defaults for a test channel: licence-free (no callsign), no CSMA/persistence delays.
// Each station needs its own callsign: the AX.25 carrier drops frames whose source equals its own
// callsign, and every node defaults to N0CALL (see apps/web/README.md §10).
const LINK_CFG = { modem: 'afsk1200', licence: 'licence_free', csma: false, persistence: 255, txdelay_ms: 150, txtail_ms: 50, aprs: false };
const cfg = (callsign) => ({ ...LINK_CFG, callsign });

// Real getUserMedia({audio}) if it answers within 5 s, else (in some locked-down environments headless Chrome waits forever
// for the OS microphone permission, even with fake devices) a Web Audio generated stream in its place.
// Resolves to 'real' | 'substitute'; the stream factory decides what the "microphone" hears.
const MIC_PROBE = `(async () => {
  const md = navigator.mediaDevices;
  if (!globalThis.__realGum) globalThis.__realGum = md.getUserMedia.bind(md);
  const r = await Promise.race([globalThis.__realGum({ audio: true }).then((s) => { s.getTracks().forEach((t) => t.stop()); return 'real'; }, (e) => 'error:' + e.name),
    new Promise((res) => setTimeout(() => res('timeout'), 5000))]);
  return r;
})()`;

// Probed once in a throw-away browser: a hanging getUserMedia wedges that browser's audio.
let MIC = 'unknown';
before(async () => {
  if (skip) return;
  const b = await startBrowser(MEDIA_FLAGS);
  if (b.error) return;
  const srv = await startServer(DIST_DIR);
  try {
    const P = await nodePage(b, `${srv.url}index.html?nohub=1&profile=micprobe`);
    MIC = await P.evaluate(MIC_PROBE);
  } catch (e) { MIC = `probe failed: ${e.message}`; } finally { await runCleanups([() => b.close(), () => srv.close()]); }
  console.log(`# real getUserMedia(audio) in this headless Chrome: ${MIC}${MIC === 'real' ? '' : ' -> microphone replaced by a Web Audio stream'}`);
}, HOOK);

// Both nodes quiet before the test frame: no beacons, no relaying, and both endpoints idle for
// 1.5 s (a half-duplex modem is deaf while it transmits: B keying up during A's frame loses it).
// Returns a page expression giving how often B keyed up since (0 = B stayed quiet).
async function quiet(A) {
  await A.evaluate(`Promise.all([globalThis.__emberWasmBackend, globalThis.__B].map((b) => b.call('settings.set', { settings: { beacon_interval_s: 0, relay_enabled: false } }))).then(() => true)`);
  await A.waitFor(`(() => { const idle = [__epA, __epB].every((e) => !e.ptt && !e._burst && !e._pttCancel); const now = performance.now(); if (!idle || !globalThis.__quietSince) globalThis.__quietSince = idle ? (globalThis.__quietSince || now) : 0; return idle && now - __quietSince >= 1500; })()`, 30000);
  await A.evaluate('(globalThis.__bKeys0 = __epB.pttEvents.filter((e) => e.on).length, true)');
  return '__epB.pttEvents.filter((e) => e.on).length - __bKeys0';
}

// What B's microphone is handed, sample by sample (both capture paths end in the endpoint's _rx):
// each stretch of signal — first to last sample above the noise floor, closed by 50 ms of silence —
// with the holes inside it. A's modem tone has a constant envelope, so 8 samples in a row near zero
// inside a stretch is a hole. A's audio reaches B through a MediaStream between two AudioContexts;
// a browser short of time on its audio threads drops a block there although the page kept time.
const HEAR = `(() => {
  const ep = globalThis.__epB, rx = globalThis.__hear = { i: 0, cur: null, bursts: [], end: Math.round(ep.sampleRate * 0.05) };
  const f = ep._rx.bind(ep);
  ep._rx = (x) => {
    for (let j = 0; j < x.length; j++, rx.i++) {
      if (Math.abs(x[j]) > 0.02) {
        if (!rx.cur) rx.cur = { first: rx.i, last: rx.i, holes: 0, longest: 0 };
        else { const run = rx.i - rx.cur.last - 1; if (run >= 8) { rx.cur.holes++; rx.cur.longest = Math.max(rx.cur.longest, run); } rx.cur.last = rx.i; }
      } else if (rx.cur && rx.i - rx.cur.last > rx.end) { rx.bursts.push(rx.cur); rx.cur = null; }
    }
    return f(x);
  };
  return true;
})()`;
const HEAR_RESET = '(globalThis.__hear.bursts = [], globalThis.__hear.cur = null, true)';
/**
 * The frame under test at both ends: the burst A keyed at or after `sentAt` (finished or still
 * keyed) and what B's microphone was handed since HEAR_RESET. `whole`: one stretch without holes,
 * as long as A's burst less the modem's ramp (25 to 28 samples at 48 kHz in every decoded run
 * measured; up to 96 allowed, and one dropped render block is 128).
 */
async function sentAndHeard(A, sentAt) {
  const bursts = (await A.evaluate('__epA.bursts.concat(__epA._burst ? [__epA._burst] : [])').catch(() => null)) || [];
  const sent = bursts.filter((b) => b.on_ms >= sentAt - 50)[0] || null;
  const heard = await A.evaluate('(() => { const r = globalThis.__hear; return r.bursts.concat(r.cur ? [r.cur] : []).map((b) => ({ samples: b.last - b.first + 1, holes: b.holes, longest_hole: b.longest })); })()');
  const short = sent && heard.length === 1 ? sent.samples - heard[0].samples : NaN;
  return { bursts, sent, heard, whole: heard.length === 1 && heard[0].holes === 0 && short >= 0 && short <= 96 };
}

const CROSS_WIRE = (cardA, realMic) => `(async () => {
  const A = globalThis.__emberWasmBackend;
  const md = navigator.mediaDevices;
  if (!${realMic}) {
    // silent "microphone" for A (its RX side is not under test here)
    const sctx = new AudioContext();
    const silent = sctx.createMediaStreamDestination();
    md.getUserMedia = async () => silent.stream;
  }
  // Fake Web Serial port for RTS PTT: records every setSignals() with a timestamp.
  const signals = [];
  const port = { getInfo: () => ({ usbVendorId: 0x1a86, usbProductId: 0x7523 }), async open() {}, async close() {},
    async setSignals(s) { signals.push({ ...s, t: performance.now() }); } };
  Object.defineProperty(navigator, 'serial', { configurable: true, value: { getPorts: async () => [port], requestPort: async () => port, addEventListener() {}, removeEventListener() {} } });
  globalThis.__signals = signals;

  const la = (await A.call('host.link.create', { name: 'Radio A', endpoint: { kind: 'audio', config: { ptt: 'serial_rts', tx_level: 80 } }, link: { type: 'audio', config: ${JSON.stringify(cfg('TEST1'))} } })).link;
  const epA = A.router.links.get(la.id).endpoint;
  if (epA.ac.state !== 'running') await epA.ac.resume();
  const dest = epA.ac.createMediaStreamDestination();
  epA.node.connect(dest);

  // Node B in the same page; its "microphone" is A's audio output.
  const { default: WasmBackend } = await import(location.protocol === 'file:' ? 'https://embermesh.invalid/js/backends/wasm.js' : new URL('js/backends/wasm.js', location.href).href);
  const B = new WasmBackend({ storage: 'memory', hub: false, guides: false, restoreLinks: false });
  await B.init();
  const orig = md.getUserMedia;
  md.getUserMedia = async () => dest.stream;
  let lb;
  try {
    lb = (await B.call('host.link.create', { name: 'Radio B', endpoint: { kind: 'audio', config: { ptt: 'vox', rx_gain: 1 } }, link: { type: 'audio', config: ${JSON.stringify(cfg('TEST2'))} } })).link;
  } finally { md.getUserMedia = orig; }
  const epB = B.router.links.get(lb.id).endpoint;
  if (epB.ac.state !== 'running') await epB.ac.resume();
  await B.call('contacts.add', { card: ${JSON.stringify(cardA)} });
  const bEvents = [];
  B.on('*', (e, d) => bEvents.push([e, d]));
  globalThis.__B = B; globalThis.__bEvents = bEvents; globalThis.__epA = epA; globalThis.__epB = epB;
  return { la: la.id, lb: lb.id, rateA: epA.sampleRate, rateB: epB.sampleRate, stateA: epA.ac.state, stateB: epB.ac.state, worklet: !!(epA.node && epA.node.port) };
})()`;

test('audio: real modem through the audio endpoint, cross-wired via Web Audio — decode, gapless playout, PTT', { skip, timeout: 120000 }, async (t) => {
  const browser = await startBrowser(MEDIA_FLAGS);
  if (browser.error) { t.skip(`Chrome could not start: ${browser.error}`); return; }
  cleanups.push(() => browser.close());
  const srv = await startServer(DIST_DIR);
  cleanups.push(() => srv.close());
  const A = await nodePage(browser, `${srv.url}index.html?nohub=1&profile=audio`);
  cleanups.push(() => A.close());
  if ((await A.call('node.info')).name === '') await A.call('node.set_profile', { name: 'Radio A' });
  const a = await A.call('node.info');
  const mic = MIC;
  t.diagnostic(`real getUserMedia(audio): ${mic}`);
  const setup = await A.evaluate(CROSS_WIRE(a.card, mic === 'real'));
  t.diagnostic(`audio setup: ${JSON.stringify(setup)}`);
  assert.equal(setup.worklet, true, 'AudioWorklet path in use');
  assert.equal(setup.stateA, 'running');
  await A.waitCall('links.list', {}, (r) => (r.links.find((l) => l.id === setup.la) || {}).state === 'up', 10000, 'audio link A up');

  // Live audio through the devices (decode, gapless playout, PTT timing) needs a host that runs audio
  // in real time; probed just before. Below the probe's limits the transmission still runs once and
  // its numbers are reported, then the test is SKIPPED (the sample-time twin is audio_playout.test.mjs).
  await A.evaluate(HEAR);
  const bKeys = await quiet(A);
  const rt = await realtimeSkip(t, A, ['audio', 'timers'], 'audio cross-wire');
  await A.evaluate(HEAR_RESET);
  const text = `over the air ${Date.now()}`;
  const sentAt = Date.now();
  await A.call('messages.send', { conv: 'ch:public', text });
  if (rt) {
    const dec = await A.waitFor(`globalThis.__B.call('messages.list', { conv: 'ch:public' }).then(r => r.messages.some(m => m.text === ${JSON.stringify(text)}))`, 30000).then(() => 'yes', () => 'no');
    await A.waitFor('!__epA.ptt && !__epA._burst && !__epA._pttCancel', 20000).catch(() => {});
    const st = await A.evaluate('globalThis.__epA.stats()');
    t.diagnostic(`not real time here: decoded ${dec}, device playout ${JSON.stringify(st)}`);
    await A.evaluate('globalThis.__B.destroy().then(() => true)');
    t.skip(rt);
    return;
  }
  const got = await A.waitFor(`globalThis.__B.call('messages.list', { conv: 'ch:public' }).then(r => r.messages.find(m => m.text === ${JSON.stringify(text)}) || null)`, 40000).catch(() => null);
  const air = await sentAndHeard(A, sentAt);
  t.diagnostic(`A played ${air.sent ? air.sent.samples : 'no'} samples for the message; B's microphone was handed ${JSON.stringify(air.heard)}`);
  if (!got && air.sent && !air.whole) {
    // The probe passed a moment ago, yet the browser did not carry this burst to B whole: measured
    // on the frame itself (see HEAR). Not decoded with the whole burst received fails below.
    await A.evaluate('globalThis.__B.destroy().then(() => true)');
    t.skip(`realtime: the browser did not carry A's audio to B whole: A played ${air.sent.samples} samples, B's microphone was handed ${JSON.stringify(air.heard)}`);
    return;
  }
  assert.ok(got, `B decoded A's frame from audio (A's bursts ${JSON.stringify(air.bursts)}; B's microphone was handed ${JSON.stringify(air.heard)})`);
  assert.equal(got.from, a.node_id, 'B decoded A\'s frame from audio');

  // let the transmission finish and the PTT hang time pass
  await A.waitEvent('link.ptt', (d) => d.link_id === setup.la && d.on === false, 20000);
  await new Promise((r) => setTimeout(r, 1200));
  const ptt = (await A.events('link.ptt')).filter((d) => d.link_id === setup.la);
  assert.ok(ptt.length >= 2 && ptt[0].on === true, `link.ptt events: ${JSON.stringify(ptt)}`);

  const r = await A.evaluate(`(async () => {
    const epA = globalThis.__epA;
    const st = await epA.stats();
    const sig = globalThis.__signals;
    const links = await globalThis.__B.call('links.list');
    return { st, txSamples: epA.txSamples, rate: epA.sampleRate, sig, pttEvents: epA.pttEvents, bursts: epA.bursts, bLink: links.links[0] };
  })()`);
  t.diagnostic(`B keyed up ${await A.evaluate(bKeys)}x during the send`);
  t.diagnostic(`playout: ${JSON.stringify(r.st)}, tx samples ${r.txSamples} @ ${r.rate} Hz, B link stats ${JSON.stringify(r.bLink.stats)}`);
  assert.ok(r.txSamples > r.rate * 0.2, 'core produced audio');
  assert.ok(r.st.played >= r.txSamples, 'every transmitted sample was played');
  assert.ok(r.bLink.stats.rx_frames >= 1, 'B link counted the decoded frame');

  // serial RTS PTT, per transmission (the core may key up more than once: message, then ack/announce):
  // every key-up raised RTS, and each release came only after that burst's last sample had played.
  // Back-to-back transmissions keep the transmitter keyed (a pending release is cancelled and RTS is
  // re-asserted), so count level changes, not setSignals calls.
  const rts = r.sig.filter((s) => 'requestToSend' in s).filter((s, i, a) => i === 0 || a[i - 1].requestToSend !== s.requestToSend);
  const ons = rts.filter((s) => s.requestToSend === true);
  t.diagnostic(`bursts: ${JSON.stringify(r.bursts.map((b) => ({ samples: b.samples, keyed_ms: b.off_ms - b.on_ms, released_after_last_sample_ms: Math.round((b.off_audio - b.play_end) * 1000) })))}`);
  assert.ok(r.bursts.length >= 1, 'at least one transmission recorded');
  assert.equal(ons.length, r.bursts.length, `one RTS key-up per transmission: ${JSON.stringify(rts)}`);
  assert.equal(r.bursts.reduce((n, b) => n + b.samples, 0), r.txSamples, 'every transmitted sample belongs to a keyed burst');
  for (const b of r.bursts) assert.ok(b.off_audio >= b.play_end - 0.003, `PTT released ${((b.play_end - b.off_audio) * 1000).toFixed(0)} ms before the last sample played`);
  assert.equal(rts.at(-1).requestToSend, false, 'RTS low at the end');
  // measured at the device in wall-clock time (this host passed the real-time probe)
  t.diagnostic(`device underruns while transmitting: ${r.st.underruns} (${r.st.underrun_samples} samples)`);
  assert.equal(r.st.underruns, 0, `no gaps while transmitting (${JSON.stringify(r.st)})`);
  for (const b of r.bursts) {
    const audioMs = (b.samples / r.rate) * 1000;
    assert.ok(b.off_ms - b.on_ms >= audioMs - 30, `PTT held ${b.off_ms - b.on_ms} ms for ${audioMs.toFixed(0)} ms of audio`);
  }
  await A.evaluate('globalThis.__B.destroy().then(() => true)');
  assert.deepEqual(realErrors(A.errors), []);
});

// ---- 2. WAV from the core's modulator as Chrome's fake microphone ----------------------------

function wav16(samples, rate) {
  const b = Buffer.alloc(44 + samples.length * 2);
  b.write('RIFF', 0); b.writeUInt32LE(36 + samples.length * 2, 4); b.write('WAVE', 8);
  b.write('fmt ', 12); b.writeUInt32LE(16, 16); b.writeUInt16LE(1, 20); b.writeUInt16LE(1, 22);
  b.writeUInt32LE(rate, 24); b.writeUInt32LE(rate * 2, 28); b.writeUInt16LE(2, 32); b.writeUInt16LE(16, 34);
  b.write('data', 36); b.writeUInt32LE(samples.length * 2, 40);
  for (let i = 0; i < samples.length; i++) b.writeInt16LE(samples[i], 44 + i * 2);
  return b;
}

/** Runs a node in Node.js, sends a Public message on an audio link and records its modem output. */
async function modulateMessage(text, rate = 48000) {
  const { default: WasmBackend } = await import(ui('backends/wasm.js'));
  const g = new WasmBackend({ moduleUrl: wasmModuleUrl(), storage: 'memory', hub: false, guides: false, restoreLinks: false });
  await g.init();
  await g.call('node.set_profile', { name: 'WAV sender' });
  const { link } = await g.call('links.add', { type: 'audio', config: cfg('TEST3'), persist: false });
  await g.call('links.endpoint_state', { id: link.id, connected: true });
  await g.call('messages.send', { conv: 'ch:public', text });
  const out = [];
  let started = false, idle = 0;
  const t0 = Date.now();
  while (Date.now() - t0 < 15000) {
    g._tick();
    const pcm = g.audioTx(link.id, 960, rate);
    if (pcm.length) { started = true; idle = 0; out.push(...pcm); } else if (started && ++idle > 20) break;
    if (!pcm.length) await new Promise((r) => setTimeout(r, 10));
  }
  const card = (await g.call('node.info')).card;
  const node_id = (await g.call('node.info')).node_id;
  await g.destroy();
  if (!out.length) throw new Error('core produced no audio');
  const pad = new Array(rate / 2).fill(0);
  return { samples: [...pad, ...out, ...pad, ...pad], card, node_id, rate, seconds: out.length / rate };
}

test('audio: a WAV from the core modulator as the microphone decodes in the page (fake capture device, or a Web Audio substitute)', { skip, timeout: 120000 }, async (t) => {
  const text = `wav frame ${Date.now()}`;
  const m = await modulateMessage(text);
  const dir = tmpDir('ember-wav-');
  cleanups.push(() => fs.rmSync(dir, { recursive: true, force: true }));
  const file = path.join(dir, 'modem.wav');
  fs.writeFileSync(file, wav16(m.samples, m.rate));
  t.diagnostic(`WAV: ${m.seconds.toFixed(2)} s of AFSK1200 from the core`);
  // Chrome's sandboxed audio service (macOS) cannot read a WAV from our temp dir and then captures
  // silence; with the audio service sandbox off the fake device plays the file.
  const browser = await startBrowser([...MEDIA_FLAGS, '--disable-features=AudioServiceSandbox', `--use-file-for-fake-audio-capture=${file}`]);
  if (browser.error) { t.skip(`Chrome could not start: ${browser.error}`); return; }
  cleanups.push(() => browser.close());
  const srv = await startServer(DIST_DIR);
  cleanups.push(() => srv.close());
  const P = await nodePage(browser, `${srv.url}index.html?nohub=1&profile=wav`);
  cleanups.push(() => P.close());
  await P.call('contacts.add', { card: m.card });
  const mic = MIC;
  t.diagnostic(`microphone path: ${mic === 'real' ? 'real getUserMedia with --use-file-for-fake-audio-capture' : `substitute (${mic}): WAV decoded by decodeAudioData and looped into the endpoint as its getUserMedia stream`}`);
  if (mic !== 'real') {
    const b64 = fs.readFileSync(file).toString('base64');
    await P.evaluate(`(async () => {
      const ctx = new AudioContext();
      const bin = Uint8Array.from(atob(${JSON.stringify(b64)}), (c) => c.charCodeAt(0));
      const buf = await ctx.decodeAudioData(bin.buffer);
      const src = ctx.createBufferSource(); src.buffer = buf; src.loop = true;
      const dest = ctx.createMediaStreamDestination(); src.connect(dest); src.start();
      navigator.mediaDevices.getUserMedia = async () => dest.stream;
      return buf.duration;
    })()`);
  }
  // the fake capture device (or the looped Web Audio substitute) must play the WAV in real time
  const why = await realtimeSkip(t, P, mic === 'real' ? ['audio', 'mic'] : ['audio'], 'WAV microphone');
  if (why) { t.skip(why); return; }
  const { link } = await P.call('host.link.create', { name: 'Mic', endpoint: { kind: 'audio', config: { ptt: 'vox' } }, link: { type: 'audio', config: cfg('TEST4') } });
  await P.waitCall('links.list', {}, (r) => (r.links.find((l) => l.id === link.id) || {}).state === 'up', 10000, 'audio link up');
  // Fail with a clear reason if the "microphone" is silent instead of timing out on the decode.
  const lvl = await P.waitCall('links.list', {}, (r) => ((r.links.find((l) => l.id === link.id) || {}).status || {}).rx_level_dbfs > -50, 10000, 'audio level from the microphone (fake capture device delivering the WAV)');
  t.diagnostic(`rx level ${lvl.links.find((l) => l.id === link.id).status.rx_level_dbfs} dBFS`);
  // the fake device loops the file: the frame comes around every few seconds
  const got = await P.waitCall('messages.list', { conv: 'ch:public' }, (r) => r.messages.find((x) => x.text === text), 40000, 'message decoded from the fake microphone');
  assert.equal(got.messages.find((x) => x.text === text).from, m.node_id);
  assert.deepEqual(realErrors(P.errors), []);
});

test('audio in the single file (file://): ScriptProcessor path, real modem decodes, PTT held through playout', { skip, timeout: 120000 }, async (t) => {
  const browser = await startBrowser(MEDIA_FLAGS);
  if (browser.error) { t.skip(`Chrome could not start: ${browser.error}`); return; }
  cleanups.push(() => browser.close());
  const A = await nodePage(browser, `${pathToFileURL(path.join(DIST_DIR, 'embermesh.html')).href}?profile=audiosingle`, { timeoutMs: 60000 });
  cleanups.push(() => A.close());
  if ((await A.call('node.info')).name === '') await A.call('node.set_profile', { name: 'Radio A' });
  const a = await A.call('node.info');
  const setup = await A.evaluate(CROSS_WIRE(a.card, MIC === 'real'), 60000);
  assert.equal(await A.evaluate('__epA.mode'), 'scriptprocessor');
  // The ScriptProcessor runs on the page's main thread: a callback later than one buffer (its
  // budget, spSize / sample rate) loses audio. Measured on the path itself, A's and B's callbacks,
  // while the frame is sent: a frame lost only on a host that blew that budget is SKIPPED with the
  // numbers; lost while every callback kept time, it fails. (Twin: audio_playout.test.mjs.)
  const budget = await A.evaluate(`(() => {
    const buf = ((__epA.spSize || __epA.node.bufferSize) / __epA.sampleRate) * 1000;
    const m = globalThis.__spMon = { buf, a: 0, b: 0, n: 0 };
    for (const [k, ep] of [['a', __epA], ['b', __epB]]) {
      const node = ep.node, f = node.onaudioprocess; let last = 0;
      node.onaudioprocess = (e) => { const t = performance.now(); if (last) m[k] = Math.max(m[k], t - last - buf); last = m['last_' + k] = t; m.n++; return f.call(node, e); };
    }
    return Math.round(buf);
  })()`);
  await A.evaluate(HEAR);
  const bKeys = await quiet(A);
  // what each node's audio link has done so far, to tell the frame under test from earlier traffic
  const LINKS = `Promise.all([globalThis.__emberWasmBackend, globalThis.__B].map((b) => b.call('links.list').then((r) => (r.links || []).filter((l) => l.type === 'audio').map((l) => { const st = l.status || l; return { transmissions: st.transmissions, rx_frames: st.rx_frames, tx_frames: st.tx_frames, rx_errors: st.rx_errors }; })))).then((x) => JSON.stringify({ A: x[0], B: x[1] }))`;
  const linkBefore = await A.evaluate(LINKS).catch((e) => e.message);
  await A.evaluate(HEAR_RESET);
  const ev0 = await A.evaluate('__bEvents.length');
  const text = `single file over the air ${Date.now()}`;
  const sentAt = Date.now();
  await A.call('messages.send', { conv: 'ch:public', text });
  const decoded = await A.waitFor(`globalThis.__B.call('messages.list', { conv: 'ch:public' }).then(r => r.messages.some(m => m.text === ${JSON.stringify(text)}))`, 30000).then(() => true, () => false);
  // Lateness includes a callback that is overdue right now: callbacks that stopped coming never
  // report their own delay.
  const mon = await A.evaluate('(() => { const m = globalThis.__spMon, now = performance.now(); for (const k of ["a", "b"]) if (m["last_" + k]) m[k] = Math.max(m[k], now - m["last_" + k] - m.buf); return { a: m.a, b: m.b, n: m.n }; })()');
  const late = Math.round(Math.max(mon.a, mon.b));
  const bKeyed = await A.evaluate(bKeys);
  const { bursts: aBursts, sent, heard, whole } = await sentAndHeard(A, sentAt);
  const bPtt = await A.evaluate('__epB.pttEvents.slice(-16).map((e) => ({ on: e.on, t: Math.round(e.t || 0) }))').catch(() => []);
  // half duplex: B is deaf while it transmits, so B keyed while A's frame was on the air loses it
  const collided = !!sent && bPtt.some((e, i) => e.on && e.t < (sent.off_ms || Infinity) && ((bPtt[i + 1] && !bPtt[i + 1].on ? bPtt[i + 1].t : Infinity) > sent.on_ms));
  const linkSeen = `before ${linkBefore} after ${await A.evaluate(LINKS).catch((e) => e.message)}`;
  t.diagnostic(`ScriptProcessor budget ${budget} ms per callback; latest callback ${late} ms behind schedule (A ${Math.round(mon.a)}, B ${Math.round(mon.b)}, ${mon.n} callbacks); decoded: ${decoded}`);
  t.diagnostic(`A played ${sent ? sent.samples : 'no'} samples for the message; B's microphone was handed ${JSON.stringify(heard)}; B keyed up ${bKeyed}x since the send${collided ? ', once while A was transmitting' : ', never while A was transmitting'}`);
  t.diagnostic(`audio links (frames sent and received) ${linkSeen}`);
  let report = `links ${linkSeen}; A's bursts ${JSON.stringify(aBursts)}; B's PTT ${JSON.stringify(bPtt)}`;
  if (!decoded) {
    const bSaw = await A.evaluate(`JSON.stringify(__bEvents.slice(${ev0}).filter((e) => !/^(link\\.(status|updated|ptt)|storage\\.)/.test(e[0])).slice(0, 30).map((e) => e[0] + ' ' + JSON.stringify(e[1] || null).slice(0, 160)))`).catch((e) => e.message);
    const bMesh = await A.evaluate(`globalThis.__B.call('mesh.stats').then((r) => JSON.stringify({ rx_frames: r.rx_frames, rx_bad: r.rx_bad, delivered: r.delivered, dropped_auth: r.dropped_auth, dropped_rate: r.dropped_rate, dropped_duplicates: r.dropped_duplicates, dropped_expired: r.dropped_expired }))`).catch((e) => e.message);
    report += `; B's events ${bSaw}; B's mesh counters ${bMesh}`;
  }
  if (!decoded && late > budget) {
    await A.evaluate('globalThis.__B.destroy().then(() => true)');
    t.skip(`realtime: the main thread ran a ScriptProcessor callback ${late} ms late, more than its ${budget} ms buffer, so audio was lost`);
    return;
  }
  if (!decoded && sent && !collided && !whole) {
    await A.evaluate('globalThis.__B.destroy().then(() => true)');
    t.skip(`realtime: the browser did not carry A's audio to B whole: A played ${sent.samples} samples, B's microphone was handed ${JSON.stringify(heard)} (every callback within its ${budget} ms budget: latest ${late} ms behind)`);
    return;
  }
  assert.ok(sent, `A keyed up for the message (${report})`);
  assert.equal(collided, false, `B stayed quiet while A's frame was on the air (${report})`);
  assert.ok(decoded, `B decoded the frame: its microphone was handed A's whole burst and every callback kept its ${budget} ms budget, latest ${late} ms behind (${report})`);
  await A.waitFor('!__epA.ptt && !__epA._burst && !__epA._pttCancel', 20000);
  const bursts = await A.evaluate('__epA.bursts');
  assert.ok(bursts.length >= 1);
  for (const b of bursts) assert.ok(b.off_audio >= b.play_end - 0.003, `PTT released after the last sample played (${JSON.stringify(b)})`);
  t.diagnostic(`single file: decoded via ScriptProcessor; bursts ${JSON.stringify(bursts.map((b) => ({ samples: b.samples, keyed_ms: b.off_ms - b.on_ms })))}`);
  await A.evaluate('globalThis.__B.destroy().then(() => true)');
  assert.deepEqual(realErrors(A.errors), []);
  void setup;
});
