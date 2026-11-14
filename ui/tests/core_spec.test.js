// Tests for the logic added for the real core: spec values, hops/ids, legal mode, audio meter,
// catalogues, host actions, back button, rate limiting, Markdown fixes, and the mock behaving like
// the real core (settings ranges, locked state, interop conversations, node.ready).
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import {
  SETTINGS_SPEC, SETTINGS_DEFAULTS, settingError, validateSettings, meshFields, hopsLabel, convKind, networkTitle,
  amateurFromConfig, linkLegal, usableCallsign, audioMeter, meterPct, compatibleTypes, netplanModes, isLinkRefField,
  linkRefOptions, manualFor, hostActions, validLinkId,
} from '../js/lib/core_spec.js';
import { backAction } from '../js/lib/back.js';
import { createLimiter, unverified, strongPassphrase, passphraseStrength } from '../js/lib/ratelimit.js';
import { withLinkPickers } from '../js/lib/form.js';
import { parseMarkdown, toHtml } from '../js/lib/markdown.js';
import { CORE_LINK_TYPES } from '../js/mock/link_types.js';
import MockBackend from '../js/backends/mock.js';

const spec = readFileSync(fileURLToPath(new URL('../../docs/RPC_API.md', import.meta.url)), 'utf8');

test('settings defaults and ranges match docs/RPC_API.md', () => {
  const m = /Defaults: ([\s\S]*?)A known key/.exec(spec);
  assert.ok(m, 'defaults paragraph present');
  const d = m[1].replace(/`/g, '');
  assert.match(d, new RegExp('store_forward_days:' + SETTINGS_SPEC.store_forward_days.default + ' \\(' + SETTINGS_SPEC.store_forward_days.min + '\\.\\.' + SETTINGS_SPEC.store_forward_days.max + '\\)'));
  assert.match(d, new RegExp('beacon_interval_s:' + SETTINGS_SPEC.beacon_interval_s.default));
  assert.match(d, new RegExp('0 = never, else ' + SETTINGS_SPEC.beacon_interval_s.min + '\\.\\.' + SETTINGS_SPEC.beacon_interval_s.max));
  assert.match(d, new RegExp('default_ttl_hops:' + SETTINGS_SPEC.default_ttl_hops.default + '\\s*\\(' + SETTINGS_SPEC.default_ttl_hops.min + '\\.\\.' + SETTINGS_SPEC.default_ttl_hops.max));
  assert.equal(SETTINGS_DEFAULTS.region, 'US');
  assert.equal(settingError('store_forward_days', 0), '1–30 days');
  assert.equal(settingError('store_forward_days', 30), null);
  assert.equal(settingError('beacon_interval_s', 0), null);
  assert.ok(settingError('beacon_interval_s', 30));
  assert.ok(settingError('default_ttl_hops', 16));
  assert.ok(settingError('theme', 'blue'));
  assert.equal(settingError('unknown_key', 'x'), null);
  assert.deepEqual(Object.keys(validateSettings({ default_ttl_hops: 0, units: 'metric' })), ['default_ttl_hops']);
  const f = meshFields().find(x => x.key === 'store_forward_days');
  assert.equal(f.min, 1); assert.equal(f.max, 30); assert.equal(f.default, 3);
});

test('hops: 0 = direct, null = unknown', () => {
  assert.equal(hopsLabel(0), 'direct');
  assert.equal(hopsLabel(null), 'path unknown');
  assert.equal(hopsLabel(undefined), 'path unknown');
  assert.equal(hopsLabel(1), '1 hop');
  assert.equal(hopsLabel(3), '3 hops');
  assert.equal(hopsLabel(2, true), 'direct');
  assert.equal(validLinkId(0), false);
  assert.equal(validLinkId(3), true);
});

test('conversation ids and network titles', () => {
  assert.equal(convKind('ch:public'), 'channel');
  assert.equal(convKind('dm:0123456789abcdef'), 'dm');
  assert.equal(convKind('io:aprs'), 'interop');
  assert.equal(convKind('dm:xyz'), null);
  assert.equal(networkTitle('aprs'), 'APRS');
  assert.equal(networkTitle('psk31'), 'PSK31');
  assert.equal(networkTitle('foo'), 'FOO');
});

test('legal mode comes from driver config fields and link caps, never ham_band', () => {
  const T = t => CORE_LINK_TYPES.find(x => x.type === t).config;
  assert.equal(amateurFromConfig(T('audio'), { licence: 'amateur' }), true);
  assert.equal(amateurFromConfig(T('audio'), { licence: 'licence_free' }), false);
  assert.equal(amateurFromConfig(T('stream'), { amateur: true }), true);
  assert.equal(amateurFromConfig(T('meshtastic'), { licensed: false }), false);
  assert.equal(amateurFromConfig(T('raw'), {}), null);
  assert.equal(amateurFromConfig(T('kiss'), { ham_band: true, licence: 'licence_free' }), false);
  assert.deepEqual(linkLegal({ caps: { encryption_allowed: false, requires_station_id: true } }), { noEncryption: true, stationId: true });
  assert.equal(usableCallsign('N0CALL'), false);
  assert.equal(usableCallsign(''), false);
  assert.equal(usableCallsign('qx0abc-7'), true);
});

test('audio meter reads the documented flat fields and falls back to modem_status', () => {
  const m = audioMeter({ rx_level_dbfs: -24.5, rx_peak_dbfs: -10, clipping: false, clip_percent: 0, snr_db: 12.5, dcd: true, tx_active: false, frames_ok: 4, frames_bad: 1, rx_audio_age_ms: 20, last_error: '', level_hint: 'ok' });
  assert.equal(m.level, -24.5); assert.equal(m.peak, -10); assert.equal(m.snr, 12.5); assert.equal(m.dcd, true); assert.equal(m.ok, 4); assert.equal(m.hint, 'ok');
  const old = audioMeter({ modem_status: { level_hint: 'no_audio' }, transmitting: true });
  assert.equal(old.hint, 'no_audio'); assert.equal(old.level, -120); assert.equal(old.tx, true); assert.equal(old.snr, null);
  assert.equal(audioMeter({ type: 'raw' }), null);
  assert.equal(audioMeter({ level_hint: 'clipping', rx_level_dbfs: 5 }).clipping, true);
  assert.equal(audioMeter({ rx_level_dbfs: 5 }).level, 0);
  assert.equal(meterPct(-60), 0); assert.equal(meterPct(0), 100); assert.equal(meterPct(-30), 50); assert.equal(meterPct(NaN), 0);
});

test('catalogues: compatible types from the core, net-plan modes from modem names', () => {
  assert.deepEqual(compatibleTypes({ kind: 'ble_peer' }, CORE_LINK_TYPES, { link_types: ['raw', 'stream'] }), ['stream']);
  assert.deepEqual(compatibleTypes({ kind: 'audio' }, CORE_LINK_TYPES, null), ['audio']);
  assert.ok(compatibleTypes({ kind: 'tcp' }, CORE_LINK_TYPES, { link_types: [] }).includes('meshtastic'));
  assert.deepEqual(compatibleTypes({ kind: 'x' }, [], { link_types: ['raw'] }), ['raw']);
  const modes = netplanModes(CORE_LINK_TYPES).map(m => m.value);
  assert.equal(modes[0], 'voice');
  for (const m of ['psk31', 'robust62', 'cw', 'rtty', 'afsk1200', 'lora']) assert.ok(modes.includes(m), m);
  assert.ok(!modes.includes('bpsk31'));
  assert.equal(new Set(modes).size, modes.length);
});

test('link references become pickers, never raw ids', () => {
  assert.equal(isLinkRefField({ key: 'ptt_link_id', type: 'number' }), true);
  assert.equal(isLinkRefField({ key: 'link_idle', type: 'number' }), false);
  const links = [{ id: 1, name: 'Radio', type: 'audio', endpoint: { kind: 'audio' } }, { id: 2, name: 'Digirig', type: 'stream', endpoint: { kind: 'serial' } }];
  const opts = linkRefOptions(links, 1);
  assert.deepEqual(opts.map(o => o.value), [0, 2]);
  const f = withLinkPickers([{ key: 'ptt_link_id', type: 'number', label: 'PTT via serial link' }, { key: 'x', type: 'text' }], opts, { ptt_link_id: 9 });
  assert.equal(f[0].type, 'select');
  assert.deepEqual(f[0].options.map(o => o.value), [0, 2, 9]);
  assert.equal(f[1].type, 'text');
});

test('manual chapters and host actions', () => {
  assert.equal(manualFor('audio', 'audio'), 'embermesh/handheld-audio-modem');
  assert.equal(manualFor('raw', 'websocket'), 'embermesh/hub-setup');
  assert.equal(manualFor('stream', 'ble_peer'), 'embermesh/phone-to-phone');
  assert.equal(manualFor('x', 'y'), 'embermesh/quick-start');
  const acts = hostActions({ actions: [
    { id: 'share', title: 'Share app', method: 'host.app.share' },
    { id: 'boot', title: 'Start on boot', method: 'host.app.start_on_boot', checked: true, params: { a: 1 } },
    { id: 'evil', title: 'Wipe', method: 'security.wipe' },
    { id: 'share', title: 'dup', method: 'host.x' },
    null, { id: 'x', method: 'host.y' },
  ] });
  assert.deepEqual(acts.map(a => a.id), ['share', 'boot']);
  assert.equal(acts[1].checked, true);
  assert.deepEqual(hostActions({}), []);
});

test('Android back button decisions', () => {
  assert.equal(backAction({ sheetOpen: true, hash: '#/chats' }), 'sheet');
  assert.equal(backAction({ overlay: true }), 'overlay');
  assert.equal(backAction({ locked: true, hash: '#/settings' }), null);
  assert.equal(backAction({ hash: '#/chats/ch%3Apublic' }), 'up');
  assert.equal(backAction({ hash: '#/links/add?kind=ble' }), 'up');
  assert.equal(backAction({ hash: '#/settings' }), 'home');
  assert.equal(backAction({ hash: '#/chats' }), null);
  assert.equal(backAction({ hash: '' }), null);
});

test('notification rate limiting, unverified senders, strong passphrases', () => {
  let t = 0;
  const rl = createLimiter({ perSourceMs: 1000, maxPerWindow: 3, windowMs: 10000, now: () => t });
  assert.equal(rl.allow('a'), true);
  assert.equal(rl.allow('a'), false);
  assert.equal(rl.allow('b'), true);
  assert.equal(rl.allow('c'), true);
  assert.equal(rl.allow('d'), false); // window full
  t = 11000;
  assert.equal(rl.allow('a'), true);
  assert.equal(rl.takeSuppressed(), 2);
  assert.equal(unverified({ signed_ok: false }), true);
  assert.equal(unverified({ verified: false }), true);
  assert.equal(unverified({ signed_ok: true }), false);
  const p = strongPassphrase();
  assert.match(p, /^[a-z2-9]{4}(-[a-z2-9]{4}){4}$/);
  assert.notEqual(p, strongPassphrase());
});

test('markdown: bare "http://" does not crash; header-less pipe tables render', () => {
  assert.doesNotThrow(() => parseMarkdown('see http:// and https:// "x" or http://'));
  assert.match(toHtml('go to http://example.org/a.'), /data-href="http:\/\/example.org\/a"/);
  const html = toHtml('Intro\n\n| Severe | > 115 |\n| Mild | 90 |\n\nAfter');
  assert.match(html, /<table><tbody><tr><td>Severe<\/td><td>&gt; 115<\/td><\/tr>/);
  assert.doesNotMatch(html, /<thead>/);
  assert.match(toHtml('a | b\n--|--\n1 | 2'), /<thead>/);
  assert.match(toHtml('just | a pipe'), /<p>just \| a pipe<\/p>/);
});

// ------------------------------------------------------------------ mock behaves like the real core
const fresh = (extra = {}) => new MockBackend({ persist: false, speed: 0.001, autoplay: false, latency: 0, ...extra });

test('mock: settings validation and defaults like the core', async () => {
  const b = fresh({ params: { fresh: '1' } });
  const s = (await b.call('settings.get')).settings;
  assert.equal(s.store_forward_days, 3); assert.equal(s.beacon_interval_s, 1800); assert.equal(s.default_ttl_hops, 7);
  await assert.rejects(b.call('settings.set', { settings: { store_forward_days: 0, units: 'imperial' } }), e => e.code === 'bad_params');
  assert.equal((await b.call('settings.get')).settings.units, 'metric', 'nothing applied');
  assert.equal((await b.call('settings.set', { settings: { beacon_interval_s: 0 } })).settings.beacon_interval_s, 0);
  b.close();
});

test('mock: fresh node is empty with name ""', async () => {
  const b = fresh({ params: { fresh: '1' } });
  assert.equal((await b.call('node.info')).name, '');
  assert.deepEqual((await b.call('contacts.list')).contacts, []);
  assert.deepEqual((await b.call('links.list')).links, []);
  assert.deepEqual((await b.call('mesh.peers')).peers, []);
  assert.deepEqual(await b.call('position.get'), {});
  assert.deepEqual((await b.call('messages.list', { conv: 'ch:public' })).messages, []);
  assert.deepEqual((await b.call('conversations.list')).conversations.map(c => c.id), ['ch:public']);
  b.close();
});

test('mock: locked state like the core, node.ready after unlock', async () => {
  const b = fresh();
  const ready = [];
  b.on('node.ready', () => ready.push(1));
  await b.call('security.set_passphrase', { new: 'pw-12345' });
  await b.call('security.set_duress', { passphrase: 'duress-1' });
  await b.call('security.lock');
  const info = await b.call('node.info');
  assert.equal(info.node_id, ''); assert.deepEqual(info.fingerprint, []); assert.equal(info.card, '');
  assert.equal((await b.call('security.status')).has_duress, false);
  await b.call('links.list');
  await b.call('guides.categories');
  await assert.rejects(b.call('settings.get'), e => e.code === 'locked');
  await assert.rejects(b.call('kv.get', { ns: 'ui', key: 'x' }), e => e.code === 'locked');
  await assert.rejects(b.call('security.unlock', { passphrase: 'nope' }), e => e.code === 'denied');
  await b.call('security.unlock', { passphrase: 'pw-12345' });
  await new Promise(r => setTimeout(r, 5));
  assert.equal(ready.length, 1);
  assert.notEqual((await b.call('node.info')).node_id, '');
  b.close();
});

test('mock: duress unlock looks like a normal unlock (profile, settings, links kept; identity new)', async () => {
  const b = fresh();
  const before = await b.call('node.info');
  await b.call('settings.set', { settings: { theme: 'night' } });
  const linkIds = (await b.call('links.list')).links.map(l => l.id);
  await b.call('security.set_passphrase', { new: 'pw-12345' });
  await b.call('security.set_duress', { passphrase: 'duress-1' });
  await b.call('security.lock');
  const ready = [];
  b.on('node.ready', () => ready.push(1));
  assert.deepEqual(await b.call('security.unlock', { passphrase: 'duress-1' }), { unlocked: true });
  const info = await b.call('node.info');
  assert.notEqual(info.node_id, before.node_id);
  assert.equal(info.name, before.name, 'no first-run screen after duress');
  assert.equal(info.fingerprint.length, 8);
  assert.equal((await b.call('settings.get')).settings.theme, 'night');
  assert.deepEqual((await b.call('links.list')).links.map(l => l.id), linkIds);
  assert.deepEqual((await b.call('contacts.list')).contacts, []);
  assert.equal((await b.call('security.status')).has_duress, true);
  await new Promise(r => setTimeout(r, 5));
  assert.equal(ready.length, 1);
  b.close();
});

test('mock: channels — random key, strength, id collision → busy, endpoint configure keeps id', async () => {
  const b = fresh();
  const r = await b.call('channels.create', { name: 'Camp', random_key: true });
  assert.equal(r.channel.encrypted, true); assert.equal(r.passphrase_strength, undefined);
  assert.equal((await b.call('channels.create', { name: 'Weak', passphrase: 'camp1' })).passphrase_strength, 'weak');
  assert.equal((await b.call('channels.create', { name: 'Strong', passphrase: strongPassphrase() })).passphrase_strength, 'strong');
  const c = (await b.call('channels.create', { name: 'Same', passphrase: 'a long enough passphrase here' })).channel;
  b.channels.get(c.id).passphrase = 'different key, same 32-bit id'; // simulate an id collision
  await assert.rejects(b.call('channels.create', { name: 'Same', passphrase: 'a long enough passphrase here' }), e => e.code === 'busy');
  const { link } = await b.call('links.configure', { id: 2, endpoint: { kind: 'audio', config: { input_device: 'x' } } });
  assert.equal(link.id, 2); assert.equal(link.endpoint.config.input_device, 'x');
  const sos = (await b.call('sos.list')).alerts;
  assert.ok(sos.some(a => a.signed_ok === false) && sos.some(a => a.signed_ok !== false));
  b.close();
});

test('generated passphrase always mixes letters and digits (an all-letter draw is redrawn)', () => {
  // First draw: every byte maps to a letter; second draw: bytes that include digits.
  let calls = 0;
  const rand = n => { const b = new Uint8Array(n); b.fill(calls++ === 0 ? 3 : 0); if (calls > 1) for (let i = 0; i < n; i += 2) b[i] = 25; return b; };
  const p = strongPassphrase(rand);
  assert.match(p, /^([a-z2-9]{4}-){4}[a-z2-9]{4}$/);
  assert.ok(/[0-9]/.test(p) && /[a-z]/.test(p), p);
  assert.equal(calls, 2);
  for (let i = 0; i < 2000; i++) assert.equal(passphraseStrength(strongPassphrase()), 'strong');
});

test('passphrase strength estimate', () => {
  assert.equal(passphraseStrength(''), 'weak');
  assert.equal(passphraseStrength('password1'), 'weak');
  assert.equal(passphraseStrength('Ridge2026!'), 'weak');
  assert.equal(passphraseStrength('correct horse battery staple'), 'fair');
  assert.equal(passphraseStrength(strongPassphrase()), 'strong');
});

test('mock: interop conversations io:<network>, hops 0 = direct, 24-hex ids', async () => {
  const b = fresh();
  const convs = (await b.call('conversations.list')).conversations;
  const io = convs.find(c => c.id === 'io:aprs');
  assert.equal(io.kind, 'interop'); assert.equal(io.title, 'APRS'); assert.equal(io.encrypted, false);
  const { message } = await b.call('interop.send', { link_id: 2, to: 'QX0TO', text: 'hi' });
  assert.equal(message.conv, 'io:aprs'); assert.equal(message.kind, 'interop'); assert.equal(message.interop.to, 'QX0TO');
  assert.match(message.id, /^[0-9a-f]{24}$/);
  const r = await b.call('messages.send', { conv: 'io:aprs', text: 'reply' });
  assert.equal(r.message.via_link, 2);
  await assert.rejects(b.call('messages.send', { conv: 'io:rtty', text: 'x' }), e => e.code === 'not_found');
  await assert.rejects(b.call('interop.send', { link_id: 1, text: 'x' }), e => e.code === 'unsupported');
  const peers = (await b.call('mesh.peers')).peers;
  assert.ok(peers.some(p => p.hops === 0 && p.direct === true));
  assert.ok(peers.some(p => p.hops === null && p.direct === false));
  assert.equal((await b.call('mesh.ping', { node_id: '0123456789abcdef' })).sent, true);
  const ch = (await b.call('channels.create', { name: 'X' })).channel;
  assert.match(ch.id, /^[0-9a-f]{8}$/);
  b.close();
});

test('mock: links use the real catalogue, caps follow licence, audio meter fields, removal event', async () => {
  const b = fresh();
  const types = (await b.call('links.types')).types.map(t => t.type);
  assert.deepEqual(types, ['raw', 'stream', 'kiss', 'audio', 'meshtastic', 'rnode', 'lora_at', 'rtl_tcp']);
  const hi = await b.call('host.info');
  assert.ok(!hi.endpoint_kinds.find(k => k.kind === 'ble_peer').link_types.includes('raw'), 'raw is not offered for ble_peer');
  assert.ok(Array.isArray(hi.actions) && hi.actions.length);
  const { link } = await b.call('links.add', { type: 'audio', config: { licence: 'licence_free' } });
  assert.equal(link.caps.encryption_allowed, true);
  assert.equal(link.persist, true);
  for (const k of ['rx_level_dbfs', 'rx_peak_dbfs', 'clipping', 'clip_percent', 'snr_db', 'dcd', 'tx_active', 'frames_ok', 'frames_bad', 'rx_audio_age_ms', 'last_error', 'level_hint']) assert.ok(k in link.status, k);
  const ham = (await b.call('links.configure', { id: link.id, config: { licence: 'amateur' } })).link;
  assert.equal(ham.caps.encryption_allowed, false);
  assert.equal(ham.caps.requires_station_id, true);
  const ev = [];
  b.on('link.state', d => ev.push(d.link));
  await b.call('links.remove', { id: link.id });
  await new Promise(r => setTimeout(r, 5));
  assert.ok(ev.some(l => l.id === link.id && l.removed === true && l.state === 'down'));
  b.close();
});
