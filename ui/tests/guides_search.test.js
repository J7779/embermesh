// The guide search box works from the moment it is on screen (screens/guides.js): a query typed
// while the library is still loading waits ("Loading the library…") and runs once it is ready.
import test from 'node:test';
import assert from 'node:assert/strict';

// a tiny DOM, enough for lib/dom.js h(), icons, highlight and the guides home
class El {
  constructor(tag) { this.tagName = String(tag).toUpperCase(); this.children = []; this.attrs = {}; this.style = {}; this.dataset = {}; this.className = ''; this.listeners = {}; this.parentNode = null; this.hidden = false; this.disabled = false; this.value = ''; this._text = ''; this.classList = { add() {}, remove() {}, toggle() {}, contains: () => false }; }
  setAttribute(k, v) { this.attrs[k] = String(v); }
  getAttribute(k) { return this.attrs[k] ?? null; }
  removeAttribute(k) { delete this.attrs[k]; }
  appendChild(c) { if (c.tagName === '#FRAG') { for (const x of [...c.children]) this.appendChild(x); return c; } if (c.parentNode) c.parentNode.removeChild(c); c.parentNode = this; this.children.push(c); return c; }
  removeChild(c) { this.children.splice(this.children.indexOf(c), 1); c.parentNode = null; return c; }
  get firstChild() { return this.children[0] || null; }
  get isConnected() { let n = this; while (n.parentNode) n = n.parentNode; return n === root; }
  addEventListener(t, f) { (this.listeners[t] = this.listeners[t] || []).push(f); }
  removeEventListener() {}
  fire(t, e = {}) { for (const f of this.listeners[t] || []) f({ target: this, ...e }); }
  focus() {}
  get textContent() { return this.tagName === '#TEXT' ? this._text : this.children.map(c => c.textContent).join(''); }
  set textContent(v) { this.children = []; if (this.tagName === '#TEXT') this._text = String(v); else if (v !== '') this.appendChild(document.createTextNode(v)); }
  querySelector() { return null; }
}
const root = new El('main');
globalThis.Node = El;
globalThis.document = {
  createElement: t => new El(t),
  createElementNS: (_, t) => new El(t),
  createTextNode: t => { const n = new El('#text'); n._text = String(t); return n; },
  createDocumentFragment: () => new El('#frag'),
  addEventListener() {}, removeEventListener() {}, getElementById: () => null, documentElement: new El('html'),
};
globalThis.window = { matchMedia: () => ({ matches: true }), addEventListener() {}, removeEventListener() {} };
globalThis.matchMedia = globalThis.window.matchMedia;
globalThis.history = { replaceState() {} };

const { default: render } = await import('../js/screens/guides.js');

const CATS = { total: 2, categories: [{ id: 'water', title: 'Water', count: 2 }] };
function fakeApp({ loadedAfter }) {
  let calls = 0;
  const searches = [];
  const app = {
    backend: { on() {} },
    on: () => () => {},
    kvGet: async (_ns, _k, d) => d,
    api: {
      try: async (m) => {
        if (m === 'guides.categories') return ++calls > loadedAfter ? CATS : { total: 0, categories: [] };
        if (m === 'guides.info') return { guides: 2, semantic: { available: false } };
        return null;
      },
      call: async (m, p) => {
        if (m !== 'guides.search') throw new Error('unexpected ' + m);
        searches.push(p.query);
        return { results: [{ id: 'water/boil', title: 'Boiling water', category: 'water', snippet: 'Boil water for a minute', terms: ['water'] }] };
      },
    },
  };
  return { app, searches };
}
const find = (n, pred) => { if (pred(n)) return n; for (const c of n.children || []) { const r = find(c, pred); if (r) return r; } return null; };
const wait = ms => new Promise(r => setTimeout(r, ms));

test('typed while the library loads: "Loading the library…", then the query runs when it is ready', async () => {
  root.children = [];
  const { app, searches } = fakeApp({ loadedAfter: 1 }); // first answer: still loading; ready one second later
  const done = render(app, { rest: [], query: new URLSearchParams('') }, root);
  await wait(10);
  const input = find(root, n => n.tagName === 'INPUT' && n.attrs.type === 'search');
  assert.ok(input, 'the box is on screen');
  assert.equal(input.disabled, false, 'and usable while the library loads');
  input.value = 'water';
  input.fire('input');
  await wait(250);
  assert.match(root.textContent, /Loading the library…/);
  assert.deepEqual(searches, [], 'nothing searched before the library is ready');
  await done; // the library became ready (after one poll)
  await wait(20);
  assert.deepEqual(searches, ['water'], 'the pending query ran as soon as the library was ready');
  assert.match(root.textContent, /1 result for “water”/);
  assert.doesNotMatch(root.textContent, /Loading the library…/);
});

test('library ready at once: typing searches as before', async () => {
  root.children = [];
  const { app, searches } = fakeApp({ loadedAfter: 0 });
  await render(app, { rest: [], query: new URLSearchParams('') }, root);
  const input = find(root, n => n.tagName === 'INPUT' && n.attrs.type === 'search');
  input.value = 'boil';
  input.fire('input');
  await wait(250);
  assert.deepEqual(searches, ['boil']);
});
