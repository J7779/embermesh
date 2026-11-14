// WasmBackend — runs the shared C++ core (compiled to WebAssembly) inside this page, and is the
// web host router: `host.*` methods are answered here (ui/js/endpoints/router.js), everything else
// goes to ember_call. See docs/RPC_API.md §5 and apps/web/README.md.
//
//   const b = new WasmBackend();   await b.init();
//   await b.call('node.info');      b.on('message.new', fn);      b.kind === 'wasm'
//
// Options (all optional; ui/js/backend.js constructs it without any):
//   moduleUrl   URL of the Emscripten ES module (default ../../wasm/ember.mjs next to this file)
//   storage     'auto' (IndexedDB if possible) | 'memory' (nothing persisted)
//   guides      false | URL | Uint8Array   (default: wasm/build.json tells whether a pack ships)
//   hub         false | 'auto' | ws:// URL  (default 'auto': probe hub.json when served over http)
//   restoreLinks  default true: on every node.ready reopen the endpoints of the core's links
//   profile     separate storage + identity on this origin (default: URL ?profile=<name>, else none)
//   env         globals (tests)
import { HostRouter } from '../endpoints/router.js';
import { ENDPOINTS } from '../endpoints/index.js';
import { CaptureWake } from '../endpoints/capture.js';
import { OfflineManager, GuideStore } from '../endpoints/offline.js';

const STORAGE_DIR = '/ember';
const LOCK_NAME = 'embermesh-storage';
const PROFILE_RE = /^[a-z0-9_-]{1,32}$/;
// Calls that never change stored state (no IndexedDB flush needed after them).
const READ_ONLY = /(^|\.)(list|get|info|status|search|categories|types|card|peers|stats|bands|radios|run|qr)$|^(guides|tools|morse)\./;
const SYNC_DEBOUNCE_MS = 400;
const MIN_TICK_MS = 4;
const enc = new TextEncoder();

// Smallest module using a v128 instruction (the wasm-feature-detect "simd" probe): engines without
// SIMD reject it in WebAssembly.validate, so the baseline module is used there.
const SIMD_PROBE = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 5, 1, 96, 0, 1, 123, 3, 2, 1, 0, 10, 10, 1, 8, 0, 65, 0, 253, 15, 253, 98, 11]);
export function wasmSimdSupported() {
  try { return typeof WebAssembly === 'object' && WebAssembly.validate(SIMD_PROBE); } catch { return false; }
}
const dec = new TextDecoder();

function rpcError(code, message) {
  return Object.assign(new Error(message || code), { code });
}

function deviceName(env) {
  const nav = env.navigator || {};
  const ua = nav.userAgent || '';
  const os = /Android/.test(ua) ? 'Android' : /iPhone|iPad|iPod/.test(ua) ? 'iOS'
    : /Mac OS X/.test(ua) ? 'Mac' : /Windows/.test(ua) ? 'Windows' : /CrOS/.test(ua) ? 'ChromeOS'
    : /Linux/.test(ua) ? 'Linux' : (typeof process !== 'undefined' && process.versions && process.versions.node) ? 'Node' : 'Web';
  const br = /Edg\//.test(ua) ? 'Edge' : /Firefox\//.test(ua) ? 'Firefox' : /Chrome\//.test(ua) ? 'Chrome'
    : /Safari\//.test(ua) ? 'Safari' : '';
  return br ? `${br} on ${os}` : os;
}

function localStore(env) {
  const prefix = 'embermesh.';
  if (!env.document) return { get: () => null, set: () => {} }; // Node / workers: no localStorage
  return {
    get(key) {
      try { const v = env.localStorage && env.localStorage.getItem(prefix + key); return v ? JSON.parse(v) : null; } catch { return null; }
    },
    set(key, value) {
      try { if (env.localStorage) env.localStorage.setItem(prefix + key, JSON.stringify(value)); } catch {}
    },
  };
}

