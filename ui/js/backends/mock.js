// In-browser fake of the whole EmberMesh RPC surface (docs/RPC_API.md) for UI development,
// demos (?mock=1) and tests. No DOM access at import time; works in Node too.
//
//   const b = new MockBackend({ params, persist, speed, autoplay, latency });
//   await b.init(); await b.call('node.info'); b.on('message.new', fn); b.close();
//
// Query params honoured (via opts.params): fresh=1, locked=1, unsupported=a.b,c.d

import {
  HOME, OWN_ID, WORLD, CANNED_REPLIES, PUBLIC_CHATTER, INTEROP_CHATTER, CHECKIN_STATUSES, BULLETIN_CATEGORIES,
  LINK_TYPES, ENDPOINT_KINDS, SCAN_RESULTS, fingerprint, seedState, seedLinks, linkCaps, linkConfig, linkStatus, worldPeer, peerPosition, destination,
} from '../mock/data.js';
import { SETTINGS_DEFAULTS, validateSettings, networkTitle } from '../lib/core_spec.js';
import { passphraseStrength } from '../lib/ratelimit.js';
import { MockMorseRx } from '../mock/morse_rx.js';
import { RADIOS } from '../mock/radios.js';
import { HARDWARE } from '../mock/hardware.js';
import { bestPassage, related as semRelated, ask as semAsk, MODEL_INFO } from '../mock/semantic.js';
import { GUIDES, CATEGORY_TITLES } from '../mock/guides.js';
import { TOOLS, runTool, bandsFor } from '../mock/tools.js';
import { encodeQR } from '../mock/qr.js';

const STORE_KEY = 'embermesh.mock.v1';
const VERSION = '0.1.0-mock';
const MIN = 60000;
const FEATURES = ['dm', 'channels', 'sos', 'checkin', 'position', 'bulletins', 'guides', 'tools', 'bundles', 'files', 'interop', 'netplan', 'kv', 'security'];
const HOST_FEATURES = ['hub', 'qr_scan', 'location', 'flash', 'tone', 'file', 'notify', 'webrtc', 'keep_awake'];
// Same defaults and ranges as the real core (docs/RPC_API.md, ui/js/lib/core_spec.js).
const DEFAULT_SETTINGS = { ...SETTINGS_DEFAULTS, links_autostart: [] };
const MAX_FILE = 128 * 1024;
// RPC_API "Locked state": the only core methods that work while storage is locked.
const LOCKED_OK = new Set(['node.info', 'node.set_time', 'position.set', 'security.status', 'security.unlock', 'security.wipe',
  'links.types', 'links.list', 'links.endpoint_state', 'rf.bands', 'tools.qr']);
const lockedOk = m => LOCKED_OK.has(m) || m.startsWith('guides.') || m.startsWith('tools.') || m.startsWith('host.');
// Platform extras the host offers (RPC_API §3 host.info.actions), as an Android shell would.
const HOST_ACTIONS = [
  { id: 'share_app', title: 'Share app', description: 'Send the EmberMesh installer to a nearby phone (Bluetooth, Nearby Share, file).', method: 'host.app.share' },
  { id: 'start_on_boot', title: 'Start on boot', description: 'Start relaying automatically when the phone is switched on.', method: 'host.app.start_on_boot', checked: false },
  { id: 'battery', title: 'Battery optimisation', description: 'Allow EmberMesh to keep running in the background (opens system settings).', method: 'host.app.battery_optimization' },
];

function rpcError(code, message) {
  const e = new Error(message || code);
  e.code = code;
  return e;
}
const clone = v => (v === undefined ? v : (typeof structuredClone === 'function' ? structuredClone(v) : JSON.parse(JSON.stringify(v))));
const hex = n => Array.from({ length: n }, () => Math.floor(Math.random() * 16).toString(16)).join('');
const pick = arr => arr[Math.floor(Math.random() * arr.length)];

function utf8Bytes(s) { return new TextEncoder().encode(String(s)); }
function b64FromBytes(bytes) {
  let s = '';
  for (let i = 0; i < bytes.length; i += 0x8000) s += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
  return btoa(s);
}
function bytesFromB64(b64) {
  const s = atob(String(b64).replace(/\s+/g, ''));
  const out = new Uint8Array(s.length);
  for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i);
  return out;
}
const b64utf8 = s => b64FromBytes(utf8Bytes(s));
const utf8b64 = b => new TextDecoder().decode(bytesFromB64(b));
function b64Size(b64) {
  const s = String(b64 || '').replace(/\s+/g, '');
  return Math.floor((s.length * 3) / 4) - (s.endsWith('==') ? 2 : s.endsWith('=') ? 1 : 0);
}
function simpleHash(s) {
  let h = 2166136261;
  for (const c of utf8Bytes(s)) { h ^= c; h = Math.imul(h, 16777619) >>> 0; }
  return h.toString(16).padStart(8, '0');
}
function parseEmberUri(text, kind) {
  const t = String(text || '').trim();
  const prefix = `ember:${kind}?`;
  if (!t.startsWith(prefix)) return null;
  try { return new URLSearchParams(t.slice(prefix.length)); } catch { return null; }
}
function localStore() {
  try { return typeof localStorage !== 'undefined' ? localStorage : null; } catch { return null; }
}
function getParam(params, key) {
  if (!params) return null;
  if (typeof params.get === 'function') return params.get(key);
  return params[key] ?? null;
}

export default class MockBackend {
  constructor(opts = {}) {
    this.kind = 'mock';
    this.opts = opts;
    this.speed = opts.speed ?? 1;
    this.latency = opts.latency ?? 40;
    this.autoplay = opts.autoplay ?? true;
    const store = opts.persist === false ? null : localStore();
    this.persist = !!store;
    const params = opts.params ?? (typeof location !== 'undefined' ? new URLSearchParams(location.search) : null);
    this.fresh = getParam(params, 'fresh') === '1';
    const unsup = getParam(params, 'unsupported');
    this.unsupported = new Set(unsup ? String(unsup).split(',').map(s => s.trim()).filter(Boolean) : []);

    this._handlers = new Map();
    this._timers = new Set();
    this._intervals = new Set();
    this._closed = false;
    this._inited = false;
    this._seq = 0;

    this._resetWorld(this.fresh);
    this.profile = { name: this.fresh ? '' : 'Alex', callsign: '', status: this.fresh ? '' : 'At home base' };
    this.startOnBoot = false;
    this.semanticOn = getParam(params, 'nosemantic') !== '1';
    this.nodeId = OWN_ID;
    this.settings = { ...DEFAULT_SETTINGS };
    this.kv = {};
    this.security = { passphrase: '', duress: '' };
    this.locked = false;
    this.timeOffset = 0;
    this.timeSource = 'system';

    if (this.persist && !this.fresh) this._load();
    if (getParam(params, 'locked') === '1') {
      this.security.passphrase = this.security.passphrase || 'demo';
      this.locked = true;
    }
  }

  // ---------------------------------------------------------------- plumbing

  _now() { return Date.now() + this.timeOffset; }

  _resetWorld(empty) {
    const now = this._now ? Date.now() + (this.timeOffset || 0) : Date.now();
    const s = seedState(now, { empty });
    Object.assign(this, {
      contacts: s.contacts, messages: s.messages, files: s.files, convMeta: s.convMeta, channels: s.channels,
      bulletins: s.bulletins, checkins: s.checkins, alerts: s.alerts, heardPlans: s.heardPlans,
    });
    this.links = new Map(empty ? [] : seedLinks(now).map(l => [l.id, l]));
    this.nextLinkId = Math.max(this.nextLinkId || 0, 4); // link ids are never reused
    this.peerSeen = new Map(empty ? [] : WORLD.map(p => [p.node_id, now - p.seenAgoMin * MIN]));
    this.plan = null;
    this.ownPosition = empty ? null : { lat: HOME.lat, lon: HOME.lon, alt: 52, accuracy: 12, ts: now - 5 * MIN };
    this.sos = { active: false, since_ts: undefined, text: '', sent: 0, acked_by: [] };
    this.hub = { running: false, urls: [], clients: 0 };
    this.pairings = new Map();
    this.stats = { relayed: 1843, dropped_duplicates: 5120, dropped_ttl: 77, tx_frames: 567, rx_frames: 1026, rx_bad: 31 };
  }

  _load() {
    const store = localStore();
    if (!store) return;
    try {
      const raw = store.getItem(STORE_KEY);
      if (!raw) return;
      const d = JSON.parse(raw);
      if (d.profile) Object.assign(this.profile, d.profile);
      if (d.settings) Object.assign(this.settings, d.settings);
      if (d.kv && typeof d.kv === 'object') this.kv = d.kv;
      if (d.security) Object.assign(this.security, d.security);
      if (d.nodeId) this.nodeId = d.nodeId;
      if (d.contactFlags) {
        for (const [id, f] of Object.entries(d.contactFlags)) {
          const c = this.contacts.get(id);
          if (c) Object.assign(c, f);
        }
      }
      if (d.wiped) this._resetWorld(true);
    } catch { /* corrupt storage: ignore */ }
  }

  _save() {
    if (!this.persist) return;
    const store = localStore();
    if (!store) return;
    const contactFlags = {};
    for (const c of this.contacts.values()) contactFlags[c.node_id] = { name: c.name, verified: c.verified, blocked: c.blocked, favorite: c.favorite };
    try {
      store.setItem(STORE_KEY, JSON.stringify({
        profile: this.profile, settings: this.settings, kv: this.kv, security: this.security,
        nodeId: this.nodeId, contactFlags, wiped: !!this.wiped,
      }));
    } catch { /* quota / private mode */ }
  }

  _after(ms, fn) {
    if (this._closed) return null;
    const t = setTimeout(() => {
      this._timers.delete(t);
      if (!this._closed) {
        try { fn(); } catch (e) { console.error('[mock]', e); }
      }
    }, Math.max(0, ms * this.speed));
    this._timers.add(t);
    return t;
  }

  _every(ms, fn) {
    if (this._closed) return null;
    const t = setInterval(() => {
      if (this._closed) return;
      try { fn(); } catch (e) { console.error('[mock]', e); }
    }, Math.max(1, ms * this.speed));
    this._intervals.add(t);
    return t;
  }

  _clearInterval(t) { if (t) { clearInterval(t); this._intervals.delete(t); } }

