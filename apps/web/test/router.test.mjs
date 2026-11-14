// Host-router logic with fakes: no WASM, no browser.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { ui, sleep } from './helpers.mjs';

const { HostRouter } = await import(ui('endpoints/router.js'));
const { Endpoint } = await import(ui('endpoints/base.js'));
const { parseSdp } = await import(ui('endpoints/webrtc_blob.js'));
const websocket = (await import(ui('endpoints/websocket.js'))).default;
const webrtc = (await import(ui('endpoints/webrtc.js'))).default;

const notImpl = () => Object.assign(new Error('nope'), { code: 'not_implemented' });

class FakeCore {
  constructor({ settings = true, linkTypes = null } = {}) {
    this.links = [];
    this.next = 1;
    this.calls = [];
    this.rx = [];
    this.kicks = 0;
    this.settingsOk = settings;
    this.settings = {};
    this.linkTypes = linkTypes;
  }
  async call(m, p) {
    this.calls.push([m, p]);
    switch (m) {
      case 'links.add': { const l = { id: this.next++, type: p.type, name: p.name, config: p.config, endpoint: p.endpoint, persist: p.persist !== false, state: 'down' }; this.links.push(l); return { link: l }; }
      case 'links.list': return { links: this.links };
      case 'links.configure': { const l = this.links.find((x) => x.id === p.id); if (!l) throw Object.assign(new Error('x'), { code: 'not_found' }); if (p.endpoint) l.endpoint = p.endpoint; if (p.name) l.name = p.name; return { link: l }; }
      case 'links.remove': { const i = this.links.findIndex((l) => l.id === p.id); if (i < 0) throw Object.assign(new Error('x'), { code: 'not_found' }); this.links.splice(i, 1); return {}; }
      case 'links.endpoint_state': { const l = this.links.find((x) => x.id === p.id); if (l) l.state = p.connected ? 'up' : 'down'; return {}; }
      case 'links.types': if (!this.linkTypes) throw notImpl(); return { types: this.linkTypes };
      case 'settings.get': if (!this.settingsOk) throw notImpl(); return { settings: { ...this.settings } };
      case 'settings.set': if (!this.settingsOk) throw notImpl(); Object.assign(this.settings, p.settings); return { settings: this.settings };
      case 'position.set': return {};
      default: throw notImpl();
    }
  }
  linkRx(id, b) { this.rx.push([id, [...b]]); }
  audioRx() {}
  audioTx() { return new Int16Array(0); }
  kick() { this.kicks++; }
  callsOf(m) { return this.calls.filter((c) => c[0] === m).map((c) => c[1]); }
  /** What a restart does to the real core: only persisted links come back, in state down. */
  restart() { this.links = this.links.filter((l) => l.persist).map((l) => ({ ...l, state: 'down' })); this.calls = []; }
}

class FakeEp extends Endpoint {
  constructor(kind, config, ctx, opts) { super(kind, config, ctx); this.opts = opts; this.sent = []; this.ptt = []; this.preparedInteractive = null; }
  async prepare({ interactive } = {}) {
    this.preparedInteractive = interactive;
    if (this.opts.prepareFail && !interactive) throw Object.assign(new Error('device not permitted'), { code: 'not_found' });
    this.config.device_id = this.config.device_id || 'dev-1';
  }
  async open() { if (this.opts.openFail) throw new Error('open failed'); this._setState('up'); }
  send(b) { this.sent.push([...b]); return true; }
  setPtt(on) { this.ptt.push(on); }
}

function fakeModule(kind, opts = {}) {
  const m = {
    kind, title: `Fake ${kind}`, description: 'fake', datagram: true, scannable: true, autoRestore: opts.autoRestore ?? true,
    linkTypes: ['raw', 'kiss'], defaultLinkType: 'kiss', instances: [],
    detect: () => (opts.unavailable ? { available: false, reason: 'not here, sorry' } : { available: true }),
    config: () => [{ key: 'device_id', label: 'Device', type: 'text' }],
    scan: opts.slowScan ? () => new Promise(() => {}) : async () => ({ devices: [{ id: 'd1', name: 'Dev', config: { device_id: 'd1' } }] }),
    create(config, ctx) { const ep = new FakeEp(kind, config, ctx, opts); m.instances.push(ep); return ep; },
  };
  return m;
}

