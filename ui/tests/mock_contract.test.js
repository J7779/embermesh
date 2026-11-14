// Contract checks: the mock backend implements every method in docs/RPC_API.md §2/§3,
// returns plain objects, and only emits events listed in §4.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import MockBackend from '../js/backends/mock.js';

const spec = readFileSync(fileURLToPath(new URL('../../docs/RPC_API.md', import.meta.url)), 'utf8');

function section(from, to) {
  const a = spec.indexOf(from), b = to ? spec.indexOf(to, a + 1) : spec.length;
  assert.ok(a >= 0, `spec section ${from} missing`);
  return spec.slice(a, b < 0 ? spec.length : b);
}
function firstColumnNames(text, header) {
  const names = [];
  let inTable = false;
  for (const line of text.split('\n')) {
    const m = /^\|([^|]*)\|/.exec(line);
    if (!m) { inTable = false; continue; }
    // only tables whose first header cell is `header` (skips e.g. the audio status field table)
    if (/^\|\s*[A-Z]/.test(line) && !/^\|[-\s|:]+$/.test(line)) { inTable = !header || m[1].trim() === header; continue; }
    if (!inTable) continue;
    for (const t of m[1].matchAll(/`([a-z_][a-z0-9_.]*)`/g)) names.push(t[1]);
  }
  return names;
}
const METHODS = firstColumnNames(section('## 2.', '## 4.'), 'Method');
const EVENTS = new Set(firstColumnNames(section('## 4.', '## 5.'), 'Event'));

const sleep = ms => new Promise(r => setTimeout(r, ms));
const fresh = (extra = {}) => new MockBackend({ persist: false, speed: 0.001, autoplay: false, latency: 0, ...extra });

async function waitFor(pred, ms = 2000) {
  const end = Date.now() + ms;
  while (Date.now() < end) { if (pred()) return true; await sleep(5); }
  return pred();
}

test('spec parsing found the method and event tables', () => {
  assert.ok(METHODS.length > 90, `only ${METHODS.length} methods parsed`);
  for (const m of ['node.info', 'host.hub.stop', 'host.hub.status', 'tools.qr', 'security.wipe', 'host.keep_awake']) assert.ok(METHODS.includes(m), m);
  assert.ok(EVENTS.has('message.new') && EVENTS.has('host.location'));
});

