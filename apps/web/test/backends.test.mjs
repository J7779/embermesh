// native.js and remote.js backends against fake bridges / sockets.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { ui, sleep } from './helpers.mjs';

const { default: NativeBackend, findBridge } = await import(ui('backends/native.js'));
const { default: RemoteBackend, remoteUrl } = await import(ui('backends/remote.js'));

test('native: Android bridge request/reply/event round trip', async () => {
  const posted = [];
  const env = { EmberNative: { postMessage: (s) => posted.push(JSON.parse(s)) } };
  const b = new NativeBackend({ env });
  assert.equal(b.kind, 'native');
  await b.init();
  assert.equal(b.platform, 'android');
  const events = [];
  b.on('message.new', (d) => events.push(['one', d]));
  const off = b.on('*', (e, d) => events.push([e, d]));
  const p1 = b.call('node.info');
  const p2 = b.call('host.qr.scan', { x: 1 });
  assert.deepEqual(posted[0], { id: 1, method: 'node.info', params: {} });
  assert.deepEqual(posted[1], { id: 2, method: 'host.qr.scan', params: { x: 1 } });
  // replies may arrive out of order, as objects or JSON strings
  env.__emberBridge.onReply(JSON.stringify({ id: 2, ok: false, error: { code: 'unsupported', message: 'no camera' } }));
  env.__emberBridge.onReply({ id: 1, ok: true, result: { node_id: 'abcd' } });
  env.__emberBridge.onReply({ id: 99, ok: true, result: {} }); // unknown id ignored
  assert.deepEqual(await p1, { node_id: 'abcd' });
  await assert.rejects(p2, (e) => e.code === 'unsupported' && e.message === 'no camera');
  env.__emberBridge.onEvent({ event: 'message.new', data: { message: { id: 'm1' } } });
  env.__emberBridge.onEvent('{"event":"link.state","data":{"link":{"id":1}}}');
  env.__emberBridge.onEvent('not json');
  off();
  env.__emberBridge.onEvent({ event: 'node.ready' });
  assert.deepEqual(events, [['one', { message: { id: 'm1' } }], ['message.new', { message: { id: 'm1' } }], ['link.state', { link: { id: 1 } }]]);
});

test('native: iOS bridge, missing bridge, post failure, queued events', async () => {
  const posted = [];
  const env = { webkit: { messageHandlers: { ember: { postMessage: (s) => posted.push(s) } } }, __emberBridge: { queued: ['{"event":"node.ready","data":{}}'] } };
  assert.equal(findBridge(env).platform, 'ios');
  const b = new NativeBackend({ env });
  const seen = [];
  b.on('node.ready', () => seen.push(1));
  await b.init();
  assert.deepEqual(seen, [1]);
  b.call('x');
  assert.equal(typeof posted[0], 'string');
  await assert.rejects(new NativeBackend({ env: {} }).init(), { code: 'unsupported' });
  const bad = new NativeBackend({ env: { EmberNative: { postMessage() { throw new Error('boom'); } } } });
  await assert.rejects(bad.call('node.info'), { code: 'internal' });
});

class FakeSocket {
  static servers = [];
  constructor(url) {
    this.url = url; this.readyState = 0; this.sent = [];
    FakeSocket.servers.push(this);
    setTimeout(() => { if (FakeSocket.refuse) { this.readyState = 3; this.onclose && this.onclose({ code: 1006 }); return; } this.readyState = 1; this.onopen && this.onopen(); }, 1);
  }
  send(s) { this.sent.push(JSON.parse(s)); if (FakeSocket.autoReply) FakeSocket.autoReply(this, JSON.parse(s)); }
  close(code = 1000) { this.readyState = 3; setTimeout(() => this.onclose && this.onclose({ code }), 0); }
  reply(o) { this.onmessage({ data: JSON.stringify(o) }); }
}

test('remote: URL building from params', () => {
  assert.equal(remoteUrl({}, { href: 'http://10.0.0.2:8437/?remote=1&token=s3cr3t', protocol: 'http:', host: '10.0.0.2:8437' }), 'ws://10.0.0.2:8437/rpc?token=s3cr3t');
  assert.equal(remoteUrl({}, { href: 'file:///x/embermesh.html?remote=1&host=pi.local:8437', protocol: 'file:', host: '' }), 'ws://pi.local:8437/rpc');
  assert.equal(remoteUrl({ host: 'h:1', token: 't t', tls: true }, undefined), 'wss://h:1/rpc?token=t+t');
});