  _emit(event, data) {
    if (this._closed) return;
    Promise.resolve().then(() => {
      if (this._closed) return;
      for (const key of [event, '*']) {
        const set = this._handlers.get(key);
        if (!set) continue;
        for (const h of [...set]) {
          try { key === '*' ? h(event, clone(data)) : h(clone(data)); } catch (e) { console.error('[mock] handler', e); }
        }
      }
    });
  }

  on(event, handler) {
    if (!this._handlers.has(event)) this._handlers.set(event, new Set());
    this._handlers.get(event).add(handler);
    return () => { const s = this._handlers.get(event); if (s) s.delete(handler); };
  }

  close() {
    this._closed = true;
    for (const t of this._timers) clearTimeout(t);
    for (const t of this._intervals) clearInterval(t);
    this._timers.clear();
    this._intervals.clear();
    this._handlers.clear();
  }

  async init() {
    if (this._inited) return;
    this._inited = true;
    // T-Beam finishes its BLE connection.
    this._after(3500, () => this._setLinkState(3, 'up'));
    if (this.autoplay) this._startAutoplay();
    this._emit('node.ready', {});
  }

  async call(method, params = {}) {
    params = params && typeof params === 'object' && !Array.isArray(params) ? clone(params) : {};
    if (this.latency > 0) await new Promise(r => setTimeout(r, this.latency * this.speed * (0.5 + Math.random())));
    if (this._closed) throw rpcError('internal', 'Backend closed');
    if (this.unsupported.has(method)) {
      throw method.startsWith('host.') ? rpcError('unsupported', `${method} is not supported on this platform`) : rpcError('not_implemented', `${method} is not implemented yet`);
    }
    const fn = METHODS[method];
    if (!fn) throw rpcError(method.startsWith('host.') ? 'unsupported' : 'not_implemented', `Unknown method ${method}`);
    if (this.locked && !lockedOk(method)) throw rpcError('locked', 'storage is locked: unlock with security.unlock');
    const result = await fn.call(this, params);
    return clone(result ?? {});
  }

  // ---------------------------------------------------------------- helpers

  _need(p, key, type = 'string') {
    const v = p[key];
    if (v === undefined || v === null || (type === 'string' && (typeof v !== 'string' || v === ''))) throw rpcError('bad_params', `${key} is required`);
    if (type === 'number' && !Number.isFinite(Number(v))) throw rpcError('bad_params', `${key} must be a number`);
    return type === 'number' ? Number(v) : v;
  }

  _nodeInfo() {
    const L = this.locked; // while locked the identity fields are empty (RPC_API "Locked state")
    return {
      version: VERSION, node_id: L ? '' : this.nodeId, name: L ? '' : this.profile.name, callsign: L ? '' : this.profile.callsign, status: L ? '' : this.profile.status,
      fingerprint: L ? [] : fingerprint(this.nodeId), card: L ? '' : this._card(), platform: 'web', app_version: VERSION, device_name: 'Mock device',
      time_ms: this._now(), time_source: this.timeSource, uptime_ms: Math.round(performanceNow()), locked: L, features: FEATURES, storage: this.persist ? 'disk' : 'memory',
    };
  }

  _card() {
    const q = new URLSearchParams({ v: '1', id: this.nodeId, n: this.profile.name || '' });
    if (this.profile.callsign) q.set('cs', this.profile.callsign);
    q.set('k', b64utf8(`mock-pub-keys:${this.nodeId}`).replace(/=+$/, ''));
    return `ember:card?${q.toString()}`;
  }

  /** Message / bulletin ids: 24 lowercase hex chars, like the core. */
  _newId() { return (this.nodeId + Date.now().toString(16).slice(-4) + (++this._seq).toString(16).padStart(4, '0')).slice(0, 24); }

  _convTitle(conv) {
    if (conv.startsWith('ch:')) { const c = this.channels.get(conv.slice(3)); return c ? c.name : conv.slice(3); }
    if (conv.startsWith('io:')) return networkTitle(conv.slice(3));
    const id = conv.slice(3);
    const c = this.contacts.get(id);
    if (c && c.name) return c.name;
    const w = worldPeer(id);
    return w && w.name ? w.name : `Node ${id.slice(0, 4)}…${id.slice(-4)}`;
  }

  _meta(conv) {
    if (!this.convMeta.has(conv)) this.convMeta.set(conv, { unread: 0, muted: false });
    return this.convMeta.get(conv);
  }

  _conversation(conv) {
    const msgs = [...this.messages.values()].filter(m => m.conv === conv).sort((a, b) => a.ts - b.ts);
    const last = msgs[msgs.length - 1];
    const isCh = conv.startsWith('ch:');
    const isIo = conv.startsWith('io:');
    const ch = isCh ? this.channels.get(conv.slice(3)) : null;
    const meta = this._meta(conv);
    return {
      id: conv, kind: isCh ? 'channel' : isIo ? 'interop' : 'dm', title: this._convTitle(conv), unread: meta.unread,
      muted: ch ? ch.muted : meta.muted, encrypted: ch ? ch.encrypted : !isIo, ...(last ? { last_message: last } : {}), last_ts: last ? last.ts : 0,
    };
  }

  _channelOut(ch) {
    const { passphrase, ...rest } = ch;
    return { ...rest, unread: this._meta(`ch:${ch.id}`).unread };
  }

  _validConv(conv) {
    if (typeof conv !== 'string') throw rpcError('bad_params', 'conv is required');
    if (conv.startsWith('ch:')) {
      if (!this.channels.has(conv.slice(3))) throw rpcError('not_found', 'No such channel');
    } else if (conv.startsWith('dm:')) {
      if (!/^[0-9a-f]{16}$/.test(conv.slice(3))) throw rpcError('bad_params', 'Bad node id');
    } else if (!/^io:[a-z0-9_]{1,24}$/.test(conv)) throw rpcError('bad_params', 'conv must start with dm:, ch: or io:');
    return conv;
  }

  _emitConv(conv) { this._emit('conversation.updated', { conversation: this._conversation(conv) }); }

  _outgoing(conv, text, extra = {}) {
    const isCh = conv.startsWith('ch:');
    const ch = isCh ? this.channels.get(conv.slice(3)) : null;
    const now = this._now();
    const m = {
      id: this._newId(), conv, from: this.nodeId, from_name: this.profile.name, outgoing: true, ts: now, received_ts: now,
      text, kind: 'text', status: 'queued', priority: 'normal', encrypted: ch ? ch.encrypted : true, signed_ok: true,
      hops: 0, via_link: conv === 'ch:public' ? 2 : 1, ...extra,
    };
    this.messages.set(m.id, m);
    this._emit('message.new', { message: m });
    this._emitConv(conv);
    return m;
  }

  _incoming(conv, peer, text, extra = {}) {
    const isCh = conv.startsWith('ch:');
    const ch = isCh ? this.channels.get(conv.slice(3)) : null;
    const c = this.contacts.get(peer.node_id);
    if (c && c.blocked) return null;
    const now = this._now();
    const m = {
      id: this._newId(), conv, from: peer.node_id, from_name: (c && c.name) || peer.name, outgoing: false,
      ts: now - 800 - (peer.hops || 0) * 600, received_ts: now, text, kind: 'text', status: 'received', priority: 'normal',
      encrypted: ch ? ch.encrypted : true, signed_ok: true, hops: peer.hops, via_link: peer.link, ...extra,
    };
    this.messages.set(m.id, m);
    this._meta(conv).unread++;
    this._touchPeer(peer);
    this._emit('message.new', { message: m });
    this._emitConv(conv);
    if (ch) this._emit('channel.updated', { channel: this._channelOut(ch) });
    return m;
  }

  _touchPeer(peer) {
    const now = this._now();
    this.peerSeen.set(peer.node_id, now);
    const c = this.contacts.get(peer.node_id);
    if (c) c.last_seen_ts = now;
  }

  _setStatus(m, status) {
    if (!this.messages.has(m.id)) return;
    m.status = status;
    this._emit('message.status', { id: m.id, conv: m.conv, status });
  }

  _progress(m, { fail = false, reply = true } = {}) {
    const isDm = m.conv.startsWith('dm:');
    const k = m.priority === 'emergency' ? 0.3 : m.priority === 'high' ? 0.7 : m.priority === 'low' ? 1.6 : 1;
    const steps = isDm ? ['sent', 'relayed', 'delivered', 'read'] : ['sent', 'relayed'];
    const at = [500, 1500, 3000, 5000];
    if (m.via_link === 2 && this.links.get(2)?.state === 'up') {
      this._after(300 * k, () => this._emit('link.ptt', { link_id: 2, on: true }));
      this._after(1300 * k, () => { this._emit('link.ptt', { link_id: 2, on: false }); this._activity(2, 'tx', utf8Bytes(m.text).length + 40); });
    }
    if (fail) {
      this._after(at[0] * k, () => this._setStatus(m, 'sent'));
      this._after(at[2] * 2 * k, () => this._setStatus(m, 'failed'));
      return;
    }
    // chained so the order holds even when timers are coarse (tests run the mock at speed 0.001)
    const next = i => { if (i < steps.length) this._after((at[i] - (at[i - 1] || 0)) * k, () => { this._setStatus(m, steps[i]); next(i + 1); }); };
    next(0);
    if (!reply) return;
    if (isDm) {
      const peer = worldPeer(m.conv.slice(3));
      if (peer && this.peerSeen.get(peer.node_id) > this._now() - 6 * 3600000) {
        this._after(at[3] * k + 2500 + Math.random() * 2000, () => this._incoming(m.conv, peer, pick(CANNED_REPLIES)));
      }
    } else if (Math.random() < 0.5) {
      const pool = m.conv === 'ch:5e1d9a70' ? [worldPeer('d41f8e2b6c7a9035'), worldPeer('1d6e3f8a2c9b4071')] : WORLD.filter(p => p.name && p.seenAgoMin < 120);
      this._after(4000 + Math.random() * 4000, () => this._incoming(m.conv, pick(pool), pick(CANNED_REPLIES)));
    }
  }

