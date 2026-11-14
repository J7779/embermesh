import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createAnnouncer, shortcutTarget, isTyping, focusDescriptor, SHORTCUTS } from '../js/lib/a11y.js';
import { GLOSSARY } from '../js/lib/simple.js';

const UI = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function clock() {
  let t = 1000;
  const timers = [];
  return {
    now: () => t,
    schedule: (fn, ms) => { timers.push({ at: t + ms, fn }); return timers.length; },
    advance(ms) { t += ms; for (const x of timers.filter(x => x.at <= t && !x.done)) { x.done = true; x.fn(); } },
  };
}

test('announcer: dedupes, caps floods and summarises what it dropped', () => {
  const c = clock();
  const said = [];
  const a = createAnnouncer({ write: (t, as) => said.push([t, as]), now: c.now, schedule: c.schedule, windowMs: 10000, maxPerWindow: 3, maxAssertive: 2, dedupeMs: 5000 });
  assert.equal(a.announce(''), false);
  assert.equal(a.announce('  Message delivered.  '), true);
  assert.equal(a.announce('Message delivered.'), false, 'same text within 5 s');
  c.advance(5000);
  assert.equal(a.announce('Message delivered.'), true, 'repeated after the dedupe time');
  for (let i = 0; i < 20; i++) a.announce('New message ' + i);
  assert.equal(said.filter(s => !s[1]).length, 3, 'three polite per window');
  assert.ok(a.suppressed >= 18);
  c.advance(10000);
  assert.equal(said.at(-1)[0], '19 more updates', 'one summary instead of a flood');
  // assertive has its own budget, so an SOS is not blocked by chatter
  assert.equal(a.announce('Your SOS is on.', { assertive: true }), true);
  assert.equal(a.announce('1 person heard your SOS', { assertive: true }), true);
  assert.equal(a.announce('2 people heard your SOS', { assertive: true }), false);
  assert.equal(said.filter(s => s[1]).length, 2);
  // very long text is cut
  c.advance(20000);
  a.announce('x'.repeat(1000));
  assert.equal(said.at(-1)[0].length, 300);
});

test('shortcuts: g + letter destinations, never while typing', () => {
  assert.equal(shortcutTarget('c'), '#/chats');
  assert.equal(shortcutTarget('S'), '#/sos');
  assert.equal(shortcutTarget('a'), '#/settings/accessibility');
  assert.equal(shortcutTarget('z'), null);
  assert.equal(new Set(SHORTCUTS.map(s => s[0])).size, SHORTCUTS.length, 'keys are unique');
  assert.equal(isTyping({ tagName: 'TEXTAREA' }), true);
  assert.equal(isTyping({ tagName: 'INPUT', type: 'search' }), true);
  assert.equal(isTyping({ tagName: 'INPUT', type: 'checkbox' }), false);
  assert.equal(isTyping({ tagName: 'DIV', isContentEditable: true }), true);
  assert.equal(isTyping({ tagName: 'BUTTON' }), false);
  assert.equal(isTyping(null), false);
});

test('focus descriptors find the same control after a re-render', () => {
  const el = (tag, attrs = {}, text = '') => ({ tagName: tag.toUpperCase(), id: attrs.id || '', textContent: text, getAttribute: k => (k in attrs ? attrs[k] : null) });
  assert.deepEqual(focusDescriptor(el('a', { href: '#/guides/read/x' }, 'X')), { href: '#/guides/read/x' });
  assert.deepEqual(focusDescriptor(el('input', { id: 'setup-name' })), { id: 'setup-name' });
  assert.deepEqual(focusDescriptor(el('button', { id: 'sheet-abc', 'aria-label': 'Close' })), { tag: 'button', label: 'Close' }, 'random ids are not used');
  assert.deepEqual(focusDescriptor(el('button', {}, '  Save profile ')), { tag: 'button', label: 'Save profile' });
  assert.equal(focusDescriptor(el('div')), null);
  assert.equal(focusDescriptor(null), null);
});

test('plain-wording glossary is unique and documented in the README', () => {
  const from = GLOSSARY.map(g => g[0]);
  assert.equal(new Set(from).size, from.length, 'no duplicate source strings');
  const readme = fs.readFileSync(path.join(UI, 'README.md'), 'utf8');
  for (const [a, b] of GLOSSARY.filter(g => g[0].length < 40)) assert.ok(readme.includes('| ' + a + ' | ' + b + ' |'), 'README wording table lacks: ' + a);
});

test('a11y.css covers every attribute prefs.js sets', () => {
  const css = fs.readFileSync(path.join(UI, 'css', 'a11y.css'), 'utf8');
  for (const sel of ['data-simple="true"', 'data-motion="off"', 'data-motion="reduced"', 'data-contrast="high"', 'data-theme="light"', 'data-theme="night"', 'data-spacing="wide"', 'data-transparency="reduced"', 'data-textsize="xlarge"', 'forced-colors', '--text-scale', '--a11y-css', '.skip-link', ':focus-visible']) {
    assert.ok(css.includes(sel), sel);
  }
  assert.doesNotMatch(css, /@import|url\(\s*["']?https?:/);
});
