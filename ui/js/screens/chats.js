// Chats: conversation list (DMs, channels, Public), thread view, compose, search, channel admin.
import { h, clear, debounce, highlight, append } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { toast, toastError, sheet, confirmDialog, showQr, copyText, avatar, empty, errorText, pickFile, saveFile, promptDialog, skeletonRows, bar } from '../lib/ui.js';
import { allows } from '../lib/motion.js';
import { announce } from '../lib/a11y.js';
import { loadConnectivity, capabilities } from '../lib/connectivity.js';
import { msgTime, timeAgo, shortDate, formatBytes, formatDistance, distance, bearing, compass, estimateWireBytes, estimateAirtimeMs, formatDuration, utf8Length, displayName, formatPos, validPos } from '../lib/format.js';
import { hopsLabel, convKind, networkTitle, validLinkId } from '../lib/core_spec.js';
import { strongPassphrase, passphraseStrength } from '../lib/ratelimit.js';

const STATUS = {
  queued: ['clock', 'Queued — waiting for a link'],
  sent: ['check', 'Sent'],
  relayed: ['relay', 'Relayed by another node'],
  delivered: ['check2', 'Delivered'],
  read: ['check2', 'Read'],
  failed: ['alert', 'Failed'],
};
const PRIORITIES = [['low', 'Low'], ['normal', 'Normal'], ['high', 'High'], ['emergency', 'Emergency']];
const CHECKIN_LABEL = { ok: "I'm OK", need_help: 'Need help', injured: 'Injured', moving: 'Moving', sheltering: 'Sheltering' };
const MAX_FILE = 128 * 1024; // core limit (RPC_API core behaviour notes)

export default async function render(app, params, root) {
  const conv = params.rest.length ? params.rest.join('/') : null;
  root.classList.add('chat-layout');
  const split = h('div', { class: 'chat-split' + (conv ? ' has-conv' : '') });
  const listPane = h('section', { class: 'chat-list', 'aria-label': 'Conversations' });
  const threadPane = h('section', { class: 'chat-thread', 'aria-label': 'Messages' });
  append(split, listPane, threadPane);
  root.appendChild(split);

  const cleanups = [];
  cleanups.push(await renderList(app, listPane, conv));
  if (conv) cleanups.push(await renderThread(app, threadPane, conv));
  else threadPane.appendChild(h('div', { class: 'empty wide-only-block' }, h('span', { class: 'empty-icon', 'aria-hidden': 'true' }, icon('chat', { size: 32 })), h('p', null, 'Choose a conversation')));
  return () => { for (const c of cleanups) c && c(); if (app.currentConv === conv) app.currentConv = null; };
}

// ---------------------------------------------------------------- list
async function renderList(app, pane, activeConv) {
  const searchInput = h('input', { type: 'search', placeholder: 'Search messages', 'aria-label': 'Search messages', enterkeyhint: 'search' });
  const listEl = h('ul', { class: 'list' }, skeletonRows(5, 'li'));
  const resultsEl = h('div', { hidden: true });
  append(pane, h('div', { class: 'page' },
    // one h1 per screen: with a conversation open (wide split view) the thread title is the h1
    h('div', { class: 'page-head' }, h(activeConv ? 'h2' : 'h1', null, 'Chats'),
      h('button', { class: 'btn primary', onclick: () => newChat(app) }, icon('plus'), 'New')),
    h('div', { class: 'field' }, searchInput),
    resultsEl, listEl));

  let channels = [];
  const load = async () => {
    const [cr, chr] = await Promise.all([
      app.api.try('conversations.list', {}, { conversations: [] }, true),
      app.api.try('channels.list', {}, { channels: [] }, true),
    ]);
    channels = chr.channels || [];
    const convs = [...(cr.conversations || [])];
    const known = new Set(convs.map(c => c.id));
    for (const ch of channels) {
      const id = 'ch:' + ch.id;
      if (!known.has(id)) convs.push({ id, kind: 'channel', title: ch.name, unread: ch.unread || 0, muted: ch.muted, last_ts: 0 });
    }
    // Public channel first if it has no recent activity? Keep most-recent-first, Public pinned on top.
    convs.sort((a, b) => (isDefault(b) - isDefault(a)) || ((b.last_ts || 0) - (a.last_ts || 0)));
    function isDefault(c) { const ch = channelFor(c.id); return ch && ch.is_default ? 1 : 0; }
    // first fill rises in (staggered); later refreshes replace the rows in place
    const first = !!listEl.querySelector('.skel-row');
    clear(listEl);
    listEl.classList.toggle('enter-list', first);
    if (!convs.length) { listEl.appendChild(h('li', null, empty('No conversations yet. Start one with New.', null, 'chat'))); return; }
    for (const c of convs) listEl.appendChild(h('li', null, convItem(c, channelFor(c.id), c.id === activeConv)));
  };
  const channelFor = id => id.startsWith('ch:') ? channels.find(ch => 'ch:' + ch.id === id) : null;

  const doSearch = debounce(async () => {
    const q = searchInput.value.trim();
    if (!q) { resultsEl.hidden = true; listEl.hidden = false; return; }
    const r = await app.api.try('messages.search', { query: q, limit: 50 }, null, true);
    clear(resultsEl);
    resultsEl.hidden = false; listEl.hidden = true;
    if (!r) { resultsEl.appendChild(empty('Search is not available yet.', null, 'search')); return; }
    const ms = r.messages || [];
    resultsEl.appendChild(h('h2', { class: 'section-title' }, ms.length + ' result' + (ms.length === 1 ? '' : 's')));
    resultsEl.appendChild(h('ul', { class: 'list' }, ms.map(m => h('li', null,
      h('a', { class: 'item', href: '#/chats/' + encodeURIComponent(m.conv) },
        h('div', { class: 'grow' },
          h('div', { class: 'title' }, m.outgoing ? 'You' : (m.from_name || 'Unknown')),
          h('div', { class: 'snippet' }, highlight(String(m.text || '').slice(0, 300), q.split(/\s+/)))),
        h('div', { class: 'meta' }, msgTime(m.ts)))))));
  }, 250);
  searchInput.addEventListener('input', doSearch);

  await load();
  const reload = debounce(load, 300);
  app.on('conversation.updated', reload);
  app.on('message.new', reload);
  app.on('message.updated', reload);
  app.on('channel.updated', reload);
  return () => { reload.cancel(); doSearch.cancel(); };
}

