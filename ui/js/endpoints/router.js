// Web host router: implements the `host.*` RPC methods (RPC_API §3) in a browser, owns the
// endpoints underneath core links, and moves bytes between them and the core.
//
// It is independent of how the core is reached; the WASM backend passes a `core` adapter:
//   core.call(method, params) -> Promise<result>   (rejects with Error{code, message})
//   core.linkRx(link_id, Uint8Array)               bytes from an endpoint
//   core.audioRx(link_id, Int16Array, rate)        captured PCM
//   core.audioTx(link_id, max, rate) -> Int16Array PCM to play (length 0 = idle)
//   core.kick()                                    "input arrived, tick soon"
import websocket from './websocket.js';
import webrtc, { createOffer, acceptOffer, acceptAnswer } from './webrtc.js';
import serial from './serial.js';
import ble from './ble.js';
import audio from './audio.js';
import loopback from './loopback.js';
import * as device from './device.js';

export const DEFAULT_ENDPOINTS = [websocket, webrtc, serial, ble, audio, loopback];

const LEGACY_KEY = 'links_autostart'; // old host-side saved-link list; migrated once, then cleared
const PAIRING_TTL_MS = 15 * 60 * 1000;

export function rpcError(code, message) {
  return Object.assign(new Error(message), { code });
}

function randomId(env, bytes = 6) {
  const b = new Uint8Array(bytes);
  if (env.crypto && env.crypto.getRandomValues) env.crypto.getRandomValues(b);
  else for (let i = 0; i < bytes; i++) b[i] = Math.floor(Math.random() * 256);
  return Array.from(b, (x) => x.toString(16).padStart(2, '0')).join('');
}

/** JSON with sorted object keys (the core may reorder members). */
function stableJson(v) {
  if (Array.isArray(v)) return `[${v.map(stableJson).join(',')}]`;
  if (v && typeof v === 'object') return `{${Object.keys(v).filter((k) => v[k] !== undefined).sort().map((k) => `${JSON.stringify(k)}:${stableJson(v[k])}`).join(',')}}`;
  return JSON.stringify(v === undefined ? null : v);
}

function sameEndpoint(a, b) {
  if (!a || !b || a.kind !== b.kind) return false;
  const ca = a.config || {}, cb = b.config || {};
  const keys = { websocket: ['url'], loopback: ['channel'], serial: ['usb_vendor_id', 'usb_product_id', 'port_index'], ble: ['device_id'], audio: ['input_device', 'output_device'] }[a.kind];
  if (!keys) return false;
  return keys.every((k) => (ca[k] ?? null) === (cb[k] ?? null));
}

export class HostRouter {
  /**
   * @param {object} o
   * @param {object} o.core      core adapter (see top of file)
   * @param {function} o.emit    (event, data) -> deliver an event to the UI
   * @param {object} [o.env]     globals (window); tests pass fakes
   * @param {object} [o.store]   {get(key), set(key, value)} fallback persistence (localStorage)
   * @param {object[]} [o.endpoints] endpoint modules
   * @param {object} [o.info]    extra fields merged into host.info (app_version, backend, ...)
   * @param {boolean} [o.autoReopen] run startup() on every core node.ready (default true)
   */
  constructor({ core, emit, env = globalThis, store = null, endpoints = DEFAULT_ENDPOINTS, info = {}, log = console, autoReopen = true } = {}) {
    this.core = core;
    this.emit = emit || (() => {});
    this.env = env;
    this.store = store;
    this.endpoints = endpoints;
    this.info = { app_version: '0.0.0', backend: 'wasm', ...info };
    this.log = log;
    this.links = new Map();     // link_id -> {endpoint, def, connected}
    this.pairings = new Map();  // pairing_id -> {endpoint, created, name}
    this.hub = null;
    this._locStop = null;
    this._keepAwake = null;
    this._linkTypes = null;
    this._queue = Promise.resolve();
    this._migrated = false;
    this.autoReopen = autoReopen;
    this._installPrompt = null;
    this._a11yWatch();
    if (env.addEventListener && env.document) {
      // PWA install prompt (Chrome/Edge/Android): keep it to offer "Install as app" in host.info.actions
      env.addEventListener('beforeinstallprompt', (e) => { e.preventDefault(); this._installPrompt = e; });
      env.addEventListener('appinstalled', () => { this._installPrompt = null; });
    }
  }

