// Compact WebRTC pairing blobs for fully manual signalling (QR code or copy-paste, no server).
//
// A browser's offer/answer SDP is ~1-3 KB. Everything a data-channel-only session needs is a
// handful of fields, so we pack exactly those into ~80-150 bytes and rebuild a minimal SDP on the
// other side:
//
//   "ember:rtc:" + base64url( bytes )
//   bytes = [hdr] [ufrag len+ascii] [pwd len+ascii] [fp alg] [fp bytes] [mid len+ascii]
//           [sctp-port u16] [max-message-size u32] [n] n x candidate [crc16 u16]
//   hdr   = version(4 bits, =1) << 4 | is_answer << 2 | setup (0 actpass, 1 active, 2 passive)
//   candidate = flags [addr] [port u16]
//     flags = type (bits 0-1: host, srflx, prflx, relay) | addr kind (bits 2-3: ipv4, ipv6,
//             mdns uuid, hostname string) << 2 | tcp passive (bit 4)
//
// Only component-1 candidates are kept (data channels are always rtcp-mux / BUNDLE), at most
// MAX_CANDIDATES of them, IPv4 host candidates first: they are what works on a LAN or phone hotspot
// with no STUN server.

import { bytesToB64, b64ToBytes, crc16 } from './framing.js';

export const BLOB_PREFIX = 'ember:rtc:';
export const MAX_CANDIDATES = 8;
const VERSION = 1;
const SETUPS = ['actpass', 'active', 'passive'];
const TYPES = ['host', 'srflx', 'prflx', 'relay'];
const FP_ALGS = { 'sha-256': [1, 32], 'sha-384': [2, 48], 'sha-512': [3, 64] };
const FP_BY_ID = { 1: ['sha-256', 32], 2: ['sha-384', 48], 3: ['sha-512', 64] };
const TYPE_PREF = { host: 126, prflx: 110, srflx: 100, relay: 0 };

export class BlobError extends Error {
  constructor(message) { super(message); this.code = 'bad_params'; }
}

/** Extracts the data-channel essentials from an SDP string. */
export function parseSdp(sdp) {
  const out = { ufrag: '', pwd: '', fingerprint: null, setup: 'actpass', mid: '0', sctpPort: 5000, maxMessageSize: 0, candidates: [] };
  const seen = new Set();
  for (const raw of String(sdp).split(/\r?\n/)) {
    const line = raw.trim();
    if (!line.startsWith('a=')) continue;
    const colon = line.indexOf(':');
    const key = colon < 0 ? line.slice(2) : line.slice(2, colon);
    const val = colon < 0 ? '' : line.slice(colon + 1);
    switch (key) {
      case 'ice-ufrag': out.ufrag ||= val; break;
      case 'ice-pwd': out.pwd ||= val; break;
      case 'fingerprint': {
        const [alg, hex] = val.split(/\s+/);
        if (!out.fingerprint && alg && hex) out.fingerprint = { alg: alg.toLowerCase(), hex: hex.toUpperCase() };
        break;
      }
      case 'setup': out.setup = val; break;
      case 'mid': out.mid = val; break;
      case 'sctp-port': out.sctpPort = Number(val) || 5000; break;
      case 'max-message-size': out.maxMessageSize = Number(val) || 0; break;
      case 'candidate': {
        const c = parseCandidate(val);
        if (c && !seen.has(`${c.address}|${c.port}|${c.tcp}`)) { seen.add(`${c.address}|${c.port}|${c.tcp}`); out.candidates.push(c); }
        break;
      }
      default: break;
    }
  }
  return out;
}

/** Parses "candidate:<foundation> <component> <transport> <priority> <address> <port> typ <type> ..." */
export function parseCandidate(str) {
  const s = String(str).replace(/^a=/, '').replace(/^candidate:/, '');
  const p = s.trim().split(/\s+/);
  if (p.length < 8 || p[6] !== 'typ') return null;
  const component = Number(p[1]);
  const transport = p[2].toLowerCase();
  const port = Number(p[5]);
  const type = p[7];
  if (component !== 1 || !TYPES.includes(type) || !(port > 0 && port < 65536)) return null;
  let tcp = false;
  if (transport === 'tcp') {
    const i = p.indexOf('tcptype');
    if (i < 0 || p[i + 1] !== 'passive') return null; // only passive TCP candidates are reachable
    tcp = true;
  } else if (transport !== 'udp') return null;
  return { address: p[4], port, type, tcp };
}

function addrKind(address) {
  if (/^\d{1,3}(\.\d{1,3}){3}$/.test(address)) return 0;
  if (address.includes(':')) return 1;
  if (/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\.local$/i.test(address)) return 2;
  return 3;
}