test('every spec method is implemented and returns a plain object', async () => {
  const b = fresh();
  const emitted = new Set();
  b.on('*', ev => emitted.add(ev));
  await b.init();
  const PEER = 'b7e4c1d2a9f03e56';
  const card = (await b.call('host.qr.scan')).text;
  const offer = (await b.call('host.webrtc.create_offer', { name: 't' }));
  const backup = (await b.call('identity.export', { passphrase: 'pw' })).backup;
  const bundle = (await b.call('bundle.export', { scope: 'all' })).data_b64;
  const firstMsg = (await b.call('messages.list', { conv: 'ch:public' })).messages.at(-1);
  const fileMsg = (await b.call('messages.search', { query: 'netplan-week46' })).messages[0];
  const created = await b.call('channels.create', { name: 'Test', passphrase: 'x' });
  const params = {
    'node.set_profile': { name: 'Tester', callsign: 'n0call' },
    'node.set_time': { ts: Date.now(), source: 'manual' },
    'identity.export': { passphrase: 'pw' },
    'identity.import': { backup, passphrase: 'pw' },
    'contacts.add': { card },
    'contacts.update': { node_id: PEER, favorite: false },
    'contacts.remove': { node_id: '9d2e7f4a1b3c5e60' },
    'channels.create': { name: 'Another' },
    'channels.invite': { id: '5e1d9a70' },
    'channels.join': { name: 'Joined', passphrase: 'p' },
    'channels.update': { id: '5e1d9a70', muted: true },
    'channels.leave': { id: created.channel.id },
    'conversations.clear': { conv: 'dm:e8a2b5c4d3f10967' },
    'messages.list': { conv: 'ch:public' },
    'messages.send': { conv: `dm:${PEER}`, text: 'hello' },
    'messages.retry': { id: firstMsg.id },
    'messages.mark_read': { conv: 'ch:public' },
    'messages.delete': { id: firstMsg.id },
    'messages.search': { query: 'water' },
    'files.send': { conv: `dm:${PEER}`, name: 'a.txt', mime: 'text/plain', data_b64: 'aGVsbG8=' },
    'files.get': { id: fileMsg.id },
    'interop.send': { link_id: 2, to: 'APRS', text: 'hi' },
    'sos.start': { text: 'help' },
    'sos.ack': { node_id: 'f02c6d9e8b1a4573' },
    'checkin.send': { status: 'ok', note: 'fine' },
    'position.set': { lat: 40, lon: -98, ts: Date.now() },
    'position.share': { conv: 'ch:public' },
    'bulletins.list': { category: 'water' },
    'bulletins.post': { category: 'info', title: 'T', text: 'x', expires_s: 3600 },
    'bulletins.delete': { id: 'b00100000000000000000006' },
    'mesh.ping': { node_id: PEER },
    'netplan.set': { entries: [{ label: 'Net', freq_hz: 146520000, mode: 'voice' }] },
    'links.add': { type: 'raw', name: 'L', config: {}, endpoint: { kind: 'loopback' } },
    'links.configure': { id: 1, name: 'LAN' },
    'links.remove': { id: 4 },
    'links.endpoint_state': { id: 1, connected: true },
    'guides.list': { category: 'radio' },
    'guides.get': { id: 'radio/handheld-basics-2' },
    'guides.search': { query: 'boil' },
    'tools.run': { id: 'dipole', inputs: { freq_mhz: 146.52 } },
    'rf.bands': { region: 'US' },
    'tools.qr': { text: 'hello', ecc: 'M' },
    'bundle.export': { scope: 'all' },
    'bundle.import': { data_b64: bundle },
    'settings.set': { settings: { units: 'imperial' } },
    'kv.get': { ns: 'notes', key: 'a' },
    'kv.set': { ns: 'notes', key: 'a', value: { x: 1 } },
    'kv.delete': { ns: 'notes', key: 'a' },
    'kv.list': { ns: 'notes' },
    'security.set_passphrase': { new: 'secret' },
    'security.unlock': { passphrase: 'secret' },
    'security.set_duress': { passphrase: 'duress' },
    'security.wipe': { confirm: 'ERASE' },
    'host.endpoints.scan': { kind: 'ble', timeout_ms: 100 },
    'host.link.create': { name: 'H', endpoint: { kind: 'tcp', config: { host: '127.0.0.1', port: 8001 } }, link: { type: 'kiss', config: { callsign: 'N0CALL' } } },
    'host.link.remove': { id: 5 },
    'host.link.reconnect': { id: 1 },
    'host.hub.start': { port: 8437 },
    'host.webrtc.create_offer': { name: 'x' },
    'host.webrtc.accept_offer': { offer: offer.offer },
    'host.webrtc.accept_answer': { pairing_id: offer.pairing_id, answer: 'ember:webrtc?a=e30=' },
    'host.location.watch': { enabled: false },
    'host.file.save': { name: 'x.txt', mime: 'text/plain', data_b64: 'eA==' },
    'host.file.pick': { accept: '.txt' },
    'host.signal.flash': { pattern_ms: [100, 100, 300] },
    'host.signal.tone': { pattern_ms: [100], freq_hz: 700 },
    'host.notify': { title: 't', body: 'b' },
    'host.keep_awake': { enabled: true },
  };
  // Run destructive/security calls last, in a sensible order.
  const order = [...METHODS].sort((a, b) => rank(a) - rank(b));
  function rank(m) {
    return { 'security.lock': 2, 'security.unlock': 3, 'security.wipe': 4 }[m] || (m.startsWith('security.') ? 1 : 0);
  }
  const origInfo = console.info; console.info = () => {};
  try {
    for (const m of order) {
      let res;
      try {
        res = await b.call(m, params[m] || {});
      } catch (e) {
        assert.fail(`${m} rejected: ${e.code} ${e.message}`);
      }
      assert.equal(typeof res, 'object', m);
      assert.ok(res !== null && !Array.isArray(res), `${m} must return an object`);
    }
  } finally { console.info = origInfo; }
  await sleep(50);
  for (const ev of emitted) assert.ok(EVENTS.has(ev), `mock emitted undocumented event ${ev}`);
  b.close();
});

test('unknown methods reject with not_implemented / unsupported', async () => {
  const b = fresh();
  await assert.rejects(b.call('nope.nothing'), e => e.code === 'not_implemented');
  await assert.rejects(b.call('host.teleport'), e => e.code === 'unsupported');
  b.close();
});

