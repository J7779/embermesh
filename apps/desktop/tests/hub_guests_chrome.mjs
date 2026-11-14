#!/usr/bin/env node
// Hub end-to-end with real browsers: starts emberd serving dist/web, opens two headless-Chrome guest
// pages (separate browser contexts = separate storage = separate WASM nodes) at
// http://127.0.0.1:<port>/, lets them auto-join through /hub.json + /ws, completes the first-run
// screens through the DOM, then exchanges Public messages and DMs guest<->guest and guest<->hub
// node, checks the operator page (?remote=1&token=…) against the daemon's real core, and that a
// guest can fetch the single-file app from /download/.
//
//   node apps/desktop/tests/hub_guests_chrome.mjs [--emberd PATH] [--web DIR] [--chrome PATH] [--keep]
//
// No npm dependencies: Chrome DevTools Protocol over Node's built-in WebSocket (Node >= 22).
// Exit code 0 = everything passed. Chrome must be able to launch (it will not in some locked-down CI containers).
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { sleep, freePort, waitFor, findChrome, launchChrome, firstRun as firstRunWith } from './chrome_guest.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const repo = path.resolve(here, '../../..');
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const keep = process.argv.includes('--keep');
const emberd = arg('--emberd', path.join(repo, 'build/desktop/apps/desktop/emberd'));
const web = arg('--web', path.join(repo, 'dist/web'));
const chrome = findChrome(arg('--chrome'));
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'ember-guests-'));
const procs = [];
var finishing = false;
let failures = 0;
let browser = null;
process.on('uncaughtException', e => { console.log('FAIL exception: ' + (e && e.stack || e)); failures++; finish(); });
process.on('unhandledRejection', e => { console.log('FAIL exception: ' + (e && e.stack || e)); failures++; finish(); });
const ok = (cond, what) => { console.log(`${cond ? 'ok  ' : 'FAIL'} ${what}`); if (!cond) failures++; return cond; };

// ---- emberd ---------------------------------------------------------------------------------------
const port = await freePort();
const token = 'tok-' + Math.random().toString(16).slice(2);
const dlog = fs.openSync(path.join(tmp, 'emberd.log'), 'w');
const d = spawn(emberd, ['--data-dir', path.join(tmp, 'hub'), '--port', String(port), '--bind', '127.0.0.1', '--token', token,
  '--no-udp', '--no-qr', '--name', 'test-hub', '--web-root', web], { stdio: ['ignore', dlog, dlog] });
procs.push(d);
const rpc = async (method, params = {}) => {
  const r = await fetch(`http://127.0.0.1:${port}/rpc`, { method: 'POST', headers: { authorization: 'Bearer ' + token }, body: JSON.stringify({ method, params }) });
  const j = await r.json();
  if (!j.ok) throw new Error(`${method}: ${j.error && j.error.message}`);
  return j.result;
};
ok(await waitFor(async () => (await fetch(`http://127.0.0.1:${port}/hub.json`)).ok, 15000), 'emberd hub is up');
const hubInfo = await rpc('node.info');
console.log(`     hub node ${hubInfo.node_id}`);

// ---- Chrome + CDP (apps/desktop/tests/chrome_guest.mjs) -------------------------------------------
browser = await launchChrome({ chrome, dir: tmp, procs });
if (!ok(!!browser, 'headless Chrome started')) await finish();
const { openPage, consoleLines } = browser;
const firstRun = (page, name) => firstRunWith(page, name, ok, consoleLines);

