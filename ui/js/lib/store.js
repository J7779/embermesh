// Minimal observable state store. Pure (no DOM). Tested in ui/tests/store.test.js.
//
//   const s = createStore({a: 1});
//   const off = s.subscribe('a', (v, prev) => …);   // or s.subscribe(state => …) for any change
//   s.set({a: 2});  s.update('a', v => v + 1);  s.get('a');
// Subscribers run synchronously after each set, only when a key's value changed (Object.is).

export function createStore(initial = {}) {
  let state = { ...initial };
  const keyed = new Map(); // key -> Set(fn)
  const any = new Set();

  function get(key) { return key === undefined ? state : state[key]; }

  function set(patch) {
    if (!patch || typeof patch !== 'object') return;
    const prev = state;
    const changed = [];
    for (const k of Object.keys(patch)) if (!Object.is(prev[k], patch[k])) changed.push(k);
    if (!changed.length) return;
    state = { ...prev, ...patch };
    for (const k of changed) {
      const subs = keyed.get(k);
      if (subs) for (const fn of [...subs]) safe(fn, state[k], prev[k]);
    }
    for (const fn of [...any]) safe(fn, state, prev, changed);
  }

  function update(key, fn) { set({ [key]: fn(state[key]) }); }

  function subscribe(keyOrFn, fn) {
    if (typeof keyOrFn === 'function') {
      any.add(keyOrFn);
      return () => any.delete(keyOrFn);
    }
    if (!keyed.has(keyOrFn)) keyed.set(keyOrFn, new Set());
    keyed.get(keyOrFn).add(fn);
    return () => keyed.get(keyOrFn)?.delete(fn);
  }

  return { get, set, update, subscribe };
}

function safe(fn, ...args) {
  try { fn(...args); } catch (e) { console.error('store subscriber failed', e); }
}