test('unsupported= query param disables methods', async () => {
  const b = fresh({ params: new URLSearchParams('unsupported=host.hub.start,guides.search') });
  await assert.rejects(b.call('host.hub.start'), e => e.code === 'unsupported');
  await assert.rejects(b.call('guides.search', { query: 'x' }), e => e.code === 'not_implemented');
  b.close();
});

test('messages.send progresses through statuses and a reply arrives', async () => {
  const b = fresh();
  const statuses = [];
  const incoming = [];
  b.on('message.status', d => statuses.push(d.status));
  b.on('message.new', d => { if (!d.message.outgoing) incoming.push(d.message); });
  await b.init();
  const { message } = await b.call('messages.send', { conv: 'dm:b7e4c1d2a9f03e56', text: 'ping', priority: 'high' });
  assert.equal(message.status, 'queued');
  assert.equal(message.encrypted, true);
  assert.ok(await waitFor(() => statuses.includes('read') && incoming.length > 0));
  assert.deepEqual(statuses.slice(0, 4), ['sent', 'relayed', 'delivered', 'read']);
  const list = await b.call('messages.list', { conv: 'dm:b7e4c1d2a9f03e56' });
  assert.ok(list.messages.some(m => m.id === message.id && m.status === 'read'));
  // oldest first
  for (let i = 1; i < list.messages.length; i++) assert.ok(list.messages[i - 1].ts <= list.messages[i].ts);
  b.close();
});

test('failing send ends in failed and retry recovers', async () => {
  const b = fresh();
  const seen = [];
  b.on('message.status', d => seen.push(d.status));
  const { message } = await b.call('messages.send', { conv: 'ch:public', text: 'this will fail' });
  assert.equal(message.encrypted, false);
  assert.ok(await waitFor(() => seen.includes('failed')));
  await b.call('messages.retry', { id: message.id });
  assert.ok(await waitFor(() => seen.lastIndexOf('relayed') > seen.indexOf('failed')));
  b.close();
});

test('messages.list paging with before_ts', async () => {
  const b = fresh();
  const all = (await b.call('messages.list', { conv: 'ch:public' })).messages;
  const page = await b.call('messages.list', { conv: 'ch:public', limit: 3 });
  assert.equal(page.messages.length, 3);
  assert.equal(page.more, true);
  const older = await b.call('messages.list', { conv: 'ch:public', before_ts: page.messages[0].ts, limit: 100 });
  assert.equal(older.more, false);
  assert.equal(older.messages.length + 3, all.length);
  b.close();
});

test('locked mode rejects with locked and unlocks with passphrase', async () => {
  const b = fresh({ params: { locked: '1' } });
  assert.equal((await b.call('node.info')).locked, true);
  await assert.rejects(b.call('contacts.list'), e => e.code === 'locked');
  assert.ok(Array.isArray((await b.call('host.info')).endpoint_kinds));
  await assert.rejects(b.call('security.unlock', { passphrase: 'wrong' }), e => e.code === 'denied');
  assert.deepEqual(await b.call('security.unlock', { passphrase: 'demo' }), { unlocked: true });
  assert.ok((await b.call('contacts.list')).contacts.length > 0);
  b.close();
});

test('duress passphrase silently wipes', async () => {
  const b = fresh({ params: { locked: '1' } });
  await b.call('security.unlock', { passphrase: 'demo' });
  await b.call('security.set_duress', { passphrase: 'panic' });
  await b.call('security.lock');
  assert.deepEqual(await b.call('security.unlock', { passphrase: 'panic' }), { unlocked: true });
  assert.equal((await b.call('contacts.list')).contacts.length, 0);
  assert.equal((await b.call('node.info')).name, 'Alex', 'duress unlock keeps the profile (looks normal)');
  assert.equal((await b.call('security.status')).has_duress, true, 'phantom duress slot');
  b.close();
});

test('wipe requires confirm', async () => {
  const b = fresh();
  await assert.rejects(b.call('security.wipe', {}), e => e.code === 'bad_params');
  await assert.rejects(b.call('security.wipe', { confirm: 'erase' }), e => e.code === 'bad_params');
  await b.call('security.wipe', { confirm: 'ERASE' });
  assert.equal((await b.call('contacts.list')).contacts.length, 0);
  b.close();
});

