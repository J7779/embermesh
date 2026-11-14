// RemoteBackend — controls a desktop daemon's node over its JSON-RPC WebSocket (RPC_API §5):
//   ws://<host>:8437/rpc?token=<admin token>   text frames
//   -> {id, method, params}     <- {id, ok, result?, error?}     <- {event, data}
// URL parameters: ?remote=1[&host=<host:port>][&token=<token>][&tls=1]
// Calls made while disconnected wait for the reconnect (up to `queueTimeoutMs`).
// Browser-local services (QR scan, file save/pick, notifications, keep-awake, flash/tone) are
// handled in this browser, since that is where the user, the camera and the screen are.
import * as device from '../endpoints/device.js';
import { CaptureWake, FeedBatcher } from '../endpoints/capture.js';

const CONGESTED_BYTES = 256 * 1024;

function rpcError(code, message) {
  return Object.assign(new Error(message || code), { code });
}

const LOCAL_HOST_METHODS = new Set([
  'host.qr.scan', 'host.file.save', 'host.file.pick', 'host.notify', 'host.keep_awake',
  'host.signal.flash', 'host.signal.tone', 'host.signal.stop',
]);

/** Builds the /rpc URL from options and the page location. */
export function remoteUrl({ host, token, tls } = {}, loc = globalThis.location) {
  let params = null;
  try { params = loc ? new URL(loc.href).searchParams : null; } catch {}
  const h = host || (params && params.get('host')) || (loc && /^https?:$/.test(loc.protocol) && loc.host) || 'localhost:8437';
  const t = token !== undefined ? token : (params && params.get('token')) || '';
  const secure = tls !== undefined ? tls : (params && params.get('tls') === '1') || (loc && loc.protocol === 'https:');
  const u = new URL(`${secure ? 'wss' : 'ws'}://${h}/rpc`);
  if (t) u.searchParams.set('token', t);
  return u.href;
}

export default class RemoteBackend {
  constructor(opts = {}) {
    this.kind = 'remote';
    this.env = opts.env || globalThis;
    this.url = opts.url || remoteUrl(opts, this.env.location);
    this.callTimeoutMs = opts.callTimeoutMs || 120000;
    this.queueTimeoutMs = opts.queueTimeoutMs || 30000;
    this.connected = false;
    this._ws = null;
    this._next = 1;
    this._pending = new Map();   // id -> {resolve, reject, timer, frame}
    this._queue = [];            // frames waiting for a connection
    this._listeners = new Map();
    this._backoff = 500;
    this._timer = 0;
    this._closed = false;
    this._firstOpen = null;
    this._keepAwake = null;
    this._wake = new CaptureWake(() => this._ka());
    // morse.rx.feed at camera frame rate: one in flight per session, the rest merged and capped
    this.feeds = new FeedBatcher((p) => this._remoteCall('morse.rx.feed', p), {
      congested: () => !this.connected || !this._ws || this._ws.bufferedAmount > CONGESTED_BYTES,
    });
  }

  _ka() {
    if (!this._keepAwake) this._keepAwake = new device.KeepAwake(this.env);
    return this._keepAwake;
  }

  init() {
    if (!this._firstOpen) {
      this._firstOpen = new Promise((resolve, reject) => {
        this._resolveFirst = resolve;
        this._rejectFirst = reject;
      });
      this._connect();
    }
    return this._firstOpen;
  }

  _connect() {
    if (this._closed) return;
    let ws;
    try { ws = new this.env.WebSocket(this.url); } catch (e) {
      if (this._rejectFirst) { this._rejectFirst(rpcError('io_error', `cannot open ${this.url}: ${e.message || e}`)); this._rejectFirst = null; }
      return;
    }
    this._ws = ws;
    ws.onopen = () => {
      this.connected = true;
      this._backoff = 500;
      const q = this._queue.splice(0);
      for (const f of q) ws.send(f);
      this._emit('remote.state', { connected: true });
      if (this._resolveFirst) { this._resolveFirst(this); this._resolveFirst = this._rejectFirst = null; }
    };
    ws.onmessage = (ev) => this._onMessage(ev.data);
    ws.onerror = () => {};
    ws.onclose = (ev) => {
      if (this._ws !== ws) return;
      this._ws = null;
      const was = this.connected;
      this.connected = false;
      if (was) this._emit('remote.state', { connected: false });
      if (ev && (ev.code === 4401 || ev.code === 4403 || ev.code === 1008)) {
        const err = rpcError('denied', 'the daemon rejected the admin token');
        if (this._rejectFirst) { this._rejectFirst(err); this._rejectFirst = null; }
        this._failAll(err);
        return;
      }
      if (this._rejectFirst && !was) {
        // never connected: keep retrying, but let init() fail fast so the UI can show an error
        this._rejectFirst(rpcError('io_error', `cannot reach the EmberMesh daemon at ${this.url}`));
        this._rejectFirst = null;
        this._firstOpen = Promise.resolve(this); // later init() calls succeed once reconnected
      }
      this._schedule();
    };
  }

  _schedule() {
    if (this._closed) return;
    clearTimeout(this._timer);
    this._timer = setTimeout(() => this._connect(), this._backoff);
    this._backoff = Math.min(15000, this._backoff * 2);
  }

