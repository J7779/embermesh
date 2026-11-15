// Hosted web app, fully offline installs and background updates (RPC_API §9), in real headless
// Chrome, with the site served from a sub-path (/embermesh/) as GitHub Pages does:
//   1. fresh load -> offline_ready -> server gone -> cold restart of the same browser profile ->
//      same identity, guides open and search, a message is composed and queued
//   2. the guide pack download is cut half-way: not offline_ready; when the network is back the
//      missing chunks are fetched (ranges) and it completes; then it works offline
//   3. updates: A installed; the site changes to B (UI file changed, same pack) -> B downloads in
//      the background while A runs -> B is used after a restart -> A's cache is removed only once B
//      booted; then C with a changed guide pack (the old pack cache goes too)
//   4. a corrupt / partial B (truncated file) is never activated; A keeps working; a later check
//      (once the site is fixed) downloads it
//   5. an update that cannot boot is rolled back to the previous build automatically
//   6. upgrade from the previous release's service worker (per-version precache) to this one
// Every site is packaged into a temp dir from the current build (build/web/wasm + ui/).
import { test, after } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { WASM_DIR, WEB_DIR, ROOT, haveWasm } from './helpers.mjs';
import { e2eSkip, nodePage, tmpDir, runCleanups, HOOK, REQUIRE, realErrors } from './lib/e2e.mjs';
import { findChrome, launchChrome } from './lib/cdp.mjs';
import { startServer } from './lib/static_server.mjs';

const BIG_PACK = path.join(ROOT, 'build/guides.pack');
const GUIDEPACK = path.join(ROOT, 'build/host/tools/guidepack');
const skip = e2eSkip([!haveWasm() && 'WASM build missing', !fs.existsSync(BIG_PACK) && 'build/guides.pack missing (./build.sh guides)']);
const cleanups = [];
after(async () => { await runCleanups(cleanups); }, HOOK);
const BASE = '/embermesh/';

// The browser logs a failed network request (an update check while offline) as a console error.
// That is the browser's network log, not an error of the app.
const offlineNoise = (errs) => realErrors(errs).filter((e) => !/net::ERR_(CONNECTION_REFUSED|FAILED|INTERNET_DISCONNECTED|EMPTY_RESPONSE|CONNECTION_RESET)/.test(e));

// ---- building site variants -----------------------------------------------------------------

function copyDir(src, dst, skipDirs = []) {
  fs.mkdirSync(dst, { recursive: true });
  for (const e of fs.readdirSync(src, { withFileTypes: true })) {
    if (skipDirs.includes(e.name)) continue;
    const a = path.join(src, e.name), b = path.join(dst, e.name);
    if (e.isDirectory()) copyDir(a, b); else fs.copyFileSync(a, b);
  }
}

/** Packages a site into a temp dir. mutate(uiDir) may change UI files; pack = guide pack path. */
function site(name, { pack = BIG_PACK, mutate = null, legacySw = false } = {}) {
  const dir = tmpDir(`ember-site-${name}-`);
  cleanups.push(() => fs.rmSync(dir, { recursive: true, force: true }));
  const uiDir = path.join(dir, 'ui');
  copyDir(path.join(ROOT, 'ui'), uiDir, ['tests']);
  if (mutate) mutate(uiDir);
  const out = path.join(dir, 'site');
  execFileSync(process.execPath, [path.join(WEB_DIR, 'tools/package.mjs'), '--ui', uiDir, '--wasm', WASM_DIR, '--out', out, '--guides', pack, '--no-single'], { stdio: 'pipe' });
  const build = JSON.parse(fs.readFileSync(path.join(out, 'wasm/build.json'), 'utf8'));
  if (legacySw) {
    // the previous release's worker: precache every file into embermesh-<version>
    const precache = ['./', ...build.files.map((f) => f.path).filter((p) => p !== 'embermesh.html'), 'wasm/build.json'];
    const tpl = fs.readFileSync(path.join(WEB_DIR, 'test/fixtures/sw-legacy.template.js'), 'utf8');
    fs.writeFileSync(path.join(out, 'sw.js'), tpl.replace('__VERSION__', `${build.version}-${build.build}`)
      .replace('__PRECACHE__', JSON.stringify(precache)).replace('__GUIDES__', JSON.stringify({ path: 'data/guides.pack', cache: `embermesh-guides-${build.guides_hash}` })));
  }
  return { dir: out, build };
}

