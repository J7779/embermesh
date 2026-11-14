// Proves the binding layer: the built WASM module exports the whole C API of ember.h, and
// ui/js/backends/wasm.js drives it (create, call, tick/poll, link bytes, audio, guide pack).
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { haveWasm, wasmModuleUrl, ui, NO_WASM, sleep } from './helpers.mjs';

const C_API = ['ember_version', 'ember_create', 'ember_destroy', 'ember_add_entropy', 'ember_call', 'ember_free',
  'ember_tick', 'ember_next_wakeup_ms', 'ember_link_rx', 'ember_audio_rx', 'ember_audio_tx', 'ember_poll', 'ember_guides_load'];

for (const variant of ['ember.mjs', 'ember.single.mjs']) {
  test(`raw C API through ${variant}`, { skip: !haveWasm() && NO_WASM }, async () => {
    const { default: create } = await import(wasmModuleUrl(variant));
    const M = await create();
    for (const f of [...C_API, 'malloc', 'free']) assert.equal(typeof M[`_${f}`], 'function', `missing export _${f}`);
    for (const v of ['HEAPU8', 'HEAP16', 'HEAPU32', 'FS', 'IDBFS', 'UTF8ToString']) assert.ok(M[v], `missing runtime ${v}`);
    assert.equal(M._ember_web_out_size(), 16);

    const str = (s) => { const b = new TextEncoder().encode(s); const p = M._malloc(b.length + 1); M.HEAPU8.set(b, p); M.HEAPU8[p + b.length] = 0; return p; };
    assert.match(M.UTF8ToString(M._ember_version()), /^\d+\.\d+\.\d+/);

    // missing / short entropy must be refused
    const cfg = str('{"storage_dir":"","platform":"web"}');
    const ent = M._malloc(64);
    M.HEAPU8.set(new Uint8Array(64).map((_, i) => i * 7), ent);
    assert.equal(M._ember_create(cfg, ent, 16), 0, 'create with 16 bytes of entropy must fail');
    const node = M._ember_create(cfg, ent, 64);
    assert.ok(node, 'ember_create');

    const call = (method, params = '{}') => {
      const mp = str(method), pp = str(params);
      const r = M._ember_call(node, mp, pp);
      const text = M.UTF8ToString(r);
      M._ember_free(r); M._free(mp); M._free(pp);
      return JSON.parse(text);
    };
    const info = call('node.info');
    assert.equal(info.ok, true);
    assert.match(info.result.node_id, /^[0-9a-f]{16}$/);
    const bad = call('no.such.method');
    assert.equal(bad.ok, false);
    assert.equal(typeof bad.error.code, 'string');

    M._ember_tick(node, BigInt(Date.now()));
    const wake = M._ember_next_wakeup_ms(node);
    assert.ok(wake >= 0 && wake <= 60000);

    const out = M._malloc(16);
    let events = 0;
    while (M._ember_poll(node, out)) {
      const kind = M.HEAPU32[out >> 2];
      const dp = M.HEAPU32[(out >> 2) + 2], len = M.HEAPU32[(out >> 2) + 3];
      if (kind === 1) {
        const ev = JSON.parse(new TextDecoder().decode(M.HEAPU8.subarray(dp, dp + len)));
        assert.equal(typeof ev.event, 'string');
        events++;
      }
    }
    assert.ok(events >= 1, 'expected at least node.ready');

    // link bytes / audio / garbage guide pack must not crash
    const buf = M._malloc(256);
    M.HEAPU8.set(new Uint8Array(256).map((_, i) => i), buf);
    M._ember_link_rx(node, 999, buf, 256);
    M._ember_audio_rx(node, 999, buf, 128, 8000);
    assert.ok(M._ember_audio_tx(node, 999, buf, 128, 8000) <= 128);
    M._ember_add_entropy(node, buf, 32);
    const rc = M._ember_guides_load(node, buf, 256, 1);
    assert.equal(typeof rc, 'number');
    M._ember_tick(node, BigInt(Date.now() + 1000));
    while (M._ember_poll(node, out)) { /* drain */ }
    M._ember_destroy(node);
  });
}

