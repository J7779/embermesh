// Settings: profile, region/units, theme, low power, mesh relay options, position, time,
// identity backup/restore, storage passphrase + lock, duress, panic wipe, sneakernet bundles, about.
import { h, clear, debounce, append } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { toast, toastError, sheet, confirmDialog, showQr, copyText, segmented, holdButton, errorText, pickFile, saveFile, qrSvg, textToB64, b64ToText } from '../lib/ui.js';
import { renderForm } from '../lib/form.js';
import { formatPos, validPos, dateTime, formatBytes, timeAgo } from '../lib/format.js';
import { REGIONS } from './setup.js';
import { meshFields, validateSettings, SETTINGS_DEFAULTS, hostActions } from '../lib/core_spec.js';
import { APP_LICENCE, THIRD_PARTY, licenceSummary } from '../lib/about.js';


/** Split a base64 bundle into QR-sized frames: "EMB1 <i>/<n> <data>". Pure; exported for tests. */
export function chunkForQr(b64, size = 700) {
  const n = Math.max(1, Math.ceil(b64.length / size));
  const out = [];
  for (let i = 0; i < n; i++) out.push('EMB1 ' + (i + 1) + '/' + n + ' ' + b64.slice(i * size, (i + 1) * size));
  return out;
}

/** Collect QR frames; returns {done, data_b64?, have, total} — tolerant of repeats/out-of-order. */
export function createQrAssembler() {
  const parts = new Map();
  let total = 0;
  return {
    add(text) {
      const m = /^EMB1 (\d{1,4})\/(\d{1,4}) ([A-Za-z0-9+/=]*)$/.exec(String(text).trim());
      if (!m) return { error: 'Not an EmberMesh bundle QR code', have: parts.size, total };
      const i = +m[1], n = +m[2];
      if (n < 1 || i < 1 || i > n || (total && n !== total)) return { error: 'QR code from a different bundle', have: parts.size, total };
      total = n;
      parts.set(i, m[3]);
      if (parts.size === total) {
        let s = '';
        for (let k = 1; k <= total; k++) s += parts.get(k);
        return { done: true, data_b64: s, have: total, total };
      }
      return { done: false, have: parts.size, total };
    },
  };
}