const operatorOnly = process.argv.includes('--operator-only');
const t = Math.random().toString(16).slice(2, 8);
let g1 = null, g2 = null;
var has = async (call, conv, text) => ((await call('messages.list', { conv, limit: 200 })).messages || []).find(m => m.text === text);
const hubHas = (conv, text) => has(rpc, conv, text);
if (!operatorOnly) {
g1 = await openPage(`http://127.0.0.1:${port}/`);
g2 = await openPage(`http://127.0.0.1:${port}/`);
const i1 = await firstRun(g1, 'Guest One');
const i2 = await firstRun(g2, 'Guest Two');
if (!i1 || !i2) await finish();
ok(i1.node_id !== i2.node_id, 'the two guests are separate nodes');
for (const [g, n] of [[g1, 'Guest One'], [g2, 'Guest Two']]) {
  const l = await waitFor(async () => (await g.call('links.list')).links.find(x => x.endpoint && x.endpoint.kind === 'websocket' && x.state === 'up'), 15000);
  ok(!!l, `${n}: hub link up (${l && l.endpoint.config && l.endpoint.config.url}, type ${l && l.type})`);
}
ok(await waitFor(async () => (await rpc('host.hub.status')).clients >= 2, 10000), 'hub counts 2 guests');


// Public guest -> everyone
const p1 = `public from guest one ${t}`;
await g1.call('messages.send', { conv: 'ch:public', text: p1 });
ok(await waitFor(() => has(g2.call, 'ch:public', p1), 20000), 'guest two received guest one\'s Public message');
ok(await waitFor(() => hubHas('ch:public', p1), 20000), 'hub node received guest one\'s Public message');
// Public hub -> guests
const p2 = `public from hub ${t}`;
await rpc('messages.send', { conv: 'ch:public', text: p2 });
ok(await waitFor(() => has(g1.call, 'ch:public', p2), 20000) && await waitFor(() => has(g2.call, 'ch:public', p2), 20000), 'both guests received the hub\'s Public message');
// DMs with delivered receipts
async function dm(fromCall, toId, convAtTarget, toCall, label) {
  const text = `dm ${label} ${t}`;
  await fromCall('messages.send', { conv: 'dm:' + toId, text });
  const got = await waitFor(() => has(toCall, convAtTarget, text), 30000);
  const st = await waitFor(async () => { const m = await has(fromCall, 'dm:' + toId, text); return m && (m.status === 'delivered' || m.status === 'read') && m.status; }, 30000);
  ok(!!got && !!st && got.encrypted, `DM ${label}: received (encrypted=${got && got.encrypted}), sender status ${st}`);
}
await dm(g1.call, i2.node_id, 'dm:' + i1.node_id, g2.call, 'guest1->guest2');
await dm(g2.call, hubInfo.node_id, 'dm:' + i2.node_id, rpc, 'guest2->hub');
await dm(rpc, i1.node_id, 'dm:' + hubInfo.node_id, g1.call, 'hub->guest1');
// UI shows it: the Public conversation is listed in Chats.
ok(await g2.ev(`(async () => { location.hash = '#/chats'; await new Promise(r => setTimeout(r, 800)); return document.body.innerText.includes('Public'); })()`), 'guest UI renders the chat list');

}
// ---- operator page against the real core ----------------------------------------------------------
const op = await openPage(`http://127.0.0.1:${port}/?remote=1&token=${token}`);
const opInfo = await waitFor(() => op.ev(`window.__ember && window.__ember.api.call('node.info')`), 20000);
ok(opInfo && opInfo.node_id === hubInfo.node_id, `operator page controls the hub node (${opInfo && opInfo.node_id})`);
const hi = await op.call('host.info');
ok(hi.backend === 'remote' && hi.platform === 'desktop', `operator host.info backend=${hi.backend} platform=${hi.platform}`);
const ol = await op.call('links.list');
ok(ol.links.some(l => l.endpoint.kind === 'ws_server' && l.state === 'up'), `operator sees the hub link (${ol.links.length} links)`);
const hs = await op.call('host.hub.status');
ok(hs.running && hs.clients >= (g1 ? 2 : 0), `operator host.hub.status: running, ${hs.clients} guests`);
const p3 = `operator says hi ${t}`;
await op.call('messages.send', { conv: 'ch:public', text: p3 });
if (g1) ok(await waitFor(() => has(g1.call, 'ch:public', p3), 20000), 'message sent from the operator page reached a guest');
else ok(!!(await hubHas('ch:public', p3)), 'message sent from the operator page is in the hub node');
ok(await op.ev(`(async () => { location.hash = '#/more'; await new Promise(r => setTimeout(r, 800)); return !document.body.innerText.includes('could not start'); })()`), 'operator UI renders');

// ---- /download/ ---------------------------------------------------------------------------------------
const listing = await (await fetch(`http://127.0.0.1:${port}/download/?format=json`)).json();
ok(listing.files.some(f => f.name === 'embermesh.html'), `/download/ lists embermesh.html (${listing.files.map(f => f.name).join(', ')})`);
const size = await (g2 || op).ev(`fetch('/download/embermesh.html').then(r => r.ok ? r.arrayBuffer() : null).then(b => b ? b.byteLength : 0)`);
ok(size > 100000, `a guest fetched the single-file app from /download/ (${size} bytes)`);

for (const [n, g] of [['guest1', g1], ['guest2', g2], ['operator', op]].filter(x => x[1])) {
  const l = consoleLines.get(g.sessionId) || [];
  if (l.length) console.log(`     ${n} console errors: ${l.slice(0, 3).join(' | ')}`);
}
await finish();

async function finish() {
  if (finishing) return;
  finishing = true;
  try { browser.ws.close(); } catch {}
  for (const p of procs) { try { p.kill('SIGTERM'); } catch {} }
  await sleep(1500);
  for (const p of procs) { try { p.kill('SIGKILL'); } catch {} }
  console.log(failures ? `\n${failures} check(s) FAILED (logs in ${tmp})` : '\nall hub guest checks passed');
  if (!keep && !failures) fs.rmSync(tmp, { recursive: true, force: true });
  process.exit(failures ? 1 : 0);
}