function memStore() { const s = {}; return { s, get: (k) => (k in s ? JSON.parse(s[k]) : null), set: (k, v) => { s[k] = JSON.stringify(v); } }; }

function makeRouter(core, endpoints, extra = {}) {
  const events = [];
  const r = new HostRouter({ core, endpoints, env: extra.env || { crypto: globalThis.crypto }, store: extra.store || memStore(), emit: (e, d) => events.push([e, d]), info: { app_version: '9.9', backend: 'wasm' }, log: { warn() {}, error() {}, log() {} } });
  return { r, events };
}

test('host.info: endpoint kinds, link types from core, unavailable reasons, features', async () => {
  const core = new FakeCore({ linkTypes: [{ type: 'kiss', endpoint_kinds: ['serial'] }, { type: 'stream', endpoint_kinds: ['serial', 'ble'] }] });
  const { r } = makeRouter(core, [fakeModule('serial'), fakeModule('ble', { unavailable: true }), fakeModule('loopback')]);
  const info = await r.handle('host.info');
  assert.equal(info.platform, 'web');
  assert.equal(info.app_version, '9.9');
  const k = Object.fromEntries(info.endpoint_kinds.map((x) => [x.kind, x]));
  assert.deepEqual(k.serial.link_types, ['kiss', 'stream']);
  assert.equal(k.serial.default_link_type, 'kiss');
  assert.deepEqual(k.ble.link_types, ['stream']);
  assert.equal(k.ble.default_link_type, 'stream', 'default falls back to the first compatible type');
  assert.equal(k.ble.available, false);
  assert.equal(k.ble.unavailable_reason, 'not here, sorry');
  assert.deepEqual(k.loopback.link_types, ['raw', 'kiss'], 'no core info for this kind: static list');
  assert.ok(Array.isArray(info.features));
  assert.ok(info.unavailable_features.hub);
  // stub core (links.types not implemented) still works
  const info2 = await makeRouter(new FakeCore(), [fakeModule('serial')]).r.handle('host.info');
  assert.deepEqual(info2.endpoint_kinds[0].link_types, ['raw', 'kiss']);
});

test('link.create wires endpoint <-> core, PTT, state and autostart; link.remove undoes it', async () => {
  const core = new FakeCore();
  const mod = fakeModule('serial');
  const { r, events } = makeRouter(core, [mod]);
  const { link } = await r.handle('host.link.create', { name: 'TNC', endpoint: { kind: 'serial', config: { baud: 9600 } }, link: { type: 'kiss', config: { port: 0 } } });
  const ep = mod.instances[0];
  assert.equal(ep.preparedInteractive, true, 'device picker gets the user gesture');
  const add = core.callsOf('links.add')[0];
  assert.equal(add.type, 'kiss');
  assert.deepEqual(add.config, { port: 0 });
  assert.equal(add.endpoint.kind, 'serial');
  assert.equal(add.endpoint.config.device_id, 'dev-1', 'config learnt during prepare is stored');
  assert.equal(link.id, 1);
  assert.deepEqual(core.callsOf('links.endpoint_state'), [{ id: 1, connected: true }]);
  assert.deepEqual(events.find((e) => e[0] === 'host.endpoint.state')[1], { link_id: 1, connected: true, state: 'up' });
  assert.equal(link.state, 'up', 'returned link reflects endpoint state');

  ep._emitData(new Uint8Array([1, 2, 3]));
  assert.deepEqual(core.rx, [[1, [1, 2, 3]]]);
  assert.ok(core.kicks > 0);
  assert.equal(r.onLinkTx(1, new Uint8Array([9])), true);
  assert.deepEqual(ep.sent, [[9]]);
  assert.equal(r.onLinkTx(42, new Uint8Array([9])), false, 'unknown link id');
  r.onCoreEvent('link.ptt', { link_id: 1, on: true });
  r.onCoreEvent('link.ptt', { link_id: 1, on: false });
  await sleep(1);
  assert.deepEqual(ep.ptt, [true, false]);

  assert.equal(add.persist, true, 'the core persists the definition (no host-side list)');
  assert.equal(core.settings.links_autostart, undefined, 'no links_autostart mirror');

  ep._setState('error', 'unplugged');
  assert.deepEqual(core.callsOf('links.endpoint_state').at(-1), { id: 1, connected: false, error: 'unplugged' });

  await r.handle('host.link.remove', { id: 1 });
  assert.ok(ep.closed);
  assert.equal(core.links.length, 0);
  assert.equal(r.onLinkTx(1, new Uint8Array([1])), false);
  ep._emitData(new Uint8Array([7]));
  assert.equal(core.rx.length, 1, 'closed endpoint no longer feeds the core');
});

