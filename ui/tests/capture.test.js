// Morse reader capture logic (synthetic frames and PCM), the mock's morse.rx / rf.radios /
// rf.hardware per docs/RPC_API.md §6, and the pure helpers of the radio wizard, Instructions and
// the library screens.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { clampRoi, roiAt, roiBrightness, BlinkFinder, SampleBatcher, Resampler, pcmToB64, b64ToPcm, rmsDb, lightAdvice, captureUnavailableReason } from '../js/lib/capture.js';
import { toMorse, morsePattern } from '../js/lib/morse.js';
import MockBackend from '../js/backends/mock.js';
import { methodsFor, METHODS } from '../js/screens/radio.js';
import { groupChapters } from '../js/screens/help.js';
import { groupCategory, sourcesOf } from '../js/screens/guides.js';
import { methodFor } from '../js/screens/hardware.js';

const W = 80, H = 60;
/** RGBA frame: dark background with a w×h light of brightness `v` (0..255) at (x, y) pixels. */
function frame(v, x = 60, y = 10, w = 10, h = 10, bg = 30) {
  const d = new Uint8ClampedArray(W * H * 4);
  for (let i = 0; i < W * H; i++) { d[4 * i] = d[4 * i + 1] = d[4 * i + 2] = bg; d[4 * i + 3] = 255; }
  for (let yy = y; yy < y + h; yy++) for (let xx = x; xx < x + w; xx++) { const i = (yy * W + xx) * 4; d[i] = d[i + 1] = d[i + 2] = v; }
  return d;
}
const pattern = (text, wpm) => morsePattern(toMorse(text), wpm);
const isOn = (pat, t) => { let a = 0; for (let i = 0; i < pat.length; i++) { if (t < a + pat[i]) return i % 2 === 0; a += pat[i]; } return false; };

test('region brightness and clamping', () => {
  assert.deepEqual(clampRoi({ x: 0.95, y: -1, w: 0.2, h: 0.2 }), { x: 0.8, y: 0, w: 0.2, h: 0.2 });
  assert.equal(clampRoi({ x: 0, y: 0, w: 0, h: 0 }).w, 0.02);
  const r = roiAt(0.5, 0.5);
  assert.ok(Math.abs(r.x - 0.4) < 1e-9 && Math.abs(r.w - 0.2) < 1e-9);
  const lit = frame(255), dark = frame(30);
  const roi = { x: 60 / W, y: 10 / H, w: 10 / W, h: 10 / H };
  assert.ok(roiBrightness(lit, W, H, roi, 1) > 0.95);
  assert.ok(Math.abs(roiBrightness(dark, W, H, roi, 1) - 30 / 255) < 0.01);
  assert.ok(roiBrightness(lit, W, H, { x: 0, y: 0.5, w: 0.3, h: 0.3 }) < 0.2, 'elsewhere stays dark');
});

test('auto-find locks onto the blinking light, not a steady lamp', () => {
  const f = new BlinkFinder(8, 6);
  assert.equal(f.best(), null);
  for (let i = 0; i < 40; i++) {
    const d = frame(i % 3 === 0 ? 250 : 30, 60, 10); // blinking at top right
    for (let yy = 40; yy < 50; yy++) for (let xx = 5; xx < 15; xx++) { const k = (yy * W + xx) * 4; d[k] = d[k + 1] = d[k + 2] = 255; } // steady lamp bottom left
    f.push(d, W, H);
  }
  const b = f.best();
  assert.ok(b && b.score > 0.1);
  const cx = b.roi.x + b.roi.w / 2, cy = b.roi.y + b.roi.h / 2;
  assert.ok(cx > 0.7 && cy < 0.4, 'found at ' + cx + ',' + cy);
});

test('sample batcher drops duplicate frames and batches by count or time', () => {
  const b = new SampleBatcher(3, 1000);
  assert.equal(b.add(0, 0.1), null);
  assert.equal(b.add(0, 0.2), null, 'duplicate timestamp ignored');
  assert.equal(b.add(33, 1.4), null);
  const out = b.add(66, 0.5);
  assert.deepEqual(out, { times_ms: [0, 33, 66], levels: [0.1, 1, 0.5] });
  const t = new SampleBatcher(100, 100);
  t.add(0, 0); assert.ok(t.add(120, 1));
});