/** Timer that keeps firing in background tabs: Worker timers aren't throttled like page timers. */
function makeTicker(env, fn) {
  let worker = null;
  if (typeof env.Worker === 'function' && typeof env.Blob === 'function' && env.document) {
    try {
      const src = 'let t=0;onmessage=e=>{clearTimeout(t);t=setTimeout(()=>postMessage(0),e.data)}';
      const url = URL.createObjectURL(new env.Blob([src], { type: 'text/javascript' }));
      worker = new env.Worker(url);
      URL.revokeObjectURL(url);
      worker.onmessage = () => fn();
      worker.onerror = (e) => { if (e && e.preventDefault) e.preventDefault(); worker = null; };
    } catch { worker = null; }
  }
  let t = 0;
  return {
    get kind() { return worker ? 'worker' : 'timeout'; },
    set(ms) {
      clearTimeout(t);
      t = setTimeout(fn, ms); // page timer (precise while visible)
      if (worker) worker.postMessage(ms + 25); // backstop when the page timer is throttled
    },
    stop() { clearTimeout(t); if (worker) { worker.terminate(); worker = null; } },
  };
}

export default class WasmBackend {
  constructor(opts = {}) {
    this.kind = 'wasm';
    this.opts = opts;
    this.env = opts.env || globalThis;
    this.M = null;
    this.node = 0;
    this.ready = false;
    this.storage = 'memory';
    this.storageNote = '';
    this.version = '';
    this.buildInfo = null;
    this._listeners = new Map();
    this._initPromise = null;
    this._outPtr = 0;
    this._scratch = { ptr: 0, size: 0 };
    this._tickQueued = false;
    this._ticker = null;
    this._syncTimer = 0;
    this._syncing = null;
    this._syncAgain = false;
    this._lastSync = 0;
    this._lockRelease = null;
    this._unload = [];
    this._guidesPtr = 0;
    this.router = null;
    this.profile = '';
    this.storageDir = STORAGE_DIR;
    this.guides = { state: 'none' };
    this.guidesReady = Promise.resolve();
  }

  // ------------------------------------------------------------------------------ Backend API

  init() {
    if (!this._initPromise) {
      this._initPromise = this._init().catch(async (e) => {
        // a freshly activated update that cannot even start: go back to the previous build
        try { if (await new OfflineManager({ env: this.env, buildInfo: this.buildInfo }).bootFailed()) return new Promise(() => {}); } catch {}
        throw e;
      });
    }
    return this._initPromise;
  }

  async call(method, params) {
    if (!this.ready) await this.init();
    if (typeof method !== 'string' || !method) throw rpcError('bad_params', 'method required');
    if (method.startsWith('host.')) return this.router.handle(method, params || {});
    if (method.startsWith('morse.rx.')) {
      // screen stays awake while a capture session is open (camera / microphone run in this page)
      return this._coreCall(method, params).then(
        (r) => { this._wake.observe(method, params, r); return r; },
        (e) => { this._wake.observe(method, params, null, e); throw e; });
    }
    return this._coreCall(method, params);
  }

  on(event, handler) {
    if (!this._listeners.has(event)) this._listeners.set(event, new Set());
    this._listeners.get(event).add(handler);
    return () => { const s = this._listeners.get(event); if (s) s.delete(handler); };
  }

  async destroy() {
    if (!this.node) return;
    if (this._ticker) this._ticker.stop();
    if (this._wake) this._wake.close();
    if (this.offline) this.offline.stop();
    for (const off of this._unload) off();
    await this.router.close();
    await this._flush();
    this.M._ember_destroy(this.node);
    this.node = 0;
    this.ready = false;
    if (this._lockRelease) this._lockRelease();
  }

  // ------------------------------------------------------------------------------ startup