  // --------------------------------------------------------------------------------- dispatch

  async handle(method, params = {}) {
    const p = params || {};
    switch (method) {
      case 'host.info': return this.hostInfo();
      case 'host.endpoints.scan': return this.scan(p);
      case 'host.link.create': return this.linkCreate(p);
      case 'host.link.remove': return this.linkRemove(p);
      case 'host.link.reconnect': return this.linkReconnect(p);
      case 'host.hub.start':
      case 'host.hub.stop':
      case 'host.hub.status':
        throw rpcError('unsupported', 'A browser cannot accept incoming connections. Run the hub on a computer with the desktop app (emberd), then open its address here.');
      case 'host.webrtc.create_offer': return this.webrtcCreateOffer(p);
      case 'host.webrtc.accept_offer': return this.webrtcAcceptOffer(p);
      case 'host.webrtc.accept_answer': return this.webrtcAcceptAnswer(p);
      case 'host.location.get': return this.locationGet();
      case 'host.location.watch': return this.locationWatch(p);
      case 'host.qr.scan': return device.qrScan(this.env);
      case 'host.file.save': return device.fileSave(this.env, p);
      case 'host.file.pick': return device.filePick(this.env, p);
      case 'host.signal.flash': return device.flash(this.env, p);
      case 'host.signal.tone': return device.tone(this.env, p);
      case 'host.signal.stop': return device.signalStop();
      case 'host.notify': return device.notify(this.env, p);
      case 'host.web.download_single_file': return this.downloadSingleFile();
      case 'host.haptic': return device.haptic(this.env, p);
      case 'host.app.offline_status':
        if (!this.offline) throw rpcError('unsupported', 'offline installs are not available here');
        return this.offline.status();
      case 'host.app.make_offline':
        if (!this.offline) throw rpcError('unsupported', 'offline installs are not available here');
        await this.offline.makeOffline();
        return {};
      case 'host.app.check_update':
        if (!this.offline) throw rpcError('unsupported', 'offline installs are not available here');
        return this.offline.checkUpdate({ apply: !!p.apply, force: true }); // the user asked: retry a failed download now
      case 'host.web.install': return this.installApp();
      case 'host.keep_awake': return this.keepAwake().set(p.enabled);
      default:
        throw rpcError('unsupported', `${method} is not available in the browser`);
    }
  }

  // --------------------------------------------------------------------------------- host.info

  async _coreLinkTypes() {
    if (this._linkTypes) return this._linkTypes;
    try {
      const r = await this.core.call('links.types', {});
      if (r && Array.isArray(r.types)) this._linkTypes = r.types;
    } catch { /* stub core or not ready: static lists */ }
    return this._linkTypes;
  }

  async endpointKinds() {
    const types = await this._coreLinkTypes();
    const out = [];
    for (const m of this.endpoints) {
      let d;
      try { d = m.detectAsync ? await m.detectAsync(this.env) : m.detect(this.env); } catch (e) { d = { available: false, reason: String(e.message || e) }; }
      let linkTypes = m.linkTypes.slice();
      if (types) {
        const fromCore = types.filter((t) => Array.isArray(t.endpoint_kinds) && t.endpoint_kinds.includes(m.kind)).map((t) => t.type);
        if (fromCore.length) linkTypes = fromCore;
      }
      const def = linkTypes.includes(m.defaultLinkType) ? m.defaultLinkType : linkTypes[0];
      const k = {
        kind: m.kind, title: m.title, description: m.description,
        available: !!d.available, scannable: !!m.scannable,
        config: m.config(this.env), link_types: linkTypes, default_link_type: def,
        datagram: !!m.datagram, pairing: !!m.pairing, auto_restore: !!m.autoRestore,
      };
      if (!d.available) k.unavailable_reason = d.reason || 'not available in this browser';
      out.push(k);
    }
    return out;
  }

