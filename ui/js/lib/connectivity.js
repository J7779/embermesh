// Nearby devices and the internet relay (docs/RPC_API.md §11 revision 2, docs/CONNECTIVITY.md):
// helpers and the UI pieces shared by first-run setup, the one-time upgrade notice, Settings →
// Connections, the contact page, the add-contact sheet, channel options, the Links screen and the
// status strip.
//
// Honest consent: both texts are the core's own (`nearby.notice`, `internet.notice`) — never written
// here; the internet relay is never preselected and nothing is sent before the user turns it on;
// turning it on or off is one tap either way.
import { h, clear, append } from './dom.js';
import { icon } from './icons.js';
import { sheet, toastError, confirmDialog } from './ui.js';

const UNAVAILABLE = new Set(['not_implemented', 'unsupported']);
/** Relay session links (one per contact / relayed channel) have ids from here (§11). */
export const RELAY_SESSION_BASE = 0x40000000;
export const THIRD_PARTY_LINE = 'The listed relays are free services run by other companies, with no guarantee: they may be slow, change or disappear.';

// ------------------------------------------------------------------ pure helpers

export const relayHost = url => { try { return new URL(url).host; } catch { return String(url || ''); } };
export const isRelaySession = id => Number(id) >= RELAY_SESSION_BASE;

/** The core's notices (never hard-coded here). '' when the core sends none. Pure. */
export const internetNoticeOf = c => (c && c.internet && typeof c.internet.notice === 'string' ? c.internet.notice.trim() : '');
export const nearbyNoticeOf = c => (c && c.nearby && typeof c.nearby.notice === 'string' ? c.nearby.notice.trim() : '');

/**
 * What this host can do. `hostInfo.features`: "nearby" (discovery here), "relay" (TLS websocket).
 * `conn` null = the core has no connectivity.* (older core). Pure.
 */
export function capabilities(hostInfo, conn) {
  const f = new Set((hostInfo && hostInfo.features) || []);
  return { core: !!conn, nearby: !!conn && f.has('nearby'), relay: !!conn && f.has('relay') && !(hostInfo && hostInfo.websocket_tls === false), ready: !!(conn && conn.ready) };
}

/** LAN, link-local or .local host — the only places ws:// may be allowed for a relay. Pure. */
export function isLanHost(host) {
  const hn = String(host || '').toLowerCase().replace(/^\[|\]$/g, '');
  if (hn.endsWith('.local')) return true;
  const m = /^(\d+)\.(\d+)\.\d+\.\d+$/.exec(hn);
  if (m) { const a = +m[1], b = +m[2]; return a === 10 || (a === 172 && b >= 16 && b <= 31) || (a === 192 && b === 168) || (a === 169 && b === 254); }
  return /^f[cd][0-9a-f]{2}:/.test(hn) || /^fe[89ab][0-9a-f]:/.test(hn);
}

/** Problem with a relay URL typed by the user, or null. wss://; ws:// only when allowed AND on the LAN. Pure. */
export function relayUrlProblem(url, allowPlaintext = false, existing = []) {
  const s = String(url || '').trim();
  if (!s) return 'Enter the relay address, for example wss://relay.example.org/mqtt.';
  let u;
  try { u = new URL(s); } catch { return 'That is not a web address. It starts with wss://'; }
  if (u.protocol === 'ws:' && !allowPlaintext) return 'Use wss:// (encrypted). Plain ws:// is only allowed with "Allow an unencrypted connection" switched on, for a hub on your own network.';
  if (u.protocol === 'ws:' && !isLanHost(u.hostname)) return 'An unencrypted ws:// address must be on your own network (a 192.168.x.x, 10.x.x.x or .local address). Use wss:// for anything else.';
  if (u.protocol !== 'wss:' && u.protocol !== 'ws:') return 'A relay address starts with wss://';
  if (!u.hostname) return 'The address needs a host name.';
  if (existing.some(r => r && r.url === s)) return 'That relay is already in the list.';
  if (existing.length >= 8) return 'At most 8 relays.';
  return null;
}

/** Plain-words state of one relay (Relay.state/error). Pure. */
export function relayStateText(r) {
  switch (r && r.state) {
    case 'up': return 'Connected';
    case 'connecting': return 'Connecting…';
    case 'error': return 'Not connected' + (r.error ? ': ' + r.error : '');
    case 'off': return r.enabled ? 'Off' : 'Not used';
    default: return 'Waiting to connect';
  }
}

