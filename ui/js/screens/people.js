// People & mesh: contacts (verify, QR card, block, favourite), live peers, radar, mesh stats, ping.
import { h, clear, svg, debounce, append } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { toast, toastError, sheet, confirmDialog, showQr, avatar, empty, segmented, promptDialog, signalBars, skeletonRows } from '../lib/ui.js';
import { playClass } from '../lib/motion.js';
import { timeAgo, distance, bearing, compass, formatDistance, formatBytes, formatDuration, displayName, validPos, formatPos } from '../lib/format.js';
import { hopsLabel, validLinkId } from '../lib/core_spec.js';

const TABS = [{ value: 'contacts', label: 'Contacts' }, { value: 'peers', label: 'Nearby' }, { value: 'radar', label: 'Radar' }, { value: 'stats', label: 'Stats' }];

export async function ownPosition(app) {
  const r = await app.api.try('position.get', {}, null, true);
  const p = r && r.position;
  if (validPos(p)) app.store.set({ position: p });
  return validPos(p) ? p : app.store.get('position');
}

export default async function render(app, params, root) {
  const tab = params.rest[0] || 'contacts';
  if (tab === 'contact' && params.rest[1]) return contactDetail(app, params.rest[1], root);
  const body = h('div');
  const page = h('div', { class: 'page' },
    h('div', { class: 'page-head' }, h('h1', null, 'People & mesh')),
    segmented(TABS, tab, v => app.nav('#/people/' + v), 'People views'),
    body);
  root.appendChild(page);
  const pos = await ownPosition(app);
  if (tab === 'peers') return peersTab(app, body, pos);
  if (tab === 'radar') return radarTab(app, body, pos);
  if (tab === 'stats') return statsTab(app, body);
  return contactsTab(app, body, pos);
}

function geoText(app, me, p) {
  if (!validPos(me) || !validPos(p)) return '';
  const b = bearing(me, p);
  return formatDistance(distance(me, p), app.units()) + ' ' + compass(b);
}

// ---------------------------------------------------------------- contacts
async function contactsTab(app, body, me) {
  const listEl = h('ul', { class: 'list' });
  append(body, 
    h('div', { class: 'row', style: { margin: '.75rem 0' } },
      h('button', { class: 'btn primary', onclick: () => addContact(app, load) }, icon('plus'), 'Add contact'),
      h('button', { class: 'btn', onclick: () => myCard(app) }, icon('qr'), 'My card')),
    listEl);
  async function load() {
    const r = await app.api.try('contacts.list', {}, { contacts: [] }, true);
    const cs = r.contacts || [];
    cs.sort((a, b) => (a.blocked - b.blocked) || (b.favorite - a.favorite) || String(a.name || '').localeCompare(String(b.name || '')));
    clear(listEl);
    if (!cs.length) { listEl.appendChild(h('li', null, empty('No contacts yet. Scan someone\'s card or add nodes you hear nearby.', null, 'people'))); return; }
    for (const c of cs) {
      const geo = geoText(app, me, c.position);
      listEl.appendChild(h('li', null, h('a', { class: 'item', href: '#/people/contact/' + c.node_id },
        avatar(c.node_id, c.name),
        h('div', { class: 'grow' },
          h('div', { class: 'title' }, displayName(c), ' ', c.favorite ? icon('star', { size: 14, fill: true }) : null),
          h('div', { class: 'sub' }, [c.status, c.verified ? '✔ verified' : 'unverified', c.blocked ? 'BLOCKED' : ''].filter(Boolean).join(' · '))),
        h('div', { class: 'meta' }, h('span', null, timeAgo(c.last_seen_ts)), c.last_seen_ts ? h('span', null, hopsLabel(c.hops, c.direct)) : null, geo ? h('span', null, geo) : null))));
    }
  }
  await load();
  const reload = debounce(load, 400);
  app.on('contact.updated', reload);
  return () => reload.cancel();
}

async function myCard(app) {
  try {
    const r = await app.api.call('identity.card');
    const info = app.store.get('info') || {};
    showQr(app, 'My contact card', r.card, {
      intro: 'Let someone scan this to add you. Fingerprint: ' + (info.fingerprint || []).join(' '),
    });
  } catch (e) { toastError(e, 'Card not available'); }
}

