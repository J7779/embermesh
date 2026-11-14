// Links & radios: link list with state/stats/airtime, schema-driven "Add connection" wizard,
// WebRTC QR/copy-paste pairing, hub start/stop, net plan view/edit, legal-mode explanation.
import { h, clear, debounce, append } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { toast, toastError, sheet, confirmDialog, showQr, copyText, empty, errorText, qrSvg, setFill, bar } from '../lib/ui.js';
import { playClass } from '../lib/motion.js';
import { renderForm } from '../lib/form.js';
import { loadHardware, methodFor, needsCard, platformOf } from './hardware.js';
import { timeAgo, formatBytes, formatDuration, formatBitrate, formatFreq, trimNum } from '../lib/format.js';
import { linkLegal, usableCallsign, audioMeter, meterPct, LEVEL_HINTS, METER_TYPES, compatibleTypes, netplanModes, linkRefOptions, manualFor, amateurFromConfig } from '../lib/core_spec.js';

/** Extra plain-language help per endpoint kind (shown in the wizard next to the host's description). */
export const KIND_HELP = {
  websocket: 'Use this when a hub (a laptop or phone running EmberMesh hub) shares a Wi-Fi network. Ask the hub owner for the address shown on their Links screen.',
  ws_server: 'Lets other devices connect to this one over the network. Usually easier: start the Hub below.',
  tcp: 'For software TNCs such as Direwolf (KISS on port 8001), radios with network KISS, or another EmberMesh node.',
  tcp_server: 'Accept TCP connections from other programs or nodes.',
  udp: 'Zero setup: every EmberMesh device on the same Wi-Fi or phone hotspot finds the others automatically.',
  serial: 'USB or RS-232 cable to a TNC, a radio with a built-in TNC, a Meshtastic device or an RNode. Plug it in first, then scan.',
  ble: 'Bluetooth Low Energy TNCs (Mobilinkd, many KISS adapters), Meshtastic devices and RNodes. Turn the device on and keep it close while scanning.',
  ble_peer: 'Phone to phone over Bluetooth, no other hardware. Range is roughly 10–50 m. Both phones must have EmberMesh open.',
  bt_spp: 'Classic Bluetooth serial (older TNCs, some radios). Pair the device in the system Bluetooth settings first.',
  audio: 'Any radio: connect the headphone/mic jack to the radio with a cable, or hold the phone near the radio speaker and mic (acoustic coupling). Slow but works with any handheld.',
  webrtc: 'Direct device-to-device data channel set up by exchanging two codes (QR or copy-paste). Works across a local network without a hub.',
  multipeer: 'Apple devices nearby find each other over Wi-Fi and Bluetooth automatically.',
  wifi_direct: 'Android phones connect to each other directly over Wi-Fi without a router.',
  rtl_tcp: 'Receive only: a cheap RTL-SDR dongle running rtl_tcp lets you listen to packet radio without transmitting.',
  loopback: 'For testing: a link that talks to itself.',
};

const STATE_LABEL = { up: 'Connected', connecting: 'Connecting…', down: 'Down', error: 'Error' };
const NOT_INTEROP = new Set(['raw', 'stream']);

/** "?" link to the operator manual chapter for a link type / endpoint kind. */
export function manualLink(type, kind, label = 'Help') {
  const id = manualFor(type, kind);
  return h('a', { class: 'btn small ghost', href: '#/help/read/' + id.split('/').map(encodeURIComponent).join('/'), title: 'Open the manual chapter', 'aria-label': label + ': manual chapter' }, icon('book', { size: 16 }), '?');
}

export default async function render(app, params, root) {
  if (params.rest[0] === 'add') return addWizard(app, root, params.query.get('kind'));
  if (params.rest[0] === 'radio') return (await import('./radio.js')).default(app, params, root);
  const page = h('div', { class: 'page' });
  root.appendChild(page);
  const listEl = h('div');
  const legalEl = h('div');
  const hubEl = h('div');
  const rtcEl = h('div');
  const planEl = h('div');
  append(page,
    h('div', { class: 'page-head' }, h('h1', null, 'Links & radios'),
      h('a', { class: 'btn primary', href: '#/links/add' }, icon('plus'), 'Add connection')),
    legalEl, listEl,
    h('h2', { class: 'section-title', 'data-secondary': '' }, 'Share this device'), h('div', { 'data-secondary': '' }, hubEl, rtcEl),
    h('h2', { class: 'section-title', 'data-secondary': '' }, 'Net plan (where to tune)'), h('div', { 'data-secondary': '' }, planEl));

  const tr = await app.api.try('links.types', {}, { types: [] }, true);
  const types = tr.types || [];
  const cards = new Map();
  const meters = new MeterPoller(app, cards);

  async function load() {
    const [lr, sr] = await Promise.all([app.api.try('links.list', {}, null, true), app.api.try('mesh.stats', {}, null, true)]);
    clear(listEl); cards.clear(); meters.reset();
    if (!lr) { listEl.appendChild(empty('Links are not available yet in this build.')); return; }
    const links = lr.links || [];
    app.store.set({ links });
    const perLink = new Map(((sr && sr.per_link) || []).map(p => [p.link_id, p]));
    clear(legalEl);
    if (links.some(l => linkLegal(l).noEncryption)) {
      legalEl.appendChild(legalNotice(true));
      if (links.some(l => linkLegal(l).stationId) && !usableCallsign((app.store.get('info') || {}).callsign)) legalEl.appendChild(callsignPrompt(app, () => load()));
    }
    if (!links.length) { listEl.appendChild(empty('No connections yet. Add Wi-Fi, Bluetooth, a radio or a hub to reach other people.', h('a', { class: 'btn primary', href: '#/links/add' }, icon('plus'), 'Add connection'), 'link')); return; }
    for (const l of links) {
      const c = linkCard(app, l, types, perLink.get(l.id), load);
      cards.set(l.id, c);
      listEl.appendChild(c.el);
      if (c.meter) meters.watch(l.id, c);
    }
  }
  await load();
  const reload = debounce(load, 400);
  app.on('link.state', d => { if (d && d.link && cards.has(d.link.id) && !d.link.removed && cards.get(d.link.id).sameShape(d.link)) cards.get(d.link.id).update(d.link); else reload(); });
  app.on('host.endpoint.state', reload);
  app.on('link.activity', d => { const c = d && cards.get(d.link_id); if (c) c.activity(d); });
  app.on('link.ptt', d => { const c = d && cards.get(d.link_id); if (c) c.ptt(!!d.on); });

  if (app.has('hub')) await hubSection(app, hubEl);
  else {
    // host.info.features lacks "hub": this device cannot serve the app to others.
    hubEl.appendChild(h('div', { class: 'card stack muted-card' },
      h('div', { class: 'row nowrap' }, icon('hub'), h('h2', { class: 'grow' }, 'Hub'), h('span', { class: 'tag' }, 'not on this device'), manualLink(null, 'websocket')),
      h('p', { class: 'muted small' }, 'This device cannot host a hub. A computer running the EmberMesh hub (emberd) serves this app to nearby phones over Wi-Fi; to join one, use Add connection → Hub (WebSocket).')));
  }
  if (app.has('webrtc')) rtcEl.appendChild(h('div', { class: 'card stack' },
    h('h2', null, 'Pair directly (WebRTC)'),
    h('p', { class: 'muted small' }, 'Connect two devices on the same network by exchanging two codes. One device creates an invite, the other answers it.'),
    h('div', { class: 'row' }, h('button', { class: 'btn', onclick: () => rtcOffer(app, load) }, icon('qr'), 'Create invite'), h('button', { class: 'btn', onclick: () => rtcAnswer(app, load) }, icon('scan'), 'Answer an invite'))));
  await netPlan(app, planEl, types);
  app.on('netplan.heard', debounce(() => netPlan(app, planEl, types), 500));
  return () => { reload.cancel(); meters.stop(); };
}