export default async function render(app, params, root) {
  if (params && params.rest && params.rest[0] === 'accessibility') return (await import('./accessibility.js')).default(app, params, root);
  const page = h('div', { class: 'page' });
  root.appendChild(page);
  const info = await app.refreshInfo().catch(() => app.store.get('info') || {});
  const settings = await app.refreshSettings();
  const sec = await app.api.try('security.status', {}, null, true);
  const hostInfo = app.store.get('host') || {};

  // Sections other than the everyday ones are secondary: Simple mode tucks them under "More options".
  const SECONDARY = new Set(['Mesh & relaying', 'Position', 'Clock', 'Identity', 'Security', 'Sneakernet bundles', 'This device', 'About']);
  const section = (title, ...kids) => h('section', { class: 'card stack', 'aria-label': title, 'data-secondary': SECONDARY.has(title) ? '' : null }, h('h2', null, title), ...kids);
  const save = async patch => { try { await app.saveSettings(patch); } catch (e) { toastError(e, 'Not saved'); } };

  // ---- profile
  const pName = h('input', { type: 'text', value: info.name || '', maxlength: 40 });
  const pCall = h('input', { type: 'text', value: info.callsign || '', maxlength: 12, autocapitalize: 'characters' });
  const pStatus = h('input', { type: 'text', value: info.status || '', maxlength: 80, placeholder: 'e.g. At the school shelter, have water' });
  const profile = section('Profile',
    h('label', { class: 'field-label' }, 'Name', pName),
    h('label', { class: 'field-label', 'data-secondary': '' }, 'Callsign (licensed amateurs only)', pCall),
    h('label', { class: 'field-label', 'data-secondary': '' }, 'Status', pStatus),
    h('button', {
      class: 'btn primary', onclick: async () => {
        if (!pName.value.trim()) return pName.focus();
        try { await app.api.call('node.set_profile', { name: pName.value.trim(), callsign: pCall.value.trim().toUpperCase(), status: pStatus.value.trim() }); await app.refreshInfo(); toast('Profile saved', { kind: 'ok' }); } catch (e) { toastError(e); }
      },
    }, 'Save profile'));

  // ---- region, units, appearance
  const region = h('select', { onchange: () => save({ region: region.value }) }, REGIONS.map(([v, l]) => h('option', { value: v, selected: v === (settings.region || 'US') }, l)));
  const lowPower = h('input', { type: 'checkbox', checked: !!settings.low_power, onchange: () => save({ low_power: lowPower.checked }) });
  const appearance = section('Region & display',
    h('label', { class: 'field-label' }, 'Region', region),
    h('div', { class: 'field-label' }, 'Units'),
    segmented([{ value: 'metric', label: 'Metric' }, { value: 'imperial', label: 'Imperial' }], settings.units === 'imperial' ? 'imperial' : 'metric', v => save({ units: v }), 'Units'),
    h('div', { class: 'field-label' }, 'Theme'),
    segmented([{ value: 'dark', label: 'Dark' }, { value: 'light', label: 'Light' }, { value: 'night', label: 'Night (red)' }], settings.theme || 'dark', v => save({ theme: v }), 'Theme'),
    h('p', { class: 'muted small' }, 'Night mode uses only dim red to protect your night vision. Dark saves battery on OLED screens.'),
    h('label', { class: 'check' }, lowPower, h('span', null, 'Low-power mode (no animation, fewer refreshes)')));

  // ---- mesh
  const meshForm = renderForm(meshFields(), { ...SETTINGS_DEFAULTS, ...settings }, { check: validateSettings, onChange: debounce(v => { if (!Object.keys(meshForm.validate()).length) save(v); }, 600) });
  const mesh = section('Mesh & relaying', meshForm.el);

  // ---- position
  const posOut = h('div', { class: 'mono small' });
  const showPos = p => { posOut.textContent = validPos(p) ? formatPos(p) + (p.accuracy ? ' ±' + Math.round(p.accuracy) + ' m' : '') + ' · ' + timeAgo(p.ts) : 'Unknown'; };
  const cur = await app.api.try('position.get', {}, null, true);
  showPos(cur && cur.position);
  const lat = h('input', { type: 'text', inputmode: 'decimal', placeholder: 'Latitude, e.g. 39.828' });
  const lon = h('input', { type: 'text', inputmode: 'decimal', placeholder: 'Longitude, e.g. -98.580' });
  const watch = h('input', { type: 'checkbox', onchange: async () => { try { await app.api.call('host.location.watch', { enabled: watch.checked }); } catch (e) { watch.checked = false; toastError(e); } } });
  const sharing = h('select', { onchange: () => save({ position_sharing: sharing.value }) },
    [['off', 'Off — never share'], ['manual', 'Manual — only when I share it'], ['auto', 'Automatic — include in announcements']].map(([v, l]) => h('option', { value: v, selected: v === (settings.position_sharing || 'manual') }, l)));
  const position = section('Position',
    h('label', { class: 'field-label' }, 'Position sharing', sharing),
    h('div', null, h('span', { class: 'muted' }, 'Current: '), posOut),
    app.has('location') ? h('div', { class: 'row' },
      h('button', { class: 'btn', onclick: async () => { try { const p = await app.api.call('host.location.get'); app.store.set({ position: p }); showPos(p); toast('Position updated', { kind: 'ok' }); } catch (e) { toastError(e, 'No GPS fix'); } } }, icon('pin'), 'Update from GPS'),
      h('label', { class: 'check' }, watch, h('span', null, 'Keep updating'))) : h('p', { class: 'muted small' }, 'No GPS on this device — enter your position manually.'),
    h('details', null, h('summary', null, 'Enter position manually'), h('div', { class: 'stack' }, lat, lon,
      h('button', {
        class: 'btn', onclick: async () => {
          const p = { lat: Number(lat.value.replace(',', '.')), lon: Number(lon.value.replace(',', '.')), ts: Date.now() };
          if (!validPos(p)) { toast('Enter latitude −90…90 and longitude −180…180 in decimal degrees', { kind: 'bad' }); return; }
          try { await app.api.call('position.set', p); app.store.set({ position: p }); showPos(p); toast('Position set', { kind: 'ok' }); } catch (e) { toastError(e); }
        },
      }, 'Set position'))));

  // ---- time
  const timeOut = h('span', null, dateTime(info.time_ms) + ' (' + (info.time_source || '?') + ')');
  const time = section('Clock',
    h('p', null, 'Node time: ', timeOut),
    h('p', { class: 'muted small' }, 'Message order and expiry depend on a roughly correct clock. A working clock is never changed by other nodes. If this device has lost its clock, GPS, a verified contact or several agreeing nodes can set it; you can also set it here.'),
    h('button', {
      class: 'btn', onclick: async () => {
        try { const r = await app.api.call('node.set_time', { ts: Date.now(), source: 'manual' }); timeOut.textContent = dateTime(r.time_ms) + ' (manual)'; } catch (e) { toastError(e); }
      },
    }, icon('clock'), 'Set from this device (' + dateTime(Date.now()) + ')'));

  // ---- identity
  const identity = section('Identity',
    h('dl', { class: 'kv' }, h('dt', null, 'Node ID'), h('dd', { class: 'mono' }, info.node_id || '?'), h('dt', null, 'Version'), h('dd', null, String(info.version || '?'))),
    h('div', { class: 'field-label' }, 'Fingerprint words'),
    h('div', { class: 'words' }, (info.fingerprint || []).map(w => h('span', null, w))),
    h('div', { class: 'row' },
      h('button', { class: 'btn', onclick: async () => { try { const r = await app.api.call('identity.card'); showQr(app, 'My contact card', r.card); } catch (e) { toastError(e); } } }, icon('qr'), 'Show my card'),
      h('button', { class: 'btn', onclick: () => backup(app) }, icon('download'), 'Back up identity'),
      h('button', { class: 'btn', onclick: () => restore(app) }, icon('upload'), 'Restore identity')));

  // ---- security
  const security = section('Security', ...securityBody(app, sec));

  // ---- bundles
  const bundles = section('Sneakernet bundles',
    h('p', { class: 'muted small' }, 'Carry messages by hand: export a bundle to a file or a series of QR codes, carry it to another device, and import it there. Bundles contain only signed, already-encrypted mesh packets.'),
    h('div', { class: 'row' },
      h('button', { class: 'btn', onclick: () => exportBundle(app) }, icon('download'), 'Export bundle'),
      h('button', { class: 'btn', onclick: () => importBundle(app) }, icon('upload'), 'Import bundle')));

  // ---- platform extras (host.info.actions, RPC_API §3): e.g. Android share app / start on boot
  const actions = hostActions(hostInfo);
  const device = actions.length ? section('This device', h('p', { class: 'muted small' }, 'Options this ' + (hostInfo.platform || 'device') + ' app offers.'),
    h('ul', { class: 'list' }, actions.map(a => h('li', null, actionRow(app, a))))) : null;

  // ---- about
  const about = section('About',
    h('dl', { class: 'kv small' },
      h('dt', null, 'EmberMesh'), h('dd', null, String(info.version || '?')),
      h('dt', null, 'App'), h('dd', null, String(hostInfo.app_version || '?')),
      h('dt', null, 'Platform'), h('dd', null, String(hostInfo.platform || info.platform || '?')),
      h('dt', null, 'Backend'), h('dd', null, app.backend.kind),
      h('dt', null, 'Connections'), h('dd', null, (hostInfo.endpoint_kinds || []).filter(k => k.available !== false).map(k => k.kind).join(', ') || '—'),
      h('dt', null, 'Device features'), h('dd', null, (hostInfo.features || []).join(', ') || '—'),
      h('dt', null, 'Core features'), h('dd', null, (info.features || []).join(', ') || '—')),
    await licencesSection(app));

  const { settingsEntry } = await import('./accessibility.js');
  append(page, h('div', { class: 'page-head' }, h('h1', null, 'Settings'), h('a', { class: 'btn small', href: '#/help' }, icon('info', { size: 18 }), 'Instructions')), settingsEntry(), profile, appearance, mesh, position, time, identity, security, bundles, device, about);
}

