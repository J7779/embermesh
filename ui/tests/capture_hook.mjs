// Shared by ui/tests/smoke_chrome.mjs and apps/web/test/e2e_morse.test.mjs (importing the smoke
// script itself would run it).
/**
 * Test hook for ui/js/lib/capture_media.js: a canvas.captureStream() "camera" showing a light keyed
 * in Morse, and a Web Audio oscillator keyed in Morse as the "microphone". Keying starts only when
 * the walk calls window.__emberCaptureHook.go() — after the reader reports live samples
 * (.morse-transcript[data-live]) — so a slow start never eats the first character.
 */
// What the test light/tone keys: a "VVV" tune-up first (as operators do), so the decoder's tone and
// speed tracking have settled before the message; the walk checks the message that follows.
export const MORSE_SENT = 'VVV CQ TEST';
export const MORSE_OK = /(^|\s)CQ TEST$/;

/**
 * The camera's scene (RPC_API §12 reader: whole-frame grids, a tracker that finds the light): a
 * 1280×720 dark picture with a steady bright lamp (which the tracker must ignore) and, elsewhere, a
 * tiny keyed light of LIGHT.s px. `keyed: false` leaves only the lamp (nothing to read).
 */
export const SCENE = { w: 1280, h: 720, bg: '#181818', lamp: { x: 140, y: 500, s: 48 }, light: { x: 958, y: 178, s: 4 } };
// page source: paint(g, lit, keyed) draws one frame of the scene into a 2D context
const PAINT = `const S = ${JSON.stringify(SCENE)};
  const paint = (g, lit, keyed = true) => {
    g.fillStyle = S.bg; g.fillRect(0, 0, S.w, S.h);
    g.fillStyle = '#ffffff'; g.fillRect(S.lamp.x, S.lamp.y, S.lamp.s, S.lamp.s);
    if (keyed && lit) g.fillRect(S.light.x, S.light.y, S.light.s, S.light.s);
  };`;

export const CAPTURE_HOOK = (text, wpm, { keyed = true } = {}) => `(async () => {
  ${PAINT}
  const { toMorse, morsePattern } = await import(new URL('js/lib/morse.js', location.href).href);
  let pat = morsePattern(toMorse(${JSON.stringify(text)}), ${wpm});
  const on = t => { let a = 0; for (let i = 0; i < pat.length; i++) { if (t < a + pat[i]) return i % 2 === 0; a += pat[i]; } return false; };
  let t0 = Infinity, mics = [], draws = [];
  window.__emberCaptureHook = {
    // go(wpm?): start keying (optionally at another speed); returns the keying time in ms
    go: (w) => { if (w) pat = morsePattern(toMorse(${JSON.stringify(text)}), w); t0 = performance.now() + 300; for (const m of mics) m(); mics = []; return pat.reduce((x, y) => x + y, 0) + 300; },
    // camera frames per second actually produced over the last 2 s (this page's paint rate)
    fps: () => { const now = performance.now(); draws = draws.filter(d => now - d < 2000); return draws.length / 2; },
    camera: () => {
      const c = document.createElement('canvas'); c.width = S.w; c.height = S.h;
      const g = c.getContext('2d');
      // The light's state is a function of the clock, so each frame shows what a real lamp would show
      // at that moment, however few frames a busy machine manages to paint.
      const draw = () => { const n = performance.now(); draws.push(n); if (draws.length > 400) draws.splice(0, 200); const t = n - t0; paint(g, t > 0 && on(t), ${keyed}); requestAnimationFrame(draw); };
      draw(); return c.captureStream(30);
    },
    mic: () => {
      const ac = new AudioContext(); const o = ac.createOscillator(); o.frequency.value = 650; const gn = ac.createGain(); gn.gain.value = 0;
      const dst = ac.createMediaStreamDestination(); o.connect(gn).connect(dst); o.start();
      mics.push(() => {
        const s0 = ac.currentTime + 0.3; let t = 0;
        for (let i = 0; i < pat.length; i++) { gn.gain.setValueAtTime(i % 2 === 0 ? 0.5 : 0, s0 + t / 1000); t += pat[i]; }
        gn.gain.setValueAtTime(0, s0 + t / 1000);
      });
      return dst.stream;
    },
  };
  return true;
})()`;
/**
 * Keying speed a camera can follow: 4 frames per dot (the core decoder needs about 2–3; the margin
 * covers frames a busy machine paints late). A busy machine paints the test light at a few frames
 * per second; a real camera at 30 fps reads 12 WPM.
 */