export function legalNotice(collapsed) {
  const text = 'Ham-band rules forbid hiding the meaning of messages. On links marked "No encryption", EmberMesh sends only signed cleartext with your callsign: the Public channel, open channels, SOS, check-ins and bulletins. Encrypted direct messages and private channels are never sent on them — they wait for another link. Anyone can read what goes over these links.';
  if (collapsed) return h('details', { class: 'notice legal' }, h('summary', null, 'Some links carry no encryption (amateur radio) — what this means'), h('p', null, text));
  return h('div', { class: 'notice legal' }, h('strong', null, 'Amateur radio: no encryption, callsign required'), h('p', null, text));
}

/** Asks for the operator's callsign (saved in the profile; the core copies it into amateur links). */
export function callsignPrompt(app, onSaved) {
  const input = h('input', { type: 'text', autocapitalize: 'characters', maxlength: 12, placeholder: 'e.g. QX0ABC', 'aria-label': 'Your callsign', spellcheck: 'false' });
  const err = h('div', { class: 'field-error', role: 'alert' });
  const save = async () => {
    const cs = input.value.trim().toUpperCase();
    if (!usableCallsign(cs)) { err.textContent = 'Enter your licensed callsign (letters and digits, optional -SSID).'; input.focus(); return; }
    try {
      await app.api.call('node.set_profile', { callsign: cs });
      await app.refreshInfo();
      toast('Callsign saved', { kind: 'ok' });
      onSaved && onSaved(cs);
    } catch (e) { err.textContent = errorText(e); }
  };
  input.addEventListener('keydown', e => { if (e.key === 'Enter') { e.preventDefault(); save(); } });
  return h('div', { class: 'notice bad stack callsign-prompt' },
    h('strong', null, 'Your callsign is needed'),
    h('p', { class: 'small' }, 'Amateur-radio links identify every transmission with your callsign and refuse to transmit without one. It is saved in your profile (Settings → Profile).'),
    h('div', { class: 'row nowrap' }, h('div', { class: 'grow' }, input), h('button', { class: 'btn primary', type: 'button', onclick: save }, 'Save callsign')), err);
}

/** Flattens nested status objects to "a.b: value" rows (no truncated JSON). Pure; exported for tests. */
export function flattenStatus(obj, prefix = '', out = [], depth = 0) {
  if (!obj || typeof obj !== 'object') return out;
  for (const [k, v] of Object.entries(obj)) {
    if (out.length >= 80) break;
    if (k === 'caps' || k === 'type' || k === 'state') continue;
    const key = prefix ? prefix + '.' + k : k;
    if (v && typeof v === 'object' && !Array.isArray(v) && depth < 3) flattenStatus(v, key, out, depth + 1);
    else out.push([key, Array.isArray(v) ? v.map(x => (typeof x === 'object' ? JSON.stringify(x) : String(x))).join(', ') : String(v)]);
  }
  return out;
}

// ---------------------------------------------------------------- audio level meter
/** Live level meter for audio / rtl_tcp links (RPC_API "Audio link status fields"). */
export function levelMeter() {
  const fill = h('i');
  const peak = h('b', { class: 'peak', 'aria-hidden': 'true' });
  const bar = h('div', { class: 'meter-bar', role: 'meter', 'aria-label': 'Receive level', 'aria-valuemin': '-60', 'aria-valuemax': '0' }, fill, peak,
    h('span', { class: 'zone ok', 'aria-hidden': 'true' }));
  const dbOut = h('span', { class: 'mono' });
  const clip = h('span', { class: 'lamp', title: 'Clipping' }, 'CLIP');
  const dcd = h('span', { class: 'lamp', title: 'Carrier detect: a signal is being heard' }, 'DCD');
  const tx = h('span', { class: 'lamp', title: 'Transmitting' }, 'TX');
  const snr = h('span', { class: 'small' });
  const frames = h('span', { class: 'small' });
  const hint = h('div', { class: 'small meter-hint', 'aria-live': 'polite' });
  const el = h('div', { class: 'meter stack' },
    h('div', { class: 'row nowrap' }, h('span', { class: 'small muted' }, 'RX level'), h('div', { class: 'grow' }, bar), dbOut),
    h('div', { class: 'row' }, clip, dcd, tx, snr, frames), hint);
  let lastHint = null;
  return {
    el,
    set(m) {
      if (!m) { el.hidden = true; return; }
      el.hidden = false;
      const pct = meterPct(m.level);
      setFill(fill, pct);
      fill.className = m.clipping ? 'bad' : m.hint === 'too_loud' ? 'warn' : m.hint === 'too_quiet' || m.hint === 'no_audio' ? 'dim' : '';
      // peak marker moves with a transform (percent of the bar width via the bar's own width)
      if (m.peak != null) { peak.hidden = false; peak.style.transform = 'translateX(' + Math.max(0, bar.clientWidth * meterPct(m.peak) / 100 - 1).toFixed(1) + 'px)'; } else peak.hidden = true;
      bar.setAttribute('aria-valuenow', String(Math.round(m.level)));
      dbOut.textContent = m.level <= -119 ? '— dBFS' : Math.round(m.level) + ' dBFS';
      clip.classList.toggle('on-bad', m.clipping);
      clip.title = 'Clipping' + (m.clipPct != null ? ': ' + m.clipPct + ' % of samples at full scale' : '');
      dcd.classList.toggle('on-ok', m.dcd);
      tx.classList.toggle('on-bad', m.tx);
      snr.textContent = 'SNR ' + (m.snr == null ? '—' : m.snr + ' dB');
      frames.textContent = m.ok == null ? '' : 'frames ' + m.ok + ' ok / ' + (m.bad ?? 0) + ' bad';
      if (m.hint !== lastHint || m.error) {
        lastHint = m.hint;
        const [cls, txt] = LEVEL_HINTS[m.hint] || ['', m.hint ? 'Level: ' + m.hint : ''];
        clear(hint).appendChild(h('span', { class: cls ? cls + '-text' : '' }, txt));
        if (m.hint) hint.appendChild(h('span', { class: 'muted mono' }, ' (' + m.hint + ')'));
        if (m.error) hint.appendChild(h('div', { class: 'bad-text' }, String(m.error)));
      }
    },
  };
}