function convItem(c, ch, active) {
  const lm = c.last_message;
  const io = c.kind === 'interop' || convKind(c.id) === 'interop';
  const encrypted = ch ? ch.encrypted : (typeof c.encrypted === 'boolean' ? c.encrypted : c.kind === 'dm');
  const ioFrom = lm && lm.interop ? (lm.outgoing ? 'You' : String(lm.interop.from || lm.from_name || '')) + ': ' : '';
  const preview = lm ? (io ? ioFrom : lm.outgoing ? 'You: ' : (c.kind === 'channel' && lm.from_name ? lm.from_name + ': ' : '')) + previewText(lm) : (ch && ch.is_default ? 'Open channel for everyone nearby' : 'No messages yet');
  const title = c.title || (io ? networkTitle(c.id.slice(3)) : c.id);
  return h('a', { class: 'item' + (io ? ' interop-conv' : ''), href: '#/chats/' + encodeURIComponent(c.id), 'aria-current': active ? 'true' : null },
    io ? h('span', { class: 'avatar io', 'aria-hidden': 'true' }, icon('signal', { size: 20 })) : avatar(c.id, title, { channel: c.kind === 'channel' }),
    h('div', { class: 'grow' },
      h('div', { class: 'title' }, title, ' ', io ? h('span', { class: 'tag warn' }, 'radio · plain text') : encrypted ? icon('lock', { size: 14 }) : h('span', { class: 'tag' }, 'cleartext')),
      h('div', { class: 'sub' }, preview)),
    h('div', { class: 'meta' },
      c.last_ts ? msgTime(c.last_ts) : '',
      c.muted ? icon('mute', { size: 16 }) : null,
      c.unread ? h('span', { class: 'badge', 'aria-label': c.unread + ' unread' }, String(c.unread)) : null));
}

function previewText(m) {
  switch (m.kind) {
    case 'sos': return 'SOS: ' + (m.text || '');
    case 'position': return 'Shared a position';
    case 'file': return 'File: ' + ((m.file && m.file.name) || '');
    case 'checkin': return m.text && /^check-in/i.test(m.text) ? String(m.text) : 'Check-in: ' + (m.text || '');
    default: return String(m.text || '').slice(0, 120);
  }
}

// ---------------------------------------------------------------- new chat
async function newChat(app) {
  const body = h('div', { class: 'stack' });
  const s = sheet('New conversation', body);
  const contacts = ((await app.api.try('contacts.list', {}, { contacts: [] }, true)).contacts || []).filter(c => !c.blocked);
  contacts.sort((a, b) => (b.favorite - a.favorite) || String(a.name).localeCompare(String(b.name)));
  append(body, 
    h('h2', { class: 'section-title' }, 'Direct message'),
    contacts.length ? h('ul', { class: 'list' }, contacts.map(c => h('li', null,
      h('a', { class: 'item', href: '#/chats/' + encodeURIComponent('dm:' + c.node_id), onclick: () => s.close() },
        avatar(c.node_id, c.name), h('div', { class: 'grow' }, h('div', { class: 'title' }, displayName(c)), h('div', { class: 'sub' }, c.verified ? 'Verified' : 'Not verified')),
        c.favorite ? icon('star', { size: 16, fill: true }) : null))))
      : h('p', { class: 'muted' }, 'No contacts yet. Add people from the People tab.'),
    h('h2', { class: 'section-title' }, 'Channels'),
    h('div', { class: 'row' },
      h('button', { class: 'btn', onclick: () => { s.close(); createChannel(app); } }, icon('plus'), 'Create channel'),
      h('button', { class: 'btn', onclick: () => { s.close(); joinChannel(app); } }, icon('scan'), 'Join channel')));
}

const CHANNEL_BUSY = 'A different channel with the same identifier already exists on this device (another name or key). Choose another name or use a random key.';

/** Create channel: random key (recommended, shared by invite/QR only), passphrase, or open. */
async function createChannel(app) {
  const name = h('input', { type: 'text', maxlength: 40, placeholder: 'e.g. Ridge Camp' });
  const pass = h('input', { type: 'password', autocomplete: 'new-password', placeholder: 'Long passphrase everyone types in' });
  const meter = h('div', { class: 'small', 'aria-live': 'polite' });
  const passBox = h('div', { class: 'stack', hidden: true }, pass,
    h('div', { class: 'row' }, h('button', { class: 'btn small', type: 'button', onclick: () => { pass.type = 'text'; pass.value = strongPassphrase(); showStrength(); } }, icon('lock', { size: 16 }), 'Generate strong passphrase')),
    meter,
    h('p', { class: 'muted small' }, 'Anyone in radio range can record the channel and try guesses offline, so a passphrase must be long and unguessable (4+ random words, or the generated one). Short words, names or dates are cracked quickly.'));
  const LABEL = { weak: ['bad-text', 'Weak — can be guessed offline. Do not use it.'], fair: ['warn-text', 'Fair — longer is safer.'], strong: ['ok-text', 'Strong.'] };
  const showStrength = () => { const [c, t] = LABEL[passphraseStrength(pass.value)]; clear(meter).appendChild(h('span', { class: c }, pass.value ? t : '')); };
  pass.addEventListener('input', showStrength);
  let mode = 'random';
  const choice = (value, title, sub) => {
    const r = h('input', { type: 'radio', name: 'chmode', value, checked: value === mode });
    r.addEventListener('change', () => { mode = value; passBox.hidden = mode !== 'passphrase'; });
    return h('label', { class: 'check' }, r, h('span', null, h('strong', null, title), h('br'), h('span', { class: 'muted small' }, sub)));
  };
  const err = h('div', { class: 'field-error', role: 'alert' });
  const s = sheet('Create channel', h('div', { class: 'stack' },
    h('label', { class: 'field-label' }, 'Channel name', name),
    h('div', { class: 'stack', role: 'radiogroup', 'aria-label': 'Channel protection' },
      choice('random', 'Random key (recommended)', 'Encrypted with a key nobody can guess. Share it only by invite (QR or text) with the people who should read it.'),
      choice('passphrase', 'Passphrase', 'Encrypted with a passphrase people type in. Only as strong as the passphrase.'),
      choice('open', 'Open (no encryption)', 'Anyone can read it; it may also travel over amateur-radio links.')),
    passBox, err,
    h('div', { class: 'row end' }, h('button', {
      class: 'btn primary', onclick: async () => {
        err.textContent = '';
        if (!name.value.trim()) { name.focus(); return; }
        if (mode === 'passphrase') {
          if (!pass.value) { pass.focus(); return; }
          if (passphraseStrength(pass.value) === 'weak' && !await confirmDialog('Weak passphrase', 'This passphrase can be guessed by anyone who records the channel. Use a random key or the generated passphrase instead. Create it anyway?', { ok: 'Create anyway', danger: true })) return;
        }
        const p = { name: name.value.trim(), ...(mode === 'random' ? { random_key: true } : mode === 'passphrase' ? { passphrase: pass.value } : {}) };
        let r;
        try { r = await app.api.call('channels.create', p); } catch (e) { err.textContent = e.code === 'busy' ? CHANNEL_BUSY : errorText(e); return; }
        s.close();
        const conv = '#/chats/' + encodeURIComponent('ch:' + r.channel.id);
        if (r.passphrase_strength === 'weak') weakChannel(app, r.channel, conv);
        else {
          if (mode === 'random') toast('Channel created. Invite people from the channel menu (⋯ → Invite people).', { kind: 'ok', ms: 7000 });
          app.nav(conv);
        }
      },
    }, 'Create'))));
}

