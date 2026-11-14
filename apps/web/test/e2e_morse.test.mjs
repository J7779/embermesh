// Morse receive and handheld-radio presets (RPC_API §6) end to end in real headless Chrome with the
// real WASM core:
//   (a)  camera Morse: Chrome's fake camera plays a generated .y4m of a blinking light; the page
//        samples brightness of each video frame and feeds morse.rx -> transcript
//   (a2) the sender is another page: its host.signal.flash (the Send tab's screen flash) is
//        mirrored onto a canvas, streamed over WebRTC video to the receiving page, sampled there
//   (b)  sound Morse: a keyed-tone WAV as Chrome's fake microphone -> PCM resampled to 8 kHz ->
//        morse.rx (pcm_b64) -> transcript
//   (c)  "Connect a radio": two audio links with the handheld_vox preset, cross-wired through
//        Web Audio with a VOX model in between (clips the first 300 ms of every transmission, holds
//        500 ms), Public messages delivered both ways
//   (d)  handheld_ptt preset: PTT on a serial RTS line (fake Web Serial port), released only after
//        the audio has played out
// Until the core answers morse.rx.* / has the presets, those tests are reported as TODO (pending),
// never as passed. The capture code in the page is test code written against the contract; the UI's
// own capture library is exercised by the UI's smoke test.
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { DIST_DIR, ui } from './helpers.mjs';
import { e2eSkip, startBrowser, nodePage, realErrors, tmpDir, runCleanups, HOOK } from './lib/e2e.mjs';
import { startServer } from './lib/static_server.mjs';

const skip = e2eSkip();
const { toMorse, morsePattern } = await import(ui('lib/morse.js'));
const cleanups = [];
after(async () => { await runCleanups(cleanups); }, HOOK);

const LIGHT_TEXT = 'TEST';
const LIGHT_WPM = 8; // 150 ms dots = 4.5 camera frames at 30 fps
const AUDIO_TEXT = 'CQ DE EMBER';
const AUDIO_WPM = 15;
const TONE_HZ = 700;

/** [{on, from, to}] intervals in ms for a text, with `lead` and `tail` ms of darkness/silence. */
function keying(text, wpm, lead, tail) {
  const p = morsePattern(toMorse(text), wpm);
  const iv = [];
  let t = lead;
  p.forEach((d, i) => { if (i % 2 === 0) iv.push([t, t + d]); t += d; });
  return { on: iv, total: t + tail };
}
const isOn = (iv, t) => iv.some(([a, b]) => t >= a && t < b);

function writeY4m(file, { w = 64, h = 48, fps = 30, text, wpm }) {
  const k = keying(text, wpm, 1500, 2500);
  const frames = Math.ceil((k.total / 1000) * fps);
  const head = Buffer.from(`YUV4MPEG2 W${w} H${h} F${fps}:1 Ip A1:1 C420jpeg\n`);
  const fh = Buffer.from('FRAME\n');
  const uv = Buffer.alloc((w * h) / 2, 128);
  const fd = fs.openSync(file, 'w');
  fs.writeSync(fd, head);
  for (let i = 0; i < frames; i++) {
    const on = isOn(k.on, (i * 1000) / fps);
    fs.writeSync(fd, fh);
    fs.writeSync(fd, Buffer.alloc(w * h, on ? 235 : 20));
    fs.writeSync(fd, uv);
  }
  fs.closeSync(fd);
  return k.total;
}

function writeToneWav(file, { rate = 48000, text, wpm, hz }) {
  const k = keying(text, wpm, 1000, 2000);
  const n = Math.ceil((k.total / 1000) * rate);
  const b = Buffer.alloc(44 + n * 2);
  b.write('RIFF', 0); b.writeUInt32LE(36 + n * 2, 4); b.write('WAVE', 8); b.write('fmt ', 12);
  b.writeUInt32LE(16, 16); b.writeUInt16LE(1, 20); b.writeUInt16LE(1, 22); b.writeUInt32LE(rate, 24);
  b.writeUInt32LE(rate * 2, 28); b.writeUInt16LE(2, 32); b.writeUInt16LE(16, 34); b.write('data', 36); b.writeUInt32LE(n * 2, 40);
  const ramp = rate * 0.005; // 5 ms raised-cosine edges, like a real keyer
  for (let i = 0; i < n; i++) {
    const t = (i * 1000) / rate;
    let env = 0;
    for (const [a, c] of k.on) {
      if (t >= a && t < c) {
        const sa = (t - a) * rate / 1000, sc = (c - t) * rate / 1000;
        env = Math.min(1, sa / ramp, sc / ramp);
        env = 0.5 - 0.5 * Math.cos(Math.PI * env);
        break;
      }
    }
    const noise = (Math.random() - 0.5) * 0.02;
    b.writeInt16LE(Math.round(32767 * (0.5 * env * Math.sin((2 * Math.PI * hz * i) / rate) + noise)), 44 + i * 2);
  }
  fs.writeFileSync(file, b);
  return k.total;
}

// ---- in-page capture (test code, written against RPC_API §6) --------------------------------

