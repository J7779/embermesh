// Fully offline installs and background updates of the hosted web app (RPC_API §9).
//
// The page does the downloading (it is alive while the user uses the app; a service worker may be
// stopped at any time), the service worker (apps/web/pwa/sw.js) serves whatever the shared state
// record says is the active build. Both talk through the Cache API:
//
//   embermesh-app-<build>     every file listed in that build's wasm/build.json, sha256-verified
//   embermesh-guides-<hash>   the guide pack as 4 MiB chunks + an index; resumable; verified
//                             against build.json's guides_hash/guides_bytes before it is complete
//   embermesh-state           state.json {active, pending, trial, previous, builds{}, failed{}, …}
//
// A build is "complete" (offline_ready) only when every file and every guide chunk is stored and
// verified. Updates go into a NEW cache; the worker switches to them on the next start; the page
// confirms a good boot (then old caches are deleted); a build that never boots is rolled back.
import { Sha256, sha256Hex } from './sha256.js';

const STATE_CACHE = 'embermesh-state';
const STATE_KEY = 'state.json';
const APP_PREFIX = 'embermesh-app-';
const GUIDES_PREFIX = 'embermesh-guides-';
const GUIDES_PATH = 'data/guides.pack';
export const CHUNK = 4 * 1024 * 1024;
const CHECK_EVERY_MS = 6 * 3600 * 1000;
const CHECK_TIMEOUT_MS = 6000;
const RETRY_FAILED_MS = 3600 * 1000;

const mb = (n) => `${(n / 1048576).toFixed(n < 10 * 1048576 ? 1 : 0)} MB`;

function errCode(code, message) { return Object.assign(new Error(message), { code }); }

/** Guide pack stored as chunks in a content-addressed cache (resumable, verified). */
export class GuideStore {
  constructor(caches, hash, bytes, scope) {
    this.cachesApi = caches;
    this.hash = hash;
    this.bytes = bytes;
    this.scope = scope;
    this.chunks = Math.max(1, Math.ceil(bytes / CHUNK));
    this.name = GUIDES_PREFIX + hash;
  }
  key(i) { return new URL(`${GUIDES_PATH}.part${i}`, this.scope).href; }
  indexKey() { return new URL(`${GUIDES_PATH}.parts`, this.scope).href; }
  async cache() { return this.cachesApi.open(this.name); }
  async index() {
    const r = await (await this.cache()).match(this.indexKey());
    return r ? r.json() : null;
  }
  async complete() { const i = await this.index(); return !!(i && i.complete); }
  /** Indexes of chunks already stored (with the right length). */
  async present() {
    const c = await this.cache();
    const have = new Set();
    for (const req of await c.keys()) {
      const m = /\.part(\d+)$/.exec(req.url);
      if (m) have.add(Number(m[1]));
    }
    return have;
  }
  async storedBytes() {
    const have = await this.present();
    let n = 0;
    for (const i of have) n += i === this.chunks - 1 ? this.bytes - CHUNK * (this.chunks - 1) : CHUNK;
    return n;
  }
  async putChunk(i, bytes) {
    const want = i === this.chunks - 1 ? this.bytes - CHUNK * (this.chunks - 1) : CHUNK;
    if (bytes.length !== want) throw errCode('io_error', `guide chunk ${i}: ${bytes.length} bytes, expected ${want}`);
    await (await this.cache()).put(this.key(i), new Response(bytes, { headers: { 'content-type': 'application/octet-stream' } }));
  }
  /** Reads the stored pack (complete or not: callers check). */
  async readAll() {
    const c = await this.cache();
    const out = [];
    for (let i = 0; i < this.chunks; i++) {
      const r = await c.match(this.key(i));
      if (!r) return null;
      out.push(new Uint8Array(await r.arrayBuffer()));
    }
    return out;
  }
  /** Hashes the stored chunks; marks the pack complete only if size and sha256 prefix match. */
  async verifyAndSeal() {
    const c = await this.cache();
    const h = new Sha256();
    let n = 0;
    for (let i = 0; i < this.chunks; i++) {
      const r = await c.match(this.key(i));
      if (!r) return false;
      const b = new Uint8Array(await r.arrayBuffer());
      h.update(b);
      n += b.length;
    }
    const ok = n === this.bytes && h.hex().slice(0, this.hash.length) === this.hash;
    if (!ok) { await this.cachesApi.delete(this.name); return false; }
    await c.put(this.indexKey(), new Response(JSON.stringify({ hash: this.hash, bytes: this.bytes, chunks: this.chunks, complete: true }), { headers: { 'content-type': 'application/json' } }));
    return true;
  }
  /**
   * Stores a stream of the whole pack (e.g. the copy the page is loading anyway), skipping chunks
   * already present. Resolves to the number of new bytes written.
   */
  async writeStream(stream, onProgress) {
    const have = await this.present();
    const reader = stream.getReader();
    let buf = new Uint8Array(Math.min(CHUNK, this.bytes));
    let fill = 0, idx = 0, total = 0, written = 0;
    const flush = async () => {
      if (!have.has(idx)) { await this.putChunk(idx, buf.subarray(0, fill).slice()); written += fill; }
      idx++; fill = 0;
      const left = this.bytes - idx * CHUNK;
      if (left > 0 && left < buf.length) buf = new Uint8Array(left);
    };
    for (;;) {
      const { done, value } = await reader.read();
      if (done) break;
      let off = 0;
      while (off < value.length) {
        const n = Math.min(buf.length - fill, value.length - off);
        buf.set(value.subarray(off, off + n), fill);
        fill += n; off += n; total += n;
        if (fill === buf.length) await flush();
      }
      if (onProgress) onProgress(total);
    }
    if (fill > 0) await flush();
    return written;
  }
}

