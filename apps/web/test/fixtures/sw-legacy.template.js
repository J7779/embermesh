// Service worker template of the previous release (precache-per-version design), kept so the tests can
// check the upgrade path from it to the current apps/web/pwa/sw.js. Filled in like the old packager did.
// Precaches the whole static site so the app works with no network at all. The cache name is
// versioned by a content hash; a new build replaces the old cache on activation.
const VERSION = '__VERSION__';
const CACHE = `embermesh-${VERSION}`;
const PRECACHE = __PRECACHE__;
// The guide pack is not precached with the app shell (tens of MB; it would block installation and
// be re-downloaded by every app update). The page streams it once and stores it in its own
// content-addressed cache (GUIDES.cache); here it is served cache-first from there.
const GUIDES = __GUIDES__;
// Never served from cache: hub discovery and live hub endpoints (ARCHITECTURE §6).
const NETWORK_ONLY = [/\/ws$/, /\/rpc$/, /\/download\//];
const NETWORK_FIRST = [/\/hub\.json$/];

self.addEventListener('install', (event) => {
  event.waitUntil((async () => {
    const cache = await caches.open(CACHE);
    // cache: 'reload' bypasses the HTTP cache so a new version never precaches stale files
    await cache.addAll(PRECACHE.map((p) => new Request(p, { cache: 'reload' })));
    await self.skipWaiting();
  })());
});

self.addEventListener('activate', (event) => {
  event.waitUntil((async () => {
    for (const key of await caches.keys()) {
      if (!key.startsWith('embermesh-') || key === CACHE) continue;
      if (key.startsWith('embermesh-guides-') && GUIDES && key === GUIDES.cache) continue;
      await caches.delete(key);
    }
    await self.clients.claim();
  })());
});

self.addEventListener('fetch', (event) => {
  const req = event.request;
  if (req.method !== 'GET') return;
  const url = new URL(req.url);
  if (url.origin !== self.location.origin) return;
  if (NETWORK_ONLY.some((r) => r.test(url.pathname))) return;
  if (GUIDES && url.pathname.endsWith(`/${GUIDES.path}`)) {
    event.respondWith((async () => {
      const hit = await (await caches.open(GUIDES.cache)).match(req, { ignoreSearch: true });
      return hit || fetch(req); // the page puts what it fetched into GUIDES.cache
    })());
    return;
  }
  if (NETWORK_FIRST.some((r) => r.test(url.pathname))) {
    event.respondWith(fetch(req).catch(async () => (await caches.match(req, { ignoreSearch: true })) || Response.error()));
    return;
  }
  event.respondWith((async () => {
    const cache = await caches.open(CACHE);
    const key = req.mode === 'navigate' && url.pathname.endsWith('/') ? new URL('index.html', url).href : req;
    const hit = await cache.match(key, { ignoreSearch: true });
    if (hit) return hit;
    try {
      return await fetch(req);
    } catch (e) {
      if (req.mode === 'navigate') {
        const index = await cache.match('index.html', { ignoreSearch: true });
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
