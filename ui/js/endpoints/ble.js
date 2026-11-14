// Web Bluetooth central endpoint: UART-style GATT byte streams.
//   - Ember peer service (phone running EmberMesh, ARCHITECTURE §6)
//   - Nordic UART Service (BLE KISS TNCs, serial bridges, many LoRa boards)
//   - custom service/characteristic UUIDs
// Stream semantics: notifications are concatenated by the core link driver; writes are chunked.
import { Endpoint, field, applyDefaults, isSecure } from './base.js';
import { WriteQueue, chunkBytes, toBytes, Backoff } from './framing.js';

export const EMBER_SERVICE = '8e3b0001-5d1f-4c6a-9b7e-0d2a6f1c4e10';
export const EMBER_RX = '8e3b0002-5d1f-4c6a-9b7e-0d2a6f1c4e10'; // we write
export const EMBER_TX = '8e3b0003-5d1f-4c6a-9b7e-0d2a6f1c4e10'; // we subscribe
export const NUS_SERVICE = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
export const NUS_RX = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';
export const NUS_TX = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';

export const PROFILES = {
  ember: { service: EMBER_SERVICE, rx: EMBER_RX, tx: EMBER_TX },
  nus: { service: NUS_SERVICE, rx: NUS_RX, tx: NUS_TX },
};

function schema() {
  return [
    field.select('profile', 'Device type', [
      { value: 'auto', label: 'Auto (EmberMesh phone or Nordic UART device)' },
      { value: 'ember', label: 'EmberMesh phone (peer service)' },
      { value: 'nus', label: 'Nordic UART (BLE TNC / serial bridge)' },
      { value: 'custom', label: 'Custom UUIDs' },
    ], { default: 'auto' }),
    field.text('name_prefix', 'Device name starts with', { default: '', advanced: true }),
    field.text('service_uuid', 'Service UUID', { default: '', advanced: true, help: 'Custom profile only.' }),
    field.text('rx_uuid', 'Write characteristic UUID', { default: '', advanced: true }),
    field.text('tx_uuid', 'Notify characteristic UUID', { default: '', advanced: true }),
    field.number('write_chunk', 'Write chunk size', { default: 20, min: 20, max: 512, unit: 'bytes', advanced: true, help: '20 works with every device; larger is faster if the device negotiated a big MTU.' }),
    field.bool('reconnect', 'Reconnect automatically', { default: true, advanced: true }),
  ];
}

/** Service UUIDs and requestDevice() options for a config. */
export function requestOptions(config) {
  const p = config.profile || 'auto';
  let services;
  if (p === 'custom') services = [String(config.service_uuid || '').toLowerCase()].filter(Boolean);
  else if (p === 'auto') services = [EMBER_SERVICE, NUS_SERVICE];
  else services = [PROFILES[p].service];
  if (!services.length) throw Object.assign(new Error('custom profile needs a service UUID'), { code: 'bad_params' });
  const filters = services.map((s) => (config.name_prefix ? { services: [s], namePrefix: config.name_prefix } : { services: [s] }));
  return { filters, optionalServices: services };
}

class BleEndpoint extends Endpoint {
  constructor(config, ctx) {
    super('ble', applyDefaults(schema(), config), ctx);
    this.device = null;
    this.rx = null;
    this.tx = null;
    this._backoff = new Backoff(2000, 60000);
    this._timer = null;
    this._queue = new WriteQueue((b) => this._write(b), { maxBytes: 64 * 1024 });
    this._queue.onError = (e) => this._log('warn', 'BLE write failed', e && e.message);
    this._onNotify = (ev) => {
      const v = ev.target.value;
      this._emitData(new Uint8Array(v.buffer.slice(v.byteOffset, v.byteOffset + v.byteLength)));
    };
    this._onGone = () => this._disconnected();
  }

  get datagram() { return false; }

  async prepare({ interactive = true } = {}) {
    const bt = this.env.navigator && this.env.navigator.bluetooth;
    if (!bt) throw Object.assign(new Error('Web Bluetooth unavailable'), { code: 'unsupported' });
    if (this.device) return;
    if (this.config.device_id && bt.getDevices) {
      const all = await bt.getDevices().catch(() => []);
      this.device = all.find((d) => d.id === this.config.device_id) || null;
    }
    if (!this.device) {
      if (!interactive) throw Object.assign(new Error('Bluetooth device not permitted; pair it again'), { code: 'not_found' });
      this.device = await bt.requestDevice(requestOptions(this.config)).catch((e) => {
        throw Object.assign(new Error(e && e.name === 'NotFoundError' ? 'No Bluetooth device chosen' : String(e.message || e)), { code: 'denied' });
      });
    }
    this.config.device_id = this.device.id;
    this.config.device_name = this.device.name || 'Bluetooth device';
    this.device.addEventListener('gattserverdisconnected', this._onGone);
  }

