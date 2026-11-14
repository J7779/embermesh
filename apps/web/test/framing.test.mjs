import { test } from 'node:test';
import assert from 'node:assert/strict';
import { ui, sleep } from './helpers.mjs';

const F = await import(ui('endpoints/framing.js'));

test('base64 matches Node Buffer for all small lengths and random data', () => {
  for (let n = 0; n < 70; n++) {
    const b = new Uint8Array(n).map((_, i) => (i * 131 + n) & 255);
    const ref = Buffer.from(b).toString('base64');
    assert.equal(F.bytesToB64(b), ref, `len ${n}`);
    assert.deepEqual(F.b64ToBytes(ref), b);
    assert.equal(F.bytesToB64(b, { url: true }), Buffer.from(b).toString('base64url'));
    assert.deepEqual(F.b64ToBytes(Buffer.from(b).toString('base64url')), b);
  }
  const big = new Uint8Array(300000).map(() => Math.random() * 256);
  assert.equal(F.bytesToB64(big), Buffer.from(big).toString('base64'));
  assert.throws(() => F.b64ToBytes('ab$d'));
  assert.throws(() => F.b64ToBytes('abcde'));
  assert.deepEqual(F.b64ToBytes(' QU\nJD '), new Uint8Array([65, 66, 67]));
});

test('chunkBytes: boundaries', () => {
  assert.deepEqual(F.chunkBytes(new Uint8Array(0), 20), []);
  assert.deepEqual(F.chunkBytes(new Uint8Array(1), 20).map((c) => c.length), [1]);
  assert.deepEqual(F.chunkBytes(new Uint8Array(20), 20).map((c) => c.length), [20]);
  assert.deepEqual(F.chunkBytes(new Uint8Array(21), 20).map((c) => c.length), [20, 1]);
  assert.deepEqual(F.chunkBytes(new Uint8Array(45), 20).map((c) => c.length), [20, 20, 5]);
  assert.throws(() => F.chunkBytes(new Uint8Array(3), 0));
  const data = Uint8Array.from({ length: 50 }, (_, i) => i);
  assert.deepEqual(F.concatBytes(...F.chunkBytes(data, 7)), data);
});

test('crc16 CCITT-FALSE check value', () => {
  assert.equal(F.crc16(new TextEncoder().encode('123456789')), 0x29b1);
  assert.equal(F.crc16(new Uint8Array(0)), 0xffff);
});

test('PCM conversion clamps and round-trips', () => {
  const f = new Float32Array([0, 0.5, -0.5, 1, -1, 2, -2]);
  const i = F.floatToInt16(f);
  assert.deepEqual([...i], [0, 16383, -16384, 32767, -32768, 32767, -32768]);
  const back = F.int16ToFloat(i);
  assert.ok(Math.abs(back[1] - 0.5) < 1e-3);
});

test('toBytes normalises inputs', () => {
  assert.deepEqual(F.toBytes([1, 2]), new Uint8Array([1, 2]));
  assert.deepEqual(F.toBytes(new Uint16Array([0x0201]).buffer), new Uint8Array([1, 2]));
  assert.deepEqual(F.toBytes(new DataView(new Uint8Array([9, 8, 7]).buffer, 1)), new Uint8Array([8, 7]));
  assert.deepEqual(F.toBytes('A'), new Uint8Array([65]));
  assert.throws(() => F.toBytes(5));
});

test('WriteQueue: one write in flight, order kept, cap enforced, error clears', async () => {
  let inflight = 0, max = 0;
  const written = [];
  const q = new F.WriteQueue(async (b) => { inflight++; max = Math.max(max, inflight); await sleep(2); written.push(b[0]); inflight--; }, { maxBytes: 10 });
  assert.ok(q.push([1, 1, 1]));
  assert.ok(q.push([2, 2, 2]));
  assert.ok(q.push([3, 3, 3]));
  assert.equal(q.push([4, 4, 4, 4, 4, 4, 4, 4]), false, 'over cap is dropped');
  assert.equal(q.dropped, 1);
  await sleep(30);
  assert.deepEqual(written, [1, 2, 3]);
  assert.equal(max, 1);
  let errored = null;
  const q2 = new F.WriteQueue(async () => { throw new Error('gone'); });
  q2.onError = (e) => { errored = e.message; };
  q2.push([1]); q2.push([2]);
  await sleep(5);
  assert.equal(errored, 'gone');
  assert.equal(q2.pending, 0);
});

test('Backoff grows to the cap and resets', () => {
  const b = new F.Backoff(100, 400);
  const v = [b.next(), b.next(), b.next(), b.next()];
  assert.ok(v[0] >= 80 && v[0] <= 120);
  assert.ok(v[3] >= 320 && v[3] <= 480);
  b.reset();
  assert.ok(b.next() <= 120);
});
