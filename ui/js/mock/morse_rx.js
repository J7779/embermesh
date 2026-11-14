// Mock Morse decoder for the morse.rx.* contract (docs/RPC_API.md §6), so the UI and its tests
// work before the core's decoder lands. Simple but honest: adaptive threshold, adaptive dot
// length, Goertzel tone detection for audio. Not a reference implementation.
import { MORSE } from '../lib/morse.js';
import { b64ToPcm, b64ToBytes } from '../lib/capture.js';

const DECODE = Object.fromEntries(Object.entries(MORSE).map(([k, v]) => [v, k]));
DECODE['...---...'] = '<SOS>';
DECODE['.-.-.'] = '<AR>';

export class MockMorseRx {
  constructor({ source = 'light', sample_rate = 8000, wpm_hint = 0, tone_hz = 0 } = {}) {
    this.source = source;
    this.rate = sample_rate;
    this.unit = wpm_hint > 0 ? 1200 / wpm_hint : 100;
    this.toneHz = tone_hz || 0;
    this.toneFixed = !!tone_hz;
    this.reset();
    this.lastFeed = Date.now();
  }
  reset() {
    this.transcript = ''; this.chars = 0; this.pending = ''; this.on = false; this.level = 0;
    this.hi = 0; this.lo = 1; this.lastT = null; this.edgeT = null; this.marks = 0; this.good = 0;
    this.snr = null; this.audioT = 0; this.carry = new Float32Array(0); this.noise = 1e-6; this.peak = 1e-6;
    this.wordGap = false; this.newText = ''; this.armed = false; this.early = []; this.durs = [];
  }
  /** Feed one brightness / envelope sample at time t (ms). */
  _sample(t, v) {
    if (this.lastT !== null && t <= this.lastT) return;
    const dt = this.lastT === null ? 0 : t - this.lastT;
    this.lastT = t;
    // slowly forgetting min/max tracker → threshold in the middle
    const decay = Math.min(1, dt / 4000);
    this.hi = Math.max(v, this.hi - (this.hi - v) * decay);
    this.lo = Math.min(v, this.lo + (v - this.lo) * decay);
    const span = this.hi - this.lo;
    this.level = span > 0 ? (v - this.lo) / span : 0;
    if (span < 0.06) { this._idle(t); if (!this.armed) { this.early.push([t, v]); if (this.early.length > 400) this.early.shift(); } return; }
    if (!this.armed) {
      // first real contrast: replay the samples seen before it so the first mark is not lost
      this.armed = true;
      const early = this.early; this.early = [];
      for (const [et, ev] of early) { const on = ev > this.lo + span * 0.6; if (on !== this.on) { if (this.edgeT !== null) (this.on ? this._mark(et - this.edgeT) : this._space(et - this.edgeT)); this.on = on; this.edgeT = et; } }
    }
    const thr = this.lo + span * (this.on ? 0.4 : 0.6); // hysteresis
    const on = v > thr;
    if (on !== this.on) {
      if (this.edgeT !== null) {
        const d = t - this.edgeT;
        if (this.on) this._mark(d); else this._space(d);
      }
      this.on = on;
      this.edgeT = t;
    } else this._idle(t);
  }
  _mark(d) {
    if (d < this.unit * 0.25) return; // glitch
    // a mark between 1.5 and 2.5 units is ambiguous: compare with the char's other marks later
    const dash = d > this.unit * 2;
    this.pending += dash ? '-' : '.';
    this.durs.push(d);
    this.unit = 0.8 * this.unit + 0.2 * (dash ? d / 3 : d);
    this.unit = Math.max(30, Math.min(500, this.unit));
    this.marks++;
    const err = dash ? Math.abs(d / 3 - this.unit) : Math.abs(d - this.unit);
    if (err < this.unit * 0.5) this.good++;
  }
  _space(d) {
    if (d > this.unit * 2) this._endChar(d > this.unit * 5);
  }
  _idle(t) {
    if (!this.on && this.edgeT !== null && this.pending && t - this.edgeT > this.unit * 3.5) this._endChar(false);
    if (!this.on && this.edgeT !== null && !this.pending && !this.wordGap && this.transcript && t - this.edgeT > this.unit * 7) { this._emit(' '); this.wordGap = true; }
  }
  _endChar(word) {
    if (this.pending && this.durs.length === this.pending.length) {
      // re-classify the character's marks with the speed learnt meanwhile (fixes the first letters)
      const mn = Math.min(...this.durs), mx = Math.max(...this.durs);
      const u = mx > mn * 2 ? Math.min(this.unit, mn * 1.2) : this.unit;
      this.pending = this.durs.map(x => (mx > mn * 2 ? x > Math.sqrt(mn * mx) : x > u * 2) ? '-' : '.').join('');
    }
    this.durs = [];
    if (this.pending) { this._emit(DECODE[this.pending] || '□'); this.pending = ''; this.wordGap = false; }
    if (word && this.transcript && !this.transcript.endsWith(' ')) { this._emit(' '); this.wordGap = true; }
  }
  _emit(s) {
    this.transcript = (this.transcript + s).slice(-4000);
    if (s !== ' ') this.chars += [...s].length > 1 && s.startsWith('<') ? 1 : [...s].length;
    this.newText += s;
  }
  feedLight(p) {
    const lv = Array.isArray(p.levels) ? p.levels : [];
    if (Array.isArray(p.times_ms)) lv.forEach((v, i) => this._sample(Number(p.times_ms[i]), Number(v)));
    else { const t0 = Number(p.t_ms) || 0, dt = Number(p.dt_ms) || 33; lv.forEach((v, i) => this._sample(t0 + i * dt, Number(v))); }
    this.lastFeed = Date.now();
  }
  feedAudio(p) {
    const pcm = b64ToPcm(p.pcm_b64);
    const block = Math.max(8, Math.round(this.rate / 200)); // 5 ms
    const buf = new Float32Array(this.carry.length + pcm.length);
    buf.set(this.carry); for (let i = 0; i < pcm.length; i++) buf[this.carry.length + i] = pcm[i] / 32768;
    if (!this.toneFixed) {
      // tone search over the last ~0.2 s (feeds may be small)
      const keep = Math.round(this.rate / 5);
      const win = new Float32Array(Math.min(keep, (this.det ? this.det.length : 0) + buf.length));
      const prev = this.det || new Float32Array(0);
      const fromPrev = win.length - Math.min(buf.length, win.length);
      win.set(prev.subarray(prev.length - fromPrev), 0);
      win.set(buf.subarray(buf.length - (win.length - fromPrev)), fromPrev);
      this.det = win;
      if (win.length >= this.rate / 10 && (!this.toneHz || this.on === false)) this.toneHz = detectTone(win, this.rate) || this.toneHz;
    }
    let off = 0;
    for (; off + block <= buf.length; off += block) {
      const e = this.toneHz ? goertzel(buf, off, block, this.toneHz, this.rate) : 0;
      this.peak = Math.max(e, this.peak * 0.999);
      this.noise = e < this.peak * 0.3 ? 0.98 * this.noise + 0.02 * e : this.noise;
      this.audioT += (block / this.rate) * 1000;
      this._sample(this.audioT, e);
    }
    this.carry = buf.slice(off);
    this.snr = this.peak > 0 && this.noise > 0 ? Math.round(10 * Math.log10(this.peak / Math.max(this.noise, 1e-9)) * 10) / 10 : null;
    this.lastFeed = Date.now();
  }
  /**
   * §12 frames: whole-frame max-luma grids. A small tracker finds the cell that keys on and off
   * with strong contrast, follows it (neighbouring cells), and feeds that cell's brightness to the
   * decoder above. States: searching → locked → lost (> 2 s without a keyed edge).
   */
  feedFrames(p) {
    const tr = this.tracker || (this.tracker = new LightTracker());
    for (const f of p.frames) {
      const t = Number(f.t_ms), w = Number(f.w), h = Number(f.h);
      const max = b64ToBytes(f.max_b64);
      if (!(w > 0 && h > 0 && w * h <= 4096 && max.length === w * h && Number.isFinite(t))) continue;
      const v = tr.push(t, w, h, max);
      if (v !== null) {
        if (tr.replay) { for (const [rt, rv] of tr.replay) this._sample(rt, rv); tr.replay = null; }
        this._sample(t, v);
      }
    }
    this.lastFeed = Date.now();
  }
  target() { return this.tracker ? this.tracker.view() : null; }
  pin(x, y) { (this.tracker || (this.tracker = new LightTracker())).pin(x, y); }
  status() {
    if (this.lastT !== null) this._idle(this.lastT);
    const s = {
      transcript: this.transcript, chars: this.chars, wpm: this.marks >= 3 ? Math.round(1200 / this.unit) : 0,
      on: this.on, level: Math.round(this.level * 1000) / 1000, threshold: this.on ? 0.4 : 0.6,
      quality: this.marks ? Math.round((this.good / this.marks) * 100) / 100 : 0, pending: this.pending,
    };
    if (this.source === 'audio') { s.tone_hz = Math.round(this.toneHz) || null; s.snr_db = this.snr; }
    if (this.tracker) s.target = this.tracker.view();
    return s;
  }
  takeNew() { const t = this.newText; this.newText = ''; return t; }
}