const PAGE_HELPERS = `(() => {
  if (globalThis.__morseCap) return true;
  const B = () => globalThis.__emberWasmBackend;
  globalThis.__morseCap = {
    /**
     * A recorded light (Chrome's fake camera playing a .y4m file): every frame through
     * MediaStreamTrackProcessor, timed on the file's own timeline (frame n = n / fps). The fake
     * device advances one file frame per frame it manages to produce, so on a starved machine the
     * file plays slower than real time and wall-clock frame timestamps no longer match the light;
     * the frame index always does. (A real lamp needs no such thing: see light() below.)
     */
    fileLight(stream, session, fps) {
      if (typeof MediaStreamTrackProcessor !== 'function') return this.light(stream, session);
      const reader = new MediaStreamTrackProcessor({ track: stream.getVideoTracks()[0] }).readable.getReader();
      const c = new OffscreenCanvas(16, 12);
      const g = c.getContext('2d', { willReadFrequently: true });
      let times = [], levels = [], frames = 0, stop = false;
      (async () => {
        while (!stop) {
          const { value: f, done } = await reader.read();
          if (done || stop) { if (f) f.close(); break; }
          g.drawImage(f, 0, 0, 16, 12); f.close();
          const d = g.getImageData(4, 3, 8, 6).data;
          let sum = 0; for (let i = 0; i < d.length; i += 4) sum += 0.299 * d[i] + 0.587 * d[i + 1] + 0.114 * d[i + 2];
          times.push(Math.round((frames * 1000) / fps)); levels.push(sum / (d.length / 4) / 255); frames++;
        }
      })();
      const pump = setInterval(() => {
        if (!levels.length) return;
        const p = { session, times_ms: times, levels }; times = []; levels = [];
        B().call('morse.rx.feed', p).catch((e) => { globalThis.__morseErr = e.message; });
      }, 100);
      return { frames: () => frames, stop() { stop = true; clearInterval(pump); reader.cancel().catch(() => {}); stream.getTracks().forEach((t) => t.stop()); } };
    },
    /** Brightness of the centre of every video frame -> morse.rx.feed {times_ms, levels} every 100 ms. */
    light(stream, session, { clock = 'media' } = {}) {
      const v = document.createElement('video');
      v.muted = true; v.playsInline = true; v.srcObject = stream; v.play().catch(() => {});
      v.style.cssText = 'position:fixed;left:0;top:0;width:64px;height:48px';
      document.body.appendChild(v);
      const c = document.createElement('canvas'); c.width = 16; c.height = 12;
      const g = c.getContext('2d', { willReadFrequently: true });
      let times = [], levels = [], frames = 0, stop = false, last = -1;
      const sample = (t) => {
        if (v.readyState < 2) return;
        g.drawImage(v, 0, 0, 16, 12);
        const d = g.getImageData(4, 3, 8, 6).data;
        let s = 0; for (let i = 0; i < d.length; i += 4) s += 0.299 * d[i] + 0.587 * d[i + 1] + 0.114 * d[i + 2];
        if (t <= last) return; last = t;
        times.push(t); levels.push(s / (d.length / 4) / 255); frames++;
      };
      if (v.requestVideoFrameCallback) {
        // local camera: the frame's media time; a remote (WebRTC) stream: when the sender captured it
        const cb = (now, meta) => { if (stop) return; sample(Math.round(clock === 'media' && meta.mediaTime ? meta.mediaTime * 1000 : (meta.captureTime || meta.receiveTime || now))); v.requestVideoFrameCallback(cb); };
        v.requestVideoFrameCallback(cb);
      }
      const poll = setInterval(() => { if (!v.requestVideoFrameCallback) sample(Math.round(performance.now())); }, 33);
      const pump = setInterval(() => {
        if (!levels.length) return;
        const p = { session, times_ms: times, levels }; times = []; levels = [];
        B().call('morse.rx.feed', p).catch((e) => { globalThis.__morseErr = e.message; });
      }, 100);
      return { frames: () => frames, stop() { stop = true; clearInterval(poll); clearInterval(pump); v.remove(); stream.getTracks().forEach((t) => t.stop()); } };
    },
    /** Microphone PCM -> 8 kHz mono int16 -> morse.rx.feed {pcm_b64}. */
    audio(stream, session, rate = 8000) {
      const ac = new AudioContext();
      const src = ac.createMediaStreamSource(stream);
      const sp = ac.createScriptProcessor(4096, 1, 1);
      const ratio = ac.sampleRate / rate;
      let pos = 0, samples = 0;
      sp.onaudioprocess = (e) => {
        const x = e.inputBuffer.getChannelData(0);
        const out = [];
        for (; pos < x.length; pos += ratio) {
          const a = Math.floor(pos), b = Math.min(x.length, Math.floor(pos + ratio));
          let s = 0; for (let i = a; i < b; i++) s += x[i];
          out.push(Math.max(-1, Math.min(1, s / Math.max(1, b - a))));
        }
        pos -= x.length;
        const pcm = new Int16Array(out.length);
        for (let i = 0; i < out.length; i++) pcm[i] = Math.round(out[i] * 32767);
        samples += pcm.length;
        const u8 = new Uint8Array(pcm.buffer);
        let bin = ''; for (let i = 0; i < u8.length; i++) bin += String.fromCharCode(u8[i]);
        B().call('morse.rx.feed', { session, pcm_b64: btoa(bin) }).catch((e) => { globalThis.__morseErr = e.message; });
        e.outputBuffer.getChannelData(0).fill(0);
      };
      src.connect(sp); sp.connect(ac.destination);
      return { rate: ac.sampleRate, samples: () => samples, stop() { sp.disconnect(); stream.getTracks().forEach((t) => t.stop()); ac.close(); } };
    },
  };
  return true;
})()`;

async function morseSupported(P) {
  try {
    const r = await P.call('morse.rx.start', { source: 'light' });
    await P.call('morse.rx.stop', { session: r.session }).catch(() => {});
    return null;
  } catch (e) {
    if (e.code === 'not_implemented') return 'core does not implement morse.rx.* yet (RPC_API §6)';
    throw e;
  }
}

async function presetSupported(P, preset) {
  const t = await P.call('links.types');
  const a = t.types.find((x) => x.type === 'audio');
  const f = a && a.config.find((x) => x.key === 'preset');
  if (!f || !(f.options || []).some((o) => o.value === preset)) return `core audio link has no "${preset}" preset yet (RPC_API §6)`;
  return null;
}

