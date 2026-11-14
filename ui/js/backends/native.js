// NativeBackend — the UI runs in an Android WebView or iOS WKWebView next to the native C++ core.
// Bridge contract (docs/RPC_API.md §5):
//   JS -> native: postMessage(JSON.stringify({id, method, params}))
//        Android: window.EmberNative.postMessage(string)          (@JavascriptInterface)
//        iOS:     window.webkit.messageHandlers.ember.postMessage(string)
//   native -> JS: window.__emberBridge.onReply({id, ok, result?, error?})
//                 window.__emberBridge.onEvent({event, data})       (via evaluateJavascript)
// The native host router answers host.* itself and forwards everything else to ember_call.

function rpcError(code, message) {
  return Object.assign(new Error(message || code), { code });
}

/** Returns a function that sends a string to native, or null if no bridge exists. */
export function findBridge(env = globalThis) {
  if (env.EmberNative && typeof env.EmberNative.postMessage === 'function') {
    return { platform: 'android', post: (s) => env.EmberNative.postMessage(s) };
  }
  const h = env.webkit && env.webkit.messageHandlers && env.webkit.messageHandlers.ember;
  if (h && typeof h.postMessage === 'function') return { platform: 'ios', post: (s) => h.postMessage(s) };
  return null;
}

function parse(x) {
  if (typeof x === 'string') { try { return JSON.parse(x); } catch { return null; } }
  return x;
}

export default class NativeBackend {
  constructor(opts = {}) {
    this.kind = 'native';
    this.env = opts.env || globalThis;
    this.timeoutMs = opts.timeoutMs || 0; // 0 = no timeout (qr.scan, file.pick wait for the user)
    this.platform = null;
    this._bridge = null;
    this._next = 1;
    this._pending = new Map();
    this._listeners = new Map();
  }

  async init() {
    if (this._bridge) return this;
    const b = findBridge(this.env);
    if (!b) throw rpcError('unsupported', 'no native bridge (window.EmberNative / webkit.messageHandlers.ember)');
    this._bridge = b;
    this.platform = b.platform;
    const prev = this.env.__emberBridge || {};
    const self = this;
    // Keep any fields native code may have put there, but own the two callbacks.
    this.env.__emberBridge = Object.assign(prev, {
      onReply(msg) { self._onReply(parse(msg)); },
      onEvent(msg) { self._onEvent(parse(msg)); },
    });
    // Events native queued before the page was ready (optional, if the shell provides it).
    if (Array.isArray(prev.queued)) { const q = prev.queued.splice(0); for (const m of q) this._onEvent(parse(m)); }
    return this;
  }

  call(method, params) {
    if (!this._bridge) return this.init().then(() => this.call(method, params));
    const id = this._next++;
    return new Promise((resolve, reject) => {
      const entry = { resolve, reject, timer: 0 };
      if (this.timeoutMs > 0) {
        entry.timer = setTimeout(() => { this._pending.delete(id); reject(rpcError('busy', `${method} timed out`)); }, this.timeoutMs);
      }
      this._pending.set(id, entry);
      try {
        this._bridge.post(JSON.stringify({ id, method, params: params || {} }));
      } catch (e) {
        this._pending.delete(id);
        clearTimeout(entry.timer);
        reject(rpcError('internal', `native bridge failed: ${(e && e.message) || e}`));
      }
    });
  }

  on(event, handler) {
    if (!this._listeners.has(event)) this._listeners.set(event, new Set());
    this._listeners.get(event).add(handler);
    return () => { const s = this._listeners.get(event); if (s) s.delete(handler); };
  }

  _onReply(msg) {
    if (!msg) return;
    const p = this._pending.get(msg.id);
    if (!p) return;
    this._pending.delete(msg.id);
    clearTimeout(p.timer);
    if (msg.ok) p.resolve(msg.result === undefined ? {} : msg.result);
    else {
      const e = msg.error || {};
      p.reject(rpcError(e.code || 'internal', e.message || 'call failed'));
    }
  }

  _onEvent(msg) {
    if (!msg || typeof msg.event !== 'string') return;
    const data = msg.data || {};
    const run = (fn, ...a) => { try { fn(...a); } catch (e) { console.error(`[native backend] "${msg.event}" handler threw`, e); } };
    const s = this._listeners.get(msg.event);
    if (s) for (const fn of [...s]) run(fn, data);
    const all = this._listeners.get('*');
    if (all) for (const fn of [...all]) run(fn, msg.event, data);
  }
}