/** A small guide pack from the first `n` guides of a category (fast update tests). */
let smallPacks = 0;
function smallPack(category, n) {
  if (!fs.existsSync(GUIDEPACK)) return BIG_PACK;
  const dir = tmpDir('ember-pack-');
  cleanups.push(() => fs.rmSync(dir, { recursive: true, force: true }));
  const src = path.join(ROOT, 'guides/content', category);
  fs.mkdirSync(path.join(dir, 'content', category), { recursive: true });
  for (const f of fs.readdirSync(src).filter((x) => x.endsWith('.md') && !x.startsWith('_')).slice(0, n)) fs.copyFileSync(path.join(src, f), path.join(dir, 'content', category, f));
  const out = path.join(dir, `p${smallPacks++}.pack`);
  execFileSync(GUIDEPACK, ['build', path.join(dir, 'content'), '-o', out], { stdio: 'pipe' });
  return out;
}

const touchUi = (tag) => (uiDir) => fs.appendFileSync(path.join(uiDir, 'js/app.js'), `\n// build variant ${tag}\n`);

// ---- browser with a persistent profile (cold restarts) --------------------------------------

async function profile(t) {
  const dir = tmpDir('ember-profile-');
  let b = null;
  const p = {
    async start() { b = await launchChrome(findChrome(), { userDataDir: dir }); return b; },
    async stop() { if (b) { await b.close(); b = null; } },
    get browser() { return b; },
  };
  t.after(async () => { await p.stop(); fs.rmSync(dir, { recursive: true, force: true }); });
  await p.start();
  return p;
}

const open = (b, url, o = {}) => nodePage(b, `${url}?nohub=1`, { timeoutMs: 60000, ...o });
const status = (P) => P.call('host.app.offline_status');
const waitReady = (P, ms = 180000) => P.waitCall('host.app.offline_status', {}, (s) => s.offline_ready, ms, 'offline_ready');

// ---- 1. offline install + cold start without a network ---------------------------------------

test('offline install from a sub-path: cold start with the server gone — same identity, guides, search, queued message', { skip, timeout: 300000 }, async (t) => {
  const A = site('a');
  const srv = await startServer(A.dir, { base: BASE, ranges: true });
  t.after(() => srv.close());
  const prof = await profile(t);
  const t0 = Date.now();
  let P = await open(prof.browser, srv.url);
  const tInteractive = Date.now() - t0;
  const me = await P.call('node.info');
  if (me.name === '') await P.call('node.set_profile', { name: 'Offline Tester' });
  const st = await waitReady(P);
  const tReady = Date.now() - t0;
  assert.equal(st.build, A.build.build);
  assert.equal(st.total_bytes, A.build.total_bytes);
  assert.equal(st.stored_bytes, st.total_bytes);
  const keys = await P.evaluate('caches.keys()');
  assert.ok(keys.includes(`embermesh-app-${A.build.build}`) && keys.includes(`embermesh-guides-${A.build.guides_hash}`), JSON.stringify(keys));
  const est = await P.evaluate('navigator.storage.estimate().then((e) => e.usage)');
  const host = await P.call('host.info');
  const act = host.actions.find((a) => a.id === 'offline_download');
  assert.match(act.description, /Everything is stored on this device \(\d+ MB\)\. Works without internet\./);
  t.diagnostic(`interactive after ${tInteractive} ms, offline_ready after ${tReady} ms; ${(st.total_bytes / 1048576).toFixed(1)} MiB stored, storage.estimate usage ${(est / 1048576).toFixed(1)} MiB; persistent=${st.persistent}`);
  await P.evaluate('__emberWasmBackend._flush().then(() => true)');
  await P.close();

  // the network is physically gone: no server at all; the browser process restarts
  await srv.close();
  await prof.stop();
  await prof.start();
  const t1 = Date.now();
  P = await open(prof.browser, srv.url);
  const tCold = Date.now() - t1;
  const me2 = await P.call('node.info');
  assert.equal(me2.node_id, me.node_id, 'same identity');
  assert.equal(me2.name, 'Offline Tester');
  const s = await P.waitCall('guides.search', { query: 'water' }, (r) => r.results.length > 0, 30000, 'search offline');
  const g = await P.call('guides.get', { id: s.results[0].id });
  assert.ok(g.markdown.length > 100, 'a guide opens');
  const sent = await P.call('messages.send', { conv: 'ch:public', text: 'written with no network' });
  assert.ok(['queued', 'sent'].includes(sent.message.status), sent.message.status);
  const st2 = await status(P);
  assert.equal(st2.offline_ready, true);
  assert.equal(st2.build, A.build.build);
  // an offline update check: no error, the status is just returned
  const chk = await P.call('host.app.check_update');
  assert.equal(chk.update.available, false);
  t.diagnostic(`cold start offline: node answered after ${tCold} ms; guides ${(await P.call('host.info')).guides.source}`);
  // a navigation to the site root and to an unknown page falls back to the cached app
  await P.evaluate(`(location.href = ${JSON.stringify(srv.url)} + 'no/such/page', true)`);
  await P.waitFor('!!(globalThis.__emberWasmBackend && __emberWasmBackend.ready)', 60000);
  assert.deepEqual(offlineNoise(P.errors), []);
});