async function addContact(app, done) {
  const card = h('textarea', { rows: 3, placeholder: 'Paste an ember: contact card' });
  const nodeId = h('input', { type: 'text', placeholder: '16 hex characters', maxlength: 16, autocapitalize: 'off', spellcheck: 'false' });
  const name = h('input', { type: 'text', placeholder: 'Name (optional)' });
  const err = h('div', { class: 'field-error', role: 'alert' });
  const submit = async params => {
    try {
      const r = await app.api.call('contacts.add', params);
      s.close();
      toast('Added ' + displayName(r.contact), { kind: 'ok' });
      done && done();
      app.nav('#/people/contact/' + r.contact.node_id);
    } catch (e) { err.textContent = e.message || String(e.code); }
  };
  const s = sheet('Add contact', h('div', { class: 'stack' },
    app.has('qr_scan') ? h('button', {
      class: 'btn primary block', onclick: async () => {
        try { const r = await app.api.call('host.qr.scan'); card.value = r.text || ''; if (card.value) submit({ card: card.value.trim() }); } catch (e) { toastError(e, 'Scan failed'); }
      },
    }, icon('scan'), 'Scan contact QR') : null,
    h('label', { class: 'field-label' }, 'Contact card', card),
    h('button', { class: 'btn', onclick: () => { if (card.value.trim()) submit({ card: card.value.trim() }); else card.focus(); } }, 'Add from card'),
    h('details', null, h('summary', null, 'Add by node ID'), h('div', { class: 'stack' },
      nodeId, name,
      h('p', { class: 'muted small' }, 'Without a card the keys are learned from the mesh; verify the fingerprint words before trusting it.'),
      h('button', {
        class: 'btn', onclick: () => {
          const id = nodeId.value.trim().toLowerCase();
          if (!/^[0-9a-f]{16}$/.test(id)) { err.textContent = 'A node ID is 16 hex characters (0-9, a-f).'; return; }
          submit({ node_id: id, ...(name.value.trim() ? { name: name.value.trim() } : {}) });
        },
      }, 'Add by ID'))),
    err));
}