  _activity(linkId, dir, bytes) {
    const l = this.links.get(linkId);
    if (!l || l.state !== 'up') return;
    if (dir === 'tx') {
      l.stats.tx_frames++; l.stats.tx_bytes += bytes;
      l.stats.airtime_ms += Math.round((bytes * 8 * 1000) / Math.max(1, l.caps.bitrate_bps));
      this.stats.tx_frames++;
    } else {
      l.stats.rx_frames++; l.stats.rx_bytes += bytes; l.stats.last_rx_ts = this._now(); this.stats.rx_frames++;
    }
    this._emit('link.activity', { link_id: linkId, dir, bytes });
  }

  _setLinkState(id, state, error) {
    const l = this.links.get(id);
    if (!l) return;
    l.state = state;
    if (error) l.error = error; else delete l.error;
    this._emit('link.state', { link: l });
  }

  /** RPC_API §6 Link extras: preset (audio), overrides (keys the user set), blocked:no_callsign. */
  _linkView(l) {
    const ham = l.config && (l.config.licence === 'amateur' || l.config.amateur === true);
    const v = { ...l, overrides: l.overrides || {} };
    // effective config: the profile callsign replaces the N0CALL placeholder
    if (l.config && l.config.callsign === 'N0CALL' && this.profile.callsign) v.config = { ...l.config, callsign: this.profile.callsign };
    if (l.config && typeof l.config.preset === 'string' && l.type === 'audio') v.preset = l.config.preset;
    if (ham && !this.profile.callsign) v.blocked = 'no_callsign';
    else delete v.blocked;
    return v;
  }
  _makeLink({ type, name, config = {}, endpoint, state = 'down', persist = true }) {
    const t = LINK_TYPES.find(x => x.type === type);
    if (!t) throw rpcError('bad_params', `unknown link type '${type}'`);
    const cfg = linkConfig(type, config);
    for (const f of t.config) {
      if (f.type === 'number' && cfg[f.key] !== undefined && (typeof cfg[f.key] !== 'number' || (f.min !== undefined && cfg[f.key] < f.min) || (f.max !== undefined && cfg[f.key] > f.max))) throw rpcError('bad_params', `config.${f.key}: ${f.min}..${f.max}`);
      if (f.type === 'select' && cfg[f.key] !== undefined && !f.options.some(o => o.value === cfg[f.key])) throw rpcError('bad_params', `config.${f.key}: unknown option`);
    }
    if ('callsign' in cfg && this.profile.callsign && cfg.callsign === 'N0CALL') cfg.callsign = this.profile.callsign; // the node copies the profile callsign in
    const id = this.nextLinkId++;
    const link = {
      id, type, name: name || t.title, state, endpoint: endpoint || { kind: 'loopback' }, config: cfg, overrides: clone(config || {}), caps: linkCaps(type, cfg),
      stats: { tx_frames: 0, rx_frames: 0, tx_bytes: 0, rx_bytes: 0, rx_errors: 0, airtime_ms: 0, last_rx_ts: 0, queue: 0 }, status: linkStatus(type, state), persist,
    };
    this.links.set(id, link);
    return link;
  }

  /** Simulated receive audio for audio / rtl_tcp links that are up (fields of RPC_API "Audio link status fields"). */
  _meterTick(l) {
    if (!(l.type === 'audio' || l.type === 'rtl_tcp')) return;
    const st = l.status;
    st.state = l.state;
    if (l.state !== 'up') { Object.assign(st, { rx_level_dbfs: -120, rx_peak_dbfs: -120, dcd: false, clipping: false, clip_percent: 0, rx_audio_age_ms: null, level_hint: 'no_audio' }); return; }
    const t = Date.now() / 1000;
    const busy = Math.sin(t / 3) > 0.2;
    const lvl = Math.round((busy ? -22 + 6 * Math.sin(t * 2.3) : -46 + 2 * Math.sin(t * 5)) * 10) / 10;
    const peak = Math.round(Math.min(0, lvl + 9 + 3 * Math.abs(Math.sin(t * 1.7))) * 10) / 10;
    const clip = peak > -0.5;
    if (busy && Math.random() < 0.3) { st.frames_ok = (st.frames_ok || 0) + 1; st.snr_db = Math.round((12 + 4 * Math.sin(t)) * 10) / 10; }
    Object.assign(st, { rx_level_dbfs: lvl, rx_peak_dbfs: peak, dcd: busy, clipping: clip, clip_percent: clip ? 0.4 : 0, rx_audio_age_ms: 20, tx_active: false, last_error: '',
      level_hint: clip ? 'clipping' : peak > -1 ? 'too_loud' : lvl < -50 ? 'too_quiet' : 'ok' });
  }

  _morse(session) {
    const r = this.morse && this.morse.get(String(session || ''));
    if (!r) throw rpcError('not_found', 'no such Morse session');
    return r;
  }

  _getLink(id) {
    const l = this.links.get(Number(id));
    if (!l) throw rpcError('not_found', 'No such link');
    return l;
  }

  _peerList() {
    const now = this._now();
    return WORLD.filter(p => this.peerSeen.has(p.node_id)).map(p => {
      const seen = this.peerSeen.get(p.node_id);
      const c = this.contacts.get(p.node_id);
      const pos = peerPosition(p, now);
      return {
        node_id: p.node_id, name: (c && c.name) || p.name, callsign: p.callsign, direct: p.hops === 0, hops: p.hops, last_seen_ts: seen,
        links: [{ link_id: p.link, ...(p.rssi !== undefined ? { rssi: p.rssi + Math.round(Math.random() * 4 - 2) } : {}), ...(p.snr !== undefined ? { snr: p.snr } : {}), last_rx_ts: seen }],
        ...(pos ? { position: { ...pos, ts: seen } } : {}),
      };
    });
  }

  _wipe() {
    this.wiped = true;
    this.nodeId = hex(16);
    this.profile = { name: '', callsign: '', status: '' };
    this.settings = { ...DEFAULT_SETTINGS };
    this.kv = {};
    this.security = { passphrase: '', duress: '' };
    this.locked = false;
    for (const l of this.links.values()) this._emit('link.state', { link: { ...l, state: 'down', removed: true } });
    this._resetWorld(true);
    this._save();
    this._emit('storage.dirty', {});
    this._emit('node.ready', {});
  }

  _startAutoplay() {
    this._every(8000, () => {
      const p = pick(WORLD.filter(x => x.seenAgoMin < 200));
      this._touchPeer(p);
      const c = this.contacts.get(p.node_id);
      this._emit('peer.seen', { node_id: p.node_id, name: p.name, link_id: p.link, hops: p.hops, ...(p.rssi !== undefined ? { rssi: p.rssi } : {}), ...(p.snr !== undefined ? { snr: p.snr } : {}) });
      if (c) this._emit('contact.updated', { contact: c });
    });
    this._every(3000, () => {
      const up = [...this.links.values()].filter(l => l.state === 'up');
      if (up.length) this._activity(pick(up).id, Math.random() < 0.6 ? 'rx' : 'tx', 40 + Math.floor(Math.random() * 200));
    });
    let tick = 0;
    this._every(45000, () => {
      tick++;
      if (tick % 3 === 0) {
        const it = pick(INTEROP_CHATTER);
        const net = String(it.network).toLowerCase();
        const gw = net === 'meshtastic' ? 3 : 2;
        if (this.links.has(gw)) {
          this._incoming('io:' + net, { node_id: '', name: it.from, hops: 0, link: gw },
            it.text, { kind: 'interop', from: '', encrypted: false, signed_ok: false, interop: { network: net, from: it.from, to: it.to || '' } });
        }
      } else {
        // Sometimes the first copy arrives before the sender's key: unverified, then superseded
        // by the authenticated copy with the same id (message.updated), like the core.
        const m = this._incoming('ch:public', pick(WORLD.filter(p => p.name && p.seenAgoMin < 120)), pick(PUBLIC_CHATTER), tick % 2 ? { signed_ok: false } : {});
        if (m && m.signed_ok === false) this._after(1500, () => { m.signed_ok = true; this._emit('message.updated', { message: m }); });
      }
    });
    let alt = 0;
    this._every(70000, () => {
      alt++;
      const p = pick(WORLD.filter(x => x.name && x.seenAgoMin < 120));
      if (alt % 2) {
        const ci = { node_id: p.node_id, name: p.name, status: pick(['ok', 'ok', 'moving', 'sheltering']), note: pick(['', 'All good', 'Heading to the school', 'Low on water']), ts: this._now() };
        const pos = peerPosition(p, this._now());
        if (pos) ci.position = pos;
        this.checkins.set(p.node_id, ci);
        this._emit('checkin.received', ci);
      } else {
        const b = { id: hex(24), from: p.node_id, from_name: p.name, signed_ok: true, ts: this._now(), expires_ts: this._now() + 24 * 3600000, category: pick(['info', 'trade', 'water', 'transport']), title: pick(['Ride to Ridge Camp tomorrow 08:00', 'Firewood available', 'Water truck at Elm & 5th at noon', 'Battery charging at the hall']), text: 'Details on the Public channel. Ask for ' + p.name + '.', mine: false };
        this.bulletins.set(b.id, b);
        this._emit('bulletin.new', { bulletin: b });
      }
    });
    this._after(20000, () => this._emit('netplan.heard', this.heardPlans[0] || { entries: [] }));
  }
}

function performanceNow() {
  try { return typeof performance !== 'undefined' ? performance.now() + 3600000 : 3600000; } catch { return 3600000; }
}

// --------------------------------------------------------------------- methods
// Each handler runs with `this` = MockBackend.