export class OfflineManager {
  /**
   * @param {object} o
   * @param {object} o.env          window
   * @param {object} o.buildInfo    the running build's wasm/build.json
   * @param {(event, data) => void} o.emit
   * @param {object} [o.log]
   */
  constructor({ env = globalThis, buildInfo = null, emit = () => {}, log = console } = {}) {
    this.env = env;
    this.build = buildInfo;
    this.emit = emit;
    this.log = log;
    this.scope = new URL('./', env.location ? env.location.href : 'http://localhost/').href;
    this.busy = null;
    this.progress = { stored_bytes: 0, total_bytes: 0 };
    this.update = { available: false, ready: false };
    this.error = null;
    this.persistent = false;
    this.lastCheck = 0;
    this._timer = 0;
  }

  /** Why offline installs are impossible here, or null. */
  unsupportedReason() {
    const env = this.env;
    const loc = env.location || {};
    if (loc.protocol === 'file:') return 'This is the single-file copy: it already works without a network.';
    if (!env.isSecureContext) return 'Offline install needs HTTPS (or localhost). A hub page on plain http://<address>:8437/ works only while connected to the hub; download the single-file copy from it instead.';
    if (!env.caches) return 'This browser has no Cache API: it cannot store the app for offline use.';
    if (!(env.navigator && env.navigator.serviceWorker)) return 'This browser has no service workers: it cannot start the app without a network.';
    return null;
  }

  installed() {
    const env = this.env;
    try { if (env.matchMedia && (env.matchMedia('(display-mode: standalone)').matches || env.matchMedia('(display-mode: fullscreen)').matches)) return true; } catch {}
    return !!(env.navigator && env.navigator.standalone);
  }

  // ------------------------------------------------------------------ state shared with the worker

  async state() {
    try {
      const r = await (await this.env.caches.open(STATE_CACHE)).match(STATE_KEY);
      return r ? await r.json() : {};
    } catch { return {}; }
  }
  async saveState(st) {
    await (await this.env.caches.open(STATE_CACHE)).put(STATE_KEY, new Response(JSON.stringify(st), { headers: { 'content-type': 'application/json' } }));
  }
  async mutate(fn) {
    const st = await this.state();
    st.builds = st.builds || {};
    st.failed = st.failed || {};
    const r = await fn(st);
    await this.saveState(st);
    return r;
  }

  // ------------------------------------------------------------------ start / status

  /**
   * Called once the node and the UI are up: confirms the running build booted (ends a trial,
   * removes caches nothing refers to any more), asks for persistent storage, makes the app
   * offline-capable if it is not yet, and starts the update checks.
   */
  async start() {
    if (this.unsupportedReason()) return;
    const env = this.env;
    const running = this.build && this.build.build;
    await this.mutate(async (st) => {
      if (st.trial && st.trial.build === running) { st.trial = null; st.previous = null; }
      if (st.builds[running]) st.builds[running].booted = true;
    });
    await this.cleanup();
    if (env.navigator.storage && env.navigator.storage.persisted) {
      try { this.persistent = await env.navigator.storage.persisted(); } catch {}
    }
    if (env.addEventListener) {
      env.addEventListener('appinstalled', () => { this.makeOffline().catch(() => {}); });
      env.addEventListener('online', () => { this.resume(); });
    }
    this.resume();
    this._schedule();
  }