/** One line for the Connections status. Pure. */
export function connectivitySummary(c) {
  if (!c) return '';
  if (c.ready === false && c.setup_done === false) return 'Not started yet: finish setting up to connect.';
  const parts = [];
  if (c.nearby && c.nearby.enabled) { const n = Number(c.nearby.links) || 0; parts.push(n ? n + (n === 1 ? ' nearby device connected' : ' nearby devices connected') : 'Looking for nearby devices'); } else parts.push('Nearby devices off');
  if (c.internet && c.internet.enabled) {
    const up = (c.internet.relays || []).filter(r => r.state === 'up').length;
    parts.push(up ? 'internet relay connected (' + up + ')' : 'internet relay not connected');
  } else parts.push('internet relay off');
  return parts.join(' · ');
}

/** Why the core removed an automatic link (link.removed reason, §11 rev 2). Pure. */
export function linkRemovedText(reason, name) {
  const what = name ? '“' + name + '”' : 'A connection';
  switch (reason) {
    case 'duplicate': return what + ' was closed because another connection already reaches the same device.';
    case 'nearby_off': return what + ' was closed because Nearby devices was turned off.';
    case 'hello_failed': return what + ' was closed: the other side is not an EmberMesh device.';
    case 'evicted': return what + ' was closed to make room for another nearby device.';
    case 'relay_off': return what + ' was closed because the internet relay was turned off.';
    case 'relay_removed': return what + ' was closed because that relay was switched off or removed.';
    case 'locked': return what + ' was closed because EmberMesh was locked.';
    case 'wipe': return what + ' was closed because the data on this device was erased.';
    default: return what + ' was closed.';
  }
}

/** Relay link status.topics_paused in plain words, or ''. Pure. */
export function topicsPausedText(status) {
  const n = Number(status && status.topics_paused) || 0;
  if (!n) return '';
  return 'Paused for ' + n + (n === 1 ? ' contact or channel' : ' contacts or channels') + ': the relay sent more than expected. It resumes by itself in a few minutes.';
}

/** Kind of an automatic link, for tags and labels. Pure. */
export function autoKind(link) {
  if (!link || !link.auto) return null;
  return link.auto === 'relay' ? { key: 'relay', label: 'Automatic · internet relay' } : link.auto === 'nearby' ? { key: 'nearby', label: 'Automatic · nearby device' } : { key: String(link.auto), label: 'Automatic' };
}

/** Name of a link id for people (relay session ids read "internet relay"). Pure. */
export function linkNameFor(id, links) {
  if (isRelaySession(id)) return 'internet relay';
  const l = (links || []).find(x => x.id === Number(id));
  return l ? (l.name || l.type) : id != null ? 'link ' + id : '';
}

const ago = s => (s < 90 ? 'a minute' : s < 3600 ? Math.round(s / 60) + ' minutes' : s < 172800 ? Math.round(s / 3600) + ' hours' : Math.round(s / 86400) + ' days');

/**
 * Contact.reach (§11 rev 2) in words. Describes reachability, never delivery. Pure.
 * → {state, text}
 */
export function reachText(reach, links) {
  const r = reach || { path: 'none' };
  const heard = Number.isFinite(r.heard_s) ? ' Last heard ' + ago(r.heard_s) + ' ago.' : '';
  const wait = ' Messages wait on this device and are sent when a connection to them exists.';
  switch (r.path) {
    case 'nearby': return { state: 'nearby', text: 'Nearby now (same Wi-Fi).' };
    case 'network': return { state: 'network', text: 'Connected now over ' + (r.link_id != null ? linkNameFor(r.link_id, links) : 'the network') + '.' };
    case 'radio': return { state: 'radio', text: 'In radio range now' + (r.link_id != null ? ' (' + linkNameFor(r.link_id, links) + ')' : '') + '.' };
    case 'relay': return { state: 'relay', text: 'Reachable through the internet relay.' };
    default: return { state: 'none', text: 'Not reachable right now.' + heard + wait };
  }
}