test('startup on node.ready: links.list -> reopen endpoints from link.endpoint -> endpoint_state; never prompts', async () => {
  const core = new FakeCore();
  const mod = fakeModule('ble');
  const { r } = makeRouter(core, [mod]);
  await r.handle('host.link.create', { endpoint: { kind: 'ble', config: {} }, link: { type: 'raw' } });
  await r.handle('host.link.create', { endpoint: { kind: 'ble', config: {} }, link: { type: 'raw' }, autostart: false });
  assert.deepEqual(core.callsOf('links.add').map((p) => p.persist), [true, false]);
  await r.close();

  // "page reload": the core restores its persisted links (down); the host reattaches by id
  core.restart();
  const mod2 = fakeModule('ble');
  const { r: r2 } = makeRouter(core, [mod2]);
  r2.onCoreEvent('node.ready', {});
  const res = await r2.startup(); // queued behind the node.ready run: both finish here
  assert.equal(mod2.instances.length, 1, 'node.ready + explicit startup reopen the endpoint once');
  assert.deepEqual(res, [{ link_id: 1, ok: true, kept: true }]);
  assert.equal(mod2.instances[0].preparedInteractive, false, 'restore never opens device pickers');
  assert.equal(mod2.instances[0].config.device_id, 'dev-1', 'endpoint config comes from link.endpoint');
  assert.equal(core.callsOf('links.add').length, 0, 'no duplicate link created');
  assert.deepEqual(core.callsOf('links.endpoint_state')[0], { id: 1, connected: true });
  mod2.instances[0]._emitData(new Uint8Array([5]));
  assert.deepEqual(core.rx.at(-1), [1, [5]]);

  // wipe: the core drops every link and emits node.ready -> stale endpoints are closed
  core.links = [];
  r2.onCoreEvent('node.ready', {});
  await r2.startup();
  assert.ok(mod2.instances[0].closed);
  assert.equal(r2.onLinkTx(1, new Uint8Array([1])), false);

  // links.remove called directly by someone else: link.state removed:true releases the endpoint
  const { link } = await r2.handle('host.link.create', { endpoint: { kind: 'ble', config: {} }, link: { type: 'raw' } });
  r2.onCoreEvent('link.state', { link: { id: link.id, state: 'down', removed: true } });
  assert.ok(mod2.instances.at(-1).closed);
});

test('legacy links_autostart (settings or localStorage) is migrated into core links once, then cleared', async () => {
  const store = memStore();
  store.set('links_autostart', [{ link_id: 9, name: 'L', endpoint: { kind: 'loopback', config: { channel: 'x' } }, link: { type: 'raw', config: {} } }]);
  const core = new FakeCore();
  core.settings.links_autostart = [
    { link_id: 3, name: 'A', endpoint: { kind: 'serial', config: { device_id: 'a' } }, link: { type: 'kiss', config: {} } },
  ];
  const ser = fakeModule('serial'), lb = fakeModule('loopback');
  const { r } = makeRouter(core, [ser, lb], { store });
  await r.startup();
  assert.deepEqual(core.callsOf('links.add').map((p) => [p.type, p.endpoint.kind, p.persist]), [['kiss', 'serial', true], ['raw', 'loopback', true]]);
  assert.deepEqual(core.settings.links_autostart, []);
  assert.equal(store.get('links_autostart'), null);
  assert.equal(ser.instances.length + lb.instances.length, 2, 'migrated links are opened');
  await r.startup();
  assert.equal(core.callsOf('links.add').length, 2, 'migration runs once');
});