  features() {
    const env = this.env;
    const f = [];
    const has = (kind) => { const m = this.endpoints.find((x) => x.kind === kind); return m && m.detect(env).available; };
    if (has('webrtc')) f.push('webrtc');
    if (device.detectQr(env).available) f.push('qr_scan');
    if (device.detectLocation(env).available) f.push('location');
    if (device.detectFlash(env).available) f.push('flash');
    if (device.detectSignal(env).torch !== 'no') f.push('torch'); // RPC_API §10: "yes" or "ask"
    if (device.detectTone(env).available) f.push('tone');
    if (device.detectFile(env).available) f.push('file');
    if (device.detectNotify(env).available) f.push('notify');
    if (device.detectWakeLock(env).available) f.push('keep_awake');
    if (device.detectHaptic(env).available) f.push('haptic');
    if (device.detectCapture(env, 'camera').available) f.push('camera');
    if (device.detectCapture(env, 'microphone').available) f.push('microphone');
    return f;
  }

  /** Feature -> reason map for everything unavailable (additive host.info field). */
  unavailableFeatures() {
    const env = this.env;
    const out = {};
    const add = (name, d) => { if (!d.available) out[name] = d.reason; };
    add('qr_scan', device.detectQr(env));
    add('location', device.detectLocation(env));
    add('flash', device.detectFlash(env));
    add('tone', device.detectTone(env));
    add('file', device.detectFile(env));
    add('notify', device.detectNotify(env));
    add('keep_awake', device.detectWakeLock(env));
    add('camera', device.detectCapture(env, 'camera'));
    add('microphone', device.detectCapture(env, 'microphone'));
    out.hub = 'A browser cannot accept incoming connections; run the hub on a computer with the desktop app (emberd).';
    return out;
  }

  async hostInfo() {
    return {
      platform: 'web',
      ...this.info,
      endpoint_kinds: await this.endpointKinds(),
      features: this.features(),
      unavailable_features: this.unavailableFeatures(),
      secure_context: this.env.isSecureContext !== false,
      accessibility: this.accessibility(),
      actions: [...(this.offline ? await this.offline.actions().catch(() => []) : []), ...this.actions()],
      audio: await this.audioHints(),
      signal: device.detectSignal(this.env),
      hub: this.hub ? { ...this.hub } : null,
    };
  }

  /** host.info.accessibility from the browser's media queries (RPC_API §8). */
  accessibility() {
    const mq = (q) => { try { return !!(this.env.matchMedia && this.env.matchMedia(q).matches); } catch { return false; } };
    const out = {
      reduce_motion: mq('(prefers-reduced-motion: reduce)'),
      high_contrast: mq('(prefers-contrast: more)') || mq('(forced-colors: active)'),
    };
    if (mq('(forced-colors: active)')) out.forced_colors = true;
    return out;
  }

  /** Emits host.accessibility whenever one of those media queries changes. */
  _a11yWatch() {
    const env = this.env;
    if (!env.matchMedia) return;
    let last = JSON.stringify(this.accessibility());
    const fire = () => {
      const now = this.accessibility();
      const j = JSON.stringify(now);
      if (j === last) return;
      last = j;
      this.emit('host.accessibility', now);
    };
    for (const q of ['(prefers-reduced-motion: reduce)', '(prefers-contrast: more)', '(forced-colors: active)']) {
      try {
        const m = env.matchMedia(q);
        if (m && m.addEventListener) m.addEventListener('change', fire);
        else if (m && m.addListener) m.addListener(fire);
      } catch { /* old browsers */ }
    }
  }

  /** host.info.audio (RPC_API "Host capability hints"): what this browser can do for the radio wizard. */
  async audioHints() {
    const env = this.env;
    const audioMod = this.endpoints.find((m) => m.kind === 'audio');
    const audioOk = audioMod ? audioMod.detect(env) : { available: false, reason: 'no audio endpoint' };
    const serialMod = this.endpoints.find((m) => m.kind === 'serial');
    const serialOk = serialMod ? serialMod.detect(env) : { available: false, reason: 'no serial endpoint' };
    const types = (await this._coreLinkTypes()) || [];
    const a = types.find((t) => t.type === 'audio');
    const pf = a && Array.isArray(a.config) ? a.config.find((f) => f.key === 'preset') : null;
    const presets = pf && Array.isArray(pf.options) ? pf.options.map((o) => o.value) : [];
    const unsupported = {};
    for (const p of presets) {
      if (!audioOk.available) unsupported[p] = audioOk.reason;
      else if (p === 'handheld_ptt' && !serialOk.available) {
        unsupported[p] = 'Keying PTT on a serial line (AIOC, Digirig, USB cable) needs Web Serial — Chrome or Edge on a computer, on a secure page. Use the VOX preset, or press PTT by hand (acoustic).';
      }
    }
    const ptt = [];
    if (audioOk.available) ptt.push('vox', 'manual');
    if (audioOk.available && serialOk.available) ptt.push('serial');
    return {
      ptt, serial_ptt: ptt.includes('serial'),
      presets_supported: presets.filter((p) => !unsupported[p]),
      presets_unsupported: unsupported,
      ...(audioOk.available ? {} : { unavailable_reason: audioOk.reason }),
    };
  }