/** The core rated the passphrase weak: say so plainly and offer to switch to a random key. */
function weakChannel(app, ch, conv) {
  const s = sheet('Weak passphrase', h('div', { class: 'stack' },
    h('div', { class: 'notice bad' }, 'EmberMesh rates this passphrase weak: anyone in radio range can record "' + ch.name + '" and guess it offline. Do not use this channel for anything private.'),
    h('div', { class: 'row end' },
      h('button', { class: 'btn', onclick: () => { s.close(); app.nav(conv); } }, 'Keep it anyway'),
      h('button', {
        class: 'btn primary', onclick: async () => {
          try {
            await app.api.call('channels.leave', { id: ch.id });
            const r = await app.api.call('channels.create', { name: ch.name, random_key: true });
            s.close(); toast('Re-created with a random key. Share it by invite.', { kind: 'ok' });
            app.nav('#/chats/' + encodeURIComponent('ch:' + r.channel.id));
          } catch (e) { toastError(e, e.code === 'busy' ? CHANNEL_BUSY : 'Could not re-create'); }
        },
      }, 'Use a random key instead'))), { modal: true });
}

async function joinChannel(app) {
  const invite = h('textarea', { rows: 3, placeholder: 'Paste an ember: invite' });
  const name = h('input', { type: 'text', placeholder: 'Channel name' });
  const pass = h('input', { type: 'password', placeholder: 'Passphrase (if any)' });
  const scanBtn = app.has('qr_scan') ? h('button', {
    class: 'btn', onclick: async () => {
      try { const r = await app.api.call('host.qr.scan'); invite.value = r.text || ''; } catch (e) { toastError(e, 'Scan failed'); }
    },
  }, icon('scan'), 'Scan QR') : null;
  const s = sheet('Join channel', h('div', { class: 'stack' },
    h('label', { class: 'field-label' }, 'Invite', invite), scanBtn,
    h('details', null, h('summary', null, 'Or join by name and passphrase'), h('div', { class: 'stack' }, name, pass,
      h('p', { class: 'muted small' }, 'A short or guessable passphrase protects nothing: anyone in radio range can try guesses offline. Prefer an invite (QR) or a long random passphrase.'))),
    h('div', { class: 'row end' }, h('button', {
      class: 'btn primary', onclick: async () => {
        const p = invite.value.trim() ? { invite: invite.value.trim() } : { name: name.value.trim(), ...(pass.value ? { passphrase: pass.value } : {}) };
        if (!p.invite && !p.name) { invite.focus(); return; }
        if (p.passphrase && passphraseStrength(p.passphrase) === 'weak') toast('This channel uses a weak passphrase: outsiders may be able to read it.', { kind: 'bad' });
        try {
          const r = await app.api.call('channels.join', p);
          s.close();
          app.nav('#/chats/' + encodeURIComponent('ch:' + r.channel.id));
        } catch (e) { e.code === 'busy' ? toast(CHANNEL_BUSY, { kind: 'bad', ms: 8000 }) : toastError(e, 'Could not join'); }
      },
    }, 'Join'))));
}

