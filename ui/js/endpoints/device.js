// Browser device services behind host.* methods (RPC_API §3): QR scanning, file save/pick,
// geolocation, screen/torch flash, audible tones, wake lock, notifications.
// Every capability is feature-detected; detect*() returns {available, reason?}.
import { bytesToB64, b64ToBytes } from './framing.js';
import { isSecure } from './base.js';

const SECURE_HINT = 'HTTPS, localhost, or the single-file version opened from disk';

function err(code, message) { return Object.assign(new Error(message), { code }); }

function haveDom(env) { return !!(env.document && env.document.body); }

// ---------------------------------------------------------------------------------------------
// QR scanning (BarcodeDetector + camera)

export function detectQr(env = globalThis) {
  if (typeof env.BarcodeDetector !== 'function') {
    return { available: false, reason: 'QR scanning needs a browser with a built-in barcode reader (Chrome on Android, ChromeOS or macOS); paste the code as text instead' };
  }
  if (!isSecure(env)) return { available: false, reason: `The camera needs a secure page: ${SECURE_HINT}` };
  if (!env.navigator || !env.navigator.mediaDevices || !env.navigator.mediaDevices.getUserMedia) {
    return { available: false, reason: 'This browser does not allow camera access' };
  }
  return { available: true };
}

export async function qrScan(env = globalThis, { timeoutMs = 0 } = {}) {
  const d = detectQr(env);
  if (!d.available) throw err('unsupported', d.reason);
  const formats = env.BarcodeDetector.getSupportedFormats ? await env.BarcodeDetector.getSupportedFormats() : ['qr_code'];
  if (!formats.includes('qr_code')) throw err('unsupported', 'This browser\'s barcode reader cannot read QR codes');
  const detector = new env.BarcodeDetector({ formats: ['qr_code'] });
  let stream;
  try {
    stream = await env.navigator.mediaDevices.getUserMedia({ video: { facingMode: 'environment' }, audio: false });
  } catch (e) {
    throw err('denied', e && e.name === 'NotAllowedError' ? 'camera permission denied' : `camera unavailable: ${(e && e.message) || e}`);
  }
  const doc = env.document;
  const overlay = doc.createElement('div');
  overlay.setAttribute('role', 'dialog');
  overlay.setAttribute('aria-label', 'Scan QR code');
  overlay.style.cssText = 'position:fixed;inset:0;z-index:2147483646;background:#000;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:12px';
  const video = doc.createElement('video');
  video.setAttribute('playsinline', '');
  video.muted = true;
  video.style.cssText = 'max-width:100%;max-height:80vh;object-fit:contain';
  const btn = doc.createElement('button');
  btn.type = 'button';
  btn.textContent = 'Cancel';
  btn.style.cssText = 'font-size:18px;padding:10px 28px;border-radius:8px;border:0';
  overlay.append(video, btn);
  doc.body.appendChild(overlay);
  video.srcObject = stream;
  await video.play().catch(() => {});
  const cleanup = () => { for (const t of stream.getTracks()) t.stop(); overlay.remove(); };
  return new Promise((resolve, reject) => {
    let done = false;
    const finish = (fn, v) => { if (done) return; done = true; cleanup(); fn(v); };
    btn.onclick = () => finish(reject, err('denied', 'QR scan cancelled'));
    if (timeoutMs > 0) setTimeout(() => finish(reject, err('busy', 'QR scan timed out')), timeoutMs);
    const loop = async () => {
      if (done) return;
      try {
        if (video.readyState >= 2) {
          const codes = await detector.detect(video);
          if (codes && codes.length && codes[0].rawValue) return finish(resolve, { text: codes[0].rawValue });
        }
      } catch {}
      setTimeout(loop, 150);
    };
    loop();
  });
}

// ---------------------------------------------------------------------------------------------
// Files

export function detectFile(env = globalThis) {
  if (!haveDom(env) || typeof env.Blob !== 'function') return { available: false, reason: 'No document to save files from' };
  return { available: true };
}