  keepAwake() {
    if (!this._keepAwake) this._keepAwake = new device.KeepAwake(this.env);
    return this._keepAwake;
  }

  /** Platform actions for Settings -> "This device" (RPC_API §3 "Platform actions"). */
  actions() {
    const out = [];
    const loc = this.env.location;
    if (this._installPrompt) {
      out.push({ id: 'install_app', title: 'Install as app', description: 'Adds EmberMesh to this device so it opens offline like an app.', method: 'host.web.install' });
    }
    if (!this.info.single_file && loc && /^https?:$/.test(loc.protocol) && this.env.document) {
      out.push({ id: 'download_single_file', title: 'Download single-file copy to share', description: 'One HTML file with the whole app; copy it to a USB stick or another phone and open it without a server.', method: 'host.web.download_single_file' });
    }
    return out;
  }

  async downloadSingleFile() {
    const doc = this.env.document;
    if (!doc || !this.env.location) throw rpcError('unsupported', 'no page to download from');
    const url = new URL('../../embermesh.html', import.meta.url).href;
    const r = await this.env.fetch(url, { method: 'HEAD' }).catch(() => null);
    if (!r || !r.ok) throw rpcError('not_found', 'this site does not include the single-file copy (embermesh.html)');
    const a = doc.createElement('a');
    a.href = url; a.download = 'embermesh.html'; a.rel = 'noopener'; a.style.display = 'none';
    doc.body.appendChild(a); a.click(); a.remove();
    return { message: 'Downloading embermesh.html' };
  }

  async installApp() {
    const p = this._installPrompt;
    if (!p) throw rpcError('unsupported', 'this browser does not offer installation right now (already installed, or use the browser menu)');
    this._installPrompt = null;
    this._a11yWatch();
    await p.prompt();
    const choice = p.userChoice ? await p.userChoice : null;
    return { message: choice && choice.outcome === 'accepted' ? 'Installed' : 'Installation cancelled' };
  }

  // --------------------------------------------------------------------------------- endpoints

  _module(kind) {
    const m = this.endpoints.find((x) => x.kind === kind);
    if (!m) throw rpcError('unsupported', `endpoint kind "${kind}" is not available in the browser`);
    const d = m.detect(this.env);
    if (!d.available) throw rpcError('unsupported', d.reason);
    return m;
  }

  async scan({ kind, timeout_ms, ...rest } = {}) {
    const m = this._module(kind);
    if (!m.scannable || !m.scan) throw rpcError('unsupported', `${kind} endpoints cannot be scanned`);
    const work = m.scan(rest, this.env);
    const t = Number(timeout_ms) > 0 ? Number(timeout_ms) : 60000;
    let timer;
    const timeout = new Promise((resolve) => { timer = setTimeout(() => resolve({ devices: [] }), t); });
    try { return await Promise.race([work, timeout]); } finally { clearTimeout(timer); }
  }

  _ctxFor(holder) {
    return {
      env: this.env,
      log: this.log,
      audioRx: (pcm, rate) => { if (holder.id) { this.core.audioRx(holder.id, pcm, rate); this.core.kick(); } },
      audioTx: (max, rate) => (holder.id ? this.core.audioTx(holder.id, max, rate) : null),
    };
  }

  _attach(linkId, endpoint, def, holder) {
    if (holder) holder.id = linkId;
    const prev = this.links.get(linkId);
    if (prev && prev.endpoint !== endpoint) prev.endpoint.close().catch(() => {});
    const entry = { endpoint, def, connected: null, error: undefined };
    this.links.set(linkId, entry);
    endpoint.onData((bytes) => {
      if (this.links.get(linkId) !== entry) return;
      this.core.linkRx(linkId, bytes);
      this.core.kick();
    });
    endpoint.onState((state, error) => {
      if (this.links.get(linkId) !== entry) return;
      const connected = state === 'up';
      if (entry.connected === connected && entry.error === error) return;
      entry.connected = connected;
      entry.error = error;
      const p = { id: linkId, connected };
      if (error) p.error = error;
      Promise.resolve(this.core.call('links.endpoint_state', p)).catch(() => {});
      this.core.kick();
      const ev = { link_id: linkId, connected, state };
      if (error) ev.error = error;
      this.emit('host.endpoint.state', ev);
    });
    return entry;
  }