// ---- 2. interrupted guide download -----------------------------------------------------------

test('interrupted guide pack download: not offline_ready while incomplete; resumes with ranges and completes when the network is back', { skip, timeout: 300000 }, async (t) => {
  const A = site('cut');
  const half = Math.floor(A.build.guides_bytes * 0.45);
  const srv = await startServer(A.dir, { base: BASE, ranges: true, cutAfter: { match: /guides\.pack$/, bytes: half } });
  t.after(() => srv.close());
  const prof = await profile(t);
  const P = await open(prof.browser, srv.url);
  // the page's own copy of the pack breaks off; whatever arrived is kept in verified 4 MiB chunks
  await P.waitCall('host.info', {}, (h) => h.guides && h.guides.state === 'error', 60000, 'guide download cut');
  await new Promise((r) => setTimeout(r, 1500));
  let st = await status(P);
  assert.equal(st.offline_ready, false, 'never offline_ready with a partial pack');
  const ma = await P.call('host.app.make_offline').then(() => 'ok', (e) => e.code);
  st = await status(P);
  assert.equal(st.offline_ready, false, `still not ready after a failed attempt (${ma}): ${JSON.stringify(st).slice(0, 300)}`);
  // network back
  srv.state.cutAfter = null;
  const before = srv.state.requests.length;
  await P.call('host.app.make_offline');
  st = await waitReady(P, 120000);
  const ranges = srv.state.requests.slice(before).filter((r) => /guides\.pack/.test(r));
  assert.ok(ranges.length >= 1 && ranges.every((r) => /\[bytes=\d+-\d+\]/.test(r)), `resumed with range requests: ${ranges.join(', ')}`);
  const firstStart = Number(/bytes=(\d+)-/.exec(ranges[0])[1]);
  assert.ok(firstStart > 0, 'did not start again from byte 0');
  t.diagnostic(`resume fetched ${ranges.length} chunk(s) starting at byte ${firstStart} of ${A.build.guides_bytes}`);
  // and it is complete and verified: offline restart has the library
  await srv.close();
  await P.close();
  await prof.stop(); await prof.start();
  const Q = await open(prof.browser, srv.url);
  await Q.waitCall('guides.categories', {}, (c) => c.total > 0, 30000, 'library offline');
  assert.deepEqual(offlineNoise(Q.errors), []);
});

// ---- 3. updates -----------------------------------------------------------------------------

