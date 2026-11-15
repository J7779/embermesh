// "Can this host run media in real time?" One probe shared by the UI smoke walks
// (ui/tests/smoke_chrome.mjs) and the web suite (apps/web/test/e2e_*.test.mjs); docs/CI.md lists
// every step that depends on it and the deterministic twin that always runs.
//
// Measured in the page, over about a second:
//   camera  a canvas painted on every animation frame, captured at 30 fps and played in a <video>:
//           frames the video actually presents per second (what a camera reader gets)
//   audio   a ScriptProcessor on a silent AudioContext (the page-side audio path the Morse reader
//           and the single-file audio link use): callbacks that came late (gap > 2 buffers),
//           callbacks missing against the audio clock, and audio clock vs wall clock
//   timers  a 10 ms setTimeout chain: how late it fires (p95 / max)
//   mic     (only on request, in a browser started with Chrome's fake capture devices) the audio the
//           fake microphone delivers per second of wall clock, through MediaStreamTrackProcessor:
//           a starved host plays the device's file chopped, far slower than real time
// A step that needs real time runs only when its part of the probe passes; otherwise it is
// reported as SKIPPED with these numbers (never as passed).

export const LIMITS = {
  camera_fps: 24, // the camera reader needs about 4 frames per dot at 12 WPM (capture_hook.mjs)
  audio_late: 0, // a late ScriptProcessor callback loses input (the reader's 4096-frame buffer)
  audio_missing: 1,
  audio_clock: 0.05, // audio clock within 5 % of the wall clock
  timer_p95_ms: 25,
  timer_max_ms: 250, // one hiccup is tolerated; sustained lateness (p95) is what breaks real time
  mic_ratio: 0.95, // seconds of microphone audio delivered per second
};

/** Page expression; resolves to the measured numbers. parts: any of 'camera', 'audio', 'timers'. */
export const REALTIME_PROBE = (parts = ['camera', 'audio', 'timers'], ms = 1000) => `(async () => {
  const ms = ${Number(ms)}, want = new Set(${JSON.stringify(parts)});
  const out = { ms };
  const jobs = [];
  if (want.has('camera')) jobs.push((async () => {
    const c = document.createElement('canvas'); c.width = 64; c.height = 48;
    const g = c.getContext('2d');
    let paints = 0, run = true;
    const draw = () => { if (!run) return; paints++; g.fillStyle = paints % 2 ? '#fff' : '#000'; g.fillRect(0, 0, 64, 48); requestAnimationFrame(draw); };
    draw();
    const s = c.captureStream(30);
    const v = document.createElement('video'); v.muted = true; v.playsInline = true; v.srcObject = s;
    v.style.cssText = 'position:fixed;left:0;top:0;width:8px;height:6px;opacity:0.01;pointer-events:none';
    document.body.appendChild(v);
    try { await v.play(); } catch {}
    let frames = 0;
    if (v.requestVideoFrameCallback) { const cb = () => { if (!run) return; frames++; v.requestVideoFrameCallback(cb); }; v.requestVideoFrameCallback(cb); }
    await new Promise((r) => setTimeout(r, 200)); // first frames
    const f0 = frames, p0 = paints, t0 = performance.now();
    await new Promise((r) => setTimeout(r, ms));
    const dt = (performance.now() - t0) / 1000;
    run = false; s.getTracks().forEach((t) => t.stop()); v.remove();
    out.camera_fps = Math.round(((v.requestVideoFrameCallback ? frames - f0 : paints - p0) / dt) * 10) / 10;
    out.paint_fps = Math.round(((paints - p0) / dt) * 10) / 10;
  })());
  if (want.has('audio')) jobs.push((async () => {
    const ac = new AudioContext();
    try { await Promise.race([ac.resume(), new Promise((r) => setTimeout(r, 500))]); } catch {}
    if (ac.state !== 'running') { out.audio = { state: ac.state }; ac.close().catch(() => {}); return; }
    const N = 1024, buf = N / ac.sampleRate * 1000;
    const sp = ac.createScriptProcessor(N, 1, 1);
    let n = 0, late = 0, maxGap = 0, last = 0, a0 = 0, w0 = 0, warm = true;
    sp.onaudioprocess = (e) => {
      e.outputBuffer.getChannelData(0).fill(0);
      const now = performance.now();
      if (warm) return;
      if (!n) { a0 = ac.currentTime; w0 = now; }
      else { const gap = now - last; maxGap = Math.max(maxGap, gap); if (gap > 2 * buf) late++; }
      last = now; n++;
    };
    sp.connect(ac.destination);
    await new Promise((r) => setTimeout(r, 200)); warm = false;
    await new Promise((r) => setTimeout(r, ms));
    const a1 = ac.currentTime, w1 = performance.now();
    sp.disconnect(); await ac.close().catch(() => {});
    const audioMs = (a1 - a0) * 1000;
    out.audio = { state: 'running', rate: ac.sampleRate, buffer_ms: Math.round(buf * 10) / 10, callbacks: n,
      missing: Math.max(0, Math.round(audioMs / buf) - n), late, max_gap_ms: Math.round(maxGap),
      clock: n ? Math.round((audioMs / (w1 - w0)) * 1000) / 1000 : 0 };
  })());
  if (want.has('timers')) jobs.push((async () => {
    const lateBy = [];
    const end = performance.now() + ms;
    while (performance.now() < end) {
      const t = performance.now();
      await new Promise((r) => setTimeout(r, 10));
      lateBy.push(performance.now() - t - 10);
    }
    lateBy.sort((a, b) => a - b);
    out.timers = { n: lateBy.length, p95_late_ms: Math.round(lateBy[Math.floor(lateBy.length * 0.95)] || 0), max_late_ms: Math.round(lateBy.at(-1) || 0) };
  })());
  if (want.has('mic')) jobs.push((async () => {
    if (typeof MediaStreamTrackProcessor !== 'function') { out.mic = { state: 'no MediaStreamTrackProcessor' }; return; }
    const s = await navigator.mediaDevices.getUserMedia({ audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false } });
    const track = s.getAudioTracks()[0];
    const r = new MediaStreamTrackProcessor({ track }).readable.getReader();
    let frames = 0, rate = 0, start = 0;
    const warm = performance.now() + 200;
    let ended = false;
    const stop = setTimeout(() => { ended = true; r.cancel().catch(() => {}); }, ms + 1500); // a stalled device
    while (!ended) {
      const { value, done } = await r.read().catch(() => ({ done: true }));
      if (done) break;
      const now = performance.now();
      if (now >= warm) { if (!start) start = now; else frames += value.numberOfFrames; }
      rate = value.sampleRate; value.close();
      if (start && now - start >= ms) break;
    }
    clearTimeout(stop);
    const el = start ? performance.now() - start : 0;
    await r.cancel().catch(() => {}); track.stop();
    out.mic = { state: 'running', rate, ratio: el && rate ? Math.round((frames / rate / (el / 1000)) * 100) / 100 : 0 };
  })());
  await Promise.all(jobs);
  return out;
})()`;