  /** Online again / started: finish what is missing, then look for updates. */
  resume() {
    if (this.env.navigator && this.env.navigator.onLine === false) return;
    this.makeOffline().catch(() => {}).then(() => this.checkUpdate().catch(() => {}));
  }

  _schedule() {
    clearTimeout(this._timer);
    this._timer = setTimeout(() => { this.resume(); this._schedule(); }, CHECK_EVERY_MS);
    if (this._timer && this._timer.unref) this._timer.unref();
  }

  stop() { clearTimeout(this._timer); }

  /**
   * The running build failed to start. If it is an update on trial, roll back to the previous build
   * and reload (once). Returns true when a reload was started.
   */
  async bootFailed() {
    if (this.unsupportedReason()) return false;
    const running = this.build && this.build.build; // unknown when the build failed very early
    const rolled = await this.mutate((st) => {
      if (!(st.trial && (!running || st.trial.build === running) && st.previous && st.builds[st.previous] && st.builds[st.previous].complete)) return false;
      const bad = st.trial.build;
      st.failed[bad] = Date.now();
      st.active = st.previous; st.previous = null; st.trial = null;
      if (st.pending === bad) st.pending = null;
      return true;
    });
    if (rolled && this.env.location) { this.env.location.reload(); return true; }
    return false;
  }

  async status() {
    const reason = this.unsupportedReason();
    const base = { installed: this.installed(), version: this.build && this.build.version, build: this.build && this.build.build };
    if (reason) {
      const single = this.env.location && this.env.location.protocol === 'file:';
      return { ...base, offline_ready: single, stored_bytes: 0, total_bytes: 0, persistent: false, supported: false, reason, update: { available: false, ready: false } };
    }
    const st = await this.state();
    const active = st.active && st.builds && st.builds[st.active];
    const running = this.build && this.build.build;
    const rec = (st.builds || {})[running] || active;
    const total = rec ? rec.total_bytes : (this.build && this.build.total_bytes) || 0;
    const complete = !!(rec && rec.complete);
    const pend = st.pending && st.builds[st.pending];
    const out = {
      ...base, supported: true,
      offline_ready: complete,
      stored_bytes: complete ? total : this.progress.stored_bytes,
      total_bytes: total,
      persistent: this.persistent,
      update: {
        available: !!(this.update.available || pend),
        ...(pend ? { version: pend.version, build: pend.build, ready: !!pend.complete, total_bytes: pend.total_bytes } : { ready: false }),
        ...(this.update.downloading ? { downloaded_bytes: this.update.downloaded_bytes, total_bytes: this.update.total_bytes } : {}),
      },
      ...(st.last_check_ts ? { last_check_ts: st.last_check_ts } : {}),
      ...(this.error ? { error: this.error } : {}),
      ...(this.evictionWarning() ? { warning: this.evictionWarning() } : {}),
    };
    if (this.busy && !complete) out.downloading = true;
    return out;
  }

  evictionWarning() {
    if (this.persistent) return null;
    const ua = (this.env.navigator && this.env.navigator.userAgent) || '';
    if (/iPhone|iPad|iPod/.test(ua) && !this.installed()) {
      return 'Safari may delete the stored copy if the site is not opened for about a week. Add EmberMesh to the Home Screen (Share → Add to Home Screen) to keep it.';
    }
    return 'The browser may delete the stored copy if the device runs low on space. Installing the app makes that much less likely.';
  }

  _emit() {
    this.status().then((s) => this.emit('host.app.offline', { stored_bytes: s.stored_bytes, total_bytes: s.total_bytes, offline_ready: s.offline_ready, update: s.update, ...(s.error ? { error: s.error } : {}) })).catch(() => {});
  }

  // ------------------------------------------------------------------ downloading a build

  async _fetch(url, opts = {}) {
    const ctl = typeof AbortController === 'function' ? new AbortController() : null;
    const t = opts.timeoutMs ? setTimeout(() => ctl && ctl.abort(), opts.timeoutMs) : 0;
    try {
      const r = await this.env.fetch(url, { cache: 'no-store', signal: ctl ? ctl.signal : undefined, headers: opts.headers });
      return r;
    } finally { clearTimeout(t); }
  }