test('updates: B downloads in the background while A runs, is used after a restart, A removed once B booted; then C with a new guide pack', { skip, timeout: 300000 }, async (t) => {
  const packA = smallPack('water', 12);
  const packC = smallPack('fire', 12);
  const A = site('upA', { pack: packA });
  const B = site('upB', { pack: packA, mutate: touchUi('B') });
  const C = site('upC', { pack: packC, mutate: touchUi('C') });
  assert.notEqual(A.build.build, B.build.build);
  assert.equal(A.build.guides_hash, B.build.guides_hash);
  assert.notEqual(B.build.guides_hash, C.build.guides_hash);
  const srv = await startServer(A.dir, { base: BASE, ranges: true });
  t.after(() => srv.close());
  const prof = await profile(t);
  let P = await open(prof.browser, srv.url);
  await waitReady(P);
  const id = (await P.call('node.info')).node_id;

  srv.setRoot(B.dir);
  const before = srv.state.requests.length;
  const t0 = Date.now();
  let st = await P.call('host.app.check_update');
  st = await P.waitCall('host.app.offline_status', {}, (s) => s.update.ready, 120000, 'update B ready');
  const tB = Date.now() - t0;
  assert.equal(st.build, A.build.build, 'A keeps running');
  assert.equal(st.update.build, B.build.build);
  assert.ok(!srv.state.requests.slice(before).some((r) => /guides\.pack/.test(r)), 'same guide pack: not downloaded again');
  const restart = (await P.call('host.info')).actions.find((a) => a.id === 'restart_update');
  assert.match(restart.description, /Switch to version/);
  assert.match((await P.call('host.info')).actions.find((a) => a.id === 'check_update').description, /is downloaded and will be used next time you open the app/);
  let keys = await P.evaluate('caches.keys()');
  assert.ok(keys.includes(`embermesh-app-${A.build.build}`) && keys.includes(`embermesh-app-${B.build.build}`));

  // restart (as the user would: close and open)
  await P.close(); await prof.stop(); await prof.start();
  P = await open(prof.browser, srv.url);
  st = await status(P);
  assert.equal(st.build, B.build.build, 'B after the restart');
  assert.equal((await P.call('node.info')).node_id, id);
  await P.waitFor(`caches.keys().then((k) => !k.includes('embermesh-app-${A.build.build}'))`, 30000);
  keys = await P.evaluate('caches.keys()');
  assert.ok(keys.includes(`embermesh-app-${B.build.build}`));
  t.diagnostic(`B downloaded in ${tB} ms while A ran; A's cache removed after B booted`);

  // C: new guide pack too; "Restart to update" applies it in this session
  srv.setRoot(C.dir);
  await P.call('host.app.check_update');
  await P.waitCall('host.app.offline_status', {}, (s) => s.update.ready, 120000, 'update C ready');
  await P.evaluate('globalThis.__oldPage = true');
  await P.call('host.app.check_update', { apply: true });
  await P.waitFor('!globalThis.__oldPage && !!(globalThis.__emberWasmBackend && __emberWasmBackend.ready)', 60000);
  await P.waitCall('host.app.offline_status', {}, (s) => s.build === C.build.build, 30000, 'C running');
  await P.waitFor(`caches.keys().then((k) => !k.includes('embermesh-guides-${A.build.guides_hash}') && k.includes('embermesh-guides-${C.build.guides_hash}'))`, 30000);
  const cats = await P.waitCall('guides.categories', {}, (c) => c.total > 0, 30000);
  assert.ok(cats.categories.some((c) => c.id === 'fire' && c.count > 0), 'the new library is in use');
  assert.deepEqual(offlineNoise(P.errors), []);
});

// ---- 4. corrupt update ----------------------------------------------------------------------

test('a corrupt / partial update is never activated; A keeps working; it is downloaded once the site is fixed', { skip, timeout: 300000 }, async (t) => {
  const pack = smallPack('water', 8);
  const A = site('cA', { pack });
  const B = site('cB', { pack, mutate: touchUi('corrupt') });
  const victim = path.join(B.dir, 'js/app.js');
  const good = fs.readFileSync(victim);
  fs.writeFileSync(victim, good.subarray(0, Math.floor(good.length / 2))); // truncated on the server
  const srv = await startServer(A.dir, { base: BASE, ranges: true });
  t.after(() => srv.close());
  const prof = await profile(t);
  let P = await open(prof.browser, srv.url);
  await waitReady(P);
  srv.setRoot(B.dir);
  let st = await P.call('host.app.check_update');
  st = await status(P);
  assert.equal(st.update.ready, false, 'not ready');
  assert.ok(st.update.available);
  await P.close(); await prof.stop(); await prof.start();
  P = await open(prof.browser, srv.url);
  st = await status(P);
  assert.equal(st.build, A.build.build, 'A keeps running after a restart');
  assert.equal(st.offline_ready, true);
  // automatic checks back off; the site is fixed; an explicit check downloads it
  fs.writeFileSync(victim, good);
  st = await P.call('host.app.check_update');
  st = await P.waitCall('host.app.offline_status', {}, (s) => s.update.ready, 60000, 'fixed B ready');
  assert.equal(st.update.build, B.build.build);
  assert.deepEqual(offlineNoise(P.errors), []);
});

// ---- 5. rollback ----------------------------------------------------------------------------

