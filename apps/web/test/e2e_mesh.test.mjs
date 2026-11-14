// End-to-end mesh tests in real headless Chrome with the real WASM core (dist/web):
//   1. two tabs joined by the `loopback` (BroadcastChannel) endpoint: first run, DM with
//      delivered/read, Public channel, SOS, then persistence of both nodes across a reload
//   2. WebRTC: two pages paired with the manual offer/answer codes, messages over the data channel
//   3. hub: two guest pages joined through the desktop daemon (dist/desktop/emberd) when it is
//      built, otherwise through a small Node relay that mimics /hub.json + /ws
// Run after a build: node --test apps/web/test/   (EMBER_REQUIRE_BROWSER=1: never skip)
import { test, before, after } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { spawn } from 'node:child_process';
import net from 'node:net';
import { DIST_DIR, ROOT, ui } from './helpers.mjs';
import { e2eSkip, startBrowser, nodePage, realErrors, tmpDir, REQUIRE } from './lib/e2e.mjs';
import { runCleanups, HOOK } from './lib/e2e.mjs';
import { startServer } from './lib/static_server.mjs';

const skip = e2eSkip();
let browser;
const cleanups = [];

before(async () => {
  if (skip) return;
  browser = await startBrowser();
}, HOOK);
after(async () => {
  if (browser && browser.close) cleanups.unshift(() => browser.close()); // closed last
  await runCleanups(cleanups);
}, HOOK);
const needBrowser = (t) => {
  if (browser && browser.error) { t.skip(`Chrome could not start: ${browser.error}`); return false; }
  return true;
};

const dmConv = (id) => `dm:${id}`;
const findMsg = (r, text) => r.messages.find((m) => m.text === text);

/** Two nodes meet: profiles, contact cards exchanged. */
async function introduce(A, B, nameA, nameB) {
  const a0 = await A.call('node.info');
  const b0 = await B.call('node.info');
  if (a0.name === '') await A.call('node.set_profile', { name: nameA });
  if (b0.name === '') await B.call('node.set_profile', { name: nameB });
  const a = await A.call('node.info');
  const b = await B.call('node.info');
  await A.call('contacts.add', { card: b.card });
  await B.call('contacts.add', { card: a.card });
  return { a, b };
}

/** DM A->B: B receives it, A sees delivered, B marks read, A sees read. */
async function dmRoundTrip(A, B, a, b, text) {
  const { message } = await A.call('messages.send', { conv: dmConv(b.node_id), text });
  assert.equal(message.outgoing, true);
  const got = await B.waitCall('messages.list', { conv: dmConv(a.node_id) }, (r) => findMsg(r, text), 20000, `DM "${text}" at B`);
  const m = findMsg(got, text);
  assert.equal(m.from, a.node_id);
  assert.equal(m.encrypted, true);
  await A.waitCall('messages.list', { conv: dmConv(b.node_id) }, (r) => ['delivered', 'read'].includes((findMsg(r, text) || {}).status), 20000, 'delivered');
  await B.call('messages.mark_read', { conv: dmConv(a.node_id) });
  await A.waitCall('messages.list', { conv: dmConv(b.node_id) }, (r) => (findMsg(r, text) || {}).status === 'read', 20000, 'read receipt');
  return message.id;
}

async function publicRoundTrip(A, B, text) {
  await A.call('messages.send', { conv: 'ch:public', text });
  await B.waitCall('messages.list', { conv: 'ch:public' }, (r) => findMsg(r, text), 20000, `public "${text}"`);
}