  /** The build manifest from the network (never from a cache). */
  async latestManifest(timeoutMs = CHECK_TIMEOUT_MS) {
    const r = await this._fetch(new URL(`wasm/build.json?network=${Date.now()}`, this.scope).href, { timeoutMs });
    if (!r.ok) throw errCode('io_error', `build manifest: HTTP ${r.status}`);
    const m = await r.json();
    if (!m || !m.build || !Array.isArray(m.files)) throw errCode('io_error', 'build manifest without a file list');
    return m;
  }

  /**
   * Downloads and verifies every file of build manifest `m` into embermesh-app-<build>, and its
   * guide pack into embermesh-guides-<hash> (skipped when already complete). Returns true when the
   * build is complete. `onBytes(stored, total)` reports progress.
   */
  async downloadBuild(m, onBytes) {
    const env = this.env;
    const cache = await env.caches.open(APP_PREFIX + m.build);
    const total = m.total_bytes || m.files.reduce((n, f) => n + f.size, 0) + (m.guides_bytes || 0);
    let stored = 0;
    const report = () => onBytes && onBytes(stored, total);
    for (const f of m.files) {
      const key = new URL(f.path, this.scope).href;
      const have = await cache.match(key);
      if (have) {
        const b = new Uint8Array(await have.clone().arrayBuffer());
        if (b.length === f.size && (await sha256Hex(b)) === f.sha256) { stored += f.size; report(); continue; }
        await cache.delete(key);
      }
      // `?network=` tells the service worker not to answer from the build that is running now
      const r = await this._fetch(`${key}?network=${m.build}`, { timeoutMs: 120000 });
      if (!r.ok) throw errCode('io_error', `${f.path}: HTTP ${r.status}`);
      const b = new Uint8Array(await r.arrayBuffer());
      if (b.length !== f.size || (await sha256Hex(b)) !== f.sha256) {
        throw errCode('io_error', `${f.path} does not match the build manifest (the site is being updated, or the download was cut)`);
      }
      const headers = { 'content-type': r.headers.get('content-type') || 'application/octet-stream', 'content-length': String(b.length) };
      await cache.put(key, new Response(b, { headers }));
      stored += f.size; report();
    }
    // the manifest itself, so the stored build knows what it is (wasm/build.json of that build)
    await cache.put(new URL('wasm/build.json', this.scope).href, new Response(JSON.stringify(m) + '\n', { headers: { 'content-type': 'application/json' } }));
    if (m.guides && m.guides_hash) {
      const g = new GuideStore(env.caches, m.guides_hash, m.guides_bytes, this.scope);
      if (!(await g.complete())) {
        await this.downloadGuides(g, `${new URL(m.guides, this.scope).href}?network=${m.guides_hash}`, (n) => { onBytes && onBytes(stored + n, total); });
        if (!(await g.verifyAndSeal())) throw errCode('io_error', 'the downloaded guide library does not match the build manifest; it will be downloaded again');
      }
      stored += m.guides_bytes; report();
    }
    return true;
  }

  /** Fetches the missing chunks of a guide pack: HTTP ranges where the server has them. */
  async downloadGuides(g, url, onBytes) {
    const have = await g.present();
    let base = await g.storedBytes();
    onBytes(base);
    const missing = [];
    for (let i = 0; i < g.chunks; i++) if (!have.has(i)) missing.push(i);
    if (!missing.length) return;
    // Try a range request for the first missing chunk: 206 -> fetch chunk by chunk (resumable even
    // across restarts); 200 -> the server ignores ranges: stream the whole pack, storing what is missing.
    const first = missing[0];
    const end = (i) => Math.min(g.bytes, (i + 1) * CHUNK) - 1;
    const r0 = await this._fetch(url, { headers: { Range: `bytes=${first * CHUNK}-${end(first)}` }, timeoutMs: 300000 });
    if (r0.status === 206) {
      let r = r0;
      for (let k = 0; k < missing.length; k++) {
        const i = missing[k];
        if (k > 0) r = await this._fetch(url, { headers: { Range: `bytes=${i * CHUNK}-${end(i)}` }, timeoutMs: 300000 });
        if (r.status !== 206) throw errCode('io_error', `guide pack: HTTP ${r.status}`);
        await g.putChunk(i, new Uint8Array(await r.arrayBuffer()));
        base += end(i) - i * CHUNK + 1;
        onBytes(base);
      }
      return;
    }
    if (!r0.ok || !r0.body) throw errCode('io_error', `guide pack: HTTP ${r0.status}`);
    await g.writeStream(r0.body, (n) => onBytes(Math.max(base, n)));
  }

