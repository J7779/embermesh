// Browser capture for the Morse reader: whole-frame brightness grids of the camera (RPC_API §12; the
// watched-region levels of §6 as the fallback for older cores) and microphone PCM.
// Works wherever the page has getUserMedia (browser, desktop operator page, Android WebView, iOS
// WKWebView). Tests and demos inject media through a hook instead of real devices:
//   window.__emberCaptureHook = { camera: opts => MediaStream, mic: opts => MediaStream }
import { roiBrightness, BlinkFinder, SampleBatcher, Resampler, rmsDb, captureUnavailableReason, gridSizeFor, maxPoolLuma, FrameBatcher } from './capture.js';

const hook = () => (typeof window !== 'undefined' && window.__emberCaptureHook) || null;

/** Why capture cannot even be tried here (null = try getUserMedia). */
export function unavailableReason(kind) {
  const h = hook();
  if (h && h[kind === 'camera' ? 'camera' : 'mic']) return null;
  return captureUnavailableReason({
    mediaDevices: !!(navigator.mediaDevices && navigator.mediaDevices.getUserMedia),
    secureContext: window.isSecureContext !== false,
  }, kind === 'camera' ? 'camera' : 'microphone');
}

/** Friendly text for a getUserMedia failure. */
export function mediaErrorText(e, kind) {
  const what = kind === 'camera' ? 'camera' : 'microphone';
  const name = e && e.name;
  if (name === 'NotAllowedError' || name === 'SecurityError') return 'Permission to use the ' + what + ' was refused. Allow it in the browser’s site settings (or the app’s permissions in the system settings) and try again.';
  if (name === 'NotFoundError' || name === 'OverconstrainedError') return 'No ' + what + ' found on this device' + (kind === 'camera' ? ' (or not the one asked for — try switching camera).' : '.');
  if (name === 'NotReadableError' || name === 'AbortError') return 'The ' + what + ' is busy or blocked — close other apps that use it and try again.';
  return 'Could not open the ' + what + ': ' + String((e && e.message) || e);
}

async function getStream(kind, constraints) {
  const h = hook();
  if (h && h[kind]) return h[kind](constraints);
  return navigator.mediaDevices.getUserMedia(constraints);
}

/**
 * Camera reader (docs/RPC_API.md §12). Shows the stream in `video` (only for the user: frames are
 * reduced here and never stored or sent) and, per camera frame:
 *  - mode "grid" (default): draws the frame to a canvas at up to `maxWidth` px (enough to keep a
 *    one-pixel light alive), max-pools luma over the WHOLE frame into a 64×36 / 48×36 grid and
 *    calls onBatch({frames:[{t_ms, w, h, max_b64, mean_b64}]}) — at most 8 frames / 100 ms;
 *  - mode "levels" (fallback for a core without frame support, §6): finds the blinking region
 *    itself (BlinkFinder) and calls onBatch({times_ms, levels}).
 * Frame times come from requestVideoFrameCallback metadata (captureTime, else mediaTime), not from
 * timers. The camera is released by stop(), and also when the page is hidden or closed.
 */
