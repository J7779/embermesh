// Mock Morse decoder for the morse.rx.* contract (docs/RPC_API.md §6), so the UI and its tests
// work before the core's decoder lands. Simple but honest: adaptive threshold, adaptive dot
// length, Goertzel tone detection for audio. Not a reference implementation.
import { MORSE } from '../lib/morse.js';
import { b64ToPcm } from '../lib/capture.js';

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
  status() {
    if (this.lastT !== null) this._idle(this.lastT);
    const s = {
      transcript: this.transcript, chars: this.chars, wpm: this.marks >= 3 ? Math.round(1200 / this.unit) : 0,
      on: this.on, level: Math.round(this.level * 1000) / 1000, threshold: this.on ? 0.4 : 0.6,
      quality: this.marks ? Math.round((this.good / this.marks) * 100) / 100 : 0, pending: this.pending,
    };
    if (this.source === 'audio') { s.tone_hz = Math.round(this.toneHz) || null; s.snr_db = this.snr; }
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
