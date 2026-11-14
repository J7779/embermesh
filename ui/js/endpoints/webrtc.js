// WebRTC data-channel endpoint with fully manual signalling (no signalling server, no STUN needed
// on a shared LAN / hotspot). Pairing:
//   A: createOffer()  -> blob shown as QR / text     (host.webrtc.create_offer)
//   B: acceptOffer()  -> answer blob shown back to A (host.webrtc.accept_offer)
//   A: acceptAnswer() -> data channel opens          (host.webrtc.accept_answer)
// ICE gathering is completed before a blob is produced, so the blob holds every candidate.
import { Endpoint, field } from './base.js';
import { toBytes } from './framing.js';
import { encodeBlob, decodeBlob, buildSdp, BlobError } from './webrtc_blob.js';

const GATHER_TIMEOUT_MS = 4000;
const GATHER_TIMEOUT_STUN_MS = 9000;
const GATHER_MAX_MS = 20000;

function schema() {
  return [
    field.text('stun', 'STUN server (optional)', {
      default: '', advanced: true,
      help: 'Only needed across different networks with internet, e.g. stun:stun.example.org:3478. Leave empty on a LAN/hotspot.',
    }),
  ];
}

export function iceServers(stun) {
  const s = String(stun || '').trim();
  if (!s) return [];
  return s.split(/[\s,]+/).filter(Boolean).map((u) => ({ urls: /^(stun|turn)s?:/.test(u) ? u : `stun:${u}` }));
}

/**
 * Waits until ICE gathering is complete, at most `timeoutMs` — but never hands out a pairing code
 * without a single candidate while one may still come (a slow machine or a slow mDNS registration
 * can take longer than usual): then it waits up to `maxMs` for the first one.
 */
export async function gatherComplete(pc, timeoutMs, maxMs = GATHER_MAX_MS) {
  if (pc.iceGatheringState === 'complete') return;
  let have = /\na=candidate:/.test((pc.localDescription && pc.localDescription.sdp) || '');
  await new Promise((resolve) => {
    const t0 = Date.now();
    const soft = setTimeout(() => { if (have) done(); }, timeoutMs);
    const hard = setTimeout(done, Math.max(timeoutMs, maxMs));
    function done() {
      clearTimeout(soft); clearTimeout(hard);
      pc.removeEventListener('icegatheringstatechange', check);
      pc.removeEventListener('icecandidate', onCand);
      resolve(Date.now() - t0);
    }
    function check() { if (pc.iceGatheringState === 'complete') done(); }
    function onCand(e) {
      if (!e.candidate) { done(); return; }
      have = true;
      if (Date.now() - t0 >= timeoutMs) done(); // past the usual wait: the first one is enough
    }
    pc.addEventListener('icegatheringstatechange', check);
    pc.addEventListener('icecandidate', onCand);
  });
}

export class WebRtcEndpoint extends Endpoint {
  constructor(config, ctx) {
    super('webrtc', config, ctx);
    this.pc = null;
    this.dc = null;
  }

  get datagram() { return true; }

  _attachPc(pc) {
    this.pc = pc;
    pc.addEventListener('connectionstatechange', () => {
      const s = pc.connectionState;
      if (s === 'failed') this._setState('error', 'peer connection failed (are both devices on the same network?)');
      else if (s === 'closed') this._setState('down');
      else if (s === 'disconnected' && this.state === 'up') this._setState('connecting');
    });
  }

  _attachDc(dc) {
    this.dc = dc;
    dc.binaryType = 'arraybuffer';
    dc.onopen = () => this._setState('up');
    dc.onclose = () => { if (!this.closed) this._setState('error', 'data channel closed'); else this._setState('down'); };
    dc.onmessage = (ev) => {
      if (typeof ev.data === 'string') return;
      this._emitData(new Uint8Array(ev.data));
    };
    if (dc.readyState === 'open') this._setState('up');
  }

