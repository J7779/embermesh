// Remote backend (desktop operator page, ?remote=1) against the real daemon: morse.rx.feed at
// camera frame rate (30 feeds/s) through the FeedBatcher — round-trip times and how many feeds
// were merged. Needs dist/desktop/emberd (EMBER_EMBERD=path overrides); skipped without it unless
// EMBER_REQUIRE_BROWSER=1.
import { test, after } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import net from 'node:net';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { DIST_DIR, ROOT } from './helpers.mjs';
import { e2eSkip, startBrowser, nodePage, realErrors, tmpDir, runCleanups, HOOK, REQUIRE } from './lib/e2e.mjs';

const BIN = process.env.EMBER_EMBERD || path.join(ROOT, 'dist/desktop/emberd');
const skip = e2eSkip([!fs.existsSync(BIN) && `emberd not built (${BIN}): ./build.sh desktop`]);
const cleanups = [];
after(async () => { await runCleanups(cleanups); }, HOOK);

const freePort = () => new Promise((res) => { const s = net.createServer(); s.listen(0, '127.0.0.1', () => { const { port } = s.address(); s.close(() => res(port)); }); });

async function startDaemon() {
  const port = await freePort();
  const dir = tmpDir('emberd-remote-');
  const token = 'test-token-' + Math.random().toString(36).slice(2);
  const proc = spawn(BIN, ['--data-dir', dir, '--web-root', DIST_DIR, '--port', String(port), '--bind', '127.0.0.1', '--no-udp', '--token', token, '--log-level', 'warn'], { stdio: ['ignore', 'ignore', 'ignore'] });
  cleanups.push(async () => {
    if (proc.exitCode === null) { proc.kill('SIGTERM'); await new Promise((r) => { const t = setTimeout(() => { proc.kill('SIGKILL'); r(); }, 5000); proc.once('exit', () => { clearTimeout(t); r(); }); }); }
    fs.rmSync(dir, { recursive: true, force: true });
  });
  for (let i = 0; i < 75; i++) {
    try { const r = await fetch(`http://127.0.0.1:${port}/hub.json`); if (r.ok) return { port, token }; } catch {}
    await new Promise((r) => setTimeout(r, 200));
  }
  throw new Error('emberd did not start');
}

const FEED_LOOP = (session, seconds) => `(async () => {
  const b = globalThis.__emberBackend || globalThis.__emberRemote;
  const B = b || (await import(new URL('js/backends/remote.js', location.href).href).then(async (m) => { const x = new m.default(); await x.init(); globalThis.__emberRemote = x; return x; }));
  const rtts = []; let errors = {}; let n = 0;
  const t0 = performance.now();
  // A camera delivers 30 frames per second of real time, however late the page's timers fire
  // (hosted VMs stretch short timers several-fold): on every tick, send the feeds that are due by
  // the clock, each stamped with its own frame time.
  const FRAME = 1000 / 30;
  const send = (frameTime) => {
    const t = performance.now(); n++;
    B.call('morse.rx.feed', { session: ${JSON.stringify(session)}, times_ms: [Math.round(frameTime)], levels: [Math.random()] })
      .then(() => rtts.push(performance.now() - t), (e) => { errors[e.code] = (errors[e.code] || 0) + 1; rtts.push(performance.now() - t); });
  };
  await new Promise((done) => {
    const iv = setInterval(() => {
      const now = performance.now();
      const due = Math.min(Math.floor((now - t0) / FRAME), Math.round(${seconds} * 30));
      while (n < due) send(t0 + n * FRAME);
      if (now - t0 > ${seconds * 1000}) { clearInterval(iv); setTimeout(done, 1500); }
    }, FRAME);
  });
  rtts.sort((a, b) => a - b);
  const q = (p) => rtts[Math.min(rtts.length - 1, Math.floor(p * rtts.length))];
  return { n, done: rtts.length, p50: q(0.5), p95: q(0.95), max: rtts[rtts.length - 1], errors, stats: B.feeds.stats };
})()`;