async function waitTranscript(P, session, want, ms) {
  const r = await P.waitCall('morse.rx.status', { session }, (s) => s.transcript.replace(/\s+/g, ' ').toUpperCase().includes(want), ms, `transcript containing "${want}"`);
  return r;
}

// ---- (a) + (b): fake camera and fake microphone files ---------------------------------------

let media = null; // {browser, srv, dir}
before(async () => {
  if (skip) return;
  const dir = tmpDir('ember-morse-');
  cleanups.push(() => fs.rmSync(dir, { recursive: true, force: true }));
  const y4m = path.join(dir, 'blink.y4m');
  const wav = path.join(dir, 'keyed.wav');
  writeY4m(y4m, { text: LIGHT_TEXT, wpm: LIGHT_WPM });
  writeToneWav(wav, { text: AUDIO_TEXT, wpm: AUDIO_WPM, hz: TONE_HZ });
  const browser = await startBrowser(['--use-fake-ui-for-media-stream', '--use-fake-device-for-media-stream',
    `--use-file-for-fake-video-capture=${y4m}`, `--use-file-for-fake-audio-capture=${wav}`,
    '--disable-features=AudioServiceSandbox', '--autoplay-policy=no-user-gesture-required',
    '--host-resolver-rules=MAP hub.test 127.0.0.1',
    // several pages at once, all "in front" (a sender page must flash on time, like a phone held up)
    '--disable-background-timer-throttling', '--disable-renderer-backgrounding', '--disable-backgrounding-occluded-windows']); // hub.test = a plain-http hub address (not a secure context)
  if (browser.error) { media = { error: browser.error }; return; }
  cleanups.push(() => browser.close());
  const srv = await startServer(DIST_DIR);
  cleanups.push(() => srv.close());
  media = { browser, srv, dir };
}, HOOK);

const needMedia = (t) => {
  if (media && media.error) { t.skip(`Chrome could not start: ${media.error}`); return false; }
  return true;
};

test('(a) camera Morse: fake camera plays a blinking light (.y4m) -> brightness samples -> morse.rx transcript', { skip, timeout: 200000 }, async (t) => {
  if (!needMedia(t)) return;
  const P = await nodePage(media.browser, `${media.srv.url}index.html?nohub=1&profile=morsecam`);
  t.after(() => P.close());
  const host = await P.call('host.info');
  assert.ok(host.features.includes('camera'), `camera feature: ${JSON.stringify(host.unavailable_features)}`);
  const pending = await morseSupported(P);
  if (pending) { t.todo(pending); return; }
  const { session } = await P.call('morse.rx.start', { source: 'light' });
  await P.evaluate(PAGE_HELPERS);
  await P.evaluate(`navigator.mediaDevices.getUserMedia({ video: { width: 64, height: 48, frameRate: 30 } }).then((s) => { globalThis.__cap = __morseCap.fileLight(s, ${JSON.stringify(session)}, 30); return true; })`);
  // the file loops; a starved fake device plays it up to ~3x slower, so allow a few loops
  const st = await waitTranscript(P, session, LIGHT_TEXT, 120000);
  t.diagnostic(`camera: transcript "${st.transcript}", wpm ${st.wpm}, quality ${st.quality}, frames ${await P.evaluate('__cap.frames()')}`);
  const ev = await P.events('morse.text');
  assert.ok(ev.some((e) => e.session === session && e.source === 'light'), 'morse.text events');
  await P.evaluate('__cap.stop(), true');
  assert.ok((await P.call('morse.rx.stop', { session })).transcript.toUpperCase().includes(LIGHT_TEXT));
  assert.deepEqual(realErrors(P.errors), []);
});

test('(b) sound Morse: keyed 700 Hz tone as the fake microphone -> 8 kHz PCM -> morse.rx transcript', { skip, timeout: 150000 }, async (t) => {
  if (!needMedia(t)) return;
  const P = await nodePage(media.browser, `${media.srv.url}index.html?nohub=1&profile=morsemic`);
  t.after(() => P.close());
  const host = await P.call('host.info');
  assert.ok(host.features.includes('microphone'));
  const pending = await morseSupported(P);
  if (pending) { t.todo(pending); return; }
  const { session } = await P.call('morse.rx.start', { source: 'audio', sample_rate: 8000 });
  await P.evaluate(PAGE_HELPERS);
  await P.evaluate(`navigator.mediaDevices.getUserMedia({ audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false } }).then((s) => { globalThis.__cap = __morseCap.audio(s, ${JSON.stringify(session)}); return true; })`);
  const st = await waitTranscript(P, session, AUDIO_TEXT, 45000);
  t.diagnostic(`audio: transcript "${st.transcript}", wpm ${st.wpm}, tone ${st.tone_hz} Hz, snr ${st.snr_db} dB`);
  if (st.tone_hz) assert.ok(Math.abs(st.tone_hz - TONE_HZ) < 60, `tone estimate ${st.tone_hz}`);
  await P.evaluate('__cap.stop(), true');
  assert.deepEqual(realErrors(P.errors), []);
});

// ---- capture works under each build's CSP; refused (with the reason) on a plain-http hub page --

