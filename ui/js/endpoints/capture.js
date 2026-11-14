// Helpers for Morse receive capture (RPC_API §6) shared by the WASM and remote backends.
//
//  CaptureWake   keeps the screen awake while a morse.rx session is open (the camera / microphone
//                capture runs in this page; a sleeping screen stops it). Released on morse.rx.stop,
//                on errors that mean the session is gone, and after 65 s without a feed (the core
//                closes idle sessions after 60 s).
//  FeedBatcher   carries morse.rx.feed over a slow transport (remote backend -> desktop daemon):
//                at most one feed per session in flight; feeds made meanwhile are merged into one
//                (light: per-sample times, audio: concatenated PCM); the merged backlog is capped
//                (oldest samples dropped) so a stalled socket never grows an unbounded queue.

const IDLE_MS = 65000;

export class CaptureWake {
  /** @param {() => {want(holder, on): Promise}} getKeepAwake */
  constructor(getKeepAwake, { idleMs = IDLE_MS, log = console } = {}) {
    this.get = getKeepAwake;
    this.idleMs = idleMs;
    this.log = log;
    this.sessions = new Map(); // session -> last activity (ms)
    this._timer = 0;
  }

  _hold(session, on) {
    let ka;
    try { ka = this.get(); } catch { return; }
    if (!ka) return;
    // best effort: no wake lock (insecure page, old browser) must never break capture
    Promise.resolve(ka.want(`capture:${session}`, on)).catch(() => {});
  }

  _sweep() {
    const now = Date.now();
    for (const [s, t] of this.sessions) if (now - t > this.idleMs) this._drop(s);
    clearTimeout(this._timer);
    this._timer = this.sessions.size ? setTimeout(() => this._sweep(), Math.min(15000, this.idleMs)) : 0;
  }

  _drop(session) {
    if (!this.sessions.delete(session)) return;
    this._hold(session, false);
  }

  /** Call after every morse.rx.* call with its params and either result or error. */
  observe(method, params, result, error) {
    if (!method || !method.startsWith('morse.rx.')) return;
    const p = params || {};
    if (method === 'morse.rx.start') {
      if (error || !result || result.session === undefined) return;
      this.sessions.set(String(result.session), Date.now());
      this._hold(String(result.session), true);
      this._sweep();
      return;
    }
    const s = p.session === undefined ? null : String(p.session);
    if (s === null || !this.sessions.has(s)) return;
    if (method === 'morse.rx.stop' || (error && (error.code === 'not_found' || error.code === 'bad_params'))) { this._drop(s); return; }
    this.sessions.set(s, Date.now());
  }

  close() {
    clearTimeout(this._timer);
    for (const s of [...this.sessions.keys()]) this._drop(s);
  }
}

// ---------------------------------------------------------------------------------- batching

const B64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

function b64decode(s) {
  if (typeof atob === 'function') {
    const bin = atob(s);
    const out = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
    return out;
  }
  return new Uint8Array(Buffer.from(s, 'base64'));
}

function b64encode(bytes) {
  if (typeof btoa === 'function') {
    let bin = '';
    for (let i = 0; i < bytes.length; i += 0x8000) bin += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
    return btoa(bin);
  }
  return Buffer.from(bytes.buffer, bytes.byteOffset, bytes.length).toString('base64');
}
export { b64encode, b64decode, B64 };

/** Normalises a light feed to parallel arrays of times and levels. */
function lightSamples(p) {
  const levels = Array.isArray(p.levels) ? p.levels : [];
  if (Array.isArray(p.times_ms)) return { times: p.times_ms.slice(0, levels.length), levels: levels.slice(0, p.times_ms.length) };
  const t0 = Number(p.t_ms) || 0, dt = Number(p.dt_ms) || 0;
  return { times: levels.map((_, i) => t0 + i * dt), levels: levels.slice() };
}