test('startup failures are reported to the core and do not stop other links', async () => {
  const core = new FakeCore();
  core.links = [
    { id: 3, type: 'kiss', name: 'A', endpoint: { kind: 'serial', config: {} }, persist: true },
    { id: 4, type: 'raw', name: 'B', endpoint: { kind: 'loopback', config: {} }, persist: true },
    { id: 5, type: 'raw', name: 'C', endpoint: { kind: 'nonexistent', config: {} }, persist: true },
    { id: 6, type: 'raw', name: 'D', endpoint: { kind: 'webrtc', config: { role: 'offer' } }, persist: true },
    { id: 7, type: 'raw', name: 'E', endpoint: {}, persist: true },
  ];
  const { r, events } = makeRouter(core, [fakeModule('serial', { prepareFail: true }), fakeModule('loopback'), webrtc]);
  const res = await r.startup();
  assert.deepEqual(res.map((x) => [x.link_id, x.ok]), [[3, false], [4, true], [5, false], [6, false]]);
  assert.ok(events.some(([e, d]) => e === 'host.endpoint.state' && d.link_id === 3 && d.error));
  const st = Object.fromEntries(core.callsOf('links.endpoint_state').map((p) => [p.id, p]));
  assert.equal(st[3].connected, false);
  assert.match(st[3].error, /not permitted/);
  assert.equal(st[4].connected, true);
  assert.match(st[5].error, /not available/);
  assert.match(st[6].error, /pairing/);
  assert.equal(st[7], undefined, 'links without an endpoint are left alone');
  assert.equal(core.links.length, 5, 'definitions kept');
});

test('reconnect re-creates the endpoint on the same link id', async () => {
  const core = new FakeCore();
  const mod = fakeModule('serial');
  const { r } = makeRouter(core, [mod]);
  const { link } = await r.handle('host.link.create', { endpoint: { kind: 'serial', config: {} }, link: { type: 'kiss' } });
  const res = await r.handle('host.link.reconnect', { id: link.id });
  assert.equal(res.link.id, link.id);
  assert.equal(mod.instances.length, 2);
  assert.ok(mod.instances[0].closed);
  assert.equal(mod.instances[1].config.device_id, 'dev-1');
  r.onLinkTx(link.id, new Uint8Array([1]));
  assert.deepEqual(mod.instances[1].sent, [[1]]);
  await assert.rejects(r.handle('host.link.reconnect', { id: 99 }), { code: 'not_found' });
});

test('errors: unsupported kinds/methods, bad params, pairing kinds, failing links.add closes endpoint', async () => {
  const core = new FakeCore();
  const mod = fakeModule('serial');
  const { r } = makeRouter(core, [mod, fakeModule('ble', { unavailable: true }), webrtc]);
  await assert.rejects(r.handle('host.link.create', {}), { code: 'bad_params' });
  await assert.rejects(r.handle('host.link.create', { endpoint: { kind: 'tcp' } }), { code: 'unsupported' });
  await assert.rejects(r.handle('host.link.create', { endpoint: { kind: 'ble' } }), (e) => e.code === 'unsupported' && /not here/.test(e.message));
  await assert.rejects(r.handle('host.webrtc.create_offer', {}), { code: 'unsupported' }); // no RTCPeerConnection in env
  for (const m of ['host.hub.start', 'host.hub.stop', 'host.hub.status', 'host.foo']) await assert.rejects(r.handle(m, {}), { code: 'unsupported' });
  await assert.rejects(r.handle('host.qr.scan', {}), { code: 'unsupported' });
  await assert.rejects(r.handle('host.location.get', {}), { code: 'unsupported' });
  await assert.rejects(r.handle('host.keep_awake', { enabled: true }), { code: 'unsupported' });
  await assert.rejects(r.handle('host.signal.flash', { pattern_ms: [100] }), { code: 'unsupported' });
  await assert.rejects(r.handle('host.file.save', { name: 'x' }), { code: 'bad_params' });
  await assert.rejects(r.handle('host.link.remove', {}), { code: 'bad_params' });

  core.call = async (m) => { if (m === 'links.add') throw notImpl(); return {}; };
  await assert.rejects(r.handle('host.link.create', { endpoint: { kind: 'serial' } }), { code: 'not_implemented' });
  assert.ok(mod.instances.at(-1).closed, 'endpoint closed when the core refuses the link');
});