test('loopback: two tabs — first run, DM delivered/read, Public, SOS, persistence across reload of both', { skip, timeout: 180000 }, async (t) => {
  if (!needBrowser(t)) return;
  const srv = await startServer(DIST_DIR);
  cleanups.push(() => srv.close());
  // Same origin, two storage profiles: each tab owns its own IndexedDB store and identity.
  const A = await nodePage(browser, `${srv.url}index.html?nohub=1`);
  const B = await nodePage(browser, `${srv.url}index.html?nohub=1&profile=b`);
  cleanups.push(() => A.close(), () => B.close());

  const ha = await A.call('host.info');
  const hb = await B.call('host.info');
  assert.equal(ha.storage, 'indexeddb', ha.storage_note);
  assert.equal(hb.storage, 'indexeddb', hb.storage_note);
  assert.equal(hb.profile, 'b');
  const lk = ha.endpoint_kinds.find((k) => k.kind === 'loopback');
  assert.ok(lk.available);
  assert.deepEqual(lk.link_types, ['raw'], 'link types come from core links.types');

  // first run: a fresh identity has an empty name
  assert.equal((await A.call('node.info')).name, '', 'first run: name is empty');
  assert.equal((await B.call('node.info')).name, '');
  const { a, b } = await introduce(A, B, 'Alice', 'Bob');
  assert.notEqual(a.node_id, b.node_id);

  const channel = `t${Date.now()}`;
  const la = (await A.call('host.link.create', { name: 'Tabs', endpoint: { kind: 'loopback', config: { channel } }, link: { type: 'raw', config: {} } })).link;
  const lb = (await B.call('host.link.create', { name: 'Tabs', endpoint: { kind: 'loopback', config: { channel } }, link: { type: 'raw', config: {} } })).link;
  assert.equal(la.persist, true);
  await A.waitCall('links.list', {}, (r) => r.links.find((l) => l.id === la.id && l.state === 'up'), 10000, 'link A up');
  await B.waitCall('links.list', {}, (r) => r.links.find((l) => l.id === lb.id && l.state === 'up'), 10000, 'link B up');

  await dmRoundTrip(A, B, a, b, 'hello bob, over the loopback');
  await publicRoundTrip(A, B, 'hello everyone on public');
  await publicRoundTrip(B, A, 'bob says hi on public');

  // SOS from B reaches A (event + sos.list + a kind:"sos" message in ch:public)
  await B.call('sos.start', { text: 'twisted ankle at the creek' });
  const ev = await A.waitEvent('sos.received', (d) => d.node_id === b.node_id, 20000);
  assert.equal(ev.text, 'twisted ankle at the creek');
  const alerts = await A.waitCall('sos.list', {}, (r) => r.alerts.find((x) => x.node_id === b.node_id));
  assert.ok(alerts.alerts.length >= 1);
  await A.waitCall('messages.list', { conv: 'ch:public' }, (r) => r.messages.some((m) => m.kind === 'sos' && m.from === b.node_id), 10000, 'sos message');
  await B.call('sos.stop');

  // settings on both
  await A.call('settings.set', { settings: { units: 'imperial', store_forward_days: 9 } });
  await B.call('settings.set', { settings: { theme: 'night' } });

  // Wait until the core has written its store and the IDBFS flush (storage.dirty -> syncfs) ran.
  await new Promise((r) => setTimeout(r, 6500));
  const dirtyA = await A.events('storage.dirty');
  assert.ok(dirtyA.length > 0, 'core emitted storage.dirty');

  // ---- reload both tabs
  await A.reloadNode();
  await B.reloadNode();
  const a2 = await A.call('node.info');
  const b2 = await B.call('node.info');
  assert.equal(a2.node_id, a.node_id, 'identity A survived reload');
  assert.equal(b2.node_id, b.node_id, 'identity B survived reload');
  assert.equal(a2.name, 'Alice');
  assert.equal(b2.name, 'Bob');
  assert.equal((await A.call('settings.get')).settings.units, 'imperial');
  assert.equal((await A.call('settings.get')).settings.store_forward_days, 9);
  assert.equal((await B.call('settings.get')).settings.theme, 'night');
  assert.deepEqual((await A.call('settings.get')).settings.links_autostart || [], [], 'no host-side saved-link mirror');
  const dmA = await A.call('messages.list', { conv: dmConv(b.node_id) });
  assert.equal(findMsg(dmA, 'hello bob, over the loopback').status, 'read', 'message + status survived');
  assert.ok(findMsg(await B.call('messages.list', { conv: 'ch:public' }), 'hello everyone on public'));
  assert.ok((await A.call('contacts.list')).contacts.find((c) => c.node_id === b.node_id), 'contact survived');

  // links: the core restored them (same ids) and the host reopened the endpoints from link.endpoint
  const linksA = await A.waitCall('links.list', {}, (r) => r.links.find((l) => l.id === la.id && l.state === 'up'), 10000, 'link A reopened');
  assert.equal(linksA.links.filter((l) => l.endpoint && l.endpoint.kind === 'loopback').length, 1, 'no duplicate link');
  assert.equal(linksA.links.find((l) => l.id === la.id).endpoint.config.channel, channel);
  await B.waitCall('links.list', {}, (r) => r.links.find((l) => l.id === lb.id && l.state === 'up'), 10000, 'link B reopened');
  await dmRoundTrip(B, A, b, a, 'reply after both reloaded');

  // host.link.remove: gone from the core and not reopened after another reload
  await A.call('host.link.remove', { id: la.id });
  await new Promise((r) => setTimeout(r, 2500));
  await A.reloadNode();
  const linksA3 = await A.call('links.list');
  assert.equal(linksA3.links.find((l) => l.id === la.id), undefined, 'removed link stays removed');

  assert.deepEqual(realErrors(A.errors), []);
  assert.deepEqual(realErrors(B.errors), []);
});

