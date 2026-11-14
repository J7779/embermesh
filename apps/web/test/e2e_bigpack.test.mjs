// Big guide pack in a real browser: streamed load with progress, no full extra copy, cached for
// offline, page stays responsive. Uses EMBER_BIG_PACK=<path> (e.g. a 40-60 MB pack), else the
// largest pack at hand (build/guides.pack). The site is packaged into a temp dir with that pack.
//
// Reports (diagnostics): pack size, first load (network) and reload (Cache API) times, the core's
// share of it (ember_guides_load), the longest main-thread stall, renderer RSS growth at the peak.
import { test, after } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { WASM_DIR, WEB_DIR, ROOT } from './helpers.mjs';
import { e2eSkip, startBrowser, nodePage, realErrors, tmpDir, runCleanups, HOOK } from './lib/e2e.mjs';
import { startServer } from './lib/static_server.mjs';

const PACK = process.env.EMBER_BIG_PACK || path.join(ROOT, 'build/guides.pack');
const skip = e2eSkip([!fs.existsSync(PACK) && `no guide pack at ${PACK} (./build.sh guides)`]);
const cleanups = [];
after(async () => { await runCleanups(cleanups); }, HOOK);

function rssKb(pid) {
  try { return Number(execFileSync('ps', ['-o', 'rss=', '-p', String(pid)], { encoding: 'utf8' }).trim()) || 0; } catch { return 0; }
}

const WATCH = `(() => {
  // longest gap between 10 ms timer ticks = longest main-thread stall
  globalThis.__stall = { max: 0, last: performance.now() };
  setInterval(() => { const n = performance.now(); __stall.max = Math.max(__stall.max, n - __stall.last - 10); __stall.last = n; }, 10);
  return true;
})()`;

