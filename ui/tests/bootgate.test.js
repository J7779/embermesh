// The start-up animation ends early when a toast or an overlay appears (js/boot). Routine toasts and
// notices must therefore wait for its hand-over (`ember:bootdone`); an SOS or a real failure must not.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';

// a tiny DOM, enough for lib/dom.js h(), icons and lib/ui.js toast()
class El {
  constructor(tag) { this.tagName = String(tag).toUpperCase(); this.children = []; this.childNodes = this.children; this.attrs = {}; this.style = {}; this.dataset = {}; this.className = ''; this.listeners = {}; this.parentNode = null; this.classList = { add() {}, remove() {}, toggle() {}, contains: () => false }; }
  setAttribute(k, v) { this.attrs[k] = String(v); }
  getAttribute(k) { return this.attrs[k] ?? null; }
  removeAttribute(k) { delete this.attrs[k]; }
  appendChild(c) { c.parentNode = this; this.children.push(c); return c; }
  remove() { if (this.parentNode) this.parentNode.children.splice(this.parentNode.children.indexOf(this), 1); this.parentNode = null; }
  addEventListener(t, f) { (this.listeners[t] = this.listeners[t] || []).push(f); }
  removeEventListener() {}
  get childElementCount() { return this.children.filter(c => c instanceof El).length; }
  get firstElementChild() { return this.children.find(c => c instanceof El) || null; }
  get firstChild() { return this.children[0] || null; }
  removeChild(c) { this.children.splice(this.children.indexOf(c), 1); return c; }
}
const docListeners = {};
const ids = { toasts: new El('div'), 'ember-boot': new El('div') };
globalThis.Node = El;
globalThis.document = {
  documentElement: new El('html'),
  getElementById: id => ids[id] || null,
  createElement: t => new El(t),
  createElementNS: (_, t) => new El(t),
  createTextNode: t => Object.assign(new El('#text'), { textContent: String(t) }),
  addEventListener: (t, f) => { (docListeners[t] = docListeners[t] || []).push(f); },
  removeEventListener() {},
  visibilityState: 'visible',
};
globalThis.window = { matchMedia: () => ({ matches: false }), addEventListener() {}, removeEventListener() {} };

const { toast } = await import('../js/lib/ui.js');
const { bootDone, afterBoot, isUrgent } = await import('../js/lib/bootgate.js');
const toasts = () => ids.toasts.childElementCount;

test('routine toasts and notices wait for the start-up hand-over; SOS and failures do not', () => {
  assert.equal(bootDone(), false, 'the animation is on screen');
  toast('Saved', { kind: 'ok' });
  toast('Connected to the relay');
  let notice = false;
  afterBoot(() => { notice = true; });
  assert.equal(toasts(), 0, 'nothing routine interrupts the animation');
  assert.equal(notice, false);
  toast('SOS from a neighbour', { kind: 'sos' });
  toast('Storage error', { kind: 'bad' });
  assert.equal(toasts(), 2, 'an emergency and a failure are shown at once');
  for (const f of docListeners['ember:bootdone'] || []) f();
  assert.equal(bootDone(), true);
  assert.equal(toasts(), 4, 'the routine toasts follow the hand-over');
  assert.equal(notice, true);
  // afterwards: no waiting
  const before = ids.toasts.children.at(-1);
  toast('Later');
  assert.notEqual(ids.toasts.children.at(-1), before, 'shown at once (the host keeps at most four)');
});

test('urgency: only SOS, failures and explicitly urgent ones', () => {
  assert.equal(isUrgent({ kind: 'sos' }), true);
  assert.equal(isUrgent({ kind: 'bad' }), true);
  assert.equal(isUrgent({ urgent: true }), true);
  assert.equal(isUrgent({ kind: 'ok' }), false);
  assert.equal(isUrgent({}), false);
});

test('the start-up notices in app.js go through the gate', () => {
  const src = fs.readFileSync(new URL('../js/app.js', import.meta.url), 'utf8');
  const i = src.indexOf('upgradeNotice(app, c)');
  assert.ok(i > 0);
  assert.match(src.slice(Math.max(0, i - 200), i), /afterBoot\(/, 'the one-time upgrade notice waits for the hand-over');
});
