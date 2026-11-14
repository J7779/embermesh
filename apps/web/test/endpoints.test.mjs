// Endpoint modules with fakes (and Node's real BroadcastChannel for loopback).
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { ui, sleep } from './helpers.mjs';

const loopback = (await import(ui('endpoints/loopback.js'))).default;
const websocket = (await import(ui('endpoints/websocket.js'))).default;
const { wsUrlProblem, defaultHubUrl } = await import(ui('endpoints/websocket.js'));
const serialMod = await import(ui('endpoints/serial.js'));
const serial = serialMod.default;
const bleMod = await import(ui('endpoints/ble.js'));
const ble = bleMod.default;
const audio = (await import(ui('endpoints/audio.js'))).default;
const { applyDefaults } = await import(ui('endpoints/base.js'));
const { ENDPOINTS } = await import(ui('endpoints/index.js'));

test('every endpoint module has the common descriptor shape', () => {
  const kinds = ENDPOINTS.map((m) => m.kind);
  assert.deepEqual(kinds, ['websocket', 'webrtc', 'serial', 'ble', 'audio', 'loopback']);
  for (const m of ENDPOINTS) {
    for (const k of ['title', 'description']) assert.equal(typeof m[k], 'string', `${m.kind}.${k}`);
    assert.ok(m.linkTypes.includes(m.defaultLinkType), m.kind);
    assert.equal(typeof m.detect, 'function');
    assert.equal(typeof m.create, 'function');
    const fields = m.config(globalThis);
    for (const f of fields) {
      assert.ok(f.key && f.label && ['number', 'text', 'select', 'bool', 'textarea', 'password'].includes(f.type), `${m.kind}.${f.key}`);
      if (f.type === 'select') assert.ok(f.options.every((o) => 'value' in o && 'label' in o));
    }
    if (m.scannable) assert.equal(typeof m.scan, 'function', m.kind);
    // in Node, unavailable ones must explain why
    const d = m.detect(globalThis);
    if (!d.available) assert.ok(d.reason.length > 10);
  }
  // insecure page: reasons mention HTTPS
  const insecure = { isSecureContext: false, navigator: {}, AudioContext: function () {} };
  assert.match(serial.detect(insecure).reason, /HTTPS/);
  assert.match(ble.detect(insecure).reason, /HTTPS/);
  assert.match(audio.detect(insecure).reason, /HTTPS, localhost, or the single-file/);
});

test('loopback: two endpoints on the same channel exchange frames, other channels do not', async () => {
  const a = loopback.create({ channel: 't1' }, {});
  const b = loopback.create({ channel: 't1' }, {});
  const c = loopback.create({ channel: 't2' }, {});
  const got = { a: [], b: [], c: [] };
  a.onData((x) => got.a.push([...x]));
  b.onData((x) => got.b.push([...x]));
  c.onData((x) => got.c.push([...x]));
  await Promise.all([a.open(), b.open(), c.open()]);
  assert.equal(a.state, 'up');
  assert.ok(a.send(new Uint8Array([1, 2])));
  b.send(new Uint8Array([3]));
  await sleep(30);
  assert.deepEqual(got.b, [[1, 2]]);
  assert.deepEqual(got.a, [[3]], 'no echo to the sender');
  assert.deepEqual(got.c, []);
  await Promise.all([a.close(), b.close(), c.close()]);
  assert.equal(a.send(new Uint8Array([1])), false);
});