test('resampler keeps the tone and the duration (48 kHz → 8 kHz, 8 kHz → 16 kHz)', () => {
  const r = new Resampler(48000, 8000);
  const sine = new Float32Array(48000).map((_, i) => 0.5 * Math.sin(2 * Math.PI * 700 * i / 48000));
  let out = [];
  for (let i = 0; i < sine.length; i += 4096) out.push(...r.push(sine.subarray(i, i + 4096)));
  assert.ok(Math.abs(out.length - 8000) <= 2, 'length ' + out.length);
  let zc = 0; for (let i = 1; i < out.length; i++) if ((out[i - 1] < 0) !== (out[i] < 0)) zc++;
  assert.ok(Math.abs(zc / 2 - 700) < 10, 'frequency ' + zc / 2);
  const up = new Resampler(8000, 16000).push(new Float32Array(800).fill(0.25));
  assert.ok(Math.abs(up.length - 1600) <= 2);
  assert.throws(() => new Resampler(0, 8000));
  const pcm = Int16Array.from([0, 1, -1, 32767, -32768, 1234]);
  assert.deepEqual([...b64ToPcm(pcmToB64(pcm))], [...pcm]);
  assert.equal(rmsDb(new Float32Array(10)), -120);
  assert.ok(Math.abs(rmsDb(new Float32Array(100).fill(1)) - 0) < 1e-9);
});

test('light advice and unavailable reasons', () => {
  assert.equal(lightAdvice([0.01, 0.02, 0.015]), 'too_dark');
  assert.equal(lightAdvice([0.5, 0.51, 0.505]), 'no_blink');
  assert.equal(lightAdvice([0.99, 0.98]), 'saturated');
  assert.equal(lightAdvice([0.1, 0.8, 0.1]), null);
  assert.equal(captureUnavailableReason({ mediaDevices: true }), null);
  assert.match(captureUnavailableReason({ mediaDevices: false, secureContext: false }, 'camera'), /HTTPS|localhost|single-file/);
  assert.match(captureUnavailableReason({ mediaDevices: false, secureContext: true }, 'microphone'), /microphone/);
});

const fresh = () => new MockBackend({ persist: false, speed: 0.001, autoplay: false, latency: 0 });

test('mock morse.rx decodes light (30 fps, noisy) and audio (resampled 48 kHz tone)', async () => {
  const b = fresh();
  const texts = [];
  b.on('morse.text', d => texts.push(d.text));
  const { session } = await b.call('morse.rx.start', { source: 'light' });
  const pat = pattern('SOS HELP', 13);
  const total = pat.reduce((a, c) => a + c, 0) + 1500;
  const batch = new SampleBatcher(30, 250);
  let st;
  for (let t = 0; t < total; t += 33.4) {
    const out = batch.add(t, (isOn(pat, t) ? 0.75 : 0.25) + (Math.random() - 0.5) * 0.06);
    if (out) st = await b.call('morse.rx.feed', { session, ...out });
  }
  st = await b.call('morse.rx.status', { session });
  assert.equal(st.transcript.trim(), 'SOS HELP');
  assert.ok(st.wpm >= 10 && st.wpm <= 16, 'wpm ' + st.wpm);
  for (const k of ['transcript', 'chars', 'wpm', 'on', 'level', 'threshold', 'quality', 'pending']) assert.ok(k in st, k);
  await new Promise(r => setTimeout(r, 5));
  assert.ok(texts.join('').includes('HELP'));
  assert.deepEqual(await b.call('morse.rx.stop', { session }), { transcript: st.transcript });
  await assert.rejects(b.call('morse.rx.status', { session }), e => e.code === 'not_found');

  const a = await b.call('morse.rx.start', { source: 'audio', sample_rate: 8000 });
  const ap = pattern('CQ 73', 18);
  const res = new Resampler(48000, 8000);
  const dur = ap.reduce((x, y) => x + y, 0) + 800;
  const buf = new Float32Array(Math.round(dur * 48)).map((_, i) => (isOn(ap, i / 48) ? 0.4 * Math.sin(2 * Math.PI * 820 * i / 48000) : 0) + (Math.random() - 0.5) * 0.02);
  for (let i = 0; i < buf.length; i += 4096) await b.call('morse.rx.feed', { session: a.session, pcm_b64: pcmToB64(res.push(buf.subarray(i, i + 4096))) });
  const sa = await b.call('morse.rx.status', { session: a.session });
  assert.equal(sa.transcript.trim(), 'CQ 73');
  assert.ok(Math.abs(sa.tone_hz - 820) <= 20, 'tone ' + sa.tone_hz);
  assert.ok(sa.snr_db > 10);
  await b.call('morse.rx.reset', { session: a.session });
  assert.equal((await b.call('morse.rx.status', { session: a.session })).transcript, '');
  await assert.rejects(b.call('morse.rx.start', { source: 'smoke' }), e => e.code === 'bad_params');
  b.close();
});