function goertzel(buf, off, n, f, rate) {
  const k = 2 * Math.cos(2 * Math.PI * f / rate);
  let s1 = 0, s2 = 0;
  for (let i = 0; i < n; i++) { const s = buf[off + i] + k * s1 - s2; s2 = s1; s1 = s; }
  return Math.sqrt(Math.max(0, s1 * s1 + s2 * s2 - k * s1 * s2)) / n;
}

function detectTone(buf, rate) {
  const n = buf.length;
  const off = 0;
  let best = 0, bestE = 0;
  for (let f = 300; f <= 1500; f += 20) { const e = goertzel(buf, off, n, f, rate); if (e > bestE) { bestE = e; best = f; } }
  return bestE > 0.01 ? best : 0;
}

/**
 * Mock of the core's light tracker (RPC_API §12) over max-luma grids. Per cell: a slowly
 * forgetting min/max, the edges (threshold crossings with strong contrast) of the last 4 s.
 * The best cell keys most; a lock follows it to neighbours as the light drifts.
 */
export class LightTracker {
  constructor() { this.w = 0; this.h = 0; this.cells = null; this.state = 'searching'; this.cur = -1; this.lastEdge = 0; this.pinned = null; this.hist = []; this.replay = null; this.t = 0; }
  _reset(w, h) { this.w = w; this.h = h; this.cells = Array.from({ length: w * h }, () => ({ lo: 255, hi: 0, on: false, edges: [] })); this.cur = -1; this.state = 'searching'; }
  pin(x, y) { this.pinned = x == null ? null : { x, y }; if (this.pinned) this.cur = -1; }
  /** One frame → the tracked brightness 0..1, or null while searching. */
  push(t, w, h, max) {
    if (w !== this.w || h !== this.h || !this.cells) this._reset(w, h);
    this.t = t;
    const dt = this.prevT ? Math.max(0, t - this.prevT) : 33;
    this.prevT = t;
    const decay = Math.min(1, dt / 3000);
    for (let i = 0; i < max.length; i++) {
      const c = this.cells[i], v = max[i];
      c.hi = Math.max(v, c.hi - (c.hi - v) * decay);
      c.lo = Math.min(v, c.lo + (v - c.lo) * decay);
      const span = c.hi - c.lo;
      if (span < 40) continue;
      const on = v > c.lo + span * (c.on ? 0.4 : 0.6);
      if (on !== c.on) { c.on = on; c.edges.push(t); if (c.edges.length > 64) c.edges.shift(); }
    }
    // keep the last 3 s of frames so the first characters are not lost when the lock comes
    this.hist.push([t, max]);
    while (this.hist.length && t - this.hist[0][0] > 3000) this.hist.shift();
    const score = i => { const c = this.cells[i]; return c.edges.filter(e => t - e < 4000).length * Math.min(1, (c.hi - c.lo) / 128); };
    const cand = [];
    for (let i = 0; i < this.cells.length; i++) { const s = score(i); if (s >= 3) cand.push([i, s]); }
    // candidates: local maxima only (a light spilling into neighbours is one light)
    // one light spills into neighbouring cells: keep local maxima within 3 cells (ties: lowest index)
    this.candidates = cand.filter(([i, s]) => !cand.some(([j, s2]) => j !== i && (s2 > s || (s2 === s && j < i)) && Math.abs((j % w) - (i % w)) <= 3 && Math.abs(Math.floor(j / w) - Math.floor(i / w)) <= 3));
    let best = -1;
    if (this.pinned) {
      let bd = Infinity;
      for (const [i] of this.candidates) { const dx = ((i % w) + 0.5) / w - this.pinned.x, dy = (Math.floor(i / w) + 0.5) / h - this.pinned.y, d = dx * dx + dy * dy; if (d < bd) { bd = d; best = i; } }
    } else {
      let bs = 0;
      for (const [i, s] of this.candidates) {
        // prefer staying on (or next to) the current light
        const near = this.cur >= 0 && Math.abs((i % w) - (this.cur % w)) <= 2 && Math.abs(Math.floor(i / w) - Math.floor(this.cur / w)) <= 2;
        const sc = s * (near ? 1.5 : 1);
        if (sc > bs) { bs = sc; best = i; }
      }
    }
    if (this.cur < 0 && best >= 0 && score(best) >= 4) {
      this.state = 'locked';
      this.cur = best;
      this.sig = { lo: 255, hi: 0, on: false };
      this.replay = this.hist.slice(0, -1).map(([ht, hm]) => [ht, this._value(best, hm)]);
      this.lastEdge = t;
    } else if (this.cur >= 0 && best >= 0 && this.pinned) this.cur = best;
    if (this.cur < 0) return null;
    // follow the light as the camera drifts: the brightest cell near the current one, when it is lit
    const cx = this.cur % w, cy = Math.floor(this.cur / w);
    let bi = this.cur, bv = max[this.cur];
    for (let dy = -2; dy <= 2; dy++) for (let dx = -2; dx <= 2; dx++) {
      const x = cx + dx, y = cy + dy;
      if (x < 0 || y < 0 || x >= w || y >= h) continue;
      const i = y * w + x;
      if (max[i] > bv) { bv = max[i]; bi = i; }
    }
    if (bi !== this.cur && bv > this.cells[bi].lo + 60) this.cur = bi;
    // a light that keyed somewhere else for a while (the old one is gone): move there
    if (this.state === 'lost' && best >= 0 && !this.pinned) this.cur = best;
    const v = this._value(this.cur, max) * 255;
    const sg = this.sig;
    sg.hi = Math.max(v, sg.hi - (sg.hi - v) * decay);
    sg.lo = Math.min(v, sg.lo + (v - sg.lo) * decay);
    if (sg.hi - sg.lo >= 40) { const on = v > sg.lo + (sg.hi - sg.lo) * (sg.on ? 0.4 : 0.6); if (on !== sg.on) { sg.on = on; this.lastEdge = t; } }
    this.state = t - this.lastEdge > 2200 ? 'lost' : 'locked';
    return v / 255;
  }
  _value(i, max) {
    const w = this.w, x = i % w, y = Math.floor(i / w);
    let v = 0;
    for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
      const xx = x + dx, yy = y + dy;
      if (xx >= 0 && yy >= 0 && xx < w && yy < this.h) v = Math.max(v, max[yy * w + xx]);
    }
    return v / 255;
  }
  view() {
    const n = (this.candidates || []).length;
    if (this.state === 'searching' || this.cur < 0) return { state: 'searching', confidence: 0, candidates: n };
    let edges = 0, span = 0;
    for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
      const x = (this.cur % this.w) + dx, y = Math.floor(this.cur / this.w) + dy;
      if (x < 0 || y < 0 || x >= this.w || y >= this.h) continue;
      const c = this.cells[y * this.w + x];
      edges = Math.max(edges, c.edges.filter(e => this.t - e < 4000).length); span = Math.max(span, c.hi - c.lo);
    }
    const c = { hi: span, lo: 0 };
    return {
      state: this.state, x: ((this.cur % this.w) + 0.5) / this.w, y: (Math.floor(this.cur / this.w) + 0.5) / this.h,
      size: Math.max(2 / this.w, 0.04), confidence: Math.round(Math.min(1, edges / 14) * Math.min(1, (c.hi - c.lo) / 160) * 100) / 100, candidates: n,
    };
  }
}