export async function fileSave(env, { name, mime, data_b64, share } = {}) {
  if (!name || typeof data_b64 !== 'string') throw err('bad_params', 'name and data_b64 are required');
  const bytes = b64ToBytes(data_b64);
  const type = mime || 'application/octet-stream';
  const nav = env.navigator || {};
  if (share && nav.canShare && typeof env.File === 'function') {
    const file = new env.File([bytes], name, { type });
    if (nav.canShare({ files: [file] })) {
      try { await nav.share({ files: [file], title: name }); return {}; } catch (e) {
        if (e && e.name === 'AbortError') throw err('denied', 'sharing cancelled');
      }
    }
  }
  const d = detectFile(env);
  if (!d.available) throw err('unsupported', d.reason);
  const url = URL.createObjectURL(new env.Blob([bytes], { type }));
  const a = env.document.createElement('a');
  a.href = url;
  a.download = name;
  a.rel = 'noopener';
  a.style.display = 'none';
  env.document.body.appendChild(a);
  a.click();
  setTimeout(() => { a.remove(); URL.revokeObjectURL(url); }, 30000);
  return {};
}

export function filePick(env, { accept } = {}) {
  const d = detectFile(env);
  if (!d.available) return Promise.reject(err('unsupported', d.reason));
  return new Promise((resolve, reject) => {
    const input = env.document.createElement('input');
    input.type = 'file';
    if (accept) input.accept = accept;
    input.style.display = 'none';
    env.document.body.appendChild(input);
    let settled = false;
    const done = (fn, v) => { if (settled) return; settled = true; input.remove(); fn(v); };
    input.addEventListener('cancel', () => done(reject, err('denied', 'no file chosen')));
    input.addEventListener('change', async () => {
      const f = input.files && input.files[0];
      if (!f) return done(reject, err('denied', 'no file chosen'));
      try {
        const buf = new Uint8Array(await f.arrayBuffer());
        done(resolve, { name: f.name, mime: f.type || 'application/octet-stream', data_b64: bytesToB64(buf) });
      } catch (e) { done(reject, err('io_error', String(e.message || e))); }
    });
    input.click();
  });
}

// ---------------------------------------------------------------------------------------------
// Location

export function detectLocation(env = globalThis) {
  const nav = env.navigator || {};
  if (!nav.geolocation) return { available: false, reason: 'This browser has no location (Geolocation) support' };
  if (!isSecure(env)) return { available: false, reason: `Location needs a secure page: ${SECURE_HINT}` };
  return { available: true };
}

export function toPosition(p) {
  const c = p.coords;
  const out = { lat: c.latitude, lon: c.longitude, ts: Math.round(p.timestamp || Date.now()) };
  if (c.altitude !== null && c.altitude !== undefined) out.alt = c.altitude;
  if (c.accuracy !== null && c.accuracy !== undefined) out.accuracy = c.accuracy;
  return out;
}

const GEO_ERRORS = { 1: ['denied', 'location permission denied'], 2: ['io_error', 'position unavailable (no GPS fix?)'], 3: ['busy', 'location request timed out'] };

export function locationGet(env, { timeoutMs = 30000 } = {}) {
  const d = detectLocation(env);
  if (!d.available) return Promise.reject(err('unsupported', d.reason));
  return new Promise((resolve, reject) => {
    env.navigator.geolocation.getCurrentPosition(
      (p) => resolve(toPosition(p)),
      (e) => { const [code, msg] = GEO_ERRORS[e && e.code] || ['io_error', String((e && e.message) || e)]; reject(err(code, msg)); },
      { enableHighAccuracy: true, timeout: timeoutMs, maximumAge: 10000 });
  });
}

/** Starts watching; returns a stop() function. */
export function locationWatch(env, onPos, onErr) {
  const d = detectLocation(env);
  if (!d.available) throw err('unsupported', d.reason);
  const id = env.navigator.geolocation.watchPosition((p) => onPos(toPosition(p)), (e) => onErr && onErr(e),
    { enableHighAccuracy: true, maximumAge: 30000 });
  return () => env.navigator.geolocation.clearWatch(id);
}

// ---------------------------------------------------------------------------------------------
// Signalling (docs/RPC_API.md §10): Morse by light — the camera torch when this browser can reach
// one, else a full-screen flash — and audible tones.
//
// Host rules this code keeps: the light always goes off (end, stop, error, page hidden/closed);
// the camera is requested only when a torch flash starts, its frames are never read (the stream is
// attached to nothing) and every track is stopped as soon as the flash ends; nothing is stored or
// sent; edges are scheduled against deadlines from the pattern start (no accumulated drift).

