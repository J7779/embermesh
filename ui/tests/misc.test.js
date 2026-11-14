import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { toMorse, morsePattern, dotMs, patternDuration } from '../js/lib/morse.js';
import { pickBackendKind } from '../js/backend.js';
import { resolveGuideLink } from '../js/screens/guides.js';
import { chunkForQr, createQrAssembler } from '../js/screens/settings.js';
import { feedlineLoss, profileToInputs } from '../js/screens/tools.js';

const UI = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

test('morse text and timing', () => {
  assert.equal(toMorse('sos'), '...---...');
  assert.equal(toMorse('Hi 5'), '.... .. / .....');
  assert.equal(toMorse('<>'), '');
  assert.equal(dotMs(20), 60);
  // E T = dot, letter gap, dash
  assert.deepEqual(morsePattern(toMorse('ET'), 20), [60, 180, 180]);
  // "E E" = dot, word gap, dot
  assert.deepEqual(morsePattern(toMorse('E E'), 20), [60, 420, 60]);
  const p = morsePattern(toMorse('SOS'), 12);
  assert.equal(p.length % 2, 1);
  assert.equal(patternDuration([1, 2, 3]), 6);
});

test('backend selection follows RPC_API.md §5 order', () => {
  const loc = q => ({ location: { search: q } });
  assert.equal(pickBackendKind({ EmberNative: {}, ...loc('?mock=1') }), 'native');
  assert.equal(pickBackendKind({ webkit: { messageHandlers: { ember: {} } }, ...loc('') }), 'native');
  assert.equal(pickBackendKind({ webkit: { messageHandlers: {} }, ...loc('') }), 'wasm');
  assert.equal(pickBackendKind(loc('?remote=1&mock=1&token=x')), 'remote');
  assert.equal(pickBackendKind(loc('?mock=1')), 'mock');
  assert.equal(pickBackendKind(loc('?mock=0')), 'wasm');
  assert.equal(pickBackendKind({}), 'wasm');
});

test('guide link resolution stays inside the library', () => {
  assert.deepEqual(resolveGuideLink('radio/handheld-basics-1', 'handheld-basics-2.md'), { id: 'radio/handheld-basics-2', anchor: '' });
  assert.deepEqual(resolveGuideLink('radio/a', '../water/boil.md#steps'), { id: 'water/boil', anchor: 'steps' });
  assert.deepEqual(resolveGuideLink('radio/a', 'guide:medical/cpr'), { id: 'medical/cpr', anchor: '' });
  assert.deepEqual(resolveGuideLink('radio/a', '/water/boil'), { id: 'water/boil', anchor: '' });
  assert.equal(resolveGuideLink('radio/a', 'ember:card?x'), null);
  assert.equal(resolveGuideLink('radio/a', '../../..'), null);
  assert.equal(resolveGuideLink('radio/a', 'a b"<x>'), null);
});

test('bundle QR chunking round-trips out of order and rejects foreign frames', () => {
  const data = 'QUJD'.repeat(500);
  const frames = chunkForQr(data, 300);
  assert.ok(frames.length > 1);
  const asm = createQrAssembler();
  const order = frames.map((_, i) => i).reverse();
  let r;
  for (const i of order) { r = asm.add(frames[i]); if (i) assert.equal(r.done, false); }
  assert.equal(r.done, true);
  assert.equal(r.data_b64, data);
  assert.ok(createQrAssembler().add('hello').error);
  const a2 = createQrAssembler();
  a2.add('EMB1 1/3 AAAA');
  assert.ok(a2.add('EMB1 1/2 AAAA').error);
});

test('station profiles map onto link-budget inputs', () => {
  assert.equal(feedlineLoss('none', 10, 146), 0);
  const l = feedlineLoss('rg58', 10, 150);
  assert.ok(Math.abs(l - 1.12) < 0.01, String(l));
  const mid = feedlineLoss('rg58', 100, 300);
  assert.ok(mid > 11.2 && mid < 21, String(mid));
  const inputs = ['freq_mhz', 'distance_km', 'tx_power_w', 'tx_gain_dbi', 'tx_cable_loss_db', 'tx_height_m', 'rx_gain_dbi', 'rx_cable_loss_db', 'rx_height_m', 'rx_sensitivity_dbm'].map(key => ({ key }));
  const p = { power_w: 5, antenna_gain_dbi: 3, height_m: 10, feedline: 'lmr400', feedline_m: 20 };
  const tx = profileToInputs(p, inputs, 'tx', 146);
  assert.deepEqual(Object.keys(tx).sort(), ['tx_cable_loss_db', 'tx_gain_dbi', 'tx_height_m', 'tx_power_w']);
  const rx = profileToInputs(p, inputs, 'rx', 146);
  assert.deepEqual(Object.keys(rx).sort(), ['rx_cable_loss_db', 'rx_gain_dbi', 'rx_height_m']);
});

function walk(dir) {
  return fs.readdirSync(dir, { withFileTypes: true }).flatMap(d => d.isDirectory() ? walk(path.join(dir, d.name)) : [path.join(dir, d.name)]);
}

test('UI source never uses HTML-string sinks, eval or external URLs', () => {
  // backends/{wasm,native,remote}.js and endpoints/ are platform code, checked by apps/web/test/.
  const mine = walk(UI).filter(f => /\.(js|mjs|html|css)$/.test(f) && !/[\\/](endpoints|wasm|data)[\\/]/.test(f) && !/backends[\\/](wasm|native|remote)\.js$/.test(f) && !/[\\/]tests[\\/]/.test(f));
  assert.ok(mine.length > 15);
  for (const f of mine) {
    const src = fs.readFileSync(f, 'utf8');
    const rel = path.relative(UI, f);
    assert.doesNotMatch(src, /\.(innerHTML|outerHTML)\s*=|insertAdjacentHTML|document\.write|\beval\(|new Function\(/, rel);
    assert.doesNotMatch(src, /(src|href)\s*=\s*["']https?:\/\//i, rel + ' references an external URL');
    assert.doesNotMatch(src, /@import\s+url\(\s*["']?https?:/i, rel);
    // Element.append() turns null into the text "null"; screens must use dom.js append().
    if (!/lib[\\/]dom\.js$/.test(f) && !/backends[\\/]mock\.js$|[\\/]mock[\\/]/.test(f)) assert.doesNotMatch(src, /[\w)\]]\.append\(/, rel + ' uses Element.append');
  }
});

test('index.html has a strict CSP allowing wasm and websockets only', () => {
  const html = fs.readFileSync(path.join(UI, 'index.html'), 'utf8');
  const csp = /http-equiv="Content-Security-Policy" content="([^"]+)"/.exec(html)[1];
  assert.match(csp, /default-src 'self'/);
  assert.match(csp, /script-src 'self' 'wasm-unsafe-eval';/);
  assert.match(csp, /connect-src 'self' ws: wss:/);
  assert.match(csp, /object-src 'none'/);
  assert.doesNotMatch(csp, /unsafe-inline|https?:/);
  assert.doesNotMatch(html, /\sstyle=|<script>(?!\s*<\/script>)/);
});

test('every UI module parses and imports without a DOM', async () => {
  for (const f of walk(path.join(UI, 'js')).filter(f => f.endsWith('.js') && !/[\\/](endpoints)[\\/]/.test(f) && !/backends[\\/](wasm|native|remote)\.js$/.test(f) && !/js[\\/]app\.js$/.test(f))) {
    await import(f);
  }
});