export class FeedBatcher {
  /**
   * @param {(params) => Promise<object>} send   performs one morse.rx.feed call
   * @param {object} [o]
   * @param {() => boolean} [o.congested]       true while the transport is backed up (e.g. ws.bufferedAmount)
   * @param {number} [o.maxLightSamples=4000]   backlog cap per session for light feeds
   * @param {number} [o.maxAudioBytes=320000]   backlog cap per session for audio (≈20 s at 8 kHz)
   */
  constructor(send, { congested = () => false, maxLightSamples = 4000, maxAudioBytes = 320000, pollMs = 20 } = {}) {
    this.send = send;
    this.congested = congested;
    this.maxLight = maxLightSamples;
    this.maxAudio = maxAudioBytes;
    this.pollMs = pollMs;
    this.sessions = new Map(); // session -> {inflight, pending:{kind, times, levels, chunks, bytes, waiters}}
    this.stats = { calls: 0, sent: 0, merged: 0, dropped_samples: 0, max_backlog: 0 };
  }

  feed(params) {
    const p = params || {};
    const key = String(p.session);
    let s = this.sessions.get(key);
    if (!s) { s = { inflight: false, pending: null, timer: 0 }; this.sessions.set(key, s); }
    this.stats.calls++;
    return new Promise((resolve, reject) => {
      const pend = s.pending || (s.pending = { session: p.session, kind: null, times: [], levels: [], chunks: [], bytes: 0, waiters: [], extra: {} });
      if (pend.waiters.length) this.stats.merged++;
      pend.waiters.push({ resolve, reject });
      if (typeof p.pcm_b64 === 'string') {
        pend.kind = 'audio';
        const b = b64decode(p.pcm_b64);
        pend.chunks.push(b); pend.bytes += b.length;
        while (pend.bytes > this.maxAudio && pend.chunks.length > 1) {
          const d = pend.chunks.shift(); pend.bytes -= d.length; this.stats.dropped_samples += d.length >> 1;
        }
        if (pend.bytes > this.maxAudio) { // one huge chunk: keep its newest part (whole samples)
          const c = pend.chunks[0]; const cut = (pend.bytes - this.maxAudio + 1) & ~1;
          pend.chunks[0] = c.subarray(cut); pend.bytes -= cut; this.stats.dropped_samples += cut >> 1;
        }
        this.stats.max_backlog = Math.max(this.stats.max_backlog, pend.bytes >> 1);
      } else {
        pend.kind = 'light';
        const { times, levels } = lightSamples(p);
        for (let i = 0; i < levels.length; i++) { pend.times.push(times[i]); pend.levels.push(levels[i]); }
        const over = pend.levels.length - this.maxLight;
        if (over > 0) { pend.times.splice(0, over); pend.levels.splice(0, over); this.stats.dropped_samples += over; }
        this.stats.max_backlog = Math.max(this.stats.max_backlog, pend.levels.length);
      }
      this._pump(key, s);
    });
  }

  _pump(key, s) {
    if (s.inflight || !s.pending) return;
    if (this.congested()) {
      clearTimeout(s.timer);
      s.timer = setTimeout(() => this._pump(key, s), this.pollMs);
      return;
    }
    const pend = s.pending;
    s.pending = null;
    s.inflight = true;
    let params;
    if (pend.kind === 'audio') {
      const all = new Uint8Array(pend.bytes);
      let o = 0;
      for (const c of pend.chunks) { all.set(c, o); o += c.length; }
      params = { session: pend.session, pcm_b64: b64encode(all) };
    } else {
      params = { session: pend.session, times_ms: pend.times, levels: pend.levels };
    }
    this.stats.sent++;
    Promise.resolve().then(() => this.send(params)).then(
      (r) => { for (const w of pend.waiters) w.resolve(r); },
      (e) => { for (const w of pend.waiters) w.reject(e); },
    ).finally(() => {
      s.inflight = false;
      if (s.pending) this._pump(key, s);
      else this.sessions.delete(key);
    });
  }
}
