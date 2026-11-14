// EmberMesh UI entry point: backend bootstrap, app context, router, shell (strip, nav, toasts,
// lock screen, first-run setup). Screens live in ./screens/*.js and export
// `default async function render(app, params, root) → cleanup?`.
import { createBackend } from './backend.js';
import { h, clear, debounce, append, svg } from './lib/dom.js';
import { icon } from './lib/icons.js';
import { installMotion, routeDirection, screenTransition, finishScreenTransition, playClass } from './lib/motion.js';
import { createStore } from './lib/store.js';
import { toast, errorText, closeAllSheets, closeTopSheet } from './lib/ui.js';
import { backAction } from './lib/back.js';
import { createLimiter, unverified } from './lib/ratelimit.js';

const UNAVAILABLE = new Set(['not_implemented', 'unsupported']);
const THEME_CACHE = 'embermesh.ui.theme';

// Appearance/accessibility preferences (lib/prefs.js, RPC_API.md §8) and the accessibility layer
// (lib/a11y.js) are loaded defensively: the app works without them (full navigation, motion from
// the platform setting). prefs.js applies its mirrored values on import, so start loading at once.
const prefsReady = import('./lib/prefs.js').catch(e => { console.warn('prefs unavailable', e); return null; });
let prefsMod = null;

// Apply cached theme before anything renders (no inline script allowed by the CSP).
try {
  const c = JSON.parse(localStorage.getItem(THEME_CACHE) || 'null');
  if (c) applyLook(c.theme, c.low_power);
} catch { /* storage blocked */ }

function applyLook(theme, lowPower) {
  const t = ['dark', 'light', 'night'].includes(theme) ? theme : 'dark';
  document.documentElement.dataset.theme = t;
  document.documentElement.dataset.lowpower = lowPower ? '1' : '0';
  const meta = document.querySelector('meta[name="theme-color"]');
  if (meta) meta.setAttribute('content', t === 'light' ? '#f4f2ee' : '#000000');
}

// ---------------------------------------------------------------- app context
function makeApp(backend) {
  const unavailable = new Set();
  const store = createStore({
    info: null, host: null, settings: {}, links: [], peers: [], sos: null, sosAlerts: [], unread: 0, locked: false, position: null,
  });
  const screenSubs = new Set();

  const api = {
    async call(method, params) {
      try {
        return await backend.call(method, params || {});
      } catch (e) {
        const err = e instanceof Error ? e : Object.assign(new Error(String(e && e.message || e)), { code: e && e.code });
        if (err.code && UNAVAILABLE.has(err.code)) unavailable.add(method);
        if (err.code === 'locked' && !method.startsWith('security.')) app.setLocked(true);
        throw err;
      }
    },
    /** Like call, but resolves `fallback` for not_implemented/unsupported (and optionally any error). */
    async try(method, params, fallback = null, anyError = false) {
      try { return await api.call(method, params); } catch (e) {
        if (anyError || (e.code && UNAVAILABLE.has(e.code))) return fallback;
        throw e;
      }
    },
  };

  const app = {
    backend, api, store,
    /** host.info feature flag */
    has: f => { const hi = store.get('host'); return !!(hi && Array.isArray(hi.features) && hi.features.includes(f)); },
    /** method not known to be unavailable */
    supports: m => !unavailable.has(m),
    markUnavailable: m => unavailable.add(m),
    units: () => store.get('settings').units === 'imperial' ? 'imperial' : 'metric',
    nav: hash => { if (location.hash !== hash) location.hash = hash; else route(); },
    /** subscribe for the lifetime of the current screen */
    on(event, fn) {
      const off = backend.on(event, (...a) => { try { fn(...a); } catch (e) { console.error(e); } });
      screenSubs.add(off);
      return off;
    },
    async kvGet(ns, key, def = null) {
      const r = await api.try('kv.get', { ns, key }, null, true);
      return r && r.value !== null && r.value !== undefined ? r.value : def;
    },
    async kvSet(ns, key, value) { return api.call('kv.set', { ns, key, value }); },
    async refreshInfo() {
      const info = await api.call('node.info');
      store.set({ info, locked: !!info.locked });
      return info;
    },
    async refreshSettings() {
      const r = await api.try('settings.get', {}, { settings: {} }, true);
      const settings = (r && r.settings) || {};
      store.set({ settings });
      applyLook(settings.theme, settings.low_power);
      try { localStorage.setItem(THEME_CACHE, JSON.stringify({ theme: settings.theme, low_power: !!settings.low_power })); } catch { /* ignore */ }
      return settings;
    },
    async saveSettings(patch) {
      const r = await api.call('settings.set', { settings: patch });
      const settings = (r && r.settings) || { ...store.get('settings'), ...patch };
      store.set({ settings });
      applyLook(settings.theme, settings.low_power);
      try { localStorage.setItem(THEME_CACHE, JSON.stringify({ theme: settings.theme, low_power: !!settings.low_power })); } catch { /* ignore */ }
      return settings;
    },
    setLocked(v) {
      if (store.get('locked') === v) return;
      store.set({ locked: v });
      route();
    },
    clearScreenSubs() { for (const off of screenSubs) try { off(); } catch { /* ignore */ } screenSubs.clear(); },
    currentConv: null,
    refreshStrip: () => {},
  };
  return app;
}