/**
 * Per part: null when the host holds real time, else the reason (with the numbers) a step that
 * needs that part is skipped. A part that was not measured is reported as "not measured".
 */
export function verdict(p) {
  const L = LIMITS;
  const camera = p.camera_fps === undefined ? 'camera not measured'
    : p.camera_fps < L.camera_fps ? `the test camera delivered ${p.camera_fps} fps (painted ${p.paint_fps}), a camera reader needs ${L.camera_fps}` : null;
  const a = p.audio;
  const audio = !a ? 'audio not measured'
    : a.state !== 'running' ? `the AudioContext did not run (${a.state})`
      : a.late > L.audio_late || a.missing > L.audio_missing || Math.abs(a.clock - 1) > L.audio_clock
        ? `audio callbacks were late ${a.late}x (max gap ${a.max_gap_ms} ms for ${a.buffer_ms} ms buffers), ${a.missing} missing, audio clock ${a.clock}x the wall clock` : null;
  const t = p.timers;
  const timers = !t ? 'timers not measured'
    : t.p95_late_ms > L.timer_p95_ms || t.max_late_ms > L.timer_max_ms ? `10 ms timers fired up to ${t.max_late_ms} ms late (p95 ${t.p95_late_ms} ms)` : null;
  const m = p.mic;
  const mic = !m ? 'microphone not measured'
    : m.state !== 'running' ? `the fake microphone could not be measured (${m.state})`
      : m.ratio < L.mic_ratio ? `the fake microphone delivered ${m.ratio} s of audio per second, a sound reader needs real time` : null;
  return { camera, audio, timers, mic };
}

/** One line for logs and the CI job summary (grep "realtime probe"). */
export function probeLine(where, p) {
  const v = verdict(p);
  const ok = (k) => (v[k] ? 'NO' : 'ok');
  const parts = [];
  if (p.camera_fps !== undefined) parts.push(`camera ${p.camera_fps} fps [${ok('camera')}]`);
  if (p.audio) parts.push(p.audio.state === 'running' ? `audio late ${p.audio.late}, missing ${p.audio.missing}, max gap ${p.audio.max_gap_ms} ms, clock ${p.audio.clock} [${ok('audio')}]` : `audio ${p.audio.state} [NO]`);
  if (p.timers) parts.push(`timers p95 +${p.timers.p95_late_ms} ms, max +${p.timers.max_late_ms} ms [${ok('timers')}]`);
  if (p.mic) parts.push(p.mic.state === 'running' ? `microphone ${p.mic.ratio}x real time [${ok('mic')}]` : `microphone ${p.mic.state} [NO]`);
  return `realtime probe (${where}): ${parts.join('; ')}`;
}
