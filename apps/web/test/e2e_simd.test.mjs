// WebAssembly SIMD module with a baseline fallback (ui/js/backends/wasm.js), real headless Chrome:
//   - default: the SIMD module is used where the engine supports it (host.info.wasm_simd)
//   - ?nosimd=1 and an engine without SIMD (WebAssembly.validate says no) -> the baseline module
//   - a SIMD module that fails to compile/load -> the baseline module, no error for the user
//   - search timing on both modules with the shipped guide pack (diagnostics)
// The single file always uses the baseline module (it must run in old web views; see README).
import { test, after } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { DIST_DIR } from './helpers.mjs';
import { e2eSkip, startBrowser, nodePage, realErrors, tmpDir, runCleanups, HOOK } from './lib/e2e.mjs';
import { startServer } from './lib/static_server.mjs';

const hasSimd = fs.existsSync(path.join(DIST_DIR, 'wasm/ember-simd.wasm'));
// built with --no-simd: nothing to test here (a deliberate build choice, so a skip even under
// EMBER_REQUIRE_BROWSER=1)
const skip = e2eSkip() || (!hasSimd && 'dist/web has no SIMD module (built with --no-simd)');
const cleanups = [];
after(async () => { await runCleanups(cleanups); }, HOOK);

const QUERIES = ['boil water', 'broken leg splint', 'radio antenna', 'start a fire wet wood', 'purify water tablets', 'hypothermia', 'snake bite', 'morse code', 'solar panel battery', 'food storage'];
const SEARCH_BENCH = `(async () => {
  const b = globalThis.__emberWasmBackend;
  await b.guidesReady;
  const c = await b.call('guides.categories');
  if (!c.total) return null;
  for (const q of ${JSON.stringify(QUERIES)}) await b.call('guides.search', { query: q, limit: 20 }); // warm up
  const t = [];
  for (let r = 0; r < 3; r++) for (const q of ${JSON.stringify(QUERIES)}) { const t0 = performance.now(); await b.call('guides.search', { query: q, limit: 20 }); t.push(performance.now() - t0); }
  t.sort((x, y) => x - y);
  return { median: t[t.length >> 1], p90: t[Math.floor(t.length * 0.9)], n: t.length };
})()`;

async function page(b, url, opts) {
  const P = await nodePage(b, url, { timeoutMs: 60000, ...opts });
  cleanups.push(() => P.close());
  return P;
}

test('SIMD module where supported, baseline with ?nosimd=1 or without engine support; search timing on both', { skip, timeout: 240000 }, async (t) => {
  const b = await startBrowser(); if (b.error) { t.skip(b.error); return; }
  cleanups.push(() => b.close());
  const srv = await startServer(DIST_DIR);
  cleanups.push(() => srv.close());
  const S = await page(b, `${srv.url}index.html?nohub=1&profile=simd1`);
  assert.equal((await S.call('host.info')).wasm_simd, true, 'SIMD module in use');
  const B = await page(b, `${srv.url}index.html?nohub=1&nosimd=1&profile=simd2`);
  assert.equal((await B.call('host.info')).wasm_simd, false, '?nosimd=1 -> baseline');
  const noEngine = await page(b, `${srv.url}index.html?nohub=1&profile=simd3`, {
    beforeNavigate: (send) => send('Page.addScriptToEvaluateOnNewDocument', { source: 'const v = WebAssembly.validate; WebAssembly.validate = (bytes) => (bytes && bytes.length === 31 ? false : v(bytes));' }),
  });
  assert.equal((await noEngine.call('host.info')).wasm_simd, false, 'no SIMD in the engine -> baseline');
  for (const P of [S, B, noEngine]) assert.match((await P.call('node.info')).node_id, /^[0-9a-f]{16}$/);
  const simd = await S.evaluate(SEARCH_BENCH, 180000);
  const base = await B.evaluate(SEARCH_BENCH, 180000);
  if (simd && base) {
    t.diagnostic(`guides.search (30 queries, limit 20): SIMD median ${simd.median.toFixed(1)} ms / p90 ${simd.p90.toFixed(1)} ms; baseline median ${base.median.toFixed(1)} ms / p90 ${base.p90.toFixed(1)} ms`);
    console.log(`# search: SIMD ${simd.median.toFixed(1)} ms, baseline ${base.median.toFixed(1)} ms (median)`);
  }
  for (const P of [S, B, noEngine]) assert.deepEqual(realErrors(P.errors), []);
});

test('a SIMD module that cannot be loaded falls back to the baseline module', { skip, timeout: 120000 }, async (t) => {
  const dir = tmpDir('ember-simdbad-');
  cleanups.push(() => fs.rmSync(dir, { recursive: true, force: true }));
  // a copy of the site whose SIMD module is damaged (an engine that cannot compile it)
  for (const f of fs.readdirSync(DIST_DIR, { recursive: true })) {
    const src = path.join(DIST_DIR, f);
    if (fs.statSync(src).isDirectory()) continue;
    fs.mkdirSync(path.dirname(path.join(dir, f)), { recursive: true });
    fs.copyFileSync(src, path.join(dir, f));
  }
  fs.writeFileSync(path.join(dir, 'wasm/ember-simd.wasm'), Buffer.from('not a webassembly module'));
  const b = await startBrowser(); if (b.error) { t.skip(b.error); return; }
  cleanups.push(() => b.close());
  const srv = await startServer(dir);
  cleanups.push(() => srv.close());
  const P = await page(b, `${srv.url}index.html?nohub=1&profile=simdbad`);
  assert.equal((await P.call('host.info')).wasm_simd, false);
  assert.match((await P.call('node.info')).node_id, /^[0-9a-f]{16}$/);
  // the failed attempt shows up only as the browser's own network/compile log, never as an app error
  assert.deepEqual(realErrors(P.errors).filter((e) => !/ember-simd|WebAssembly|wasm/i.test(e)), []);
});