export class CameraReader {
  constructor({ video, onBatch, onFrame, onError, onStopped, mode = 'grid', maxWidth = 960 }) {
    this.video = video; this.onBatch = onBatch; this.onFrame = onFrame || (() => {}); this.onError = onError || (() => {});
    this.onStopped = onStopped || (() => {});
    this.mode = mode; this.maxWidth = maxWidth;
    this.roi = { x: 0.4, y: 0.4, w: 0.2, h: 0.2 };
    this.finder = new BlinkFinder();
    this.batcher = new SampleBatcher(30, 250);
    this.frames = new FrameBatcher(8, 100);
    this.canvas = document.createElement('canvas');
    this.ctx = this.canvas.getContext('2d', { willReadFrequently: true });
    this.stream = null; this.track = null; this.running = false; this.facing = 'environment'; this._cb = 0;
    this.clock = null; this.pool = { out: {} }; this.cost = { n: 0, ms: 0, max: 0 };
    this._hide = () => { if (document.visibilityState === 'hidden') this.stop('hidden'); };
    this._pagehide = () => this.stop('pagehide');
  }
  async start(facing) {
    this.stop();
    if (facing) this.facing = facing;
    // rear camera, high resolution at ≥ 24 fps (60 when offered), no audio
    this.stream = await getStream('camera', { video: { facingMode: { ideal: this.facing }, width: { ideal: 1280 }, height: { ideal: 720 }, frameRate: { ideal: 60 } }, audio: false });
    const track = this.track = this.stream.getVideoTracks()[0] || null;
    // torch off, focus continuous, exposure biased down (a bright point must not bloom); optional
    const caps = track && track.getCapabilities ? track.getCapabilities() : {};
    const tries = [{ torch: false }, { focusMode: 'continuous' }];
    if (caps.exposureCompensation && Number.isFinite(caps.exposureCompensation.min)) tries.push({ exposureCompensation: Math.max(caps.exposureCompensation.min, -1.5) });
    if (track && track.applyConstraints) for (const c of tries) { try { await track.applyConstraints({ advanced: [c] }); } catch { /* not supported here */ } }
    this.video.muted = true;
    this.video.playsInline = true;
    this.video.setAttribute('playsinline', '');
    this.video.srcObject = this.stream;
    try { await this.video.play(); } catch { /* a muted stream may autoplay; ignore */ }
    this.running = true;
    this.clock = null;
    document.addEventListener('visibilitychange', this._hide);
    window.addEventListener('pagehide', this._pagehide);
    this._loop();
    return track && track.getSettings ? track.getSettings() : {};
  }
  /** Fallback for a core without frame support (§6 levels). */
  setMode(mode) { this.mode = mode; this.frames = new FrameBatcher(8, 100); this.batcher = new SampleBatcher(30, 250); }
  /** Exposure and white balance locked once the tracker reports "locked" (§12); best effort. */
  async lockExposure(on = true) {
    const t = this.track;
    if (!t || !t.applyConstraints) return false;
    let ok = false;
    for (const c of on ? [{ exposureMode: 'manual' }, { whiteBalanceMode: 'manual' }] : [{ exposureMode: 'continuous' }, { whiteBalanceMode: 'continuous' }]) {
      try { await t.applyConstraints({ advanced: [c] }); ok = true; } catch { /* not supported */ }
    }
    return ok;
  }
  /** Zoom steps the camera offers among 1×, 2×, 5× (empty when it has no zoom). */
  zoomSteps() {
    const z = this.track && this.track.getCapabilities ? this.track.getCapabilities().zoom : null;
    if (!z || !(z.max > z.min)) return [];
    return [1, 2, 5].filter(v => v >= z.min && v <= z.max);
  }
  async setZoom(v) { try { await this.track.applyConstraints({ advanced: [{ zoom: v }] }); return true; } catch { return false; } }
  _loop() {
    if (!this.running) return;
    const v = this.video;
    const step = (now, meta) => {
      if (!this.running) return;
      try { this._frame(this._time(now, meta)); } catch (e) { this.onError(e); }
      this._loop();
    };
    if (v.requestVideoFrameCallback) this._cb = v.requestVideoFrameCallback(step);
    else this._cb = requestAnimationFrame(t => step(t, null));
  }
  /** One clock for the whole session: capture time, else the stream's media time, else now. */
  _time(now, meta) {
    if (!this.clock) this.clock = meta && Number.isFinite(meta.captureTime) ? 'capture' : meta && Number.isFinite(meta.mediaTime) ? 'media' : 'now';
    if (this.clock === 'capture' && meta && Number.isFinite(meta.captureTime)) return meta.captureTime;
    if (this.clock === 'media' && meta && Number.isFinite(meta.mediaTime)) return meta.mediaTime * 1000;
    return now;
  }
  _frame(t) {
    const v = this.video;
    const w = v.videoWidth, hgt = v.videoHeight;
    if (!w || !hgt) return;
    if (this.mode === 'levels') return this._levels(t, w, hgt);
    const t0 = performance.now();
    const cw = Math.min(this.maxWidth, w), ch = Math.max(1, Math.round(cw * hgt / w));
    if (this.canvas.width !== cw || this.canvas.height !== ch) { this.canvas.width = cw; this.canvas.height = ch; }
    this.ctx.drawImage(v, 0, 0, cw, ch);
    const img = this.ctx.getImageData(0, 0, cw, ch).data;
    const g = gridSizeFor(cw, ch);
    const { max, mean } = maxPoolLuma(img, cw, ch, g.w, g.h, { out: this.pool.out });
    this.pool.out = { max, mean };
    const ms = performance.now() - t0;
    this.cost.n++; this.cost.ms += ms; this.cost.max = Math.max(this.cost.max, ms);
    const batch = this.frames.add(t, g.w, g.h, max, mean);
    if (batch) this.onBatch(batch);
    this.onFrame({ t, grid: g, width: cw, height: ch, poolMs: ms });
  }
  _levels(t, w, hgt) {
    const cw = 160, ch = Math.max(1, Math.round(160 * hgt / w));
    if (this.canvas.width !== cw || this.canvas.height !== ch) { this.canvas.width = cw; this.canvas.height = ch; }
    this.ctx.drawImage(this.video, 0, 0, cw, ch);
    const img = this.ctx.getImageData(0, 0, cw, ch).data;
    this.finder.push(img, cw, ch);
    const b = this.finder.best();
    if (b && b.score > 0.03) this.roi = b.roi;
    const level = roiBrightness(img, cw, ch, this.roi, 1);
    const batch = this.batcher.add(t, level);
    if (batch) this.onBatch(batch);
    this.onFrame({ t, level });
  }
  /** Average grid reduction cost per frame (ms), for diagnostics and tests. */
  costMs() { return this.cost.n ? { avg: this.cost.ms / this.cost.n, max: this.cost.max, frames: this.cost.n } : null; }
  stop(reason) {
    const was = this.running || !!this.stream;
    this.running = false;
    document.removeEventListener('visibilitychange', this._hide);
    window.removeEventListener('pagehide', this._pagehide);
    if (this._cb) { try { this.video.cancelVideoFrameCallback ? this.video.cancelVideoFrameCallback(this._cb) : cancelAnimationFrame(this._cb); } catch { /* ignore */ } this._cb = 0; }
    if (this.mode === 'grid') { const rest = this.frames.flush(); if (rest && this.stream) try { this.onBatch(rest); } catch { /* ignore */ } }
    if (this.stream) { for (const t of this.stream.getTracks()) { try { t.stop(); } catch { /* gone */ } } this.stream = null; }
    this.track = null;
    if (this.video) this.video.srcObject = null;
    if (was && reason) this.onStopped(reason);
  }
}