function ipv6ToBytes(a) {
  let s = a.replace(/^\[|\]$/g, '').replace(/%.*$/, '');
  let tail = [];
  const v4 = s.match(/(\d+\.\d+\.\d+\.\d+)$/);
  if (v4) {
    const q = v4[1].split('.').map(Number);
    tail = [((q[0] << 8) | q[1]).toString(16), ((q[2] << 8) | q[3]).toString(16)];
    s = s.slice(0, -v4[1].length).replace(/:$/, ':') + tail.join(':');
  }
  const [head, rest] = s.split('::');
  const h = head ? head.split(':') : [];
  const r = rest !== undefined ? (rest ? rest.split(':') : []) : [];
  const groups = rest !== undefined ? [...h, ...Array(8 - h.length - r.length).fill('0'), ...r] : h;
  if (groups.length !== 8) throw new BlobError(`bad IPv6 address ${a}`);
  const out = new Uint8Array(16);
  groups.forEach((g, i) => { const v = parseInt(g, 16) || 0; out[i * 2] = v >> 8; out[i * 2 + 1] = v & 255; });
  return out;
}

function bytesToIpv6(b) {
  const g = [];
  for (let i = 0; i < 16; i += 2) g.push(((b[i] << 8) | b[i + 1]).toString(16));
  // compress the longest run of zeros
  let best = -1, bestLen = 0;
  for (let i = 0; i < 8;) {
    if (g[i] !== '0') { i++; continue; }
    let j = i; while (j < 8 && g[j] === '0') j++;
    if (j - i > bestLen && j - i > 1) { best = i; bestLen = j - i; }
    i = j;
  }
  if (best < 0) return g.join(':');
  return `${g.slice(0, best).join(':')}::${g.slice(best + bestLen).join(':')}`;
}

function rankCandidate(c) {
  const k = addrKind(c.address);
  const typeRank = { host: 0, srflx: 1, prflx: 2, relay: 3 }[c.type];
  const kindRank = [0, 2, 1, 3][k];          // ipv4, mdns, ipv6, hostname
  return (c.tcp ? 100 : 0) + typeRank * 10 + kindRank;
}

class Writer {
  constructor() { this.b = []; }
  u8(v) { this.b.push(v & 255); }
  u16(v) { this.u8(v >> 8); this.u8(v); }
  u32(v) { this.u16(Math.floor(v / 65536)); this.u16(v & 0xffff); }
  str(s) {
    const e = new TextEncoder().encode(s);
    if (e.length > 255) throw new BlobError('field too long');
    this.u8(e.length); for (const x of e) this.u8(x);
  }
  bytes(a) { for (const x of a) this.u8(x); }
  done() { return Uint8Array.from(this.b); }
}

class Reader {
  constructor(b) { this.b = b; this.i = 0; }
  need(n) { if (this.i + n > this.b.length) throw new BlobError('pairing code is truncated'); }
  u8() { this.need(1); return this.b[this.i++]; }
  u16() { return (this.u8() << 8) | this.u8(); }
  u32() { return this.u16() * 65536 + this.u16(); }
  str() { const n = this.u8(); this.need(n); const s = new TextDecoder().decode(this.b.subarray(this.i, this.i + n)); this.i += n; return s; }
  bytes(n) { this.need(n); const s = this.b.subarray(this.i, this.i + n); this.i += n; return s; }
}

/** SDP string -> compact pairing blob text. kind: 'offer' | 'answer'. */
export function encodeBlob(sdp, kind) {
  const d = typeof sdp === 'string' ? parseSdp(sdp) : sdp;
  if (!d.ufrag || !d.pwd) throw new BlobError('SDP has no ICE credentials');
  if (!d.fingerprint || !FP_ALGS[d.fingerprint.alg]) throw new BlobError('SDP has no supported DTLS fingerprint');
  const [algId, fpLen] = FP_ALGS[d.fingerprint.alg];
  const fp = d.fingerprint.hex.split(':').map((h) => parseInt(h, 16));
  if (fp.length !== fpLen || fp.some((x) => !(x >= 0 && x < 256))) throw new BlobError('bad DTLS fingerprint');
  const setup = Math.max(0, SETUPS.indexOf(d.setup));
  const w = new Writer();
  w.u8((VERSION << 4) | ((kind === 'answer' ? 1 : 0) << 2) | setup);
  w.str(d.ufrag);
  w.str(d.pwd);
  w.u8(algId);
  w.bytes(fp);
  w.str(d.mid || '0');
  w.u16(d.sctpPort || 5000);
  w.u32(d.maxMessageSize || 0);
  const cands = [...d.candidates].sort((a, b) => rankCandidate(a) - rankCandidate(b)).slice(0, MAX_CANDIDATES);
  w.u8(cands.length);
  for (const c of cands) {
    const k = addrKind(c.address);
    w.u8(TYPES.indexOf(c.type) | (k << 2) | (c.tcp ? 16 : 0));
    if (k === 0) w.bytes(c.address.split('.').map(Number));
    else if (k === 1) w.bytes(ipv6ToBytes(c.address));
    else if (k === 2) w.bytes(c.address.slice(0, 36).replace(/-/g, '').match(/../g).map((h) => parseInt(h, 16)));
    else w.str(c.address);
    w.u16(c.port);
  }
  const body = w.done();
  const crc = crc16(body);
  const all = new Uint8Array(body.length + 2);
  all.set(body); all[body.length] = crc >> 8; all[body.length + 1] = crc & 255;
  return BLOB_PREFIX + bytesToB64(all, { url: true });
}

