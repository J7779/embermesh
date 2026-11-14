// WebSocket client endpoint: one binary message == one mesh frame (hub /ws, ARCHITECTURE §6).
import { Endpoint, field, applyDefaults } from './base.js';
import { Backoff, toBytes } from './framing.js';
import { isLanHost } from '../lib/connectivity.js';

/** RPC_API §11: a socket whose protocols contain "mqtt" is a relay socket. */
const RELAY_MAX_MESSAGE = 8192;

export function defaultHubUrl(env = globalThis) {
  const loc = env.location;
  if (loc && /^https?:$/.test(loc.protocol) && loc.host) {
    return `${loc.protocol === 'https:' ? 'wss' : 'ws'}://${loc.host}/ws`;
  }
  return 'ws://192.168.4.1:8437/ws';
}

function schema(env) {
  return [
    field.text('url', 'Hub WebSocket URL', {
      default: defaultHubUrl(env), required: true,
      help: 'ws://<hub address>:8437/ws — the hub shows its address on screen.',
    }),
    field.bool('reconnect', 'Reconnect automatically', { default: true, advanced: true }),
  ];
}

/**
 * Explains why a URL can't be opened from this page, or returns null. `relay` (RPC_API §11): a
 * relay link needs wss:// unless the user explicitly allowed plaintext for it.
 */
export function wsUrlProblem(url, env = globalThis, { relay = false, allowPlaintext = false } = {}) {
  let u;
  try { u = new URL(url); } catch { return 'Not a valid URL'; }
  if (u.protocol !== 'ws:' && u.protocol !== 'wss:') return 'URL must start with ws:// or wss://';
  if (relay && u.protocol === 'ws:' && !(allowPlaintext && isLanHost(u.hostname))) return 'An internet relay must use wss:// (encrypted). Plain ws:// is allowed only for a hub on your own network, added with "allow unencrypted" on.';
  const pageHttps = env.location && env.location.protocol === 'https:';
  const local = /^(localhost|127\.|\[::1\])/.test(u.host);
  if (pageHttps && u.protocol === 'ws:' && !local) {
    return 'Browsers block ws:// from an https:// page; open the hub page itself (http://<hub>:8437/) or use wss://';
  }
  return null;
}

class WebSocketEndpoint extends Endpoint {
  constructor(config, ctx) {
    super('websocket', applyDefaults(schema(ctx.env || globalThis), config), ctx);
    this.relay = Array.isArray(this.config.protocols) && this.config.protocols.includes('mqtt');
    // relays: 2 s … 5 min (±20 %), reset after 2 minutes connected (§11); hubs as before
    this._backoff = this.relay ? new Backoff(2000, 300000) : new Backoff(1000, 30000);
    this._stable = null;
    this._timer = null;
    this.ws = null;
  }

  async open() {
    this.closed = false;
    const protocols = Array.isArray(this.config.protocols) ? this.config.protocols.filter((p) => typeof p === 'string' && p) : [];
    const problem = wsUrlProblem(this.config.url, this.env, { relay: protocols.includes('mqtt'), allowPlaintext: this.config.allow_plaintext === true });
    if (problem) { this._setState('error', problem); throw Object.assign(new Error(problem), { code: 'bad_params' }); }
    this._connect();
  }

  _connect() {
    if (this.closed) return;
    const WS = this.env.WebSocket;
    this._setState('connecting');
    let ws;
    try {
      // RPC_API §11: protocols → Sec-WebSocket-Protocol (relay links use ["mqtt"]). TLS and the
      // certificate/host name check are the browser's; there is no way to skip them.
      const protocols = Array.isArray(this.config.protocols) ? this.config.protocols.filter((p) => typeof p === 'string' && p) : [];
      ws = protocols.length ? new WS(this.config.url, protocols) : new WS(this.config.url);
    } catch (e) {
      this._setState('error', String(e.message || e));
      this._scheduleReconnect();
      return;
    }
    ws.binaryType = 'arraybuffer';
    this.ws = ws;
    ws.onopen = () => {
      if (this.relay) { clearTimeout(this._stable); this._stable = setTimeout(() => this._backoff.reset(), 120000); } else this._backoff.reset();
      this._setState('up');
    };
    ws.onmessage = (ev) => {
      if (typeof ev.data === 'string') return; // hub /ws and relays are binary-only
      if (this.relay && ev.data.byteLength > RELAY_MAX_MESSAGE) return; // relay messages ≤ 8 KiB
      this._emitData(new Uint8Array(ev.data));
    };
    ws.onerror = () => {}; // followed by onclose; the browser already logs details
    ws.onclose = (ev) => {
      if (this.ws !== ws) return;
      clearTimeout(this._stable);
      this.ws = null;
      if (this.closed) { this._setState('down'); return; }
      this._setState('error', ev && ev.code && ev.code !== 1000 ? `connection closed (${ev.code})` : 'connection closed');
      this._scheduleReconnect();
    };
  }

  _scheduleReconnect() {
    if (this.closed || !this.config.reconnect) return;
    clearTimeout(this._timer);
    this._timer = setTimeout(() => this._connect(), this._backoff.next());
  }

  send(bytes) {
    const ws = this.ws;
    if (!ws || ws.readyState !== 1) return false;
    if (ws.bufferedAmount > 1024 * 1024) return false; // stalled socket: let the mesh retry later
    if (this.relay && bytes.length > RELAY_MAX_MESSAGE) return false;
    ws.send(toBytes(bytes));
    return true;
  }

  async close() {
    this.closed = true;
    clearTimeout(this._timer);
    clearTimeout(this._stable);
    const ws = this.ws;
    this.ws = null;
    if (ws) try { ws.close(1000); } catch {}
    this._setState('down');
  }
}

export default {
  kind: 'websocket',
  title: 'WebSocket (hub)',
  description: 'Connect to an EmberMesh hub over Wi-Fi/LAN (ws://<hub>:8437/ws).',
  datagram: true,
  scannable: false,
  autoRestore: true,
  linkTypes: ['raw'],
  defaultLinkType: 'raw',
  detect(env) {
    if (typeof env.WebSocket !== 'function') return { available: false, reason: 'This browser has no WebSocket support' };
    return { available: true };
  },
  config: schema,
  create: (config, ctx) => new WebSocketEndpoint(config, ctx),
};
