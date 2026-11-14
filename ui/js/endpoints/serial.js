// Web Serial endpoint: USB serial radios and modems — KISS TNCs, Meshtastic, RNode, LoRa AT
// modules, Mobilinkd/Digirig style interfaces. Byte stream (the core link driver does framing).
// Includes line control: DTR/RTS levels on open and PTT keying on `link.ptt` events.
import { Endpoint, field, applyDefaults, isSecure } from './base.js';
import { WriteQueue, toBytes } from './framing.js';

export const USB_VENDORS = {
  0x0403: 'FTDI', 0x10c4: 'Silicon Labs CP210x', 0x1a86: 'QinHeng CH34x', 0x067b: 'Prolific',
  0x303a: 'Espressif', 0x239a: 'Adafruit', 0x2e8a: 'Raspberry Pi', 0x2341: 'Arduino',
  0x1915: 'Nordic', 0x0483: 'STMicro', 0x16c0: 'Teensy/V-USB', 0x1209: 'pid.codes',
};

export const BAUD_RATES = [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600];

function schema() {
  return [
    field.select('baud', 'Baud rate', BAUD_RATES, {
      default: 9600, unit: 'bd',
      help: 'KISS TNCs: usually 9600 (some 1200/19200/57600). Meshtastic and RNode: 115200.',
    }),
    field.select('flow', 'Flow control', [{ value: 'none', label: 'None' }, { value: 'hardware', label: 'Hardware (RTS/CTS)' }], { default: 'none', advanced: true }),
    field.select('data_bits', 'Data bits', [8, 7], { default: 8, advanced: true }),
    field.select('parity', 'Parity', ['none', 'even', 'odd'], { default: 'none', advanced: true }),
    field.select('stop_bits', 'Stop bits', [1, 2], { default: 1, advanced: true }),
    field.bool('dtr', 'Assert DTR on open', { default: true, advanced: true, help: 'Some ESP32 boards reset when DTR/RTS toggle.' }),
    field.bool('rts', 'Assert RTS on open', { default: false, advanced: true }),
    field.select('ptt', 'PTT keying line', [
      { value: 'none', label: 'None (TNC/radio keys itself, or VOX)' },
      { value: 'rts', label: 'RTS' },
      { value: 'dtr', label: 'DTR' },
    ], { default: 'none', help: 'Line that keys the transmitter on link.ptt events.' }),
    field.bool('ptt_invert', 'Invert PTT line', { default: false, advanced: true }),
  ];
}

export function portLabel(info = {}) {
  const v = info.usbVendorId, p = info.usbProductId;
  if (v === undefined) return info.bluetoothServiceClassId ? 'Bluetooth serial port' : 'Serial port';
  const hex = (x) => x.toString(16).padStart(4, '0');
  return `${USB_VENDORS[v] || 'USB serial'} (${hex(v)}:${hex(p ?? 0)})`;
}

/** Signals to set for a PTT state, given config {ptt, ptt_invert, dtr, rts}. */
export function pttSignals(config, on) {
  const level = config.ptt_invert ? !on : on;
  if (config.ptt === 'rts') return { requestToSend: level };
  if (config.ptt === 'dtr') return { dataTerminalReady: level };
  return null;
}

/** Finds an already-granted port that matches stored USB ids (used for auto-restore). */
export async function findGrantedPort(serial, config) {
  if (!serial || !serial.getPorts) return null;
  const ports = await serial.getPorts();
  if (config.usb_vendor_id === undefined) return ports.length === 1 ? ports[0] : null;
  const matches = ports.filter((p) => {
    const i = p.getInfo ? p.getInfo() : {};
    return i.usbVendorId === config.usb_vendor_id && i.usbProductId === config.usb_product_id;
  });
  return matches[Math.min(matches.length - 1, config.port_index || 0)] || null;
}

class SerialEndpoint extends Endpoint {
  constructor(config, ctx) {
    super('serial', applyDefaults(schema(), config), ctx);
    this.port = null;
    this._reader = null;
    this._writer = null;
    this._queue = new WriteQueue((b) => this._writer.write(b));
    this._queue.onError = (e) => this._fail(e);
  }

  get datagram() { return false; }

  async prepare({ interactive = true } = {}) {
    const serial = this.env.navigator && this.env.navigator.serial;
    if (!serial) throw Object.assign(new Error('Web Serial unavailable'), { code: 'unsupported' });
    let port = this.port || (await findGrantedPort(serial, this.config));
    if (!port) {
      if (!interactive) throw Object.assign(new Error('serial device not connected or permission not granted'), { code: 'not_found' });
      port = await serial.requestPort({}).catch((e) => {
        throw Object.assign(new Error(e && e.name === 'NotFoundError' ? 'No serial port chosen' : String(e.message || e)), { code: 'denied' });
      });
    }
    this.port = port;
    const info = port.getInfo ? port.getInfo() : {};
    if (info.usbVendorId !== undefined) {
      this.config.usb_vendor_id = info.usbVendorId;
      this.config.usb_product_id = info.usbProductId;
    }
    this.config.device_name = portLabel(info);
  }