const CAPTURE_PROBE = `(async () => {
  const s = await navigator.mediaDevices.getUserMedia({ video: { width: 64, height: 48 }, audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false } });
  const v = document.createElement('video'); v.muted = true; v.srcObject = s; await v.play();
  await new Promise((r) => setTimeout(r, 500));
  const c = document.createElement('canvas'); c.width = 16; c.height = 12;
  const g = c.getContext('2d', { willReadFrequently: true }); g.drawImage(v, 0, 0, 16, 12);
  const px = g.getImageData(0, 0, 16, 12).data.length;      // canvas readback of a camera frame
  const ac = new AudioContext();
  const an = ac.createAnalyser(); ac.createMediaStreamSource(s).connect(an);
  // the fake microphone plays a keyed tone with gaps: look for the loudest moment over 3 s
  const buf = new Float32Array(an.fftSize);
  let pk = 0;
  for (let i = 0; i < 60 && pk < 0.1; i++) {
    await new Promise((r) => setTimeout(r, 50));
    an.getFloatTimeDomainData(buf);
    for (const x of buf) pk = Math.max(pk, Math.abs(x));
  }
  s.getTracks().forEach((t) => t.stop()); await ac.close();
  // the audio endpoint itself (AudioWorklet where allowed, ScriptProcessor fallback on file://)
  const B = globalThis.__emberWasmBackend;
  const l = (await B.call('host.link.create', { name: 'probe', endpoint: { kind: 'audio', config: { ptt: 'vox' } }, link: { type: 'audio', config: { licence: 'licence_free' } }, autostart: false })).link;
  const ep = B.router.links.get(l.id).endpoint;
  for (let i = 0; i < 50 && ep.state !== 'up'; i++) await new Promise((r) => setTimeout(r, 100));
  const res = { px, peak: pk, mode: ep.mode, state: ep.state };
  await B.call('host.link.remove', { id: l.id });
  return res;
})()`;

test('capture under each build: camera frame readback + microphone + audio endpoint on http://localhost and file:// (single file); refused on a plain-http hub', { skip, timeout: 90000 }, async (t) => {
  if (!needMedia(t)) return;
  const site = await nodePage(media.browser, `${media.srv.url}index.html?nohub=1&profile=capsite`);
  const single = await nodePage(media.browser, `${pathToFileURL(path.join(DIST_DIR, 'embermesh.html')).href}?profile=capsingle`, { timeoutMs: 60000 });
  const hub = await nodePage(media.browser, `http://hub.test:${media.srv.port}/index.html?nohub=1&profile=caphub`);
  t.after(() => Promise.all([site.close(), single.close(), hub.close()]));
  for (const [name, P, mode] of [['site', site, 'worklet'], ['single file', single, 'scriptprocessor']]) {
    const h = await P.call('host.info');
    assert.ok(h.features.includes('camera') && h.features.includes('microphone'), `${name}: ${JSON.stringify(h.unavailable_features)}`);
    const r = await P.evaluate(CAPTURE_PROBE);
    assert.equal(r.state, 'up', `${name}: audio endpoint up`);
    assert.equal(r.mode, mode, `${name}: audio path`);
    assert.equal(r.px, 16 * 12 * 4, `${name}: camera frame read back`);
    assert.ok(r.peak > 0.05, `${name}: microphone delivers audio (${r.peak})`);
    t.diagnostic(`${name}: camera frame readback + microphone ok (mic peak ${r.peak.toFixed(2)}), audio endpoint up via ${r.mode}`);
  }
  const h = await hub.call('host.info');
  assert.equal(await hub.evaluate('window.isSecureContext'), false);
  assert.ok(!h.features.includes('camera') && !h.features.includes('microphone'));
  assert.match(h.unavailable_features.camera, /secure page/);
  assert.match(h.unavailable_features.camera, /hub/);
  assert.match(h.unavailable_features.microphone, /single-file/);
  t.diagnostic(`plain-http hub page: "${h.unavailable_features.camera}"`);
  for (const P of [site, single, hub]) assert.deepEqual(realErrors(P.errors), []);
});

// ---- (a2): the sender is another page flashing its screen ------------------------------------

const SENDER_SETUP = `(async () => {
  // Mirror host.signal.flash's full-screen overlay onto a canvas and stream it (what a camera
  // pointed at this screen would see).
  const c = document.createElement('canvas'); c.width = 64; c.height = 48;
  const g = c.getContext('2d');
  let on = false;
  const paint = () => { g.fillStyle = on ? '#fff' : '#000'; g.fillRect(0, 0, 64, 48); g.fillStyle = on ? '#fefefe' : '#010101'; g.fillRect(Math.random() * 60 | 0, 0, 1, 1); };
  new MutationObserver(() => {
    const el = [...document.body.children].find((e) => e.style && e.style.position === 'fixed' && e.style.zIndex === '2147483647');
    on = !!el && /255|#fff|white/i.test(el.style.background);
  }).observe(document.body, { subtree: true, childList: true, attributes: true, attributeFilter: ['style'] });
  setInterval(paint, 20); paint();
  const stream = c.captureStream(30);
  const pc = new RTCPeerConnection();
  stream.getTracks().forEach((tr) => pc.addTrack(tr, stream));
  await pc.setLocalDescription(await pc.createOffer());
  await new Promise((r) => { if (pc.iceGatheringState === 'complete') r(); else pc.onicegatheringstatechange = () => pc.iceGatheringState === 'complete' && r(); setTimeout(r, 4000); });
  globalThis.__pc = pc;
  return pc.localDescription.sdp;
})()`;

/** Waits until timers in every page fire within 25 ms of schedule for 2 s; returns the drift seen. */
async function waitQuiet(pages, ms) {
  const t0 = Date.now();
  let drift = Infinity;
  while (Date.now() - t0 < ms) {
    const d = await Promise.all(pages.map((P) => P.evaluate(`new Promise((res) => { let max = 0, last = performance.now(); const iv = setInterval(() => { const n = performance.now(); max = Math.max(max, n - last - 20); last = n; }, 20); setTimeout(() => { clearInterval(iv); res(Math.round(max)); }, 2000); })`)));
    drift = Math.max(...d);
    if (drift < 25) return drift;
  }
  return drift; // still busy: try anyway (the send is repeated)
}