async function contactDetail(app, id, root) {
  const page = h('div', { class: 'page' });
  root.appendChild(page);
  const me = await ownPosition(app);
  let pingSent = 0;
  async function load() {
    const r = await app.api.try('contacts.list', {}, { contacts: [] }, true);
    const c = (r.contacts || []).find(x => x.node_id === id);
    clear(page);
    page.appendChild(h('div', { class: 'page-head' }, h('a', { class: 'btn ghost icon-only', href: '#/people', 'aria-label': 'Back' }, icon('back')), h('h1', null, c ? displayName(c) : 'Node ' + id.slice(0, 8))));
    if (!c) {
      append(page, empty('This node is not in your contacts.', h('button', { class: 'btn primary', onclick: async () => { try { await app.api.call('contacts.add', { node_id: id }); load(); } catch (e) { toastError(e); } } }, 'Add contact')));
      return;
    }
    const upd = async patch => { try { await app.api.call('contacts.update', { node_id: id, ...patch }); load(); } catch (e) { toastError(e); } };
    const pingOut = h('span', { class: 'muted small', 'aria-live': 'polite' });
    const cardText = h('textarea', { rows: 3, placeholder: 'Paste their ember:card?… text (from their My card screen)' });
    // A card carries the node's public keys: if it is this node, the keys now match and the contact is verified.
    async function verifyByCard(text) {
      try {
        const added = await app.api.call('contacts.add', { card: String(text || '').trim() });
        if (added.contact.node_id === id) { await upd({ verified: true }); toast('Card matches — verified', { kind: 'ok' }); }
        else toast('That card belongs to a different node (' + displayName(added.contact) + '); it was added as a separate contact', { kind: 'bad' });
      } catch (e) { toastError(e, 'Not a valid card'); }
    }
    append(page, 
      h('div', { class: 'card stack' },
        h('div', { class: 'row nowrap' }, avatar(c.node_id, c.name), h('div', { class: 'grow' },
          h('div', { class: 'title' }, displayName(c)),
          h('div', { class: 'muted small' }, c.status || ''),
          h('div', { class: 'row' }, c.verified ? h('span', { class: 'tag ok' }, icon('shield', { size: 14 }), 'Verified') : h('span', { class: 'tag warn' }, 'Not verified'),
            c.blocked ? h('span', { class: 'tag bad' }, 'Blocked') : null, c.favorite ? h('span', { class: 'tag accent' }, 'Favourite') : null))),
        h('div', { class: 'row' },
          h('a', { class: 'btn primary', href: '#/chats/' + encodeURIComponent('dm:' + id) }, icon('chat'), 'Message'),
          h('button', {
            class: 'btn', onclick: async () => {
              try { await app.api.call('mesh.ping', { node_id: id }); pingSent = Date.now(); pingOut.textContent = 'Ping sent…'; } catch (e) { toastError(e, 'Ping failed'); }
            },
          }, icon('ping'), 'Ping'), pingOut)),
      h('div', { class: 'card stack' },
        h('h2', null, 'Verify identity'),
        h('p', { class: 'muted' }, 'Ask them to read their fingerprint words (over the radio, phone or in person). If every word matches the list below, mark the contact as verified.'),
        (c.fingerprint || []).length ? h('div', { class: 'words' }, c.fingerprint.map(w => h('span', null, w)))
          : h('p', { class: 'notice small' }, 'This node\'s keys are not known yet, so there are no words to compare. They appear once its announcement is heard, or add it from their contact card (QR).'),
        h('div', { class: 'row' },
          c.verified ? h('button', { class: 'btn', onclick: () => upd({ verified: false }) }, 'Mark as not verified')
            : (c.fingerprint || []).length ? h('button', { class: 'btn primary', onclick: () => upd({ verified: true }) }, icon('check'), 'Words match — verify') : null,
          app.has('qr_scan') ? h('button', {
            class: 'btn', onclick: async () => {
              let text;
              try { text = (await app.api.call('host.qr.scan')).text; } catch (e) { toastError(e, 'Scan failed'); return; }
              verifyByCard(text);
            },
          }, icon('scan'), 'Verify by scanning their card') : null),
        h('details', null, h('summary', null, 'Verify with their card text'), h('div', { class: 'stack' },
          cardText, h('button', { class: 'btn', onclick: () => { if (cardText.value.trim()) verifyByCard(cardText.value.trim()); else cardText.focus(); } }, icon('shield'), 'Check card')))),
      h('div', { class: 'card' },
        h('h2', null, 'Mesh'),
        h('dl', { class: 'kv' },
          h('dt', null, 'Node ID'), h('dd', { class: 'mono' }, c.node_id),
          h('dt', null, 'Last heard'), h('dd', null, timeAgo(c.last_seen_ts)),
          h('dt', null, 'Path'), h('dd', null, c.last_seen_ts ? hopsLabel(c.hops, c.direct) : 'never heard'),
          h('dt', null, 'Via'), h('dd', null, linkLabel(app, c.via_link)),
          c.rssi != null ? [h('dt', null, 'Signal'), h('dd', null, c.rssi + ' dBm' + (c.snr != null ? ', SNR ' + c.snr + ' dB' : ''))] : null,
          h('dt', null, 'Position'), h('dd', null, validPos(c.position) ? formatPos(c.position) + ' (' + timeAgo(c.position.ts) + ')' : 'not shared'),
          validPos(c.position) && validPos(me) ? [h('dt', null, 'Distance'), h('dd', null, formatDistance(distance(me, c.position), app.units()) + ', bearing ' + Math.round(bearing(me, c.position)) + '° ' + compass(bearing(me, c.position)))] : null)),
      h('div', { class: 'card row' },
        h('button', { class: 'btn', onclick: () => upd({ favorite: !c.favorite }) }, icon('star', { fill: c.favorite }), c.favorite ? 'Unfavourite' : 'Favourite'),
        h('button', { class: 'btn', onclick: async () => { const n = await promptDialog('Rename contact', { value: c.name || '', label: 'Name' }); if (n !== null && n.trim()) upd({ name: n.trim() }); } }, icon('edit'), 'Rename'),
        h('button', { class: 'btn ' + (c.blocked ? '' : 'danger'), onclick: async () => { if (c.blocked || await confirmDialog('Block ' + displayName(c) + '?', 'Their messages will be hidden. Your device will still relay their traffic for others.', { ok: 'Block', danger: true })) upd({ blocked: !c.blocked }); } }, icon('block'), c.blocked ? 'Unblock' : 'Block'),
        h('button', {
          class: 'btn danger', onclick: async () => {
            if (!await confirmDialog('Remove contact?', 'Removes them from your contacts and forgets verification.', { ok: 'Remove', danger: true })) return;
            try { await app.api.call('contacts.remove', { node_id: id }); app.nav('#/people'); } catch (e) { toastError(e); }
          },
        }, icon('trash'), 'Remove')));
    app.on('mesh.pong', d => {
      if (d && d.node_id === id) pingOut.textContent = 'Pong: ' + formatDuration(d.rtt_ms) + ' round trip, ' + hopsLabel(d.hops);
    });
    void pingSent;
  }
  await load();
}

