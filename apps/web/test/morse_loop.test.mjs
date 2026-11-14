// Morse → Send, end to end without a camera: the exact pattern the Send tab builds for a typed text
// (ui/js/lib/morse.js buildSignal) is turned into the brightness samples a 30 fps camera would see —
// frame-time jitter, a one-frame exposure (one frame of lag), a little sensor noise — and fed to the real WASM
// core's morse.rx light decoder, which must read back the typed text. At the default speed, the
// torch maximum and the screen maximum (RPC_API §6 / §10). Deterministic (seeded).
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { ui, wasmModuleUrl, haveWasm, NO_WASM } from './helpers.mjs';

const M = await import(ui('lib/morse.js'));
const skip = haveWasm() ? false : NO_WASM;

function camera(pattern, seed) {
  let s = seed;
  const rnd = () => ((s = (s * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff);
  const lead = 1000;
  const on = [];
  let t = lead;
  pattern.forEach((d, i) => { if (i % 2 === 0) on.push([t, t + d]); t += d; });
  const end = t + 2000;
  const litFraction = (a, b) => on.reduce((acc, [x, y]) => acc + Math.max(0, Math.min(b, y) - Math.max(a, x)), 0) / (b - a);
  const times = [], levels = [];
  const frame = 1000 / 30;
  for (let k = 0; ; k++) {
    const ft = k * frame + (rnd() - 0.5) * 10;       // ±5 ms frame-time jitter
    if (ft > end) break;
    times.push(Math.round(ft));
    // the frame integrates the light over the exposure that ends at its timestamp (one frame long:
    // one frame of lag at every edge, partly lit frames at the edges)
    levels.push(0.15 + 0.7 * litFraction(ft - frame, ft) + (rnd() - 0.5) * 0.06);
  }
  return { times, levels };
}

test('typed text -> Send pattern -> 30 fps camera samples -> core morse.rx -> the same text, at default / torch max / screen max', { skip, timeout: 120000 }, async (t) => {
  const { default: WasmBackend } = await import(ui('backends/wasm.js'));
  const b = new WasmBackend({ moduleUrl: wasmModuleUrl(), storage: 'memory', hub: false, guides: false, restoreLinks: false, offline: false });
  await b.init();
  try {
    // the Send tab sends SOS as the prosign; TEST / HELLO start without any tune-up (cold start)
    const cases = [['SOS', '<SOS>'], ['CQ CQ TEST', 'CQ CQ TEST'], ['TEST', 'TEST'], ['HELLO WORLD 73', 'HELLO WORLD 73']];
    for (const [typed, expected] of cases) {
      for (const [label, wpm] of [['default', M.TORCH_DEFAULT_WPM], ['torch max', M.TORCH_MAX_WPM], ['screen max', M.SCREEN_MAX_WPM]]) {
        const sig = M.buildSignal(typed, wpm);
        assert.equal(sig.skipped.length, 0);
        for (const seed of [1, 2, 3]) {
          const { times, levels } = camera(sig.pattern, seed);
          const { session } = await b.call('morse.rx.start', { source: 'light' });
          for (let i = 0; i < times.length; i += 3) await b.call('morse.rx.feed', { session, times_ms: times.slice(i, i + 3), levels: levels.slice(i, i + 3) });
          const { transcript } = await b.call('morse.rx.stop', { session });
          assert.equal(transcript.trim(), expected, `"${typed}" at ${wpm} WPM (${label}), seed ${seed}`);
        }
        t.diagnostic(`"${typed}" at ${wpm} WPM (${label}): read back as "${expected}" (3 seeds)`);
      }
    }
  } finally {
    await b.destroy();
  }
});
