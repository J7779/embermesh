import test from 'node:test';
import assert from 'node:assert/strict';
import {
  PREF_DEFAULTS, PREF_KEYS, cleanPref, mergePrefs, effectivePrefs, navItems, readMirror, writeMirror,
  initPrefs, getPrefs, setPref, onPrefsChange, _reset, _setHostHints, MIRROR_PREFIX,
} from '../js/lib/prefs.js';

const NAV = [
  { id: 'chats', label: 'Chats', icon: 'chat' }, { id: 'people', label: 'People', icon: 'people' },
  { id: 'sos', label: 'SOS', icon: 'sos', cls: 'sos-tab' }, { id: 'board', label: 'Bulletins', icon: 'board', wide: true },
  { id: 'links', label: 'Links', icon: 'link', wide: true }, { id: 'guides', label: 'Guides', icon: 'book' },
  { id: 'help', label: 'Instructions', icon: 'info' }, { id: 'tools', label: 'Tools', icon: 'tool', wide: true },
  { id: 'extras', label: 'Notes & lists', icon: 'notes', wide: true }, { id: 'settings', label: 'Settings', icon: 'gear', wide: true },
  { id: 'more', label: 'More', icon: 'more', narrow: true },
];

function fakeStorage(init = {}) {
  const m = new Map(Object.entries(init));
  return { getItem: k => (m.has(k) ? m.get(k) : null), setItem: (k, v) => m.set(k, String(v)), map: m };
}

function fakeBackend(kv = {}, host = {}) {
  const store = new Map(Object.entries(kv));
  const handlers = new Map();
  const calls = [];
  return {
    calls, store,
    async call(method, p) {
      calls.push([method, p]);
      if (method === 'host.info') return { platform: 'web', features: [], ...host };
      if (method === 'kv.get') return { value: store.has(p.key) ? store.get(p.key) : null };
      if (method === 'kv.set') { store.set(p.key, p.value); return {}; }
      throw Object.assign(new Error('nope'), { code: 'not_implemented' });
    },
    on(ev, fn) { if (!handlers.has(ev)) handlers.set(ev, new Set()); handlers.get(ev).add(fn); return () => handlers.get(ev).delete(fn); },
    emit(ev, d) { for (const f of handlers.get(ev) || []) f(d); },
  };
}

test('defaults and validation', () => {
  assert.deepEqual(mergePrefs(), { ...PREF_DEFAULTS });
  assert.equal(PREF_DEFAULTS.simple_mode, false);
  assert.equal(PREF_DEFAULTS.motion, 'system');
  assert.equal(PREF_DEFAULTS.text_scale, 1);
  assert.equal(PREF_DEFAULTS.high_contrast, null);
  assert.equal(PREF_DEFAULTS.boot_animation, 'full');
  assert.equal(cleanPref('text_scale', 3), 1.6);
  assert.equal(cleanPref('text_scale', 0.1), 0.85);
  assert.equal(cleanPref('text_scale', '1.25'), 1.25);
  assert.equal(cleanPref('text_scale', 'big'), undefined);
  assert.equal(cleanPref('text_scale', NaN), undefined);
  assert.equal(cleanPref('motion', 'wild'), undefined);
  assert.equal(cleanPref('boot_animation', 'short'), 'short');
  assert.equal(cleanPref('simple_mode', 'yes'), undefined);
  assert.equal(cleanPref('high_contrast', null), null);
  // invalid values never override valid earlier ones
  assert.equal(mergePrefs({ motion: 'off' }, { motion: 'bogus' }).motion, 'off');
  assert.equal(mergePrefs({ text_scale: 1.2 }, { text_scale: 1.4 }).text_scale, 1.4);
});

test('motion precedence: preference, system, low power, Simple mode, test hook', () => {
  const m = (p, env) => effectivePrefs(p, env).motion;
  assert.equal(m({}, {}), 'full');
  assert.equal(m({}, { reduceMotion: true }), 'reduced');
  assert.equal(m({ motion: 'full' }, { reduceMotion: true }), 'full', 'an explicit choice wins over the platform');
  assert.equal(m({ motion: 'off' }, {}), 'off');
  assert.equal(m({ motion: 'full' }, { lowPower: true }), 'reduced', 'low power caps at reduced');
  assert.equal(m({ motion: 'off' }, { lowPower: true }), 'off');
  assert.equal(m({ motion: 'full', simple_mode: true }, {}), 'reduced', 'Simple mode caps at reduced');
  assert.equal(m({ simple_mode: true }, {}), 'reduced');
  assert.equal(m({ motion: 'reduced', simple_mode: true }, {}), 'reduced');
  assert.equal(m({ motion: 'full' }, { forceMotionOff: true }), 'off');
  // host hints win over media queries
  assert.equal(m({}, { reduceMotion: true, host: { reduce_motion: false } }), 'full');
  assert.equal(m({}, { reduceMotion: false, host: { reduce_motion: true } }), 'reduced');
  assert.equal(effectivePrefs({ motion: 'full' }, { lowPower: true }).why.motion, 'low_power');
  assert.equal(effectivePrefs({}, { reduceMotion: true }).why.motion, 'system');
});