export const SIGNAL_LIMITS = Object.freeze({ maxEntries: 4096, minMs: 10, maxMs: 60000, maxTotalMs: 600000 });

/** Validates pattern_ms against the §10 limits; returns numbers. Pure; exported for tests. */
export function checkPattern(pattern) {
  if (!Array.isArray(pattern) || !pattern.length) throw err('bad_params', 'pattern_ms must be a non-empty array');
  if (pattern.length > SIGNAL_LIMITS.maxEntries) throw err('bad_params', `pattern_ms has more than ${SIGNAL_LIMITS.maxEntries} entries`);
  const p = pattern.map(Number);
  if (p.some((x) => !(x >= SIGNAL_LIMITS.minMs && x <= SIGNAL_LIMITS.maxMs))) throw err('bad_params', `pattern_ms values must be ${SIGNAL_LIMITS.minMs}..${SIGNAL_LIMITS.maxMs} ms`);
  if (p.reduce((a, b) => a + b, 0) > SIGNAL_LIMITS.maxTotalMs) throw err('bad_params', 'pattern_ms is longer than 10 minutes');
  return p;
}

export function detectFlash(env = globalThis) {
  if (!haveDom(env)) return { available: false, reason: 'No screen to flash' };
  return { available: true };
}

// Once a torch attempt found no torch on this device, say "no" for the rest of the session.
const torchMemo = new WeakMap();
const noTorch = (env) => torchMemo.get(env) === 'no';

/** Phones and tablets may have a rear camera with a torch; desktops and laptops do not. */
function mayHaveRearCamera(env) {
  const nav = env.navigator || {};
  if (nav.userAgentData && typeof nav.userAgentData.mobile === 'boolean' && nav.userAgentData.mobile) return true;
  const ua = String(nav.userAgent || '');
  if (/Android|iPhone|iPad|iPod|Mobile|Tablet/i.test(ua)) return true;
  // iPadOS reports a desktop user agent but has touch points
  return /Macintosh/.test(ua) && Number(nav.maxTouchPoints) > 1;
}

/** host.info.signal: {torch:"yes"|"no"|"ask", screen}. Browsers reach a torch only through the camera. */
export function detectSignal(env = globalThis) {
  const md = env.navigator && env.navigator.mediaDevices;
  const canAsk = !!(md && typeof md.getUserMedia === 'function') && isSecure(env) && mayHaveRearCamera(env) && !noTorch(env);
  return { torch: canAsk ? 'ask' : 'no', screen: detectFlash(env).available };
}

let current = null; // the running flash or tone: {stop()}

/** host.signal.stop: stops a running flash or tone at once; a no-op otherwise. */
export function signalStop() {
  if (current) current.stop();
  return {};
}

const MODES = new Set(['auto', 'torch', 'screen']);

/** Opens the rear camera only to reach its torch. Returns {stream, track} or throws {code}. */
async function openTorch(env) {
  const md = env.navigator && env.navigator.mediaDevices;
  if (!md || typeof md.getUserMedia !== 'function') throw err('unsupported', 'This browser cannot reach a flashlight');
  let stream;
  try {
    stream = await md.getUserMedia({ video: { facingMode: 'environment' }, audio: false });
  } catch (e) {
    if (e && (e.name === 'NotAllowedError' || e.name === 'SecurityError')) throw err('permission_denied', 'Camera permission is needed to use the flashlight');
    torchMemo.set(env, 'no');
    throw err('unsupported', 'No camera with a flashlight');
  }
  const stopAll = () => { for (const t of stream.getTracks()) { try { t.stop(); } catch { /* gone */ } } };
  const track = stream.getVideoTracks()[0];
  const caps = track && typeof track.getCapabilities === 'function' ? track.getCapabilities() : {};
  if (!track || !caps || !caps.torch) {
    stopAll();
    torchMemo.set(env, 'no');
    throw err('unsupported', 'This camera has no flashlight the browser can switch');
  }
  return { stream, track, stopAll };
}

/**
 * host.signal.flash {pattern_ms, mode?="auto"} → {used, stopped, timing:{max_late_ms, edges}}
 * (+ `fallback` "no_torch"|"permission_denied" when "auto" fell back to the screen — additive).
 * Resolves when the pattern has finished or was stopped.
 */