test('websocket: URL checks, default hub URL, reconnect after drop', async () => {
  assert.equal(wsUrlProblem('ws://10.0.0.1:8437/ws', { location: { protocol: 'http:' } }), null);
  assert.match(wsUrlProblem('ws://10.0.0.1:8437/ws', { location: { protocol: 'https:' } }), /block ws:\/\//);
  assert.equal(wsUrlProblem('ws://localhost:8437/ws', { location: { protocol: 'https:' } }), null);
  assert.match(wsUrlProblem('http://x', {}), /ws:\/\//);
  assert.match(wsUrlProblem('nope', {}), /valid/);
  assert.equal(defaultHubUrl({ location: { protocol: 'https:', host: 'hub.lan' } }), 'wss://hub.lan/ws');
  assert.equal(defaultHubUrl({ location: { protocol: 'file:', host: '' } }), 'ws://192.168.4.1:8437/ws');

  const socks = [];
  class WS { constructor(u) { this.url = u; this.readyState = 0; socks.push(this); setTimeout(() => { this.readyState = 1; this.onopen(); }, 1); } send() {} close() { this.readyState = 3; } }
  const ep = websocket.create({ url: 'ws://h/ws' }, { env: { WebSocket: WS } });
  const states = [];
  ep.onState((s) => states.push(s));
  ep._backoff.min = ep._backoff.cur = 5;
  await ep.open();
  await sleep(5);
  assert.equal(ep.state, 'up');
  socks[0].readyState = 3;
  socks[0].onclose({ code: 1006 });
  assert.equal(ep.state, 'error');
  await sleep(40);
  assert.equal(socks.length, 2);
  assert.equal(ep.state, 'up');
  await ep.close();
  assert.deepEqual(states, ['connecting', 'up', 'error', 'connecting', 'up', 'down']);
  await assert.rejects(websocket.create({ url: 'bad' }, { env: { WebSocket: WS } }).open(), { code: 'bad_params' });
});

function fakeSerial() {
  const written = [];
  let push;
  const port = {
    info: { usbVendorId: 0x1a86, usbProductId: 0x7523 },
    signals: [],
    opened: null,
    getInfo() { return this.info; },
    async open(o) { this.opened = o; this.readable = new ReadableStream({ start(c) { push = c; } }); this.writable = new WritableStream({ write(ch) { written.push([...ch]); } }); },
    async setSignals(s) { this.signals.push(s); },
    async close() { this.closedPort = true; },
  };
  const nav = { serial: { granted: [], async getPorts() { return this.granted; }, async requestPort() { this.granted.push(port); return port; }, addEventListener() {}, removeEventListener() {} } };
  return { port, nav, written, push: (b) => push.enqueue(new Uint8Array(b)) };
}

test('serial: picker, open options, stream rx, queued tx, PTT lines, restore by USB id', async () => {
  const f = fakeSerial();
  const env = { navigator: f.nav };
  const ep = serial.create({ baud: 115200, ptt: 'rts' }, { env });
  assert.equal(ep.datagram, false);
  await ep.prepare({ interactive: true });
  assert.equal(ep.describe().config.usb_vendor_id, 0x1a86);
  assert.match(ep.describe().config.device_name, /CH34x/);
  const rx = [];
  ep.onData((b) => rx.push(...b));
  await ep.open();
  assert.equal(f.port.opened.baudRate, 115200);
  assert.equal(f.port.opened.flowControl, 'none');
  assert.deepEqual(f.port.signals[0], { dataTerminalReady: true, requestToSend: false });
  f.push([0xc0, 0x00]); f.push([0x41, 0xc0]);
  await sleep(5);
  assert.deepEqual(rx, [0xc0, 0x00, 0x41, 0xc0], 'stream bytes passed through unchanged');
  ep.send(new Uint8Array([1, 2])); ep.send(new Uint8Array([3]));
  await sleep(5);
  assert.deepEqual(f.written, [[1, 2], [3]]);
  await ep.setPtt(true);
  await ep.setPtt(false);
  assert.deepEqual(f.port.signals.slice(1), [{ requestToSend: true }, { requestToSend: false }]);
  await ep.close();
  assert.ok(f.port.closedPort);

  // restore: no picker, finds the granted port by USB ids
  const ep2 = serial.create(ep.describe().config, { env });
  await ep2.prepare({ interactive: false });
  assert.equal(ep2.port, f.port);
  const none = serial.create({ usb_vendor_id: 1, usb_product_id: 2 }, { env });
  await assert.rejects(none.prepare({ interactive: false }), { code: 'not_found' });
  assert.deepEqual(serialMod.pttSignals({ ptt: 'dtr', ptt_invert: true }, true), { dataTerminalReady: false });
  assert.equal(serialMod.pttSignals({ ptt: 'none' }, true), null);
  const scan = await serial.scan({}, env);
  assert.equal(scan.devices[0].config.usb_product_id, 0x7523);
});

test('ble: request options per profile, chunked writes, notifications, disconnect', async () => {
  assert.deepEqual(bleMod.requestOptions({ profile: 'auto' }).optionalServices, [bleMod.EMBER_SERVICE, bleMod.NUS_SERVICE]);
  assert.deepEqual(bleMod.requestOptions({ profile: 'nus', name_prefix: 'TNC' }).filters, [{ services: [bleMod.NUS_SERVICE], namePrefix: 'TNC' }]);
  assert.throws(() => bleMod.requestOptions({ profile: 'custom' }), { code: 'bad_params' });

  const writes = [];
  let notifyHandler;
  const tx = { addEventListener: (e, h) => { notifyHandler = h; }, removeEventListener() {}, async startNotifications() {} };
  const rxc = { properties: { writeWithoutResponse: true }, async writeValueWithoutResponse(v) { writes.push(v.length); } };
  const service = { async getCharacteristic(u) { return u === bleMod.NUS_RX ? rxc : tx; } };
  const listeners = {};
  const device = {
    id: 'dev42', name: 'Mobilinkd',
    addEventListener: (e, h) => { listeners[e] = h; }, removeEventListener() {},
    gatt: { async connect() { return { async getPrimaryService(u) { if (u !== bleMod.NUS_SERVICE) throw new Error('no such service'); return service; } }; }, disconnect() {} },
  };
  const env = { navigator: { bluetooth: { async requestDevice() { return device; }, async getDevices() { return [device]; } } } };
  const ep = ble.create({ profile: 'auto', reconnect: false }, { env });
  await ep.prepare();
  assert.equal(ep.describe().config.device_id, 'dev42');
  await ep.open();
  assert.equal(ep.state, 'up', 'auto profile falls back from Ember service to NUS');
  const got = [];
  ep.onData((b) => got.push([...b]));
  const v = new Uint8Array([9, 8, 7]);
  notifyHandler({ target: { value: new DataView(v.buffer) } });
  assert.deepEqual(got, [[9, 8, 7]]);
  ep.send(new Uint8Array(45));
  await sleep(5);
  assert.deepEqual(writes, [20, 20, 5]);
  listeners.gattserverdisconnected();
  assert.equal(ep.state, 'error');
  assert.equal(ep.send(new Uint8Array(1)), false);
  // restore by device id without a picker
  const ep2 = ble.create({ device_id: 'dev42' }, { env: { navigator: { bluetooth: { getDevices: async () => [device], requestDevice: async () => { throw new Error('no gesture'); } } } } });
  await ep2.prepare({ interactive: false });
  assert.equal(ep2.device, device);
  await ep.close();
});

test('applyDefaults coerces numbers/bools and fills defaults', () => {
  const schema = [{ key: 'baud', type: 'number', default: 9600 }, { key: 'dtr', type: 'bool', default: true }, { key: 's', type: 'text', default: 'x' }];
  assert.deepEqual(applyDefaults(schema, { baud: '1200', dtr: 'false', other: 1 }), { baud: 1200, dtr: false, s: 'x', other: 1 });
  assert.deepEqual(applyDefaults(schema, { baud: 'abc', s: '' }), { baud: 9600, dtr: true, s: 'x' });
});

test('webrtc gatherComplete: usual timeout with candidates, waits longer (capped) for the first one, ends on completion', async () => {
  const { gatherComplete } = await import(ui('endpoints/webrtc.js'));
  class PC extends EventTarget {
    constructor(sdp = '') { super(); this.iceGatheringState = 'gathering'; this.localDescription = { sdp }; }
    cand(c) { const e = new Event('icecandidate'); e.candidate = c; this.dispatchEvent(e); }
  }
  const time = async (fn) => { const t0 = Date.now(); await fn(); return Date.now() - t0; };
  // a candidate is there: the usual (soft) timeout applies
  let pc = new PC('v=0\r\na=candidate:1 1 udp 1 x.local 1 typ host\r\n');
  let ms = await time(() => gatherComplete(pc, 40, 400));
  assert.ok(ms >= 35 && ms < 200, `soft timeout ${ms}`);
  // none yet: keeps waiting past the soft timeout; the first late candidate ends it
  pc = new PC();
  setTimeout(() => pc.cand({ candidate: 'late' }), 120);
  ms = await time(() => gatherComplete(pc, 40, 1000));
  assert.ok(ms >= 110 && ms < 400, `waited for the first candidate (${ms} ms)`);
  // never any: the hard cap
  pc = new PC();
  ms = await time(() => gatherComplete(pc, 20, 150));
  assert.ok(ms >= 140 && ms < 400, `hard cap ${ms}`);
  // completion (end-of-candidates) ends it at once
  pc = new PC();
  setTimeout(() => pc.cand(null), 10);
  ms = await time(() => gatherComplete(pc, 500, 1000));
  assert.ok(ms < 200, `completed ${ms}`);
});