// ---------------------------------------------------------------- licences (About)
async function licencesSection(app) {
  const gi = await app.api.try('guides.info', {}, null, true);
  const lib = licenceSummary(gi && gi.licences);
  return h('details', { class: 'licences' }, h('summary', null, 'Licences'), h('div', { class: 'small stack' },
    h('p', null, h('strong', null, 'EmberMesh'), ' is free software under the ' + APP_LICENCE.name + '. ' + APP_LICENCE.copyright + '. You may use, copy, change and pass it on; keep the licence with it.'),
    h('div', null, h('strong', null, 'Third-party components')),
    h('ul', null, THIRD_PARTY.map(c => h('li', null, c.name + ' ' + c.version + ' — ' + c.licence + ' (' + c.use + ')'))),
    h('p', { class: 'muted' }, 'Full licence texts: LICENSE and THIRD_PARTY_LICENSES.md in the source code. This interface uses no third-party code, fonts or images.'),
    h('div', null, h('strong', null, 'Guide library')),
    h('p', null, 'The guides are not MIT-licensed. Each guide keeps its own licence, shown with its source and attribution at the end of the guide. Only public-domain, CC0, CC BY and CC BY-SA texts are included.'),
    lib.length ? h('ul', null, lib.map(l => h('li', null, l.family + ': ' + l.count + (l.count === 1 ? ' guide' : ' guides')))) : null,
    h('p', null, 'If you pass guides on, keep their attribution. CC BY-SA guides stay under CC BY-SA, also inside copied guide packs. Public-domain status is under US law. Details: guides/LICENSES.md in the source code.')));
}