  async _init() {
    const env = this.env;
    this.profile = this._profileName();
    if (this.profile) this.storageDir = `${STORAGE_DIR}-${this.profile}`;
    this.buildInfo = await this._fetchJson(this.opts.buildInfoUrl || new URL('../../wasm/build.json', import.meta.url));
    const M = await this._loadModule();
    this.M = M;
    this.version = M.UTF8ToString(M._ember_version());
    if (M._ember_web_out_size() !== 16 || M._ember_web_out_data_offset() !== 8 || M._ember_web_out_len_offset() !== 12) {
      throw rpcError('internal', 'unexpected ember_out layout in WASM module');
    }
    this._outPtr = M._malloc(16);
    await this._setupStorage();

    const entropy = new Uint8Array(64);
    env.crypto.getRandomValues(entropy);
    const appVersion = this.opts.appVersion || (this.buildInfo && this.buildInfo.version) || this.version;
    const cfg = JSON.stringify({
      storage_dir: this.storage === 'indexeddb' ? this.storageDir : '',
      platform: 'web', app_version: appVersion, device_name: deviceName(env), now_ms: Date.now(),
    });
    const cfgPtr = this._cstr(cfg);
    const entPtr = M._malloc(entropy.length);
    M.HEAPU8.set(entropy, entPtr);
    this.node = M._ember_create(cfgPtr, entPtr, entropy.length);
    M.HEAPU8.fill(0, entPtr, entPtr + entropy.length);
    M._free(entPtr);
    M._free(cfgPtr);
    entropy.fill(0);
    if (!this.node) throw rpcError('internal', 'ember_create failed');
    // A first run creates the identity inside ember_create: persist it before anything else can
    // interrupt the page (the store is written synchronously into MEMFS).
    if (this.storage === 'indexeddb') this._flush();

    this.router = new HostRouter({
      env, endpoints: this.opts.endpoints || ENDPOINTS, store: localStore(env), log: this.opts.log || console,
      autoReopen: this.opts.restoreLinks !== false, // startup step runs on node.ready (first tick)
      emit: (event, data) => this._dispatch(event, data),
      core: {
        call: (m, p) => this._coreCall(m, p),
        linkRx: (id, bytes) => this.linkRx(id, bytes),
        audioRx: (id, pcm, rate) => this.audioRx(id, pcm, rate),
        audioTx: (id, max, rate) => this.audioTx(id, max, rate),
        kick: () => this.kick(),
      },
      info: {
        app_version: appVersion, backend: 'wasm', core_version: this.version, wasm_simd: !!this.simd,
        storage: this.storage, storage_note: this.storageNote || undefined, profile: this.profile || undefined,
        single_file: !!globalThis.__EMBER_SINGLE__,
      },
    });

    // RPC_API §9: offline install + background updates (no-op on file://, plain-http hubs, Node)
    this.offline = new OfflineManager({ env, buildInfo: this.buildInfo, emit: (event, data) => this._dispatch(event, data), log: this.opts.log || console });
    this.router.offline = this.offline;
    this._wake = new CaptureWake(() => this.router.keepAwake(), { log: this.opts.log || console });
    this._ticker = makeTicker(env, () => this._tick());
    this._installPageHooks();
    this.ready = true;
    if (!globalThis.__emberWasmBackend) globalThis.__emberWasmBackend = this;
    // First tick on a later task so listeners registered right after `await init()` see node.ready.
    setTimeout(() => this._tick(), 0);

    // in the background: a big pack must not delay the node (guides.* answer once it is in)
    this.guidesReady = this._loadGuides().catch((e) => this._warn('guide library not loaded:', e.message || e));
    if (this.opts.offline !== false) {
      // the node is up (= this build boots): confirm it, then store everything for offline use
      // (after the guide pack the page is loading anyway has been stored, so it is not fetched twice)
      this.guidesReady.then(() => this.guidesStoring).then(() => this.offline.start())
        .catch((e) => this._warn('offline install:', (e && e.message) || e));
    }
    this._joinHub().catch((e) => this._warn('hub auto-connect failed:', e.message || e));
    return this;
  }