/** Polls links.list 3×/s while a meter is on screen and the page is visible. */
export class MeterPoller {
  constructor(app, cards) {
    this.app = app; this.cards = cards; this.visible = new Set(); this.timer = 0; this.busy = false; this.stopped = false;
    this.io = typeof IntersectionObserver === 'function' ? new IntersectionObserver(es => {
      for (const e of es) { const id = Number(e.target.dataset.linkId); if (e.isIntersecting) this.visible.add(id); else this.visible.delete(id); }
      this.kick();
    }) : null;
    this.onVis = () => this.kick();
    document.addEventListener('visibilitychange', this.onVis);
  }
  watch(id, card) {
    card.meter.el.dataset.linkId = String(id);
    if (this.io) this.io.observe(card.meter.el); else { this.visible.add(id); this.kick(); }
  }
  reset() { if (this.io) this.io.disconnect(); this.visible.clear(); }
  kick() {
    if (this.timer || this.stopped) return;
    if (!this.visible.size || document.visibilityState !== 'visible') return;
    const lp = (this.app.store.get('settings') || {}).low_power;
    this.timer = setTimeout(() => this.poll(), lp ? 1000 : 330);
  }
  async poll() {
    this.timer = 0;
    if (this.stopped || this.busy) return;
    this.busy = true;
    try {
      const r = await this.app.api.try('links.list', {}, null, true);
      for (const l of (r && r.links) || []) { const c = this.cards.get(l.id); if (c && c.meter && this.visible.has(l.id)) c.meter.set(audioMeter(l.status)); }
    } finally { this.busy = false; }
    this.kick();
  }
  stop() { this.stopped = true; clearTimeout(this.timer); this.reset(); document.removeEventListener('visibilitychange', this.onVis); }
}

/** Link.blocked (or status.blocked from older cores) → e.g. "no_callsign". Pure; exported for tests. */
export function linkBlocked(l) {
  return (l && (l.blocked || (l.status && l.status.blocked))) || null;
}

/** Human label of the active driver preset (Link.preset, else config.preset). Pure. */
export function presetLabel(t, l) {
  const p = l && (l.preset || (l.config && l.config.preset));
  if (!p || p === 'custom') return '';
  const f = t && Array.isArray(t.config) ? t.config.find(c => c.key === 'preset') : null;
  const o = f && Array.isArray(f.options) ? f.options.find(x => x.value === p) : null;
  return o ? String(o.label) : String(p).replace(/_/g, ' ');
}

/**
 * Effective configuration as [label, value, changedByUser] rows, in the driver's field order
 * (`config` is what runs; `overrides` lists the keys the user set). Secrets are masked. Pure.
 */
export function effectiveSettings(t, l) {
  const cfg = (l && l.config) || {};
  const ov = (l && l.overrides && typeof l.overrides === 'object') ? l.overrides : {};
  const fields = (t && Array.isArray(t.config)) ? t.config : [];
  const rows = [];
  const seen = new Set();
  const show = (f, v) => {
    if (v === undefined || v === null || v === '') return null;
    if (f && f.type === 'password') return '••••';
    if (f && f.type === 'select' && Array.isArray(f.options)) { const o = f.options.find(x => x.value === v); if (o) return String(o.label); }
    if (f && f.type === 'bool') return v ? 'on' : 'off';
    return String(v) + (f && f.unit ? ' ' + f.unit : '');
  };
  for (const f of fields) {
    seen.add(f.key);
    const v = show(f, cfg[f.key]);
    if (v != null) rows.push([String(f.label || f.key), v, Object.prototype.hasOwnProperty.call(ov, f.key)]);
  }
  for (const k of Object.keys(cfg)) if (!seen.has(k)) { const v = show(null, cfg[k]); if (v != null && typeof cfg[k] !== 'object') rows.push([k, v, Object.prototype.hasOwnProperty.call(ov, k)]); }
  return rows;
}

/** Airtime (duty cycle) bar; animates between refreshes. */
function dutyBar(pct) {
  return bar(pct, pct > 80 ? 'bad' : pct > 50 ? 'warn' : '');
}