test('WebRTC: manual offer/answer pairing between two pages, messages over the data channel', { skip, timeout: 120000 }, async (t) => {
  if (!needBrowser(t)) return;
  const srv = await startServer(DIST_DIR);
  cleanups.push(() => srv.close());
  const A = await nodePage(browser, `${srv.url}index.html?nohub=1&profile=rtca`);
  const B = await nodePage(browser, `${srv.url}index.html?nohub=1&profile=rtcb`);
  cleanups.push(() => A.close(), () => B.close());
  // Capability probe (docs/CI.md): manual pairing carries only the host candidates gathered before
  // the code is shown — no trickle, no STUN. Can two peer connections in this browser connect that
  // way at all? (On some VMs the host candidates are mDNS names nobody resolves: ICE never starts.)
  const probe = await A.evaluate(`(async () => {
    const x = new RTCPeerConnection(), y = new RTCPeerConnection();
    const dc = x.createDataChannel('probe');
    const gather = (pc) => new Promise((r) => { if (pc.iceGatheringState === 'complete') return r(); pc.addEventListener('icegatheringstatechange', () => pc.iceGatheringState === 'complete' && r()); setTimeout(r, 8000); });
    await x.setLocalDescription(await x.createOffer()); await gather(x);
    await y.setRemoteDescription(x.localDescription); await y.setLocalDescription(await y.createAnswer()); await gather(y);
    await x.setRemoteDescription(y.localDescription);
    const ok = await new Promise((r) => { dc.onopen = () => r(true); setTimeout(() => r(false), 15000); });
    const out = { ok, ice: x.iceConnectionState, candidates: (x.localDescription.sdp.match(/a=candidate:\\S+ \\d \\S+ \\d+ \\S+/g) || []).map((c) => c.split(' ').pop().replace(/^[0-9a-f-]{36}\\.local$/, '<mdns>')) };
    x.close(); y.close();
    return out;
  })()`, 60000);
  t.diagnostic(`WebRTC capability probe: ${JSON.stringify(probe)}`);
  if (!probe.ok) {
    t.skip(`probe: two peer connections in this browser cannot connect with host candidates only (ICE ${probe.ice}, candidates ${JSON.stringify(probe.candidates)})`);
    return;
  }
  const { a, b } = await introduce(A, B, 'Ann', 'Ben');

  const { pairing_id, offer } = await A.call('host.webrtc.create_offer', { name: 'Ben' });
  assert.match(offer, /^ember:rtc:/);
  assert.ok(offer.length < 400, `offer is QR-sized (${offer.length} chars)`);
  const acc = await B.call('host.webrtc.accept_offer', { offer, name: 'Ann' });
  assert.match(acc.answer, /^ember:rtc:/);
  const { link } = await A.call('host.webrtc.accept_answer', { pairing_id, answer: acc.answer });
  assert.equal(link.persist, false, 'WebRTC links are session-only (a restart needs a new pairing)');
  const { decodeBlob } = await import(ui('endpoints/webrtc_blob.js'));
  const cands = (blob) => decodeBlob(blob).candidates.map((c) => `${c.type}/${c.tcp ? 'tcp' : 'udp'}/${/\.local$/.test(c.address) ? 'mdns' : c.address}:${c.port}`);
  t.diagnostic(`offer candidates ${JSON.stringify(cands(offer))}, answer candidates ${JSON.stringify(cands(acc.answer))}`);
  const pcState = (id) => `(() => { const e = __emberWasmBackend.router.links.get(${id}); const pc = e && e.endpoint && e.endpoint.pc; const dc = e && e.endpoint && e.endpoint.dc;
    return pc ? { ice: pc.iceConnectionState, conn: pc.connectionState, gathering: pc.iceGatheringState, dc: dc && dc.readyState, local: (pc.localDescription.sdp.match(/a=candidate/g) || []).length } : null; })()`;
  try {
    await A.waitCall('links.list', {}, (r) => r.links.find((l) => l.id === link.id && l.state === 'up'), 45000, 'webrtc link A up');
    await B.waitCall('links.list', {}, (r) => r.links.find((l) => l.id === acc.link.id && l.state === 'up'), 45000, 'webrtc link B up');
  } catch (e) {
    // the next CI log must say why: candidates in both codes and both peer connections' states
    throw new Error(`${e.message.split('\n')[0]}\noffer candidates ${JSON.stringify(cands(offer))}\nanswer candidates ${JSON.stringify(cands(acc.answer))}\nA ${JSON.stringify(await A.evaluate(pcState(link.id)))}\nB ${JSON.stringify(await B.evaluate(pcState(acc.link.id)))}`);
  }

  await dmRoundTrip(A, B, a, b, 'hello over a WebRTC data channel');
  await publicRoundTrip(B, A, 'public over WebRTC');
  const st = (await A.call('links.list')).links.find((l) => l.id === link.id).stats;
  assert.ok(st.rx_frames > 0 && st.tx_frames > 0, `frames both ways: ${JSON.stringify(st)}`);
  assert.deepEqual(realErrors(A.errors), []);
  assert.deepEqual(realErrors(B.errors), []);
});