test('endpoint open failure keeps the link and reports error state', async () => {
  const core = new FakeCore();
  const { r, events } = makeRouter(core, [fakeModule('serial', { openFail: true })]);
  const { link } = await r.handle('host.link.create', { endpoint: { kind: 'serial' } });
  assert.equal(link.id, 1);
  assert.equal(core.callsOf('links.endpoint_state')[0].error, 'open failed');
  assert.ok(events.some(([e, d]) => e === 'host.endpoint.state' && d.state === 'error'));
});

test('scan delegates and times out', async () => {
  const { r } = makeRouter(new FakeCore(), [fakeModule('serial'), fakeModule('ble', { slowScan: true }), { ...fakeModule('loopback'), scannable: false }]);
  assert.equal((await r.handle('host.endpoints.scan', { kind: 'serial' })).devices[0].id, 'd1');
  const t0 = Date.now();
  assert.deepEqual(await r.handle('host.endpoints.scan', { kind: 'ble', timeout_ms: 30 }), { devices: [] });
  assert.ok(Date.now() - t0 < 1000);
  await assert.rejects(r.handle('host.endpoints.scan', { kind: 'loopback' }), { code: 'unsupported' });
});

// --- fake WebSocket / RTCPeerConnection -----------------------------------------------------------

class FakeWS {
  static all = [];
  constructor(url) { this.url = url; this.readyState = 0; this.sent = []; FakeWS.all.push(this); setTimeout(() => { this.readyState = 1; this.onopen && this.onopen(); }, 1); }
  send(b) { this.sent.push([...new Uint8Array(b.buffer ? b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength) : b)]); }
  close() { this.readyState = 3; setTimeout(() => this.onclose && this.onclose({ code: 1000 }), 0); }
}

test('hub guest mode adds a websocket raw link (reused on reload) and shows in host.info', async () => {
  const core = new FakeCore();
  const env = { WebSocket: FakeWS, crypto: globalThis.crypto, location: { protocol: 'http:', host: '10.0.0.1:8437', href: 'http://10.0.0.1:8437/' } };
  const { r } = makeRouter(core, [websocket], { env });
  await r.connectHub({ url: 'ws://10.0.0.1:8437/ws', name: 'Camp' });
  const add = core.callsOf('links.add')[0];
  assert.equal(add.type, 'raw');
  assert.equal(add.endpoint.kind, 'websocket');
  assert.equal(add.endpoint.config.url, 'ws://10.0.0.1:8437/ws');
  await sleep(5);
  const ws = FakeWS.all.at(-1);
  assert.equal(ws.url, 'ws://10.0.0.1:8437/ws');
  ws.onmessage({ data: new Uint8Array([4, 5]).buffer });
  ws.onmessage({ data: 'text frames are ignored' });
  assert.deepEqual(core.rx, [[1, [4, 5]]]);
  r.onLinkTx(1, new Uint8Array([6]));
  assert.deepEqual(ws.sent, [[6]]);
  const info = await r.handle('host.info');
  assert.equal(info.hub.url, 'ws://10.0.0.1:8437/ws');
  assert.equal(info.hub.connected, true);
  assert.equal(add.persist, false, 'hub link is not persisted; it is re-detected on every load');
  // same session, node.ready again (unlock): hub link kept, not duplicated
  r.onCoreEvent('node.ready', {});
  await r.startup();
  assert.equal(core.callsOf('links.add').length, 1);
  // wipe: links gone -> hub link re-added
  core.links = [];
  r.onCoreEvent('node.ready', {});
  await r.startup();
  assert.equal(core.callsOf('links.add').length, 2, 'hub link re-added after wipe');
  await r.close();
  // reload: a non-persisted link is gone, so the hub is added again
  core.restart();
  const { r: r2 } = makeRouter(core, [websocket], { env });
  await r2.connectHub({ url: 'ws://10.0.0.1:8437/ws' });
  assert.equal(core.callsOf('links.add').length, 1);
  await r2.close();
});