function linkCard(app, l, types, per, reload) {
  const t = types.find(x => x.type === l.type);
  // TX/RX activity: two LEDs that flash per frame, plus the last frame size; PTT shows ON AIR.
  const ledTx = h('i', { class: 'led tx' }), ledRx = h('i', { class: 'led rx' });
  const actText = h('span'), pttEl = h('span', { class: 'ptt' });
  const act = h('span', { class: 'activity', 'aria-hidden': 'true' }, ledTx, ledRx, actText, pttEl);
  let actTimer = 0;
  const meter = METER_TYPES.has(l.type) ? levelMeter() : null;
  const head = h('div', { class: 'row nowrap' });
  const tagsEl = h('div', { class: 'row' });
  const errEl = h('div');
  const statsEl = h('dl', { class: 'kv small' });
  const settingsEl = h('dl', { class: 'kv small settings-in-use' });
  const blockedEl = h('div');
  const dutyEl = h('div');
  let cur = l;
  const draw = x => {
    cur = x;
    const caps = x.caps || {};
    const st = x.stats || {};
    const legal = linkLegal(x);
    head.className = 'row nowrap link-head';
    append(clear(head),
      h('span', { class: 'dot ' + x.state, 'aria-hidden': 'true' }),
      h('div', { class: 'grow' }, h('div', { class: 'title' }, x.name || (t && t.title) || x.type),
        h('div', { class: 'small muted' }, ((t && t.title) || x.type) + ' · ' + ((x.endpoint && x.endpoint.kind) || 'no endpoint')),
        presetLabel(t, x) ? h('div', { class: 'small' }, 'Radio setup: ' + presetLabel(t, x)) : null),
      h('div', { class: 'meta' }, h('strong', { class: x.state === 'up' ? 'ok-text' : x.state === 'error' ? 'bad-text' : 'warn-text' }, STATE_LABEL[x.state] || x.state), act));
    clear(errEl);
    if (x.error) errEl.appendChild(h('div', { class: 'notice bad small' }, String(x.error)));
    // blocked: the core will not transmit on this link (RPC_API §6 Link.blocked)
    clear(blockedEl);
    if (linkBlocked(x) === 'no_callsign') {
      blockedEl.appendChild(h('div', { class: 'notice bad stack link-blocked', role: 'status' },
        h('strong', null, 'This link cannot transmit until you set a callsign'),
        h('p', { class: 'small' }, 'It is on an amateur band, where every transmission must carry your licensed callsign. It still receives.'),
        h('div', null, h('button', { class: 'btn primary small', type: 'button', onclick: () => {
          const s = sheet('Your callsign', callsignPrompt(app, () => { s.close(); reload(); }));
        } }, icon('edit', { size: 18 }), 'Set callsign'))));
    }
    // effective settings, marking what the user changed (overrides) vs preset/default values
    clear(settingsEl);
    for (const [k, v, changed] of effectiveSettings(t, x)) append(settingsEl, h('dt', null, k), h('dd', null, v, changed ? h('span', { class: 'tag accent', title: 'You changed this setting' }, 'changed') : null));
    append(clear(tagsEl),
      caps.bitrate_bps ? h('span', { class: 'tag' }, formatBitrate(caps.bitrate_bps)) : null,
      caps.mtu ? h('span', { class: 'tag' }, 'MTU ' + caps.mtu + ' B') : h('span', { class: 'tag', title: 'Carries plain text only, no mesh frames' }, 'text only'),
      caps.half_duplex ? h('span', { class: 'tag' }, 'half duplex') : null,
      caps.broadcast ? h('span', { class: 'tag' }, 'broadcast') : null,
      caps.reliable ? h('span', { class: 'tag' }, 'reliable') : null,
      legal.noEncryption ? h('button', { class: 'tag warn', onclick: () => sheet('No encryption on this link', legalNotice()) }, icon('globe', { size: 14 }), 'No encryption (amateur)') : h('span', { class: 'tag ok' }, icon('lock', { size: 14 }), 'encryption ok'),
      legal.stationId ? h('span', { class: 'tag' }, 'station ID') : null,
      caps.duty_cycle_permille && caps.duty_cycle_permille < 1000 ? h('span', { class: 'tag' }, 'duty limit ' + trimNum(caps.duty_cycle_permille / 10, 1) + ' %') : null,
      x.persist === false ? h('span', { class: 'tag', title: 'Not saved: gone after a restart' }, 'temporary') : null);
    const dutyUsed = per && Number.isFinite(per.duty_used_permille) ? per.duty_used_permille : null;
    const dutyPct = dutyUsed != null && caps.duty_cycle_permille ? Math.min(100, dutyUsed / caps.duty_cycle_permille * 100) : null;
    clear(dutyEl);
    if (dutyPct != null && caps.duty_cycle_permille < 1000) append(dutyEl, h('div', { class: 'small muted' }, 'Airtime used: ' + trimNum(dutyUsed / 10, 1) + ' % of ' + trimNum(caps.duty_cycle_permille / 10, 1) + ' % allowed'),
      dutyBar(dutyPct));
    append(clear(statsEl), [
      ['Sent', (st.tx_frames ?? 0) + ' frames, ' + formatBytes(st.tx_bytes ?? 0)],
      ['Received', (st.rx_frames ?? 0) + ' frames, ' + formatBytes(st.rx_bytes ?? 0)],
      ['Errors', String(st.rx_errors ?? 0)], ['Airtime', formatDuration(st.airtime_ms ?? 0)],
      ['Last heard', timeAgo(st.last_rx_ts)], ['Queue', String(st.queue ?? 0)],
      ...flattenStatus(x.status),
    ].flatMap(([k, v]) => [h('dt', null, k), h('dd', null, v)]));
    if (meter) meter.set(audioMeter(x.status));
  };
  draw(l);
  const el = h('div', { class: 'card stack link-card', 'data-link-id': String(l.id) },
    head, blockedEl, errEl, tagsEl, dutyEl, meter ? meter.el : null,
    h('details', null, h('summary', null, 'Statistics'), statsEl),
    h('details', null, h('summary', null, 'Settings in use'), settingsEl),
    h('div', { class: 'row' },
      h('button', { class: 'btn small', onclick: async () => { try { await app.api.call('host.link.reconnect', { id: cur.id }); toast('Reconnecting…'); reload(); } catch (e) { toastError(e, 'Reconnect failed'); } } }, icon('retry', { size: 18 }), 'Reconnect'),
      h('button', { class: 'btn small', onclick: () => configure(app, cur, t, types, reload) }, icon('gear', { size: 18 }), 'Settings'),
      !NOT_INTEROP.has(l.type) ? h('button', { class: 'btn small', onclick: () => interop(app, cur) }, icon('send', { size: 18 }), 'Plain-text message') : null,
      h('button', {
        class: 'btn small danger', onclick: async () => {
          if (!await confirmDialog('Remove ' + (cur.name || cur.type) + '?', 'Closes the connection and forgets its settings.', { ok: 'Remove', danger: true })) return;
          try { await removeLink(app, cur.id); } catch (e) { toastError(e); return; }
          reload();
        },
      }, icon('trash', { size: 18 }), 'Remove'),
      manualLink(l.type, l.endpoint && l.endpoint.kind)));
  return {
    el, meter,
    sameShape: x => x.type === cur.type && x.name === cur.name,
    update: x => draw(x),
    ptt(on) { pttEl.textContent = on ? 'ON AIR' : ''; ledTx.classList.toggle('lit', on); },
    activity(d) {
      if (document.documentElement.dataset.lowpower === '1') return;
      const led = d.dir === 'tx' ? ledTx : ledRx;
      led.classList.add('lit');
      playClass(led, 'flash', 700);
      actText.textContent = (d.dir === 'tx' ? 'TX ' : 'RX ') + formatBytes(d.bytes);
      clearTimeout(actTimer);
      actTimer = setTimeout(() => { actText.textContent = ''; ledTx.classList.remove('lit'); ledRx.classList.remove('lit'); }, 1500);
    },
  };
}

async function removeLink(app, id) {
  try { await app.api.call('host.link.remove', { id }); } catch (e) {
    if (e.code === 'unsupported' || e.code === 'not_implemented') await app.api.call('links.remove', { id });
    else throw e;
  }
}

/**
 * Drivers with a `preset` select (raw, stream) default to one medium; pick the option named after
 * the endpoint kind (loopback, websocket, webrtc, ble, tcp, udp_lan…) when there is one. Pure.
 */
export function presetFor(type, kind) {
  const f = type && (type.config || []).find(x => x && x.key === 'preset' && Array.isArray(x.options));
  if (!f || !kind) return {};
  const o = f.options.find(x => x.value === kind) || f.options.find(x => typeof x.value === 'string' && x.value.startsWith(kind + '_'));
  return o ? { preset: o.value } : {};
}

/** Host endpoint kind descriptor (host.info) for a link's endpoint. */
function hostKind(app, kind) {
  return ((app.store.get('host') || {}).endpoint_kinds || []).find(k => k && k.kind === kind) || null;
}