  async open() {
    this.closed = false;
    if (!this.port) await this.prepare({ interactive: false });
    this._setState('connecting');
    const c = this.config;
    try {
      await this.port.open({
        baudRate: Number(c.baud), dataBits: Number(c.data_bits), stopBits: Number(c.stop_bits),
        parity: c.parity, flowControl: c.flow, bufferSize: 16384,
      });
    } catch (e) {
      // InvalidStateError: already open (e.g. reconnect) — continue.
      if (!(e && e.name === 'InvalidStateError')) { this._fail(e); throw e; }
    }
    try {
      const sig = { dataTerminalReady: !!c.dtr, requestToSend: !!c.rts };
      Object.assign(sig, pttSignals(c, false) || {});
      await this.port.setSignals(sig);
    } catch (e) { this._log('warn', 'setSignals failed', e); }
    this._writer = this.port.writable.getWriter();
    this._setState('up');
    this._readLoop();
    this._onDisconnect = (ev) => { if (ev.target === this.port) this._fail(new Error('device unplugged')); };
    if (this.env.navigator.serial.addEventListener) this.env.navigator.serial.addEventListener('disconnect', this._onDisconnect);
  }

  async _readLoop() {
    while (!this.closed && this.port && this.port.readable) {
      const reader = this.port.readable.getReader();
      this._reader = reader;
      try {
        for (;;) {
          const { value, done } = await reader.read();
          if (done) break;
          if (value && value.length) this._emitData(value);
        }
      } catch (e) {
        // Framing/parity/buffer-overrun errors are recoverable: loop gets a new reader.
        if (this.closed) break;
        if (e && (e.name === 'NetworkError' || e.name === 'NotFoundError')) { this._fail(e); break; }
        this._log('warn', 'serial read error', e && e.name);
      } finally {
        try { reader.releaseLock(); } catch {}
        this._reader = null;
      }
    }
  }

  send(bytes) {
    if (this.state !== 'up' || !this._writer) return false;
    return this._queue.push(toBytes(bytes).slice());
  }

  async setPtt(on) {
    const sig = pttSignals(this.config, on);
    if (!sig || !this.port) return;
    try { await this.port.setSignals(sig); } catch (e) { this._log('warn', 'PTT setSignals failed', e); }
  }

  _fail(e) {
    if (this.closed) return;
    this._setState('error', String((e && e.message) || e || 'serial error'));
    this._teardown();
  }

  async _teardown() {
    this._queue.clear();
    if (this._onDisconnect && this.env.navigator.serial.removeEventListener) {
      this.env.navigator.serial.removeEventListener('disconnect', this._onDisconnect);
    }
    try { if (this._reader) await this._reader.cancel(); } catch {}
    try { if (this._writer) { this._writer.releaseLock(); } } catch {}
    this._writer = null;
    try { if (this.port) await this.port.close(); } catch {}
  }

  async close() {
    this.closed = true;
    try { await this.setPtt(false); } catch {}
    await this._teardown();
    this._setState('down');
  }
}

export default {
  kind: 'serial',
  title: 'USB serial (radio, TNC, LoRa)',
  description: 'KISS TNCs, Meshtastic and RNode LoRa devices, LoRa AT modules and radio interfaces on a USB serial port.',
  datagram: false,
  scannable: true,
  autoRestore: true,
  linkTypes: ['kiss', 'meshtastic', 'rnode', 'lora_at', 'stream'],
  defaultLinkType: 'kiss',
  detect(env) {
    const nav = env.navigator || {};
    if (!nav.serial) {
      if (!isSecure(env)) return { available: false, reason: 'Web Serial needs Chrome/Edge on a computer and a secure page (HTTPS, localhost, or the single-file version opened from disk)' };
      return { available: false, reason: 'Web Serial needs Chrome/Edge on a computer (not available in Firefox, Safari or on iOS)' };
    }
    return { available: true };
  },
  config: schema,
  async scan(params = {}, env = globalThis) {
    const serial = env.navigator && env.navigator.serial;
    if (!serial) return { devices: [] };
    let ports = await serial.getPorts();
    if (params.request || ports.length === 0) {
      try { const p = await serial.requestPort({}); if (!ports.includes(p)) ports = [...ports, p]; } catch {}
    }
    const counts = {};
    return {
      devices: ports.map((p, i) => {
        const info = p.getInfo ? p.getInfo() : {};
        const key = `${info.usbVendorId}:${info.usbProductId}`;
        const idx = (counts[key] = (counts[key] ?? -1) + 1);
        return {
          id: `serial:${i}`, name: portLabel(info),
          detail: info.usbVendorId === undefined ? undefined : 'already permitted',
          config: info.usbVendorId === undefined ? {} : { usb_vendor_id: info.usbVendorId, usb_product_id: info.usbProductId, port_index: idx },
        };
      }),
    };
  },
  create: (config, ctx) => new SerialEndpoint(config, ctx),
};