function linkLabel(app, id) {
  if (!validLinkId(id)) return '—';
  const l = (app.store.get('links') || []).find(x => x.id === id);
  return l ? (l.name || l.type) : 'link ' + id;
}

// ---------------------------------------------------------------- peers
async function peersTab(app, body, me) {
  const listEl = h('ul', { class: 'list' }, skeletonRows(4, 'li'));
  append(body, 
    h('div', { class: 'row', style: { margin: '.75rem 0' } },
      h('button', { class: 'btn', onclick: async () => { try { await app.api.call('mesh.announce'); toast('Announcement sent'); } catch (e) { toastError(e); } } }, icon('ping'), 'Announce myself'),
      h('span', { class: 'muted small' }, 'Updates live as nodes are heard.')),
    listEl);
  let contacts = new Set();
  async function load() {
    const [r, cr] = await Promise.all([app.api.try('mesh.peers', {}, { peers: [] }, true), app.api.try('contacts.list', {}, { contacts: [] }, true)]);
    contacts = new Set((cr.contacts || []).map(c => c.node_id));
    const ps = (r.peers || []).slice().sort((a, b) => (b.last_seen_ts || 0) - (a.last_seen_ts || 0));
    app.store.set({ peers: ps });
    clear(listEl);
    if (!ps.length) { listEl.appendChild(h('li', null, empty('No nodes heard yet. Add a link (Wi-Fi, Bluetooth, radio) so this device can reach others.', h('a', { class: 'btn', href: '#/links/add' }, icon('plus'), 'Add a link'), 'radar'))); return; }
    for (const p of ps) {
      const sig = (p.links || []).map(l => linkLabel(app, l.link_id) + (l.rssi != null ? ' ' + l.rssi + ' dBm' : '') + (l.snr != null ? ' SNR ' + l.snr : '')).join(', ');
      const best = (p.links || []).filter(l => l.snr != null || l.rssi != null).sort((a, b) => (b.snr ?? -99) - (a.snr ?? -99) || (b.rssi ?? -999) - (a.rssi ?? -999))[0];
      const geo = geoText(app, me, p.position);
      const stale = Date.now() - (p.last_seen_ts || 0) > 30 * 60e3;
      listEl.appendChild(h('li', null, h('div', { class: 'item' },
        avatar(p.node_id, p.name),
        h('div', { class: 'grow' },
          h('div', { class: 'title' }, displayName(p), ' ', best ? signalBars(best.snr, best.rssi) : null, ' ', stale ? h('span', { class: 'tag' }, 'stale') : null),
          h('div', { class: 'sub wrap' }, hopsLabel(p.hops, p.direct) + (sig ? ' · ' + sig : '')),
          h('div', { class: 'sub wrap' }, 'heard ' + timeAgo(p.last_seen_ts) + (geo ? ' · ' + geo : ''))),
        h('div', { class: 'row nowrap' },
          h('button', { class: 'btn small icon-only', 'aria-label': 'Ping ' + displayName(p), onclick: () => ping(app, p) }, icon('ping', { size: 18 })),
          contacts.has(p.node_id) ? h('a', { class: 'btn small icon-only', href: '#/chats/' + encodeURIComponent('dm:' + p.node_id), 'aria-label': 'Message' }, icon('chat', { size: 18 }))
            : h('button', { class: 'btn small icon-only', 'aria-label': 'Add to contacts', onclick: async () => { try { await app.api.call('contacts.add', { node_id: p.node_id, ...(p.name ? { name: p.name } : {}) }); toast('Added'); load(); } catch (e) { toastError(e); } } }, icon('plus', { size: 18 }))))));
    }
  }
  await load();
  const reload = debounce(load, 1000);
  app.on('peer.seen', reload);
  app.on('mesh.pong', d => toast('Pong from ' + (d.node_id || '').slice(0, 8) + ': ' + formatDuration(d.rtt_ms) + ', ' + hopsLabel(d.hops), { kind: 'ok' }));
  return () => reload.cancel();
}

async function ping(app, p) {
  try { await app.api.call('mesh.ping', { node_id: p.node_id }); toast('Ping sent to ' + displayName(p)); } catch (e) { toastError(e, 'Ping failed'); }
}

// ---------------------------------------------------------------- radar
function niceStep(maxM) {
  const steps = [100, 250, 500, 1000, 2000, 2500, 5000, 10000, 20000, 25000, 50000, 100000, 200000, 500000];
  for (const s of steps) if (s * 4 >= maxM) return s;
  return 1e6;
}