// ---------------------------------------------------------------- routes
const ROUTES = [
  ['chats', () => import('./screens/chats.js')],
  ['people', () => import('./screens/people.js')],
  ['sos', () => import('./screens/sos.js')],
  ['board', () => import('./screens/bulletins.js')],
  ['links', () => import('./screens/links.js')],
  ['guides', () => import('./screens/guides.js')],
  ['tools', () => import('./screens/tools.js')],
  ['extras', () => import('./screens/extras.js')],
  ['settings', () => import('./screens/settings.js')],
  ['more', () => import('./screens/more.js')],
  ['help', () => import('./screens/help.js')],
];
const ROUTE_MAP = new Map(ROUTES);

const NAV = [
  { id: 'chats', label: 'Chats', icon: 'chat' },
  { id: 'people', label: 'People', icon: 'people' },
  { id: 'sos', label: 'SOS', icon: 'sos', cls: 'sos-tab' },
  { id: 'board', label: 'Bulletins', icon: 'board', wide: true },
  { id: 'links', label: 'Links', icon: 'link', wide: true },
  { id: 'guides', label: 'Guides', icon: 'book' },
  { id: 'help', label: 'Instructions', short: 'Help', icon: 'info' },
  { id: 'tools', label: 'Tools', icon: 'tool', wide: true },
  { id: 'extras', label: 'Notes & lists', icon: 'notes', wide: true },
  { id: 'settings', label: 'Settings', icon: 'gear', wide: true },
  { id: 'more', label: 'More', icon: 'more', narrow: true },
];
const MORE_IDS = new Set(['board', 'links', 'tools', 'extras', 'settings', 'more']);

let app;
let cleanup = null;
let routeSeq = 0;
const navLinks = new Map();
let unreadBadge;