export const cameraWpm = fps => Math.max(3, Math.min(12, Math.floor(1200 / (4 * 1000 / Math.max(1, fps)))));

// ---- deterministic twins (docs/CI.md): the same keying, timed by sample / frame index ----------
// They never depend on how fast the host runs, so they run on every runner, including the ones
// where the live steps above are skipped by the real-time probe (realtime_probe.mjs).
const API = `((window.__ember && window.__ember.api) || globalThis.__emberWasmBackend)`;

/**
 * Sound in sample time: the hook's 650 Hz oscillator and gain keying rendered by an
 * OfflineAudioContext at 48 kHz, cut into 4096-frame blocks (what the reader's ScriptProcessor
 * hands over), resampled to 8 kHz by the UI's own Resampler and fed to morse.rx -> {transcript}.
 */
export const SOUND_TWIN = (text, wpm) => `(async () => {
  const { toMorse, morsePattern } = await import(new URL('js/lib/morse.js', location.href).href);
  const { Resampler, pcmToB64 } = await import(new URL('js/lib/capture.js', location.href).href);
  const api = ${API};
  const pat = morsePattern(toMorse(${JSON.stringify(text)}), ${wpm});
  const rate = 48000, lead = 300, tail = 1500;
  const total = pat.reduce((a, b) => a + b, 0) + lead + tail;
  const oc = new OfflineAudioContext(1, Math.ceil(total / 1000 * rate), rate);
  const o = oc.createOscillator(); o.frequency.value = 650;
  const gn = oc.createGain(); gn.gain.value = 0;
  o.connect(gn).connect(oc.destination); o.start();
  let t = lead;
  for (let i = 0; i < pat.length; i++) { gn.gain.setValueAtTime(i % 2 === 0 ? 0.5 : 0, t / 1000); t += pat[i]; }
  gn.gain.setValueAtTime(0, t / 1000);
  const x = (await oc.startRendering()).getChannelData(0);
  const { session } = await api.call('morse.rx.start', { source: 'audio', sample_rate: 8000 });
  const res = new Resampler(rate, 8000);
  let fed = 0;
  for (let i = 0; i < x.length; i += 4096) {
    const pcm = res.push(x.subarray(i, i + 4096));
    if (pcm.length) { fed += pcm.length; await api.call('morse.rx.feed', { session, pcm_b64: pcmToB64(pcm) }); }
  }
  const st = await api.call('morse.rx.status', { session });
  await api.call('morse.rx.stop', { session });
  return { transcript: String(st.transcript || '').trim(), wpm: st.wpm, samples_8k: fed };
})()`;

/**
 * Camera in frame time: the hook's scene (SCENE) rendered frame by frame at `fps`, frame n stamped
 * n / fps s, then exactly what the reader does with a camera frame (ui/js/lib/capture_media.js
 * CameraReader, grid mode): scaled to at most 960 px wide, max-pooled by the UI's own
 * gridSizeFor / maxPoolLuma, batched by its FrameBatcher and fed to morse.rx (§12 frames).
 * No tap, no pin: the tracker must find the light itself.
 * -> {transcript, frames, locked_ms (frame time of the first "locked"), target (last), states}
 */