// ---------------------------------------------------------------- host actions
function actionRow(app, a) {
  const run = async (extra, ctl) => {
    if (a.confirm && !await confirmDialog(a.title, a.confirm, { ok: a.title })) { if (ctl) ctl.checked = !ctl.checked; return; }
    try {
      const r = await app.api.call(a.method, { ...a.params, ...extra });
      toast((r && typeof r.message === 'string' && r.message) || (ctl ? (ctl.checked ? 'On' : 'Off') : 'Done'), { kind: 'ok' });
      const hi = await app.api.try('host.info', {}, null, true);
      if (hi) app.store.set({ host: hi });
      const now = hostActions(hi).find(x => x.id === a.id);
      if (ctl && now && typeof now.checked === 'boolean') ctl.checked = now.checked;
    } catch (e) { if (ctl) ctl.checked = !ctl.checked; toastError(e, a.title); }
  };
  const desc = a.description ? h('div', { class: 'sub' }, a.description) : null;
  if (typeof a.checked === 'boolean') {
    const box = h('input', { type: 'checkbox', checked: a.checked, 'data-action': a.id });
    box.addEventListener('change', () => run({ enabled: box.checked }, box));
    return h('label', { class: 'item check' }, box, h('div', { class: 'grow' }, h('div', { class: 'title' }, a.title), desc));
  }
  return h('div', { class: 'item' }, h('div', { class: 'grow' }, h('div', { class: 'title' }, a.title), desc),
    h('button', { class: 'btn small', 'data-action': a.id, onclick: () => run({}) }, a.title.length > 18 ? 'Open' : a.title));
}

// ---------------------------------------------------------------- identity backup
function backup(app) {
  const p1 = h('input', { type: 'password', autocomplete: 'new-password', placeholder: 'Backup passphrase' });
  const p2 = h('input', { type: 'password', autocomplete: 'new-password', placeholder: 'Repeat passphrase' });
  const out = h('div', { class: 'stack' });
  sheet('Back up identity', h('div', { class: 'stack' },
    h('p', null, 'The backup contains your secret keys, encrypted with this passphrase. Anyone with both can pretend to be you. Store it offline (paper, USB stick).'),
    p1, p2,
    h('button', {
      class: 'btn primary', onclick: async () => {
        if (p1.value.length < 8) { toast('Use at least 8 characters', { kind: 'bad' }); return; }
        if (p1.value !== p2.value) { toast('Passphrases do not match', { kind: 'bad' }); return; }
        try {
          const r = await app.api.call('identity.export', { passphrase: p1.value });
          p1.value = p2.value = '';
          append(clear(out), h('div', { class: 'copybox' }, r.backup),
            h('div', { class: 'row' },
              h('button', { class: 'btn', onclick: () => copyText(r.backup) }, icon('copy'), 'Copy'),
              h('button', { class: 'btn', onclick: () => saveFile(app, 'embermesh-identity-backup.txt', 'text/plain', textToB64(r.backup)).catch(e => toastError(e)) }, icon('download'), 'Save file'),
              h('button', { class: 'btn', onclick: () => showQr(app, 'Identity backup', r.backup, { ecc: 'L' }) }, icon('qr'), 'QR')));
        } catch (e) { toastError(e, 'Backup failed'); }
      },
    }, 'Create backup'), out));
}