  /**
   * Makes the running build fully offline-capable (host.app.make_offline). Idempotent; resumes an
   * interrupted download; a no-op when already complete.
   */
  makeOffline() {
    if (this.unsupportedReason()) return Promise.reject(errCode('unsupported', this.unsupportedReason()));
    if (this.busy) return this.busy;
    this.busy = (async () => {
      const env = this.env;
      const st = await this.state();
      const running = this.build && this.build.build;
      const rec = st.builds && st.builds[running];
      if (rec && rec.complete) { this._emit(); return; }
      if (env.navigator.onLine === false) return; // nothing to do without a network; no errors
      if (env.navigator.storage && env.navigator.storage.persist && !this.persistent) {
        try { this.persistent = await env.navigator.storage.persist(); } catch {}
      }
      let m;
      try { m = await this.latestManifest(); } catch (e) { this.log.warn('[offline] no build manifest:', e.message); return; }
      if (m.build !== running) {
        // the site already has a newer build: make that one the offline copy (as an update)
        await this.checkUpdate();
        return;
      }
      await this._checkQuota(m);
      this.error = null;
      try {
        await this.downloadBuild(m, (s, t) => { this.progress = { stored_bytes: s, total_bytes: t }; this._emitThrottled(); });
      } catch (e) {
        this.error = this._explain(e, m);
        this._emit();
        throw e;
      }
      await this.mutate((s2) => {
        s2.builds[m.build] = { build: m.build, version: m.version, guides_hash: m.guides_hash || null, total_bytes: m.total_bytes, complete: true, booted: true, ts: Date.now() };
        if (!s2.active || !s2.builds[s2.active] || !s2.builds[s2.active].complete) s2.active = m.build;
      });
      await this.cleanup();
      this._emit();
    })().finally(() => { this.busy = null; });
    return this.busy;
  }

  _emitThrottled() {
    const now = Date.now();
    if (now - (this._lastEmit || 0) < 250) return;
    this._lastEmit = now;
    this._emit();
  }

  async _checkQuota(m) {
    const est = this.env.navigator.storage && this.env.navigator.storage.estimate ? await this.env.navigator.storage.estimate().catch(() => null) : null;
    if (est && est.quota && est.usage !== undefined && est.quota - est.usage < m.total_bytes) {
      const need = m.total_bytes - (est.quota - est.usage);
      this.error = `Not enough storage for the offline copy: ${mb(m.total_bytes)} needed, ${mb(Math.max(0, est.quota - est.usage))} available. Free about ${mb(need)} on this device and try again.`;
      this._emit();
      throw errCode('too_large', this.error);
    }
  }

  _explain(e, m) {
    if (e && (e.name === 'QuotaExceededError' || /quota/i.test(e.message || ''))) {
      return `Not enough storage for the offline copy (${mb(m.total_bytes)} needed). Free some space on this device and try again.`;
    }
    return `Download interrupted: ${(e && e.message) || e}. It continues when the network is back.`;
  }

  // ------------------------------------------------------------------ updates

  /**
   * host.app.check_update: fetches the build manifest (short timeout, network only); if the site has
   * a newer build, downloads it completely into a new cache in the background and marks it pending
   * (used on the next start). Offline: returns the status, no error.
   */
  async checkUpdate({ apply = false, force = false } = {}) {
    if (this.unsupportedReason()) return this.status();
    const env = this.env;
    if (apply) {
      const st = await this.state();
      if (st.pending && st.builds[st.pending] && st.builds[st.pending].complete && env.location) {
        setTimeout(() => env.location.reload(), 50);
      }
      return this.status();
    }
    if (env.navigator.onLine === false) return this.status();
    let m;
    try { m = await this.latestManifest(); } catch { return this.status(); } // offline / timeout: quietly nothing
    await this.mutate((st) => { st.last_check_ts = Date.now(); });
    const st = await this.state();
    const running = this.build && this.build.build;
    if (m.build === running || m.build === st.active) { this.update = { available: false, ready: false }; return this.status(); }
    if (st.builds[m.build] && st.builds[m.build].complete) return this.status(); // already downloaded
    if (!force && st.failed[m.build] && Date.now() - st.failed[m.build] < RETRY_FAILED_MS) return this.status(); // automatic checks back off; an explicit check retries
    if (this._updating) return this.status();
    this._updating = true;
    this.update = { available: true, ready: false, downloading: true, version: m.version, downloaded_bytes: 0, total_bytes: m.total_bytes };
    this._emit();
    try {
      await this._checkQuota(m);
      await this.downloadBuild(m, (s, t) => { this.update.downloaded_bytes = s; this.update.total_bytes = t; this._emitThrottled(); });
      // the site may have moved on while we downloaded: only a manifest that still matches is used
      const again = await this.latestManifest().catch(() => m);
      if (again.build !== m.build) throw errCode('io_error', 'the site changed during the download; trying again later');
      await this.mutate((s2) => {
        s2.builds[m.build] = { build: m.build, version: m.version, guides_hash: m.guides_hash || null, total_bytes: m.total_bytes, complete: true, booted: false, ts: Date.now() };
        s2.pending = m.build;
        delete s2.failed[m.build];
      });
      this.update = { available: true, ready: true, version: m.version };
    } catch (e) {
      await this.mutate((s2) => { s2.failed[m.build] = Date.now(); });
      this.update = { available: true, ready: false, version: m.version, error: (e && e.message) || String(e) };
      this.log.warn('[offline] update download failed:', e && e.message);
    } finally {
      this._updating = false;
      this._emit();
    }
    return this.status();
  }