test('remote backend: morse.rx.feed at 30 feeds/s over /rpc — batched, bounded, round trip measured', { skip, timeout: 120000 }, async (t) => {
  let d;
  try { d = await startDaemon(); } catch (e) { if (REQUIRE) throw e; t.skip(e.message); return; }
  const browser = await startBrowser();
  if (browser.error) { t.skip(`Chrome could not start: ${browser.error}`); return; }
  cleanups.push(() => browser.close());
  // the operator page: UI from the daemon, remote backend; we drive a RemoteBackend instance directly
  const { openPage } = await import('./lib/cdp.mjs');
  const P = await openPage(browser.cdp, `http://127.0.0.1:${d.port}/index.html?remote=1&token=${d.token}`);
  cleanups.push(() => P.close());
  await P.waitFor('document.readyState === "complete"', 20000);
  let session = 1;
  const start = await P.evaluate(`(async () => {
    const m = await import(new URL('js/backends/remote.js', location.href).href);
    const x = new m.default(); await x.init(); globalThis.__emberRemote = x;
    return x.call('morse.rx.start', { source: 'light' }).then((r) => ({ ok: true, r }), (e) => ({ ok: false, code: e.code }));
  })()`);
  const core = start.ok ? 'morse.rx implemented' : `morse.rx.start -> ${start.code} (core not there yet: measuring the transport round trip with the error reply)`;
  if (start.ok) session = start.r.session;
  const r = await P.evaluate(FEED_LOOP(session, 5), 30000);
  t.diagnostic(`${core}; ${r.n} feeds in 5 s -> ${r.stats.sent} RPCs (${r.stats.merged} merged), RTT p50 ${r.p50.toFixed(1)} ms, p95 ${r.p95.toFixed(1)} ms, max ${r.max.toFixed(1)} ms, errors ${JSON.stringify(r.errors)}, max backlog ${r.stats.max_backlog}, dropped ${r.stats.dropped_samples}`);
  console.log(`# remote morse.rx.feed @30/s: p50 ${r.p50.toFixed(1)} ms, p95 ${r.p95.toFixed(1)} ms, ${r.stats.sent} RPCs for ${r.n} feeds (${core})`);
  assert.equal(r.done, r.n, 'every feed settled');
  assert.ok(r.n >= 120, `about 150 feeds sent (${r.n})`);
  // the round trip is reported above; asserted is only that feeds never stall (host speed varies)
  assert.ok(r.p95 < 3000, `p95 round trip ${r.p95.toFixed(0)} ms`);
  if (start.ok) assert.deepEqual(r.errors, {});
  // A slow link (300 ms latency each way, ~16 kB/s): feeds merge, the RPC rate drops, the backlog stays bounded.
  await P.send('Network.emulateNetworkConditions', { offline: false, latency: 300, downloadThroughput: 16000, uploadThroughput: 16000 });
  const slow = await P.evaluate(FEED_LOOP(session, 5), 40000);
  await P.send('Network.emulateNetworkConditions', { offline: false, latency: 0, downloadThroughput: -1, uploadThroughput: -1 });
  const sent = slow.stats.sent - r.stats.sent, merged = slow.stats.merged - r.stats.merged;
  t.diagnostic(`slow link: ${slow.n} feeds -> ${sent} RPCs (${merged} merged), RTT p50 ${slow.p50.toFixed(0)} ms, p95 ${slow.p95.toFixed(0)} ms, max backlog ${slow.stats.max_backlog} samples, dropped ${slow.stats.dropped_samples}`);
  console.log(`# remote feed on a 300 ms link: ${slow.n} feeds -> ${sent} RPCs, p95 ${slow.p95.toFixed(0)} ms, backlog max ${slow.stats.max_backlog}`);
  assert.equal(slow.done, slow.n, 'every feed settled on the slow link');
  if (slow.p50 > 200) assert.ok(sent < slow.n / 2, 'feeds were merged while one was in flight');
  assert.ok(slow.stats.max_backlog <= 4000, 'bounded backlog');
  assert.deepEqual(realErrors(P.errors), []);
});