/** Endpoint config changed? (shallow, typed) Pure; exported for tests. */
export function endpointChanged(before, after) {
  const a = before || {}, b = after || {};
  return Object.keys(b).some(k => JSON.stringify(a[k] ?? null) !== JSON.stringify(b[k] ?? null));
}

function configure(app, l, t, types, reload) {
  const name = h('input', { type: 'text', value: l.name || '', maxlength: 60 });
  const form = t && (t.config || []).length ? renderForm(t.config, l.config || {}) : null;
  const kind = l.endpoint && l.endpoint.kind;
  const hk = kind ? hostKind(app, kind) : null;
  const epCfg = (l.endpoint && l.endpoint.config) || {};
  const linkOptions = linkRefOptions(app.store.get('links'), l.id);
  const epForm = hk && hk.available !== false && (hk.config || []).length ? renderForm(hk.config, epCfg, { linkOptions }) : null;
  const err = h('div', { class: 'field-error', role: 'alert' });
  const saveBtn = h('button', { class: 'btn primary' }, 'Save');
  const s = sheet('Settings: ' + (l.name || (t && t.title) || l.type), h('div', { class: 'stack' },
    h('label', { class: 'field-label' }, 'Name', name),
    form ? h('div', { class: 'stack' }, h('h3', null, (t && t.title) || 'Protocol'), t.description ? h('p', { class: 'muted small' }, t.description) : null, form.el) : null,
    epForm ? h('div', { class: 'stack' }, h('h3', null, 'Connection: ' + (hk.title || kind)),
      h('p', { class: 'muted small' }, 'Changing these closes the connection and opens it again with the new settings.'), epForm.el)
      : kind ? h('p', { class: 'muted small' }, 'Connection (' + kind + ') settings cannot be changed on this device' + (hk && hk.unavailable_reason ? ': ' + hk.unavailable_reason : '') + '. Remove the link and add it again to change them.') : null,
    err,
    h('div', { class: 'row end' }, manualLink(l.type, kind), saveBtn)));
  saveBtn.addEventListener('click', async () => {
    err.textContent = '';
    if ((form && Object.keys(form.validate()).length) || (epForm && Object.keys(epForm.validate()).length)) return;
    const newName = name.value.trim() || l.name;
    const cfg = form ? form.values() : l.config || {};
    const epNew = epForm ? epForm.values() : null;
    saveBtn.disabled = true;
    try {
      if (epNew && endpointChanged(epCfg, epNew)) {
        const endpoint = { kind, config: { ...epCfg, ...epNew } };
        let kept = false;
        // Preferred: links.configure with the new endpoint keeps the link id, then the host reopens it.
        try {
          await app.api.call('links.configure', { id: l.id, name: newName, ...(form ? { config: cfg } : {}), endpoint });
          kept = true;
        } catch (e) { if (!['bad_params', 'not_implemented', 'unsupported'].includes(e.code)) throw e; }
        if (kept) {
          try { await app.api.call('host.link.reconnect', { id: l.id }); } catch (e) { toastError(e, 'Saved, but reopening the connection failed (use Reconnect)'); }
        } else {
          // Older core: re-create the link (new id) after confirmation.
          if (!await confirmDialog('Re-create this connection?', 'The connection settings changed. EmberMesh will close "' + (l.name || l.type) + '", remove it and create it again with the new settings. Messages waiting in its queue go out on the new link; its statistics restart.', { ok: 'Re-create' })) return;
          await removeLink(app, l.id);
          try {
            await app.api.call('host.link.create', { name: newName, endpoint, link: { type: l.type, config: cfg } });
          } catch (e) {
            err.textContent = 'The old link was removed but the new one could not be created: ' + errorText(e) + ' Add it again with Add connection.';
            reload();
            return;
          }
        }
      } else {
        await app.api.call('links.configure', { id: l.id, name: newName, ...(form ? { config: cfg } : {}) });
      }
      s.close(); toast('Saved', { kind: 'ok' }); reload();
    } catch (e) { err.textContent = errorText(e); } finally { saveBtn.disabled = false; }
  });
}

function interop(app, l) {
  const to = h('input', { type: 'text', placeholder: 'Callsign / node (empty = everyone)', autocapitalize: 'characters' });
  const text = h('textarea', { rows: 3, maxlength: 200, placeholder: 'Plain text for non-EmberMesh stations (APRS, Meshtastic, RTTY…)' });
  const s = sheet('Plain-text message via ' + (l.name || l.type), h('div', { class: 'stack' },
    h('p', { class: 'muted small' }, 'Sent as ordinary text that other radio systems can read. Never encrypted. Keep it short. Replies appear in Chats under the radio network’s name.'),
    h('label', { class: 'field-label' }, 'To', to), h('label', { class: 'field-label' }, 'Text', text),
    h('div', { class: 'row end' }, h('button', {
      class: 'btn primary', onclick: async () => {
        if (!text.value.trim()) return text.focus();
        try {
          const r = await app.api.call('interop.send', { link_id: l.id, text: text.value.trim(), ...(to.value.trim() ? { to: to.value.trim() } : {}) });
          s.close();
          const conv = r && r.message && r.message.conv;
          toast('Sent', { kind: 'ok', ...(conv ? { action: { label: 'Open', run: () => app.nav('#/chats/' + encodeURIComponent(conv)) } } : {}) });
        } catch (e) { toastError(e, 'Not sent'); }
      },
    }, 'Send'))));
}

// ---------------------------------------------------------------- hub
async function hubSection(app, box) {
  let st = null;
  const render = x => {
    st = x;
    clear(box);
    const running = st && st.running;
    box.appendChild(h('div', { class: 'card stack' },
      h('div', { class: 'row nowrap' }, icon('hub'), h('h2', { class: 'grow' }, 'Hub'), running ? h('span', { class: 'tag ok' }, 'running · ' + (st.clients || 0) + ' connected') : h('span', { class: 'tag' }, 'off')),
      h('p', { class: 'muted small' }, 'Serves this app to phones and laptops on the same Wi-Fi or hotspot. They open the address in any browser — no install — and join the mesh through this device.'),
      running ? h('div', { class: 'stack' }, (st.urls || []).map(u => h('div', { class: 'row nowrap' },
        h('span', { class: 'mono grow copybox' }, u),
        h('button', { class: 'btn small icon-only', 'aria-label': 'Show QR for ' + u, onclick: () => showQr(app, 'Hub address', u, { intro: 'Scan with a phone camera on the same Wi-Fi.' }) }, icon('qr', { size: 18 })),
        h('button', { class: 'btn small icon-only', 'aria-label': 'Copy ' + u, onclick: () => copyText(u) }, icon('copy', { size: 18 }))))) : null,
      h('div', { class: 'row' }, running
        ? h('button', { class: 'btn danger', onclick: async () => { try { render(await app.api.call('host.hub.stop')); } catch (e) { toastError(e); } } }, 'Stop hub')
        : h('button', { class: 'btn primary', onclick: async () => { try { render(await app.api.call('host.hub.start', { port: 8437 })); } catch (e) { toastError(e, 'Hub failed to start'); } } }, 'Start hub'))));
  };
  render(await app.api.try('host.hub.status', {}, null, true));
  app.on('host.hub.clients', d => { if (st) { st = { ...st, clients: d.clients }; render(st); } });
}

