// End-to-end library/tools tests in real headless Chrome with the real WASM core:
//   - the static site: guides.search + guides.get from the real pack, tools.run, rf.bands
//   - the single file (embermesh.html, guide pack inlined) opened from file://: node answers,
//     a guide opens, and two file:// tabs talk over the loopback endpoint
// The inlined single file is built into a temp dir by package.mjs (dist/ is not touched) unless
// dist/web/embermesh.html already has the pack inlined.
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { pathToFileURL } from 'node:url';
import { DIST_DIR, WASM_DIR, WEB_DIR, ROOT } from './helpers.mjs';
import { e2eSkip, startBrowser, nodePage, realErrors, tmpDir } from './lib/e2e.mjs';
import { runCleanups, HOOK } from './lib/e2e.mjs';
import { startServer } from './lib/static_server.mjs';

const PACK = process.env.EMBER_GUIDES_PACK || path.join(ROOT, 'build/guides.pack');
const skip = e2eSkip([!fs.existsSync(PACK) && `guide pack missing (${PACK}): run ./build.sh guides`]);
let browser;
const cleanups = [];

before(async () => { if (!skip) browser = await startBrowser(); }, HOOK);
after(async () => {
  if (browser && browser.close) cleanups.unshift(() => browser.close()); // closed last
  await runCleanups(cleanups);
}, HOOK);
const needBrowser = (t) => {
  if (browser && browser.error) { t.skip(`Chrome could not start: ${browser.error}`); return false; }
  return true;
};

async function checkLibrary(P) {
  const cats = await P.waitCall('guides.categories', {}, (r) => r.total > 0, 20000, 'guide pack loaded');
  assert.ok(cats.total >= 800, `the real pack (${cats.total} guides)`);
  assert.ok(cats.categories.find((c) => c.id === 'water'));
  const s = await P.call('guides.search', { query: 'purify water boiling', limit: 5 });
  assert.ok(s.results.length > 0, 'search finds something');
  const g = await P.call('guides.get', { id: s.results[0].id });
  assert.equal(g.id, s.results[0].id);
  assert.ok(g.markdown.length > 200, 'guide body');
  assert.ok(g.license && g.source, 'licence metadata');
  return { cats, g };
}

test('static site: guides search/open from the real pack, tools.run calculator, rf.bands', { skip, timeout: 90000 }, async (t) => {
  if (!needBrowser(t)) return;
  if (!fs.existsSync(path.join(DIST_DIR, 'data/guides.pack'))) {
    assert.fail('dist/web has no data/guides.pack: build with ./build.sh web (it bundles build/guides.pack)');
  }
  const srv = await startServer(DIST_DIR);
  cleanups.push(() => srv.close());
  const P = await nodePage(browser, `${srv.url}index.html?nohub=1&profile=lib`);
  cleanups.push(() => P.close());
  await checkLibrary(P);
  const list = await P.call('guides.list', { category: 'water', limit: 3 });
  assert.equal(list.guides.length, 3);

  const tools = await P.call('tools.list');
  assert.ok(tools.tools.find((x) => x.id === 'dipole'));
  const d = await P.call('tools.run', { id: 'dipole', inputs: { freq_mhz: 146 } });
  assert.ok(d.outputs.length > 0);
  // half-wave dipole at 146 MHz with a 0.95 end factor: 0.975 m tip to tip
  const total = d.outputs.find((o) => o.key === 'total');
  assert.ok(total && Math.abs(parseFloat(total.value) - 0.975) < 0.01, JSON.stringify(d.outputs));
  const ohm = await P.call('tools.run', { id: 'ohms_law', inputs: { v: 12, i: 2, r: 0, p: 0 } });
  assert.ok(ohm.outputs.some((o) => parseFloat(o.value) === 6), `R = 6 ohm: ${JSON.stringify(ohm.outputs)}`);
  await assert.rejects(P.call('tools.run', { id: 'no_such_tool', inputs: {} }), /not_found|bad_params/);

  const bands = await P.call('rf.bands', { region: 'US' });
  assert.ok(bands.services.length > 3);
  const frs = bands.services.find((s) => s.id === 'frs');
  assert.ok(frs && frs.channels.length >= 22, 'FRS channel plan');
  assert.equal(frs.channels[0].freq_hz, 462562500);
  assert.deepEqual(realErrors(P.errors), []);
});

function inlinedSingleFile() {
  const dist = path.join(DIST_DIR, 'embermesh.html');
  if (fs.statSync(dist).size > 6 * 1024 * 1024) return { file: dist, dir: null }; // already inlined
  const dir = tmpDir('ember-single-');
  execFileSync(process.execPath, [path.join(WEB_DIR, 'tools/package.mjs'), '--wasm', WASM_DIR, '--out', dir, '--guides', PACK, '--inline-guides'], { stdio: 'pipe' });
  return { file: path.join(dir, 'embermesh.html'), dir };
}

test('single file from file:// with the guide pack inlined: node, guide, loopback between two file:// tabs', { skip, timeout: 180000 }, async (t) => {
  if (!needBrowser(t)) return;
  const { file, dir } = inlinedSingleFile();
  if (dir) cleanups.push(() => fs.rmSync(dir, { recursive: true, force: true }));
  const sizeMb = fs.statSync(file).size / 1048576;
  t.diagnostic(`single file with guides: ${sizeMb.toFixed(1)} MiB`);
  const url = pathToFileURL(file).href;
  const A = await nodePage(browser, url, { timeoutMs: 60000 });
  cleanups.push(() => A.close());
  const host = await A.call('host.info');
  assert.equal(host.single_file, true);
  assert.equal(await A.evaluate('location.protocol'), 'file:');
  const info = await A.call('node.info');
  assert.match(info.node_id, /^[0-9a-f]{16}$/);
  await checkLibrary(A);
  const tool = await A.call('tools.run', { id: 'ohms_law', inputs: { v: 12, i: 2, r: 0, p: 0 } });
  assert.ok(tool.outputs.length > 0);

  // Second file:// tab with its own profile (own IndexedDB store), joined by loopback.
  const B = await nodePage(browser, `${url}?profile=fileb`, { timeoutMs: 60000 });
  cleanups.push(() => B.close());
  const bcOk = await A.evaluate(`typeof BroadcastChannel === 'function'`);
  assert.ok(bcOk, 'BroadcastChannel exists on file://');
  const channel = `f${Date.now()}`;
  for (const P of [A, B]) {
    if ((await P.call('node.info')).name === '') await P.call('node.set_profile', { name: P === A ? 'FileA' : 'FileB' });
    await P.call('host.link.create', { endpoint: { kind: 'loopback', config: { channel } }, link: { type: 'raw', config: {} } });
  }
  const text = `file tabs ${Date.now()}`;
  await A.call('messages.send', { conv: 'ch:public', text });
  let crossTab = true;
  try {
    await B.waitCall('messages.list', { conv: 'ch:public' }, (r) => r.messages.some((m) => m.text === text), 15000, 'public message between file:// tabs');
  } catch (e) {
    crossTab = false;
    // Chrome gives file:// pages opaque origins; if it ever stops delivering BroadcastChannel
    // messages between them, say so loudly instead of hiding it.
    assert.fail(`loopback between two file:// tabs did not deliver: ${e.message}`);
  }
  t.diagnostic(`loopback between file:// tabs: ${crossTab ? 'works' : 'blocked'}`);
  assert.deepEqual(realErrors(A.errors), []);
  assert.deepEqual(realErrors(B.errors), []);
});
