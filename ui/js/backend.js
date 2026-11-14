// Backend selection — docs/RPC_API.md §5.
//
//   const backend = await createBackend();
//   await backend.call('node.info');            // rejects with Error{code, message}
//   const off = backend.on('message.new', fn);   // '*' receives (event, data)
//   backend.kind                                 // "wasm" | "native" | "remote" | "mock"
//
// Selection order (first match wins), exactly as the spec table lists it:
//   native  window.EmberNative (Android) or window.webkit.messageHandlers.ember (iOS) exists
//   remote  URL has ?remote=1
//   mock    URL has ?mock=1
//   wasm    otherwise

/** Pure: decide which backend module to load. Exported for tests. */
export function pickBackendKind(win) {
  const w = win || {};
  if (w.EmberNative || (w.webkit && w.webkit.messageHandlers && w.webkit.messageHandlers.ember)) return 'native';
  let params;
  try { params = new URLSearchParams((w.location && w.location.search) || ''); } catch { params = new URLSearchParams(''); }
  if (params.get('remote') === '1') return 'remote';
  if (params.get('mock') === '1') return 'mock';
  return 'wasm';
}

export async function createBackend(kind) {
  const k = kind || pickBackendKind(typeof window !== 'undefined' ? window : globalThis);
  // Static specifiers per kind keep the import inlinable by single-file packagers.
  let mod;
  switch (k) {
    case 'native': mod = await import('./backends/native.js'); break;
    case 'remote': mod = await import('./backends/remote.js'); break;
    case 'mock': mod = await import('./backends/mock.js'); break;
    default: mod = await import('./backends/wasm.js'); break;
  }
  const Impl = mod.default;
  const backend = new Impl();
  await backend.init();
  return backend;
}