// ---------------------------------------------------------------- thread
async function renderThread(app, pane, conv) {
  app.currentConv = conv;
  const isDm = convKind(conv) === 'dm';
  const isIo = convKind(conv) === 'interop';
  const network = isIo ? conv.slice(3) : null;
  const peerId = isDm ? conv.slice(3) : null;
  const chId = conv.startsWith('ch:') ? conv.slice(3) : null;

  const [convs, chans, contacts] = await Promise.all([
    app.api.try('conversations.list', {}, { conversations: [] }, true),
    app.api.try('channels.list', {}, { channels: [] }, true),
    isDm ? app.api.try('contacts.list', {}, { contacts: [] }, true) : null,
  ]);
  const convo = (convs.conversations || []).find(c => c.id === conv);
  const channel = chId ? (chans.channels || []).find(c => c.id === chId) : null;
  const contact = isDm ? ((contacts && contacts.contacts) || []).find(c => c.node_id === peerId) : null;
  const title = (convo && convo.title) || (channel && channel.name) || (contact && displayName(contact)) || (isDm ? 'Node ' + peerId.slice(0, 8) : isIo ? networkTitle(network) : conv);
  const encrypted = channel ? !!channel.encrypted : isIo ? false : (convo && typeof convo.encrypted === 'boolean' ? convo.encrypted : isDm);

  const msgsEl = h('div', { class: 'messages', role: 'log', 'aria-live': 'polite', 'aria-label': 'Messages' });
  const bubbles = new Map();
  let oldestTs = null;
  let lastDay = null;
  let firstDay = null;

  const sub = isIo ? 'Stations outside EmberMesh · plain text, never encrypted'
    : isDm
    ? (contact ? (contact.verified ? 'Verified · ' : 'Not verified · ') + hopsLabel(contact.hops, contact.direct) + ' · seen ' + timeAgo(contact.last_seen_ts) : 'End-to-end encrypted')
    : (encrypted ? 'Encrypted channel' : 'Cleartext, signed') + (channel && channel.members_seen ? ' · ' + channel.members_seen + ' members seen' : '');

  const head = h('div', { class: 'thread-head' },
    h('a', { class: 'btn ghost icon-only narrow-back', href: '#/chats', 'aria-label': 'Back to chats' }, icon('back')),
    isIo ? h('span', { class: 'avatar io', 'aria-hidden': 'true' }, icon('signal', { size: 20 })) : avatar(conv, title, { channel: !isDm }),
    h('div', { class: 'grow' }, h('h1', { class: 'title' }, title, ' ', encrypted ? icon('lock', { size: 14 }) : null), h('div', { class: 'sub small muted' }, sub)),
    h('button', { class: 'btn ghost icon-only', 'aria-label': 'Conversation options', onclick: () => convMenu(app, conv, { isDm, isIo, peerId, channel, title }) }, icon('more')));

  const loadMoreBtn = h('button', { class: 'btn small', hidden: true, onclick: () => loadOlder() }, 'Load earlier messages');
  msgsEl.appendChild(loadMoreBtn);
  // "New messages" pill: shown when messages arrive while the reader is scrolled up (the view
  // never jumps away from what they are reading).
  let unseen = 0;
  const newPill = h('button', { class: 'new-msgs', type: 'button', hidden: true, onclick: () => { toBottom(true); } }, icon('next', { size: 16, class: 'rot90' }), h('span'));
  const nearBottom = () => msgsEl.scrollHeight - msgsEl.scrollTop - msgsEl.clientHeight < 120;
  const toBottom = smooth => {
    unseen = 0; newPill.hidden = true;
    if (smooth && allows('full') && msgsEl.scrollTo) msgsEl.scrollTo({ top: msgsEl.scrollHeight, behavior: 'smooth' });
    else msgsEl.scrollTop = msgsEl.scrollHeight;
  };
  msgsEl.addEventListener('scroll', () => { if (unseen && nearBottom()) { unseen = 0; newPill.hidden = true; } }, { passive: true });

  const compose = isIo ? buildInteropCompose(app, conv, network, { lastMessages: () => [...bubbles.values()].map(b => b.m), add: m => addMessage(m, true) })
    : buildCompose(app, conv, { encrypted, isDm, add: m => addMessage(m, true) });
  compose.el.appendChild(newPill);
  append(clear(pane), h('div', { class: 'thread' }, head, msgsEl, compose.el));

  function dayLabel(ts) {
    const d = shortDate(ts);
    return d === shortDate(Date.now()) ? 'Today' : d;
  }

  let live = false; // after the initial load: new bubbles animate in
  function addMessage(m, atEnd) {
    if (!m || m.conv !== conv) return;
    if (bubbles.has(m.id)) { updateBubble(m); return; }
    const el = bubble(app, m, { refresh: () => updateBubble(m), remove: () => { el.remove(); bubbles.delete(m.id); } });
    bubbles.set(m.id, { el, m });
    const day = dayLabel(m.ts);
    if (atEnd) {
      if (day !== lastDay) { msgsEl.appendChild(h('div', { class: 'day-sep' + (live ? ' msg-new' : '') }, day)); lastDay = day; if (!firstDay) firstDay = day; }
      const stick = nearBottom();
      if (live) el.classList.add('msg-new');
      msgsEl.appendChild(el);
      // Sent messages and messages arriving at the bottom scroll into view instantly (the bubble
      // itself rises into place); while reading older messages the position is kept.
      if (stick || m.outgoing) toBottom(false);
      else if (live && !m.outgoing) { unseen++; newPill.lastChild.textContent = unseen + ' new message' + (unseen === 1 ? '' : 's'); newPill.hidden = false; announce(newPill.lastChild.textContent + ' below.'); }
    }
    return el;
  }

  /** Status-only change on an outgoing message: morph the tick in place (no re-render). */
  function patchStatus(b, status) {
    const old = b.el.querySelector('.tick');
    const fresh = statusTick(status, true);
    if (!old || !fresh || status === 'failed' || b.m.status === 'failed') return false;
    old.replaceWith(fresh);
    b.m = { ...b.m, status };
    return true;
  }

  function updateBubble(m) {
    const b = bubbles.get(m.id);
    if (!b) return;
    const keys = new Set([...Object.keys(m), ...Object.keys(b.m)]);
    keys.delete('status');
    if (m.status !== b.m.status && [...keys].every(k => m[k] === b.m[k]) && patchStatus(b, m.status)) return;
    const fresh = bubble(app, m, { refresh: () => updateBubble(m), remove: () => { fresh.remove(); bubbles.delete(m.id); } });
    b.el.replaceWith(fresh);
    bubbles.set(m.id, { el: fresh, m });
  }

  async function loadInitial() {
    let r;
    try { r = await app.api.call('messages.list', { conv, limit: 50 }); } catch (e) {
      msgsEl.appendChild(empty('Could not load messages: ' + errorText(e)));
      return;
    }
    const ms = r.messages || [];
    if (!ms.length) msgsEl.appendChild(h('div', { class: 'empty', id: 'thread-empty' }, isDm ? 'No messages yet. Say hello — it will be delivered when a path exists.' : isIo ? 'Nothing heard from ' + networkTitle(network) + ' stations yet.' : 'No messages in this channel yet.'));
    for (const m of ms) addMessage(m, true);
    if (compose.refresh) compose.refresh();
    oldestTs = ms.length ? ms[0].ts : null;
    loadMoreBtn.hidden = !r.more;
    msgsEl.scrollTop = msgsEl.scrollHeight;
    live = true;
    app.api.try('messages.mark_read', { conv }, null, true);
  }

  async function loadOlder() {
    if (oldestTs == null) return;
    loadMoreBtn.disabled = true;
    try {
      const r = await app.api.call('messages.list', { conv, before_ts: oldestTs, limit: 50 });
      const ms = (r.messages || []).filter(m => !bubbles.has(m.id));
      const prevH = msgsEl.scrollHeight;
      let anchor = loadMoreBtn.nextSibling;
      let day = null;
      for (const m of ms) {
        const d = dayLabel(m.ts);
        if (d !== day) { msgsEl.insertBefore(h('div', { class: 'day-sep' }, d), anchor); day = d; }
        const el = bubble(app, m, { refresh: () => updateBubble(m), remove: () => { el.remove(); bubbles.delete(m.id); } });
        bubbles.set(m.id, { el, m });
        msgsEl.insertBefore(el, anchor);
      }
      // drop a now-duplicated day separator
      if (anchor && anchor.classList && anchor.classList.contains('day-sep') && anchor.textContent === day) anchor.remove();
      if (ms.length) oldestTs = ms[0].ts;
      loadMoreBtn.hidden = !r.more;
      msgsEl.scrollTop = msgsEl.scrollHeight - prevH;
    } catch (e) { toastError(e); } finally { loadMoreBtn.disabled = false; }
  }

  app.on('message.new', d => {
    const m = d && d.message;
    if (!m || m.conv !== conv) return;
    const e = msgsEl.querySelector('#thread-empty');
    if (e) e.remove();
    addMessage(m, true);
    if (!m.outgoing && document.visibilityState === 'visible') app.api.try('messages.mark_read', { conv }, null, true);
  });
  // An authenticated copy replaced an unverified one, or a message changed in place: same id,
  // so update the existing bubble (never append a duplicate).
  app.on('message.updated', d => {
    const m = d && d.message;
    if (!m || m.conv !== conv) return;
    if (bubbles.has(m.id)) updateBubble(m); else addMessage(m, true);
  });
  app.on('file.progress', d => {
    const b = d && bubbles.get(d.id);
    if (!b || !b.m.file) return;
    const done = Number(d.total) > 0 && Number(d.received) >= Number(d.total);
    updateBubble({ ...b.m, file: { ...b.m.file, complete: done ? true : false, progress: done ? undefined : { received: Number(d.received) || 0, total: Number(d.total) || 0 } } });
  });
  app.on('message.status', d => {
    if (!d || d.conv !== conv) return;
    const b = bubbles.get(d.id);
    if (b && !patchStatus(b, d.status)) updateBubble({ ...b.m, status: d.status });
  });
  const onVis = () => { if (document.visibilityState === 'visible') app.api.try('messages.mark_read', { conv }, null, true); };
  document.addEventListener('visibilitychange', onVis);

  await loadInitial();
  compose.focus();
  return () => { document.removeEventListener('visibilitychange', onVis); if (compose.off) compose.off(); };
}