test('remote: calls, events, queue while disconnected, reconnect, local host methods', async () => {
  FakeSocket.servers = []; FakeSocket.refuse = false;
  FakeSocket.autoReply = (s, m) => setTimeout(() => s.reply(m.method === 'host.info'
    ? { id: m.id, ok: true, result: { platform: 'desktop', features: ['hub', 'qr_scan'] } }
    : m.method === 'fail' ? { id: m.id, ok: false, error: { code: 'denied', message: 'no' } }
      : { id: m.id, ok: true, result: { echo: m.method } }), 1);
  const b = new RemoteBackend({ env: { WebSocket: FakeSocket }, url: 'ws://h/rpc?token=x' });
  assert.equal(b.kind, 'remote');
  const states = [];
  b.on('remote.state', (d) => states.push(d.connected));
  await b.init();
  assert.deepEqual(await b.call('node.info'), { echo: 'node.info' });
  await assert.rejects(b.call('fail'), { code: 'denied' });
  const info = await b.call('host.info');
  assert.equal(info.backend, 'remote');
  assert.ok(info.features.includes('hub'));
  assert.ok(!info.features.includes('qr_scan'), 'qr_scan reflects THIS browser, which has no camera');
  await assert.rejects(b.call('host.qr.scan'), { code: 'unsupported' }, 'browser-local method handled locally');
  assert.ok(!FakeSocket.servers[0].sent.some((m) => m.method === 'host.qr.scan'));

  const got = [];
  b.on('*', (e, d) => got.push([e, d]));
  FakeSocket.servers[0].reply({ event: 'message.new', data: { message: { id: 'x' } } });
  assert.deepEqual(got.at(-1), ['message.new', { message: { id: 'x' } }]);

  // drop the connection: calls queue and complete after the automatic reconnect
  FakeSocket.servers[0].close(1006);
  await sleep(5);
  assert.equal(b.connected, false);
  const pending = b.call('mesh.peers');
  await sleep(700);
  assert.deepEqual(await pending, { echo: 'mesh.peers' });
  assert.ok(FakeSocket.servers.length >= 2);
  assert.deepEqual(states.slice(0, 3), [true, false, true]);
  b.close();
  await assert.rejects(b.call('node.info'), { code: 'io_error' });
});

test('remote: token rejection fails init with denied', async () => {
  FakeSocket.servers = []; FakeSocket.autoReply = null; FakeSocket.refuse = false;
  class Rejecting extends FakeSocket { constructor(u) { super(u); setTimeout(() => this.close(4401), 3); } }
  const b = new RemoteBackend({ env: { WebSocket: Rejecting }, url: 'ws://h/rpc' });
  // first open succeeds, then server closes with 4401 -> pending calls fail with denied
  await b.init();
  const p = b.call('node.info');
  await assert.rejects(p, { code: 'denied' });
  b.close();
  const unreachable = new RemoteBackend({ env: { WebSocket: class extends FakeSocket { constructor(u) { FakeSocket.refuse = true; super(u); } } }, url: 'ws://nowhere/rpc' });
  await assert.rejects(unreachable.init(), { code: 'io_error' });
  unreachable.close();
  FakeSocket.refuse = false;
});

test('host.haptic and host.accessibility pass through the native and remote backends unchanged', async () => {
  // native: the method goes to the bridge as is, the event comes back as is
  const posted = [];
  const env = { EmberNative: { postMessage: (s) => posted.push(JSON.parse(s)) } };
  const n = new NativeBackend({ env });
  await n.init();
  const seen = [];
  n.on('host.accessibility', (d) => seen.push(d));
  const p = n.call('host.haptic', { kind: 'success' });
  assert.deepEqual(posted.at(-1), { id: 1, method: 'host.haptic', params: { kind: 'success' } });
  env.__emberBridge.onReply({ id: 1, ok: true, result: {} });
  assert.deepEqual(await p, {});
  env.__emberBridge.onEvent({ event: 'host.accessibility', data: { reduce_motion: true, font_scale: 1.3 } });
  assert.deepEqual(seen, [{ reduce_motion: true, font_scale: 1.3 }]);

  // remote: forwarded to the daemon, its event delivered unchanged
  FakeSocket.servers = [];
  FakeSocket.autoReply = (ws, m) => setTimeout(() => ws.onmessage({ data: JSON.stringify({ id: m.id, ok: false, error: { code: 'unsupported', message: 'no vibration motor' } }) }), 0);
  const r = new RemoteBackend({ env: { WebSocket: FakeSocket, location: { href: 'http://h:8437/?remote=1', protocol: 'http:', host: 'h:8437' } } });
  await r.init();
  const got = [];
  r.on('host.accessibility', (d) => got.push(d));
  await assert.rejects(r.call('host.haptic', { kind: 'tap' }), { code: 'unsupported' });
  const ws = FakeSocket.servers.at(-1);
  assert.deepEqual(ws.sent.at(-1).method, 'host.haptic');
  assert.deepEqual(ws.sent.at(-1).params, { kind: 'tap' });
  ws.onmessage({ data: JSON.stringify({ event: 'host.accessibility', data: { high_contrast: true } }) });
  assert.deepEqual(got, [{ high_contrast: true }]);
  r.close();
  FakeSocket.autoReply = null;
});
