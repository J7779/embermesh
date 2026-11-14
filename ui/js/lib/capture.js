// Pure capture logic for the Morse reader (docs/RPC_API.md §6): brightness of a watched region,
// auto-finding the blinking light, batching samples, resampling microphone audio. No DOM — the
// media side lives in capture_media.js. Tested in ui/tests/capture.test.js.

/** Clamp a region {x,y,w,h} (0..1 fractions of the frame) into the frame, min size 2 %. */
export function clampRoi(r) {
  const c = v => Math.max(0, Math.min(1, Number.isFinite(v) ? v : 0));
  let w = Math.max(0.02, Math.min(1, Number.isFinite(r && r.w) ? r.w : 0.2));
  let h = Math.max(0.02, Math.min(1, Number.isFinite(r && r.h) ? r.h : 0.2));
  const x = Math.min(c(r && r.x), 1 - w), y = Math.min(c(r && r.y), 1 - h);
  return { x, y, w, h };
}

export const DEFAULT_ROI = { x: 0.4, y: 0.4, w: 0.2, h: 0.2 }; // the centre fifth

/** A region of size w×h centred on (cx, cy), clamped into the frame. */
export function roiAt(cx, cy, w = 0.2, h = 0.2) {
  return clampRoi({ x: cx - w / 2, y: cy - h / 2, w, h });
}

/**
 * Mean luma (0..1) of `roi` in an RGBA frame (Uint8ClampedArray, width × height). Samples every
 * `step`-th pixel so it stays cheap at camera resolution. Rec. 601 weights.
 */
export function roiBrightness(data, width, height, roi, step = 2) {
  const r = clampRoi(roi);
  const x0 = Math.floor(r.x * width), y0 = Math.floor(r.y * height);
  const x1 = Math.max(x0 + 1, Math.ceil((r.x + r.w) * width)), y1 = Math.max(y0 + 1, Math.ceil((r.y + r.h) * height));
  let sum = 0, n = 0;
  for (let y = y0; y < y1 && y < height; y += step) {
    let i = (y * width + x0) * 4;
    for (let x = x0; x < x1 && x < width; x += step, i += 4 * step) {
      sum += 0.299 * data[i] + 0.587 * data[i + 1] + 0.114 * data[i + 2];
      n++;
    }
  }
  return n ? sum / n / 255 : 0;
}

/**
 * Finds the blinking light: keeps a running mean and variance of every cell of a gx × gy grid and
 * reports the cell that varies most (a steady bright lamp has no variance; a light keyed in Morse
 * has a lot). Follows the light as it moves (exponential forgetting).
 */
export class BlinkFinder {
  constructor(gx = 8, gy = 6, alpha = 0.05) {
    this.gx = gx; this.gy = gy; this.alpha = alpha;
    this.mean = new Float64Array(gx * gy);
    this.var = new Float64Array(gx * gy);
    this.frames = 0;
  }
  /** Cell luma means of a frame (Float64Array, row-major). */
  cells(data, width, height) {
    const out = new Float64Array(this.gx * this.gy);
    for (let cy = 0; cy < this.gy; cy++) for (let cx = 0; cx < this.gx; cx++) {
      out[cy * this.gx + cx] = roiBrightness(data, width, height, { x: cx / this.gx, y: cy / this.gy, w: 1 / this.gx, h: 1 / this.gy }, 4);
    }
    return out;
  }
  push(data, width, height) { this.pushCells(this.cells(data, width, height)); }
  pushCells(c) {
    const a = this.frames < 20 ? 1 / (this.frames + 1) : this.alpha;
    for (let i = 0; i < c.length; i++) {
      const d = c[i] - this.mean[i];
      this.mean[i] += a * d;
      this.var[i] = (1 - a) * (this.var[i] + a * d * d);
    }
    this.frames++;
  }
  /** {roi, score (std-dev of the best cell, 0..0.5), cell} or null before enough frames. */
  best() {
    if (this.frames < 8) return null;
    let bi = 0;
    for (let i = 1; i < this.var.length; i++) if (this.var[i] > this.var[bi]) bi = i;
    const cx = bi % this.gx, cy = Math.floor(bi / this.gx);
    return { cell: bi, score: Math.sqrt(this.var[bi]), roi: clampRoi({ x: cx / this.gx, y: cy / this.gy, w: 1 / this.gx, h: 1 / this.gy }) };
  }
}

