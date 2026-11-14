// Registers the offline service worker (static-site / PWA build only; not in the single file).
// Service workers need a secure context: HTTPS or localhost. A hub page on http://<lan-ip>:8437/
// is not one, so there the app still works but is not installable/offline-cached.
(function () {
  if (!('serviceWorker' in navigator) || !window.isSecureContext || location.protocol === 'file:') return;
  window.addEventListener('load', function () {
    navigator.serviceWorker.register('sw.js', { updateViaCache: 'none' }).catch(function (e) {
      console.warn('[pwa] service worker registration failed:', e && e.message);
    });
  });
})();