function linkName(app, id) {
  if (Number(id) >= 0x40000000) return 'internet relay'; // a relay session link (§11 rev 2)
  const l = (app.store.get('links') || []).find(x => x.id === id);
  return l ? l.name || l.type : id != null ? 'link ' + id : '';
}

function bubble(app, m, ctl) {
  const out = !!m.outgoing;
  const cls = ['msg', out ? 'out' : 'in', m.kind === 'interop' ? 'interop' : '', m.kind === 'sos' ? 'sos' : '', m.kind === 'system' ? 'system' : '',
    m.kind === 'checkin' ? 'checkin' : '', m.status === 'failed' ? 'failed' : ''].filter(Boolean).join(' ');
  if (m.kind === 'system') return h('div', { class: cls }, String(m.text || ''));

  const content = [];
  if (!out && m.kind !== 'interop') content.push(h('div', { class: 'from' }, m.from_name || 'Node ' + String(m.from || '').slice(0, 8)));
  if (m.kind === 'interop') {
    const io = m.interop || {};
    content.push(h('div', { class: 'from' }, h('span', { class: 'net' }, String(io.network || 'RADIO').toUpperCase()), ' ', io.from || m.from_name || '?', io.to ? ' → ' + io.to : ''));
  }
  const me = app.store.get('position');
  switch (m.kind) {
    case 'sos':
      content.push(h('div', { class: 'row nowrap' }, icon('sos'), h('strong', null, 'SOS')), h('div', { class: 'text' }, String(m.text || '')));
      if (m.position) content.push(posLine(app, m.position, me));
      break;
    case 'position':
      content.push(h('div', { class: 'row nowrap' }, icon('pin'), h('span', null, 'Shared position')));
      content.push(posLine(app, m.position, me));
      break;
    case 'checkin':
      content.push(h('div', { class: 'text' }, String(m.text || '')));
      break;
    case 'file': {
      const f = m.file || {};
      const pr = f.progress && f.progress.total > 0 ? Math.min(100, Math.round(f.progress.received / f.progress.total * 100)) : null;
      content.push(h('div', { class: 'row nowrap' }, icon('attach'), h('div', { class: 'grow' }, h('div', null, String(f.name || 'file')), h('div', { class: 'small muted' }, (f.mime || '') + ' · ' + formatBytes(f.size) + (f.complete === false ? ' · receiving…' + (pr != null ? ' ' + pr + ' %' : '') : '')),
        f.complete === false && pr != null ? bar(pr, '', { role: 'progressbar', 'aria-valuenow': String(pr), 'aria-valuemin': '0', 'aria-valuemax': '100' }) : null)));
      if (f.complete !== false) content.push(h('div', { class: 'actions' }, h('button', {
        class: 'btn small', onclick: async e => {
          e.stopPropagation();
          try {
            const r = await app.api.call('files.get', { id: m.id });
            if (/^image\/(png|jpeg|gif|webp)$/.test(r.mime)) {
              const img = h('img', { src: 'data:' + r.mime + ';base64,' + r.data_b64, alt: String(r.name || 'image') });
              img.style.maxWidth = '100%';
              sheet(String(r.name || 'Image'), h('div', { class: 'stack' }, img, h('button', { class: 'btn', onclick: () => saveFile(app, r.name, r.mime, r.data_b64) }, icon('download'), 'Save')));
            } else await saveFile(app, r.name || 'file', r.mime || 'application/octet-stream', r.data_b64);
          } catch (err) { toastError(err, 'Could not open file'); }
        },
      }, icon('download', { size: 18 }), 'Open / save')));
      break;
    }
    default:
      content.push(h('div', { class: 'text' }, String(m.text || '')));
  }

  const meta = [];
  if (m.priority && m.priority !== 'normal') meta.push(h('span', { class: 'tag ' + (m.priority === 'emergency' ? 'bad' : m.priority === 'high' ? 'warn' : '') }, m.priority));
  if (m.kind !== 'interop') meta.push(m.encrypted ? h('span', { title: 'End-to-end encrypted' }, icon('lock'), h('span', { class: 'sr-only' }, 'encrypted')) : h('span', { title: 'Cleartext (readable by anyone)' }, icon('globe'), h('span', { class: 'sr-only' }, 'cleartext')));
  if (m.signed_ok === false && m.kind !== 'interop') meta.push(h('span', { class: 'bad-text', title: 'The sender\'s identity could not be verified — it may be forged' }, '⚠ unverified sender'));
  if (!out && m.kind !== 'interop') meta.push(h('span', null, hopsLabel(m.hops)));
  if (validLinkId(m.via_link) && (!out || m.kind === 'interop')) meta.push(h('span', null, 'via ' + linkName(app, m.via_link)));
  meta.push(h('time', { datetime: new Date(m.ts).toISOString?.() }, msgTime(m.ts)));
  if (out && STATUS[m.status]) meta.push(statusTick(m.status, false));
  content.push(h('div', { class: 'mmeta' }, meta));

  const actions = h('div', { class: 'actions', hidden: m.status !== 'failed' });
  if (out && m.status === 'failed') actions.appendChild(h('button', {
    class: 'btn small', onclick: async e => {
      e.stopPropagation();
      try { const r = await app.api.call('messages.retry', { id: m.id }); Object.assign(m, r.message || { status: 'queued' }); ctl.refresh(); } catch (err) { toastError(err, 'Retry failed'); }
    },
  }, icon('retry', { size: 18 }), 'Retry'));
  append(actions, 
    h('button', { class: 'btn small', onclick: e => { e.stopPropagation(); copyText(String(m.text || '')); } }, icon('copy', { size: 18 }), 'Copy'),
    h('button', { class: 'btn small', onclick: e => { e.stopPropagation(); details(app, m); } }, icon('info', { size: 18 }), 'Details'),
    h('button', {
      class: 'btn small danger', onclick: async e => {
        e.stopPropagation();
        if (!await confirmDialog('Delete message?', 'This removes it from this device only.', { ok: 'Delete', danger: true })) return;
        try { await app.api.call('messages.delete', { id: m.id }); ctl.remove(); } catch (err) { toastError(err); }
      },
    }, icon('trash', { size: 18 }), 'Delete'));
  content.push(actions);

  const el = h('div', {
    class: cls, tabindex: '0', role: 'article', 'aria-label': (out ? 'You' : (m.from_name || 'Message')) + ', ' + msgTime(m.ts),
    onclick: () => { actions.hidden = !actions.hidden; },
    onkeydown: e => { if (e.key === 'Enter' && e.target === el) actions.hidden = !actions.hidden; },
  }, content);
  return el;
}