function parseHash() {
  const raw = (location.hash || '').replace(/^#\/?/, '');
  const [path, query] = raw.split('?');
  const parts = path.split('/').filter(Boolean).map(p => { try { return decodeURIComponent(p); } catch { return p; } });
  return { name: parts[0] || 'chats', rest: parts.slice(1), query: new URLSearchParams(query || '') };
}

// Visited hashes, to tell "back" from "forward" for the screen transition direction.
const visits = [];
let lastHash = null;

async function route() {
  const seq = ++routeSeq;
  const main = document.getElementById('main');
  finishScreenTransition();
  if (cleanup) { try { cleanup(); } catch (e) { console.error(e); } cleanup = null; }
  app.clearScreenSubs();
  closeAllSheets();
  const nav = document.getElementById('nav');

  const stripEl = document.getElementById('strip');
  stripEl.hidden = false;
  if (app.store.get('locked')) {
    nav.hidden = true;
    stripEl.hidden = true;
    const mod = await import('./screens/lock.js');
    if (seq !== routeSeq) return;
    main.className = '';
    clear(main);
    cleanup = (await mod.default(app, {}, main)) || null;
    return;
  }
  const info = app.store.get('info');
  if (info && !info.name) {
    nav.hidden = true;
    stripEl.hidden = true;
    const mod = await import('./screens/setup.js');
    if (seq !== routeSeq) return;
    main.className = '';
    clear(main);
    cleanup = (await mod.default(app, {}, main)) || null;
    return;
  }
  nav.hidden = false;
  const r = parseHash();
  const loader = ROUTE_MAP.get(r.name) || ROUTE_MAP.get('chats');
  markNav();
  const hash = location.hash || '#/chats';
  const dir = routeDirection(lastHash, hash, visits);
  if (dir === 'back') visits.pop();
  else if (visits[visits.length - 1] !== hash) { visits.push(hash); if (visits.length > 50) visits.shift(); }
  lastHash = hash;
  let mod;
  try { mod = await loader(); } catch (e) {
    console.error(e);
    main.className = '';
    replaceMain(main, h('div', { class: 'page' }, h('p', { class: 'bad-text' }, 'Could not load screen: ' + errorText(e))));
    return;
  }
  if (seq !== routeSeq) return;
  // The new screen renders straight into #main (nothing waits for an animation); the transition
  // only decorates the swap and is cut short by the next navigation.
  const run = async () => {
    if (seq !== routeSeq) return;
    main.className = '';
    clear(main);
    main.scrollTop = 0;
    try {
      const c = (await mod.default(app, { rest: r.rest, query: r.query, name: r.name }, main)) || null;
      if (seq !== routeSeq) { if (c) try { c(); } catch (e) { console.error(e); } return; }
      cleanup = c;
    } catch (e) {
      console.error(e);
      if (seq !== routeSeq) return;
      if (e && e.code === 'locked') return;
      replaceMain(main, h('div', { class: 'page' }, h('h1', null, 'Something went wrong'), h('p', { class: 'muted' }, errorText(e)),
        h('a', { class: 'btn', href: '#/chats' }, 'Go to chats')));
    }
  };
  await screenTransition(run, main, dir);
}

function replaceMain(main, el) { clear(main).appendChild(el); }

// ---------------------------------------------------------------- shell
/** Destinations to show: lib/prefs.js navItems() (Simple mode trims them), else all of NAV. */
function navList() {
  try {
    const items = prefsMod && typeof prefsMod.navItems === 'function' ? prefsMod.navItems(NAV) : null;
    if (Array.isArray(items) && items.length && items.every(n => n && ROUTE_MAP.has(n.id))) return items;
  } catch (e) { console.error(e); }
  return NAV;
}

function buildNav() {
  const nav = document.getElementById('nav');
  const bar = h('div', { class: 'tabbar' });
  const items = navList();
  navLinks.clear();
  const unread = app && app.store.get('unread');
  for (const n of items) {
    const ico = h('span', { class: 'nav-ico', 'aria-hidden': 'true' }, icon(n.icon, { size: 24 }));
    const a = h('a', { href: '#/' + n.id, class: [n.cls, n.wide ? 'wide-only' : '', n.narrow ? 'narrow-only' : ''].filter(Boolean).join(' '), title: n.fullLabel && n.fullLabel !== n.label ? n.fullLabel : null },
      ico, n.short && !n.simple ? [h('span', { class: 'nav-lbl lbl-full' }, n.label), h('span', { class: 'nav-lbl lbl-short', 'aria-hidden': 'true' }, n.short)] : h('span', { class: 'nav-lbl' }, n.label));
    if (n.short && !n.simple) a.setAttribute('aria-label', n.label);
    if (n.id === 'chats') {
      unreadBadge = h('span', { class: 'badge', hidden: !unread }, unread ? (unread > 99 ? '99+' : String(unread)) : '');
      // phones: on the icon; wide sidebar: at the end of the row (CSS moves it)
      ico.appendChild(unreadBadge);
    }
    navLinks.set(n.id, a);
    bar.appendChild(a);
  }
  clear(nav).appendChild(bar);
  markNav();
}

/** aria-current on the destination for the current route ("More" covers what is not in the bar). */
function markNav() {
  const r = parseHash();
  for (const [id, a] of navLinks) {
    // "More" (phones) also lights for destinations that are reached through it.
    const cur = id === r.name || (id === 'more' && (MORE_IDS.has(r.name) || !navLinks.has(r.name)));
    if (cur) a.setAttribute('aria-current', 'page'); else a.removeAttribute('aria-current');
  }
}

/** The EmberMesh mark (docs/assets/logo.svg, simplified): an ember inside a hexagonal mesh. */
function brandMark() {
  const hex = 'M64 14 L107 39 L107 89 L64 114 L21 89 L21 39 Z';
  const nodes = [[64, 14], [107, 39], [107, 89], [64, 114], [21, 89], [21, 39]];
  return svg('svg', { class: 'brand-mark', viewBox: '0 0 128 128', 'aria-hidden': 'true', focusable: 'false' },
    svg('path', { class: 'bm-hex', d: hex }),
    nodes.map(([x, y]) => svg('circle', { class: 'bm-node', cx: x, cy: y, r: 9 })),
    svg('path', { class: 'bm-flame', d: 'M64 30 C73 44 88 55 88 75 C88 90 77 100 64 100 C51 100 40 90 40 75 C40 63 47 55 52 46 C54 55 58 60 63 62 C60 52 60 40 64 30 Z' }),
    svg('path', { class: 'bm-core', d: 'M64 62 C70 70 76 76 76 84 C76 92 70 97 64 97 C58 97 52 92 52 84 C52 78 56 74 59 70 C60 75 62 77 64 78 C63 72 62 67 64 62 Z' }));
}

function buildStrip() {
  const strip = document.getElementById('strip');
  strip.setAttribute('role', 'region');
  strip.setAttribute('aria-label', 'Status');
  const linksEl = h('a', { class: 's-item', href: '#/links', title: 'Links up' });
  const peersEl = h('a', { class: 's-item', href: '#/people/peers', title: 'Peers heard in the last 15 minutes' });
  const queueEl = h('span', { class: 's-item', title: 'Messages waiting to be sent' });
  const kindEl = h('span', { class: 'kind', title: 'Backend' });
  const banner = h('div');
  append(strip, h('div', { class: 'strip' }, h('span', { class: 'brand' }, brandMark(), h('span', { class: 'bm-word' }, 'EmberMesh')), linksEl, peersEl, queueEl, kindEl), banner);
  const shown = new Map();
  /** Rebuilds one status pill; the number pops when it changed (motion permitting). */
  const pill = (el, ic, value, label) => {
    append(clear(el), icon(ic, { size: 16 }), h('span', { class: 'num' }, value), h('span', { class: 'lbl' }, label));
    if (shown.has(el) && shown.get(el) !== value) playClass(el, 'changed', 600);
    shown.set(el, value);
  };
  let lastSosKey = '';

  let busy = false;
  const refresh = async () => {
    if (busy || app.store.get('locked')) return;
    busy = true;
    try {
      const [l, p] = await Promise.all([
        app.api.try('links.list', {}, { links: [] }, true),
        app.api.try('mesh.peers', {}, { peers: [] }, true),
      ]);
      app.store.set({ links: l.links || [], peers: p.peers || [] });
    } finally { busy = false; }
  };
  const render = () => {
    const links = app.store.get('links') || [];
    const up = links.filter(x => x.state === 'up').length;
    // state by shape and words too, not colour alone: a warning icon when no link is up
    linksEl.className = 's-item ' + (up ? 'ok' : 'bad');
    pill(linksEl, up ? 'link' : 'alert', up + '/' + links.length, up ? ' links' : ' links — none up');
    linksEl.setAttribute('aria-label', up + ' of ' + links.length + ' links up' + (up ? '' : ' — no connection'));
    linksEl.title = linksEl.getAttribute('aria-label');
    const now = Date.now();
    const heard = (app.store.get('peers') || []).filter(p => now - (p.last_seen_ts || 0) < 15 * 60e3).length;
    pill(peersEl, 'people', String(heard), ' heard');
    peersEl.setAttribute('aria-label', heard + ' peers heard in the last 15 minutes');
    const q = links.reduce((a, x) => a + ((x.stats && x.stats.queue) || 0), 0);
    pill(queueEl, 'clock', String(q), ' queued');
    queueEl.title = q + ' messages waiting to be sent';
    kindEl.textContent = app.backend.kind;
    kindEl.hidden = app.backend.kind !== 'mock';

    // SOS banners
    clear(banner);
    const sos = app.store.get('sos');
    if (sos && sos.active) {
      const b = h('button', { class: 'sos-banner', onclick: () => app.nav('#/sos') }, icon('sos'),
        h('span', { class: 'txt' }, 'SOS ACTIVE — beacon repeating. ' + ((sos.acked_by || []).length ? (sos.acked_by.length + ' heard you') : 'No acknowledgement yet') + ' · Tap to manage'));
      banner.appendChild(b);
      // one pulse per beacon / acknowledgement (no looping animation while the beacon runs)
      const key = (sos.sent || 0) + ':' + (sos.acked_by || []).length;
      if (key !== lastSosKey) playClass(b, 'pulse', 700);
      lastSosKey = key;
    } else lastSosKey = '';
    const all = (app.store.get('sosAlerts') || []).filter(a => !a.acked && now - (a.last_ts || 0) < 6 * 3600e3);
    // Verified alerts lead; alerts from unverified senders are only counted so a flood of forged
    // alerts cannot push real ones out of sight.
    const alerts = all.filter(a => !unverified(a));
    const unv = all.length - alerts.length;
    if (all.length) {
      const txt = alerts.length === 1 ? ('SOS from ' + (alerts[0].name || 'unknown') + ': ' + (alerts[0].text || '')).slice(0, 160)
        : alerts.length ? alerts.length + ' active SOS alerts nearby — tap to view' : '';
      banner.appendChild(h('button', { class: 'sos-banner incoming', onclick: () => app.nav('#/sos') }, icon('alert'),
        h('span', { class: 'txt' }, [txt, unv ? unv + ' SOS alert' + (unv > 1 ? 's' : '') + ' from unverified senders' : ''].filter(Boolean).join(' · '))));
    }
  };
  for (const k of ['links', 'peers', 'sos', 'sosAlerts']) app.store.subscribe(k, render);
  app.refreshStrip = debounce(refresh, 300);
  render();
  return { refresh, render };
}

async function refreshSos() {
  const [st, list] = await Promise.all([
    app.api.try('sos.status', {}, null, true),
    app.api.try('sos.list', {}, { alerts: [] }, true),
  ]);
  app.store.set({ sos: st, sosAlerts: (list && list.alerts) || [] });
}

async function refreshUnread() {
  const r = await app.api.try('conversations.list', {}, { conversations: [] }, true);
  const n = (r.conversations || []).filter(c => !c.muted).reduce((a, c) => a + (c.unread || 0), 0);
  app.store.set({ unread: n });
}

function notify(title, body, force) {
  const hidden = document.visibilityState === 'hidden';
  if ((hidden || force) && app.has('notify')) app.api.try('host.notify', { title, body }, null, true);
}

// One toast per conversation per 20 s and at most 6 per minute; SOS: one per sender per 2 min and
// at most 4 per minute (unverified senders: 1 per 5 min, 2 per minute overall).
const msgLimit = createLimiter({ perSourceMs: 20000, maxPerWindow: 6, windowMs: 60000 });
const sosLimit = createLimiter({ perSourceMs: 120000, maxPerWindow: 4, windowMs: 60000 });
const sosUnvLimit = createLimiter({ perSourceMs: 300000, maxPerWindow: 2, windowMs: 60000 });

function globalEvents(strip) {
  const b = app.backend;
  const refreshUnreadSoon = debounce(refreshUnread, 400);
  b.on('message.new', d => {
    const m = d && d.message;
    if (!m) return;
    refreshUnreadSoon();
    if (m.outgoing) return;
    const viewing = app.currentConv === m.conv && document.visibilityState === 'visible';
    if (!viewing && m.kind !== 'system') {
      const unv = m.kind !== 'interop' && unverified(m);
      const who = (m.from_name || (m.interop && m.interop.from) || 'Someone') + (unv ? ' (unverified sender)' : '');
      if (m.kind === 'sos') return; // sos.received handles SOS
      if (m.priority === 'emergency' && !unv) {
        if (sosLimit.allow('msg:' + m.from)) { toast(m.text || '', { kind: 'sos', title: 'Emergency from ' + who, icon: 'sos' }); notify('Emergency: ' + who, m.text || '', true); }
      } else if (msgLimit.allow(m.conv)) {
        toast(String(m.text || '').slice(0, 140), { title: who, icon: 'chat', action: { label: 'Open', run: () => app.nav('#/chats/' + encodeURIComponent(m.conv)) } });
        notify(who, String(m.text || '').slice(0, 200));
      }
    }
  });
  b.on('conversation.updated', refreshUnreadSoon);
  b.on('message.updated', refreshUnreadSoon);
  b.on('link.state', () => app.refreshStrip());
  b.on('peer.seen', () => app.refreshStrip());
  b.on('sos.updated', d => app.store.set({ sos: d }));
  b.on('sos.received', d => {
    refreshSos();
    if (!d) return;
    const unv = unverified(d);
    if (!(unv ? sosUnvLimit : sosLimit).allow(d.node_id)) return;
    const name = (d.name || 'unknown node') + (unv ? ' (unverified sender)' : '');
    toast(d.text || '', { kind: unv ? 'bad' : 'sos', title: 'SOS from ' + name, icon: 'sos', action: { label: 'View', run: () => app.nav('#/sos') } });
    if (!unv) notify('SOS from ' + name, d.text || '', true);
  });
  b.on('checkin.received', d => {
    if (d && (d.status === 'need_help' || d.status === 'injured') && msgLimit.allow('ci:' + d.node_id)) {
      toast((d.note || ''), { kind: 'bad', title: (d.name || 'Someone') + ': ' + d.status.replace('_', ' '), icon: 'alert' });
      notify((d.name || 'Someone') + ' needs help', d.note || '', true);
    }
  });
  b.on('bulletin.new', d => {
    const bl = d && d.bulletin;
    if (bl && !bl.mine && !unverified(bl) && (bl.category === 'hazard' || bl.category === 'missing' || bl.category === 'medical') && msgLimit.allow('bl:' + bl.from)) {
      toast(bl.title || '', { title: 'New ' + bl.category + ' bulletin', icon: 'board', action: { label: 'View', run: () => app.nav('#/board') } });
    }
  });
  b.on('security.locked', () => app.setLocked(true));
  b.on('host.location', p => { if (p && Number.isFinite(p.lat)) app.store.set({ position: p }); });
  let lastUnread = app.store.get('unread') || 0;
  app.store.subscribe('unread', n => {
    if (!unreadBadge) return;
    unreadBadge.hidden = !n;
    unreadBadge.textContent = n > 99 ? '99+' : String(n);
    if (n > lastUnread) playClass(unreadBadge, 'pop', 600);
    lastUnread = n;
  });
  // periodic refresh, slower in low-power mode
  let timer = 0;
  const schedule = () => {
    clearTimeout(timer);
    const lp = app.store.get('settings').low_power;
    timer = setTimeout(() => { if (document.visibilityState === 'visible') strip.refresh(); schedule(); }, lp ? 60000 : 15000);
  };
  schedule();
  app.store.subscribe('settings', schedule);
}

async function boot() {
  const main = document.getElementById('main');
  let backend;
  try {
    backend = await createBackend();
  } catch (e) {
    console.error(e);
    clear(main).appendChild(h('div', { class: 'page' }, h('h1', null, 'EmberMesh could not start'),
      h('p', null, 'The mesh core failed to load: ' + errorText(e)),
      h('p', { class: 'muted' }, 'Try reloading. To explore the interface without the core, add ?mock=1 to the address.')));
    return;
  }
  app = makeApp(backend);
  window.__ember = app; // debugging aid only
  prefsMod = await prefsReady;
  installMotion({ fallback: !prefsMod });
  if (prefsMod && typeof prefsMod.initPrefs === 'function') {
    prefsMod.initPrefs(backend);
    // Simple mode changes the navigation; rebuild it (cheap) when preferences change.
    if (typeof prefsMod.onPrefsChange === 'function') prefsMod.onPrefsChange(debounce(() => { if (document.getElementById('nav').firstChild) buildNav(); }, 50));
  }
  const hostInfo = await app.api.try('host.info', {}, null, true);
  app.store.set({ host: hostInfo || { features: [], endpoint_kinds: [] } });
  try { await app.refreshInfo(); } catch (e) {
    if (e.code !== 'locked') {
      clear(main).appendChild(h('div', { class: 'page' }, h('h1', null, 'Core not responding'), h('p', null, errorText(e))));
      return;
    }
    app.store.set({ locked: true });
  }
  if (!app.store.get('locked')) {
    const sec = await app.api.try('security.status', {}, null, true);
    if (sec && sec.locked) app.store.set({ locked: true });
  }
  if (!app.store.get('locked')) await app.refreshSettings();
  detectFlexGap();
  buildNav();
  const strip = buildStrip();
  import('./lib/a11y.js').then(m => { if (m && typeof m.initA11y === 'function') m.initA11y(app); })
    .then(() => import('./lib/simple.js')).then(m => { if (m && typeof m.initSimple === 'function') m.initSimple(app); })
    .catch(e => console.warn('a11y unavailable', e));
  globalEvents(strip);
  // node.ready is re-emitted after unlock, duress unlock and wipe: the identity, links and all
  // content may have changed, so reload everything and redraw the current screen.
  let booted = false;
  const onReady = debounce(async () => {
    if (!booted) return;
    const hi = await app.api.try('host.info', {}, null, true);
    if (hi) app.store.set({ host: hi });
    let info = null;
    try { info = await app.refreshInfo(); } catch { /* locked */ }
    if (info && !info.locked) {
      app.store.set({ locked: false });
      await app.refreshSettings();
      strip.refresh(); refreshSos(); refreshUnread();
    }
    route();
  }, 250);
  backend.on('node.ready', onReady);
  backend.on('time.updated', d => {
    const info = app.store.get('info');
    if (info && d && Number.isFinite(d.time_ms)) app.store.set({ info: { ...info, time_ms: d.time_ms, time_source: d.source || info.time_source } });
  });
  installBackHandler();
  window.addEventListener('hashchange', route);
  document.addEventListener('visibilitychange', () => { if (document.visibilityState === 'visible') app.refreshStrip(); });
  // Unlocking or finishing setup re-runs these.
  app.afterUnlock = async () => {
    await app.refreshInfo().catch(() => {});
    await app.refreshSettings();
    strip.refresh(); refreshSos(); refreshUnread();
  };
  if (!app.store.get('locked')) { strip.refresh(); refreshSos(); refreshUnread(); }
  // Scrolled state for sticky headers (a hairline appears once content runs underneath).
  document.addEventListener('scroll', e => {
    const t = e.target;
    if (t && t.nodeType === 1 && (t.id === 'main' || t.classList.contains('chat-list'))) t.classList.toggle('is-scrolled', t.scrollTop > 2);
  }, { capture: true, passive: true });
  await route();
  booted = true;
  try { document.dispatchEvent(new CustomEvent('ember:ready')); } catch { /* old WebView */ }
}

/** Chrome < 84 WebViews have no flex `gap`: mark <html> so app.css adds margins instead. */
function detectFlexGap() {
  try {
    const d = h('div', { style: { display: 'flex', flexDirection: 'column', rowGap: '1px', position: 'absolute', visibility: 'hidden' } }, h('div'), h('div'));
    document.body.appendChild(d);
    const ok = d.scrollHeight === 1;
    d.remove();
    document.documentElement.classList.toggle('no-flexgap', !ok);
  } catch { /* ignore */ }
}

/**
 * Android back button: native calls window.__emberBridge.onBack() and uses the result to decide
 * whether to leave the app. Adds the method to the bridge object the native backend installed
 * (never replaces it). Returns true when the UI handled the press.
 */
function installBackHandler() {
  const bridge = window.__emberBridge && typeof window.__emberBridge === 'object' ? window.__emberBridge : (window.__emberBridge = {});
  bridge.onBack = () => {
    try {
      const overlay = document.querySelector('.flash-overlay');
      const info = app.store.get('info');
      const a = backAction({ sheetOpen: !!document.querySelector('.overlay:not(.leaving)'), overlay: !!overlay, locked: app.store.get('locked'), setup: !!(info && !info.name), hash: location.hash });
      if (a === 'sheet') return closeTopSheet();
      if (a === 'overlay') { overlay.dataset.stop = '1'; overlay.remove(); return true; }
      if (a === 'up') { history.length > 1 ? history.back() : app.nav('#/' + parseHash().name); return true; }
      if (a === 'home') { app.nav('#/chats'); return true; }
      return false;
    } catch (e) { console.error(e); return false; }
  };
}

boot().catch(e => {
  console.error(e);
  const main = document.getElementById('main');
  if (main) clear(main).appendChild(h('div', { class: 'page' }, h('h1', null, 'Startup error'), h('p', null, String(e && e.message || e))));
});
