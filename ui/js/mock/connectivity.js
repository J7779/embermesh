// Mock of RPC_API §11 revision 2 (Nearby devices and the internet relay; docs/CONNECTIVITY.md) for
// the mock backend: connectivity.get/set/network_changed, gating by `ready` (setup done and
// unlocked), notices produced by the "core", the default relay list with notes, automatic nearby
// links (two neighbours on this Wi-Fi, named once their identity is proven) and one relay link per
// enabled relay, link.removed with the revision-2 reasons, Contact.reach and relay sessions.
// URL switches: ?nearby=0, ?internet=1 (consent given), ?relay=fail (relays fail to connect),
// ?paused=1 (a relay pauses topics), ?upgrade=1 (an existing install that has not seen the
// disclosure: setup_done false), ?fresh=1 (first run: setup_done false),
// ?unsupported=connectivity.get (older core), ?unsupported=feature:nearby,feature:relay (host).

import { isLanHost } from '../lib/connectivity.js';

export const DEFAULT_RELAYS = [
  { url: 'wss://broker.hivemq.com:8884/mqtt', enabled: true, note: 'HiveMQ public broker: a free service with no guarantee. It does not let other users watch topics.' },
  { url: 'wss://broker.emqx.io:8084/mqtt', enabled: false, note: 'EMQX public broker: a free service with no guarantee. Off by default: anyone connected to it can watch topic names, message sizes and timing.' },
  { url: 'wss://test.mosquitto.org:8081/mqtt', enabled: false, note: 'Mosquitto test server: a free service with no guarantee. Off by default: anyone connected to it can watch topic names, message sizes and timing.' },
];
export const NEARBY_NOTICE = 'Find other EmberMesh devices on this Wi-Fi or hotspot and connect to them automatically. Devices nearby can see your name and your EmberMesh identity.';
/** The core's internet notice names the enabled relays. */
export function internetNotice(relays) {
  const hosts = relays.filter(r => r.enabled).map(r => hostOf(r.url));
  return 'Send messages over the internet when no other link reaches your contacts. They travel through ' +
    (hosts.length ? 'a relay server (' + hosts.join(', ') + ')' : 'a relay server you choose') +
    ', which cannot read them but can see your IP address, when you are online and which other users you exchange messages with.';
}

// Neighbours on this Wi-Fi when Nearby is active, and the far contact reachable through a relay.
export const NEARBY_NEIGHBOURS = ['b7e4c1d2a9f03e56', 'e8a2b5c4d3f10967'];
export const RELAY_CONTACT = '1d6e3f8a2c9b4071';
export const SESSION_BASE = 0x40000000;
const MAX_RELAYS = 8;
const RADIO_TYPES = new Set(['kiss', 'audio', 'meshtastic', 'rnode', 'lora_at']);

const err = (code, message) => Object.assign(new Error(message), { code });
const hostOf = url => { try { return new URL(url).host; } catch { return url; } };

export { isLanHost };

/** Validates a relays list for connectivity.set (RPC_API §11 rev 2). Pure; exported for tests. */
export function checkRelays(relays) {
  if (!Array.isArray(relays)) throw err('bad_params', 'relays must be an array');
  if (relays.length > MAX_RELAYS) throw err('bad_params', 'at most 8 relays');
  const seen = new Set();
  return relays.map(r => {
    if (!r || typeof r.url !== 'string') throw err('bad_params', 'every relay needs a url');
    const url = r.url.trim();
    let u;
    try { u = new URL(url); } catch { throw err('bad_params', 'not a valid URL: ' + url); }
    if (u.protocol === 'ws:' && !(r.allow_plaintext === true && isLanHost(u.hostname))) throw err('bad_params', 'ws:// only with allow_plaintext and a LAN, link-local or .local host: ' + url);
    if (u.protocol !== 'wss:' && u.protocol !== 'ws:') throw err('bad_params', 'relay URLs must be wss://host[:port][/path]: ' + url);
    if (seen.has(url)) throw err('bad_params', 'duplicate relay ' + url);
    seen.add(url);
    const def = DEFAULT_RELAYS.find(d => d.url === url);
    return { url, enabled: r.enabled !== false, ...(r.allow_plaintext === true ? { allow_plaintext: true } : {}), ...(typeof r.username === 'string' && r.username ? { username: r.username } : {}), ...(def ? { note: def.note } : {}) };
  });
}