/**
 * Delivery tick (queued → sent → relayed → delivered → read). `morph`: the new glyph pops in over
 * the old one, which shrinks away (CSS, full motion; a fade when reduced; instant when off).
 * The element's class starts with "status-" (tests look for it).
 */
function statusTick(status, morph) {
  if (!STATUS[status]) return null;
  const [ic, label] = STATUS[status];
  const order = ['queued', 'sent', 'relayed', 'delivered', 'read'];
  const prev = order[order.indexOf(status) - 1];
  const el = h('span', { class: 'status-' + status + ' tick', title: label },
    icon(ic, { class: morph ? 'tick-in' : '' }),
    morph && prev && allows('reduced') ? icon(STATUS[prev][0], { class: 'tick-out' }) : null,
    h('span', { class: 'sr-only' }, label), status === 'failed' ? ' failed' : '');
  const outEl = el.querySelector('.tick-out');
  if (outEl) setTimeout(() => outEl.remove(), 400);
  return el;
}

function posLine(app, p, me) {
  if (!validPos(p)) return h('div', { class: 'small' }, 'Position unknown');
  const d = validPos(me) ? distance(me, p) : NaN;
  const b = validPos(me) ? bearing(me, p) : NaN;
  return h('div', { class: 'small mono' }, formatPos(p), Number.isFinite(d) ? ' · ' + formatDistance(d, app.units()) + ' ' + compass(b) + ' (' + Math.round(b) + '°)' : '',
    p.accuracy ? ' · ±' + Math.round(p.accuracy) + ' m' : '');
}

function details(app, m) {
  const rows = [
    ['From', m.outgoing ? 'You' : (m.from_name || '') + ' ' + (m.from || '')], ['Sent', new Date(m.ts).toString()],
    ['Received', m.received_ts ? new Date(m.received_ts).toString() : '—'], ['Kind', m.kind], ['Status', m.status], ['Priority', m.priority],
    ['Encrypted', m.encrypted ? 'yes' : 'no'], ['Signature', m.signed_ok === false ? 'INVALID' : m.signed_ok ? 'valid' : 'n/a'],
    ['Hops', m.outgoing ? '—' : hopsLabel(m.hops)], ['Via link', validLinkId(m.via_link) ? linkName(app, m.via_link) : '—'],
    ['Expires', m.expires_ts ? new Date(m.expires_ts).toString() : '—'], ['ID', m.id],
  ];
  if (m.interop) rows.push(['Network', String(m.interop.network)], ['Station', String(m.interop.from) + ' → ' + String(m.interop.to || '')]);
  sheet('Message details', h('dl', { class: 'kv' }, rows.flatMap(([k, v]) => [h('dt', null, k), h('dd', null, String(v ?? ''))])));
}