  async open() {
    // The connection is driven by the pairing flow; open() only reports the current state.
    if (this.dc && this.dc.readyState === 'open') this._setState('up');
    else if (this.state === 'down') this._setState('connecting');
  }

  send(bytes) {
    const dc = this.dc;
    if (!dc || dc.readyState !== 'open') return false;
    if (dc.bufferedAmount > 1024 * 1024) return false;
    dc.send(toBytes(bytes));
    return true;
  }

  async close() {
    this.closed = true;
    try { if (this.dc) this.dc.close(); } catch {}
    try { if (this.pc) this.pc.close(); } catch {}
    this._setState('down');
  }
}

/** Starts a pairing as the offering side. Returns {endpoint, offer}. */
export async function createOffer({ stun, env = globalThis, ctx = {} } = {}) {
  const pc = new env.RTCPeerConnection({ iceServers: iceServers(stun) });
  const ep = new WebRtcEndpoint({ role: 'offer', stun: stun || '' }, { ...ctx, env });
  ep._attachPc(pc);
  ep._attachDc(pc.createDataChannel('ember', { ordered: true }));
  await pc.setLocalDescription(await pc.createOffer());
  await gatherComplete(pc, stun ? GATHER_TIMEOUT_STUN_MS : GATHER_TIMEOUT_MS);
  const offer = encodeBlob(pc.localDescription.sdp, 'offer');
  ep._setState('connecting');
  return { endpoint: ep, offer };
}

/** Answers a pairing offer. Returns {endpoint, answer}; the channel opens when the offerer accepts. */
export async function acceptOffer(offerText, { stun, env = globalThis, ctx = {} } = {}) {
  const d = decodeBlob(offerText);
  if (d.kind !== 'offer') throw new BlobError('this is an answer code — paste it on the device that created the offer');
  const pc = new env.RTCPeerConnection({ iceServers: iceServers(stun) });
  const ep = new WebRtcEndpoint({ role: 'answer', stun: stun || '' }, { ...ctx, env });
  ep._attachPc(pc);
  pc.addEventListener('datachannel', (e) => ep._attachDc(e.channel));
  await pc.setRemoteDescription({ type: 'offer', sdp: buildSdp(d) });
  await pc.setLocalDescription(await pc.createAnswer());
  await gatherComplete(pc, stun ? GATHER_TIMEOUT_STUN_MS : GATHER_TIMEOUT_MS);
  const answer = encodeBlob(pc.localDescription.sdp, 'answer');
  ep._setState('connecting');
  return { endpoint: ep, answer };
}

/** Completes a pairing on the offering side. */
export async function acceptAnswer(endpoint, answerText) {
  const d = decodeBlob(answerText);
  if (d.kind !== 'answer') throw new BlobError('this is an offer code — paste the answer shown on the other device');
  if (!endpoint || !endpoint.pc) throw new BlobError('unknown pairing');
  if (d.setup === 'actpass') d.setup = 'active';
  await endpoint.pc.setRemoteDescription({ type: 'answer', sdp: buildSdp(d) });
  return endpoint;
}

export default {
  kind: 'webrtc',
  title: 'WebRTC (pair by QR / copy-paste)',
  description: 'Direct browser-to-device link over Wi-Fi/LAN/hotspot. Pair by exchanging two codes; no server or internet needed.',
  datagram: true,
  scannable: false,
  autoRestore: false, // a new pairing (offer/answer) is needed after every restart
  pairing: true,
  linkTypes: ['raw'],
  defaultLinkType: 'raw',
  detect(env) {
    if (typeof env.RTCPeerConnection !== 'function') {
      return { available: false, reason: 'This browser has no WebRTC (RTCPeerConnection) support' };
    }
    return { available: true };
  },
  config: schema,
  create() {
    throw Object.assign(new Error('WebRTC links are created with host.webrtc.create_offer / accept_offer'), { code: 'bad_params' });
  },
};
