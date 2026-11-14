// SOS: guarded hold-to-activate beacon, active status, received alerts + ack, safety check-in, roll call.
import { h, clear, debounce, append } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { toast, toastError, holdButton, confirmDialog, empty } from '../lib/ui.js';
import { timeAgo, distance, bearing, compass, formatDistance, formatPos, validPos, dateTime } from '../lib/format.js';
import { ownPosition } from './people.js';
import { hopsLabel } from '../lib/core_spec.js';
import { unverified } from '../lib/ratelimit.js';
import { EMERGENCY_QUESTIONS } from '../lib/library.js';
import { askBox } from './ask.js';

export const CHECKIN = [
  { status: 'ok', label: "I'm OK", icon: 'check' },
  { status: 'need_help', label: 'Need help', icon: 'alert' },
  { status: 'injured', label: 'Injured', icon: 'sos' },
  { status: 'moving', label: 'Moving', icon: 'next' },
  { status: 'sheltering', label: 'Sheltering', icon: 'shield' },
];
const LABEL = Object.fromEntries(CHECKIN.map(c => [c.status, c.label]));
const HOLD_MS = 3000;

export default async function render(app, _p, root) {
  const me = await ownPosition(app);
  // one-tap first-aid questions answered from the guides (guides.ask); hidden without smart search
  const askSlot = h('div');
  askBox(app, { title: 'First aid: ask the guides', shortcuts: EMERGENCY_QUESTIONS, placeholder: 'e.g. bleeding won’t stop' }).then(b => { if (b) askSlot.appendChild(b); });
  const sosBox = h('div', { class: 'card stack' });
  const alertsBox = h('div');
  const rollBox = h('div');
  root.appendChild(h('div', { class: 'page' },
    h('div', { class: 'page-head' }, h('h1', null, 'Emergency')),
    sosBox,
    askSlot,
    h('h2', { class: 'section-title' }, 'Safety check-in'),
    checkinCard(app, me),
    h('h2', { class: 'section-title' }, 'SOS alerts received'),
    alertsBox,
    h('h2', { class: 'section-title', 'data-secondary': '' }, 'Roll call (latest check-in per person)'),
    h('div', { 'data-secondary': '' }, rollBox)));

  const text = h('input', { type: 'text', maxlength: 140, placeholder: 'What is happening? (optional, e.g. "broken leg, 2 people")' });
  const withPos = h('input', { type: 'checkbox', checked: true });

  let wasActive = null;
  function renderSos() {
    const st = app.store.get('sos') || { active: false };
    clear(sosBox);
    // the card's content rises in when the beacon switches on or off (not on every counter update)
    sosBox.classList.toggle('enter-list', wasActive !== null && wasActive !== !!st.active);
    wasActive = !!st.active;
    if (st.active) {
      append(sosBox, 
        h('div', { class: 'notice bad' }, h('strong', null, 'Your SOS beacon is active'), h('br'),
          'Since ' + dateTime(st.since_ts) + '. Sent ' + (st.sent || 0) + ' time(s). Every node that hears it relays it with top priority.'),
        st.text ? h('p', null, '“' + st.text + '”') : null,
        h('div', null, h('strong', null, 'Heard by: '), (st.acked_by || []).length
          ? h('ul', null, st.acked_by.map(a => h('li', null, (a.name || a.node_id) + ' — ' + timeAgo(a.ts))))
          : h('span', { class: 'muted' }, 'nobody has acknowledged yet. Keep the device on and, if possible, high up or outdoors.')),
        h('button', {
          class: 'btn block', onclick: async () => {
            if (!await confirmDialog('Stop SOS?', 'Stop the beacon only when you are safe or help has arrived.', { ok: 'Stop SOS' })) return;
            try { const r = await app.api.call('sos.stop'); app.store.set({ sos: { ...st, ...r, active: false } }); toast('SOS stopped'); } catch (e) { toastError(e); }
          },
        }, 'I am safe — stop SOS'));
    } else {
      const btn = holdButton(h('span', null, 'SOS', h('br'), h('small', null, 'hold 3 seconds')), HOLD_MS, start, { class: 'sos-hold', label: 'Send SOS: press and hold for 3 seconds' });
      append(sosBox, 
        h('p', { class: 'muted' }, 'Sends a repeating distress beacon with your position to every device in range. Every node relays it first. Press and hold to activate.'),
        btn,
        h('label', { class: 'field-label' }, 'Message', text),
        h('label', { class: 'check' }, withPos, h('span', null, 'Include my position' + (validPos(me) ? ' (' + formatPos(me) + ')' : ' (unknown — will try GPS)'))));
    }
  }

  async function start() {
    let position = null;
    if (withPos.checked) {
      position = validPos(me) ? me : null;
      if (!position && app.has('location')) position = await app.api.try('host.location.get', {}, null, true);
    }
    try {
      const params = { ...(text.value.trim() ? { text: text.value.trim() } : {}), ...(validPos(position) ? { position } : {}) };
      const r = await app.api.call('sos.start', params);
      app.store.set({ sos: { active: true, since_ts: r.since_ts, text: params.text, sent: 0, acked_by: [] } });
      app.api.try('host.keep_awake', { enabled: true }, null, true);
      toast('SOS beacon active', { kind: 'sos', icon: 'sos' });
      const st = await app.api.try('sos.status', {}, null, true);
      if (st) app.store.set({ sos: st });
    } catch (e) { toastError(e, 'SOS failed'); }
  }

  async function loadAlerts() {
    const r = await app.api.try('sos.list', {}, null, true);
    clear(alertsBox);
    if (!r) { alertsBox.appendChild(empty('Not available.')); return; }
    const alerts = (r.alerts || []).slice().sort((a, b) => (a.acked - b.acked) || (b.last_ts - a.last_ts));
    app.store.set({ sosAlerts: alerts });
    if (!alerts.length) { alertsBox.appendChild(h('p', { class: 'muted' }, 'No SOS alerts received.')); return; }
    // Alerts from verified senders first; unverified ones are grouped (collapsed when there are
    // several) so a flood of forged alerts cannot bury real ones.
    const ver = alerts.filter(a => !unverified(a)), unv = alerts.filter(a => unverified(a));
    let target = alertsBox;
    for (const a of [...ver, ...unv]) {
      if (a === unv[0]) {
        const grp = h('div', { class: 'stack' });
        if (unv.length > 2) {
          alertsBox.appendChild(h('details', { class: 'card unverified-group', open: !ver.length && unv.length <= 5 }, h('summary', null, unv.length + ' alerts from unverified senders'),
            h('p', { class: 'small muted' }, 'Their identity could not be checked: they may be real people whose keys you do not have yet, or forged. Treat with care.'), grp));
        } else alertsBox.appendChild(grp);
        target = grp;
      }
      const geo = validPos(me) && validPos(a.position) ? formatDistance(distance(me, a.position), app.units()) + ' ' + compass(bearing(me, a.position)) + ' (' + Math.round(bearing(me, a.position)) + '°)' : '';
      const unvA = unverified(a);
      target.appendChild(h('div', { class: 'card stack' + (a.acked || unvA ? '' : ' sos-card') },
        h('div', { class: 'row nowrap' }, icon('sos'), h('strong', { class: 'grow' }, a.name || 'Node ' + String(a.node_id).slice(0, 8)),
          unvA ? h('span', { class: 'tag warn', title: 'The sender\'s identity could not be verified' }, 'unverified sender') : null,
          a.acked ? h('span', { class: 'tag ok' }, 'acknowledged') : h('span', { class: 'tag bad' }, 'needs response')),
        a.text ? h('p', null, a.text) : null,
        h('div', { class: 'small muted' }, 'First ' + timeAgo(a.first_ts) + ' · last ' + timeAgo(a.last_ts) + ' · ' + hopsLabel(a.hops)),
        validPos(a.position) ? h('div', { class: 'mono small' }, formatPos(a.position) + (geo ? ' · ' + geo : '')) : h('div', { class: 'small muted' }, 'No position given'),
        h('div', { class: 'row' },
          !a.acked ? h('button', { class: 'btn primary', onclick: async () => { try { await app.api.call('sos.ack', { node_id: a.node_id }); toast('They will be told someone heard them', { kind: 'ok' }); loadAlerts(); } catch (e) { toastError(e); } } }, icon('check'), 'Acknowledge — I hear you') : null,
          h('a', { class: 'btn', href: '#/chats/' + encodeURIComponent('dm:' + a.node_id) }, icon('chat'), 'Message'))));
    }
  }

  async function loadRoll() {
    const r = await app.api.try('checkin.list', {}, null, true);
    clear(rollBox);
    if (!r) { rollBox.appendChild(empty('Not available.')); return; }
    const cs = (r.checkins || []).slice().sort((a, b) => b.ts - a.ts);
    const counts = {};
    for (const c of cs) counts[c.status] = (counts[c.status] || 0) + 1;
    append(rollBox, 
      h('div', { class: 'row' }, CHECKIN.map(c => h('span', { class: 'tag st-' + c.status }, c.label + ': ' + (counts[c.status] || 0)))),
      cs.length ? h('ul', { class: 'list' }, cs.map(c => h('li', null, h('div', { class: 'item' },
        h('div', { class: 'grow' }, h('div', { class: 'title' }, c.name || String(c.node_id).slice(0, 8), unverified(c) ? h('span', { class: 'tag warn' }, ' unverified sender') : null), c.note ? h('div', { class: 'sub' }, c.note) : null,
          validPos(c.position) && validPos(me) ? h('div', { class: 'sub' }, formatDistance(distance(me, c.position), app.units()) + ' ' + compass(bearing(me, c.position))) : null),
        h('div', { class: 'meta' }, h('strong', { class: 'st-' + c.status }, LABEL[c.status] || c.status), timeAgo(c.ts))))))
        : h('p', { class: 'muted' }, 'No check-ins yet.'));
  }

  renderSos();
  const offSos = app.store.subscribe('sos', renderSos);
  const st = await app.api.try('sos.status', {}, null, true);
  if (st) app.store.set({ sos: st });
  await Promise.all([loadAlerts(), loadRoll()]);
  const ra = debounce(loadAlerts, 500), rr = debounce(loadRoll, 500);
  app.on('sos.received', ra);
  app.on('checkin.received', rr);
  return () => { ra.cancel(); rr.cancel(); offSos(); };
}

function checkinCard(app, me) {
  const note = h('input', { type: 'text', maxlength: 100, placeholder: 'Where, who with, what you need' });
  const withPos = h('input', { type: 'checkbox', checked: validPos(me) });
  const send = async status => {
    try {
      await app.api.call('checkin.send', { status, ...(note.value.trim() ? { note: note.value.trim() } : {}), ...(withPos.checked && validPos(me) ? { position: me } : {}) });
      toast('Check-in sent: ' + LABEL[status], { kind: 'ok' });
      note.value = '';
    } catch (e) { toastError(e, 'Check-in failed'); }
  };
  return h('div', { class: 'card stack' },
    h('p', { class: 'muted small' }, 'Tell everyone nearby how you are. Shows in their roll call.'),
    h('div', { class: 'checkin-grid' }, CHECKIN.map(c => h('button', { class: 'btn st-' + c.status, onclick: () => send(c.status) }, icon(c.icon), c.label))),
    h('label', { class: 'field-label' }, 'Note (optional)', note),
    h('label', { class: 'check' }, withPos, h('span', null, 'Include my position')));
}