async function radarTab(app, body, me) {
  const box = h('div');
  body.appendChild(box);
  const seen = new Set();
  let drawn = false;
  let lastSweep = 0; // the sweep runs at most every 10 s (peers are heard all the time on a busy mesh)
  async function draw() {
    const r = await app.api.try('mesh.peers', {}, { peers: [] }, true);
    const alerts = (app.store.get('sosAlerts') || []);
    const sosIds = new Set(alerts.filter(a => !a.acked).map(a => a.node_id));
    clear(box);
    if (!validPos(me)) {
      append(box, empty('Your own position is unknown, so peers cannot be placed around you.',
        h('div', { class: 'row', style: { justifyContent: 'center' } },
          app.has('location') ? h('button', { class: 'btn primary', onclick: async () => { try { const p = await app.api.call('host.location.get'); app.store.set({ position: p }); me = p; draw(); } catch (e) { toastError(e, 'No GPS fix'); } } }, icon('pin'), 'Use GPS') : null,
          h('a', { class: 'btn', href: '#/settings' }, 'Enter position manually'))));
      return;
    }
    const pts = (r.peers || []).filter(p => validPos(p.position)).map(p => ({ p, d: distance(me, p.position), b: bearing(me, p.position) }));
    const noPos = (r.peers || []).length - pts.length;
    const maxD = Math.max(500, ...pts.map(x => x.d));
    const step = niceStep(maxD);
    const rings = Math.max(1, Math.ceil(maxD / step));
    const S = 400, C = S / 2, R = C - 28;
    const scale = R / (rings * step);
    const g = [svg('circle', { class: 'face', cx: C, cy: C, r: R })];
    // Sweep: a soft wedge + arm that turns twice after each update (no idle looping).
    const wa = 38 * Math.PI / 180;
    const sweep = svg('g', { class: 'sweep', 'aria-hidden': 'true' },
      svg('path', { class: 'sweep-wedge', d: 'M' + C + ' ' + C + 'L' + C + ' ' + (C - R) + 'A' + R + ' ' + R + ' 0 0 0 ' + (C - R * Math.sin(wa)).toFixed(1) + ' ' + (C - R * Math.cos(wa)).toFixed(1) + 'Z' }),
      svg('line', { class: 'sweep-arm', x1: C, y1: C, x2: C, y2: C - R }));
    g.push(sweep);
    for (let i = 1; i <= rings; i++) {
      g.push(svg('circle', { class: 'ring' + (i === rings ? ' outer' : ''), cx: C, cy: C, r: i * step * scale }));
      g.push(svg('text', { x: C + 3, y: C - i * step * scale - 3 }, formatDistance(i * step, app.units())));
    }
    g.push(svg('line', { class: 'axis', x1: C, y1: C - R, x2: C, y2: C + R }), svg('line', { class: 'axis', x1: C - R, y1: C, x2: C + R, y2: C }));
    for (const [t, x, y] of [['N', C, 14], ['S', C, S - 4], ['E', S - 10, C + 4], ['W', 10, C + 4]]) g.push(svg('text', { x, y, 'text-anchor': 'middle', 'font-weight': 700 }, t));
    g.push(svg('circle', { class: 'me-halo', cx: C, cy: C, r: 14 }), svg('circle', { class: 'me', cx: C, cy: C, r: 6 }));
    const blips = [];
    const placed = [];
    const dots = [], labels = [];
    for (const { p, d, b } of pts.slice().sort((a, b2) => a.d - b2.d)) {
      const rad = b * Math.PI / 180;
      const x = C + Math.sin(rad) * d * scale, y = C - Math.cos(rad) * d * scale;
      const cls = 'peer' + (sosIds.has(p.node_id) ? ' sos' : (p.direct || p.hops === 0) ? ' direct' : '');
      const label = (p.name || p.callsign || p.node_id.slice(0, 6)).slice(0, 14);
      // simple label placement: right of the dot unless near the right edge; nudge down on overlap
      const w = label.length * 6.6 + 4;
      const left = x + 10 + w > S;
      let lx = left ? x - 10 - w : x + 10, ly = y + 4;
      for (let tries = 0; tries < 8 && placed.some(r => lx < r.x + r.w && lx + w > r.x && ly - 11 < r.y && ly > r.y - 11); tries++) ly += 13;
      placed.push({ x: lx, y: ly, w });
      dots.push(svg('g', null,
        svg('title', null, displayName(p) + ': ' + formatDistance(d, app.units()) + ' ' + compass(b) + ', ' + hopsLabel(p.hops, p.direct)),
        svg('circle', { class: cls, cx: x.toFixed(1), cy: y.toFixed(1), r: 7 })));
      // a ping ring for peers new on the plot or heard in the last half minute
      if ((drawn && !seen.has(p.node_id)) || Date.now() - (p.last_seen_ts || 0) < 30e3) {
        const bl = svg('circle', { class: 'blip ' + cls, cx: x.toFixed(1), cy: y.toFixed(1), r: 7 });
        blips.push(bl);
      }
      seen.add(p.node_id);
      labels.push(svg('text', { class: 'label', x: lx.toFixed(1), y: ly.toFixed(1) }, label));
    }
    g.push(...blips, ...dots, ...labels);
    drawn = true;
    if (Date.now() - lastSweep > 10e3) { lastSweep = Date.now(); playClass(sweep, 'run', 2600); }
    for (const bl of blips) playClass(bl, 'run', 1400);
    append(box, 
      h('div', { class: 'radar-wrap' }, svg('svg', { class: 'radar', viewBox: '0 0 ' + S + ' ' + S, role: 'img', 'aria-label': 'Radar plot of ' + pts.length + ' peers around you, north up' }, g)),
      h('p', { class: 'muted small center' }, 'North is up. Green = heard directly, blue = via relays, red = SOS. ' + (noPos ? noPos + ' node(s) without a known position not shown.' : '')),
      h('table', { class: 'data' }, h('thead', null, h('tr', null, h('th', null, 'Node'), h('th', null, 'Distance'), h('th', null, 'Bearing'))),
        h('tbody', null, pts.sort((a, b) => a.d - b.d).map(({ p, d, b }) => h('tr', null, h('td', null, displayName(p)), h('td', { class: 'num' }, formatDistance(d, app.units())), h('td', { class: 'num' }, Math.round(b) + '° ' + compass(b)))))));
  }
  await draw();
  const redraw = debounce(draw, 2000);
  app.on('peer.seen', redraw);
  return () => redraw.cancel();
}

