// Simulated native camera for the mock backend (?nativecam=1): what a host with
// capture_camera_luma feeds the core (docs/RPC_API.md §12) — 48×36 max-luma grids at 30 frames a
// second of a dim room with a small keyed light that drifts like a hand-held phone, sends a message,
// goes dark for a few seconds (the tracker reports "lost") and starts again. With lights: 2 a
// second light keys elsewhere, so the reader shows "tap to choose".
import { toMorse, morsePattern } from '../lib/morse.js';
import { bytesToB64 } from '../lib/capture.js';

const W = 48, H = 36, FPS = 30;
export const SIM_TEXT = 'CQ CQ DE N0CALL K';

export class NativeCameraSim {
  constructor({ feed, after, speed = 1, lights = 1, text = SIM_TEXT, wpm = 10 }) {
    this.feed = feed; this.after = after; this.speed = speed; this.lights = lights;
    this.pattern = morsePattern(toMorse(text), wpm);
    this.cycle = this.pattern.reduce((a, b) => a + b, 0) + 5000; // message, then 5 s dark
    this.second = morsePattern(toMorse('EEEE TTTT'), 8);
    this.secondCycle = this.second.reduce((a, b) => a + b, 0) + 1500;
    this.t = 1000; this.running = false;
  }
  start() { this.running = true; this.t0 = 2500; this._tick(); }
  stop() { this.running = false; }
  /** Grid of one frame at simulated time t (ms). Pure apart from `this`. */
  frame(t) {
    const g = new Uint8Array(W * H);
    for (let i = 0; i < g.length; i++) g[i] = 26 + ((i * 7) % 9); // dim room, a little texture
    for (let x = 30; x < 44; x++) for (let y = 4; y < 10; y++) g[y * W + x] = 140; // a steady lamp (ignored)
    const tt = t - this.t0;
    const on = tt > 0 && keyed(this.pattern, tt % this.cycle);
    // hand-held drift: a slow ellipse plus a little shake
    const cx = 18 + 6 * Math.sin(t / 2600) + Math.sin(t / 170) * 0.6, cy = 20 + 4 * Math.cos(t / 3100) + Math.cos(t / 230) * 0.6;
    if (on) light(g, cx, cy);
    if (this.lights > 1 && keyed(this.second, t % this.secondCycle)) light(g, 38, 28);
    return g;
  }
  _tick() {
    if (!this.running) return;
    const frames = [];
    for (let k = 0; k < 3; k++) { this.t += 1000 / FPS; frames.push({ t_ms: Math.round(this.t * 10) / 10, w: W, h: H, max_b64: bytesToB64(this.frame(this.t)) }); }
    this.feed(frames);
    this.after(100, () => this._tick());
  }
}

function keyed(pattern, t) {
  let a = 0;
  for (let i = 0; i < pattern.length; i++) { if (t < a + pattern[i]) return i % 2 === 0; a += pattern[i]; }
  return false;
}
function light(g, cx, cy) {
  const x = Math.round(cx), y = Math.round(cy);
  for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
    const xx = x + dx, yy = y + dy;
    if (xx >= 0 && yy >= 0 && xx < W && yy < H) g[yy * W + xx] = Math.max(g[yy * W + xx], dx || dy ? 110 : 250);
  }
}