test('WasmBackend: init, node.info, events, host router, link/audio pumps', { skip: !haveWasm() && NO_WASM }, async () => {
  const { default: WasmBackend } = await import(ui('backends/wasm.js'));
  const b = new WasmBackend({ moduleUrl: wasmModuleUrl(), storage: 'memory', hub: false, guides: false });
  const seen = [];
  b.on('*', (ev) => seen.push(ev));
  let ready = false;
  b.on('node.ready', () => { ready = true; });
  assert.equal(b.kind, 'wasm');
  await b.init();
  assert.equal(await b.init(), b, 'init is idempotent');
  const info = await b.call('node.info');
  assert.match(info.node_id, /^[0-9a-f]{16}$/);
  await sleep(20);
  assert.ok(ready, 'node.ready delivered to listeners registered before init');

  // Errors reject with Error{code, message}
  await assert.rejects(b.call('definitely.not.a.method'), (e) => typeof e.code === 'string' && e.code !== '');
  await assert.rejects(b.call('host.hub.start'), { code: 'unsupported' });
  await assert.rejects(b.call('host.nonexistent'), { code: 'unsupported' });

  const hi = await b.call('host.info');
  assert.equal(hi.platform, 'web');
  assert.equal(hi.backend, 'wasm');
  assert.equal(hi.storage, 'memory');
  const kinds = Object.fromEntries(hi.endpoint_kinds.map((k) => [k.kind, k]));
  for (const k of ['websocket', 'webrtc', 'serial', 'ble', 'audio', 'loopback']) assert.ok(kinds[k], `endpoint kind ${k}`);
  for (const k of hi.endpoint_kinds) {
    assert.ok(Array.isArray(k.config) && Array.isArray(k.link_types) && k.link_types.length, k.kind);
    assert.ok(k.link_types.includes(k.default_link_type));
    if (!k.available) assert.ok(k.unavailable_reason && k.unavailable_reason.length > 10, `${k.kind} reason`);
  }
  assert.equal(kinds.serial.available, false); // Node has no Web Serial
  assert.equal(kinds.loopback.available, true);

  // binding pumps (stub core ignores data, real core will route it)
  b.linkRx(1, new Uint8Array([0xc0, 0x00, 1, 2, 3, 0xc0]));
  b.audioRx(1, new Int16Array(441), 44100);
  const pcm = b.audioTx(1, 256, 44100);
  assert.ok(pcm instanceof Int16Array && pcm.length <= 256);
  // large input exercises scratch growth + memory growth
  b.linkRx(1, new Uint8Array(3 * 1024 * 1024));
  // a garbage pack is either accepted (stub) or rejected with bad_params (real core) — never a crash
  try { assert.ok(b.loadGuidePack(new Uint8Array([1, 2, 3, 4])) >= 0); } catch (e) { assert.equal(e.code, 'bad_params'); }
  await sleep(30);
  await b.destroy();
  await assert.rejects(b._coreCall('node.info'), { code: 'internal' });
});

test('WasmBackend: two nodes in one process have independent state', { skip: !haveWasm() && NO_WASM }, async () => {
  const { default: WasmBackend } = await import(ui('backends/wasm.js'));
  const a = new WasmBackend({ moduleUrl: wasmModuleUrl(), storage: 'memory', hub: false, guides: false });
  const c = new WasmBackend({ moduleUrl: wasmModuleUrl('ember.single.mjs'), storage: 'memory', hub: false, guides: false });
  await Promise.all([a.init(), c.init()]);
  assert.notEqual(a.M, c.M);
  assert.ok((await a.call('node.info')).version);
  assert.ok((await c.call('node.info')).version);
  await a.destroy();
  assert.ok((await c.call('node.info')).version, 'other node unaffected');
  await c.destroy();
});