/** What the system will ask when Nearby is turned on, per platform. Pure. */
export function nearbyPromptText(platform) {
  switch (platform) {
    case 'ios': return 'Your iPhone will ask to let EmberMesh find devices on your local network.';
    case 'android': return 'Android may ask to let EmberMesh find devices on your local network.';
    case 'desktop': return 'No permission is needed on this computer.';
    default: return 'Your device may ask for permission to find devices on your local network.';
  }
}

// ------------------------------------------------------------------ data

/** connectivity.get, or null when the core does not have it. */
export async function loadConnectivity(app) {
  try { return await app.api.call('connectivity.get'); } catch (e) { if (UNAVAILABLE.has(e && e.code)) return null; throw e; }
}

/** connectivity.set + store the result for the rest of the UI. */
export async function setConnectivity(app, patch) {
  const c = await app.api.call('connectivity.set', patch);
  app.store.set({ connectivity: c });
  return c;
}

// ------------------------------------------------------------------ UI pieces

/**
 * The two choices with the core's notices (first-run card and the upgrade notice). Returns
 * {el, choice() → {nearby?, internet?}} or null when this app can do neither.
 */
export function connectChoices(app, conn, { idPrefix = 'setup' } = {}) {
  const caps = capabilities(app.store.get('host'), conn);
  if (!caps.core || (!caps.nearby && !caps.relay)) return null;
  const platform = (app.store.get('host') || {}).platform;
  const nb = h('input', { type: 'checkbox', class: 'switch', id: idPrefix + '-nearby', checked: !!(conn.nearby && conn.nearby.enabled) });
  const net = h('input', { type: 'checkbox', class: 'switch', id: idPrefix + '-internet', checked: !!(conn.internet && conn.internet.enabled) });
  const prompt = h('p', { class: 'small muted' }, nearbyPromptText(platform));
  const sync = () => { prompt.hidden = !nb.checked; };
  nb.addEventListener('change', sync);
  sync();
  const el = h('div', { class: 'stack' },
    caps.nearby ? h('div', { class: 'conn-option' },
      h('label', { class: 'check', for: nb.id }, nb, h('span', { class: 'grow' }, h('strong', null, 'Nearby devices'), ' ', h('span', { class: 'tag ok' }, 'recommended'), h('br'), h('span', { class: 'small muted' }, nearbyNoticeOf(conn)))),
      prompt) : null,
    caps.relay ? h('div', { class: 'conn-option' },
      h('label', { class: 'check', for: net.id }, net, h('span', { class: 'grow' }, h('strong', null, 'Reach contacts over the internet'), h('br'), h('span', { class: 'small muted' }, internetNoticeOf(conn)))),
      h('p', { class: 'small muted' }, 'Off unless you switch it on. Nothing is sent to a relay before that. ' + THIRD_PARTY_LINE)) : null);
  return { el, choice: () => ({ ...(caps.nearby ? { nearby: nb.checked } : {}), ...(caps.relay ? { internet: net.checked } : {}) }) };
}

/**
 * The one-time notice for installs that were set up before connectivity existed (setup_done false
 * on a node that already has a name). Acknowledging it records the choices and setup_done.
 */
export function upgradeNotice(app, conn) {
  const ch = connectChoices(app, conn, { idPrefix: 'upgrade' });
  return new Promise(resolve => {
    let done = false;
    const apply = async () => {
      try { await setConnectivity(app, { setup_done: true, ...(ch ? ch.choice() : {}) }); done = true; s.close(); resolve(true); }
      catch (e) { toastError(e, 'Not saved'); }
    };
    const s = sheet('New: connect to people', h('div', { class: 'stack' },
      h('p', null, 'EmberMesh can now connect to people by itself. Choose what it may do; you can change it any time in Settings → Connections.'),
      ch ? ch.el : null,
      h('div', { class: 'row end' }, h('button', { class: 'btn primary', type: 'button', onclick: apply }, 'Continue'))),
    { modal: true, onClose: () => { if (!done) resolve(false); } });
  });
}

/**
 * The quiet one-tap offer on the contact page and in the add-contact sheet (relay off only).
 * Returns null when the host/core cannot do the relay or it is already on.
 */