test('contrast, text size and spacing', () => {
  const c = (p, env) => effectivePrefs(p, env).contrast;
  assert.equal(c({}, {}), 'normal');
  assert.equal(c({}, { prefersContrast: 'more' }), 'high');
  assert.equal(c({}, { forcedColors: true }), 'high');
  assert.equal(c({}, { host: { high_contrast: true } }), 'high');
  assert.equal(c({}, { prefersContrast: 'more', host: { high_contrast: false } }), 'normal');
  assert.equal(c({ high_contrast: false }, { prefersContrast: 'more' }), 'normal', 'explicit off wins');
  assert.equal(c({ high_contrast: true }, {}), 'high');
  assert.equal(effectivePrefs({ high_contrast: null }, {}).follows.contrast, true);
  assert.equal(effectivePrefs({ text_scale: 1 }).textSize, 'normal');
  assert.equal(effectivePrefs({ text_scale: 1.2 }).textSize, 'large');
  assert.equal(effectivePrefs({ text_scale: 1.6 }).textSize, 'xlarge');
  assert.equal(effectivePrefs({ text_scale: 1.05, simple_mode: true }).textSize, 'large');
  assert.equal(effectivePrefs({ reading_spacing: true }).spacing, 'wide');
  assert.equal(effectivePrefs({ high_contrast: true }).transparency, 'reduced');
  assert.equal(effectivePrefs({}, { host: { font_scale: 1.3 } }).system.fontScale, 1.3);
});

test('navItems: everything normally, the essentials in Simple mode', () => {
  assert.deepEqual(navItems(NAV, false), NAV);
  assert.notEqual(navItems(NAV, false), NAV, 'returns a copy');
  const s = navItems(NAV, true);
  assert.deepEqual(s.map(i => i.id), ['chats', 'sos', 'guides', 'help', 'settings', 'more']);
  assert.equal(s.find(i => i.id === 'sos').label, 'Send for help');
  assert.equal(s.find(i => i.id === 'sos').fullLabel, 'SOS');
  assert.equal(s.find(i => i.id === 'sos').cls, 'sos-tab');
  assert.equal(s.find(i => i.id === 'more').narrow, undefined, 'More shows at every width');
  assert.equal(s.find(i => i.id === 'settings').wide, true, 'phones reach Settings from More');
  assert.equal(NAV.find(i => i.id === 'more').narrow, true, 'input not mutated');
  assert.deepEqual(navItems(null, true), []);
});

test('local-storage mirror round trip', () => {
  const ls = fakeStorage({ [MIRROR_PREFIX + 'simple_mode']: 'true', [MIRROR_PREFIX + 'text_scale']: '1.3', [MIRROR_PREFIX + 'motion']: '{bad json' });
  const m = readMirror(ls);
  assert.deepEqual(m, { simple_mode: true, text_scale: 1.3 });
  const out = fakeStorage();
  writeMirror(mergePrefs({ boot_animation: 'off', high_contrast: true }), out);
  for (const k of PREF_KEYS) assert.ok(out.map.has(MIRROR_PREFIX + k), k);
  assert.equal(out.map.get('ember.ui.boot_animation'), '"off"');
  assert.equal(out.map.get('ember.ui.high_contrast'), 'true');
  assert.equal(out.map.get('ember.ui.text_scale'), '1');
  assert.equal(readMirror(null).simple_mode, undefined);
});

test('initPrefs loads kv, setPref saves and notifies, host events re-apply', async () => {
  _reset();
  const b = fakeBackend({ simple_mode: true, text_scale: 1.4, motion: 'nonsense' }, { accessibility: { reduce_motion: true } });
  const seen = [];
  const off = onPrefsChange((p, eff, changed) => seen.push([p.simple_mode, eff.motion, changed.join(',')]));
  await initPrefs(b);
  let p = getPrefs();
  assert.equal(p.simple_mode, true);
  assert.equal(p.text_scale, 1.4);
  assert.equal(p.motion, 'system', 'invalid stored value ignored');
  assert.equal(p.effective.motion, 'reduced');
  assert.ok(seen.some(s => s[2].includes('simple_mode')), JSON.stringify(seen));

  await setPref('motion', 'off');
  assert.equal(b.store.get('motion'), 'off');
  assert.equal(getPrefs().effective.motion, 'off');
  assert.equal(seen.at(-1)[2], 'motion');
  const n = seen.length;
  await setPref('motion', 'off');
  assert.equal(seen.length, n, 'no event when nothing changed');
  await assert.rejects(setPref('motion', 'sideways'), /invalid/);
  await assert.rejects(setPref('colour', 1), /unknown/);

  await setPref('motion', 'system');
  await setPref('simple_mode', false);
  assert.equal(getPrefs().effective.motion, 'reduced', 'host says reduce motion');
  b.emit('host.accessibility', { reduce_motion: false });
  assert.equal(getPrefs().effective.motion, 'full');
  b.emit('host.accessibility', { high_contrast: true });
  assert.equal(getPrefs().effective.contrast, 'high');
  assert.equal(_setHostHints(null).contrast, 'normal');
  off();
});

test('preferences survive a locked core (kv failing) and are pushed later', async () => {
  _reset();
  const b = fakeBackend();
  const real = b.call;
  b.call = async (m, p) => { if (m.startsWith('kv.')) throw Object.assign(new Error('locked'), { code: 'locked' }); return real(m, p); };
  await initPrefs(b);
  await setPref('high_contrast', true); // kv.set fails, value kept in memory/mirror
  assert.equal(getPrefs().high_contrast, true);
  b.call = real;
  await initPrefs(b);
  assert.equal(getPrefs().high_contrast, true);
  await new Promise(r => setTimeout(r, 10));
  assert.equal(b.store.get('high_contrast'), true, 'pushed to kv after unlock');
  _reset();
});