function restore(app) {
  const text = h('textarea', { rows: 4, placeholder: 'Paste the backup text' });
  const pass = h('input', { type: 'password', placeholder: 'Backup passphrase' });
  const s = sheet('Restore identity', h('div', { class: 'stack' },
    h('div', { class: 'notice bad' }, 'This replaces the identity on this device. Contacts will see you as the restored identity.'),
    text,
    h('div', { class: 'row' },
      h('button', { class: 'btn', onclick: async () => { try { const f = await pickFile(app, 'text/plain,.txt'); text.value = b64ToText(f.data_b64).trim(); } catch (e) { if (e.code !== 'bad_params') toastError(e); } } }, icon('upload'), 'From file'),
      app.has('qr_scan') ? h('button', { class: 'btn', onclick: async () => { try { text.value = (await app.api.call('host.qr.scan')).text || ''; } catch (e) { toastError(e); } } }, icon('scan'), 'Scan QR') : null),
    pass,
    h('button', {
      class: 'btn danger solid', onclick: async () => {
        if (!text.value.trim()) return text.focus();
        if (!await confirmDialog('Replace identity?', 'The current identity on this device will be replaced.', { ok: 'Replace', danger: true })) return;
        try { await app.api.call('identity.import', { backup: text.value.trim(), passphrase: pass.value }); s.close(); await app.refreshInfo(); toast('Identity restored', { kind: 'ok' }); app.nav('#/settings?r=' + Date.now()); } catch (e) { toastError(e, e.code === 'denied' ? 'Wrong passphrase' : 'Restore failed'); }
      },
    }, 'Restore')));
}

// ---------------------------------------------------------------- security
function securityBody(app, sec) {
  if (!sec) return [h('p', { class: 'muted' }, 'Storage encryption is not available in this build.')];
  const out = [];
  out.push(h('p', null, sec.has_passphrase ? h('span', { class: 'ok-text' }, icon('lock', { size: 16 }), ' Storage is encrypted with a passphrase.') : h('span', { class: 'warn-text' }, 'Storage is not protected by a passphrase. Anyone with this device can read your messages and notes.')));
  out.push(h('div', { class: 'row' },
    h('button', { class: 'btn', onclick: () => setPass(app, sec) }, icon('lock'), sec.has_passphrase ? 'Change passphrase' : 'Set passphrase'),
    sec.has_passphrase ? h('button', { class: 'btn', onclick: async () => { try { await app.api.call('security.lock'); app.setLocked(true); } catch (e) { toastError(e); } } }, icon('lock'), 'Lock now') : null));
  if (sec.has_passphrase) out.push(h('details', null, h('summary', null, 'Duress passphrase' + (sec.has_duress ? ' (set)' : '')), h('div', { class: 'stack' },
    h('p', { class: 'small' }, 'If someone forces you to unlock the app, enter the duress passphrase instead. The app unlocks as usual and keeps your name, settings and connections, but silently erases your identity, contacts, messages, channels and notes first.'),
    h('button', { class: 'btn', onclick: () => setDuress(app) }, sec.has_duress ? 'Change duress passphrase' : 'Set duress passphrase'))));
  out.push(h('details', null, h('summary', { class: 'bad-text' }, 'Panic wipe'), wipeBox(app)));
  return out;
}