test('big guide pack: streamed with progress, ~1x memory, cached for offline, responsive', { skip, timeout: 300000 }, async (t) => {
  const mb = fs.statSync(PACK).size / 1048576;
  const dir = tmpDir('ember-bigpack-');
  cleanups.push(() => fs.rmSync(dir, { recursive: true, force: true }));
  execFileSync(process.execPath, [path.join(WEB_DIR, 'tools/package.mjs'), '--wasm', WASM_DIR, '--out', dir, '--guides', PACK, '--no-single'], { stdio: 'pipe' });
  const browser = await startBrowser();
  if (browser.error) { t.skip(`Chrome could not start: ${browser.error}`); return; }
  cleanups.push(() => browser.close());
  const RATE = Number(process.env.EMBER_BIG_PACK_RATE) || 8 * 1024 * 1024;
  const srv = await startServer(dir, { throttle: { match: /guides\.pack$/, bytesPerSec: RATE } });
  cleanups.push(() => srv.close());

  // The server paces the pack like a hub over Wi-Fi (~8 MiB/s): it takes seconds, the app must not wait.
  const P = await nodePage(browser, `${srv.url}index.html?nohub=1&profile=big`);
  cleanups.push(() => P.close());
  await P.evaluate(WATCH);
  // usable before the library is in: node answers, a message can be sent, guides.* are just empty
  const early = await P.call('host.info');
  const t0 = Date.now();
  const info = await P.call('node.info');
  await P.call('node.set_profile', { name: 'Early bird' });
  const sent = await P.call('messages.send', { conv: 'ch:public', text: 'sent while the library downloads' });
  const emptyCats = await P.call('guides.categories');
  const stillLoading = (await P.call('host.info')).guides.state;
  t.diagnostic(`while loading (${early.guides.state} -> ${stillLoading}): node.info + messages.send answered in ${Date.now() - t0} ms, guides.categories total ${emptyCats.total}`);
  assert.match(info.node_id, /^[0-9a-f]{16}$/);
  assert.ok(sent.message.id);
  if (mb > 8) {
    assert.equal(stillLoading, 'loading', 'the pack was still downloading while the app answered');
    assert.equal(emptyCats.total, 0, 'empty library until ember_guides_load');
  }
  // renderer process for RSS sampling
  const procs = await browser.cdp.send('SystemInfo.getProcessInfo').catch(() => ({ processInfo: [] }));
  const renderers = (procs.processInfo || []).filter((p) => p.type === 'renderer').map((p) => p.id);
  const sample = () => renderers.reduce((n, id) => n + rssKb(id), 0);
  let peak = 0;
  const sampler = setInterval(() => { peak = Math.max(peak, sample()); }, 100);
  cleanups.push(() => clearInterval(sampler));

  // host.info.guides mirrors the events (events may fire before a test listener is attached)
  const loaded = (await P.waitCall('host.info', {}, (h) => h.guides && h.guides.state === 'loaded', 120000, 'guides loaded')).guides;
  const progress = await P.events('guides.progress'); // may miss the first ones (see above)
  const hostProgress = await P.events('host.guides.progress');
  assert.ok(hostProgress.length >= 1, 'host.guides.progress emitted');
  const fin = hostProgress.at(-1);
  assert.equal(fin.loaded, fin.total, 'final host.guides.progress has loaded == total');
  assert.ok(hostProgress.slice(0, -1).every((p) => p.loaded < p.total), 'loaded < total until the library is in');
  const cats = await P.waitCall('guides.categories', {}, (r) => r.total > 0, 10000);
  const s = await P.call('guides.search', { query: 'water purification', limit: 3 });
  assert.ok(s.results.length > 0);
  const g = await P.call('guides.get', { id: s.results[0].id });
  assert.ok(g.markdown.length > 100);
  const stall1 = await P.evaluate('__stall.max');
  const wasmMb = await P.evaluate('__emberWasmBackend.M.HEAPU8.length / 1048576');
  const host = await P.call('host.info');
  clearInterval(sampler);
  peak = Math.max(peak, sample());

  assert.equal(loaded.source, 'network');
  assert.equal(loaded.bytes, fs.statSync(PACK).size);
  assert.equal(host.guides.state, 'loaded');
  if (mb > 4) assert.ok(progress.length >= 1, `progress events (${progress.length})`);
  assert.ok(progress.every((p) => p.total === loaded.bytes), 'progress knows the total (Content-Length)');
  t.diagnostic(`pack ${mb.toFixed(1)} MiB, ${cats.total} guides: first load ${loaded.ms} ms (network at ${(RATE / 1048576).toFixed(0)} MiB/s), ${progress.length} progress events, longest main-thread stall ${stall1.toFixed(0)} ms, wasm memory ${wasmMb.toFixed(0)} MiB, all renderer processes RSS peak ${(peak / 1024).toFixed(0)} MiB`);
  assert.ok(wasmMb < mb + 48, `wasm memory ${wasmMb.toFixed(0)} MiB for a ${mb.toFixed(0)} MiB pack: one copy, not two`);

  // reload: the content-addressed cache serves it (also offline)
  await P.waitEvent('guides.cached', () => true, 60000);
  await P.waitFor(`navigator.serviceWorker.getRegistration().then(r => !!(r && r.active))`, 30000);
  await P.send('Network.emulateNetworkConditions', { offline: true, latency: 0, downloadThroughput: -1, uploadThroughput: -1 });
  await P.reloadNode();
  const off = await P.call('node.info');
  assert.equal(off.node_id, info.node_id, 'same node offline');
  await P.evaluate(WATCH);
  const again = (await P.waitCall('host.info', {}, (h) => h.guides && /loaded|error/.test(h.guides.state), 120000, 'guides loaded offline')).guides;
  assert.equal(again.state, 'loaded', JSON.stringify(again));
  const stall2 = await P.evaluate('__stall.max');
  await P.send('Network.emulateNetworkConditions', { offline: false, latency: 0, downloadThroughput: -1, uploadThroughput: -1 });
  assert.equal(again.source, 'cache');
  assert.ok((await P.call('guides.categories')).total === cats.total, 'same library offline');
  t.diagnostic(`offline reload: ${again.ms} ms from the Cache API, longest stall ${stall2.toFixed(0)} ms`);
  console.log(`# big pack ${mb.toFixed(1)} MiB / ${cats.total} guides: network ${loaded.ms} ms, cache ${again.ms} ms, stall ${stall1.toFixed(0)}/${stall2.toFixed(0)} ms, wasm ${wasmMb.toFixed(0)} MiB, renderers RSS peak ${(peak / 1024).toFixed(0)} MiB`);
  assert.deepEqual(realErrors(P.errors), []);
});