const RECEIVER_SETUP = (offer, session) => `(async () => {
  const pc = new RTCPeerConnection();
  const got = new Promise((r) => { pc.ontrack = (e) => r(e.streams[0] || new MediaStream([e.track])); });
  await pc.setRemoteDescription({ type: 'offer', sdp: ${JSON.stringify(offer)} });
  await pc.setLocalDescription(await pc.createAnswer());
  await new Promise((r) => { if (pc.iceGatheringState === 'complete') r(); else pc.onicegatheringstatechange = () => pc.iceGatheringState === 'complete' && r(); setTimeout(r, 4000); });
  globalThis.__pc = pc;
  got.then((s) => { globalThis.__cap = __morseCap.light(s, ${JSON.stringify(session)}, { clock: 'arrival' }); });
  return pc.localDescription.sdp;
})()`;

test('(a2) camera Morse from another page: its screen flash (host.signal.flash) -> canvas -> WebRTC video -> transcript', { skip, timeout: 240000 }, async (t) => {
  if (!needMedia(t)) return;
  // two windows side by side, like two phones: neither is a throttled background tab
  const S = await nodePage(media.browser, `${media.srv.url}index.html?nohub=1&profile=flashsend`, { newWindow: true });
  const R = await nodePage(media.browser, `${media.srv.url}index.html?nohub=1&profile=flashrecv`, { newWindow: true });
  t.after(() => Promise.all([S.close(), R.close()]));
  const pending = await morseSupported(R);
  if (pending) { t.todo(pending); return; }
  const { session } = await R.call('morse.rx.start', { source: 'light' });
  await R.evaluate(PAGE_HELPERS);
  const offer = await S.evaluate(SENDER_SETUP);
  const answer = await R.evaluate(RECEIVER_SETUP(offer, session));
  await S.evaluate(`__pc.setRemoteDescription({ type: 'answer', sdp: ${JSON.stringify(answer)} }).then(() => true)`);
  try { await R.waitFor('globalThis.__cap && __cap.frames() > 20', 20000); } catch (e) {
    const d = await R.evaluate(`({ conn: __pc.connectionState, ice: __pc.iceConnectionState, cap: !!globalThis.__cap, frames: globalThis.__cap ? __cap.frames() : -1, recv: __pc.getReceivers().map((r) => r.track && r.track.readyState) })`);
    const ds = await S.evaluate(`({ conn: __pc.connectionState, senders: __pc.getSenders().map((x) => x.track && x.track.readyState) })`);
    throw new Error(`no video frames from the sender page: receiver ${JSON.stringify(d)}, sender ${JSON.stringify(ds)}`);
  }
  // Reading a screen through a camera is real-time: when the machine is saturated (other test
  // files run in parallel) both pages drop frames. Wait until both pages' timers keep time.
  const quiet = await waitQuiet([S, R], 90000);
  t.diagnostic(`timer drift before sending: ${quiet} ms`);
  const text = 'SOS';
  const pattern = morsePattern(toMorse(text), 5); // 240 ms dots: a phone screen filmed by a camera
  // The Send tab's flash: host.signal.flash with the Morse timing. A sender repeats until the reader
  // has it (other tests share the CPU, so a timer can slip); at most 5 sends.
  let st = null;
  for (let i = 1; i <= 5 && !st; i++) {
    await R.call('morse.rx.reset', { session });
    await S.call('host.signal.flash', { pattern_ms: pattern });
    try { st = await waitTranscript(R, session, text, 8000); } catch { t.diagnostic(`flash ${i}: not read (${JSON.stringify(await R.call('morse.rx.status', { session }))})`); }
    if (st) t.diagnostic(`read on flash ${i}`);
  }
  assert.ok(st, 'screen flash read within 5 sends');
  t.diagnostic(`screen flash via WebRTC: transcript "${st.transcript}", wpm ${st.wpm}`);
  await R.evaluate('__cap.stop(), true');
  assert.deepEqual(realErrors(S.errors), []);
  assert.deepEqual(realErrors(R.errors), []);
});

// ---- (c) + (d): handheld presets over cross-wired Web Audio ----------------------------------

// Two nodes in one page (the page's own node A, and node B), each with a real audio endpoint.
// A helper AudioContext H carries A's output to B's "microphone" and back, each through a VOX
// model: the gate opens only after the input has been above threshold for ATTACK ms (the start of
// every transmission is clipped, like a handheld's VOX) and closes HOLD ms after it went quiet.
const RADIO_DIAG = `(async () => {
  const pick = (l) => l && { state: l.state, cfg: { preset: l.config.preset, licence: l.config.licence, callsign: l.config.callsign }, stats: l.stats, st: { lvl: l.status.rx_level_dbfs, dcd: l.status.dcd, ok: l.status.frames_ok, bad: l.status.frames_bad, err: l.status.last_error, refused: l.status.tx_refused, txa: l.status.tx_active } };
  const a = (await __emberWasmBackend.call('links.list')).links.find((l) => l.type === 'audio');
  const b = (await __B.call('links.list')).links.find((l) => l.type === 'audio');
  return JSON.stringify({ a: pick(a), b: pick(b), vox: __vox.opened.length });
})()`;

/** Waits for a page condition; on timeout the error carries both radios' link status. */
async function waitRadio(P, expr, ms, what) {
  try { return await P.waitFor(expr, ms); } catch (e) {
    throw new Error(`${what}: ${e.message.split('\n')[0]}\nradios: ${await P.evaluate(RADIO_DIAG).catch((x) => x.message)}`);
  }
}

