// Common interface of every browser endpoint (a byte or PCM pipe underneath a core link).
//
//   const ep = mod.create(config, ctx);  // ctx: {env, audioRx?, audioTx?, log}
//   await ep.prepare();                  // user-gesture work: device pickers / permissions
//   ep.onData(bytes => ...);             // bytes from the medium (one datagram per call if ep.datagram)
//   ep.onState((state, error) => ...);   // 'down' | 'connecting' | 'up' | 'error'
//   await ep.open();  ep.send(bytes);  ep.setPtt(on);  await ep.close();
//   ep.describe()                        // {kind, config} to persist (may include device ids learnt in prepare)
//
// Each endpoint module (websocket.js, webrtc.js, ...) exports a descriptor:
//   {kind, title, description, datagram, scannable, autoRestore, linkTypes, defaultLinkType,
//    detect(env) -> {available, reason?}, config(env) -> [Field], scan?(params, env), create(config, ctx)}

export const STATES = Object.freeze(['down', 'connecting', 'up', 'error']);

export class Endpoint {
  constructor(kind, config = {}, ctx = {}) {
    this.kind = kind;
    this.config = { ...config };
    this.ctx = ctx;
    this.env = ctx.env || globalThis;
    this.state = 'down';
    this.error = undefined;
    this.closed = false;
    this._data = new Set();
    this._states = new Set();
  }

  /** True when one send()/onData() call is exactly one mesh frame (WebSocket, WebRTC, BroadcastChannel). */
  get datagram() { return true; }

  onData(cb) { this._data.add(cb); return () => this._data.delete(cb); }
  onState(cb) { this._states.add(cb); return () => this._states.delete(cb); }

  async prepare() {}
  async open() {}
  async close() { this.closed = true; this._setState('down'); }
  send(_bytes) { return false; }
  setPtt(_on) {}
  describe() { return { kind: this.kind, config: { ...this.config } }; }

  _emitData(bytes) {
    for (const cb of this._data) {
      try { cb(bytes); } catch (e) { this._log('error', 'endpoint data handler failed', e); }
    }
  }

  _setState(state, error) {
    if (this.state === state && this.error === error) return;
    this.state = state;
    this.error = error;
    for (const cb of this._states) {
      try { cb(state, error); } catch (e) { this._log('error', 'endpoint state handler failed', e); }
    }
  }

  _log(level, ...args) {
    const log = this.ctx.log || console;
    (log[level] || log.log).call(log, `[${this.kind}]`, ...args);
  }
}

/** Reasons shown to the user when a capability is missing (RPC host.info unavailable_reason). */
export function secureContextReason(env, what) {
  return `${what} needs a secure page: HTTPS, localhost, or the single-file version opened from disk`;
}

export function isSecure(env) {
  if (typeof env.isSecureContext === 'boolean') return env.isSecureContext;
  return true; // Node / tests
}

/** Common Field helpers (RPC_API §1 Field schema). */
export const field = {
  text: (key, label, extra = {}) => ({ key, label, type: 'text', ...extra }),
  number: (key, label, extra = {}) => ({ key, label, type: 'number', ...extra }),
  bool: (key, label, extra = {}) => ({ key, label, type: 'bool', ...extra }),
  select: (key, label, options, extra = {}) => ({
    key, label, type: 'select',
    options: options.map((o) => (typeof o === 'object' ? o : { value: o, label: String(o) })),
    ...extra,
  }),
};

/** Apply schema defaults and coerce types; unknown keys are kept (opaque to us). */
export function applyDefaults(schema, config = {}) {
  const out = { ...config };
  for (const f of schema) {
    if (out[f.key] === undefined || out[f.key] === '') {
      if (f.default !== undefined) out[f.key] = f.default;
      continue;
    }
    if (f.type === 'number') {
      const n = Number(out[f.key]);
      out[f.key] = Number.isFinite(n) ? n : f.default;
    } else if (f.type === 'bool') {
      out[f.key] = out[f.key] === true || out[f.key] === 'true' || out[f.key] === 1;
    }
  }
  return out;
}