/** Pairing blob text -> {kind, setup, ufrag, pwd, fingerprint, mid, sctpPort, maxMessageSize, candidates}. */
export function decodeBlob(text) {
  let s = String(text || '').replace(/\s+/g, '');
  if (s.toLowerCase().startsWith(BLOB_PREFIX)) s = s.slice(BLOB_PREFIX.length);
  if (!s) throw new BlobError('empty pairing code');
  let all;
  try { all = b64ToBytes(s); } catch { throw new BlobError('pairing code contains invalid characters'); }
  if (all.length < 8) throw new BlobError('pairing code is truncated');
  const body = all.subarray(0, all.length - 2);
  const crc = (all[all.length - 2] << 8) | all[all.length - 1];
  if (crc16(body) !== crc) throw new BlobError('pairing code is damaged (checksum mismatch) — copy it again');
  const r = new Reader(body);
  const hdr = r.u8();
  if (hdr >> 4 !== VERSION) throw new BlobError(`unsupported pairing code version ${hdr >> 4}`);
  const kind = hdr & 4 ? 'answer' : 'offer';
  const setup = SETUPS[hdr & 3];
  if (!setup) throw new BlobError('bad pairing code header');
  const ufrag = r.str();
  const pwd = r.str();
  const fpInfo = FP_BY_ID[r.u8()];
  if (!fpInfo) throw new BlobError('unknown fingerprint algorithm');
  const fpb = r.bytes(fpInfo[1]);
  const hex = Array.from(fpb, (x) => x.toString(16).toUpperCase().padStart(2, '0')).join(':');
  const mid = r.str();
  const sctpPort = r.u16();
  const maxMessageSize = r.u32();
  const n = r.u8();
  if (n > 32) throw new BlobError('too many candidates');
  const candidates = [];
  for (let i = 0; i < n; i++) {
    const f = r.u8();
    const type = TYPES[f & 3];
    const k = (f >> 2) & 3;
    let address;
    if (k === 0) address = Array.from(r.bytes(4)).join('.');
    else if (k === 1) address = bytesToIpv6(r.bytes(16));
    else if (k === 2) {
      const h = Array.from(r.bytes(16), (x) => x.toString(16).padStart(2, '0')).join('');
      address = `${h.slice(0, 8)}-${h.slice(8, 12)}-${h.slice(12, 16)}-${h.slice(16, 20)}-${h.slice(20)}.local`;
    } else address = r.str();
    const port = r.u16();
    candidates.push({ type, address, port, tcp: !!(f & 16) });
  }
  if (r.i !== body.length) throw new BlobError('pairing code has trailing data');
  if (!ufrag || !pwd) throw new BlobError('pairing code has no ICE credentials');
  return { kind, setup, ufrag, pwd, fingerprint: { alg: fpInfo[0], hex }, mid, sctpPort, maxMessageSize, candidates };
}

/** Builds a minimal, standards-conformant data-channel SDP from decoded blob fields. */
export function buildSdp(d, { sessionId } = {}) {
  const sid = sessionId || String(Math.floor(Math.random() * 1e15) + 1e15);
  const L = [
    'v=0',
    `o=- ${sid} 2 IN IP4 127.0.0.1`,
    's=-',
    't=0 0',
    `a=group:BUNDLE ${d.mid}`,
    'a=msid-semantic: WMS',
    'm=application 9 UDP/DTLS/SCTP webrtc-datachannel',
    'c=IN IP4 0.0.0.0',
  ];
  d.candidates.forEach((c, i) => {
    const pref = TYPE_PREF[c.type] ?? 0;
    const local = 65535 - i * 64 - (c.tcp ? 4096 : 0);
    const priority = pref * 16777216 + Math.max(0, local) * 256 + 255;
    let line = `a=candidate:${i + 1} 1 ${c.tcp ? 'tcp' : 'udp'} ${priority} ${c.address} ${c.port} typ ${c.type}`;
    if (c.type !== 'host') line += ' raddr 0.0.0.0 rport 0';
    if (c.tcp) line += ' tcptype passive';
    L.push(line);
  });
  L.push('a=end-of-candidates');
  L.push(`a=ice-ufrag:${d.ufrag}`, `a=ice-pwd:${d.pwd}`, 'a=ice-options:trickle');
  L.push(`a=fingerprint:${d.fingerprint.alg} ${d.fingerprint.hex}`);
  L.push(`a=setup:${d.setup}`, `a=mid:${d.mid}`, `a=sctp-port:${d.sctpPort || 5000}`);
  if (d.maxMessageSize) L.push(`a=max-message-size:${d.maxMessageSize}`);
  return L.join('\r\n') + '\r\n';
}