test('kv round trip', async () => {
  const b = fresh();
  assert.deepEqual(await b.call('kv.get', { ns: 'checklists', key: 'bag' }), { value: null });
  await b.call('kv.set', { ns: 'checklists', key: 'bag', value: { items: ['water', 'radio'] } });
  await b.call('kv.set', { ns: 'checklists', key: 'comms', value: 1 });
  await b.call('kv.set', { ns: 'other', key: 'bag', value: 2 });
  assert.deepEqual((await b.call('kv.get', { ns: 'checklists', key: 'bag' })).value, { items: ['water', 'radio'] });
  assert.deepEqual((await b.call('kv.list', { ns: 'checklists' })).keys, ['bag', 'comms']);
  assert.deepEqual((await b.call('kv.list', { ns: 'checklists', prefix: 'co' })).keys, ['comms']);
  await b.call('kv.delete', { ns: 'checklists', key: 'bag' });
  assert.equal((await b.call('kv.get', { ns: 'checklists', key: 'bag' })).value, null);
  b.close();
});

test('results are copies (UI cannot mutate mock state)', async () => {
  const b = fresh();
  const a = await b.call('settings.get');
  a.settings.theme = 'hacked';
  assert.equal((await b.call('settings.get')).settings.theme, 'dark');
  b.close();
});

test('fresh=1 starts with an empty name', async () => {
  const b = fresh({ params: { fresh: '1' } });
  const info = await b.call('node.info');
  assert.equal(info.name, '');
  assert.equal(info.node_id.length, 16);
  assert.equal(info.fingerprint.length, 8);
  assert.ok(info.card.startsWith('ember:'));
  b.close();
});

test('guides: categories, books, search', async () => {
  const b = fresh();
  const cats = await b.call('guides.categories');
  assert.ok(cats.total >= 8 && cats.categories.some(c => c.id === 'radio'));
  const book = await b.call('guides.list', { part_of: 'radio/handheld-basics' });
  assert.deepEqual(book.guides.map(g => g.order), [1, 2, 3]);
  const ch2 = await b.call('guides.get', { id: 'radio/handheld-basics-2' });
  assert.equal(ch2.prev, 'radio/handheld-basics-1');
  assert.equal(ch2.next, 'radio/handheld-basics-3');
  assert.ok(ch2.markdown.includes('Prowords') && !ch2.markdown.startsWith('---'));
  for (const k of ['license', 'source', 'attribution', 'source_title', 'bytes']) assert.ok(ch2[k] !== undefined, k);
  const res = await b.call('guides.search', { query: 'Tourniquet' });
  assert.equal(res.results[0].id, 'medical/bleeding-control');
  assert.deepEqual(res.results[0].terms, ['tourniquet']);
  assert.ok(res.results[0].snippet.toLowerCase().includes('tourniquet'));
  const prefix = await b.call('guides.search', { query: 'purif' });
  assert.ok(prefix.results.some(r => r.id === 'water/purification-boiling'));
  assert.deepEqual((await b.call('guides.search', { query: 'zzzqqq' })).results, []);
  await assert.rejects(b.call('guides.get', { id: 'nope' }), e => e.code === 'not_found');
  b.close();
});

test('tools: link budget numbers and schema keys', async () => {
  const b = fresh();
  const { tools } = await b.call('tools.list');
  const lb = tools.find(t => t.id === 'link_budget');
  assert.deepEqual(lb.inputs.map(f => f.key), ['freq_mhz', 'distance_km', 'tx_power_w', 'tx_gain_dbi', 'tx_cable_loss_db', 'tx_height_m', 'rx_gain_dbi', 'rx_cable_loss_db', 'rx_height_m', 'rx_sensitivity_dbm']);
  const out = await b.call('tools.run', { id: 'link_budget', inputs: { freq_mhz: 146.52, distance_km: 10, tx_power_w: 5, tx_gain_dbi: 0, tx_cable_loss_db: 0, rx_gain_dbi: 0, rx_cable_loss_db: 0, tx_height_m: 10, rx_height_m: 2, rx_sensitivity_dbm: -120 } });
  const v = Object.fromEntries(out.outputs.map(o => [o.key, o.value]));
  for (const o of out.outputs) assert.equal(typeof o.value, 'number', o.key);
  assert.ok(Math.abs(v.fspl_db - 95.8) < 0.2, `fspl ${v.fspl_db}`);
  assert.ok(Math.abs(v.rx_dbm - (37 - 95.8)) < 0.3);
  assert.equal(typeof out.text, 'string');
  const morse = await b.call('tools.run', { id: 'morse', inputs: { text: 'SOS' } });
  assert.equal(morse.text, '... --- ...');
  await assert.rejects(b.call('tools.run', { id: 'dipole', inputs: { freq_mhz: 'abc' } }), e => e.code === 'bad_params');
  const bands = await b.call('rf.bands', { region: 'EU' });
  assert.ok(bands.services.some(s => s.id === 'pmr446') && !bands.services.some(s => s.id === 'frs'));
  b.close();
});

