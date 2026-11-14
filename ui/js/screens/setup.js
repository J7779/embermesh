// First-run setup: name, optional callsign, region; explain identity and show the fingerprint.
import { h, clear, append } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { toastError } from '../lib/ui.js';
import { simpleOffer } from './accessibility.js';
import { loadConnectivity, connectChoices, setConnectivity } from '../lib/connectivity.js';

export const REGIONS = [
  ['US', 'United States / Canada (ITU 2)'], ['EU', 'Europe (ITU 1)'], ['UK', 'United Kingdom'], ['AU', 'Australia / NZ'],
  ['JP', 'Japan'], ['KR', 'South Korea'], ['AS', 'South-East Asia (AS923)'], ['IN', 'India'], ['BR', 'Brazil / South America'], ['ZA', 'Southern Africa'], ['OTHER', 'Other / unknown'],
];

export default async function render(app, _p, root) {
  const info = app.store.get('info') || {};
  const settings = app.store.get('settings') || {};
  const wrap = h('div', { class: 'setup' });
  root.appendChild(wrap);

  const name = h('input', { type: 'text', id: 'setup-name', autocomplete: 'nickname', maxlength: 40, value: info.name || '', placeholder: 'e.g. Sam at Oak St shelter' });
  const callsign = h('input', { type: 'text', id: 'setup-call', autocapitalize: 'characters', maxlength: 12, value: info.callsign || '', placeholder: 'Optional, e.g. QX0ABC' });
  const region = h('select', { id: 'setup-region' }, REGIONS.map(([v, l]) => h('option', { value: v, selected: (settings.region || 'US') === v }, l)));
  const err = h('div', { class: 'field-error', role: 'alert' });

  function step1() {
    append(clear(wrap), 
      h('h1', null, 'Welcome to EmberMesh'),
      h('p', null, 'An off-grid messenger and survival library. There are no accounts and no servers: this device talks directly to nearby devices and radios, and carries messages for others until they can be delivered.'),
      h('div', { class: 'card stack' },
        h('div', { class: 'field' }, h('label', { class: 'field-label', for: 'setup-name' }, 'Your name'), name,
          h('div', { class: 'field-help' }, 'Shown to people you message. Use something others will recognise.')),
        h('div', { class: 'field' }, h('label', { class: 'field-label', for: 'setup-call' }, 'Amateur radio callsign'), callsign,
          h('div', { class: 'field-help' }, 'Only if you hold a licence. Required to transmit on ham bands; those links carry signed, unencrypted traffic only.')),
        h('div', { class: 'field' }, h('label', { class: 'field-label', for: 'setup-region' }, 'Region'), region,
          h('div', { class: 'field-help' }, 'Selects the legal radio frequencies and power limits shown in tools.')),
        err,
        h('button', { class: 'btn primary block', onclick: next }, 'Continue', icon('next'))),
      simpleOffer());
    name.focus();
  }

  async function next() {
    const n = name.value.trim();
    if (!n) { err.textContent = 'Please enter a name.'; name.focus(); return; }
    err.textContent = '';
    try {
      const i = await app.api.call('node.set_profile', { name: n, callsign: callsign.value.trim().toUpperCase() });
      await app.api.try('settings.set', { settings: { region: region.value } }, null, true);
      const conn = await loadConnectivity(app).catch(() => null);
      step2(i || info, conn);
    } catch (e) { toastError(e, 'Could not save profile'); }
  }

  // "Connect to people" (RPC_API §11 rev 2): both notices come from the core; Nearby devices is
  // recommended and on, the internet relay off until chosen. "Start using EmberMesh" applies them
  // together with setup_done (hosts start nothing before that).
  let connChoice = null, connCore = false;
  function connectCard(conn) {
    connCore = !!conn;
    const ch = connectChoices(app, conn);
    if (!ch) return null;
    connChoice = ch.choice;
    return h('section', { class: 'card stack setup-connect', 'aria-labelledby': 'setup-connect-h' },
      h('h2', { id: 'setup-connect-h' }, 'Connect to people'),
      h('p', { class: 'small' }, 'Choose how this device finds the people you talk to. You can change both later in Settings → Connections.'),
      ch.el);
  }

  function step2(i, conn) {
    append(clear(wrap), 
      h('h1', null, 'Your identity'),
      h('div', { class: 'card stack' },
        h('p', null, 'This device created a unique cryptographic identity for you. Direct messages are end-to-end encrypted and every message is signed, so nobody can impersonate you.'),
        h('p', null, 'Your node ID:'), h('p', { class: 'mono' }, i.node_id || '?'),
        h('p', null, 'Your fingerprint words — read them aloud over a radio or in person so others can verify it is really you:'),
        h('div', { class: 'words', 'aria-label': 'Fingerprint words' }, (i.fingerprint || []).map(w => h('span', null, w))),
        h('div', { class: 'notice' }, 'Back up your identity in Settings → Identity. If this device is lost, the backup is the only way to keep your name and contacts\' trust.'),
        h('p', null, 'New to EmberMesh? ', h('a', { href: '#/help', onclick: () => finishTo('#/help') }, 'Read the instructions'), ' · ', h('a', { href: '#/links/radio', onclick: () => finishTo('#/links/radio') }, 'Connect a radio'))),
      conn ? connectCard(conn) : null,
      h('div', { class: 'row end' }, h('button', { class: 'btn primary', onclick: finish }, 'Start using EmberMesh')));
  }

  async function finishTo(hash) { await finish(hash); }

  async function finish(to) {
    if (connCore) await setConnectivity(app, { setup_done: true, ...(connChoice ? connChoice() : {}) }).catch(e => toastError(e, 'Connection choices not saved'));
    await app.api.try('kv.set', { ns: 'ui', key: 'setup_done', value: true }, null, true);
    await app.refreshInfo().catch(() => {});
    await app.refreshSettings();
    app.refreshStrip();
    const dest = typeof to === 'string' ? to : '#/chats';
    location.hash = dest;
    app.nav(dest);
  }

  step1();
}