/** Collects (time, level) samples and hands them over in batches for morse.rx.feed. */
export class SampleBatcher {
  constructor(maxSamples = 30, maxMs = 250) { this.max = maxSamples; this.maxMs = maxMs; this.times = []; this.levels = []; }
  add(t, level) {
    if (this.times.length && t <= this.times[this.times.length - 1]) return null; // duplicate frame
    this.times.push(Math.round(t * 10) / 10);
    this.levels.push(Math.round(Math.max(0, Math.min(1, level)) * 10000) / 10000);
    if (this.times.length >= this.max || this.times[this.times.length - 1] - this.times[0] >= this.maxMs) return this.take();
    return null;
  }
  take() {
    if (!this.times.length) return null;
    const out = { times_ms: this.times, levels: this.levels };
    this.times = []; this.levels = [];
    return out;
  }
}

/**
 * Streaming resampler: Float32 audio at `from` Hz → Int16 at `to` Hz. Averages the input over
 * each output interval when downsampling (a cheap anti-alias filter), interpolates linearly when
 * upsampling. Keeps fractional position across calls.
 */
export class Resampler {
  constructor(from, to) {
    if (!(from > 0) || !(to > 0)) throw new Error('bad sample rates');
    this.from = from; this.to = to; this.ratio = from / to; this.pos = 0; this.prev = 0;
    this.accum = 0; this.count = 0;
  }
  push(input) {
    const out = [];
    const r = this.ratio;
    if (r >= 1) {
      // box filter: each output sample = mean of the ~r input samples it covers
      for (let i = 0; i < input.length; i++) {
        this.accum += input[i]; this.count++;
        this.pos += 1;
        if (this.pos >= r) { this.pos -= r; out.push(this.accum / this.count); this.accum = 0; this.count = 0; }
      }
    } else {
      for (let i = 0; i < input.length; i++) {
        const cur = input[i];
        while (this.pos < 1) { out.push(this.prev + (cur - this.prev) * this.pos); this.pos += r; }
        this.pos -= 1;
        this.prev = cur;
      }
    }
    const pcm = new Int16Array(out.length);
    for (let i = 0; i < out.length; i++) pcm[i] = Math.max(-32768, Math.min(32767, Math.round(out[i] * 32767)));
    return pcm;
  }
}

/** Int16 PCM → base64 of little-endian bytes (pcm_b64 for morse.rx.feed). */
export function pcmToB64(pcm) {
  const bytes = new Uint8Array(pcm.length * 2);
  for (let i = 0; i < pcm.length; i++) { const v = pcm[i] & 0xffff; bytes[2 * i] = v & 0xff; bytes[2 * i + 1] = v >> 8; }
  let s = '';
  for (let i = 0; i < bytes.length; i += 0x8000) s += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
  return btoa(s);
}

/** base64 little-endian 16-bit PCM → Int16Array. */
export function b64ToPcm(b64) {
  const s = atob(String(b64 || ''));
  const out = new Int16Array(s.length >> 1);
  for (let i = 0; i < out.length; i++) { const v = s.charCodeAt(2 * i) | (s.charCodeAt(2 * i + 1) << 8); out[i] = v >= 0x8000 ? v - 0x10000 : v; }
  return out;
}

/** RMS level of Float32 audio in dBFS (−120 floor). */
export function rmsDb(buf) {
  let s = 0;
  for (let i = 0; i < buf.length; i++) s += buf[i] * buf[i];
  const rms = Math.sqrt(s / Math.max(1, buf.length));
  return rms > 1e-6 ? Math.max(-120, 20 * Math.log10(rms)) : -120;
}

/** Light-level advice from a short history of brightness samples (0..1). Pure. */
export function lightAdvice(levels) {
  if (!levels.length) return null;
  let mn = 1, mx = 0, sum = 0;
  for (const v of levels) { if (v < mn) mn = v; if (v > mx) mx = v; sum += v; }
  const mean = sum / levels.length;
  if (mx < 0.04) return 'too_dark';
  if (mx - mn < 0.03) return mean > 0.92 ? 'saturated' : 'no_blink';
  return null;
}

/**
 * Why live capture is unavailable, as a helpful sentence (or null when getUserMedia can be tried).
 * env: {mediaDevices: bool, secureContext: bool, protocol, hostname, singleFile}
 */
export function captureUnavailableReason(env, kind = 'camera') {
  if (env.mediaDevices) return null;
  if (env.secureContext === false) {
    return 'The ' + kind + ' needs a secure page: open the app over HTTPS, as http://localhost, or use the single-file version (embermesh.html) opened from the device. A hub page served over plain http:// on the local network cannot use the ' + kind + '.';
  }
  return 'This browser or app view does not offer ' + kind + ' access. Try a current Chrome, Firefox or Safari, or the EmberMesh app for your device.';
}