export function internetOffer(app, conn, opts = {}) {
  const caps = capabilities(app.store.get('host'), conn);
  if (!caps.relay || (conn.internet && conn.internet.enabled)) return null;
  const btn = h('button', { class: 'btn small', type: 'button' }, icon('globe', { size: 16 }), 'Reach this contact over the internet');
  btn.addEventListener('click', async () => {
    btn.disabled = true;
    try {
      await setConnectivity(app, { internet: true });
      clear(box).appendChild(h('p', { class: 'small ok-text' }, 'Internet relay is on. You can turn it off in Settings → Connections.'));
      opts.onChange && opts.onChange();
    } catch (e) { btn.disabled = false; toastError(e, 'Could not turn on the internet relay'); }
  });
  const box = h('div', { class: 'conn-offer stack' },
    h('p', { class: 'small muted' }, internetNoticeOf(conn)), h('div', null, btn));
  return box;
}

/**
 * Settings → Connections. Two switches, live status, relay list editor with each default relay's
 * note from the core. Re-renders on connectivity.changed. Returns {el, cleanup}.
 */
export function connectionsSection(app, initial) {
  let c = initial;
  const el = h('section', { class: 'card stack connections', id: 'connections', 'aria-labelledby': 'connections-h' });
  const host = () => app.store.get('host') || {};
  const render = () => {
    const caps = capabilities(host(), c);
    clear(el);
    el.appendChild(h('h2', { id: 'connections-h', tabindex: '-1' }, 'Connections'));
    if (!caps.core) { el.appendChild(h('p', { class: 'muted small' }, 'This version of EmberMesh connects only through the links you add under Links & radios.')); return; }
    el.appendChild(h('p', { class: 'small conn-summary', role: 'status' }, connectivitySummary(c)));
    // nearby
    const nb = h('input', { type: 'checkbox', class: 'switch', id: 'conn-nearby', checked: !!c.nearby.enabled, disabled: !caps.nearby });
    nb.addEventListener('change', async () => {
      nb.disabled = true;
      try { c = await setConnectivity(app, { nearby: nb.checked }); } catch (e) { nb.checked = !nb.checked; toastError(e, 'Not saved'); }
      render();
    });
    el.appendChild(h('div', { class: 'conn-option' },
      h('label', { class: 'check', for: 'conn-nearby' }, nb, h('span', { class: 'grow' }, h('strong', null, 'Nearby devices'), h('br'), h('span', { class: 'small muted' }, nearbyNoticeOf(c)))),
      caps.nearby ? h('p', { class: 'small muted' }, (c.nearby.enabled ? '' : nearbyPromptText(host().platform) + ' ') + 'Bluetooth and radio links are added by hand under Links & radios.')
        : h('p', { class: 'small muted' }, 'This app cannot find devices by itself here: a web page cannot search the network. Join a hub, or pair with a code under Links & radios.')));
    // internet relay
    const net = h('input', { type: 'checkbox', class: 'switch', id: 'conn-internet', checked: !!c.internet.enabled, disabled: !caps.relay });
    net.addEventListener('change', async () => {
      net.disabled = true;
      try { c = await setConnectivity(app, { internet: net.checked }); } catch (e) { net.checked = !net.checked; toastError(e, 'Not saved'); }
      render();
    });
    el.appendChild(h('div', { class: 'conn-option' },
      h('label', { class: 'check', for: 'conn-internet' }, net, h('span', { class: 'grow' }, h('strong', null, 'Internet relay'), h('br'), h('span', { class: 'small muted' }, internetNoticeOf(c)))),
      caps.relay ? (c.internet.enabled && c.internet.consent_ts ? h('p', { class: 'small muted' }, 'Turned on ' + new Date(c.internet.consent_ts).toLocaleDateString() + '. Messages for a contact whose app is closed wait on this device until you are both online.') : null)
        : h('p', { class: 'small muted' }, host().websocket_tls === false ? 'Not available in this app yet: it cannot make encrypted (TLS) connections.' : 'Not available in this app.')));
    if (!caps.relay) return;
    el.appendChild(relayList(app, c, nc => { c = nc; render(); }));
  };
  render();
  const off = initial ? app.on('connectivity.changed', d => { if (d && d.nearby && d.internet) { c = d; app.store.set({ connectivity: d }); render(); } }) : null;
  return { el, cleanup: () => { if (typeof off === 'function') off(); }, render };
}