  async _openAttached(linkId, endpoint, { rethrow = false } = {}) {
    try {
      await endpoint.open();
    } catch (e) {
      // The link exists; report the endpoint failure through state (UI can reconnect).
      if (endpoint.state !== 'error') endpoint._setState('error', String((e && e.message) || e));
      if (rethrow) throw e;
    }
  }

  /** Bytes the core wants written to a link's endpoint (EMBER_OUT_LINK_TX). */
  onLinkTx(linkId, bytes) {
    const e = this.links.get(linkId);
    if (!e) return false;
    return e.endpoint.send(bytes);
  }

  /** Core events the host must act on (called for every core event, before the UI sees it). */
  onCoreEvent(event, data) {
    if (event === 'link.ptt' && data) {
      const e = this.links.get(Number(data.link_id));
      if (e) Promise.resolve(e.endpoint.setPtt(!!data.on)).catch(() => {});
    } else if (event === 'node.ready') {
      // RPC_API "Link persistence": re-run the startup step on every node.ready (create, unlock,
      // duress unlock, wipe). Never prompts for devices.
      if (this.autoReopen) this.startup().catch((e) => this.log.warn('[router] reopening links failed:', e && e.message));
    } else if (event === 'link.state' && data && data.link && data.link.removed) {
      // removed behind our back (links.remove called directly): release the endpoint
      const id = Number(data.link.id);
      const e = this.links.get(id);
      if (e) { this.links.delete(id); e.endpoint.close().catch(() => {}); }
    } else if (event === 'link.state' && data && data.link) {
      this._onLinkState(data.link);
    }
  }

  async _coreLinks() {
    try {
      const r = await this.core.call('links.list', {});
      return (r && r.links) || [];
    } catch { return []; }
  }

  /** Runs fn after every earlier startup/hub step finished (they touch the same links). */
  _serial(fn) {
    const run = this._queue.then(fn, fn);
    this._queue = run.catch(() => {});
    return run;
  }

  async _addLink(def, persist) {
    const r = await this.core.call('links.add', {
      type: def.link.type, name: def.name, config: def.link.config || {}, endpoint: def.endpoint, persist: !!persist,
    });
    if (!r || !r.link || !(r.link.id > 0)) throw rpcError('internal', 'links.add returned no link');
    return r.link;
  }

  async linkCreate({ name, endpoint, link, autostart = true } = {}) {
    if (!endpoint || !endpoint.kind) throw rpcError('bad_params', 'endpoint.kind is required');
    const m = this._module(endpoint.kind);
    if (m.pairing) throw rpcError('bad_params', 'WebRTC links are paired with host.webrtc.create_offer / accept_offer');
    const type = (link && link.type) || m.defaultLinkType;
    const holder = { id: 0 };
    const ep = m.create(endpoint.config || {}, this._ctxFor(holder));
    await ep.prepare({ interactive: true }); // device pickers need the user's click: do this first
    const def = { name: name || ep.config.device_name || m.title, endpoint: ep.describe(), link: { type, config: (link && link.config) || {} } };
    let l;
    try {
      // The core persists the definition (incl. the opaque endpoint descriptor) and reopening it
      // at startup is driven by links.list; autostart:false (or a kind we cannot reopen) = session only.
      l = await this._addLink(def, autostart !== false && m.autoRestore);
    } catch (e) {
      ep.close().catch(() => {});
      throw e;
    }
    this._attach(l.id, ep, def, holder);
    try {
      await this._openAttached(l.id, ep, { rethrow: true });
    } catch (e) {
      // Permission refused / device never answered: no half-open link is left behind.
      if (e && (e.code === 'denied' || e.code === 'io_error')) {
        this.links.delete(l.id);
        await ep.close().catch(() => {});
        await Promise.resolve(this.core.call('links.remove', { id: l.id })).catch(() => {});
        throw e;
      }
    }
    return { link: await this._linkById(l.id, l) };
  }

