// Small byte helpers shared by the browser endpoints and backends. No protocol knowledge here:
// all framing (KISS, AX.25, modems ...) lives in the core link drivers. These helpers only move
// bytes around: base64, chunking for small transport units, concatenation, CRC for pairing blobs.

const B64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
const B64_REV = (() => {
  const t = new Int16Array(128).fill(-1);
  for (let i = 0; i < 64; i++) t[B64.charCodeAt(i)] = i;
  t['-'.charCodeAt(0)] = 62; // base64url
  t['_'.charCodeAt(0)] = 63;
  return t;
})();

/** Standard base64 (with padding) of a Uint8Array / ArrayBuffer. Works on any size. */
export function bytesToB64(bytes, { url = false } = {}) {
  const b = toBytes(bytes);
  let out = '';
  const alpha = url ? B64.slice(0, 62) + '-_' : B64;
  let i = 0;
  const parts = [];
  for (; i + 2 < b.length; i += 3) {
    const n = (b[i] << 16) | (b[i + 1] << 8) | b[i + 2];
    out += alpha[n >> 18] + alpha[(n >> 12) & 63] + alpha[(n >> 6) & 63] + alpha[n & 63];
    if (out.length > 65536) { parts.push(out); out = ''; }
  }
  const rem = b.length - i;
  if (rem === 1) {
    const n = b[i] << 16;
    out += alpha[n >> 18] + alpha[(n >> 12) & 63] + (url ? '' : '==');
  } else if (rem === 2) {
    const n = (b[i] << 16) | (b[i + 1] << 8);
    out += alpha[n >> 18] + alpha[(n >> 12) & 63] + alpha[(n >> 6) & 63] + (url ? '' : '=');
  }
  parts.push(out);
  return parts.join('');
}

/** Decodes standard or url-safe base64; whitespace is ignored. Throws on invalid characters. */
export function b64ToBytes(str) {
  const s = String(str).replace(/[\s=]+/g, '');
  const out = new Uint8Array(Math.floor((s.length * 3) / 4));
  let o = 0, acc = 0, bits = 0;
  for (let i = 0; i < s.length; i++) {
    const c = s.charCodeAt(i);
    const v = c < 128 ? B64_REV[c] : -1;
    if (v < 0) throw new Error('invalid base64');
    acc = (acc << 6) | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out[o++] = (acc >> bits) & 0xff;
    }
  }
  if (s.length % 4 === 1) throw new Error('invalid base64 length');
  return o === out.length ? out : out.slice(0, o);
}

/** Normalises ArrayBuffer / typed array / DataView / number[] to a Uint8Array (no copy if possible). */
export function toBytes(x) {
  if (x instanceof Uint8Array) return x;
  if (x instanceof ArrayBuffer) return new Uint8Array(x);
  if (ArrayBuffer.isView(x)) return new Uint8Array(x.buffer, x.byteOffset, x.byteLength);
  if (Array.isArray(x)) return Uint8Array.from(x);
  if (typeof x === 'string') return new TextEncoder().encode(x);
  throw new TypeError('expected bytes');
}

export function concatBytes(...arrays) {
  let n = 0;
  for (const a of arrays) n += a.length;
  const out = new Uint8Array(n);
  let o = 0;
  for (const a of arrays) { out.set(a, o); o += a.length; }
  return out;
}

/** Splits a byte stream write into pieces of at most `max` bytes (BLE characteristics, etc.). */
export function chunkBytes(bytes, max) {
  const b = toBytes(bytes);
  if (!(max > 0)) throw new RangeError('chunk size must be > 0');
  const out = [];
  for (let i = 0; i < b.length; i += max) out.push(b.subarray(i, Math.min(b.length, i + max)));
  return out;
}

/** CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF). Used to catch typos in pasted pairing blobs. */
export function crc16(bytes) {
  let crc = 0xffff;
  for (const byte of toBytes(bytes)) {
    crc ^= byte << 8;
    for (let k = 0; k < 8; k++) crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff;
  }
  return crc;
}

/** Float32 [-1,1] PCM -> Int16 PCM (clamped). */
export function floatToInt16(f32, gain = 1) {
  const out = new Int16Array(f32.length);
  for (let i = 0; i < f32.length; i++) {
    let v = f32[i] * gain;
    v = v > 1 ? 1 : v < -1 ? -1 : v;
    out[i] = v < 0 ? v * 0x8000 : v * 0x7fff;
  }
  return out;
}

/** Int16 PCM -> Float32 [-1,1]. */
export function int16ToFloat(i16, gain = 1) {
  const out = new Float32Array(i16.length);
  for (let i = 0; i < i16.length; i++) out[i] = (i16[i] / 0x8000) * gain;
  return out;
}

/**
 * A serial write queue: guarantees one write in flight at a time (GATT and Web Serial writers
 * reject or interleave concurrent writes) and caps buffered bytes so a dead device can't eat RAM.
 */
export class WriteQueue {
  constructor(writeFn, { maxBytes = 256 * 1024 } = {}) {
    this._write = writeFn;
    this._q = [];
    this._bytes = 0;
    this._busy = false;
    this.maxBytes = maxBytes;
    this.dropped = 0;
    this.onError = null;
  }
  get pending() { return this._bytes; }
  push(bytes) {
    const b = toBytes(bytes);
    if (this._bytes + b.length > this.maxBytes) { this.dropped++; return false; }
    this._q.push(b);
    this._bytes += b.length;
    this._pump();
    return true;
  }
  clear() { this._q.length = 0; this._bytes = 0; }
  async _pump() {
    if (this._busy) return;
    this._busy = true;
    try {
      while (this._q.length) {
        const b = this._q.shift();
        this._bytes -= b.length;
        try {
          await this._write(b);
        } catch (e) {
          this.clear();
          if (this.onError) this.onError(e);
          break;
        }
      }
    } finally {
      this._busy = false;
    }
  }
}

/** Exponential backoff helper for reconnecting endpoints. */
export class Backoff {
  constructor(min = 1000, max = 30000) { this.min = min; this.max = max; this.cur = min; }
  next() { const v = this.cur; this.cur = Math.min(this.max, this.cur * 2); return v * (0.8 + Math.random() * 0.4); }
  reset() { this.cur = this.min; }
}
