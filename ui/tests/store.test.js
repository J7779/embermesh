import test from 'node:test';
import assert from 'node:assert/strict';
import { createStore } from '../js/lib/store.js';

test('get/set/update and keyed subscriptions', () => {
  const s = createStore({ a: 1, b: { x: 1 } });
  const seen = [];
  const off = s.subscribe('a', (v, prev) => seen.push([v, prev]));
  s.set({ a: 2 });
  s.set({ a: 2 }); // unchanged → no notification
  s.update('a', v => v + 1);
  assert.deepEqual(seen, [[2, 1], [3, 2]]);
  off();
  s.set({ a: 4 });
  assert.equal(seen.length, 2);
  assert.equal(s.get('a'), 4);
  assert.deepEqual(s.get(), { a: 4, b: { x: 1 } });
});

test('any-change subscribers get changed keys; state is replaced immutably', () => {
  const s = createStore({ a: 1 });
  const before = s.get();
  let changed = null;
  s.subscribe((state, prev, keys) => { changed = keys; assert.equal(prev, before); });
  s.set({ a: 1, b: 2 });
  assert.deepEqual(changed, ['b']);
  assert.notEqual(s.get(), before);
  assert.equal(before.b, undefined);
});

test('a throwing subscriber does not break others', () => {
  const s = createStore({ a: 0 });
  let ok = 0;
  const origErr = console.error; console.error = () => {};
  s.subscribe('a', () => { throw new Error('boom'); });
  s.subscribe('a', () => ok++);
  s.set({ a: 1 });
  console.error = origErr;
  assert.equal(ok, 1);
});

test('bad input ignored', () => {
  const s = createStore();
  s.set(null); s.set('x');
  assert.deepEqual(s.get(), {});
});