// ---------------------------------------------------------------- stats
async function statsTab(app, body) {
  const box = h('div');
  body.appendChild(box);
  async function load() {
    const [s, l] = await Promise.all([app.api.try('mesh.stats', {}, null, true), app.api.try('links.list', {}, { links: [] }, true)]);
    clear(box);
    if (!s) { box.appendChild(empty('Mesh statistics are not available yet.')); return; }
    const linkName = id => { const x = (l.links || []).find(k => k.id === id); return x ? x.name || x.type : 'link ' + id; };
    const n = v => (Number.isFinite(v) ? v.toLocaleString() : '—');
    append(box, 
      h('div', { class: 'card' }, h('dl', { class: 'kv' }, [
        ['Stored messages', n(s.stored_messages)], ['Stored data', formatBytes(s.stored_bytes)], ['Relayed for others', n(s.relayed)],
        ['Frames sent', n(s.tx_frames)], ['Frames received', n(s.rx_frames)], ['Bad frames', n(s.rx_bad)],
        ['Duplicates dropped', n(s.dropped_duplicates)], ['Expired (TTL)', n(s.dropped_ttl)], ['Uptime', formatDuration(s.uptime_ms)],
      ].flatMap(([k, v]) => [h('dt', null, k), h('dd', null, v)]))),
      h('h2', { class: 'section-title' }, 'Per link'),
      (s.per_link || []).length ? h('div', { class: 'card table-wrap' }, h('table', { class: 'data' },
        h('thead', null, h('tr', null, h('th', null, 'Link'), h('th', null, 'Airtime'), h('th', null, 'Duty cycle used'), h('th', null, 'Queue'))),
        h('tbody', null, s.per_link.map(p => {
          const pct = Math.min(100, (p.duty_used_permille || 0) / 10);
          return h('tr', null, h('td', null, linkName(p.link_id)), h('td', { class: 'num' }, formatDuration(p.airtime_ms)),
            h('td', null, h('div', { class: 'bar' + (pct > 80 ? ' bad' : pct > 50 ? ' warn' : '') }, h('i', { style: { width: pct + '%' } })), h('span', { class: 'small muted' }, (p.duty_used_permille / 10).toFixed(1) + ' %')),
            h('td', { class: 'num' }, n(p.queue)));
        })))) : empty('No links.'));
  }
  await load();
  const t = setInterval(load, app.store.get('settings').low_power ? 60000 : 10000);
  return () => clearInterval(t);
}