const FP = Array.from({ length: 32 }, (_, i) => i.toString(16).toUpperCase().padStart(2, '0')).join(':');
const pcs = new Map();
class FakeDC {
  constructor(pc) { this.pc = pc; this.readyState = 'connecting'; this.bufferedAmount = 0; }
  send(b) { const peer = this.peer; const copy = new Uint8Array(b).slice().buffer; setTimeout(() => peer.onmessage && peer.onmessage({ data: copy }), 0); }
  close() { this.readyState = 'closed'; this.onclose && this.onclose(); }
  _open() { this.readyState = 'open'; this.onopen && this.onopen(); }
}
class FakePC extends EventTarget {
  constructor(cfg) { super(); this.cfg = cfg; this.ufrag = Math.random().toString(36).slice(2, 8); this.iceGatheringState = 'new'; this.connectionState = 'new'; pcs.set(this.ufrag, this); }
  createDataChannel() { this.dc = new FakeDC(this); return this.dc; }
  _sdp(setup) { return `v=0\r\na=ice-ufrag:${this.ufrag}\r\na=ice-pwd:pwpwpwpwpwpwpwpwpwpwpwpw\r\na=fingerprint:sha-256 ${FP}\r\na=setup:${setup}\r\na=mid:0\r\na=candidate:1 1 udp 1 192.168.0.${pcs.size} 5000 typ host\r\n`; }
  async createOffer() { return { type: 'offer', sdp: this._sdp('actpass') }; }
  async createAnswer() { return { type: 'answer', sdp: this._sdp('active') }; }
  async setLocalDescription(d) { this.localDescription = d; this.iceGatheringState = 'complete'; }
  async setRemoteDescription(d) {
    this.remoteDescription = d;
    this.peer = pcs.get(parseSdp(d.sdp).ufrag);
    assert.ok(this.peer, 'rebuilt SDP carries the peer ICE ufrag');
    if (d.type === 'answer') {
      const remote = new FakeDC(this.peer);
      remote.peer = this.dc; this.dc.peer = remote;
      const ev = new Event('datachannel'); ev.channel = remote;
      this.peer.dispatchEvent(ev);
      setTimeout(() => { this.dc._open(); remote._open(); }, 2);
    }
  }
  close() { this.connectionState = 'closed'; }
}

test('WebRTC manual pairing: offer -> answer -> links on both sides carrying bytes', async () => {
  const env = { RTCPeerConnection: FakePC, crypto: globalThis.crypto };
  const coreA = new FakeCore(), coreB = new FakeCore();
  coreB.next = 50;
  const A = makeRouter(coreA, [webrtc], { env }).r;
  const B = makeRouter(coreB, [webrtc], { env }).r;
  const { pairing_id, offer } = await A.handle('host.webrtc.create_offer', { name: 'Alice' });
  assert.match(offer, /^ember:rtc:/);
  await assert.rejects(A.handle('host.webrtc.accept_answer', { pairing_id, answer: offer }), { code: 'bad_params' });
  const acc = await B.handle('host.webrtc.accept_offer', { offer, name: 'Bob' });
  assert.match(acc.answer, /^ember:rtc:/);
  assert.equal(acc.link.id, 50);
  await assert.rejects(A.handle('host.webrtc.accept_answer', { pairing_id: 'nope', answer: acc.answer }), { code: 'not_found' });
  const { link } = await A.handle('host.webrtc.accept_answer', { pairing_id, answer: acc.answer });
  assert.equal(link.id, 1);
  assert.equal(coreA.callsOf('links.add')[0].endpoint.kind, 'webrtc');
  await sleep(10);
  assert.deepEqual(coreA.callsOf('links.endpoint_state').at(-1), { id: 1, connected: true });
  assert.deepEqual(coreB.callsOf('links.endpoint_state').at(-1), { id: 50, connected: true });
  A.onLinkTx(1, new Uint8Array([1, 2, 3]));
  B.onLinkTx(50, new Uint8Array([9]));
  await sleep(5);
  assert.deepEqual(coreB.rx, [[50, [1, 2, 3]]]);
  assert.deepEqual(coreA.rx, [[1, [9]]]);
  await assert.rejects(A.handle('host.webrtc.accept_answer', { pairing_id, answer: acc.answer }), { code: 'not_found' }, 'pairing consumed');
  await assert.rejects(A.handle('host.link.reconnect', { id: 1 }), { code: 'unsupported' });
  await A.close(); await B.close();
});