  async _loadModule() {
    const tried = [];
    // WebAssembly SIMD build when this build ships one and the engine supports it (feature test
    // below); the baseline module otherwise, and as the fallback if the SIMD one fails to load.
    const simdUrl = this._useSimd() ? new URL('../../wasm/ember-simd.mjs', import.meta.url).href : null;
    const candidates = [this.opts.moduleUrl, globalThis.EMBER_WASM_URL, simdUrl, new URL('../../wasm/ember.mjs', import.meta.url).href].filter(Boolean);
    for (const url of candidates) {
      try {
        const mod = await import(/* @vite-ignore */ String(url));
        this.simd = /ember-simd\.mjs/.test(String(url));
        const factory = mod.default || mod.createEmberModule;
        return await factory({
          print: (s) => console.log('[core]', s),
          printErr: (s) => console.warn('[core]', s),
        });
      } catch (e) {
        tried.push(`${url}: ${e.message || e}`);
      }
    }
    throw rpcError('internal', `could not load the WebAssembly core (${tried.join('; ')})`);
  }

  _profileName() {
    let p = this.opts.profile;
    if (p === undefined) {
      try { p = new URL(this.env.location.href).searchParams.get('profile'); } catch { p = null; }
    }
    p = String(p || '').toLowerCase();
    return PROFILE_RE.test(p) ? p : '';
  }

  /** True when the SIMD module ships (build.json simd) and may be used (?nosimd=1 / opts.simd:false force baseline). */
  _useSimd() {
    if (this.opts.simd === false || !(this.buildInfo && this.buildInfo.simd)) return false;
    try { if (new URL(this.env.location.href).searchParams.get('nosimd') === '1') return false; } catch {}
    return wasmSimdSupported();
  }

  async _fetchJson(url) {
    try {
      if (typeof this.env.fetch !== 'function') return null;
      const r = await this.env.fetch(String(url), { cache: 'no-store' });
      return r.ok ? await r.json() : null;
    } catch { return null; }
  }

  // ------------------------------------------------------------------------------ storage

  async _setupStorage() {
    const env = this.env;
    const M = this.M;
    const want = this.opts.storage || 'auto';
    if (want === 'memory') { this.storageNote = 'in-memory (requested)'; return; }
    if (!env.indexedDB || !M.IDBFS) { this.storageNote = 'IndexedDB unavailable: nothing is saved when the page closes'; return; }
    // One node per profile owns the persistent store; extra tabs run a temporary node.
    if (env.navigator && env.navigator.locks && env.navigator.locks.request) {
      const got = await new Promise((resolve) => {
        env.navigator.locks.request(this.profile ? `${LOCK_NAME}:${this.profile}` : LOCK_NAME, { ifAvailable: true }, (lock) => {
          if (!lock) { resolve(false); return undefined; }
          resolve(true);
          return new Promise((release) => { this._lockRelease = release; });
        }).catch(() => resolve(false));
      });
      if (!got) { this.storageNote = 'EmberMesh is already open in another tab; this tab runs a temporary node with its own identity'; return; }
    }
    try {
      // IDBFS names its IndexedDB database after the mount point: one database per profile.
      M.FS.mkdir(this.storageDir);
      M.FS.mount(M.IDBFS, { autoPersist: false }, this.storageDir);
      await new Promise((resolve, reject) => M.FS.syncfs(true, (err) => (err ? reject(err) : resolve())));
      this.storage = 'indexeddb';
      if (env.navigator && env.navigator.storage && env.navigator.storage.persist) {
        env.navigator.storage.persisted().then((p) => p || env.navigator.storage.persist()).catch(() => {});
      }
    } catch (e) {
      this.storageNote = `IndexedDB failed (${e && (e.message || e.name || e)}); nothing is saved when the page closes`;
      try { M.FS.unmount(this.storageDir); } catch {}
    }
  }