// ------------------------------------------------------------------------------------------ hub

function freePort() {
  return new Promise((resolve, reject) => {
    const s = net.createServer();
    s.listen(0, '127.0.0.1', () => { const { port } = s.address(); s.close(() => resolve(port)); });
    s.on('error', reject);
  });
}

async function waitHttp(url, ms) {
  const t0 = Date.now();
  for (;;) {
    try { const r = await fetch(url, { cache: 'no-store' }); if (r.ok) return await r.json(); } catch {}
    if (Date.now() - t0 > ms) throw new Error(`no answer from ${url}`);
    await new Promise((r) => setTimeout(r, 200));
  }
}

/** Starts dist/desktop/emberd on a spare port with a temp data dir; null if not built/usable. */
async function startEmberd() {
  const bin = process.env.EMBER_EMBERD || path.join(ROOT, 'dist/desktop/emberd');
  if (!fs.existsSync(bin) || process.env.EMBER_HUB === 'relay') return null;
  const port = await freePort();
  const dir = tmpDir('emberd-');
  const proc = spawn(bin, ['--data-dir', dir, '--web-root', DIST_DIR, '--port', String(port), '--bind', '127.0.0.1',
    '--no-udp', '--name', 'Test hub', '--log-level', 'warn'], { stdio: ['ignore', 'pipe', 'pipe'] });
  let log = '';
  proc.stdout.on('data', (d) => { log += d; });
  proc.stderr.on('data', (d) => { log += d; });
  const stop = async () => {
    if (proc.exitCode === null) {
      proc.kill('SIGTERM');
      await new Promise((r) => { const t = setTimeout(() => { proc.kill('SIGKILL'); r(); }, 5000); proc.once('exit', () => { clearTimeout(t); r(); }); });
    }
    fs.rmSync(dir, { recursive: true, force: true });
  };
  try {
    const info = await waitHttp(`http://127.0.0.1:${port}/hub.json`, 15000);
    if (info.embermesh_hub !== true) throw new Error(`hub.json: ${JSON.stringify(info)}`);
    return { kind: 'emberd', url: `http://127.0.0.1:${port}/`, port, stop, log: () => log };
  } catch (e) {
    await stop();
    if (REQUIRE && process.env.EMBER_HUB === 'emberd') throw e;
    console.warn(`emberd not usable (${e.message}); falling back to the Node relay. Log:\n${log.slice(-800)}`);
    return null;
  }
}

/** Node relay: dist/web static files + hub.json + /ws that forwards each binary message to the others. */
async function startRelay() {
  const srv = await startServer(DIST_DIR, { hub: { name: 'Relay hub' }, relay: true });
  return { kind: 'node-relay', url: srv.url, port: srv.port, stop: () => srv.close(), log: () => '' };
}

test('hub: two guest pages exchange messages through the hub /ws (emberd if built, else a Node relay)', { skip, timeout: 120000 }, async (t) => {
  if (!needBrowser(t)) return;
  const hub = (await startEmberd()) || (await startRelay());
  cleanups.push(() => hub.stop());
  t.diagnostic(`hub used: ${hub.kind}`);
  console.log(`# hub used: ${hub.kind}`);
  const A = await nodePage(browser, `${hub.url}index.html?profile=huba`);
  const B = await nodePage(browser, `${hub.url}index.html?profile=hubb`);
  cleanups.push(() => A.close(), () => B.close());
  for (const P of [A, B]) {
    const h = await P.waitCall('host.info', {}, (r) => r.hub && r.hub.connected, 20000, 'hub connected');
    assert.equal(h.hub.url, `ws://127.0.0.1:${hub.port}/ws`);
    const l = (await P.call('links.list')).links.find((x) => x.id === h.hub.link_id);
    assert.equal(l.type, 'raw');
    assert.equal(l.persist, false, 'hub link is re-detected on every load, not persisted');
  }
  const { a, b } = await introduce(A, B, 'Guest A', 'Guest B');
  await dmRoundTrip(A, B, a, b, 'hello via the hub');
  await publicRoundTrip(B, A, 'public via the hub');
  // reload: the hub link is re-added (not duplicated) and works again
  await A.reloadNode();
  const h2 = await A.waitCall('host.info', {}, (r) => r.hub && r.hub.connected, 20000, 'hub reconnected');
  const hubLinks = (await A.call('links.list')).links.filter((l) => l.endpoint && l.endpoint.kind === 'websocket');
  assert.equal(hubLinks.length, 1, 'one hub link after reload');
  assert.equal(hubLinks[0].id, h2.hub.link_id);
  await publicRoundTrip(A, B, 'public via the hub after reload');
  assert.deepEqual(realErrors(A.errors), []);
  assert.deepEqual(realErrors(B.errors), []);
});