test('an update that cannot start is rolled back to the previous build automatically', { skip, timeout: 300000 }, async (t) => {
  const pack = smallPack('water', 8);
  const A = site('rA', { pack });
  const B = site('rB', { pack, mutate: (ui) => fs.writeFileSync(path.join(ui, 'js/backends/wasm.js'), `${fs.readFileSync(path.join(ui, 'js/backends/wasm.js'), 'utf8').replace("this._initPromise = this._init().catch(", "this._initPromise = Promise.reject(new Error('broken build (test)')).catch(")}`) });
  const srv = await startServer(A.dir, { base: BASE, ranges: true });
  t.after(() => srv.close());
  const prof = await profile(t);
  let P = await open(prof.browser, srv.url);
  await waitReady(P);
  srv.setRoot(B.dir);
  await P.call('host.app.check_update');
  await P.waitCall('host.app.offline_status', {}, (s) => s.update.ready, 60000, 'B ready');
  await P.close(); await prof.stop(); await prof.start();
  // B starts, its core never comes up, the page reports it and the worker switches back to A
  const { openPage } = await import('./lib/cdp.mjs');
  const R = await openPage(prof.browser.cdp, `${srv.url}?nohub=1`);
  t.after(() => R.close());
  await R.waitFor('!!(globalThis.__emberWasmBackend && __emberWasmBackend.ready)', 90000);
  const st = await R.evaluate(`__emberWasmBackend.call('host.app.offline_status')`);
  assert.equal(st.build, A.build.build, 'rolled back to A');
  const state = await R.evaluate(`caches.open('embermesh-state').then((c) => c.match('state.json')).then((r) => r.json())`);
  assert.ok(state.failed && state.failed[B.build.build], 'B is marked failed');
});

// ---- 6. upgrade from the previous service worker ---------------------------------------------

test('upgrade from the previous release\'s service worker: the new one takes over, nothing is lost offline', { skip, timeout: 300000 }, async (t) => {
  const pack = smallPack('water', 8);
  const OLD = site('legacy', { pack, legacySw: true });
  const NEW = site('new', { pack, mutate: touchUi('new') });
  const srv = await startServer(OLD.dir, { base: BASE, ranges: true });
  t.after(() => srv.close());
  const prof = await profile(t);
  let P = await open(prof.browser, srv.url, { });
  await P.waitFor(`navigator.serviceWorker.getRegistration().then((r) => !!(r && r.active && r.active.state === 'activated'))`, 60000);
  await P.waitFor(`caches.keys().then((k) => k.some((n) => /^embermesh-\\d/.test(n)))`, 30000);
  const id = (await P.call('node.info')).node_id;
  // the browser's own record of each worker version (DevTools ServiceWorker domain), for the report
  // when the takeover does not happen: which version was installed/activated, running or stopped
  const tl = [];
  const t0 = Date.now();
  const offSw = prof.browser.cdp.onEvent((m) => {
    if (m.method === 'ServiceWorker.workerVersionUpdated') for (const v of m.params.versions) tl.push(`+${Date.now() - t0}ms v${v.versionId} ${v.status}/${v.runningStatus} clients=${(v.controlledClients || []).length}`);
    else if (m.method === 'ServiceWorker.workerErrorReported') tl.push(`+${Date.now() - t0}ms error ${m.params.errorMessage.errorMessage}`);
  });
  t.after(() => offSw());
  await P.send('ServiceWorker.enable').catch(() => {});
  srv.setRoot(NEW.dir);
  // the browser checks sw.js on navigation; the new worker installs, skips waiting and claims
  await P.reloadNode();
  // the new worker precaches the site before it takes over: on a starved host that takes a while,
  // so only a safety bound here (the test's own timeout), and its state if it never does
  try {
    await P.waitFor(`new Promise((res) => { const sw = navigator.serviceWorker.controller; if (!sw) return res(false); navigator.serviceWorker.addEventListener('message', (e) => res(e.data && e.data.logic), { once: true }); sw.postMessage({ type: 'embermesh-sw-logic' }); setTimeout(() => res(false), 2000); })`, 240000);
  } catch (e) {
    const reg = await P.evaluate(`navigator.serviceWorker.getRegistration().then((r) => JSON.stringify(r && { active: r.active && r.active.state, waiting: r.waiting && r.waiting.state, installing: r.installing && r.installing.state, controller: !!navigator.serviceWorker.controller }))`).catch((x) => x.message);
    throw new Error(`the new service worker did not take over within 240 s: registration ${reg}\nworker versions as the browser saw them:\n${tl.slice(-60).join('\n')}`);
  }
  await P.reloadNode();
  const st = await waitReady(P, 120000);
  assert.equal(st.build, NEW.build.build);
  await P.waitFor(`caches.keys().then((k) => !k.some((n) => /^embermesh-\\d/.test(n)))`, 30000);
  await P.close();
  await srv.close(); await prof.stop(); await prof.start();
  const Q = await open(prof.browser, srv.url);
  assert.equal((await Q.call('node.info')).node_id, id, 'same identity offline after the upgrade');
  assert.equal((await Q.call('host.app.offline_status')).build, NEW.build.build);
  assert.deepEqual(offlineNoise(Q.errors), []);
});

void REQUIRE;