  /**
   * storage.dirty -> write MEMFS to IndexedDB. Leading edge (a quiet store flushes at once, so a
   * reload right after e.g. first-run identity creation keeps it) plus a trailing flush that
   * coalesces bursts. The core writes its files synchronously (pwrite into MEMFS), so whatever it
   * reported dirty is in MEMFS by the time the event is drained.
   */
  _scheduleSync(delayMs = SYNC_DEBOUNCE_MS) {
    if (this.storage !== 'indexeddb') return;
    clearTimeout(this._syncTimer);
    if (!this._syncing && Date.now() - this._lastSync >= SYNC_DEBOUNCE_MS) { this._flush(); return; }
    this._syncTimer = setTimeout(() => this._flush(), delayMs);
  }

  /** Writes the in-memory FS to IndexedDB. Serialised: one syncfs at a time. */
  _flush() {
    if (this.storage !== 'indexeddb' || !this.M) return Promise.resolve();
    clearTimeout(this._syncTimer);
    if (this._syncing) { this._syncAgain = true; return this._syncing; }
    this._lastSync = Date.now();
    this._syncing = new Promise((resolve) => {
      this.M.FS.syncfs(false, (err) => {
        if (err) this._warn('saving to IndexedDB failed:', err.message || err);
        resolve();
      });
    }).then(() => {
      this._syncing = null;
      if (this._syncAgain) { this._syncAgain = false; return this._flush(); }
      return undefined;
    });
    return this._syncing;
  }

  _installPageHooks() {
    const env = this.env;
    if (!env.addEventListener) return;
    const add = (target, ev, fn) => { target.addEventListener(ev, fn); this._unload.push(() => target.removeEventListener(ev, fn)); };
    add(env, 'pagehide', () => this._flush());
    if (env.document) {
      add(env.document, 'visibilitychange', () => {
        if (env.document.visibilityState === 'hidden') this._flush();
        else this.kick();
      });
    }
    add(env, 'online', () => this.kick());
    add(env, 'focus', () => this.kick());
  }

  // ------------------------------------------------------------------------------ guides / hub

  /**
   * Loads the guide library in the background (init() does not wait: a pack can be 50+ MB).
   * The response is streamed straight into one WASM heap buffer sized from Content-Length (no
   * intermediate ArrayBuffer, so the peak is ~1x the pack, not 2x) and handed to
   * ember_guides_load(copy=0). Progress: event `guides.progress {received, total, source}`, then
   * `guides.loaded {bytes, ms, source}` or `guides.error {message}`; host.info.guides mirrors it;
   * `guides.cached {cache}` once a network copy is stored for offline use.
   * Over http(s) the pack is kept in a content-addressed Cache API cache (embermesh-guides-<hash>,
   * shared with the service worker), so it is downloaded once and works offline.
   */
  async _loadGuides() {
    let g = this.opts.guides;
    if (g === false) return;
    if (g === undefined) {
      const path = this.buildInfo && this.buildInfo.guides;
      if (!path) return; // this build ships no guide pack
      g = new URL(`../../${path}`, import.meta.url);
    }
    const t0 = Date.now();
    this._guidesState({ state: 'loading', received: 0, total: 0 });
    try {
      if (g instanceof Uint8Array) {
        this.loadGuidePack(g);
        this._guidesDone(g.length, t0, 'memory');
        return;
      }
      const url = String(g);
      const { resp, source } = await this._openGuides(url);
      const total = Number(resp.headers.get('content-length')) || (this.buildInfo && this.buildInfo.guides_bytes) || 0;
      const { ptr, len } = await this._streamToHeap(resp, total, (received) => this._guidesProgress(received, total, source));
      this._adoptGuides(ptr, len);
      this._guidesDone(len, t0, source);
    } catch (e) {
      this._guidesState({ state: 'error', error: String((e && e.message) || e) });
      this._dispatch('guides.error', { message: String((e && e.message) || e) });
      this._dispatch('host.guides.progress', { loaded: 0, total: 0, error: String((e && e.message) || e) });
      throw e;
    }
  }