export function initConnectivity(b, getParam, params) {
  const fresh = getParam(params, 'fresh') === '1';
  b.conn = {
    setup_done: !fresh && getParam(params, 'upgrade') !== '1',
    nearby: getParam(params, 'nearby') !== '0',
    internet: getParam(params, 'internet') === '1',
    consent_ts: getParam(params, 'internet') === '1' ? Date.now() - 86400e3 : null,
    relays: DEFAULT_RELAYS.map(r => ({ ...r })),
    token: newToken(),
    failRelays: getParam(params, 'relay') === 'fail',
    paused: getParam(params, 'paused') === '1',
    relayState: new Map(), // url -> {state, error?, link_id?}
    // switches given in the URL win over the mock's saved state (so every state can be opened directly)
    fromUrl: new Set(['nearby', 'internet', 'upgrade', 'fresh'].filter(k => getParam(params, k) != null)),
  };
}
const newToken = () => [...Array(8)].map(() => '0123456789abcdef'[Math.floor(Math.random() * 16)]).join('');

/** Restores the user's choices from the mock's saved state. */
export function loadConnectivity(b, d) {
  if (!d || typeof d !== 'object') return;
  const u = b.conn.fromUrl;
  for (const k of ['nearby', 'internet']) if (typeof d[k] === 'boolean' && !u.has(k)) b.conn[k] = d[k];
  if (typeof d.setup_done === 'boolean' && !u.has('upgrade') && !u.has('fresh')) b.conn.setup_done = d.setup_done;
  if (Number.isFinite(d.consent_ts) && !u.has('internet')) b.conn.consent_ts = d.consent_ts;
  if (Array.isArray(d.relays)) { try { b.conn.relays = checkRelays(d.relays); } catch { /* keep defaults */ } }
}
export const saveConnectivity = b => ({ setup_done: b.conn.setup_done, nearby: b.conn.nearby, internet: b.conn.internet, consent_ts: b.conn.consent_ts, relays: b.conn.relays });

const isReady = b => b.conn.setup_done && !b.locked;

export function connectivityView(b) {
  const c = b.conn;
  const ready = isReady(b);
  const nearbyUp = [...b.links.values()].filter(l => l.auto === 'nearby' && l.state === 'up').length;
  return {
    ready, setup_done: c.setup_done,
    nearby: { enabled: c.nearby, active: c.nearby && ready, token: c.token, notice: NEARBY_NOTICE, links: nearbyUp },
    internet: {
      enabled: c.internet, active: c.internet && ready, ...(c.consent_ts ? { consent_ts: c.consent_ts } : {}), notice: internetNotice(c.relays),
      relays: c.relays.map(r => {
        const st = c.relayState.get(r.url);
        if (!(c.internet && r.enabled)) return { ...r, state: 'off' };
        return { ...r, ...(st ? { state: st.state, ...(st.error ? { error: st.error } : {}), ...(st.link_id ? { link_id: st.link_id } : {}) } : { state: 'down' }) };
      }),
    },
    available_link_types: ['stream', 'relay'],
  };
}

function emitChanged(b) { b._emit('connectivity.changed', connectivityView(b)); }

function removeLink(b, l, reason) {
  b.links.delete(l.id);
  l.state = 'down';
  b._emit('link.state', { link: { ...l, removed: true } });
  b._emit('link.removed', { id: l.id, reason });
}