function setPass(app, sec) {
  const old = sec.has_passphrase ? h('input', { type: 'password', autocomplete: 'current-password', placeholder: 'Current passphrase' }) : null;
  const n1 = h('input', { type: 'password', autocomplete: 'new-password', placeholder: 'New passphrase (empty to remove)' });
  const n2 = h('input', { type: 'password', autocomplete: 'new-password', placeholder: 'Repeat new passphrase' });
  const s = sheet(sec.has_passphrase ? 'Change passphrase' : 'Set passphrase', h('div', { class: 'stack' },
    h('p', { class: 'small' }, 'Encrypts all messages, keys and notes on this device. There is no recovery if you forget it — keep an identity backup.'),
    old, n1, n2,
    h('button', {
      class: 'btn primary', onclick: async () => {
        if (n1.value !== n2.value) { toast('Passphrases do not match', { kind: 'bad' }); return; }
        if (n1.value && n1.value.length < 6) { toast('Use at least 6 characters', { kind: 'bad' }); return; }
        if (!n1.value && !await confirmDialog('Remove passphrase?', 'Storage will no longer be encrypted at rest.', { ok: 'Remove', danger: true })) return;
        try { await app.api.call('security.set_passphrase', { ...(old ? { old: old.value } : {}), new: n1.value }); s.close(); toast(n1.value ? 'Passphrase set' : 'Passphrase removed', { kind: 'ok' }); app.nav('#/settings?r=' + Date.now()); } catch (e) { toastError(e, e.code === 'denied' ? 'Current passphrase is wrong' : 'Not changed'); }
      },
    }, 'Save')));
}

function setDuress(app) {
  const p = h('input', { type: 'password', autocomplete: 'new-password', placeholder: 'Duress passphrase (different from the real one)' });
  const s = sheet('Duress passphrase', h('div', { class: 'stack' }, p, h('button', {
    class: 'btn primary', onclick: async () => {
      if (p.value.length < 4) return p.focus();
      try { await app.api.call('security.set_duress', { passphrase: p.value }); s.close(); toast('Duress passphrase set', { kind: 'ok' }); } catch (e) { toastError(e); }
    },
  }, 'Save')));
}

function wipeBox(app) {
  const confirmIn = h('input', { type: 'text', placeholder: 'Type ERASE', autocapitalize: 'characters', autocomplete: 'off', 'aria-label': 'Type ERASE to confirm' });
  const btn = holdButton(h('span', null, 'Hold 3 s to erase everything'), 3000, async () => {
    if (confirmIn.value.trim() !== 'ERASE') { toast('Type ERASE first', { kind: 'bad' }); confirmIn.focus(); return; }
    try {
      await app.api.call('security.wipe', { confirm: 'ERASE' });
      try { localStorage.clear(); } catch { /* ignore */ }
      toast('All data erased');
      await app.refreshInfo().catch(() => {});
      app.nav('#/chats');
    } catch (e) { toastError(e, 'Wipe failed'); }
  }, { class: 'btn danger solid block', label: 'Panic wipe: hold for 3 seconds' });
  return h('div', { class: 'stack' },
    h('p', { class: 'small' }, 'Destroys your identity, keys, messages, contacts, notes and settings on this device. It cannot be undone.'),
    confirmIn, btn);
}