// ---------------------------------------------------------------- WebRTC pairing
async function scanInto(app, ta) {
  try { const r = await app.api.call('host.qr.scan'); ta.value = r.text || ''; } catch (e) { toastError(e, 'Scan failed'); }
}

async function rtcOffer(app, reload) {
  const body = h('div', { class: 'stack' }, h('p', { class: 'muted' }, 'Creating invite…'));
  const s = sheet('Pair: create invite', body);
  let r;
  try { r = await app.api.call('host.webrtc.create_offer', { name: (app.store.get('info') || {}).name || '' }); } catch (e) { clear(body).appendChild(h('p', { class: 'bad-text' }, errorText(e))); return; }
  const answer = h('textarea', { rows: 3, placeholder: 'Paste the answer code from the other device' });
  const qrBox = h('div');
  append(clear(body), 
    h('p', null, h('strong', null, 'Step 1. '), 'On the other device choose “Answer an invite” and scan or paste this code:'),
    qrBox,
    h('div', { class: 'copybox' }, r.offer),
    h('button', { class: 'btn', onclick: () => copyText(r.offer) }, icon('copy'), 'Copy invite'),
    h('p', null, h('strong', null, 'Step 2. '), 'Enter the answer code it shows:'),
    answer,
    h('div', { class: 'row' },
      app.has('qr_scan') ? h('button', { class: 'btn', onclick: () => scanInto(app, answer) }, icon('scan'), 'Scan answer') : null,
      h('button', {
        class: 'btn primary', onclick: async () => {
          if (!answer.value.trim()) return answer.focus();
          try { await app.api.call('host.webrtc.accept_answer', { pairing_id: r.pairing_id, answer: answer.value.trim() }); s.close(); toast('Paired — connecting', { kind: 'ok' }); reload(); } catch (e) { toastError(e, 'Pairing failed'); }
        },
      }, 'Connect')));
  try { const q = await app.api.call('tools.qr', { text: r.offer, ecc: 'L' }); qrBox.appendChild(qrSvg(q.rows, 'Invite QR')); } catch { qrBox.appendChild(h('p', { class: 'muted small' }, 'Invite too long for a QR code here — copy and paste it instead.')); }
}

async function rtcAnswer(app, reload) {
  const offer = h('textarea', { rows: 3, placeholder: 'Paste the invite code from the other device' });
  const out = h('div', { class: 'stack' });
  sheet('Pair: answer an invite', h('div', { class: 'stack' },
    h('p', null, h('strong', null, 'Step 1. '), 'Scan or paste the invite from the other device:'),
    offer,
    h('div', { class: 'row' },
      app.has('qr_scan') ? h('button', { class: 'btn', onclick: () => scanInto(app, offer) }, icon('scan'), 'Scan invite') : null,
      h('button', {
        class: 'btn primary', onclick: async () => {
          if (!offer.value.trim()) return offer.focus();
          try {
            const r = await app.api.call('host.webrtc.accept_offer', { offer: offer.value.trim() });
            append(clear(out), 
              h('p', null, h('strong', null, 'Step 2. '), 'Show this answer to the other device (scan or paste). The link comes up when it connects.'),
              h('div', { class: 'copybox' }, r.answer),
              h('div', { class: 'row' }, h('button', { class: 'btn', onclick: () => copyText(r.answer) }, icon('copy'), 'Copy answer'),
                h('button', { class: 'btn', onclick: () => showQr(app, 'Answer code', r.answer, { ecc: 'L' }) }, icon('qr'), 'Show as QR')));
            reload();
          } catch (e) { toastError(e, 'Invalid invite'); }
        },
      }, 'Create answer')),
    out));
}

// ---------------------------------------------------------------- net plan
async function netPlan(app, box, types) {
  const modes = netplanModes(types);
  const modeLabel = v => (modes.find(m => m.value === v) || {}).label || String(v || '');
  const r = await app.api.try('netplan.get', {}, null, true);
  clear(box);
  if (!r) { box.appendChild(h('p', { class: 'muted small' }, 'Net plans are not available yet.')); return; }
  const table = entries => entries && entries.length ? h('div', { class: 'table-wrap' }, h('table', { class: 'data' },
    h('thead', null, h('tr', null, ['Label', 'Frequency', 'Mode', 'Tone', 'Schedule', 'Notes'].map(t => h('th', null, t)))),
    h('tbody', null, entries.map(e => h('tr', null, h('td', null, String(e.label || '')), h('td', { class: 'num' }, formatFreq(e.freq_hz)), h('td', null, modeLabel(e.mode)),
      h('td', null, String(e.tone ?? '')), h('td', null, String(e.schedule || '')), h('td', null, String(e.notes || ''))))))) : h('p', { class: 'muted small' }, 'No entries.');
  append(box, 
    h('div', { class: 'card stack' },
      h('div', { class: 'row nowrap' }, h('h2', { class: 'grow' }, 'My announced plan'), h('button', { class: 'btn small', onclick: () => editPlan(app, r.plan, modes, () => netPlan(app, box, types)) }, icon('edit', { size: 18 }), 'Edit')),
      h('p', { class: 'muted small' }, 'Announced periodically so people with handheld radios know where to tune and listen.'),
      table(r.plan && r.plan.entries)),
    ...(r.heard || []).map(p => h('div', { class: 'card stack' },
      h('h3', null, 'Heard from ' + (p.from_name || String(p.from || '?').slice(0, 8)), h('span', { class: 'muted small' }, ' · ' + timeAgo(p.ts))), table(p.entries))));
}

