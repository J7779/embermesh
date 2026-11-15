// A virtual sound card for the sample-time twins of the real-time audio tests (docs/CI.md).
//
// Stands in for Web Audio in an endpoint's `env`: AudioContext, AudioWorkletNode (running the real
// ui/js/endpoints/audio_worklet.js processor in a vm context), ScriptProcessor and media streams.
// Time is the sample counter: run() renders 128-frame quanta, delivers the messages between the
// "main thread" and the worklet after a fixed latency in frames, and advances the test's mocked
// timers (node:test mock.timers) by exactly the audio time rendered. Nothing depends on how fast
// the host runs, so the result is the same on every machine.
import fs from 'node:fs';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';

export const QUANTUM = 128;

export class VirtualAudio {
  /** latencyMs: how long a message between the main thread and the worklet takes (each way). */
  constructor({ rate = 48000, latencyMs = 3, mockTimers = null } = {}) {
    this.rate = rate;
    this.frames = 0;
    this.latency = Math.max(1, Math.round((latencyMs / 1000) * rate));
    this.timers = mockTimers;
    this.tickedMs = 0;
    this.queue = []; // [{at, fn}]
    this.nodes = []; // processing order
    this.procs = new Map(); // name -> processor class
  }

  get currentTime() { return this.frames / this.rate; }

  _post(fn) { this.queue.push({ at: this.frames + this.latency, fn }); }

  /** A silent stream, or one that pulls each quantum from `pull(frames) -> Float32Array`. */
  stream(pull = null) {
    return { pull: pull || ((n) => new Float32Array(n)), getTracks: () => [{ stop() {}, kind: 'audio' }], getAudioTracks() { return this.getTracks(); } };
  }

  /** The output of a node (what its speaker plays) as a stream, one quantum behind nothing. */
  outputOf(getNode) { return this.stream((n) => { const node = getNode(); return node && node.lastOut ? node.lastOut.slice(0, n) : new Float32Array(n); }); }

  _loadWorklet(url) {
    const src = fs.readFileSync(fileURLToPath(url), 'utf8');
    const va = this;
    const ctx = {
      AudioWorkletProcessor: class { constructor() { this.port = va._pendingPort; } },
      registerProcessor: (name, cls) => va.procs.set(name, cls),
    };
    Object.defineProperty(ctx, 'sampleRate', { get: () => va.rate });
    Object.defineProperty(ctx, 'currentTime', { get: () => va.currentTime });
    vm.runInNewContext(src, ctx, { filename: url });
  }

  /** Globals for an endpoint's env. worklet:false = a browser without AudioWorklet (ScriptProcessor). */
  env({ mic, worklet = true, serial = null } = {}) {
    const va = this;
    class AudioContext {
      constructor() {
        this.sampleRate = va.rate; this.state = 'running'; this.outputLatency = 0; this.baseLatency = 0;
        this.destination = {}; this.onstatechange = null;
        this.audioWorklet = worklet ? { addModule: async (url) => va._loadWorklet(url) } : undefined;
      }
      get currentTime() { return va.currentTime; }
      async resume() {} async close() { this.state = 'closed'; }
      createMediaStreamSource(stream) { return { connect(node) { node._src = stream; }, disconnect() {} }; }
      createScriptProcessor(size) {
        const sp = { kind: 'sp', size, onaudioprocess: null, _in: new Float32Array(size), _n: 0, _out: [], connect() {}, disconnect() { va.nodes = va.nodes.filter((x) => x !== sp); } };
        va.nodes.push(sp);
        return sp;
      }
    }
    class AudioWorkletNode {
      constructor(ac, name) {
        const main = { onmessage: null, postMessage: (m) => va._post(() => proc.port.onmessage && proc.port.onmessage({ data: m })) };
        va._pendingPort = { onmessage: null, postMessage: (m) => va._post(() => main.onmessage && main.onmessage({ data: m })) };
        const Cls = va.procs.get(name);
        const proc = new Cls();
        this.kind = 'worklet'; this.port = main; this.proc = proc; this.lastOut = null;
        va.nodes.push(this);
      }
      connect() {} disconnect() { va.nodes = va.nodes.filter((x) => x !== this); }
    }
    const nav = { mediaDevices: { getUserMedia: async () => mic || va.stream(), enumerateDevices: async () => [] } };
    if (serial) nav.serial = serial;
    const env = Object.create(globalThis);
    // own properties (some globals, e.g. navigator, are getter-only on globalThis)
    const own = (o) => { for (const [k, v] of Object.entries(o)) Object.defineProperty(env, k, { value: v, writable: true, configurable: true, enumerable: true }); };
    own({ crypto: globalThis.crypto, AudioContext, webkitAudioContext: undefined, AudioWorkletNode: worklet ? AudioWorkletNode : undefined, navigator: nav, isSecureContext: true, location: { protocol: 'http:', href: 'http://localhost/' }, document: undefined, Worker: undefined });
    return env;
  }

