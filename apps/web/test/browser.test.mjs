// Headless Chrome end-to-end: the built site from a dumb HTTP server (incl. offline via the
// service worker and hub auto-detection) and the single file from file://.
// Requires apps/web/build.sh output and Chrome (set CHROME=/path/to/chrome to override).
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { DIST_DIR } from './helpers.mjs';
import { findChrome, launchChrome, openPage } from './lib/cdp.mjs';
import { e2eSkip, runCleanups, HOOK } from './lib/e2e.mjs';
import { startServer } from './lib/static_server.mjs';

const chrome = findChrome();
const skip = e2eSkip(); // EMBER_REQUIRE_BROWSER=1: missing build/Chrome is a failure, not a skip

const NODE_READY = `(async () => {
  const b = globalThis.__emberWasmBackend;
  if (!b || !b.ready) return null;
  const info = await b.call('node.info');
  const host = await b.call('host.info');
  return { info, host };
})()`;

// Errors that come from the browser environment, not from our code.
const IGNORABLE = [/favicon\.ico/];
const realErrors = (errs) => errs.filter((e) => !IGNORABLE.some((r) => r.test(e)));
// After a stretch with the network cut off: the browser logs each failed network request (the app's
// update check, expected to fail then) as a console error; that is its network log, not the app's.
const offlineNoise = (errs) => realErrors(errs).filter((e) => !/net::ERR_(CONNECTION_REFUSED|FAILED|INTERNET_DISCONNECTED|EMPTY_RESPONSE|CONNECTION_RESET)/.test(e));

let browser;
let launchError = null;
const servers = [];

before(async () => {
  if (skip) return;
  try {
    browser = await launchChrome(chrome);
  } catch (e) {
    // Managed/locked-down machines can refuse to start Chrome; that is an environment problem,
    // reported as a skip unless EMBER_REQUIRE_BROWSER=1 makes it a failure.
    if (process.env.EMBER_REQUIRE_BROWSER === '1') throw e;
    launchError = String(e.message || e).split('\n')[0];
    console.warn(`browser tests skipped: ${launchError}`);
  }
}, HOOK);

const needBrowser = (t) => { if (launchError) { t.skip(`Chrome could not start: ${launchError}`); return false; } return true; };

after(async () => {
  await runCleanups([...(browser ? [() => browser.close()] : []), ...servers.map((s) => () => s.close())]);
}, HOOK);

test('static site over HTTP: WASM node answers, no console errors, service worker caches for offline', { skip, timeout: 90000 }, async (t) => {
  if (!needBrowser(t)) return;
  const srv = await startServer(DIST_DIR);
  servers.push(srv);
  const page = await openPage(browser.cdp, `${srv.url}index.html`);
  const r = await page.waitFor(NODE_READY);
  assert.match(r.info.node_id, /^[0-9a-f]{16}$/);
  assert.equal(r.host.platform, 'web');
  assert.equal(r.host.backend, 'wasm');
  assert.equal(r.host.storage, 'indexeddb', `storage: ${r.host.storage_note}`);
  assert.equal(r.host.hub, null, 'plain static server is not a hub');
  const kinds = Object.fromEntries(r.host.endpoint_kinds.map((k) => [k.kind, k]));
  for (const k of ['websocket', 'webrtc', 'serial', 'ble', 'audio', 'loopback']) assert.ok(kinds[k], k);
  assert.equal(kinds.webrtc.available, true);
  assert.equal(kinds.loopback.available, true);
  assert.equal(kinds.serial.available, true, 'desktop Chrome has Web Serial');
  for (const k of r.host.endpoint_kinds) if (!k.available) assert.ok(k.unavailable_reason, k.kind);
  assert.ok(r.host.features.includes('webrtc'));
  assert.ok(r.host.features.includes('file'));

  // service worker installs and takes control; then the site loads with the network cut off
  await page.waitFor(`navigator.serviceWorker.getRegistration().then(r => !!(r && r.active && r.active.state === 'activated'))`, 30000);
  await page.reload();
  await page.waitFor('!!navigator.serviceWorker.controller');
  await page.waitFor(NODE_READY);
  // everything (app, core, guide library) is stored and verified before the network goes away
  await page.waitFor(`globalThis.__emberWasmBackend.call('host.app.offline_status').then((s) => s.offline_ready)`, 120000);
  await page.send('Network.emulateNetworkConditions', { offline: true, latency: 0, downloadThroughput: -1, uploadThroughput: -1 });
  await srv.close();
  servers.splice(servers.indexOf(srv), 1);
  await page.reload();
  const off = await page.waitFor(NODE_READY);
  assert.match(off.info.node_id, /^[0-9a-f]{16}$/, 'works offline from the service-worker cache');
  assert.equal(off.info.node_id, r.info.node_id, 'same identity offline (IndexedDB)');
  if (await page.evaluate(`fetch('wasm/build.json').then(r => r.json()).then(j => !!j.guides)`)) {
    const g = await page.waitFor(`globalThis.__emberWasmBackend.call('guides.categories').then(c => c.total > 0 && c.total)`);
    assert.ok(g > 0, 'guide library loads offline from the cache');
  }
  await page.send('Network.emulateNetworkConditions', { offline: false, latency: 0, downloadThroughput: -1, uploadThroughput: -1 });
  assert.deepEqual(offlineNoise(page.errors), []);
  await page.close();
});

test('hub guest mode: page served by a hub auto-connects to its /ws', { skip, timeout: 60000 }, async (t) => {
  if (!needBrowser(t)) return;
  const srv = await startServer(DIST_DIR, { hub: { name: 'Camp' } });
  servers.push(srv);
  const page = await openPage(browser.cdp, `${srv.url}index.html`);
  const r = await page.waitFor(NODE_READY);
  const hub = await page.waitFor(`globalThis.__emberWasmBackend.call('host.info').then(h => h.hub)`);
  assert.equal(hub.url, `ws://127.0.0.1:${srv.port}/ws`);
  assert.equal(hub.name, 'Camp');
  if (!r.info.stub) {
    // real core: the websocket link is added and the endpoint connects
    await page.waitFor(`globalThis.__emberWasmBackend.call('host.info').then(h => h.hub && h.hub.connected)`);
    assert.ok(srv.wsClients.size >= 1);
  }
  assert.deepEqual(realErrors(page.errors), []);
  await page.close();
});

test('single file from file:// (no server): WASM node answers, no console errors', { skip, timeout: 60000 }, async (t) => {
  if (!needBrowser(t)) return;
  const url = pathToFileURL(path.join(DIST_DIR, 'embermesh.html')).href;
  const page = await openPage(browser.cdp, url);
  const r = await page.waitFor(NODE_READY);
  assert.match(r.info.node_id, /^[0-9a-f]{16}$/);
  assert.equal(r.host.single_file, true);
  assert.equal(r.host.hub, null);
  assert.ok(r.host.endpoint_kinds.find((k) => k.kind === 'webrtc').available);
  // the single file is a secure context, so device APIs are offered
  assert.equal(await page.evaluate('window.isSecureContext'), true);
  // a second tab (same profile/origin) gets a temporary node instead of corrupting storage
  const page2 = await openPage(browser.cdp, url);
  const r2 = await page2.waitFor(NODE_READY);
  if (r.host.storage === 'indexeddb') assert.equal(r2.host.storage, 'memory');
  assert.deepEqual(realErrors(page.errors), []);
  assert.deepEqual(realErrors(page2.errors), []);
  await page2.close();
  await page.close();
});