  /** Deletes app/guide caches nothing refers to (only once the running build has booted). */
  async cleanup() {
    const env = this.env;
    const st = await this.state();
    if (st.trial) return; // the active build has not proven itself yet: keep the previous one
    const keepBuilds = new Set([st.active, st.pending, st.previous, this.build && this.build.build].filter(Boolean));
    const keepGuides = new Set();
    for (const b of keepBuilds) { const r = st.builds && st.builds[b]; if (r && r.guides_hash) keepGuides.add(r.guides_hash); }
    if (this.build && this.build.guides_hash) keepGuides.add(this.build.guides_hash);
    const activeComplete = st.active && st.builds && st.builds[st.active] && st.builds[st.active].complete;
    for (const name of await env.caches.keys()) {
      if (name.startsWith(APP_PREFIX) && !keepBuilds.has(name.slice(APP_PREFIX.length))) await env.caches.delete(name);
      else if (name.startsWith(GUIDES_PREFIX) && !keepGuides.has(name.slice(GUIDES_PREFIX.length))) await env.caches.delete(name);
      // caches of the previous service-worker design, once a complete build has replaced them
      else if (activeComplete && /^embermesh-\d/.test(name)) await env.caches.delete(name);
    }
    let changed = false;
    for (const b of Object.keys(st.builds || {})) if (!keepBuilds.has(b)) { delete st.builds[b]; changed = true; }
    if (changed) await this.saveState(st);
  }

  // ------------------------------------------------------------------ host.info.actions

  async actions() {
    if (this.unsupportedReason()) return [];
    const s = await this.status();
    const out = [];
    let desc;
    if (s.offline_ready) desc = `Everything is stored on this device (${mb(s.total_bytes)}). Works without internet.`;
    else if (s.downloading || this.busy) desc = `Downloading ${mb(s.stored_bytes)} of ${mb(s.total_bytes)}…`;
    else if (s.error) desc = s.error;
    else desc = `Store the whole app and guide library on this device (${s.total_bytes ? mb(s.total_bytes) : 'tens of MB'}) so it works without internet.`;
    if (s.warning && s.offline_ready) desc += ` ${s.warning}`;
    out.push({ id: 'offline_download', title: 'Download everything for offline use', description: desc, method: 'host.app.make_offline' });
    let ud;
    if (s.update.ready) ud = `Version ${s.update.version || ''} is downloaded and will be used next time you open the app.`;
    else if (this.update.downloading) ud = `Downloading version ${this.update.version || ''}: ${mb(this.update.downloaded_bytes || 0)} of ${mb(this.update.total_bytes || 0)}…`;
    else if (s.last_check_ts) ud = `You have version ${s.version || ''}-${s.build || ''}. Last checked ${new Date(s.last_check_ts).toLocaleString()}.`;
    else ud = `You have version ${s.version || ''}-${s.build || ''}. Updates are checked automatically when there is a network.`;
    out.push({ id: 'check_update', title: 'Check for updates', description: ud, method: 'host.app.check_update' });
    if (s.update.ready) out.push({ id: 'restart_update', title: 'Restart to update', description: `Switch to version ${s.update.version || ''} now.`, method: 'host.app.check_update', params: { apply: true } });
    return out;
  }
}
