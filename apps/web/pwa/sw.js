// EmberMesh service worker (RPC_API §9 "Hosted web app and fully offline installs").
//
// The worker holds no build-specific data: which build is served is decided by a small state record
// that the page and the worker share through the Cache API, so an old worker can never pin an old
// app, and a newer worker script simply takes over (skipWaiting + clients.claim are safe here).
//
//   caches  embermesh-app-<build>     every file of one build (listed with size + sha256 in its
//                                     wasm/build.json), written and verified by the page
//           embermesh-guides-<hash>   the guide pack in chunks (data/guides.pack.part<i>), resumable
//           embermesh-state           state.json: {active, pending, trial, previous, builds{…}, failed{…}}
//
//   fetch   a file of the active build      -> from embermesh-app-<active> (never a mix of builds)
//           data/guides.pack                -> assembled from its chunks when complete, else network
//           any URL with ?network=…         -> network only (update checks, downloading a new build)
//           hub.json, /ws, /rpc, /download/ -> network (hub endpoints)
//           anything else                   -> network, then any cache; navigations fall back to the
//                                              active build's index.html when offline
//
//   start   a navigation with no other window open switches to a downloaded and verified pending
//           build ("next start"); the page confirms a good boot (trial cleared, old caches removed);
//           a build that was started twice without confirming is rolled back.
const SW_LOGIC = 2;
const STATE_CACHE = 'embermesh-state';
const STATE_KEY = 'state.json';
const APP_PREFIX = 'embermesh-app-';
const GUIDES_PREFIX = 'embermesh-guides-';
const GUIDES_PATH = 'data/guides.pack';
const NETWORK_ONLY = [/(^|\/)ws$/, /(^|\/)rpc$/, /(^|\/)download\//];
const NETWORK_FIRST = [/(^|\/)hub\.json$/];
const MAX_TRIAL_STARTS = 2;

const scopeUrl = () => new URL(self.registration.scope);
/** Path of a URL relative to the worker's scope ('' when outside it). */
function rel(url) {
  const s = scopeUrl().href;
  return url.href.startsWith(s) ? url.pathname.slice(new URL(s).pathname.length) : null;
}

async function readState() {
  try {
    const r = await (await caches.open(STATE_CACHE)).match(STATE_KEY);
    return r ? await r.json() : {};
  } catch { return {}; }
}
async function writeState(st) {
  const c = await caches.open(STATE_CACHE);
  await c.put(STATE_KEY, new Response(JSON.stringify(st), { headers: { 'content-type': 'application/json' } }));
}

self.addEventListener('install', () => { self.skipWaiting(); });
self.addEventListener('activate', (event) => { event.waitUntil(self.clients.claim()); });

self.addEventListener('message', (event) => {
  const m = event.data || {};
  if (m && m.type === 'embermesh-sw-logic' && event.source) event.source.postMessage({ type: 'embermesh-sw-logic', logic: SW_LOGIC });
});

/** On a fresh start, switch to a pending build or roll back a trial that never booted. */
async function onStart() {
  const st = await readState();
  let changed = false;
  if (st.trial && st.trial.build === st.active) {
    st.trial.starts = (st.trial.starts || 0) + 1;
    changed = true;
    if (st.trial.starts > MAX_TRIAL_STARTS && st.previous && st.builds && st.builds[st.previous] && st.builds[st.previous].complete) {
      st.failed = st.failed || {};
      st.failed[st.active] = Date.now();
      st.active = st.previous;
      st.previous = null;
      st.trial = null;
    }
  }
  if (st.pending && st.builds && st.builds[st.pending] && st.builds[st.pending].complete) {
    const windows = await self.clients.matchAll({ type: 'window', includeUncontrolled: true });
    if (windows.length <= 1) { // nobody is using the old build any more
      st.previous = st.active || null;
      st.active = st.pending;
      st.pending = null;
      st.trial = { build: st.active, starts: 1, since: Date.now() };
      changed = true;
    }
  }
  if (changed) await writeState(st);
  return st;
}

async function guidesResponse(st) {
  const b = st.active && st.builds && st.builds[st.active];
  const hash = b && b.guides_hash;
  if (!hash) return null;
  const c = await caches.open(GUIDES_PREFIX + hash);
  const idx = await c.match(`${GUIDES_PATH}.parts`);
  if (!idx) return null;
  const meta = await idx.json();
  if (!meta.complete) return null;
  let i = 0;
  const body = new ReadableStream({
    async pull(ctl) {
      if (i >= meta.chunks) { ctl.close(); return; }
      const r = await c.match(`${GUIDES_PATH}.part${i++}`);
      if (!r) { ctl.error(new Error('guide pack chunk missing')); return; }
      ctl.enqueue(new Uint8Array(await r.arrayBuffer()));
    },
  });
  return new Response(body, { headers: { 'content-type': 'application/octet-stream', 'content-length': String(meta.bytes) } });
}

async function fromActive(st, path) {
  if (!st.active) return null;
  const c = await caches.open(APP_PREFIX + st.active);
  return c.match(path || 'index.html');
}

self.addEventListener('fetch', (event) => {
  const req = event.request;
  if (req.method !== 'GET') return;
  const url = new URL(req.url);
  if (url.origin !== self.location.origin) return; // nothing is ever fetched from third parties
  const path = rel(url);
  if (path === null) return;
  if (NETWORK_ONLY.some((r) => r.test(path))) return;
  if (url.searchParams.has('network')) return; // update checks and downloads of a new build: network only
  if (NETWORK_FIRST.some((r) => r.test(path))) {
    event.respondWith(fetch(req).catch(async () => (await caches.match(req, { ignoreSearch: true })) || Response.error()));
    return;
  }
  event.respondWith((async () => {
    const navigate = req.mode === 'navigate';
    const st = navigate ? await onStart() : await readState();
    const key = navigate || path === '' || path.endsWith('/') ? 'index.html' : path;
    if (navigate && st.active && path !== '' && path !== 'index.html' && !(await fromActive(st, path))) {
      // the app is a single page at the scope root: any other address inside the scope (an old
      // bookmark, a mistyped path) goes there, so relative URLs keep working offline
      return Response.redirect(new URL(`./${url.search}`, scopeUrl()).href, 302);
    }
    if (key === GUIDES_PATH) {
      const g = await guidesResponse(st);
      if (g) return g;
      return fetch(req);
    }
    const hit = await fromActive(st, key);
    if (hit) return hit;
    try {
      return await fetch(req);
    } catch (e) {
      // offline and not in the active build: any cached copy (e.g. from the previous worker's
      // per-version precache, kept until a complete build has replaced it), then the app shell
      const any = await caches.match(req, { ignoreSearch: true });
      if (any) return any;
      if (navigate) {
        const index = (await fromActive(st, 'index.html')) || (await caches.match(new URL('index.html', scopeUrl()).href, { ignoreSearch: true }));
        if (index) return index;
      }
      throw e;
    }
  })());
});

self.addEventListener('notificationclick', (event) => {
  event.notification.close();
  event.waitUntil((async () => {
    const all = await self.clients.matchAll({ type: 'window', includeUncontrolled: true });
    if (all.length) return all[0].focus();
    return self.clients.openWindow('./');
  })());
});