test('host.link.create fails cleanly when getUserMedia never answers or is refused: no half-open link', async () => {
  const audio = (await import(ui('endpoints/audio.js'))).default;
  class FakeAC { constructor() { this.state = 'running'; this.sampleRate = 48000; } resume() { return Promise.resolve(); } close() { return Promise.resolve(); } }
  let mode = 'hang';
  const env = {
    AudioContext: FakeAC, isSecureContext: true, crypto: globalThis.crypto,
    navigator: { mediaDevices: { getUserMedia: () => (mode === 'hang' ? new Promise(() => {}) : Promise.reject(Object.assign(new Error('no'), { name: 'NotAllowedError' }))) } },
  };
  const core = new FakeCore();
  const { r } = makeRouter(core, [audio], { env });
  const t0 = Date.now();
  await assert.rejects(r.handle('host.link.create', { endpoint: { kind: 'audio', config: { media_timeout_ms: 50 } }, link: { type: 'audio' } }),
    (e) => e.code === 'io_error' && /no answer from the microphone/.test(e.message));
  assert.ok(Date.now() - t0 < 2000);
  assert.equal(core.links.length, 0, 'link removed again');
  assert.equal(r.links.size, 0);
  mode = 'deny';
  await assert.rejects(r.handle('host.link.create', { endpoint: { kind: 'audio', config: {} }, link: { type: 'audio' } }), (e) => e.code === 'denied' && /permission denied/.test(e.message));
  assert.equal(core.links.length, 0);
});

test('host.info.actions: download single file over http, install prompt when the browser offers one', async () => {
  const listeners = {};
  const env = { crypto: globalThis.crypto, location: { protocol: 'http:', href: 'http://10.0.0.1:8437/' }, document: {}, addEventListener: (n, f) => { listeners[n] = f; } };
  const { r } = makeRouter(new FakeCore(), [], { env });
  let info = await r.handle('host.info');
  assert.deepEqual(info.actions.map((a) => a.id), ['download_single_file']);
  for (const a of info.actions) assert.match(a.method, /^host\./);
  listeners.beforeinstallprompt({ preventDefault() {}, prompt: async () => {}, userChoice: Promise.resolve({ outcome: 'accepted' }) });
  info = await r.handle('host.info');
  assert.deepEqual(info.actions.map((a) => a.id), ['install_app', 'download_single_file']);
  assert.deepEqual(await r.handle('host.web.install'), { message: 'Installed' });
  await assert.rejects(r.handle('host.web.install'), { code: 'unsupported' });
});

test('reconnect and links.configure: the endpoint is reopened from the descriptor stored in the core', async () => {
  const core = new FakeCore();
  const mod = fakeModule('serial');
  const { r } = makeRouter(core, [mod]);
  const { link } = await r.handle('host.link.create', { endpoint: { kind: 'serial', config: { baud: 9600 } }, link: { type: 'kiss' } });
  // settings edited elsewhere (UI -> links.configure {endpoint}): same id, new descriptor
  await core.call('links.configure', { id: link.id, endpoint: { kind: 'serial', config: { baud: 115200, device_id: 'dev-2' } } });
  await r.handle('host.link.reconnect', { id: link.id });
  assert.equal(mod.instances.length, 2);
  assert.ok(mod.instances[0].closed);
  assert.equal(mod.instances[1].config.baud, 115200, 'reconnect uses the core descriptor, not a cached copy');
  assert.equal(mod.instances[1].config.device_id, 'dev-2');
  r.onLinkTx(link.id, new Uint8Array([1]));
  assert.deepEqual(mod.instances[1].sent, [[1]]);

  // the core announces a changed descriptor via link.state: the open endpoint is reopened (no prompt)
  const changed = { kind: 'serial', config: { baud: 57600, device_id: 'dev-2' } };
  await core.call('links.configure', { id: link.id, endpoint: changed });
  r.onCoreEvent('link.state', { link: { ...core.links[0], endpoint: changed } });
  await r.startup(); // queued behind the reopen
  assert.equal(mod.instances.length, 3);
  assert.equal(mod.instances[2].config.baud, 57600);
  assert.equal(mod.instances[2].preparedInteractive, false);
  // an unchanged descriptor (key order differs) does nothing
  r.onCoreEvent('link.state', { link: { ...core.links[0], endpoint: { config: { device_id: 'dev-2', baud: 57600 }, kind: 'serial' } } });
  await r.startup();
  assert.equal(mod.instances.length, 3);
  await assert.rejects(r.handle('host.link.reconnect', { id: 99 }), { code: 'not_found' });
});

