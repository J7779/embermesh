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
  const rt = await realtimeSkip(t, A, ['audio', 'timers'], 'audio cross-wire');
  const text = `over the air ${Date.now()}`;
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
  const got = await A.waitFor(`globalThis.__B.call('messages.list', { conv: 'ch:public' }).then(r => r.messages.find(m => m.text === ${JSON.stringify(text)}) || null)`, 40000);
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
  // ScriptProcessor runs on the page's main thread: a late block loses audio, so this needs a host
  // that runs audio in real time (probed just before; the sample-time twin is audio_playout.test.mjs).
  const why = await realtimeSkip(t, A, ['audio'], 'single-file audio');
  if (why) { await A.evaluate('globalThis.__B.destroy().then(() => true)'); t.skip(why); return; }
  const text = `single file over the air ${Date.now()}`;
  await A.call('messages.send', { conv: 'ch:public', text });
  await A.waitFor(`globalThis.__B.call('messages.list', { conv: 'ch:public' }).then(r => r.messages.some(m => m.text === ${JSON.stringify(text)}))`, 30000);
  await A.waitFor('!__epA.ptt && !__epA._burst && !__epA._pttCancel', 20000);
  const bursts = await A.evaluate('__epA.bursts');
  assert.ok(bursts.length >= 1);
  for (const b of bursts) assert.ok(b.off_audio >= b.play_end - 0.003, 'PTT released after the last sample played');
  t.diagnostic(`single file: decoded via ScriptProcessor; bursts ${JSON.stringify(bursts.map((b) => ({ samples: b.samples, keyed_ms: b.off_ms - b.on_ms })))}`);
  await A.evaluate('globalThis.__B.destroy().then(() => true)');
  assert.deepEqual(realErrors(A.errors), []);
  void setup;
});