export async function flash(env, { pattern_ms, mode = 'auto', torch } = {}) {
  const pattern = checkPattern(pattern_ms);
  if (torch === true && (mode === undefined || mode === 'auto')) mode = 'auto'; // old callers
  if (!MODES.has(mode)) throw err('bad_params', 'mode must be "auto", "torch" or "screen"');
  if (current) current.stop();
  const doc = env.document;
  const perf = env.performance || globalThis.performance;
  const later = env.setTimeout ? env.setTimeout.bind(env) : setTimeout;
  const cancel = env.clearTimeout ? env.clearTimeout.bind(env) : clearTimeout;

  let stopped = false;
  let wake = null;
  const waiters = new Set();
  const ctl = { stop() { if (stopped) return; stopped = true; for (const w of [...waiters]) w(); } };
  current = ctl;
  const onHide = () => { if (!doc || doc.visibilityState === 'hidden') ctl.stop(); };
  const listen = (on) => {
    if (doc && doc.addEventListener) doc[on ? 'addEventListener' : 'removeEventListener']('visibilitychange', onHide);
    const w = env.window || env;
    if (w && w.addEventListener && w !== doc) w[on ? 'addEventListener' : 'removeEventListener']('pagehide', ctl.stop);
  };

  let torchDev = null;
  let fallback;
  let el = null;
  listen(true);
  try {
    // Only where a torch may exist (never opens a desktop's webcam): host.info.signal.torch != "no".
    if (mode !== 'screen' && detectSignal(env).torch !== 'no') {
      try { torchDev = await openTorch(env); } catch (e) {
        if (mode === 'torch') throw e;
        fallback = e.code === 'permission_denied' ? 'permission_denied' : 'no_torch';
      }
    } else if (mode === 'torch') throw err('unsupported', 'This device has no flashlight the browser can switch');
    else if (mode === 'auto') fallback = 'no_torch';
    if (stopped) return { used: torchDev ? 'torch' : 'screen', stopped: true };
    if (!torchDev && !detectFlash(env).available) throw err('unsupported', 'No screen to flash');
    if (!torchDev) {
      el = doc.createElement('div');
      // The full-screen light. Night theme: red only. Tap, click or Escape stops it.
      const lit = doc.documentElement && doc.documentElement.dataset && doc.documentElement.dataset.theme === 'night' ? '#f00' : '#fff';
      el.style.cssText = 'position:fixed;top:0;right:0;bottom:0;left:0;z-index:2147483647;background:#000;cursor:pointer';
      el.setAttribute('role', 'button');
      el.setAttribute('aria-label', 'Flashing Morse. Tap to stop');
      el.tabIndex = 0;
      el.dataset.lit = lit;
      const hint = doc.createElement('div');
      hint.textContent = 'Tap to stop';
      hint.style.cssText = 'position:absolute;left:0;right:0;bottom:24px;text-align:center;font:16px system-ui,sans-serif;color:#777;pointer-events:none';
      el.appendChild(hint);
      el.onclick = () => ctl.stop();
      el.onkeydown = (e) => { if (e.key === 'Escape' || e.key === ' ' || e.key === 'Enter') ctl.stop(); };
      doc.body.appendChild(el);
      try { el.focus({ preventScroll: true }); } catch { /* old */ }
    }
    if (env.navigator && env.navigator.wakeLock && typeof env.navigator.wakeLock.request === 'function') {
      try { wake = await env.navigator.wakeLock.request('screen'); } catch { wake = null; }
    }
    const setOn = (on) => {
      if (torchDev) torchDev.track.applyConstraints({ advanced: [{ torch: on }] }).catch(() => {});
      else el.style.background = on ? el.dataset.lit : '#000';
    };
    // Edges against deadlines from t0: a late timer shortens the next element instead of
    // stretching the rest of the message (a camera reader keeps its speed lock).
    const t0 = perf.now();
    let due = 0, maxLate = 0, edges = 0;
    for (let i = 0; i < pattern.length && !stopped; i++) {
      const late = perf.now() - (t0 + due);
      if (late > maxLate) maxLate = late;
      setOn(i % 2 === 0);
      edges++;
      due += pattern[i];
      await new Promise((r) => {
        const t = later(() => { waiters.delete(done); r(); }, Math.max(0, t0 + due - perf.now()));
        const done = () => { cancel(t); r(); };
        waiters.add(done);
      });
    }
    const end = perf.now() - (t0 + due);
    if (!stopped && end > maxLate) maxLate = end;
    return { used: torchDev ? 'torch' : 'screen', stopped, timing: { max_late_ms: Math.round(maxLate), edges }, ...(fallback && !torchDev ? { fallback } : {}) };
  } finally {
    // the light always goes off
    if (torchDev) {
      try { torchDev.track.applyConstraints({ advanced: [{ torch: false }] }).catch(() => {}); } catch { /* gone */ }
      torchDev.stopAll();
    }
    if (el) el.remove();
    if (wake) { try { wake.release(); } catch { /* released */ } }
    listen(false);
    if (current === ctl) current = null;
  }
}