  async open() {
    this.closed = false;
    if (!this.device) await this.prepare({ interactive: false });
    await this._connect();
  }

  async _connect() {
    if (this.closed) return;
    this._setState('connecting');
    try {
      const server = await this.device.gatt.connect();
      const ids = await this._resolveService(server);
      this.rx = await ids.service.getCharacteristic(ids.rx);
      this.tx = await ids.service.getCharacteristic(ids.tx);
      this.tx.addEventListener('characteristicvaluechanged', this._onNotify);
      await this.tx.startNotifications();
      this._backoff.reset();
      this._setState('up');
    } catch (e) {
      this._setState('error', String((e && e.message) || e));
      this._scheduleReconnect();
    }
  }

  async _resolveService(server) {
    const p = this.config.profile;
    const candidates = p === 'custom'
      ? [{ service: this.config.service_uuid.toLowerCase(), rx: this.config.rx_uuid.toLowerCase(), tx: this.config.tx_uuid.toLowerCase() }]
      : p === 'auto' ? [PROFILES.ember, PROFILES.nus] : [PROFILES[p]];
    let lastErr;
    for (const c of candidates) {
      try {
        const service = await server.getPrimaryService(c.service);
        return { service, rx: c.rx, tx: c.tx };
      } catch (e) { lastErr = e; }
    }
    throw lastErr || new Error('device does not offer a supported service');
  }

  async _write(bytes) {
    const ch = this.rx;
    if (!ch) return;
    const noResp = ch.properties && ch.properties.writeWithoutResponse && ch.writeValueWithoutResponse;
    for (const piece of chunkBytes(bytes, Math.max(20, Number(this.config.write_chunk) || 20))) {
      if (noResp) await ch.writeValueWithoutResponse(piece);
      else if (ch.writeValueWithResponse) await ch.writeValueWithResponse(piece);
      else await ch.writeValue(piece);
    }
  }

  send(bytes) {
    if (this.state !== 'up') return false;
    return this._queue.push(toBytes(bytes).slice());
  }

  _disconnected() {
    this.rx = this.tx = null;
    this._queue.clear();
    if (this.closed) { this._setState('down'); return; }
    this._setState('error', 'Bluetooth device disconnected');
    this._scheduleReconnect();
  }

  _scheduleReconnect() {
    if (this.closed || !this.config.reconnect) return;
    clearTimeout(this._timer);
    this._timer = setTimeout(() => this._connect(), this._backoff.next());
  }

  async close() {
    this.closed = true;
    clearTimeout(this._timer);
    this._queue.clear();
    try { if (this.tx) this.tx.removeEventListener('characteristicvaluechanged', this._onNotify); } catch {}
    try { if (this.device) { this.device.removeEventListener('gattserverdisconnected', this._onGone); this.device.gatt.disconnect(); } } catch {}
    this._setState('down');
  }
}

export default {
  kind: 'ble',
  title: 'Bluetooth LE',
  description: 'Bluetooth LE to an EmberMesh phone, a BLE KISS TNC or a Nordic-UART serial bridge.',
  datagram: false,
  scannable: true,
  autoRestore: true,
  linkTypes: ['stream', 'kiss', 'raw'],
  defaultLinkType: 'stream',
  detect(env) {
    const nav = env.navigator || {};
    if (!nav.bluetooth) {
      if (!isSecure(env)) return { available: false, reason: 'Web Bluetooth needs Chrome/Edge and a secure page (HTTPS, localhost, or the single-file version opened from disk)' };
      return { available: false, reason: 'Web Bluetooth needs Chrome/Edge on a computer or Android (not available in Firefox, Safari or on iOS)' };
    }
    return { available: true };
  },
  async detectAsync(env) {
    const bt = env.navigator && env.navigator.bluetooth;
    if (bt && bt.getAvailability && !(await bt.getAvailability().catch(() => true))) {
      return { available: false, reason: 'No Bluetooth adapter found, or Bluetooth is switched off' };
    }
    return this.detect(env);
  },
  config: schema,
  async scan(params = {}, env = globalThis) {
    const bt = env.navigator && env.navigator.bluetooth;
    if (!bt) return { devices: [] };
    let devices = bt.getDevices ? await bt.getDevices().catch(() => []) : [];
    if (params.request || devices.length === 0) {
      try {
        const d = await bt.requestDevice(requestOptions(applyDefaults(schema(), params.config || {})));
        if (!devices.find((x) => x.id === d.id)) devices = [...devices, d];
      } catch {}
    }
    return {
      devices: devices.map((d) => ({ id: `ble:${d.id}`, name: d.name || 'Bluetooth device', config: { device_id: d.id } })),
    };
  },
  create: (config, ctx) => new BleEndpoint(config, ctx),
};