// ---------------------------------------------------------------- bundles
async function exportBundle(app) {
  const scope = h('select', null, h('option', { value: 'all' }, 'Everything I carry'), h('option', { value: 'for_node' }, 'Only messages for one person'));
  const contacts = (await app.api.try('contacts.list', {}, { contacts: [] }, true)).contacts || [];
  const node = h('select', { hidden: true }, contacts.map(c => h('option', { value: c.node_id }, c.name || c.node_id)));
  scope.addEventListener('change', () => { node.hidden = scope.value !== 'for_node'; });
  const maxKb = h('select', null, [[0, 'No limit'], [2, '2 KB (one QR code)'], [16, '16 KB'], [256, '256 KB'], [4096, '4 MB']].map(([v, l]) => h('option', { value: String(v) }, l)));
  const out = h('div', { class: 'stack' });
  sheet('Export bundle', h('div', { class: 'stack' },
    h('label', { class: 'field-label' }, 'What to include', scope), node,
    h('label', { class: 'field-label' }, 'Size limit', maxKb),
    h('button', {
      class: 'btn primary', onclick: async () => {
        const p = { scope: scope.value };
        if (scope.value === 'for_node') p.node_id = node.value;
        if (+maxKb.value) p.max_bytes = +maxKb.value * 1024;
        try {
          const r = await app.api.call('bundle.export', p);
          const name = 'embermesh-bundle-' + new Date().toISOString().slice(0, 16).replace(/[:T]/g, '') + '.ember';
          append(clear(out), h('p', null, r.count + ' packets, ' + formatBytes(r.bytes)),
            h('div', { class: 'row' },
              h('button', { class: 'btn', onclick: () => saveFile(app, name, 'application/octet-stream', r.data_b64).then(() => toast('Saved'), e => toastError(e)) }, icon('download'), 'Save file'),
              h('button', { class: 'btn', onclick: () => qrSequence(app, r.data_b64) }, icon('qr'), 'Show as QR codes')));
        } catch (e) { toastError(e, 'Export failed'); }
      },
    }, 'Create bundle'), out));
}

async function qrSequence(app, b64) {
  const frames = chunkForQr(b64);
  let i = 0;
  const box = h('div', { class: 'center' });
  const label = h('p', { class: 'center' });
  const draw = async () => {
    label.textContent = 'Code ' + (i + 1) + ' of ' + frames.length + ' — scan each one on the other device';
    try { const q = await app.api.call('tools.qr', { text: frames[i], ecc: 'L' }); clear(box).appendChild(qrSvg(q.rows, 'Bundle part ' + (i + 1))); } catch (e) { clear(box).appendChild(h('p', { class: 'bad-text' }, errorText(e))); }
  };
  sheet('Bundle as QR codes', h('div', { class: 'stack' }, label, box,
    h('div', { class: 'row', style: { justifyContent: 'space-between' } },
      h('button', { class: 'btn', onclick: () => { i = (i - 1 + frames.length) % frames.length; draw(); } }, icon('back'), 'Previous'),
      h('button', { class: 'btn primary', onclick: () => { i = (i + 1) % frames.length; draw(); } }, 'Next', icon('next')))));
  draw();
}

function importBundle(app) {
  const ta = h('textarea', { rows: 3, placeholder: 'Or paste bundle data (base64)' });
  const status = h('p', { class: 'small', 'aria-live': 'polite' });
  const doImport = async data_b64 => {
    try {
      const r = await app.api.call('bundle.import', { data_b64 });
      status.textContent = 'Imported ' + r.imported + ' new packets (' + r.duplicates + ' already known, ' + r.for_me + ' for you' + (r.rejected ? ', ' + r.rejected + ' rejected as invalid' : '') + ').';
      toast('Bundle imported', { kind: 'ok' });
    } catch (e) { status.textContent = errorText(e); }
  };
  const asm = createQrAssembler();
  sheet('Import bundle', h('div', { class: 'stack' },
    h('div', { class: 'row' },
      h('button', { class: 'btn primary', onclick: async () => { try { const f = await pickFile(app, '.ember,application/octet-stream'); doImport(f.data_b64); } catch (e) { if (e.code !== 'bad_params') toastError(e); } } }, icon('upload'), 'Choose file'),
      app.has('qr_scan') ? h('button', {
        class: 'btn', onclick: async () => {
          try {
            const r = asm.add((await app.api.call('host.qr.scan')).text || '');
            if (r.error) status.textContent = r.error;
            else if (r.done) doImport(r.data_b64);
            else status.textContent = 'Got ' + r.have + ' of ' + r.total + ' codes — scan the next one.';
          } catch (e) { toastError(e); }
        },
      }, icon('scan'), 'Scan QR codes') : null),
    ta,
    h('button', { class: 'btn', onclick: () => { const v = ta.value.trim(); if (!v) return ta.focus(); const r = /^EMB1 /.test(v) ? asm.add(v) : { done: true, data_b64: v.replace(/\s+/g, '') }; if (r.done) doImport(r.data_b64); else status.textContent = r.error || ('Got ' + r.have + ' of ' + r.total); } }, 'Import pasted data'),
    status));
}