export const CAMERA_TWIN = (text, wpm, { keyed = true, fps = 30, lead = 1500, tail = 2500 } = {}) => `(async () => {
  ${PAINT}
  const { toMorse, morsePattern } = await import(new URL('js/lib/morse.js', location.href).href);
  const { gridSizeFor, maxPoolLuma, FrameBatcher } = await import(new URL('js/lib/capture.js', location.href).href);
  const api = ${API};
  const pat = morsePattern(toMorse(${JSON.stringify(text)}), ${wpm});
  const total = pat.reduce((a, b) => a + b, 0) + ${lead} + ${tail};
  const on = (ms) => { let a = ${lead}; for (let i = 0; i < pat.length; i++) { if (ms >= a && ms < a + pat[i]) return i % 2 === 0; a += pat[i]; } return false; };
  const big = new OffscreenCanvas(S.w, S.h), bg = big.getContext('2d');
  const cw = Math.min(960, S.w), ch = Math.max(1, Math.round(cw * S.h / S.w));
  const small = new OffscreenCanvas(cw, ch), sg = small.getContext('2d', { willReadFrequently: true });
  const grid = gridSizeFor(cw, ch), fb = new FrameBatcher(8, 100);
  const { session } = await api.call('morse.rx.start', { source: 'light' });
  let out = {}, frames = 0, locked_ms = null, target = null;
  const states = {};
  const seen = (st) => { const tg = st && st.target; if (!tg) return; target = tg; states[tg.state] = (states[tg.state] || 0) + 1; if (tg.state === 'locked' && locked_ms === null) locked_ms = Math.round(frames * 1000 / ${fps}); };
  for (let n = 0; n * 1000 / ${fps} < total; n++) {
    const ms = n * 1000 / ${fps};
    paint(bg, on(ms), ${keyed});
    sg.drawImage(big, 0, 0, cw, ch);
    const { max, mean } = maxPoolLuma(sg.getImageData(0, 0, cw, ch).data, cw, ch, grid.w, grid.h, { out });
    out = { max, mean }; frames++;
    const b = fb.add(ms, grid.w, grid.h, max, mean);
    if (b) seen(await api.call('morse.rx.feed', { session, ...b }));
  }
  const rest = fb.flush();
  if (rest) seen(await api.call('morse.rx.feed', { session, ...rest }));
  const st = await api.call('morse.rx.status', { session });
  seen(st);
  await api.call('morse.rx.stop', { session });
  return { transcript: String(st.transcript || '').trim(), wpm: st.wpm, frames, locked_ms, target, states, grid };
})()`;

/** Where the keyed light is, as a fraction of the picture (what a locked target must point at). */
export const LIGHT_AT = { x: (SCENE.light.x + SCENE.light.s / 2) / SCENE.w, y: (SCENE.light.y + SCENE.light.s / 2) / SCENE.h };

/** Checks a CAMERA_TWIN result; returns the list of problems (empty = ok). */
export function cameraTwinProblems(r, { keyed = true } = {}) {
  const p = [];
  if (!r) return ['no result'];
  if (keyed) {
    if (r.locked_ms === null) p.push(`never locked (states ${JSON.stringify(r.states)})`);
    const t = r.target || {};
    if (!(Math.abs(t.x - LIGHT_AT.x) < 0.04 && Math.abs(t.y - LIGHT_AT.y) < 0.06)) p.push(`target at ${t.x},${t.y}, the keyed light is at ${LIGHT_AT.x.toFixed(3)},${LIGHT_AT.y.toFixed(3)}`);
    if (!MORSE_OK.test(r.transcript)) p.push(`transcript ${JSON.stringify(r.transcript)} (expected it to end with "CQ TEST")`);
  } else {
    if (r.states.locked) p.push(`locked on a steady lamp (states ${JSON.stringify(r.states)})`);
    if (r.transcript) p.push(`transcript ${JSON.stringify(r.transcript)} without a keyed light`);
  }
  return p;
}