function editPlan(app, plan, modes, done) {
  const rows = h('div', { class: 'stack' });
  const addRow = (e = {}) => {
    const label = h('input', { type: 'text', value: e.label || '', placeholder: 'Label, e.g. Calling' });
    const freq = h('input', { type: 'text', inputmode: 'decimal', value: e.freq_hz ? String(e.freq_hz / 1e6) : '', placeholder: 'MHz, e.g. 146.520' });
    const cur = e.mode || 'voice';
    const opts = modes.some(m => m.value === cur) ? modes : [...modes, { value: cur, label: cur }];
    const mode = h('select', { 'aria-label': 'Mode' }, opts.map(m => h('option', { value: m.value, selected: m.value === cur }, m.label)));
    const tone = h('input', { type: 'text', value: e.tone != null ? String(e.tone) : '', placeholder: 'CTCSS, e.g. 100.0' });
    const sched = h('input', { type: 'text', value: e.schedule || '', placeholder: 'e.g. top of every hour' });
    const notes = h('input', { type: 'text', value: e.notes || '', placeholder: 'Notes' });
    const row = h('div', { class: 'card stack' }, h('div', { class: 'row nowrap' }, h('div', { class: 'grow' }, label), h('button', { class: 'btn small ghost icon-only', 'aria-label': 'Remove entry', onclick: () => row.remove() }, icon('trash', { size: 18 }))),
      h('div', { class: 'row nowrap' }, h('div', { class: 'grow' }, freq), mode), h('div', { class: 'row nowrap' }, h('div', { class: 'grow' }, tone), h('div', { class: 'grow' }, sched)), notes);
    row._get = () => {
      const mhz = Number(String(freq.value).replace(',', '.'));
      if (!Number.isFinite(mhz) || mhz <= 0) return null;
      const o = { label: label.value.trim(), freq_hz: Math.round(mhz * 1e6), mode: mode.value };
      if (tone.value.trim()) o.tone = Number(tone.value) || tone.value.trim();
      if (sched.value.trim()) o.schedule = sched.value.trim();
      if (notes.value.trim()) o.notes = notes.value.trim();
      return o;
    };
    rows.appendChild(row);
  };
  for (const e of (plan && plan.entries) || []) addRow(e);
  if (!rows.childElementCount) addRow();
  const s = sheet('Edit net plan', h('div', { class: 'stack' }, rows,
    h('button', { class: 'btn', onclick: () => addRow() }, icon('plus'), 'Add frequency'),
    h('div', { class: 'row end' }, h('button', {
      class: 'btn primary', onclick: async () => {
        const entries = [...rows.children].map(r => r._get()).filter(Boolean);
        try { await app.api.call('netplan.set', { entries }); s.close(); toast('Net plan saved and will be announced', { kind: 'ok' }); done(); } catch (e) { toastError(e, 'Not saved'); }
      },
    }, 'Save & announce'))));
}