test('tools.qr returns a square matrix', async () => {
  const b = fresh();
  const q = await b.call('tools.qr', { text: 'ember:card?v=1&id=a1b2c3d4e5f60718&n=Alex' });
  assert.equal(q.rows.length, q.size);
  assert.ok(q.rows.every(r => r.length === q.size && /^[01]+$/.test(r)));
  assert.equal((q.size - 17) % 4, 0);
  // finder pattern top-left
  assert.equal(q.rows[0].slice(0, 7), '1111111');
  assert.equal(q.rows[1].slice(0, 7), '1000001');
  await assert.rejects(b.call('tools.qr', { text: 'x'.repeat(4000), ecc: 'H' }), e => e.code === 'too_large');
  b.close();
});

test('links: host.link.create comes up, ham links forbid encryption', async () => {
  const b = fresh();
  const states = [];
  b.on('link.state', d => states.push(d.link.state));
  await b.init();
  const { links } = await b.call('links.list');
  const radio = links.find(l => l.type === 'audio');
  assert.equal(radio.caps.encryption_allowed, false);
  assert.equal(radio.caps.requires_station_id, true);
  const { link } = await b.call('host.link.create', { endpoint: { kind: 'serial', config: { port: '/dev/ttyUSB0' } }, link: { type: 'kiss', config: { callsign: 'N0CALL' } } });
  assert.equal(link.state, 'connecting');
  assert.ok(await waitFor(() => states.includes('up')));
  await assert.rejects(b.call('host.link.create', { endpoint: { kind: 'bt_spp', config: {} }, link: { type: 'kiss' } }), e => e.code === 'unsupported');
  await assert.rejects(b.call('host.link.create', { endpoint: { kind: 'serial', config: { port: 'x' } }, link: { type: 'kiss', config: { licence: 'xyz' } } }), e => e.code === 'bad_params');
  await assert.rejects(b.call('host.link.create', { endpoint: { kind: 'serial', config: { port: 'x' } }, link: { type: 'kiss', config: { port: 99999 } } }), e => e.code === 'bad_params');
  b.close();
});

test('sos start gets acks, stop clears', async () => {
  const b = fresh();
  const ups = [];
  b.on('sos.updated', d => ups.push(d));
  await b.call('sos.start', { text: 'Need help' });
  assert.ok(await waitFor(() => ups.some(u => u.acked_by.length >= 2)));
  assert.equal((await b.call('sos.status')).active, true);
  await b.call('sos.stop');
  assert.equal((await b.call('sos.status')).active, false);
  b.close();
});

test('files.send rejects too_large', async () => {
  const b = fresh();
  const big = Buffer.alloc(129 * 1024).toString('base64');
  await assert.rejects(b.call('files.send', { conv: 'ch:public', name: 'big.bin', mime: 'application/octet-stream', data_b64: big }), e => e.code === 'too_large');
  b.close();
});

test('bundle round trip counts duplicates', async () => {
  const b = fresh();
  const ex = await b.call('bundle.export', { scope: 'all' });
  assert.ok(ex.count > 0 && ex.bytes > 0);
  const im = await b.call('bundle.import', { data_b64: ex.data_b64 });
  assert.equal(im.imported, 0);
  assert.equal(im.duplicates, ex.count);
  await assert.rejects(b.call('bundle.import', { data_b64: 'bm90IGEgYnVuZGxl' }), e => e.code === 'bad_params');
  b.close();
});

test('autoplay chatter emits only documented events and close() stops timers', async () => {
  const b = new MockBackend({ persist: false, speed: 0.0005, autoplay: true, latency: 0 });
  const seen = new Set();
  b.on('*', ev => seen.add(ev));
  await b.init();
  await sleep(150);
  b.close();
  assert.ok(seen.has('peer.seen') && seen.has('message.new'));
  for (const ev of seen) assert.ok(EVENTS.has(ev), ev);
});