const RADIO_SETUP = (cfgA, cfgB, epA, epB, cardA) => `(async () => {
  const A = globalThis.__emberWasmBackend;
  const H = new AudioContext();
  const toA = H.createMediaStreamDestination(), toB = H.createMediaStreamDestination();
  const md = navigator.mediaDevices, orig = md.getUserMedia;
  const signals = [];
  const port = { getInfo: () => ({ usbVendorId: 0x1209, usbProductId: 0x7388 }), async open() {}, async close() {}, async setSignals(s) { signals.push({ ...s, t: performance.now() }); } };
  Object.defineProperty(navigator, 'serial', { configurable: true, value: { getPorts: async () => [port], requestPort: async () => port, addEventListener() {}, removeEventListener() {} } });
  globalThis.__signals = signals;
  md.getUserMedia = async () => toA.stream;
  const la = (await A.call('host.link.create', { name: 'Radio A', endpoint: { kind: 'audio', config: ${JSON.stringify(epA)} }, link: { type: 'audio', config: ${JSON.stringify(cfgA)} } })).link;
  const { default: WasmBackend } = await import(new URL('js/backends/wasm.js', location.href).href);
  const B = new WasmBackend({ storage: 'memory', hub: false, guides: false, restoreLinks: false });
  await B.init();
  md.getUserMedia = async () => toB.stream;
  let lb;
  try { lb = (await B.call('host.link.create', { name: 'Radio B', endpoint: { kind: 'audio', config: ${JSON.stringify(epB)} }, link: { type: 'audio', config: ${JSON.stringify(cfgB)} } })).link; }
  finally { md.getUserMedia = orig; }
  const epa = A.router.links.get(la.id).endpoint, epb = B.router.links.get(lb.id).endpoint;
  for (const e of [epa, epb, { ac: H }]) if (e.ac.state !== 'running') await e.ac.resume();
  const vox = { ATTACK: 300, HOLD: 500, opened: [] };
  const wire = (ep, dest, name) => {
    const out = ep.ac.createMediaStreamDestination(); ep.node.connect(out);
    const src = H.createMediaStreamSource(out.stream);
    const an = H.createAnalyser(); an.fftSize = 512;
    const gate = H.createGain(); gate.gain.value = 0;
    src.connect(an); src.connect(gate); gate.connect(dest);
    const buf = new Float32Array(an.fftSize);
    let above = 0, quiet = 0, open = false;
    setInterval(() => {
      an.getFloatTimeDomainData(buf);
      let pk = 0; for (const v of buf) pk = Math.max(pk, Math.abs(v));
      const now = performance.now();
      if (pk > 0.02) { quiet = 0; if (!above) above = now; if (!open && now - above >= vox.ATTACK) { open = true; gate.gain.setValueAtTime(1, H.currentTime); vox.opened.push({ name, t: now }); } }
      else { above = 0; if (!quiet) quiet = now; if (open && now - quiet >= vox.HOLD) { open = false; gate.gain.setValueAtTime(0, H.currentTime); } }
    }, 5);
  };
  wire(epa, toB, 'A->B'); wire(epb, toA, 'B->A');
  // amateur links carry the operator's callsign: the node only transmits once the profile has one
  await B.call('node.set_profile', { name: 'Radio B', callsign: ${JSON.stringify(cfgB.callsign)} });
  await B.call('contacts.add', { card: ${JSON.stringify(cardA)} });
  globalThis.__B = B; globalThis.__vox = vox; globalThis.__epa = epa;
  return { la: la.id, lb: lb.id, cardB: (await B.call('node.info')).card, linkA: (await A.call('links.list')).links.find((l) => l.id === la.id).config };
})()`;

async function radioPage(t, profile) {
  const browser = await startBrowser(['--use-fake-ui-for-media-stream', '--use-fake-device-for-media-stream', '--autoplay-policy=no-user-gesture-required']);
  if (browser.error) { t.skip(`Chrome could not start: ${browser.error}`); return null; }
  const srv = await startServer(DIST_DIR);
  let P = null;
  t.after(() => runCleanups([() => browser.close(), () => srv.close(), () => P && P.close()])); // page, server, browser
  P = await nodePage(browser, `${srv.url}index.html?nohub=1&profile=${profile}`);
  return P;
}