test('host.info.audio: PTT methods and preset support from what this browser can do', async () => {
  const presets = [{ type: 'audio', endpoint_kinds: ['audio'], config: [{ key: 'preset', options: [{ value: 'custom' }, { value: 'handheld_vox' }, { value: 'handheld_ptt' }, { value: 'acoustic' }] }] }];
  const audio = (await import(ui('endpoints/audio.js'))).default;
  const serial = (await import(ui('endpoints/serial.js'))).default;
  const base = { crypto: globalThis.crypto, AudioContext: class {}, isSecureContext: true };
  const withSerial = { ...base, navigator: { mediaDevices: { getUserMedia() {} }, serial: {} } };
  let h = (await makeRouter(new FakeCore({ linkTypes: presets }), [audio, serial], { env: withSerial }).r.handle('host.info')).audio;
  assert.deepEqual(h.ptt, ['vox', 'manual', 'serial']);
  assert.equal(h.serial_ptt, true);
  assert.deepEqual(h.presets_unsupported, {});
  const noSerial = { ...base, navigator: { mediaDevices: { getUserMedia() {} } } };
  h = (await makeRouter(new FakeCore({ linkTypes: presets }), [audio, serial], { env: noSerial }).r.handle('host.info')).audio;
  assert.deepEqual(h.ptt, ['vox', 'manual']);
  assert.deepEqual(h.presets_supported, ['custom', 'handheld_vox', 'acoustic']);
  assert.match(h.presets_unsupported.handheld_ptt, /Web Serial/);
  const insecure = { ...noSerial, isSecureContext: false };
  h = (await makeRouter(new FakeCore({ linkTypes: presets }), [audio, serial], { env: insecure }).r.handle('host.info')).audio;
  assert.deepEqual(h.ptt, []);
  assert.deepEqual(h.presets_supported, []);
  assert.match(h.presets_unsupported.handheld_vox, /secure page/);
});

test('host.info.accessibility from media queries, host.accessibility on change, host.haptic via the Vibration API', async () => {
  const mqs = {};
  const matchMedia = (q) => (mqs[q] = mqs[q] || { matches: false, ls: [], addEventListener(_e, f) { this.ls.push(f); } });
  const vib = [];
  const env = { crypto: globalThis.crypto, matchMedia, navigator: { vibrate: (p) => { vib.push(p); return true; } } };
  const { r, events } = makeRouter(new FakeCore(), [], { env });
  let info = await r.handle('host.info');
  assert.deepEqual(info.accessibility, { reduce_motion: false, high_contrast: false });
  assert.ok(info.features.includes('haptic'));
  mqs['(prefers-reduced-motion: reduce)'].matches = true;
  for (const f of mqs['(prefers-reduced-motion: reduce)'].ls) f();
  assert.deepEqual(events.filter((e) => e[0] === 'host.accessibility').map((e) => e[1]), [{ reduce_motion: true, high_contrast: false }]);
  mqs['(forced-colors: active)'].matches = true;
  for (const f of mqs['(forced-colors: active)'].ls) f();
  info = await r.handle('host.info');
  assert.deepEqual(info.accessibility, { reduce_motion: true, high_contrast: true, forced_colors: true });
  await r.handle('host.haptic', { kind: 'success' });
  assert.deepEqual(vib, [[12, 60, 12]]);
  await assert.rejects(r.handle('host.haptic', { kind: 'boom' }), { code: 'bad_params' });
  const { r: r2 } = makeRouter(new FakeCore(), [], { env: { crypto: globalThis.crypto } });
  await assert.rejects(r2.handle('host.haptic', {}), { code: 'unsupported' });
});