  _render(node) {
    const input = node._src ? node._src.pull(QUANTUM) : new Float32Array(QUANTUM);
    if (node.kind === 'worklet') {
      const out = new Float32Array(QUANTUM);
      node.proc.process([[input]], [[out]]);
      node.lastOut = out;
      return;
    }
    // ScriptProcessor: collects `size` input frames; the main-thread callback runs `latency` frames
    // later and its output plays one buffer after the block it was handed (double buffering: the
    // callback has `size` frames to answer). Whatever
    // part of that window has passed before the callback returned is lost: played as silence and
    // counted in node.lost (frames of signal lost).
    node._in.set(input, node._n); node._n += QUANTUM;
    if (node._n === node.size) {
      const inBuf = node._in, outBuf = new Float32Array(node.size), start = this.frames + QUANTUM + node.size; // block k plays during block k+1
      node._in = new Float32Array(node.size); node._n = 0;
      const slot = { start, buf: null };
      node._out.push(slot);
      this._post(() => {
        if (node.onaudioprocess) node.onaudioprocess({ inputBuffer: { getChannelData: () => inBuf }, outputBuffer: { getChannelData: () => outBuf } });
        slot.buf = outBuf;
        const passed = Math.max(0, Math.min(node.size, this.frames - slot.start));
        for (let i = 0; i < passed; i++) if (outBuf[i] !== 0) { node.lost = (node.lost || 0) + 1; }
      });
    }
    node.lastOut = new Float32Array(QUANTUM);
    while (node._out.length && node._out[0].start + node.size <= this.frames) node._out.shift();
    const cur = node._out[0];
    if (cur && cur.buf && cur.start <= this.frames) { const off = this.frames - cur.start; node.lastOut = cur.buf.slice(off, off + QUANTUM); }
  }

  /** One quantum: messages due, every node in order, then the clock and the mocked timers. */
  async step(onQuantum) {
    const due = this.queue.filter((m) => m.at <= this.frames);
    this.queue = this.queue.filter((m) => m.at > this.frames);
    for (const m of due) m.fn();
    for (const n of [...this.nodes]) this._render(n);
    if (onQuantum) onQuantum(this.frames);
    this.frames += QUANTUM;
    if (this.timers) {
      const ms = Math.floor((this.frames / this.rate) * 1000);
      if (ms > this.tickedMs) { this.timers.tick(ms - this.tickedMs); this.tickedMs = ms; }
    }
    await new Promise((r) => setImmediate(r)); // let promise chains (PTT, RPC replies) settle
  }

  /** Renders until `done()` is true or `seconds` of audio have passed; returns the seconds rendered. */
  async run(seconds, done = () => false, onQuantum = null) {
    const end = this.frames + seconds * this.rate;
    while (this.frames < end) {
      await this.step(onQuantum);
      if (await done()) break;
    }
    return this.currentTime;
  }
}