  async _linkById(id, fallback) {
    const list = await this._coreLinks();
    return list.find((x) => x.id === id) || fallback;
  }

  async linkRemove({ id } = {}) {
    const linkId = Number(id);
    if (!(linkId > 0)) throw rpcError('bad_params', 'id is required');
    const e = this.links.get(linkId);
    this.links.delete(linkId);
    if (e) await e.endpoint.close().catch(() => {});
    if (this.hub && this.hub.link_id === linkId) this.hub.link_id = 0;
    await this.core.call('links.remove', { id: linkId });
    return {};
  }

  /**
   * Reopens a link's endpoint. The definition always comes from the core (links.list), never from a
   * cached copy: links.configure may have replaced the endpoint descriptor (same id) since it opened.
   */
  async linkReconnect({ id } = {}) {
    const linkId = Number(id);
    if (!(linkId > 0)) throw rpcError('bad_params', 'id is required');
    return this._serial(() => this._reopen(linkId, { interactive: true }));
  }

  async _reopen(linkId, { interactive }) {
    const l = (await this._coreLinks()).find((x) => x.id === linkId);
    if (!l) throw rpcError('not_found', `no link ${linkId}`);
    if (!l.endpoint || !l.endpoint.kind) throw rpcError('bad_params', 'link has no endpoint to reconnect');
    const m = this._module(l.endpoint.kind);
    if (m.pairing) throw rpcError('unsupported', 'WebRTC links cannot reconnect; pair again');
    const cur = this.links.get(linkId);
    if (cur) { this.links.delete(linkId); await cur.endpoint.close().catch(() => {}); }
    const holder = { id: linkId };
    const ep = m.create(l.endpoint.config || {}, this._ctxFor(holder));
    const def = { name: l.name, endpoint: l.endpoint, link: { type: l.type, config: l.config || {} } };
    try {
      await ep.prepare({ interactive });
    } catch (e) {
      ep.close().catch(() => {});
      const msg = String((e && e.message) || e);
      await this._reportEndpoint(linkId, false, msg);
      this.emit('host.endpoint.state', { link_id: linkId, connected: false, state: 'error', error: msg });
      throw e;
    }
    // Device ids learnt while preparing (picker) go back into the core's descriptor.
    const learnt = ep.describe();
    if (l.persist !== false && stableJson(learnt) !== stableJson(l.endpoint)) {
      def.endpoint = learnt;
      await Promise.resolve(this.core.call('links.configure', { id: linkId, endpoint: learnt })).catch(() => {});
    }
    this._attach(linkId, ep, def, holder);
    await this._openAttached(linkId, ep);
    return { link: await this._linkById(linkId, { id: linkId }) };
  }

  /** link.state: if the core's endpoint descriptor of an open link changed (links.configure), reopen it. */
  _onLinkState(link) {
    const id = Number(link.id);
    const e = this.links.get(id);
    if (!e || !link.endpoint || !link.endpoint.kind) return;
    if (stableJson(link.endpoint) === stableJson(e.def.endpoint)) return;
    e.def = { ...e.def, endpoint: link.endpoint }; // do not react twice to the same change
    this._serial(() => this._reopen(id, { interactive: false }))
      .catch((err) => this.log.warn('[router] reopening link', id, 'after its endpoint changed failed:', err && err.message));
  }

  // --------------------------------------------------------------------------------- startup

  _reportEndpoint(id, connected, error) {
    const p = { id, connected };
    if (error) p.error = error;
    return Promise.resolve(this.core.call('links.endpoint_state', p)).catch(() => {});
  }