test('mock implements every §6 core method; radios and hardware follow the contract', async () => {
  const spec = readFileSync(fileURLToPath(new URL('../../docs/RPC_API.md', import.meta.url)), 'utf8');
  const sec = spec.slice(spec.indexOf('## 6.'), spec.indexOf('## 7.') > 0 ? spec.indexOf('## 7.') : undefined);
  const methods = [...sec.matchAll(/^\| `([a-z_.]+)` \|/gm)].map(m => m[1]).filter(m => !m.startsWith('host.'));
  assert.ok(methods.includes('morse.rx.feed') && methods.includes('rf.radios') && methods.includes('rf.hardware'), methods.join());
  const b = fresh();
  const known = new Set(Object.keys((await import('../js/backends/mock.js'))._MOCK_METHODS));
  for (const m of methods) assert.ok(known.has(m), m);
  const { radios } = await b.call('rf.radios', { query: 'uv-5r' });
  assert.ok(radios.length >= 1);
  for (const k of ['id', 'maker', 'model', 'kind', 'connector', 'vox', 'verified', 'settings', 'recommended']) assert.ok(k in radios[0], k);
  const { methods: hw } = await b.call('rf.hardware', { platform: 'ios' });
  assert.ok(hw.length > 5);
  for (const m of hw) {
    for (const k of ['id', 'title', 'what_it_connects', 'platforms', 'items', 'manual_chapter', 'proven']) assert.ok(k in m, m.id + ' ' + k);
    assert.ok(['emulated', 'untested hardware'].includes(m.proven));
    assert.match(m.manual_chapter, /^embermesh\//);
  }
  assert.equal(methodFor(hw, { preset: 'acoustic' }).id, 'radio_acoustic');
  assert.equal(methodFor(hw, { link_type: 'meshtastic' }).id, 'meshtastic');
  await assert.rejects(b.call('rf.hardware', { platform: 'amiga' }), e => e.code === 'bad_params');
  b.close();
});

test('radio wizard offers only what this host can do', () => {
  const audio = { kind: 'audio', available: true, config: [{ key: 'ptt', type: 'select' }] };
  const web = methodsFor([audio, { kind: 'serial', available: true }, { kind: 'ble', available: true }], { kind: 'handheld', builtin_tnc: false });
  assert.ok(web.every(m => m.ok));
  const ios = methodsFor([{ kind: 'audio', available: true, config: [] }], { kind: 'handheld' });
  assert.equal(ios.find(m => m.id === 'handheld_ptt').ok, false);
  assert.match(ios.find(m => m.id === 'handheld_ptt').why, /PTT/);
  assert.equal(ios.find(m => m.id === 'tnc').ok, false);
  const iosHint = methodsFor([audio], null, { serial_ptt: false, presets_supported: ['handheld_vox', 'acoustic', 'sound_card', 'custom'], presets_unsupported: { handheld_ptt: 'iPhone cannot key a PTT line.' } });
  assert.equal(iosHint.find(m => m.id === 'handheld_ptt').ok, false);
  assert.equal(iosHint.find(m => m.id === 'handheld_ptt').why, 'iPhone cannot key a PTT line.');
  assert.equal(iosHint.find(m => m.id === 'handheld_vox').ok, true);
  const none = methodsFor([], null);
  assert.ok(none.every(m => !m.ok && m.why));
  assert.deepEqual(METHODS.map(m => m.id), ['handheld_vox', 'handheld_ptt', 'acoustic', 'tnc', 'sound_card']);
});

test('instructions group the manual by task with this platform first', () => {
  const g = id => ({ id: 'embermesh/' + id, title: 'EmberMesh 1: ' + id });
  const groups = groupChapters([g('troubleshooting'), g('quick-start'), g('app-web'), g('app-android'), g('new-chapter'), { id: 'water/x' }], 'android');
  assert.deepEqual(groups.map(x => x[0]), ['Your device', 'Start here', 'When something does not work', 'More']);
  assert.deepEqual(groups[0][1].map(x => x.id), ['embermesh/app-android', 'embermesh/app-web']);
  assert.deepEqual(groupChapters([], 'web'), []);
});

test('library: books grouped, sorted, sources counted', () => {
  const gs = [
    { id: 'a/zebra', title: 'Zebra', bytes: 10, source_title: 'S1' },
    { id: 'a/b-2', title: 'Book: Two', part_of: 'a/b', order: 2, bytes: 5, source_title: 'Big Book' },
    { id: 'a/b-1', title: 'Book: One', part_of: 'a/b', order: 1, bytes: 5, source_title: 'Big Book' },
    { id: 'a/apple', title: 'The Apple', bytes: 100, source_title: 'S1' },
  ];
  const az = groupCategory(gs, 'title');
  assert.deepEqual(az.map(i => i.book ? 'B:' + i.title : i.g.id), ['a/apple', 'B:Big Book', 'a/zebra']);
  assert.deepEqual(az[1].chapters.map(c => c.id), ['a/b-1', 'a/b-2']);
  assert.equal(groupCategory(gs, 'books')[0].book, 'a/b');
  assert.equal(groupCategory(gs, 'size')[0].g.id, 'a/apple');
  assert.deepEqual(sourcesOf(gs).map(s => [s.source_title, s.count]), [['Big Book', 2], ['S1', 2]]);
});