  async _openGuides(url) {
    const env = this.env;
    const bi = this.buildInfo || {};
    const loc = env.location;
    const store = bi.guides_hash && bi.guides_bytes && env.caches && loc && /^https?:$/.test(loc.protocol) && env.isSecureContext !== false
      ? new GuideStore(env.caches, bi.guides_hash, bi.guides_bytes, new URL('./', loc.href).href) : null;
    if (store && (await store.complete().catch(() => false))) {
      // stored and verified on this device (offline install): read the chunks, no network at all
      this.guidesCached = true;
      const parts = await store.readAll();
      if (parts) {
        let i = 0;
        const body = new ReadableStream({ pull(c) { if (i < parts.length) c.enqueue(parts[i++]); else c.close(); } });
        return { resp: new Response(body, { headers: { 'content-length': String(bi.guides_bytes) } }), source: 'cache' };
      }
    }
    const r = await env.fetch(url);
    if (!r.ok) throw new Error(`HTTP ${r.status} for ${url}`);
    if (store && r.body && r.body.tee) {
      // one branch goes to the heap, the other into the verified chunk store (offline copy)
      const [a2, b2] = r.body.tee();
      const headers = new Headers(r.headers);
      this.guidesStoring = store.writeStream(b2)
        .then(() => store.verifyAndSeal())
        .then((ok) => { if (ok) { this.guidesCached = true; this._dispatch('guides.cached', { cache: store.name }); } })
        .catch((e) => this._warn('storing the guide pack failed:', e && e.message));
      return { resp: new Response(a2, { headers }), source: 'network' };
    }
    return { resp: r, source: loc && loc.protocol === 'file:' ? 'inline' : 'network' };
  }

  /** Reads a Response body into a malloc'd heap buffer. Returns {ptr, len}; caller owns ptr. */
  async _streamToHeap(resp, total, onProgress) {
    const M = this.M;
    if (!resp.body || !resp.body.getReader) {
      const bytes = new Uint8Array(await resp.arrayBuffer());
      const ptr = M._malloc(bytes.length || 1);
      if (!ptr) throw rpcError('too_large', 'out of memory for guide pack');
      M.HEAPU8.set(bytes, ptr);
      onProgress(bytes.length);
      return { ptr, len: bytes.length };
    }
    let cap = total > 0 ? total : 4 << 20;
    let ptr = M._malloc(cap);
    if (!ptr) throw rpcError('too_large', 'out of memory for guide pack');
    let len = 0;
    let last = 0;
    const reader = resp.body.getReader();
    try {
      for (;;) {
        const { done, value } = await reader.read();
        if (done) break;
        if (!value || !value.length) continue;
        if (len + value.length > cap) {
          // only without (or with a wrong) Content-Length: grow geometrically
          const ncap = Math.max(cap * 2, len + value.length);
          const np = M._malloc(ncap);
          if (!np) throw rpcError('too_large', 'out of memory for guide pack');
          M.HEAPU8.copyWithin(np, ptr, ptr + len);
          M._free(ptr);
          ptr = np; cap = ncap;
        }
        M.HEAPU8.set(value, ptr + len); // HEAPU8 is re-read: memory may have grown
        len += value.length;
        const now = Date.now();
        if (now - last > 100) { last = now; onProgress(len); }
      }
    } catch (e) {
      M._free(ptr);
      throw e;
    }
    onProgress(len);
    return { ptr, len };
  }

  _guidesProgress(received, total, source) {
    this._guidesState({ state: 'loading', received, total, source });
    this._dispatch('guides.progress', { received, total, source });
    // RPC_API "Host capability hints": the library screens wait for this one; loaded == total only
    // after ember_guides_load (see _guidesDone), so cap it below total while still streaming.
    this._dispatch('host.guides.progress', { loaded: total > 0 ? Math.min(received, total - 1) : received, total });
  }