async function convMenu(app, conv, ctx) {
  const body = h('div', { class: 'stack' });
  const s = sheet(ctx.title, body);
  const items = [];
  if (ctx.isDm) {
    items.push(['people', 'Contact details', () => app.nav('#/people/contact/' + ctx.peerId)]);
    items.push(['ping', 'Ping', async () => { try { await app.api.call('mesh.ping', { node_id: ctx.peerId }); toast('Ping sent'); } catch (e) { toastError(e); } }]);
  }
  if (ctx.channel) {
    if (!ctx.channel.is_default) items.push(['qr', 'Invite people', async () => {
      try { const r = await app.api.call('channels.invite', { id: ctx.channel.id }); showQr(app, 'Invite to ' + ctx.channel.name, r.invite, { intro: 'Let others scan this, or send them the text. Anyone with the invite can read this channel.' }); } catch (e) { toastError(e); }
    }]);
    items.push(['mute', ctx.channel.muted ? 'Unmute' : 'Mute', async () => {
      try { await app.api.call('channels.update', { id: ctx.channel.id, muted: !ctx.channel.muted }); app.nav('#/chats/' + encodeURIComponent(conv)); } catch (e) { toastError(e); }
    }]);
  }
  if (!ctx.isIo && app.supports('position.share')) items.push(['pin', 'Share my position', async () => {
    try { await app.api.call('position.share', { conv }); toast('Position shared'); } catch (e) { toastError(e, 'Could not share position'); }
  }]);
  items.push(['download', 'Export as bundle (sneakernet)', async () => {
    try {
      const r = await app.api.call('bundle.export', { scope: 'conv', conv });
      await saveFile(app, 'embermesh-' + conv.replace(/[^a-z0-9]/gi, '_') + '.ember', 'application/octet-stream', r.data_b64);
      toast(r.count + ' messages exported (' + formatBytes(r.bytes) + ')');
    } catch (e) { toastError(e); }
  }]);
  items.push(['trash', 'Clear conversation', async () => {
    if (!await confirmDialog('Clear conversation?', 'Deletes all messages in this conversation from this device.', { ok: 'Clear', danger: true })) return;
    try { await app.api.call('conversations.clear', { conv }); app.nav('#/chats/' + encodeURIComponent(conv) + '?r=' + Date.now()); } catch (e) { toastError(e); }
  }]);
  if (ctx.channel && !ctx.channel.is_default) items.push(['close', 'Leave channel', async () => {
    if (!await confirmDialog('Leave ' + ctx.channel.name + '?', 'You will stop receiving it. You need a new invite to rejoin an encrypted channel.', { ok: 'Leave', danger: true })) return;
    try { await app.api.call('channels.leave', { id: ctx.channel.id }); app.nav('#/chats'); } catch (e) { toastError(e); }
  }]);
  body.appendChild(h('ul', { class: 'list' }, items.map(([ic, label, fn]) => h('li', null, h('button', { class: 'item', onclick: () => { s.close(); fn(); } }, icon(ic), h('span', { class: 'grow' }, label))))));
  // Private channels over the internet relay (§11 rev 2): only when the core allows it for this
  // channel (random key, or a passphrase rated strong)
  if (ctx.channel && ctx.channel.encrypted) {
    const conn = await loadConnectivity(app).catch(() => null);
    if (conn && capabilities(app.store.get('host'), conn).relay) body.appendChild(channelRelayRow(app, ctx.channel, conn));
  }
}

function channelRelayRow(app, ch, conn) {
  if (!ch.relay_allowed) {
    return h('p', { class: 'small muted channel-relay' }, 'Not offered over the internet relay: this channel uses a typed passphrase that was not rated strong, and anyone who guessed it could read what a relay carries. Channels with a random key can use the relay.');
  }
  const sw = h('input', { type: 'checkbox', class: 'switch', id: 'ch-relay', checked: !!ch.relay });
  sw.addEventListener('change', async () => {
    try { const r = await app.api.call('channels.update', { id: ch.id, relay: sw.checked }); Object.assign(ch, r.channel || { relay: sw.checked }); }
    catch (e) { sw.checked = !sw.checked; toastError(e, 'Not changed'); }
  });
  return h('div', { class: 'channel-relay' },
    h('label', { class: 'check', for: 'ch-relay' }, sw, h('span', { class: 'grow' }, h('strong', null, 'Also over the internet relay'), h('br'),
      h('span', { class: 'small muted' }, conn.internet && conn.internet.enabled ? 'Members far away get this channel through the relay. The relay cannot read it.' : 'Takes effect when the internet relay is on (Settings → Connections).'))));
}

// ---------------------------------------------------------------- interop compose (io:<network>)
// Replies to non-EmberMesh stations go out with interop.send on a chosen link; with no usable link
// the core's messages.send picks the link the network was last heard on.
const NOT_INTEROP = new Set(['raw', 'stream', 'rtl_tcp']); // mesh-only framing / receive-only SDR
function buildInteropCompose(app, conv, network, ctx) {
  const ta = h('textarea', { rows: 1, placeholder: 'Plain-text message', 'aria-label': 'Message text', maxlength: 200, enterkeyhint: 'send' });
  const to = h('input', { type: 'text', placeholder: 'To (callsign, empty = everyone)', 'aria-label': 'Send to station', autocapitalize: 'characters', maxlength: 20, spellcheck: 'false' });
  const via = h('select', { 'aria-label': 'Send via link' });
  const sendBtn = h('button', { class: 'btn primary icon-only', 'aria-label': 'Send', onclick: () => send() }, icon('send'));
  const hint = h('div', { class: 'hint', 'aria-live': 'polite' }, h('span', { class: 'warn-text' }, 'Plain text, readable by anyone · keep it short'));
  let toTouched = false;
  to.addEventListener('input', () => { toTouched = true; });
  const refresh = () => {
    const msgs = ctx.lastMessages().slice().sort((a, b) => b.ts - a.ts);
    const used = msgs.map(m => m.via_link).filter(validLinkId);
    const links = (app.store.get('links') || []).filter(l => !NOT_INTEROP.has(l.type) || used.includes(l.id));
    links.sort((a, b) => ((used.indexOf(a.id) + 1 || 999) - (used.indexOf(b.id) + 1 || 999)) || ((b.state === 'up') - (a.state === 'up')));
    const cur = Number(via.value) || 0;
    clear(via);
    via.appendChild(h('option', { value: '0' }, 'Automatic (link it was last heard on)'));
    for (const l of links) via.appendChild(h('option', { value: String(l.id), selected: l.id === cur }, 'via ' + (l.name || l.type) + (l.state === 'up' ? '' : ' (' + l.state + ')')));
    if (!cur && used.length && links.some(l => l.id === used[0])) via.value = String(used[0]);
    const lastIn = msgs.find(m => !m.outgoing && m.interop && m.interop.from);
    if (!toTouched && !to.value && lastIn) to.value = String(lastIn.interop.from);
  };
  ta.addEventListener('keydown', e => { if (e.key === 'Enter' && !e.shiftKey && !e.isComposing) { e.preventDefault(); send(); } });
  async function send() {
    const text = ta.value.trim();
    if (!text) { ta.focus(); return; }
    sendBtn.disabled = true;
    try {
      const linkId = Number(via.value) || 0;
      const r = linkId
        ? await app.api.call('interop.send', { link_id: linkId, text, ...(to.value.trim() ? { to: to.value.trim().toUpperCase() } : {}) })
        : await app.api.call('messages.send', { conv, text });
      ta.value = '';
      if (r && r.message) ctx.add(r.message);
    } catch (e) {
      toastError(e, e.code === 'not_found' ? 'Choose a link to send on' : 'Not sent');
    } finally { sendBtn.disabled = false; ta.focus(); }
  }
  const el = h('div', { class: 'compose interop' },
    h('div', { class: 'row nowrap' }, h('div', { class: 'grow' }, to), via),
    h('div', { class: 'row nowrap' }, h('div', { class: 'grow' }, ta), sendBtn), hint);
  const off = app.store.subscribe('links', refresh);
  refresh();
  return { el, focus: () => {}, refresh, off };
}