export function detectTone(env = globalThis) {
  if (!(env.AudioContext || env.webkitAudioContext)) return { available: false, reason: 'This browser has no Web Audio support' };
  return { available: true };
}

let toneCtx = null;

/** Plays on/off tone pattern; resolves when done or stopped. Uses short ramps so keying is click-free. */
export async function tone(env, { pattern_ms, freq_hz = 700 } = {}) {
  const pattern = checkPattern(pattern_ms);
  const f = Number(freq_hz);
  if (!(f >= 50 && f <= 8000)) throw err('bad_params', 'freq_hz must be 50..8000');
  const AC = env.AudioContext || env.webkitAudioContext;
  if (!AC) throw err('unsupported', 'This browser has no Web Audio support');
  if (current) current.stop();
  if (!toneCtx || toneCtx.state === 'closed') toneCtx = new AC();
  const ac = toneCtx;
  if (ac.state === 'suspended') await ac.resume().catch(() => {});
  const osc = ac.createOscillator();
  const gain = ac.createGain();
  osc.frequency.value = f;
  gain.gain.value = 0;
  osc.connect(gain).connect(ac.destination);
  const ramp = 0.005;
  let t = ac.currentTime + 0.05;
  const t0 = t;
  for (let i = 0; i < pattern.length; i++) {
    const d = pattern[i] / 1000;
    if (i % 2 === 0 && d > 0) {
      gain.gain.setValueAtTime(0, t);
      gain.gain.linearRampToValueAtTime(0.5, t + Math.min(ramp, d / 2));
      gain.gain.setValueAtTime(0.5, t + Math.max(d - ramp, d / 2));
      gain.gain.linearRampToValueAtTime(0, t + d);
    }
    t += d;
  }
  let stopped = false;
  let finish;
  const ended = new Promise((r) => { finish = r; });
  const ctl = { stop() { if (stopped) return; stopped = true; try { gain.gain.cancelScheduledValues(ac.currentTime); gain.gain.setValueAtTime(0, ac.currentTime); osc.stop(); } catch { /* not started */ } finish(); } };
  current = ctl;
  osc.start(t0);
  osc.stop(t + 0.02);
  osc.onended = () => finish();
  const timer = setTimeout(() => finish(), (t - ac.currentTime) * 1000 + 500);
  try { await ended; } finally {
    clearTimeout(timer);
    try { osc.disconnect(); gain.disconnect(); } catch {}
    if (current === ctl) current = null;
  }
  return { stopped };
}

// ---------------------------------------------------------------------------------------------
// Wake lock and notifications

export function detectWakeLock(env = globalThis) {
  const nav = env.navigator || {};
  if (!nav.wakeLock) {
    if (!isSecure(env)) return { available: false, reason: `Keeping the screen on needs a secure page: ${SECURE_HINT}` };
    return { available: false, reason: 'This browser cannot keep the screen on (Screen Wake Lock API missing)' };
  }
  return { available: true };
}

/**
 * Screen wake lock with independent holders: the user's host.keep_awake switch ('user') and every
 * open Morse capture session ('capture:<id>') — the lock is held while any holder wants it, and
 * re-acquired when the page becomes visible again (browsers drop it on hide).
 */
