// Loopback endpoint: a BroadcastChannel shared by every tab (and every node object) of this browser
// profile. Each message is one mesh frame. Used for demos ("open two tabs") and automated
// two-node tests; also works in Node.js, which implements BroadcastChannel.
import { Endpoint, field, applyDefaults } from './base.js';
import { toBytes } from './framing.js';

const PREFIX = 'embermesh-loopback:';

function schema() {
  return [
    field.text('channel', 'Channel name', {
      default: 'default', help: 'Tabs using the same channel name hear each other.',
    }),
  ];
}

let seq = 0;

class LoopbackEndpoint extends Endpoint {
  constructor(config, ctx) {
    super('loopback', applyDefaults(schema(), config), ctx);
    this._id = `${Date.now().toString(36)}-${(seq++).toString(36)}-${Math.random().toString(36).slice(2, 8)}`;
    this.bc = null;
  }

  async open() {
    this.closed = false;
    const BC = this.env.BroadcastChannel;
    this.bc = new BC(PREFIX + this.config.channel);
    this.bc.onmessage = (ev) => {
      const m = ev.data;
      if (!m || m.from === this._id || !(m.data instanceof ArrayBuffer || ArrayBuffer.isView(m.data))) return;
      this._emitData(toBytes(m.data).slice());
    };
    this._setState('up');
  }

  send(bytes) {
    if (!this.bc) return false;
    const b = toBytes(bytes);
    this.bc.postMessage({ from: this._id, data: b.slice().buffer });
    return true;
  }

  async close() {
    this.closed = true;
    if (this.bc) { this.bc.close(); this.bc = null; }
    this._setState('down');
  }
}

export default {
  kind: 'loopback',
  title: 'Loopback (other tabs)',
  description: 'Connects to other EmberMesh tabs in this same browser. For demos and testing.',
  datagram: true,
  scannable: false,
  autoRestore: true,
  linkTypes: ['raw'],
  defaultLinkType: 'raw',
  detect(env) {
    if (typeof env.BroadcastChannel !== 'function') {
      return { available: false, reason: 'This browser has no BroadcastChannel support' };
    }
    return { available: true };
  },
  config: schema,
  create: (config, ctx) => new LoopbackEndpoint(config, ctx),
};