test('(c) radio wizard: handheld_vox audio links between two nodes through a VOX model, messages both ways', { skip, timeout: 330000 }, async (t) => {
  const P = await radioPage(t, 'voxa');
  if (!P) return;
  const pending = await presetSupported(P, 'handheld_vox');
  // EMBER_PRESET_FALLBACK=1: explicit settings instead of the preset (checks the test plumbing only)
  const fallback = pending && process.env.EMBER_PRESET_FALLBACK === '1';
  if (pending && !fallback) { t.todo(pending); return; }
  await P.call('node.set_profile', { name: 'Radio A', callsign: 'QX1AAA' });
  const a = await P.call('node.info');
  // what the wizard creates for a UV-5R on 2 m: the preset + the operator's callsign (amateur licence;
  // Q-prefixed callsigns are never issued, so these samples cannot belong to anyone)
  const cfg = (callsign) => (fallback ? { modem: 'afsk1200', txdelay_ms: 700, vox_tone_ms: 400, csma: false, callsign } : { preset: 'handheld_vox', callsign });
  const setup = await P.evaluate(RADIO_SETUP(cfg('QX1AAA'), cfg('QX1AAB'), { ptt: 'vox' }, { ptt: 'vox' }, a.card), 60000);
  t.diagnostic(`handheld_vox link config: ${JSON.stringify(setup.linkA)}`);
  await P.call('contacts.add', { card: setup.cardB });
  await new Promise((r) => setTimeout(r, 5000)); // let both nodes' first announcements go out
  // Amateur rules: cleartext signed Public messages only (no encrypted DMs). Half-duplex radios behind
  // VOX can collide; like an operator, resend (at most 3 times) if the other side did not get it.
  const deliver = async (send, seen, label) => {
    for (let i = 1; i <= 3; i++) {
      const text = `${label} ${Date.now()}`;
      await send(text);
      try { await P.waitFor(seen(text), 25000); return i; } catch { t.diagnostic(`${label}: attempt ${i} lost (${await P.evaluate(RADIO_DIAG)})`); }
    }
    throw new Error(`${label}: not delivered in 3 attempts: ${await P.evaluate(RADIO_DIAG)}`);
  };
  const ab = await deliver((x) => P.call('messages.send', { conv: 'ch:public', text: x }),
    (x) => `__B.call('messages.list', { conv: 'ch:public' }).then((r) => r.messages.some((m) => m.text === ${JSON.stringify(x)}))`, 'vox A to B');
  const ba = await deliver((x) => P.evaluate(`__B.call('messages.send', { conv: 'ch:public', text: ${JSON.stringify(x)} }).then(() => true)`),
    (x) => `__emberWasmBackend.call('messages.list', { conv: 'ch:public' }).then((r) => r.messages.some((m) => m.text === ${JSON.stringify(x)}))`, 'vox B to A');
  t.diagnostic(`delivered A->B on attempt ${ab}, B->A on attempt ${ba}`);
  const vox = await P.evaluate('__vox');
  t.diagnostic(`VOX opened ${vox.opened.length} times (attack ${vox.ATTACK} ms clipped each time)`);
  assert.ok(vox.opened.some((o) => o.name === 'A->B') && vox.opened.some((o) => o.name === 'B->A'), 'both directions went through the VOX gate');
  await P.evaluate('__B.destroy().then(() => true)');
  assert.deepEqual(realErrors(P.errors), []);
});

test('(d) handheld_ptt preset: serial RTS keys the radio for the whole transmission, released after playout', { skip, timeout: 150000 }, async (t) => {
  const P = await radioPage(t, 'ptta');
  if (!P) return;
  const pending = await presetSupported(P, 'handheld_ptt');
  const fallback = pending && process.env.EMBER_PRESET_FALLBACK === '1';
  if (pending && !fallback) { t.todo(pending); return; }
  await P.call('node.set_profile', { name: 'Radio A', callsign: 'QX1AAA' });
  const a = await P.call('node.info');
  const cfg = (callsign) => (fallback ? { modem: 'afsk1200', csma: false, persistence: 255, callsign } : { preset: 'handheld_ptt', callsign });
  const setup = await P.evaluate(RADIO_SETUP(cfg('QX1AAA'), cfg('QX1AAB'), { ptt: 'serial_rts' }, { ptt: 'vox' }, a.card), 60000);
  // no VOX needed with PTT: open the A->B gate immediately
  await P.evaluate('(__vox.ATTACK = 0, true)');
  await new Promise((r) => setTimeout(r, 5000)); // let both nodes' first announcements go out
  let got = false;
  for (let i = 1; i <= 3 && !got; i++) { // half duplex: resend like an operator if it collided
    const text = `ptt ${Date.now()}`;
    await P.call('messages.send', { conv: 'ch:public', text });
    try { await waitRadio(P, `__B.call('messages.list', { conv: 'ch:public' }).then((r) => r.messages.some((m) => m.text === ${JSON.stringify(text)}))`, 25000, 'A -> B with PTT'); got = true; } catch (e) { if (i === 3) throw e; t.diagnostic(`attempt ${i} lost`); }
  }
  await P.waitEvent('link.ptt', (d) => d.link_id === setup.la && d.on === false, 20000);
  // wait until A is idle (no transmission keyed or pending release) before reading the line history
  await P.waitFor('!__epa.ptt && !__epa._burst && !__epa._pttCancel', 20000);
  const r = await P.evaluate(`({ sig: __signals, bursts: __epa.bursts, rate: __epa.sampleRate })`);
  const rts = r.sig.filter((s) => 'requestToSend' in s).filter((s, i, x) => i === 0 || x[i - 1].requestToSend !== s.requestToSend);
  assert.ok(rts.some((s) => s.requestToSend === true), `RTS keyed: ${JSON.stringify(rts)}`);
  assert.equal(rts.at(-1).requestToSend, false);
  assert.ok(r.bursts.length >= 1);
  for (const b of r.bursts) assert.ok(b.off_audio >= b.play_end - 0.003, 'released after the last sample played');
  t.diagnostic(`bursts: ${JSON.stringify(r.bursts.map((b) => ({ samples: b.samples, keyed_ms: b.off_ms - b.on_ms })))}`);
  await P.evaluate('__B.destroy().then(() => true)');
  assert.deepEqual(realErrors(P.errors), []);
});

test('rf.radios: the offline radio database answers in the browser (Baofeng UV-5R entry with a recommended link)', { skip, timeout: 60000 }, async (t) => {
  if (!needMedia(t)) return;
  const P = await nodePage(media.browser, `${media.srv.url}index.html?nohub=1&profile=radios`);
  t.after(() => P.close());
  let r;
  try { r = await P.call('rf.radios', { query: 'UV-5R' }); } catch (e) {
    if (e.code === 'not_implemented') { t.todo('core does not implement rf.radios yet (RPC_API §6)'); return; }
    throw e;
  }
  const uv = r.radios.find((x) => /UV-?5R/i.test(`${x.model}`));
  assert.ok(uv, JSON.stringify(r.radios.map((x) => x.model)));
  assert.equal(uv.maker.toLowerCase(), 'baofeng');
  assert.ok(Array.isArray(uv.settings) && uv.settings.length > 0, 'radio settings to make');
  assert.ok(uv.recommended && uv.recommended.link && uv.recommended.link.type === 'audio', JSON.stringify(uv.recommended));
  t.diagnostic(`${r.radios.length} match(es); UV-5R recommended ${JSON.stringify(uv.recommended)}`);
  assert.deepEqual(realErrors(P.errors), []);
});