  /**
   * The host's startup step (RPC_API "Link persistence (normative)"): links.list -> reopen every
   * endpoint we can provide from link.endpoint -> links.endpoint_state. The core is the only list of
   * saved links. Runs on every node.ready; safe to run again (attached links are kept).
   * Returns [{link_id, ok, error?}] for the links it looked at.
   */
  startup() {
    return this._serial(async () => {
      await this._migrateLegacy();
      const list = await this._coreLinks();
      const ids = new Set(list.map((l) => l.id));
      for (const [id, e] of [...this.links]) {
        if (!ids.has(id)) { this.links.delete(id); e.endpoint.close().catch(() => {}); } // wiped / removed
      }
      const results = [];
      for (const l of list) {
        const kind = l.endpoint && l.endpoint.kind;
        if (!kind) continue; // added without an endpoint (tests, other tools): nothing to open
        const cur = this.links.get(l.id);
        if (cur) {
          // still attached (runtime unlock): make sure the core knows the endpoint state
          if (cur.connected !== null) await this._reportEndpoint(l.id, cur.connected, cur.error);
          results.push({ link_id: l.id, ok: true, kept: true });
          continue;
        }
        const m = this.endpoints.find((x) => x.kind === kind);
        let why = null;
        if (!m) why = `endpoint kind "${kind}" is not available in the browser`;
        else if (!m.autoRestore) why = m.pairing ? 'WebRTC links need a new pairing after a restart' : `${m.title} cannot be reopened automatically`;
        else { const d = m.detect(this.env); if (!d.available) why = d.reason || 'not available in this browser'; }
        if (why) {
          await this._reportEndpoint(l.id, false, why);
          results.push({ link_id: l.id, ok: false, error: why });
          continue;
        }
        const holder = { id: l.id };
        const ep = m.create(l.endpoint.config || {}, this._ctxFor(holder));
        const def = { name: l.name, endpoint: l.endpoint, link: { type: l.type, config: l.config || {} } };
        try {
          await ep.prepare({ interactive: false });
          this._attach(l.id, ep, def, holder);
          await this._openAttached(l.id, ep);
          results.push({ link_id: l.id, ok: true });
        } catch (e) {
          ep.close().catch(() => {});
          const msg = String((e && e.message) || e);
          await this._reportEndpoint(l.id, false, msg);
          this.emit('host.endpoint.state', { link_id: l.id, connected: false, state: 'error', error: msg });
          results.push({ link_id: l.id, ok: false, error: msg });
        }
      }
      await this._ensureHub();
      return results;
    });
  }

  /** Back-compat name used by older callers/tests. */
  restoreLinks() { return this.startup(); }

  /**
   * One-time migration from the old host-side list (settings.links_autostart, or localStorage when
   * the core had no settings): definitions the core does not have yet become persisted core links,
   * then the host list is cleared. Hosts keep no saved-link list any more.
   */
  async _migrateLegacy() {
    if (this._migrated) return;
    let defs = [];
    let inSettings = false;
    try {
      const r = await this.core.call('settings.get', {});
      const v = r && r.settings && r.settings[LEGACY_KEY];
      if (Array.isArray(v) && v.length) { defs = v; inSettings = true; }
    } catch (e) {
      if (e && e.code === 'locked') return; // try again after unlock
    }
    const stored = this.store && this.store.get(LEGACY_KEY);
    if (Array.isArray(stored) && stored.length) defs = defs.concat(stored);
    this._migrated = true;
    if (!defs.length) return;
    const existing = await this._coreLinks();
    for (const d of defs) {
      if (!d || !d.endpoint || !d.endpoint.kind || !d.link || !d.link.type) continue;
      if (existing.some((l) => (d.link_id && l.id === d.link_id) || (l.type === d.link.type && sameEndpoint(l.endpoint, d.endpoint)))) continue;
      try { existing.push(await this._addLink({ name: d.name, endpoint: d.endpoint, link: d.link }, true)); } catch (e) { this.log.warn('[router] could not migrate saved link', d.name, e && e.message); }
    }
    if (inSettings) await Promise.resolve(this.core.call('settings.set', { settings: { [LEGACY_KEY]: [] } })).catch(() => {});
    if (stored && this.store) this.store.set(LEGACY_KEY, null);
  }

  // --------------------------------------------------------------------------------- hub guest

  /**
   * Joins the hub that served this page (RPC_API §5 "Hub mode for guests"). The link is added with
   * persist:false: it is re-detected on every load (and re-added after unlock / wipe).
   */
  async connectHub({ url, name } = {}) {
    if (!url) throw rpcError('bad_params', 'hub url required');
    this.hub = { url, name: name || '', connected: false, link_id: 0 };
    return this._serial(() => this._ensureHub());
  }