// ---------------------------------------------------------------- compose
function buildCompose(app, conv, ctx) {
  const ta = h('textarea', { rows: 1, placeholder: 'Message', 'aria-label': 'Message text', maxlength: 4000, enterkeyhint: 'send' });
  const prio = h('select', { 'aria-label': 'Priority' }, PRIORITIES.map(([v, l]) => h('option', { value: v, selected: v === 'normal' }, l)));
  const hint = h('div', { class: 'hint', 'aria-live': 'polite' });
  const sendBtn = h('button', { class: 'btn primary icon-only', 'aria-label': 'Send', onclick: () => send() }, icon('send'));
  const attachBtn = h('button', { class: 'btn ghost icon-only', 'aria-label': 'Send a small file', 'data-secondary': '', hidden: !app.supports('files.send'), onclick: () => attach() }, icon('attach'));
  const coarse = window.matchMedia && window.matchMedia('(pointer: coarse)').matches;

  const updateHint = () => {
    ta.style.height = 'auto';
    ta.style.height = Math.min(ta.scrollHeight, window.innerHeight * 0.3) + 'px';
    const text = ta.value;
    clear(hint);
    if (!text) {
      append(hint, h('span', null, ctx.encrypted ? 'Encrypted · ' : 'Cleartext, signed · '), h('span', null, coarse ? 'tap send' : 'Enter to send, Shift+Enter for new line'));
      return;
    }
    const body = utf8Length(text);
    const wire = estimateWireBytes(text, { encrypted: ctx.encrypted });
    const links = (app.store.get('links') || []).filter(l => l.state === 'up' && l.caps && l.caps.bitrate_bps > 0);
    const usable = ctx.encrypted ? links.filter(l => l.caps.encryption_allowed !== false) : links;
    const slow = usable.sort((a, b) => a.caps.bitrate_bps - b.caps.bitrate_bps)[0];
    append(hint, h('span', { class: body > 1000 ? 'over' : '' }, body + ' B'), h('span', null, '≈' + wire + ' B on air'));
    if (slow) {
      const ms = estimateAirtimeMs(wire, slow.caps.bitrate_bps);
      const frags = slow.caps.mtu ? Math.ceil(wire / slow.caps.mtu) : 1;
      append(hint, h('span', { class: ms > 10000 ? 'over' : '' }, '~' + formatDuration(ms) + ' on ' + (slow.name || slow.type) + (frags > 1 ? ' (' + frags + ' fragments)' : '')));
    }
    const blocked = ctx.encrypted ? links.filter(l => l.caps.encryption_allowed === false) : [];
    if (blocked.length) append(hint, h('span', { class: 'warn-text', title: 'Amateur-radio links may not carry encrypted traffic' }, 'not via ' + blocked.map(l => l.name || l.type).join(', ') + ' (no encryption allowed)'));
    if (body > 1000) append(hint, h('span', { class: 'over' }, 'Long messages are slow on radio links — keep it short'));
  };
  ta.addEventListener('input', updateHint);
  ta.addEventListener('keydown', e => {
    if (e.key === 'Enter' && !e.shiftKey && !coarse && !e.isComposing) { e.preventDefault(); send(); }
  });

  async function send() {
    const text = ta.value.trim();
    if (!text) { ta.focus(); return; }
    if (prio.value === 'emergency' && !await confirmDialog('Send as emergency?', 'Emergency priority jumps every queue on every relay. Use it only for life-threatening situations. For a repeating distress beacon use the SOS screen.', { ok: 'Send emergency', danger: true })) return;
    sendBtn.disabled = true;
    try {
      const r = await app.api.call('messages.send', { conv, text, priority: prio.value });
      ta.value = '';
      prio.value = 'normal';
      updateHint();
      if (r && r.message) ctx.add(r.message);
      announce(r && r.message && r.message.status === 'queued' ? 'Message saved — it goes out when a link is available.' : 'Message sent.');
    } catch (e) { toastError(e, 'Not sent'); } finally { sendBtn.disabled = false; ta.focus(); }
  }

  async function attach() {
    let f;
    try { f = await pickFile(app); } catch (e) { if (e.code !== 'bad_params') toastError(e); return; }
    const size = Math.floor((f.data_b64 || '').length * 3 / 4);
    if (size > MAX_FILE && !await confirmDialog('Large file', 'This file is ' + formatBytes(size) + '. Files this big take a very long time on radio links and may be refused. Send anyway?', { ok: 'Try anyway' })) return;
    try {
      const r = await app.api.call('files.send', { conv, name: f.name, mime: f.mime, data_b64: f.data_b64 });
      if (r && r.message) ctx.add(r.message);
    } catch (e) {
      if (e.code === 'not_implemented' || e.code === 'unsupported') attachBtn.hidden = true;
      toastError(e, e.code === 'too_large' ? 'File too large for the mesh' : 'File not sent');
    }
  }

  updateHint();
  const el = h('div', { class: 'compose' },
    h('div', { class: 'row nowrap' }, attachBtn, h('div', { class: 'grow' }, ta), sendBtn),
    h('div', { class: 'row nowrap', 'data-secondary': '' }, prio, hint));
  return { el, focus: () => { if (!coarse) ta.focus(); } };
}