/** Microphone reader: raw audio (no echo cancellation / noise suppression / AGC) → Int16 PCM at `rate`. */
export class MicReader {
  constructor({ rate = 8000, onPcm, onLevel, onLive }) {
    this.rate = rate; this.onPcm = onPcm; this.onLevel = onLevel || (() => {}); this.onLive = onLive || (() => {});
    this.ac = null; this.stream = null; this.node = null; this.src = null; this.pending = []; this.pendingN = 0;
  }
  async start() {
    this.stop();
    this.stream = await getStream('mic', { audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false, channelCount: 1 }, video: false });
    const AC = window.AudioContext || window.webkitAudioContext;
    this.ac = new AC();
    try { await this.ac.resume(); } catch { /* ignore */ }
    this.src = this.ac.createMediaStreamSource(this.stream);
    this.res = new Resampler(this.ac.sampleRate, this.rate);
    // ScriptProcessor: deprecated but available in every web view; 4096 frames ≈ 85 ms at 48 kHz.
    this.node = this.ac.createScriptProcessor(4096, 1, 1);
    let live = false;
    this.node.onaudioprocess = e => {
      const input = e.inputBuffer.getChannelData(0);
      // first block of samples: the stream is really flowing now (not just "started")
      if (!live) { live = true; try { this.onLive(); } catch { /* ignore */ } }
      this.onLevel(rmsDb(input));
      const pcm = this.res.push(input);
      if (pcm.length) this.onPcm(pcm);
      e.outputBuffer.getChannelData(0).fill(0);
    };
    this.src.connect(this.node);
    this.node.connect(this.ac.destination); // needed for processing to run; outputs silence
    return { sampleRate: this.ac.sampleRate };
  }
  stop() {
    if (this.node) { try { this.node.disconnect(); this.src.disconnect(); } catch { /* ignore */ } this.node = null; }
    if (this.stream) { for (const t of this.stream.getTracks()) t.stop(); this.stream = null; }
    if (this.ac) { this.ac.close().catch(() => {}); this.ac = null; }
  }
}