/** Opens the UI's Morse reader on a fresh profile (past the first-run screen). */
async function openReader(P) {
  if ((await P.call('node.info')).name === '') await P.call('node.set_profile', { name: 'Reader' });
  await P.evaluate('__emberWasmBackend._flush().then(() => true)'); // on disk before the reload
  await P.reloadNode();
  await P.evaluate(`(location.hash = '#/tools/morse?tab=camera', true)`);
  try { await P.waitFor(`!!document.querySelector('.morse-page')`, 15000); } catch {
    throw new Error(`Morse reader did not open: ${await P.evaluate(`location.hash + ' | ' + document.body.innerText.slice(0, 300)`)}`);
  }
  // the UI's own "find the blinking light" option (as the UI smoke test does), then start
  await P.evaluate(`(() => { const c = document.querySelector('.morse-page input[type=checkbox]'); if (c && !c.checked) c.click(); return true; })()`);
  await P.evaluate(`(() => { const b = [...document.querySelectorAll('button')].find((x) => /Start reading/.test(x.textContent)); if (!b) throw new Error('no Start reading button'); b.click(); return true; })()`);
}
const transcriptHas = (text) => `(() => { const e = document.querySelector('.morse-transcript'); const s = e ? e.textContent.replace(/\\s+/g, ' ').trim().toUpperCase() : ''; return s.includes(${JSON.stringify(text)}) ? s : null; })()`;

/**
 * Capability probe for the .y4m variant: Chrome's fake camera advances one file frame per frame it
 * produces, so the recorded light only plays at its real speed when the fake device keeps up (it
 * does on an idle machine; a starved VM manages ~9 of 30 fps). Measures the delivered frame rate.
 */
async function fakeCameraFps(P) {
  return P.evaluate(`(async () => {
    const s = await navigator.mediaDevices.getUserMedia({ video: { width: 64, height: 48, frameRate: 30 } });
    const track = s.getVideoTracks()[0];
    let n = 0;
    if (typeof MediaStreamTrackProcessor === 'function') {
      const r = new MediaStreamTrackProcessor({ track }).readable.getReader();
      const end = performance.now() + 2000;
      while (performance.now() < end) { const { value, done } = await r.read(); if (done) break; value.close(); n++; }
      await r.cancel().catch(() => {});
    }
    track.stop();
    return n / 2;
  })()`, 30000);
}

test('the UI Morse reader (#/tools/morse) with the real camera path: fake camera .y4m -> UI capture library -> core -> on-screen transcript', { skip, timeout: 150000 }, async (t) => {
  if (!needMedia(t)) return;
  const P = await nodePage(media.browser, `${media.srv.url}index.html?nohub=1&profile=morseui`);
  t.after(() => P.close());
  const pending = await morseSupported(P);
  if (pending) { t.todo(pending); return; }
  const fps = await fakeCameraFps(P);
  t.diagnostic(`fake camera delivers ${fps} fps`);
  if (fps < 24) {
    // documented in docs/CI.md: the camera-light variant below covers the same UI path at any speed
    t.skip(`probe: Chrome's fake camera plays its .y4m file at ${fps} of 30 fps here, so the recorded light is not real time`);
    return;
  }
  await openReader(P);
  const tr = await P.waitFor(transcriptHas(LIGHT_TEXT), 45000);
  t.diagnostic(`UI transcript: "${tr}"`);
  assert.deepEqual(realErrors(P.errors), []);
});

test('the UI Morse reader with a live light (clock-driven canvas camera, speed matched to the frame rate): UI capture library -> core -> transcript', { skip, timeout: 180000 }, async (t) => {
  if (!needMedia(t)) return;
  const P = await nodePage(media.browser, `${media.srv.url}index.html?nohub=1&profile=morseui2`);
  t.after(() => P.close());
  const pending = await morseSupported(P);
  if (pending) { t.todo(pending); return; }
  const { CAPTURE_HOOK, MORSE_SENT, MORSE_OK, cameraWpm } = await import('../../../ui/tests/capture_hook.mjs');
  if ((await P.call('node.info')).name === '') await P.call('node.set_profile', { name: 'Reader' });
  await P.evaluate('__emberWasmBackend._flush().then(() => true)');
  await P.reloadNode();
  await P.evaluate(CAPTURE_HOOK(MORSE_SENT, 12));
  await P.evaluate(`(location.hash = '#/tools/morse?tab=camera', true)`);
  await P.waitFor(`!!document.querySelector('.morse-page')`, 15000);
  await P.evaluate(`(() => { const c = document.querySelector('.morse-page input[type=checkbox]'); if (c && !c.checked) c.click(); [...document.querySelectorAll('button')].find((x) => /Start reading/.test(x.textContent)).click(); return true; })()`);
  await P.waitFor(`!!document.querySelector('.morse-transcript[data-live]')`, 15000);
  await new Promise((r) => setTimeout(r, 2000));
  const fps = await P.evaluate('window.__emberCaptureHook.fps()');
  const wpm = cameraWpm(fps);
  const keyMs = await P.evaluate(`window.__emberCaptureHook.go(${wpm})`);
  const tr = await P.waitFor(`(() => { const s = ((document.querySelector('.morse-transcript') || {}).textContent || '').trim(); return new RegExp(${JSON.stringify(MORSE_OK.source)}).test(s) ? s : null; })()`, keyMs + 20000);
  t.diagnostic(`light painted at ${fps} fps -> keyed at ${wpm} WPM; UI transcript "${tr}"`);
  assert.deepEqual(realErrors(P.errors), []);
});