// ---------------------------------------------------------------- add wizard
async function addWizard(app, root, preKind) {
  const page = h('div', { class: 'page' });
  root.appendChild(page);
  const hostInfo = app.store.get('host') || {};
  const kinds = (hostInfo.endpoint_kinds || []).filter(k => k && k.kind);
  const tr = await app.api.try('links.types', {}, { types: [] }, true);
  const types = tr.types || [];
  const head = title => h('div', { class: 'page-head' }, h('a', { class: 'btn ghost icon-only', href: '#/links', 'aria-label': 'Back to links' }, icon('back')), h('h1', null, title));
  let hwList = null;
  try { hwList = await loadHardware(app, { platform: platformOf(app) }); } catch { hwList = null; }

  function step1() {
    append(clear(page), head('Add connection'));
    if (!kinds.length) { page.appendChild(empty('This device does not offer any connection types.')); return; }
    append(page,
      h('a', { class: 'card manual-entry row nowrap', href: '#/links/radio' }, icon('signal', { size: 28 }),
        h('div', { class: 'grow' }, h('div', { class: 'title' }, 'Connect a radio (guided)'), h('div', { class: 'sub muted small' }, 'Baofeng UV-5R or any other handheld, mobile or base radio: step by step.')), icon('next')),
      h('p', { class: 'muted' }, 'How do you want to reach other devices?'));
    const sorted = kinds.slice().sort((a, b) => (b.available !== false) - (a.available !== false));
    page.appendChild(h('ul', { class: 'list card' }, sorted.map(k => {
      const avail = k.available !== false;
      return h('li', null, h('button', { class: 'item', disabled: !avail, 'aria-disabled': String(!avail), onclick: () => step2(k) },
        h('div', { class: 'grow' },
          h('div', { class: 'title' }, String(k.title || k.kind)),
          h('div', { class: 'sub pre-wrap' }, avail ? String(k.description || KIND_HELP[k.kind] || '') : String(k.unavailable_reason || 'Not available on this device'))),
        avail ? icon('next') : h('span', { class: 'tag' }, 'unavailable')));
    })));
  }

  function step2(k) {
    if (k.kind === 'webrtc') { rtcOffer(app, () => app.nav('#/links')); return; }
    append(clear(page), head(String(k.title || k.kind)));
    const help = [k.description, KIND_HELP[k.kind]].filter(Boolean);
    const helpLink = h('span');
    page.appendChild(h('div', { class: 'notice info' }, help.map(t => h('p', null, String(t))), helpLink));
    const needsBox = h('div');
    page.appendChild(needsBox);

    const epForm = renderForm(k.config || [], {}, { linkOptions: linkRefOptions(app.store.get('links'), 0) });
    const epBox = h('div', null, epForm.el);
    // Compatible protocols come from the core (links.types endpoint_kinds) ∩ the host's list.
    const compatible = compatibleTypes(k, types, k);
    const def = compatible.includes(k.default_link_type) ? k.default_link_type : compatible[0];
    const typeSel = h('select', { id: 'link-type' }, compatible.map(tp => {
      const t = types.find(x => x.type === tp);
      return h('option', { value: tp, selected: tp === def }, t ? t.title : tp);
    }));
    const typeDesc = h('p', { class: 'muted small' });
    const linkBox = h('div');
    const legalBox = h('div');
    let linkForm = null;
    let curType = null;
    const rebuild = () => {
      const t = types.find(x => x.type === typeSel.value);
      curType = t;
      typeDesc.textContent = t ? String(t.description || '') : '';
      const info = app.store.get('info') || {};
      const init = presetFor(t, k.kind);
      if (t && (t.config || []).some(f => f.key === 'callsign') && usableCallsign(info.callsign)) init.callsign = info.callsign;
      linkForm = renderForm(t ? t.config || [] : [], init, { onChange: v => showLegal(v) });
      clear(linkBox).appendChild(linkForm.el);
      clear(helpLink).appendChild(h('p', null, h('a', { href: '#/help/read/' + manualFor(typeSel.value, k.kind).split('/').map(encodeURIComponent).join('/') }, icon('book', { size: 16 }), ' Step-by-step guide in the manual')));
      showLegal(linkForm.values());
      showNeeds();
    };
    // "You will need" (rf.hardware) for this endpoint kind / protocol on this platform
    const KIND_HW = { websocket: 'hub', ws_server: 'hub', ble_peer: 'phone_to_phone', webrtc: 'phone_to_phone', multipeer: 'phone_to_phone', wifi_direct: 'phone_to_phone', rtl_tcp: 'sdr' };
    const showNeeds = () => {
      clear(needsBox);
      if (!hwList) return;
      const t = typeSel.value;
      const m = methodFor(hwList, { id: KIND_HW[k.kind] }) || (t === 'kiss' ? methodFor(hwList, { id: /ble|bt_spp/.test(k.kind) ? 'bt_tnc' : 'serial_tnc' }) : null) || methodFor(hwList, { link_type: t });
      if (m) needsBox.appendChild(h('details', { class: 'hw-details', open: true }, h('summary', null, 'You will need'), needsCard(m, platformOf(app), { title: m.title })));
    };
    // Legal (amateur) mode is predicted from the driver's own config fields (licence / amateur /
    // licensed); the created link's caps.encryption_allowed / requires_station_id are authoritative.
    let legalShown = null;
    const showLegal = v => {
      const ham = curType ? amateurFromConfig(curType.config, v) === true : false;
      const info = app.store.get('info') || {};
      const needCall = ham && !usableCallsign(info.callsign) && !usableCallsign(v && v.callsign);
      const key = ham + '/' + needCall;
      if (key === legalShown) return;
      legalShown = key;
      clear(legalBox);
      if (!ham) return;
      legalBox.appendChild(legalNotice());
      if (needCall) legalBox.appendChild(callsignPrompt(app, cs => { if (linkForm && (curType.config || []).some(f => f.key === 'callsign')) linkForm.set({ callsign: cs }); legalShown = null; showLegal(linkForm.values()); }));
    };
    typeSel.addEventListener('change', () => { legalShown = null; rebuild(); });
    const name = h('input', { type: 'text', placeholder: 'e.g. Baofeng on 2 m', maxlength: 60 });
    const autostart = h('input', { type: 'checkbox', checked: true });
    const devBox = h('div');

    if (k.scannable) {
      const scanBtn = h('button', { class: 'btn primary', onclick: () => scan() }, icon('search'), 'Scan for devices');
      append(devBox, scanBtn, h('div', { class: 'devices' }));
      async function scan() {
        const list = devBox.querySelector('.devices');
        scanBtn.disabled = true;
        clear(list).appendChild(h('p', { class: 'muted' }, 'Scanning…'));
        try {
          const r = await app.api.call('host.endpoints.scan', { kind: k.kind, timeout_ms: 8000 });
          const ds = r.devices || [];
          clear(list);
          if (!ds.length) list.appendChild(h('p', { class: 'muted' }, 'Nothing found. Make sure the device is on, in range and not connected to another app.'));
          list.appendChild(h('ul', { class: 'list' }, ds.map(d => h('li', null, h('button', {
            class: 'item', onclick: () => {
              epForm.set(d.config || {});
              if (!name.value) name.value = String(d.name || '');
              for (const b of list.querySelectorAll('.item')) b.removeAttribute('aria-current');
              list.querySelectorAll('.item')[ds.indexOf(d)].setAttribute('aria-current', 'true');
              toast('Selected ' + (d.name || d.id));
            },
          }, h('div', { class: 'grow' }, h('div', { class: 'title' }, String(d.name || d.id)), h('div', { class: 'sub' }, [d.detail, d.rssi != null ? d.rssi + ' dBm' : ''].filter(Boolean).join(' · '))), icon('next'))))));
        } catch (e) { clear(list).appendChild(h('p', { class: 'bad-text' }, 'Scan failed: ' + errorText(e))); } finally { scanBtn.disabled = false; }
      }
    }

    const err = h('div', { class: 'field-error', role: 'alert' });
    const create = h('button', { class: 'btn primary block', onclick: () => submit() }, 'Connect');
    async function submit() {
      err.textContent = '';
      if (Object.keys(epForm.validate()).length || (linkForm && Object.keys(linkForm.validate()).length)) { err.textContent = 'Please fix the highlighted fields.'; return; }
      create.disabled = true;
      // Opening a device can wait on a permission prompt (microphone, USB, Bluetooth): say so
      // instead of looking frozen, and let the user carry on (the link already exists).
      const slow = setTimeout(() => {
        clear(err).appendChild(h('div', { class: 'notice info small' }, 'Still opening the device… If the system asked for permission (microphone, USB, Bluetooth), answer it. ',
          h('a', { href: '#/links' }, 'Continue in the background')));
      }, 6000);
      try {
        const r = await app.api.call('host.link.create', {
          ...(name.value.trim() ? { name: name.value.trim() } : {}),
          endpoint: { kind: k.kind, config: epForm.values() },
          link: { type: typeSel.value, config: linkForm ? linkForm.values() : {} },
          autostart: autostart.checked,
        });
        toast('Added ' + ((r.link && r.link.name) || 'link'), { kind: 'ok' });
        if (!page.isConnected) return; // the user moved on while the device was opening
        if (r.link && METER_TYPES.has(r.link.type)) testStep(r.link);
        else app.nav('#/links');
      } catch (e) { clear(err).textContent = errorText(e); } finally { clearTimeout(slow); create.disabled = false; }
    }

    append(page, 
      devBox,
      h('div', { class: 'card stack' }, h('h2', null, 'Connection'), epBox),
      h('div', { class: 'card stack' }, h('h2', null, 'Protocol'),
        h('label', { class: 'field-label', for: 'link-type' }, 'What is on the other end?'), typeSel, typeDesc, linkBox, legalBox),
      h('div', { class: 'card stack' },
        h('label', { class: 'field-label' }, 'Name', name),
        h('label', { class: 'check' }, autostart, h('span', null, 'Reconnect automatically when the app starts'))),
      err, create,
      h('button', { class: 'btn ghost block', onclick: step1 }, 'Choose a different connection type'));
    rebuild();
  }

  // Test step for sound-card / SDR links: live level meter while the user sets the radio volume.
  function testStep(link) {
    const meter = levelMeter();
    append(clear(page), head('Set the audio level'),
      h('div', { class: 'card stack' },
        h('p', null, 'Play a signal into ', h('strong', null, link.name || link.type), ' (let the radio receive a busy channel, or have someone transmit). Turn the radio volume until the bar sits in the middle and the hint says “Level OK”. The CLIP lamp must stay off.'),
        meter.el,
        h('p', { class: 'muted small' }, 'DCD lights while the modem hears a signal; frames count packets it decoded. You can open this meter again on the link card.')),
      h('div', { class: 'row end' }, manualLink(link.type, link.endpoint && link.endpoint.kind), h('a', { class: 'btn primary', href: '#/links' }, 'Done')));
    const cards = new Map([[link.id, { meter }]]);
    const poller = new MeterPoller(app, cards);
    poller.watch(link.id, { meter });
    meter.set(audioMeter(link.status) || { level: -120, hint: 'no_audio' });
    const off = () => { poller.stop(); window.removeEventListener('hashchange', off); };
    window.addEventListener('hashchange', off);
  }

  const pre = preKind && kinds.find(k => k.kind === preKind);
  if (pre && pre.available !== false) step2(pre); else step1();
}