/** Brings the automatic links in line with the switches and `ready` (what core + host do together). */
export function syncAutoLinks(b, why) {
  if (b.unsupported.has('connectivity.get')) return; // an older core: no automatic links at all
  const c = b.conn;
  const ready = isReady(b);
  const autos = [...b.links.values()].filter(l => l.auto);
  // nearby: one stream link per neighbour while active
  const nearby = autos.filter(l => l.auto === 'nearby');
  if (!(c.nearby && ready)) for (const l of nearby) removeLink(b, l, why || 'nearby_off');
  else if (!nearby.length) {
    NEARBY_NEIGHBOURS.forEach((nid, i) => {
      const addr = '192.0.2.' + (20 + i);
      const l = b._makeLink({ type: 'stream', name: 'Nearby device', config: { preset: 'nearby' }, endpoint: { kind: 'tcp', config: { host: addr, port: 47475 } }, state: 'connecting', persist: false });
      Object.assign(l, { auto: 'nearby', auto_role: i ? 'accept' : 'dial', auto_key: i ? '@' + addr : 'a1b2c3d4@' + addr, persist: false });
      b._emit('link.state', { link: l });
      b._after(600 + 300 * i, () => {
        if (!b.links.has(l.id)) return;
        b._setLinkState(l.id, 'up');
        emitChanged(b);
        // identity proven a moment later: the core names the link after the device
        b._after(500, () => { if (b.links.has(l.id)) { l.neighbour = nid; const w = b.contacts.get(nid); l.name = 'Nearby: ' + ((w && w.name) || 'device'); b._emit('link.state', { link: l }); } });
      });
    });
  }
  // relays: one link per enabled relay while active (never before consent and setup)
  const relayLinks = autos.filter(l => l.auto === 'relay');
  const want = new Map(c.internet && ready ? c.relays.filter(r => r.enabled).map(r => [r.url, r]) : []);
  for (const l of relayLinks) {
    if (!want.has(l.auto_key)) { removeLink(b, l, why || (c.internet ? 'relay_removed' : 'relay_off')); c.relayState.delete(l.auto_key); } else want.delete(l.auto_key);
  }
  for (const [url, r] of want) {
    const l = b._makeLink({ type: 'relay', name: 'Internet relay (' + hostOf(url) + ')', config: {}, endpoint: { kind: 'websocket', config: { url, protocols: ['mqtt'], ...(r.allow_plaintext ? { allow_plaintext: true } : {}) } }, state: 'connecting', persist: false });
    Object.assign(l, { auto: 'relay', auto_key: url, persist: false, config: { url, keepalive_s: 240 }, caps: { mtu: 0, broadcast: false, half_duplex: false, reliable: false, encryption_allowed: true, requires_station_id: false, cost: 30, bitrate_bps: 0 } });
    l.status = { ...(l.status || {}), mqtt_state: 'connecting', sessions: 0, session_link_ids: [], topics: 0, topics_paused: 0, topics_refused: 0, breaker_trips: 0, reconnects: 0 };
    c.relayState.set(url, { state: 'connecting', link_id: l.id });
    b._emit('link.state', { link: l });
    b._after(900, () => {
      if (!b.links.has(l.id)) return;
      if (c.failRelays) {
        const error = 'Could not connect to ' + hostOf(url) + ': the server did not answer (check the internet connection)';
        c.relayState.set(url, { state: 'error', error, link_id: l.id });
        l.status = { ...l.status, mqtt_state: 'disconnected' };
        b._setLinkState(l.id, 'error', error);
      } else {
        c.relayState.set(url, { state: 'up', link_id: l.id });
        l.status = { ...l.status, mqtt_state: 'up', sessions: 6, session_link_ids: [SESSION_BASE + 1, SESSION_BASE + 2], topics: 18, topics_paused: c.paused ? 2 : 0, breaker_trips: c.paused ? 1 : 0 };
        b._setLinkState(l.id, 'up');
      }
      emitChanged(b);
    });
  }
}

/** Peers heard through the automatic links (used by _peerList): node_id → link id (or session id). */
export function autoPeerLinks(b) {
  const out = new Map();
  for (const l of b.links.values()) {
    if (l.state !== 'up') continue;
    if (l.auto === 'nearby' && l.neighbour) out.set(l.neighbour, l.id);
    if (l.auto === 'relay' && !out.has(RELAY_CONTACT)) out.set(RELAY_CONTACT, SESSION_BASE + 1); // a relay session link
  }
  return out;
}

/** Contact.reach (§11 rev 2) from the mock world. */
export function contactReach(b, nodeId, peer) {
  const now = b._now();
  const heard = peer ? Math.max(0, Math.round((now - peer.last_seen_ts) / 1000)) : null;
  const base = heard != null ? { heard_s: heard } : {};
  const id = peer && peer.links && peer.links[0] && peer.links[0].link_id;
  if (!peer || heard > 900) return { path: 'none', ...base };
  if (id >= SESSION_BASE) return { path: 'relay', link_id: id, ...base };
  if (!(peer.hops === 0 || peer.direct)) return { path: 'none', ...base };
  const l = b.links.get(id);
  if (l && l.auto === 'nearby') return { path: 'nearby', link_id: id, ...base };
  if (l && RADIO_TYPES.has(l.type)) return { path: 'radio', link_id: id, ...base };
  return { path: 'network', ...(id ? { link_id: id } : {}), ...base };
}

export const CONNECTIVITY_METHODS = {
  'connectivity.get'() { return connectivityView(this); },
  'connectivity.set'(p) {
    const c = this.conn;
    if (p.relay_bridge_all !== undefined || p.bridge_all !== undefined) throw err('bad_params', 'there is no bridge-all option');
    if (p.relays !== undefined) {
      c.relays = checkRelays(p.relays);
      for (const url of [...c.relayState.keys()]) if (!c.relays.some(r => r.url === url && r.enabled)) c.relayState.delete(url);
    }
    for (const k of ['nearby', 'internet']) if (p[k] !== undefined && typeof p[k] !== 'boolean') throw err('bad_params', k + ' must be true or false');
    if (p.setup_done !== undefined && p.setup_done !== true) throw err('bad_params', 'setup_done can only be set to true');
    if (p.setup_done === true) c.setup_done = true;
    if (typeof p.nearby === 'boolean') c.nearby = p.nearby;
    if (typeof p.internet === 'boolean') {
      if (p.internet && !c.internet) c.consent_ts = this._now();
      c.internet = p.internet;
    }
    syncAutoLinks(this);
    this._save();
    emitChanged(this);
    return connectivityView(this);
  },
  'connectivity.network_changed'() { this.conn.token = newToken(); emitChanged(this); return { token: this.conn.token }; },
};