  _guidesDone(bytes, t0, source) {
    const ms = Date.now() - t0;
    this._guidesState({ state: 'loaded', received: bytes, total: bytes, bytes, ms, source });
    this._dispatch('guides.loaded', { bytes, ms, source });
    this._dispatch('host.guides.progress', { loaded: bytes, total: bytes }); // final: library ready
  }

  _guidesState(st) {
    this.guides = st;
    if (this.router) this.router.info.guides = st;
  }

  /** Hands a heap buffer to the core (zero-copy; we keep it alive until replaced/destroyed). */
  _adoptGuides(ptr, len) {
    const rc = this.M._ember_guides_load(this.node, ptr, len, 0);
    if (rc < 0) { this.M._free(ptr); throw rpcError('bad_params', `guide pack rejected (${rc})`); }
    if (this._guidesPtr) this.M._free(this._guidesPtr);
    this._guidesPtr = ptr;
    this.kick();
    return rc;
  }

  /** Loads (or replaces) the guide library from a guides.pack. Returns the core's result code. */
  loadGuidePack(bytes) {
    const M = this.M;
    const ptr = M._malloc(bytes.length || 1);
    if (!ptr) throw rpcError('too_large', 'out of memory for guide pack');
    M.HEAPU8.set(bytes, ptr);
    return this._adoptGuides(ptr, bytes.length);
  }

  async _joinHub() {
    const env = this.env;
    const mode = this.opts.hub === undefined ? 'auto' : this.opts.hub;
    if (mode === false) return;
    const loc = env.location;
    let params = null;
    try { params = loc ? new URL(loc.href).searchParams : null; } catch {}
    if (params && params.get('nohub') === '1') return;
    let url = typeof mode === 'string' && mode !== 'auto' ? mode : params && params.get('hub');
    let name = '';
    if (!url) {
      if (!loc || !/^https?:$/.test(loc.protocol)) return;
      const info = await this._fetchJson(this.opts.hubProbeUrl || new URL('../../hub.json', import.meta.url));
      if (!info || info.embermesh_hub !== true) return;
      const u = new URL(info.ws || '/ws', loc.href);
      u.protocol = u.protocol === 'https:' ? 'wss:' : u.protocol === 'http:' ? 'ws:' : u.protocol;
      url = u.href;
      name = String(info.name || '');
    }
    await this.router.connectHub({ url, name });
  }

  // ------------------------------------------------------------------------------ core glue

  _cstr(s) {
    const M = this.M;
    const b = enc.encode(s);
    const p = M._malloc(b.length + 1);
    M.HEAPU8.set(b, p);
    M.HEAPU8[p + b.length] = 0;
    return p;
  }

  _scratchPtr(bytes) {
    const s = this._scratch;
    if (s.size < bytes) {
      if (s.ptr) this.M._free(s.ptr);
      s.size = Math.max(bytes, 4096, s.size * 2);
      s.ptr = this.M._malloc(s.size);
    }
    return s.ptr;
  }

  _coreCall(method, params) {
    const M = this.M;
    if (!this.node) return Promise.reject(rpcError('internal', 'node not running'));
    const mp = this._cstr(method);
    const pp = this._cstr(params === undefined || params === null ? '{}' : JSON.stringify(params));
    let text;
    try {
      const rp = M._ember_call(this.node, mp, pp);
      if (!rp) return Promise.reject(rpcError('internal', 'ember_call returned NULL'));
      text = M.UTF8ToString(rp);
      M._ember_free(rp);
    } finally {
      M._free(mp);
      M._free(pp);
    }
    this.kick();
    // A state-changing call is on disk (MEMFS) as soon as it returns, but the core reports
    // storage.dirty at most once a second: flush soon so a reload right after it keeps it.
    if (!READ_ONLY.test(method)) this._scheduleSync(50);
    let env;
    try { env = JSON.parse(text); } catch { return Promise.reject(rpcError('internal', 'core returned invalid JSON')); }
    if (env && env.ok) return Promise.resolve(env.result === undefined ? {} : env.result);
    const e = (env && env.error) || {};
    return Promise.reject(rpcError(e.code || 'internal', e.message || 'call failed'));
  }

