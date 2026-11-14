// Browser capture for the Morse reader: camera brightness of a watched region and microphone PCM.
// Works wherever the page has getUserMedia (browser, desktop operator page, Android WebView, iOS
// WKWebView). Tests and demos inject media through a hook instead of real devices:
//   window.__emberCaptureHook = { camera: opts => MediaStream, mic: opts => MediaStream }
import { roiBrightness, BlinkFinder, SampleBatcher, Resampler, rmsDb, captureUnavailableReason } from './capture.js';

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
 * Camera reader. Shows the stream in `video`, averages the watched region every frame and calls
 * onBatch({times_ms, levels}) for morse.rx.feed and onFrame({level, roi, auto}) for the UI.
 */
export class CameraReader {
  constructor({ video, onBatch, onFrame, onError }) {
    this.video = video; this.onBatch = onBatch; this.onFrame = onFrame || (() => {}); this.onError = onError || (() => {});
    this.roi = { x: 0.4, y: 0.4, w: 0.2, h: 0.2 };
    this.auto = false;
    this.finder = new BlinkFinder();
    this.batcher = new SampleBatcher(30, 250);
    this.canvas = document.createElement('canvas');
    this.ctx = this.canvas.getContext('2d', { willReadFrequently: true });
    this.stream = null; this.running = false; this.facing = 'environment'; this._cb = 0; this.t0 = 0;
  }
  async start(facing) {
    this.stop();
    if (facing) this.facing = facing;
    this.stream = await getStream('camera', { video: { facingMode: { ideal: this.facing }, width: { ideal: 640 }, height: { ideal: 480 }, frameRate: { ideal: 30 } }, audio: false });
    const track = this.stream.getVideoTracks()[0];
    // Fixed exposure / white balance help, the torch must be off; all optional, ignored when unsupported.
    if (track && track.applyConstraints) {
      for (const c of [{ exposureMode: 'manual' }, { whiteBalanceMode: 'manual' }, { torch: false }, { focusMode: 'continuous' }]) {
        try { await track.applyConstraints({ advanced: [c] }); } catch { /* not supported here */ }
      }
    }
    this.video.muted = true;
    this.video.playsInline = true;
    this.video.setAttribute('playsinline', '');
    this.video.srcObject = this.stream;
    try { await this.video.play(); } catch { /* autoplay of a muted stream is allowed; ignore */ }
    this.running = true;
    this.t0 = performance.now();
    this._loop();
    return track ? (track.getSettings ? track.getSettings() : {}) : {};
  }
  _loop() {
    if (!this.running) return;
    const v = this.video;
    const step = (now, meta) => {
      if (!this.running) return;
      try { this._frame(meta && Number.isFinite(meta.mediaTime) ? meta.mediaTime * 1000 : now); } catch (e) { this.onError(e); }
      this._loop();
    };
    if (v.requestVideoFrameCallback) this._cb = v.requestVideoFrameCallback(step);
    else this._cb = requestAnimationFrame(t => step(t, null));
  }
  _frame(t) {
    const v = this.video;
    const w = v.videoWidth, hgt = v.videoHeight;
    if (!w || !hgt) return;
    const cw = 160, ch = Math.max(1, Math.round(160 * hgt / w));
    if (this.canvas.width !== cw || this.canvas.height !== ch) { this.canvas.width = cw; this.canvas.height = ch; }
    this.ctx.drawImage(v, 0, 0, cw, ch);
    const img = this.ctx.getImageData(0, 0, cw, ch).data;
    if (this.auto) {
      this.finder.push(img, cw, ch);
      const b = this.finder.best();
      if (b && b.score > 0.03) this.roi = b.roi;
    }
    const level = roiBrightness(img, cw, ch, this.roi, 1);
    const batch = this.batcher.add(t, level);
    if (batch) this.onBatch(batch);
    this.onFrame({ level, roi: this.roi, auto: this.auto, finder: this.auto ? this.finder.best() : null });
  }
  setRoi(r) { this.roi = r; this.auto = false; }
  setAuto(on) { this.auto = !!on; if (on) this.finder = new BlinkFinder(); }
  stop() {
    this.running = false;
    if (this._cb) { try { this.video.cancelVideoFrameCallback ? this.video.cancelVideoFrameCallback(this._cb) : cancelAnimationFrame(this._cb); } catch { /* ignore */ } this._cb = 0; }
    if (this.stream) { for (const t of this.stream.getTracks()) t.stop(); this.stream = null; }
    if (this.video) this.video.srcObject = null;
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