function relayList(app, c, update) {
  const relays = (c.internet.relays || []).map(r => ({ ...r }));
  const save = async list => {
    try { update(await setConnectivity(app, { relays: list.map(({ url, enabled, allow_plaintext, username, password }) => ({ url, enabled, ...(allow_plaintext ? { allow_plaintext: true } : {}), ...(username ? { username } : {}), ...(password ? { password } : {}) })) })); }
    catch (e) { toastError(e, 'Relay list not saved'); }
  };
  const rows = relays.map((r, i) => {
    const sw = h('input', { type: 'checkbox', class: 'switch', checked: !!r.enabled, 'aria-label': 'Use ' + relayHost(r.url) });
    sw.addEventListener('change', () => { relays[i].enabled = sw.checked; save(relays); });
    const dot = r.state === 'up' ? 'up' : r.state === 'error' ? 'error' : r.state === 'connecting' ? 'connecting' : '';
    return h('li', { class: 'item relay-row' },
      h('span', { class: 'dot ' + dot, 'aria-hidden': 'true' }),
      h('div', { class: 'grow' },
        h('div', { class: 'title' }, relayHost(r.url)),
        // a default relay's own description comes from the core (why it is off; no guarantee)
        r.note ? h('div', { class: 'sub wrap' }, r.note) : h('div', { class: 'sub wrap' }, r.allow_plaintext ? 'Added by you · unencrypted connection' : 'Added by you'),
        h('div', { class: 'sub wrap' }, relayStateText(r)),
        r.allow_plaintext ? h('div', { class: 'sub wrap warn-text' }, 'Unencrypted: anyone on your network can see which topics you use.') : null),
      sw,
      r.note ? null : h('button', { class: 'btn small ghost icon-only', type: 'button', 'aria-label': 'Remove ' + relayHost(r.url), onclick: async () => {
        if (!await confirmDialog('Remove this relay?', relayHost(r.url) + ' will no longer be used.', { ok: 'Remove', danger: true })) return;
        save(relays.filter((_, k) => k !== i));
      } }, icon('trash', { size: 18 })));
  });
  // add a relay
  const url = h('input', { type: 'text', id: 'relay-url', inputmode: 'url', autocapitalize: 'off', spellcheck: 'false', placeholder: 'wss://relay.example.org/mqtt' });
  const plain = h('input', { type: 'checkbox', id: 'relay-plain' });
  const user = h('input', { type: 'text', id: 'relay-user', autocapitalize: 'off', spellcheck: 'false', autocomplete: 'off' });
  const pass = h('input', { type: 'password', id: 'relay-pass', autocomplete: 'new-password' });
  const err = h('div', { class: 'field-error', role: 'alert' });
  const add = h('button', { class: 'btn', type: 'button' }, icon('plus'), 'Add relay');
  add.addEventListener('click', async () => {
    const u = url.value.trim();
    const p = relayUrlProblem(u, plain.checked, relays);
    err.textContent = p || '';
    if (p) { url.focus(); return; }
    if (plain.checked && !await confirmDialog('Use an unencrypted connection?', 'Messages stay sealed end to end, but anyone on your network can see which topics you use and when. Use this only for a hub on your own network.', { ok: 'Use it anyway', danger: true })) return;
    save([...relays, { url: u, enabled: true, ...(plain.checked ? { allow_plaintext: true } : {}), ...(user.value.trim() ? { username: user.value.trim() } : {}), ...(pass.value ? { password: pass.value } : {}) }]);
  });
  return h('div', { class: 'stack relays', 'data-secondary': '' },
    h('h3', null, 'Relays'),
    h('p', { class: 'small muted' }, THIRD_PARTY_LINE + ' Your own hub reachable from the internet works as a relay with no third party.'),
    h('ul', { class: 'list' }, rows),
    h('details', { class: 'advanced' }, h('summary', null, 'Add your own relay or hub'),
      h('div', { class: 'stack' },
        h('label', { class: 'field-label', for: 'relay-url' }, 'Relay address'), url,
        h('label', { class: 'check' }, plain, h('span', null, 'Allow an unencrypted connection (ws://) — only for a hub on your own network')),
        h('label', { class: 'field-label', for: 'relay-user' }, 'User name (only if the relay needs one)'), user,
        h('label', { class: 'field-label', for: 'relay-pass' }, 'Password (only if the relay needs one; never shown again)'), pass,
        err, add)));
}