  linkRx(linkId, bytes) {
    if (!this.node || !bytes || !bytes.length) return;
    const p = this._scratchPtr(bytes.length);
    this.M.HEAPU8.set(bytes, p);
    this.M._ember_link_rx(this.node, linkId >>> 0, p, bytes.length);
  }

  audioRx(linkId, pcm, rate) {
    if (!this.node || !pcm || !pcm.length) return;
    const p = this._scratchPtr(pcm.length * 2);
    this.M.HEAP16.set(pcm, p >> 1);
    this.M._ember_audio_rx(this.node, linkId >>> 0, p, pcm.length, rate >>> 0);
  }

  audioTx(linkId, max, rate) {
    if (!this.node || !(max > 0)) return new Int16Array(0);
    const p = this._scratchPtr(max * 2);
    const n = this.M._ember_audio_tx(this.node, linkId >>> 0, p, max, rate >>> 0);
    if (!n) return new Int16Array(0);
    const out = this.M.HEAP16.slice(p >> 1, (p >> 1) + Math.min(n, max));
    this.kick(); // a finished transmission may queue events (PTT off)
    return out;
  }

  /** Input arrived or state changed: tick + drain soon (coalesced within the current task). */
  kick() {
    if (this._tickQueued || !this.node) return;
    this._tickQueued = true;
    Promise.resolve().then(() => this._tick());
  }

  _tick() {
    this._tickQueued = false;
    if (!this.node) return;
    const M = this.M;
    M._ember_tick(this.node, BigInt(Date.now()));
    this._drain();
    let next = M._ember_next_wakeup_ms(this.node);
    if (!(next >= MIN_TICK_MS)) next = MIN_TICK_MS;
    if (next > 60000) next = 60000;
    if (this._ticker) this._ticker.set(next);
  }

  /** Drains the outbox: events to listeners, link bytes to endpoints. */
  _drain() {
    const M = this.M;
    const o = this._outPtr;
    for (let guard = 0; guard < 100000; guard++) {
      if (!M._ember_poll(this.node, o)) break;
      const u32 = M.HEAPU32;
      const kind = u32[o >> 2];
      const linkId = u32[(o >> 2) + 1];
      const dp = u32[(o >> 2) + 2];
      const len = u32[(o >> 2) + 3];
      if (kind === 1) {
        let msg;
        try { msg = JSON.parse(dec.decode(M.HEAPU8.subarray(dp, dp + len))); } catch { this._warn('bad event JSON from core'); continue; }
        if (msg && typeof msg.event === 'string') this._coreEvent(msg.event, msg.data || {});
      } else if (kind === 2) {
        const bytes = M.HEAPU8.slice(dp, dp + len);
        try { this.router.onLinkTx(linkId, bytes); } catch (e) { this._warn('link tx failed:', e.message || e); }
      }
    }
  }

  _coreEvent(event, data) {
    if (event === 'storage.dirty') this._scheduleSync();
    try { this.router.onCoreEvent(event, data); } catch (e) { this._warn('host event handler failed:', e.message || e); }
    this._dispatch(event, data);
  }

  _dispatch(event, data) {
    const call = (fn, ...args) => { try { fn(...args); } catch (e) { console.error(`[wasm backend] "${event}" handler threw`, e); } };
    const s = this._listeners.get(event);
    if (s) for (const fn of [...s]) call(fn, data);
    const all = this._listeners.get('*');
    if (all) for (const fn of [...all]) call(fn, event, data);
  }

  _warn(...args) { (this.opts.log || console).warn('[wasm backend]', ...args); }
}