const METHODS = {
  // ---- node / identity
  'node.info'() { return this._nodeInfo(); },
  'node.set_profile'(p) {
    for (const k of ['name', 'callsign', 'status']) {
      if (p[k] === undefined) continue;
      if (typeof p[k] !== 'string') throw rpcError('bad_params', `${k} must be a string`);
      if (utf8Bytes(p[k]).length > (k === 'status' ? 120 : 40)) throw rpcError('too_large', `${k} is too long`);
      this.profile[k] = k === 'callsign' ? p[k].trim().toUpperCase() : p[k].trim();
    }
    this._save();
    return this._nodeInfo();
  },
  'node.set_time'(p) {
    const ts = this._need(p, 'ts', 'number');
    this.timeOffset = ts - Date.now();
    this.timeSource = ['system', 'gps', 'mesh', 'manual'].includes(p.source) ? p.source : 'manual';
    this._emit('time.updated', { time_ms: this._now(), source: this.timeSource });
    return { time_ms: this._now() };
  },
  'identity.card'() { return { card: this._card() }; },
  'identity.export'(p) {
    const pass = this._need(p, 'passphrase');
    const body = JSON.stringify({ v: 1, node_id: this.nodeId, name: this.profile.name, callsign: this.profile.callsign, check: simpleHash(pass + this.nodeId), keys: hex(64) });
    return { backup: `ember:backup?v=1&d=${b64utf8(body).replace(/=+$/, '')}` };
  },
  'identity.import'(p) {
    const backup = this._need(p, 'backup'), pass = this._need(p, 'passphrase');
    const q = parseEmberUri(backup, 'backup');
    if (!q || !q.get('d')) throw rpcError('bad_params', 'Not an EmberMesh identity backup');
    let d;
    try { const raw = q.get('d').replace(/ /g, '+'); d = JSON.parse(utf8b64(raw + '='.repeat((4 - raw.length % 4) % 4))); } catch { throw rpcError('bad_params', 'Backup is corrupt'); }
    if (!d || !/^[0-9a-f]{16}$/.test(d.node_id || '')) throw rpcError('bad_params', 'Backup is corrupt');
    if (d.check !== simpleHash(pass + d.node_id)) throw rpcError('denied', 'Wrong passphrase');
    this.nodeId = d.node_id;
    Object.assign(this.profile, { name: d.name || this.profile.name, callsign: d.callsign || '' });
    this._save();
    return this._nodeInfo();
  },

  // ---- contacts
  'contacts.list'() {
    return { contacts: [...this.contacts.values()].sort((a, b) => (b.favorite - a.favorite) || a.name.localeCompare(b.name)) };
  },
  'contacts.add'(p) {
    let id, name = p.name || '', callsign = '';
    if (p.card) {
      const q = parseEmberUri(p.card, 'card');
      if (!q || !/^[0-9a-f]{16}$/.test(q.get('id') || '')) throw rpcError('bad_params', 'Not a valid ember: contact card');
      id = q.get('id'); name = q.get('n') || name; callsign = q.get('cs') || '';
    } else {
      id = String(p.node_id || '').toLowerCase();
      if (!/^[0-9a-f]{16}$/.test(id)) throw rpcError('bad_params', 'node_id must be 16 hex characters');
    }
    if (id === this.nodeId) throw rpcError('bad_params', 'That is your own card');
    const w = worldPeer(id);
    let c = this.contacts.get(id);
    if (c) {
      if (name) c.name = name;
      if (callsign) c.callsign = callsign;
    } else {
      const now = this._now();
      // Unknown node added by id: no name, path or keys yet (fingerprint [] until its keys are heard).
      c = {
        node_id: id, name: name || (w && w.name) || '', callsign: callsign || (w ? w.callsign : ''), verified: false, blocked: false, favorite: false,
        status: w ? w.status : '', last_seen_ts: this.peerSeen.get(id) || 0, hops: w ? w.hops : null, direct: w ? w.hops === 0 : false, via_link: w ? w.link : 0,
        ...(w && w.rssi !== undefined ? { rssi: w.rssi } : {}), ...(w && w.snr !== undefined ? { snr: w.snr } : {}),
        ...(w && peerPosition(w, now) ? { position: peerPosition(w, now) } : {}), fingerprint: (w || p.card) ? fingerprint(id) : [],
      };
      if (p.card) c.verified = false;
      this.contacts.set(id, c);
    }
    this._save();
    this._emit('contact.updated', { contact: c });
    return { contact: c };
  },
  'contacts.update'(p) {
    const c = this.contacts.get(String(p.node_id || ''));
    if (!c) throw rpcError('not_found', 'No such contact');
    if (typeof p.name === 'string' && p.name.trim()) c.name = p.name.trim();
    for (const k of ['verified', 'blocked', 'favorite']) if (typeof p[k] === 'boolean') c[k] = p[k];
    this._save();
    this._emit('contact.updated', { contact: c });
    return { contact: c };
  },
  'contacts.remove'(p) {
    if (!this.contacts.delete(String(p.node_id || ''))) throw rpcError('not_found', 'No such contact');
    this._save();
    return {};
  },

  // ---- channels
  'channels.list'() { return { channels: [...this.channels.values()].map(c => this._channelOut(c)) }; },
  'channels.create'(p) {
    const name = this._need(p, 'name').trim();
    if (!name) throw rpcError('bad_params', 'name is required');
    if (name.length > 40) throw rpcError('too_large', 'Name too long');
    const random = p.random_key === true;
    const passphrase = random ? hex(32) : (p.passphrase || '');
    // passphrase channels get a deterministic id; another key on the same id → busy (like the core)
    const id = random ? hex(8) : passphrase ? simpleHash(name.toLowerCase() + '\u0000' + passphrase) : simpleHash(name.toLowerCase());
    const clash = this.channels.get(id);
    if (clash && (clash.passphrase || '') !== passphrase) throw rpcError('busy', 'another channel uses this id with a different key');
    const ch = clash || { id, name, encrypted: !!passphrase, is_default: false, muted: false, members_seen: 1, passphrase };
    this.channels.set(ch.id, ch);
    this._meta(`ch:${ch.id}`);
    this._emit('channel.updated', { channel: this._channelOut(ch) });
    this._emitConv(`ch:${ch.id}`);
    return { channel: this._channelOut(ch), ...(passphrase && !random ? { passphrase_strength: passphraseStrength(passphrase) } : {}) };
  },
  'channels.invite'(p) {
    const ch = this.channels.get(String(p.id || ''));
    if (!ch) throw rpcError('not_found', 'No such channel');
    const q = new URLSearchParams({ v: '1', ch: ch.id, n: ch.name });
    if (ch.encrypted) q.set('k', b64utf8(ch.passphrase || ch.id));
    return { invite: `ember:invite?${q.toString()}` };
  },
  'channels.join'(p) {
    let id, name, passphrase = '';
    if (p.invite) {
      const q = parseEmberUri(p.invite, 'invite');
      if (!q || !q.get('ch') || !q.get('n')) throw rpcError('bad_params', 'Not a valid channel invite');
      id = q.get('ch'); name = q.get('n');
      if (q.get('k')) { try { passphrase = utf8b64(q.get('k')); } catch { throw rpcError('bad_params', 'Invite key is corrupt'); } }
    } else {
      name = this._need(p, 'name').trim();
      passphrase = p.passphrase || '';
      const existing = [...this.channels.values()].find(c => c.name.toLowerCase() === name.toLowerCase() && (c.passphrase || '') === passphrase);
      id = existing ? existing.id : simpleHash(name.toLowerCase() + '\u0000' + passphrase);
    }
    let ch = this.channels.get(id);
    if (ch && (ch.passphrase || '') !== passphrase) throw rpcError('busy', 'another channel uses this id with a different key');
    if (!ch) {
      ch = { id, name, encrypted: !!passphrase, is_default: false, muted: false, members_seen: 1 + Math.floor(Math.random() * 5), passphrase };
      this.channels.set(id, ch);
      this._meta(`ch:${id}`);
    }
    this._emit('channel.updated', { channel: this._channelOut(ch) });
    this._emitConv(`ch:${id}`);
    return { channel: this._channelOut(ch) };
  },
  'channels.update'(p) {
    const ch = this.channels.get(String(p.id || ''));
    if (!ch) throw rpcError('not_found', 'No such channel');
    if (typeof p.muted === 'boolean') { ch.muted = p.muted; this._meta(`ch:${ch.id}`).muted = p.muted; }
    this._emit('channel.updated', { channel: this._channelOut(ch) });
    this._emitConv(`ch:${ch.id}`);
    return { channel: this._channelOut(ch) };
  },
  'channels.leave'(p) {
    const ch = this.channels.get(String(p.id || ''));
    if (!ch) throw rpcError('not_found', 'No such channel');
    if (ch.is_default) throw rpcError('denied', 'The Public channel cannot be left');
    this.channels.delete(ch.id);
    for (const [id, m] of this.messages) if (m.conv === `ch:${ch.id}`) this.messages.delete(id);
    this.convMeta.delete(`ch:${ch.id}`);
    return {};
  },

  // ---- conversations / messages
  'conversations.list'() {
    const convs = new Set();
    for (const ch of this.channels.values()) convs.add(`ch:${ch.id}`);
    for (const m of this.messages.values()) if (m.conv.startsWith('dm:') || m.conv.startsWith('io:')) convs.add(m.conv);
    const list = [...convs].map(c => this._conversation(c)).sort((a, b) => b.last_ts - a.last_ts);
    return { conversations: list };
  },
  'conversations.clear'(p) {
    const conv = this._validConv(p.conv);
    for (const [id, m] of this.messages) if (m.conv === conv) this.messages.delete(id);
    this._meta(conv).unread = 0;
    this._emitConv(conv);
    return {};
  },
  'messages.list'(p) {
    const conv = this._validConv(p.conv);
    const limit = Math.max(1, Math.min(500, Number(p.limit) || 50));
    let msgs = [...this.messages.values()].filter(m => m.conv === conv).sort((a, b) => a.ts - b.ts);
    if (p.before_ts !== undefined && p.before_ts !== null) msgs = msgs.filter(m => m.ts < Number(p.before_ts));
    const more = msgs.length > limit;
    return { messages: msgs.slice(-limit), more };
  },
  'messages.send'(p) {
    const conv = this._validConv(p.conv);
    const text = typeof p.text === 'string' ? p.text : '';
    if (!text.trim()) throw rpcError('bad_params', 'text is required');
    if (conv.startsWith('io:')) {
      const heard = [...this.messages.values()].filter(m => m.conv === conv && m.via_link > 0 && this.links.has(m.via_link)).sort((a, b) => b.ts - a.ts)[0];
      if (!heard) throw rpcError('not_found', 'no link carries this network; use interop.send with a link_id');
      return METHODS['interop.send'].call(this, { link_id: heard.via_link, text });
    }
    if (utf8Bytes(text).length > 4000) throw rpcError('too_large', 'Message too long (max 4000 bytes)');
    const priority = ['low', 'normal', 'high', 'emergency'].includes(p.priority) ? p.priority : 'normal';
    const extra = { priority };
    if (p.expires_s) extra.expires_ts = this._now() + Number(p.expires_s) * 1000;
    const m = this._outgoing(conv, text, extra);
    this._progress(m, { fail: /fail/i.test(text) });
    return { message: m };
  },
  'messages.retry'(p) {
    const m = this.messages.get(String(p.id || ''));
    if (!m) throw rpcError('not_found', 'No such message');
    if (!m.outgoing) throw rpcError('bad_params', 'Only outgoing messages can be retried');
    this._setStatus(m, 'queued');
    this._progress(m, { reply: false });
    return { message: m };
  },
  'messages.mark_read'(p) {
    const conv = this._validConv(p.conv);
    const meta = this._meta(conv);
    if (meta.unread) {
      meta.unread = 0;
      this._emitConv(conv);
      const ch = conv.startsWith('ch:') ? this.channels.get(conv.slice(3)) : null;
      if (ch) this._emit('channel.updated', { channel: this._channelOut(ch) });
    }
    return {};
  },
  'messages.delete'(p) {
    const m = this.messages.get(String(p.id || ''));
    if (!m) throw rpcError('not_found', 'No such message');
    this.messages.delete(m.id);
    this.files.delete(m.id);
    this._emitConv(m.conv);
    return {};
  },
  'messages.search'(p) {
    const q = String(p.query || '').trim().toLowerCase();
    if (!q) return { messages: [] };
    const limit = Math.max(1, Math.min(500, Number(p.limit) || 50));
    const res = [...this.messages.values()]
      .filter(m => m.text.toLowerCase().includes(q) || (m.from_name || '').toLowerCase().includes(q))
      .sort((a, b) => b.ts - a.ts).slice(0, limit);
    return { messages: res };
  },
  'files.send'(p) {
    const conv = this._validConv(p.conv);
    const name = this._need(p, 'name'), data = this._need(p, 'data_b64');
    const size = b64Size(data);
    if (size > MAX_FILE) throw rpcError('too_large', `Files over ${MAX_FILE / 1024} KiB cannot be sent over the mesh`);
    const mime = typeof p.mime === 'string' && p.mime ? p.mime : 'application/octet-stream';
    const m = this._outgoing(conv, name, { kind: 'file', file: { name, mime, size, complete: true } });
    this.files.set(m.id, { name, mime, data_b64: data });
    const chunks = Math.max(1, Math.ceil(size / 4096));
    for (let i = 1; i <= Math.min(chunks, 8); i++) this._after(i * 400, () => this._emit('file.progress', { id: m.id, received: Math.round((size * i) / Math.min(chunks, 8)), total: size }));
    this._progress(m, { reply: false });
    return { message: m };
  },
  'files.get'(p) {
    const f = this.files.get(String(p.id || ''));
    if (!f) throw rpcError('not_found', 'File not available');
    return { ...f };
  },
  'interop.send'(p) {
    const l = this._getLink(p.link_id);
    const text = this._need(p, 'text');
    if (l.type === 'raw' || l.type === 'stream') throw rpcError('unsupported', 'this link carries mesh frames only');
    if (l.type === 'rtl_tcp') throw rpcError('unsupported', 'receive-only link');
    const modem = l.config.modem;
    const network = l.type === 'meshtastic' ? 'meshtastic' : l.type === 'lora_at' ? 'lora' : ['cw', 'psk31', 'rtty', 'dtmf'].includes(modem) ? modem : 'aprs';
    const from = this.profile.callsign || (l.config.callsign !== 'N0CALL' ? l.config.callsign : '') || this.profile.name;
    const m = this._outgoing('io:' + network, text, { kind: 'interop', status: 'sent', encrypted: false, via_link: l.id, interop: { network, from, to: typeof p.to === 'string' ? p.to : '' } });
    return { message: m };
  },

  // ---- emergency
  'sos.start'(p) {
    const now = this._now();
    const interval = Math.max(10, Number(p.interval_s) || 60);
    const text = typeof p.text === 'string' && p.text.trim() ? p.text.trim() : 'SOS - need help';
    const position = p.position || this.ownPosition;
    if (!this.sos.active) {
      this.sos = { active: true, since_ts: now, text, sent: 1, acked_by: [] };
      const m = this._outgoing('ch:public', text, { kind: 'sos', priority: 'emergency', encrypted: false, ...(position ? { position } : {}) });
      this._progress(m, { reply: false });
      this._sosTimer = this._every(interval * 1000, () => { this.sos.sent++; this._emit('sos.updated', this._sosStatus()); });
      const ackers = WORLD.filter(w => w.name && w.seenAgoMin < 120).slice(0, 3);
      ackers.forEach((w, i) => this._after(2500 + i * 3500, () => {
        if (!this.sos.active) return;
        this.sos.acked_by.push({ node_id: w.node_id, name: w.name, ts: this._now() });
        this._emit('sos.updated', this._sosStatus());
      }));
    } else {
      this.sos.text = text;
    }
    this._emit('sos.updated', this._sosStatus());
    return { active: true, since_ts: this.sos.since_ts };
  },
  'sos.stop'() {
    this._clearInterval(this._sosTimer);
    this._sosTimer = null;
    this.sos = { active: false, since_ts: undefined, text: '', sent: this.sos.sent, acked_by: [] };
    this._emit('sos.updated', this._sosStatus());
    return { active: false };
  },
  'sos.status'() { return this._sosStatus(); },
  'sos.list'() { return { alerts: [...this.alerts.values()].map(a => ({ signed_ok: true, ...a })).sort((a, b) => b.last_ts - a.last_ts) }; },
  'sos.ack'(p) {
    const a = this.alerts.get(String(p.node_id || ''));
    if (!a) throw rpcError('not_found', 'No such alert');
    if (p.text !== undefined && typeof p.text !== 'string') throw rpcError('bad_params', 'text must be a string');
    a.acked = true;
    return {};
  },
  'checkin.send'(p) {
    if (!CHECKIN_STATUSES.includes(p.status)) throw rpcError('bad_params', `status must be one of ${CHECKIN_STATUSES.join(', ')}`);
    const ci = { node_id: this.nodeId, name: this.profile.name, status: p.status, note: typeof p.note === 'string' ? p.note.slice(0, 200) : '', ts: this._now() };
    const pos = p.position || (this.settings.position_sharing !== 'off' ? this.ownPosition : null);
    if (pos) ci.position = pos;
    this.checkins.set(this.nodeId, ci);
    const label = { ok: "I'm OK", need_help: 'Need help', injured: 'Injured', moving: 'Moving', sheltering: 'Sheltering' }[p.status];
    const m = this._outgoing('ch:public', `Check-in: ${label}${ci.note ? '. ' + ci.note : ''}`, { kind: 'checkin', encrypted: false, priority: p.status === 'ok' ? 'normal' : 'high', ...(pos ? { position: pos } : {}) });
    this._progress(m, { reply: false });
    return {};
  },
  'checkin.list'() { return { checkins: [...this.checkins.values()].sort((a, b) => b.ts - a.ts) }; },
  'position.set'(p) {
    const lat = Number(p.lat), lon = Number(p.lon);
    if (!Number.isFinite(lat) || !Number.isFinite(lon) || Math.abs(lat) > 90 || Math.abs(lon) > 180) throw rpcError('bad_params', 'lat/lon out of range');
    this.ownPosition = { lat, lon, ...(p.alt !== undefined ? { alt: Number(p.alt) } : {}), ...(p.accuracy !== undefined ? { accuracy: Number(p.accuracy) } : {}), ts: Number(p.ts) || this._now() };
    return {};
  },
  'position.get'() { return this.ownPosition ? { position: this.ownPosition } : {}; },
  'position.share'(p) {
    const conv = this._validConv(p.conv);
    if (!this.ownPosition) throw rpcError('not_found', 'No position fix yet');
    const pos = this.ownPosition;
    const m = this._outgoing(conv, `Position ${pos.lat.toFixed(5)}, ${pos.lon.toFixed(5)}`, { kind: 'position', position: pos });
    this._progress(m, { reply: false });
    return { message: m };
  },
  'bulletins.list'(p) {
    const now = this._now();
    if (p.category && !BULLETIN_CATEGORIES.includes(p.category)) throw rpcError('bad_params', 'Unknown category');
    const list = [...this.bulletins.values()].filter(b => b.expires_ts > now && (!p.category || b.category === p.category)).sort((a, b) => b.ts - a.ts);
    return { bulletins: list };
  },
  'bulletins.post'(p) {
    if (!BULLETIN_CATEGORIES.includes(p.category)) throw rpcError('bad_params', 'Unknown category');
    const title = this._need(p, 'title').trim();
    const text = typeof p.text === 'string' ? p.text : '';
    if (utf8Bytes(title + text).length > 1200) throw rpcError('too_large', 'Bulletin too long');
    const now = this._now();
    const exp = Math.max(60, Math.min(30 * 86400, Number(p.expires_s) || 3 * 86400));
    const b = { id: hex(24), from: this.nodeId, from_name: this.profile.name, signed_ok: true, ts: now, expires_ts: now + exp * 1000, category: p.category, title, text, mine: true, ...(p.position ? { position: p.position } : {}) };
    this.bulletins.set(b.id, b);
    this._emit('bulletin.new', { bulletin: b });
    return { bulletin: b };
  },
  'bulletins.delete'(p) {
    const b = this.bulletins.get(String(p.id || ''));
    if (!b) throw rpcError('not_found', 'No such bulletin');
    if (!b.mine) throw rpcError('denied', 'Only your own bulletins can be withdrawn');
    this.bulletins.delete(b.id);
    return {};
  },

  // ---- mesh
  'mesh.peers'() { return { peers: this._peerList() }; },
  'mesh.stats'() {
    let bytes = 0;
    for (const m of this.messages.values()) bytes += utf8Bytes(m.text).length + 96;
    const up = performanceNow();
    return {
      stored_messages: this.messages.size, stored_bytes: bytes, ...this.stats, uptime_ms: Math.round(up),
      per_link: [...this.links.values()].map(l => ({ link_id: l.id, airtime_ms: l.stats.airtime_ms, duty_used_permille: Math.min(1000, Math.round((l.stats.airtime_ms / Math.max(1, up)) * 1000)), queue: l.stats.queue })),
    };
  },
  'mesh.ping'(p) {
    if (!/^[0-9a-f]{16}$/.test(String(p.node_id || ''))) throw rpcError('bad_params', 'node_id must be 16 hex characters');
    const w = worldPeer(String(p.node_id || ''));
    if (!w) return { sent: true }; // keys unknown: still sent, no pong
    const seen = this.peerSeen.get(w.node_id) || 0;
    if (seen > this._now() - 6 * 3600000) {
      const rtt = Math.round(((w.hops ?? 2) + 1) * (w.link === 2 ? 2400 : w.link === 3 ? 1100 : 60) + Math.random() * 300);
      this._after(rtt, () => { this._touchPeer(w); this._emit('mesh.pong', { node_id: w.node_id, rtt_ms: rtt, hops: w.hops }); });
    }
    return { sent: true };
  },
  'mesh.announce'() {
    for (const l of this.links.values()) if (l.state === 'up') this._activity(l.id, 'tx', 64);
    return {};
  },
  'netplan.get'() { return { ...(this.plan ? { plan: this.plan } : {}), heard: this.heardPlans }; },
  'netplan.set'(p) {
    if (!Array.isArray(p.entries)) throw rpcError('bad_params', 'entries must be an array');
    if (p.entries.length > 32) throw rpcError('too_large', 'Too many entries');
    const entries = p.entries.map(e => {
      const f = Number(e && e.freq_hz);
      if (!e || !Number.isFinite(f) || f <= 0) throw rpcError('bad_params', 'Each entry needs freq_hz');
      return { label: String(e.label || ''), freq_hz: Math.round(f), mode: String(e.mode || 'voice'), ...(e.tone ? { tone: String(e.tone) } : {}), ...(e.schedule ? { schedule: String(e.schedule) } : {}), ...(e.notes ? { notes: String(e.notes) } : {}) };
    });
    this.plan = { from: this.nodeId, from_name: this.profile.name, ts: this._now(), entries };
    return { plan: this.plan };
  },

  // ---- links
  'links.types'() { return { types: LINK_TYPES }; },
  'links.list'() {
    for (const l of this.links.values()) this._meterTick(l);
    return { links: [...this.links.values()].map(l => this._linkView(l)) };
  },
  'links.add'(p) {
    const l = this._makeLink({ type: p.type, name: p.name, config: p.config, endpoint: p.endpoint, persist: p.persist !== false });
    this._emit('link.state', { link: l });
    return { link: l };
  },
  'links.configure'(p) {
    const l = this._getLink(p.id);
    if (typeof p.name === 'string' && p.name.trim()) l.name = p.name.trim();
    if (p.config && typeof p.config === 'object') { l.config = linkConfig(l.type, { ...l.config, ...p.config }); l.overrides = { ...(l.overrides || {}), ...clone(p.config) }; l.caps = linkCaps(l.type, l.config); }
    if (p.endpoint !== undefined) {
      if (!p.endpoint || typeof p.endpoint !== 'object' || typeof p.endpoint.kind !== 'string') throw rpcError('bad_params', 'endpoint.kind is required');
      l.endpoint = clone(p.endpoint); // replaced as a whole, same id
    }
    this._emit('link.state', { link: l });
    return { link: l };
  },
  'links.remove'(p) {
    const l = this._getLink(p.id);
    this.links.delete(l.id);
    l.state = 'down';
    this._emit('link.state', { link: { ...l, removed: true } });
    return {};
  },
  'links.endpoint_state'(p) {
    const l = this._getLink(p.id);
    this._setLinkState(l.id, p.connected ? 'up' : (p.error ? 'error' : 'down'), p.error);
    return {};
  },

  // ---- morse receive (RPC_API §6)
  'morse.rx.start'(p) {
    if (!['light', 'audio'].includes(p.source)) throw rpcError('bad_params', 'source must be "light" or "audio"');
    this.morse = this.morse || new Map();
    for (const [id, r] of this.morse) if (Date.now() - r.lastFeed > 60000) this.morse.delete(id);
    if (this.morse.size >= 4) throw rpcError('busy', 'at most 4 Morse sessions');
    const rate = p.sample_rate === undefined ? 8000 : Number(p.sample_rate);
    if (p.source === 'audio' && !(rate >= 4000 && rate <= 48000)) throw rpcError('bad_params', 'sample_rate 4000..48000');
    const session = hex(8);
    this.morse.set(session, new MockMorseRx({ source: p.source, sample_rate: rate, wpm_hint: Number(p.wpm_hint) || 0, tone_hz: Number(p.tone_hz) || 0 }));
    return { session, source: p.source };
  },
  'morse.rx.feed'(p) {
    const r = this._morse(p.session);
    if (r.source === 'light') {
      if (!Array.isArray(p.levels)) throw rpcError('bad_params', 'levels must be an array');
      if (p.levels.length > 4096) throw rpcError('too_large', 'at most 4096 samples per feed');
      r.feedLight(p);
    } else {
      if (typeof p.pcm_b64 !== 'string') throw rpcError('bad_params', 'pcm_b64 is required');
      r.feedAudio(p);
    }
    const st = r.status();
    const text = r.takeNew();
    if (text) this._emit('morse.text', { session: p.session, source: r.source, text, wpm: st.wpm });
    return st;
  },
  'morse.rx.status'(p) { return this._morse(p.session).status(); },
  'morse.rx.reset'(p) { this._morse(p.session).reset(); return {}; },
  'morse.rx.stop'(p) { const r = this._morse(p.session); this.morse.delete(p.session); return { transcript: r.transcript }; },
  'rf.hardware'(p) {
    let list = HARDWARE;
    if (p.method) list = list.filter(m => m.id === p.method || m.link_type === p.method);
    if (p.platform) {
      if (!['android', 'ios', 'web', 'desktop'].includes(p.platform)) throw rpcError('bad_params', 'unknown platform');
      list = list.filter(m => m.platforms.includes(p.platform) || (m.platform_notes || {})[p.platform]);
    }
    return { methods: list };
  },
  'rf.radios'(p) {
    const q = String(p.query || '').trim().toLowerCase();
    return { radios: RADIOS.filter(r => !q || (r.maker + ' ' + r.model + ' ' + r.id).toLowerCase().includes(q)) };
  },

  // ---- guides
  'guides.categories'() {
    const counts = {};
    for (const g of GUIDES) counts[g.category] = (counts[g.category] || 0) + 1;
    return { categories: Object.keys(counts).map(id => ({ id, title: CATEGORY_TITLES[id] || id, count: counts[id] })), total: GUIDES.length };
  },
  'guides.list'(p) {
    let list = GUIDES.filter(g => (!p.category || g.category === p.category) && (!p.part_of || g.part_of === p.part_of));
    list = list.slice().sort((a, b) => (a.part_of || a.id).localeCompare(b.part_of || b.id) || (a.order || 0) - (b.order || 0) || a.title.localeCompare(b.title));
    const offset = Math.max(0, Number(p.offset) || 0), limit = Math.max(1, Math.min(1000, Number(p.limit) || 100));
    return { guides: list.slice(offset, offset + limit).map(guideMeta), total: list.length };
  },
  'guides.get'(p) {
    const g = GUIDES.find(x => x.id === p.id);
    if (!g) throw rpcError('not_found', 'No such guide');
    const out = { ...guideMeta(g), markdown: g.markdown };
    if (g.part_of) {
      const book = GUIDES.filter(x => x.part_of === g.part_of).sort((a, b) => (a.order || 0) - (b.order || 0));
      const i = book.indexOf(g);
      if (i > 0) out.prev = book[i - 1].id;
      if (i < book.length - 1) out.next = book[i + 1].id;
    }
    return out;
  },
  'guides.search'(p) {
    // Keyword ranking as before; with semantic (default on, unless ?nosemantic=1) the mock's
    // meaning ranking decides among weak/zero keyword matches and keyword hits always rank first.
    const terms = String(p.query || '').toLowerCase().split(/[^\p{L}\p{N}]+/u).filter(t => t.length >= 2);
    if (!terms.length) return { results: [] };
    const limit = Math.max(1, Math.min(200, Number(p.limit) || 20));
    const sem = p.semantic !== false && this.semanticOn;
    const results = [];
    for (const g of GUIDES) {
      if (p.category && g.category !== p.category) continue;
      const plain = plainText(g.markdown);
      const lower = plain.toLowerCase(), title = g.title.toLowerCase(), tags = g.tags.join(' ').toLowerCase();
      let score = 0, ok = true;
      for (const t of terms) {
        const body = countOf(lower, t);
        const s = (title.includes(t) ? 10 : 0) + (tags.includes(t) ? 5 : 0) + (g.summary.toLowerCase().includes(t) ? 3 : 0) + Math.min(body, 20);
        if (!s) { ok = false; break; }
        score += s;
      }
      const bp = sem ? bestPassage(g, p.query) : null;
      const meaning = bp && bp.score >= 0.25;
      if (!ok && !meaning) continue;
      const r = { id: g.id, title: g.title, category: g.category, score: Math.round((ok ? 100 + score : bp.score * 100) * 10) / 10, snippet: snippet(plain, terms), terms: ok ? terms : [] };
      if (sem) { r.match = ok ? (meaning ? 'both' : 'words') : 'meaning'; if (bp) r.passage = { text: bp.text, ...(bp.heading ? { heading: bp.heading } : {}) }; }
      results.push(r);
    }
    results.sort((a, b) => b.score - a.score);
    return { results: results.slice(0, limit) };
  },
  'guides.related'(p) {
    if (!GUIDES.some(g => g.id === p.id)) throw rpcError('not_found', 'No such guide');
    if (!this.semanticOn) return { results: [] };
    const limit = Math.max(1, Math.min(50, Number(p.limit) || 8));
    const rel = semRelated(GUIDES, p.id, limit);
    // weak: the mock's own confidence cut-off (bag-of-words scores run lower than the core's)
    return { results: rel.map(x => ({ ...guideMeta(x.g), score: x.score })), weak: !rel.length || rel[0].score < 0.15 };
  },
  'guides.ask'(p) {
    const q = typeof p.question === 'string' ? p.question.trim() : '';
    if (!q) throw rpcError('bad_params', 'question is required');
    if (q.length > 500) throw rpcError('too_large', 'question too long');
    if (!this.semanticOn) return { passages: [] };
    const passages = semAsk(GUIDES, q, { category: p.category, limit: Math.max(1, Math.min(20, Number(p.limit) || 5)) });
    return { passages, weak: !passages.length || passages[0].score < 0.2 };
  },
  'guides.info'() {
    const lic = new Map();
    for (const g of GUIDES) lic.set(g.license, (lic.get(g.license) || 0) + 1);
    return {
      guides: GUIDES.length, categories: new Set(GUIDES.map(g => g.category)).size, bytes: GUIDES.reduce((n, g) => n + g.bytes, 0),
      semantic: this.semanticOn ? { available: true, ...MODEL_INFO, passages: GUIDES.length * 4 } : { available: false },
      licences: [...lic].map(([name, count]) => ({ name, count })).sort((a, b) => b.count - a.count),
    };
  },

  // ---- tools
  'tools.list'() { return { tools: TOOLS }; },
  'tools.run'(p) {
    if (!TOOLS.find(t => t.id === p.id)) throw rpcError('not_found', 'No such tool');
    return runTool(p.id, p.inputs && typeof p.inputs === 'object' ? p.inputs : {});
  },
  'rf.bands'(p) { return { services: bandsFor(p.region, p.service) }; },
  'tools.qr'(p) {
    if (typeof p.text !== 'string' || !p.text) throw rpcError('bad_params', 'text is required');
    return encodeQR(p.text, ['L', 'M', 'Q', 'H'].includes(p.ecc) ? p.ecc : 'M');
  },

  // ---- bundles
  'bundle.export'(p) {
    const scope = p.scope || 'all';
    if (!['all', 'conv', 'for_node'].includes(scope)) throw rpcError('bad_params', 'Unknown scope');
    let msgs = [...this.messages.values()].filter(m => m.kind !== 'system');
    if (scope === 'conv') { this._validConv(p.conv); msgs = msgs.filter(m => m.conv === p.conv); }
    if (scope === 'for_node') { const id = this._need(p, 'node_id'); msgs = msgs.filter(m => m.conv === `dm:${id}` || m.conv === 'ch:public'); }
    if (p.since_ts) msgs = msgs.filter(m => m.ts >= Number(p.since_ts));
    msgs.sort((a, b) => a.ts - b.ts);
    const max = Number(p.max_bytes) || Infinity;
    let packets = msgs.map(m => ({ ...m, status: m.outgoing ? 'queued' : m.status }));
    let json = JSON.stringify({ embermesh_bundle: 1, from: this.nodeId, packets });
    while (utf8Bytes(json).length > max && packets.length) { packets = packets.slice(1); json = JSON.stringify({ embermesh_bundle: 1, from: this.nodeId, packets }); }
    const bytes = utf8Bytes(json);
    return { data_b64: b64FromBytes(bytes), count: packets.length, bytes: bytes.length };
  },
  'bundle.import'(p) {
    const data = this._need(p, 'data_b64');
    let d;
    try { d = JSON.parse(utf8b64(data)); } catch { throw rpcError('bad_params', 'Not an EmberMesh bundle'); }
    if (!d || d.embermesh_bundle !== 1 || !Array.isArray(d.packets)) throw rpcError('bad_params', 'Not an EmberMesh bundle');
    let imported = 0, duplicates = 0, forMe = 0;
    for (const pk of d.packets.slice(0, 5000)) {
      if (!pk || typeof pk.id !== 'string' || typeof pk.conv !== 'string' || typeof pk.text !== 'string') continue;
      if (this.messages.has(pk.id)) { duplicates++; continue; }
      const isCh = pk.conv.startsWith('ch:');
      if (isCh && !this.channels.has(pk.conv.slice(3))) continue;
      const m = { ...pk, outgoing: pk.from === this.nodeId, status: pk.from === this.nodeId ? pk.status : 'received', received_ts: this._now(), via_link: 0 };
      if (pk.conv === `dm:${d.from}` || pk.conv.startsWith('dm:')) forMe++;
      this.messages.set(m.id, m);
      imported++;
      if (!m.outgoing) this._meta(m.conv).unread++;
      this._emit('message.new', { message: m });
    }
    return { imported, duplicates, for_me: forMe, rejected: 0 };
  },

  // ---- settings / storage / security
  'settings.get'() { return { settings: this.settings }; },
  'settings.set'(p) {
    if (!p.settings || typeof p.settings !== 'object' || Array.isArray(p.settings)) throw rpcError('bad_params', 'settings must be an object');
    const bad = Object.entries(validateSettings(p.settings));
    if (bad.length) throw rpcError('bad_params', `settings.${bad[0][0]}: ${bad[0][1]}`); // nothing applied
    if (utf8Bytes(JSON.stringify({ ...this.settings, ...p.settings })).length > 64 * 1024) throw rpcError('too_large', 'settings exceed 64 KiB');
    Object.assign(this.settings, p.settings);
    this._save();
    return { settings: this.settings };
  },
  'kv.get'(p) {
    const ns = this._need(p, 'ns'), key = this._need(p, 'key');
    const v = this.kv[ns] && Object.prototype.hasOwnProperty.call(this.kv[ns], key) ? this.kv[ns][key] : null;
    return { value: v };
  },
  'kv.set'(p) {
    const ns = this._need(p, 'ns'), key = this._need(p, 'key');
    if (p.value === undefined) throw rpcError('bad_params', 'value is required');
    if (utf8Bytes(JSON.stringify(p.value)).length > 256 * 1024) throw rpcError('too_large', 'Value too large');
    if (!this.kv[ns] || typeof this.kv[ns] !== 'object') this.kv[ns] = {};
    this.kv[ns][key] = p.value;
    this._save();
    this._emit('storage.dirty', {});
    return {};
  },
  'kv.delete'(p) {
    const ns = this._need(p, 'ns'), key = this._need(p, 'key');
    if (this.kv[ns]) delete this.kv[ns][key];
    this._save();
    return {};
  },
  'kv.list'(p) {
    const ns = this._need(p, 'ns');
    const prefix = typeof p.prefix === 'string' ? p.prefix : '';
    return { keys: Object.keys(this.kv[ns] || {}).filter(k => k.startsWith(prefix)).sort() };
  },
  'security.status'() { return { locked: this.locked, has_passphrase: !!this.security.passphrase, has_duress: !this.locked && !!this.security.duress }; },
  // (a phantom duress slot after a duress unlock never matches a typed passphrase)
  'security.set_passphrase'(p) {
    if (typeof p.new !== 'string') throw rpcError('bad_params', 'new is required');
    if (this.security.passphrase && p.old !== this.security.passphrase) throw rpcError('denied', 'wrong passphrase');
    if (p.new && p.new === this.security.duress) throw rpcError('bad_params', 'Must differ from the duress passphrase');
    this.security.passphrase = p.new;
    if (!p.new) this.security.duress = '';
    this._save();
    return {};
  },
  'security.unlock'(p) {
    const pass = typeof p.passphrase === 'string' ? p.passphrase : '';
    if (!this.locked) return { unlocked: true };
    if (this.security.duress && pass === this.security.duress) {
      // Duress (RPC_API "Duress unlock"): looks like a normal unlock. Identity and content are
      // destroyed; profile, presentation settings and links are kept; has_duress stays true.
      const keepProfile = { ...this.profile };
      const keepSettings = { ...this.settings };
      const keepLinks = [...this.links.values()];
      const keepNext = this.nextLinkId;
      this.wiped = true;
      this.nodeId = hex(16);
      this.kv = {};
      this._resetWorld(true);
      this.links = new Map(keepLinks.map(l => [l.id, l]));
      this.nextLinkId = keepNext;
      this.profile = keepProfile;
      this.settings = keepSettings;
      this.security = { passphrase: pass, duress: '\u0000phantom' };
      this.locked = false;
      this._save();
      this._emit('node.ready', {});
      return { unlocked: true };
    }
    if (pass !== this.security.passphrase) throw rpcError('denied', 'wrong passphrase');
    this.locked = false;
    this._emit('node.ready', {});
    return { unlocked: true };
  },
  'security.lock'() {
    if (!this.security.passphrase) throw rpcError('bad_params', 'Set a passphrase first');
    this.locked = true;
    this._emit('security.locked', {});
    return {};
  },
  'security.set_duress'(p) {
    if (typeof p.passphrase !== 'string') throw rpcError('bad_params', 'passphrase is required');
    if (!this.security.passphrase) throw rpcError('bad_params', 'Set a storage passphrase first');
    if (p.passphrase && p.passphrase === this.security.passphrase) throw rpcError('bad_params', 'Duress passphrase must differ from the real one');
    this.security.duress = p.passphrase;
    this._save();
    return {};
  },
  'security.wipe'(p) {
    if (p.confirm !== 'ERASE') throw rpcError('bad_params', 'confirm must be "ERASE"');
    this._wipe();
    return {};
  },

  // ---- host.*
  'host.info'() {
    const actions = this.unsupported.has('feature:actions') ? [] : HOST_ACTIONS.map(a => (a.id === 'start_on_boot' ? { ...a, checked: this.startOnBoot } : a));
    return { platform: 'web', app_version: VERSION, backend: 'mock', endpoint_kinds: ENDPOINT_KINDS, features: HOST_FEATURES.filter(f => !this.unsupported.has(`feature:${f}`)), actions };
  },
  'host.app.share'() { return { message: 'Share sheet opened (mock)' }; },
  'host.app.start_on_boot'(p) {
    if (typeof p.enabled !== 'boolean') throw rpcError('bad_params', 'enabled must be a boolean');
    this.startOnBoot = p.enabled;
    return {};
  },
  'host.app.battery_optimization'() { return {}; },
  async 'host.endpoints.scan'(p) {
    const k = ENDPOINT_KINDS.find(e => e.kind === p.kind);
    if (!k) throw rpcError('bad_params', 'Unknown endpoint kind');
    if (!k.available) throw rpcError('unsupported', k.unavailable_reason || 'Not available');
    if (!k.scannable) throw rpcError('unsupported', `${k.title} cannot be scanned`);
    const wait = Math.min(Number(p.timeout_ms) || 1200, 1200);
    await new Promise(r => setTimeout(r, wait * this.speed));
    return { devices: SCAN_RESULTS[p.kind] || [] };
  },
  'host.link.create'(p) {
    const ep = p.endpoint || {};
    const k = ENDPOINT_KINDS.find(e => e.kind === ep.kind);
    if (!k) throw rpcError('bad_params', 'Unknown endpoint kind');
    if (!k.available) throw rpcError('unsupported', k.unavailable_reason || 'Not available');
    const linkSpec = p.link || {};
    const type = linkSpec.type || k.default_link_type;
    if (!k.link_types.includes(type)) throw rpcError('bad_params', `${type} cannot run over ${k.title}`);
    for (const f of k.config) {
      if (f.required && (!ep.config || ep.config[f.key] === undefined || ep.config[f.key] === '')) throw rpcError('bad_params', `${f.label} is required`);
    }
    const l = this._makeLink({ type, name: p.name, config: linkSpec.config, endpoint: { kind: ep.kind, config: ep.config || {} }, state: 'connecting' });
    this._emit('link.state', { link: l });
    if (p.autostart !== false) {
      const failing = JSON.stringify(ep.config || {}).includes('fail');
      this._after(1800, () => {
        if (!this.links.has(l.id)) return;
        if (failing) {
          this._setLinkState(l.id, 'error', 'Could not open endpoint (simulated)');
          this._emit('host.endpoint.state', { link_id: l.id, connected: false, error: 'Could not open endpoint (simulated)' });
        } else {
          this._setLinkState(l.id, 'up');
          this._emit('host.endpoint.state', { link_id: l.id, connected: true });
        }
      });
    } else l.state = 'down';
    return { link: l };
  },
  'host.link.remove'(p) {
    const l = this._getLink(p.id);
    this._emit('host.endpoint.state', { link_id: l.id, connected: false });
    return METHODS['links.remove'].call(this, { id: l.id });
  },
  'host.link.reconnect'(p) {
    const l = this._getLink(p.id);
    this._setLinkState(l.id, 'connecting');
    this._after(1500, () => { this._setLinkState(l.id, 'up'); this._emit('host.endpoint.state', { link_id: l.id, connected: true }); });
    return { link: l };
  },
  'host.hub.start'(p) {
    const port = Number(p.port) || 8437;
    if (port < 1 || port > 65535) throw rpcError('bad_params', 'Bad port');
    if (!this.hub.running) {
      this.hub = { running: true, urls: [`http://192.168.4.1:${port}/`, `http://10.42.0.1:${port}/`], clients: 0 };
      this._hubTimer = this._every(6000, () => {
        if (!this.hub.running) return;
        this.hub.clients = Math.max(0, Math.min(6, this.hub.clients + (Math.random() < 0.7 ? 1 : -1)));
        this._emit('host.hub.clients', { clients: this.hub.clients });
      });
    }
    return { ...this.hub };
  },
  'host.hub.stop'() {
    this._clearInterval(this._hubTimer);
    this.hub = { running: false, urls: [], clients: 0 };
    this._emit('host.hub.clients', { clients: 0 });
    return { ...this.hub };
  },
  'host.hub.status'() { return { ...this.hub }; },
  'host.webrtc.create_offer'(p) {
    const pairing_id = hex(12);
    this.pairings.set(pairing_id, { name: p.name || '' });
    const blob = b64utf8(JSON.stringify({ t: 'offer', id: pairing_id, n: p.name || this.profile.name, sdp: `v=0\r\no=- ${Date.now()} 2 IN IP4 127.0.0.1\r\ns=EmberMesh\r\na=fingerprint:sha-256 ${hex(64)}\r\na=ice-ufrag:${hex(8)}\r\n` }));
    return { pairing_id, offer: `ember:webrtc?o=${blob}` };
  },
  'host.webrtc.accept_offer'(p) {
    const offer = String(p.offer || '').trim();
    if (!offer.startsWith('ember:webrtc?o=')) throw rpcError('bad_params', 'That is not a pairing offer');
    let d;
    try { d = JSON.parse(utf8b64(offer.slice(15))); } catch { throw rpcError('bad_params', 'Offer is corrupt'); }
    const pairing_id = hex(12);
    this.pairings.set(pairing_id, { name: (d && d.n) || 'WebRTC peer', answered: true });
    const answer = `ember:webrtc?a=${b64utf8(JSON.stringify({ t: 'answer', id: d && d.id, sdp: `v=0\r\na=fingerprint:sha-256 ${hex(64)}\r\n` }))}`;
    // The offering side will paste our answer; on this side the link comes up shortly.
    this._after(4000, () => {
      const l = this._makeLink({ type: 'raw', name: `WebRTC: ${(d && d.n) || 'peer'}`, endpoint: { kind: 'webrtc', config: { pairing_id } }, state: 'up' });
      this._emit('link.state', { link: l });
    });
    return { pairing_id, answer };
  },
  'host.webrtc.accept_answer'(p) {
    const pr = this.pairings.get(String(p.pairing_id || ''));
    if (!pr) throw rpcError('not_found', 'Unknown pairing');
    if (!String(p.answer || '').trim().startsWith('ember:webrtc?a=')) throw rpcError('bad_params', 'That is not a pairing answer');
    this.pairings.delete(p.pairing_id);
    const l = this._makeLink({ type: 'raw', name: `WebRTC${pr.name ? ': ' + pr.name : ' peer'}`, endpoint: { kind: 'webrtc', config: { pairing_id: p.pairing_id } }, state: 'connecting' });
    this._emit('link.state', { link: l });
    this._after(1200, () => this._setLinkState(l.id, 'up'));
    return { link: l };
  },
  'host.location.get'() {
    const base = this.ownPosition || { lat: HOME.lat, lon: HOME.lon };
    const j = destination(base.lat, base.lon, Math.random() * 360, Math.random() * 0.01);
    this.ownPosition = { ...j, alt: 52, accuracy: 8 + Math.round(Math.random() * 10), ts: this._now() };
    return { ...this.ownPosition };
  },
  'host.location.watch'(p) {
    this._clearInterval(this._watchTimer);
    this._watchTimer = null;
    if (p.enabled) {
      this._watchTimer = this._every(5000, () => {
        const pos = METHODS['host.location.get'].call(this);
        this._emit('host.location', pos);
      });
    }
    return {};
  },
  'host.qr.scan'() {
    const q = new URLSearchParams({ v: '1', id: '9d2e7f4a1b3c5e60', n: 'Jordan Reyes', cs: 'QX0JRD', k: b64utf8('mock-pub-keys:9d2e7f4a1b3c5e60').replace(/=+$/, '') });
    return { text: `ember:card?${q.toString()}` };
  },
  'host.file.save'(p) {
    const name = this._need(p, 'name'), data = this._need(p, 'data_b64');
    if (typeof document !== 'undefined' && typeof Blob !== 'undefined' && typeof URL !== 'undefined' && URL.createObjectURL) {
      try {
        const blob = new Blob([bytesFromB64(data)], { type: p.mime || 'application/octet-stream' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url; a.download = name.replace(/[\\/]/g, '_'); a.rel = 'noopener'; a.style.display = 'none';
        document.body.appendChild(a); a.click(); a.remove();
        setTimeout(() => URL.revokeObjectURL(url), 10000);
      } catch (e) { throw rpcError('io_error', String(e && e.message || e)); }
    }
    return {};
  },
  'host.file.pick'(p) {
    const accept = String(p.accept || '').toLowerCase();
    if (accept.includes('ember') || accept.includes('bundle')) {
      const peer = worldPeer('d41f8e2b6c7a9035');
      const pk = { id: hex(16), conv: 'ch:public', from: peer.node_id, from_name: peer.name, outgoing: false, ts: this._now() - 3600000, received_ts: 0, text: 'Carried by hand from Ridge Camp: all well, 12 people, need antibiotics.', kind: 'text', status: 'received', priority: 'normal', encrypted: false, signed_ok: true, hops: 1, via_link: 0 };
      const json = JSON.stringify({ embermesh_bundle: 1, from: peer.node_id, packets: [pk] });
      return { name: 'ridge-camp.ember', mime: 'application/x-embermesh-bundle', data_b64: b64utf8(json) };
    }
    if (accept.includes('backup')) {
      return { name: 'embermesh-identity.txt', mime: 'text/plain', data_b64: b64utf8(METHODS['identity.export'].call(this, { passphrase: 'demo' }).backup) };
    }
    if (accept.includes('image')) {
      // 1x1 PNG
      return { name: 'photo.png', mime: 'image/png', data_b64: 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==' };
    }
    return { name: 'field-notes.txt', mime: 'text/plain', data_b64: b64utf8('Field notes\n- Creek level dropping\n- 3 families at the school\n') };
  },
  async 'host.signal.flash'(p) { return signalWait.call(this, p); },
  async 'host.signal.tone'(p) { return signalWait.call(this, p); },
  'host.notify'(p) {
    if (typeof console !== 'undefined') console.info('[mock notify]', String(p.title || ''), String(p.body || ''));
    return {};
  },
  'host.keep_awake'() { return {}; },
};

MockBackend.prototype._sosStatus = function () {
  return { active: this.sos.active, ...(this.sos.since_ts ? { since_ts: this.sos.since_ts } : {}), ...(this.sos.active ? { text: this.sos.text } : {}), sent: this.sos.sent, acked_by: this.sos.acked_by };
};

async function signalWait(p) {
  if (!Array.isArray(p.pattern_ms) || p.pattern_ms.some(v => !Number.isFinite(Number(v)) || v < 0)) throw rpcError('bad_params', 'pattern_ms must be an array of durations');
  const total = Math.min(60000, p.pattern_ms.reduce((a, b) => a + Number(b), 0));
  await new Promise(r => setTimeout(r, total * this.speed));
  return {};
}

function guideMeta(g) {
  const { markdown, ...meta } = g;
  return meta;
}
function plainText(md) {
  return md.replace(/```[\s\S]*?```/g, ' ').replace(/[#>*_`|\-]+/g, ' ').replace(/\[([^\]]*)\]\([^)]*\)/g, '$1').replace(/<[^>]*>/g, ' ').replace(/\s+/g, ' ').trim();
}
function countOf(hay, needle) {
  let n = 0, i = hay.indexOf(needle);
  while (i >= 0) { n++; i = hay.indexOf(needle, i + needle.length); }
  return n;
}
function snippet(plain, terms) {
  const lower = plain.toLowerCase();
  let at = -1;
  for (const t of terms) { const i = lower.indexOf(t); if (i >= 0 && (at < 0 || i < at)) at = i; }
  if (at < 0) return plain.slice(0, 160) + (plain.length > 160 ? '…' : '');
  const start = Math.max(0, at - 60);
  const end = Math.min(plain.length, start + 160);
  return (start > 0 ? '…' : '') + plain.slice(start, end).trim() + (end < plain.length ? '…' : '');
}

export { METHODS as _MOCK_METHODS };