export class KeepAwake {
  constructor(env = globalThis) {
    this.env = env;
    this.holders = new Set();
    this.lock = null;
    this._onVis = () => { if (this.enabled && env.document && env.document.visibilityState === 'visible' && !this.lock) this._acquire().catch(() => {}); };
    this._listening = false;
  }
  get enabled() { return this.holders.size > 0; }
  async _acquire() {
    try {
      this.lock = await this.env.navigator.wakeLock.request('screen');
      this.lock.addEventListener('release', () => { this.lock = null; });
    } catch (e) { this.lock = null; throw e; }
  }
  /** Adds/removes one holder. Throws `unsupported` / `denied` like host.keep_awake. */
  async want(holder, on) {
    const d = detectWakeLock(this.env);
    if (!d.available) throw err('unsupported', d.reason);
    if (on) this.holders.add(holder); else this.holders.delete(holder);
    const doc = this.env.document;
    if (this.enabled) {
      if (doc && !this._listening) { doc.addEventListener('visibilitychange', this._onVis); this._listening = true; }
      if (!this.lock) await this._acquire().catch((e) => { throw err('denied', `screen wake lock refused: ${(e && e.message) || e}`); });
    } else {
      if (doc && this._listening) { doc.removeEventListener('visibilitychange', this._onVis); this._listening = false; }
      if (this.lock) { const l = this.lock; this.lock = null; await l.release().catch(() => {}); }
    }
    return {};
  }
  /** host.keep_awake {enabled} */
  set(enabled) { return this.want('user', !!enabled); }
}

/** Camera / microphone capture (getUserMedia) for the Morse reader and the audio endpoint. */
export function detectCapture(env = globalThis, what = 'camera') {
  const nav = env.navigator || {};
  const name = what === 'camera' ? 'The camera' : 'The microphone';
  if (!isSecure(env)) {
    return { available: false, reason: `${name} needs a secure page: HTTPS, localhost, or the single-file version opened from disk. A plain http:// hub address (http://<ip>:8437/) is not secure — download the single-file copy from the hub and open it, or use the app` };
  }
  if (!nav.mediaDevices || typeof nav.mediaDevices.getUserMedia !== 'function') {
    return { available: false, reason: `${name} is not available in this browser (no getUserMedia)` };
  }
  return { available: true };
}

export function detectNotify(env = globalThis) {
  if (typeof env.Notification !== 'function') return { available: false, reason: 'This browser has no notification support' };
  if (!isSecure(env)) return { available: false, reason: `Notifications need a secure page: ${SECURE_HINT}` };
  if (env.Notification.permission === 'denied') return { available: false, reason: 'Notifications are blocked for this page in the browser settings' };
  return { available: true };
}

export async function notify(env, { title, body } = {}) {
  const d = detectNotify(env);
  if (!d.available) throw err('unsupported', d.reason);
  const N = env.Notification;
  let perm = N.permission;
  if (perm === 'default') perm = await N.requestPermission();
  if (perm !== 'granted') throw err('denied', 'notification permission not granted');
  const opts = { body: String(body || ''), tag: undefined };
  // Prefer the service worker (required on Android Chrome); fall back to a page notification.
  const sw = env.navigator && env.navigator.serviceWorker;
  if (sw && sw.controller) {
    const reg = await sw.ready.catch(() => null);
    if (reg && reg.showNotification) { await reg.showNotification(String(title || 'EmberMesh'), opts); return {}; }
  }
  try { new N(String(title || 'EmberMesh'), opts); } catch (e) { throw err('unsupported', `notifications unavailable here: ${(e && e.message) || e}`); }
  return {};
}

// --------------------------------------------------------------------------------- haptics

const HAPTIC = { tap: [10], success: [12, 60, 12], warning: [30, 80, 30], error: [60, 50, 60, 50, 60] };

export function detectHaptic(env = globalThis) {
  const nav = env.navigator || {};
  if (typeof nav.vibrate !== 'function') return { available: false, reason: 'This browser cannot vibrate (no Vibration API: iOS, desktop browsers)' };
  return { available: true };
}

/** host.haptic {kind:"tap"|"success"|"warning"|"error"} (RPC_API §8). */
export async function haptic(env, { kind = 'tap' } = {}) {
  const d = detectHaptic(env);
  if (!d.available) throw err('unsupported', d.reason);
  const p = HAPTIC[kind];
  if (!p) throw err('bad_params', `kind must be one of ${Object.keys(HAPTIC).join(', ')}`);
  // Browsers only vibrate after the user has interacted with the page (sticky activation); before
  // that a call is refused with a console warning, so do not even try.
  const ua = env.navigator.userActivation;
  if (ua && ua.hasBeenActive === false) return { vibrated: false };
  env.navigator.vibrate(p);
  return {};
}