  _failAll(err) {
    for (const [id, p] of this._pending) { clearTimeout(p.timer); p.reject(err); this._pending.delete(id); }
    this._queue.length = 0;
  }

  _onMessage(text) {
    let m;
    try { m = JSON.parse(typeof text === 'string' ? text : new TextDecoder().decode(text)); } catch { return; }
    if (!m) return;
    if (typeof m.event === 'string') { this._emit(m.event, m.data || {}); return; }
    const p = this._pending.get(m.id);
    if (!p) return;
    this._pending.delete(m.id);
    clearTimeout(p.timer);
    if (m.ok) p.resolve(m.result === undefined ? {} : m.result);
    else { const e = m.error || {}; p.reject(rpcError(e.code || 'internal', e.message || 'call failed')); }
  }

  async call(method, params) {
    if (LOCAL_HOST_METHODS.has(method)) return this._local(method, params || {});
    if (method.startsWith('morse.rx.')) {
      const work = method === 'morse.rx.feed' ? this.feeds.feed(params || {}) : this._remoteCall(method, params);
      return work.then((r) => { this._wake.observe(method, params, r); return r; }, (e) => { this._wake.observe(method, params, null, e); throw e; });
    }
    if (method === 'host.info') {
      const info = await this._remoteCall(method, params);
      const local = ['qr_scan', 'file', 'notify', 'keep_awake', 'flash', 'tone', 'camera', 'microphone'];
      const f = new Set(info.features || []);
      const unavail = info.unavailable_features && typeof info.unavailable_features === 'object' ? { ...info.unavailable_features } : {};
      if (device.detectQr(this.env).available) f.add('qr_scan'); else f.delete('qr_scan');
      for (const x of local.slice(1)) {
        const d = { file: device.detectFile, notify: device.detectNotify, keep_awake: device.detectWakeLock, flash: device.detectFlash, tone: device.detectTone,
          camera: (e) => device.detectCapture(e, 'camera'), microphone: (e) => device.detectCapture(e, 'microphone') }[x](this.env);
        if (d.available) { f.add(x); if (unavail) delete unavail[x]; } else { f.delete(x); if (unavail) unavail[x] = d.reason; }
      }
      // capture runs in this browser: the daemon's own capture_* sources are not ours to use here
      f.delete('capture_camera_luma'); f.delete('capture_mic_pcm');
      // light signalling runs in this browser too (RPC_API §10): this device's torch, not the daemon's
      const signal = device.detectSignal(this.env);
      if (signal.torch !== 'no') f.add('torch'); else f.delete('torch');
      return { ...info, backend: 'remote', features: [...f], signal, ...(unavail ? { unavailable_features: unavail } : {}) };
    }
    return this._remoteCall(method, params);
  }

  _remoteCall(method, params) {
    if (this._closed) return Promise.reject(rpcError('io_error', 'backend closed'));
    const id = this._next++;
    const frame = JSON.stringify({ id, method, params: params || {} });
    return new Promise((resolve, reject) => {
      const entry = { resolve, reject, timer: 0 };
      this._pending.set(id, entry);
      const timeout = this.connected ? this.callTimeoutMs : this.queueTimeoutMs + this.callTimeoutMs;
      entry.timer = setTimeout(() => {
        this._pending.delete(id);
        reject(rpcError(this.connected ? 'busy' : 'io_error', this.connected ? `${method} timed out` : 'not connected to the daemon'));
      }, timeout);
      if (this.connected && this._ws) this._ws.send(frame);
      else {
        this._queue.push(frame);
        if (!this._ws && !this._timer) this._connect();
      }
    });
  }

  async _local(method, p) {
    const env = this.env;
    switch (method) {
      case 'host.qr.scan': return device.qrScan(env);
      case 'host.file.save': return device.fileSave(env, p);
      case 'host.file.pick': return device.filePick(env, p);
      case 'host.notify': return device.notify(env, p);
      case 'host.signal.flash': return device.flash(env, p);
      case 'host.signal.tone': return device.tone(env, p);
      case 'host.signal.stop': return device.signalStop();
      case 'host.keep_awake': return this._ka().set(p.enabled);
      default: throw rpcError('unsupported', method);
    }
  }

  on(event, handler) {
    if (!this._listeners.has(event)) this._listeners.set(event, new Set());
    this._listeners.get(event).add(handler);
    return () => { const s = this._listeners.get(event); if (s) s.delete(handler); };
  }

  _emit(event, data) {
    const run = (fn, ...a) => { try { fn(...a); } catch (e) { console.error(`[remote backend] "${event}" handler threw`, e); } };
    const s = this._listeners.get(event);
    if (s) for (const fn of [...s]) run(fn, data);
    const all = this._listeners.get('*');
    if (all) for (const fn of [...all]) run(fn, event, data);
  }

  close() {
    this._closed = true;
    this._wake.close();
    clearTimeout(this._timer);
    const ws = this._ws;
    this._ws = null;
    this.connected = false;
    if (ws) try { ws.close(1000); } catch {}
    this._failAll(rpcError('io_error', 'backend closed'));
  }
}