  async _ensureHub() {
    const hub = this.hub;
    if (!hub) return null;
    if (hub.link_id && this.links.has(hub.link_id)) return { link: await this._linkById(hub.link_id, { id: hub.link_id }) };
    const def = { name: hub.name ? `Hub ${hub.name}` : 'Hub', endpoint: { kind: 'websocket', config: { url: hub.url, reconnect: true, hub: true } }, link: { type: 'raw', config: {} } };
    const list = await this._coreLinks();
    let l = list.find((x) => x.type === 'raw' && sameEndpoint(x.endpoint, def.endpoint) && this.links.has(x.id));
    if (l) { hub.link_id = l.id; return { link: l }; } // a saved websocket link to the same hub, already open
    l = list.find((x) => x.type === 'raw' && sameEndpoint(x.endpoint, def.endpoint));
    if (!l) {
      try { l = await this._addLink(def, false); } catch (e) {
        if (e && e.code === 'locked') return null; // node.ready after unlock runs this again
        throw e;
      }
    }
    hub.link_id = l.id;
    const holder = { id: l.id };
    const ep = websocket.create(def.endpoint.config, this._ctxFor(holder));
    ep.onState((s) => { if (this.hub === hub) hub.connected = s === 'up'; });
    this._attach(l.id, ep, def, holder);
    await this._openAttached(l.id, ep);
    return { link: l };
  }

  // --------------------------------------------------------------------------------- WebRTC

  _sweepPairings() {
    const now = Date.now();
    for (const [id, p] of this.pairings) {
      if (now - p.created > PAIRING_TTL_MS) { p.endpoint.close().catch(() => {}); this.pairings.delete(id); }
    }
  }

  async webrtcCreateOffer({ name, stun } = {}) {
    this._module('webrtc');
    this._sweepPairings();
    const { endpoint, offer } = await createOffer({ stun, env: this.env, ctx: { log: this.log } });
    const pairing_id = randomId(this.env);
    this.pairings.set(pairing_id, { endpoint, created: Date.now(), name: name || '' });
    return { pairing_id, offer };
  }

  async _webrtcLink(endpoint, name, role) {
    const def = { name: name || 'WebRTC peer', endpoint: { kind: 'webrtc', config: { role } }, link: { type: 'raw', config: {} } };
    let l;
    try { l = await this._addLink(def, false); } catch (e) { endpoint.close().catch(() => {}); throw e; }
    def.link_id = l.id;
    this._attach(l.id, endpoint, def, null);
    // the endpoint may already be up (fast LAN): replay its state through the new listener
    const s = endpoint.state; endpoint.state = 'down'; endpoint._setState(s, endpoint.error);
    await this._openAttached(l.id, endpoint);
    return l;
  }

  async webrtcAcceptOffer({ offer, name, stun } = {}) {
    this._module('webrtc');
    if (!offer) throw rpcError('bad_params', 'offer is required');
    const { endpoint, answer } = await acceptOffer(offer, { stun, env: this.env, ctx: { log: this.log } });
    const pairing_id = randomId(this.env);
    const link = await this._webrtcLink(endpoint, name, 'answer');
    return { pairing_id, answer, link };
  }

  async webrtcAcceptAnswer({ pairing_id, answer } = {}) {
    const p = this.pairings.get(pairing_id);
    if (!p) throw rpcError('not_found', 'unknown or expired pairing; create a new offer');
    if (!answer) throw rpcError('bad_params', 'answer is required');
    await acceptAnswer(p.endpoint, answer);
    this.pairings.delete(pairing_id);
    const link = await this._webrtcLink(p.endpoint, p.name, 'offer');
    return { link };
  }

  // --------------------------------------------------------------------------------- location

  async locationGet() {
    const pos = await device.locationGet(this.env);
    Promise.resolve(this.core.call('position.set', pos)).catch(() => {});
    return pos;
  }

  async locationWatch({ enabled } = {}) {
    if (this._locStop) { this._locStop(); this._locStop = null; }
    if (enabled) {
      this._locStop = device.locationWatch(this.env, (pos) => {
        Promise.resolve(this.core.call('position.set', pos)).catch(() => {});
        this.emit('host.location', pos);
      }, (e) => this.log.warn('[location]', e && e.message));
    }
    return {};
  }

  // --------------------------------------------------------------------------------- shutdown

  async close() {
    if (this._locStop) { this._locStop(); this._locStop = null; }
    const all = [...this.links.values()].map((e) => e.endpoint.close().catch(() => {}));
    for (const p of this.pairings.values()) all.push(p.endpoint.close().catch(() => {}));
    this.links.clear();
    this.pairings.clear();
    await Promise.all(all);
  }
}
