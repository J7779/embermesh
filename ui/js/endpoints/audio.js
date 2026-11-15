// Sound-card endpoint for soft-modem links: microphone/line-in -> ember_audio_rx, and
// ember_audio_tx -> speaker/line-out. This is how a phone or laptop browser talks through a
// handheld radio (audio cable / interface, or acoustic coupling) with VOX or serial-line PTT.
//
// Capture runs in an AudioWorklet (ScriptProcessor fallback) with every voice "enhancement"
// disabled: echo cancellation, noise suppression and AGC destroy modem tones.
import { Endpoint, field, applyDefaults, isSecure } from './base.js';
import { floatToInt16, int16ToFloat } from './framing.js';
import { pttSignals } from './serial.js';

const GUM_TIMEOUT_MS = 30000;
// PTT release (these key a real transmitter):
// - worklet: release PTT_TAIL_S plus the device output latency after the worklet reports its queue
//   empty, never on a main-thread guess (the guess ignores the main thread -> worklet delay).
// - a worklet that never answers (stalled, crashed) still releases: at the predicted end plus
//   PTT_STALL_S, and never later than PTT_MAX_HANG_S after the core dropped PTT.
// - ScriptProcessor: the main thread fills the device buffers itself, so the prediction is exact
//   (each buffer plays one buffer after its callback; _notePlayout counts that). The wait runs on
//   the wall clock and the samples on the audio clock: when the timer fires and the audio clock has
//   not reached the predicted end (the device fell behind), it waits for the rest, never later
//   than PTT_MAX_HANG_S after the core dropped PTT.
// - close / page hide / errors release at once, as before.
const PTT_TAIL_S = 0.03;
const PTT_STALL_S = 1;
const PTT_MAX_HANG_S = 6;

// ScriptProcessor fallback (the single file on file://, old browsers): the main-thread callback for
// one buffer must return before the previous buffer has finished playing, or that part of the
// buffer plays as silence — a gap mid-frame the far end cannot decode. The buffer length is
// therefore how late a callback may run. 4096 frames (85 ms at 48 kHz) lost 12,648 frames of signal
// with callbacks 100 ms late. SP_MIN_S = 160 ms gives 8192 frames at 48 and 44.1 kHz (171 / 186 ms)
// and 4096 at 16 kHz (256 ms): callbacks up to ~170 ms late play whole. The price is one extra
// buffer before the first sample and before PTT release (≈ 2 × 171 ms keyed in all, which the
// release already waits for) and staler receive audio, which delays the "channel busy" sense by
// the same amount; 16384 (341 ms) would double that for stalls a busy page rarely reaches.
// A deeper queue on the main thread would not help: the late callback itself is the deadline.
const SP_MIN_S = 0.16;
/** ScriptProcessor buffer size: the smallest power of two of at least SP_MIN_S (256…16384). Pure. */
export function spBufferSize(rate) {
  const want = (Number(rate) || 48000) * SP_MIN_S;
  let n = 256;
  while (n < want && n < 16384) n *= 2;
  return n;
}


// The AudioWorklet processor lives in audio_worklet.js: a real file satisfies a strict CSP
// (script-src 'self'); in the single-file build the bootstrap maps it to its blob: URL.
export function workletUrl() {
  const canon = new URL('./audio_worklet.js', import.meta.url).href;
  const single = globalThis.__EMBER_SINGLE__;
  return (single && single.urls && single.urls[canon]) || canon;
}

function schema() {
  return [
    field.text('input_device', 'Input (microphone / line in)', { default: '', help: 'Leave empty for the default device; use Scan to pick one.' }),
    field.text('output_device', 'Output (speaker / line out)', { default: '', help: 'Output selection works in Chrome/Edge; other browsers use the default output.' }),
    field.select('sample_rate', 'Sample rate', [
      { value: 'auto', label: 'Device default' }, { value: '48000', label: '48 kHz' },
      { value: '44100', label: '44.1 kHz' }, { value: '16000', label: '16 kHz' },
    ], { default: 'auto', advanced: true }),
    field.number('tx_level', 'Transmit level', { default: 70, min: 1, max: 100, step: 1, unit: '%', help: 'Lower it if the radio distorts; raise it for acoustic coupling.' }),
    field.number('rx_gain', 'Receive gain', { default: 1, min: 0.1, max: 20, step: 0.1, advanced: true }),
    field.select('ptt', 'Transmitter keying (PTT)', [
      { value: 'vox', label: 'VOX (radio keys on audio)' },
      { value: 'serial_rts', label: 'Serial port RTS (USB cable / Digirig)' },
      { value: 'serial_dtr', label: 'Serial port DTR' },
    ], { default: 'vox' }),
    field.bool('ptt_invert', 'Invert PTT line', { default: false, advanced: true }),
  ];
}

class AudioEndpoint extends Endpoint {
  constructor(config, ctx) {
    super('audio', applyDefaults(schema(), config), ctx);
    this.ac = null;
    this.stream = null;
    this.node = null;
    this.pttPort = null;
    this.sampleRate = 0;
    this._resume = null;
    this.ptt = false;
    this.pttEvents = [];
    this.txSamples = 0;
    this._playEnd = 0;
    this._pttTimer = 0;
    this._pttCancel = null;
    this._burst = null;
    this.bursts = [];
    this._statsWait = null;
    this._drainWait = null;
    this._drainSeq = 0;
    this._drainedAt = 0;
  }

  get datagram() { return false; }

  _ensureContext() {
    if (this.ac) return;
    const AC = this.env.AudioContext || this.env.webkitAudioContext;
    const opts = { latencyHint: 'interactive' };
    if (this.config.sample_rate && this.config.sample_rate !== 'auto') opts.sampleRate = Number(this.config.sample_rate);
    this.ac = new AC(opts);
  }

  async prepare({ interactive = true } = {}) {
    // Create the AudioContext inside the user gesture so it may start immediately.
    this._ensureContext();
    if (interactive && this.ac.state === 'suspended') this.ac.resume().catch(() => {});
    if (this.config.ptt.startsWith('serial_') && !this.pttPort) {
      const serial = this.env.navigator && this.env.navigator.serial;
      if (!serial) throw Object.assign(new Error('serial PTT needs Web Serial (Chrome/Edge on a computer); use VOX'), { code: 'unsupported' });
      const ports = await serial.getPorts();
      const match = ports.find((p) => {
        const i = p.getInfo ? p.getInfo() : {};
        return i.usbVendorId === this.config.ptt_usb_vendor_id && i.usbProductId === this.config.ptt_usb_product_id;
      });
      this.pttPort = match || (interactive ? await serial.requestPort({}) : null);
      if (!this.pttPort) throw Object.assign(new Error('PTT serial port not available'), { code: 'not_found' });
      const info = this.pttPort.getInfo ? this.pttPort.getInfo() : {};
      this.config.ptt_usb_vendor_id = info.usbVendorId;
      this.config.ptt_usb_product_id = info.usbProductId;
    }
  }

  async open() {
    this.closed = false;
    this._setState('connecting');
    await this.prepare({ interactive: false }).catch((e) => { this._setState('error', e.message); throw e; });
    const md = this.env.navigator.mediaDevices;
    const audio = {
      echoCancellation: false, noiseSuppression: false, autoGainControl: false, channelCount: 1,
    };
    if (this.config.input_device) audio.deviceId = { exact: this.config.input_device };
    let timer;
    let req = null;
    const limit = Number(this.config.media_timeout_ms) > 0 ? Number(this.config.media_timeout_ms) : GUM_TIMEOUT_MS; // opaque config key (tests)
    try {
      // getUserMedia can wait forever (an OS permission dialog nobody answers, headless browsers).
      req = md.getUserMedia({ audio, video: false });
      this.stream = await Promise.race([req, new Promise((_, rej) => { timer = setTimeout(() => rej(Object.assign(new Error('timeout'), { name: 'TimeoutError' })), limit); })]);
    } catch (e) {
      // a stream that arrives after we gave up must not leave the microphone on
      if (e && e.name === 'TimeoutError' && req) req.then((st) => st.getTracks().forEach((t) => t.stop()), () => {});
      const msg = e && e.name === 'NotAllowedError' ? 'microphone permission denied'
        : e && e.name === 'TimeoutError' ? `no answer from the microphone within ${Math.round(limit / 1000)} s (permission prompt not answered, or the system blocks microphone access for this browser)`
          : String((e && e.message) || e);
      this._setState('error', msg);
      throw Object.assign(new Error(msg), { code: e && e.name === 'TimeoutError' ? 'io_error' : 'denied' });
    } finally { clearTimeout(timer); }
    const ac = this.ac;
    if (this.config.output_device && ac.setSinkId) {
      try { await ac.setSinkId(this.config.output_device); } catch (e) { this._log('warn', 'setSinkId failed', e); }
    }
    this.sampleRate = ac.sampleRate;
    const src = ac.createMediaStreamSource(this.stream);
    const fileBlob = this.env.location && this.env.location.protocol === 'file:' && /^blob:/.test(workletUrl());
    let worklet = !!(ac.audioWorklet && typeof this.env.AudioWorkletNode === 'function') && !fileBlob;
    if (worklet) {
      // Chrome refuses worklet modules from blob:/data: URLs on a file:// page (opaque origin, "Not
      // allowed to load local resource"), so the single file opened from disk uses the
      // ScriptProcessor path below (fileBlob); any other addModule failure falls back too.
      try { await ac.audioWorklet.addModule(workletUrl()); } catch (e) {
        worklet = false;
        this._log('warn', 'AudioWorklet unavailable here, using ScriptProcessor', (e && e.message) || e);
      }
    }
    this.mode = worklet ? 'worklet' : 'scriptprocessor';
    if (worklet) {
      const node = new this.env.AudioWorkletNode(ac, 'ember-modem', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [1] });
      node.port.onmessage = (e) => this._onWorklet(e.data);
      src.connect(node);
      node.connect(ac.destination);
      this.node = node;
    } else {
      // Fallback for old browsers: ScriptProcessorNode on the main thread.
      const sp = ac.createScriptProcessor(spBufferSize(ac.sampleRate), 1, 1); // capture and playback share it
      this.spSize = sp.bufferSize || spBufferSize(ac.sampleRate);
      sp.onaudioprocess = (e) => {
        this._rx(e.inputBuffer.getChannelData(0));
        const out = e.outputBuffer.getChannelData(0);
        const pcm = this.ctx.audioTx ? this.ctx.audioTx(out.length, this.sampleRate) : null;
        out.fill(0);
        if (pcm && pcm.length) { out.set(int16ToFloat(pcm, this._txGain()).subarray(0, out.length)); this._notePlayout(pcm.length, out.length); this.txSamples += pcm.length; if (this._burst) this._burst.samples += pcm.length; }
      };
      src.connect(sp);
      sp.connect(ac.destination);
      this.node = sp;
    }
    if (this.pttPort) {
      try { await this.pttPort.open({ baudRate: 9600 }); } catch (e) { if (!(e && e.name === 'InvalidStateError')) this._log('warn', 'PTT port open failed', e); }
      await this.setPtt(false);
    }
    this._watchRunning();
  }

  _txGain() { return Math.max(0.01, Math.min(1, Number(this.config.tx_level) / 100)); }

  _watchRunning() {
    const update = () => {
      if (this.closed) return;
      if (this.ac.state === 'running') this._setState('up');
      else this._setState('connecting', 'tap anywhere on the page to start audio');
    };
    this.ac.onstatechange = update;
    update();
    if (this.ac.state !== 'running' && this.env.document) {
      const doc = this.env.document;
      this._resume = () => this.ac && this.ac.resume().catch(() => {});
      doc.addEventListener('pointerdown', this._resume, { capture: true });
      doc.addEventListener('keydown', this._resume, { capture: true });
    }
  }

  _rx(f32) {
    if (this.ctx.audioRx) this.ctx.audioRx(floatToInt16(f32, Number(this.config.rx_gain) || 1), this.sampleRate);
  }

  _onWorklet(m) {
    // messages still in flight when the endpoint was closed: nothing to feed or answer any more
    if (!m || this.closed || !this.node) return;
    if (m.rx) this._rx(m.rx);
    if (m.stats && this._statsWait) { const w = this._statsWait; this._statsWait = null; w(m.stats); }
    if (m.drained !== undefined && this._drainWait && this._drainWait.id === m.drained) { const w = this._drainWait; this._drainWait = null; w.fn(m); }
    if (m.need !== undefined) {
      const want = Math.max(128, Math.min(48000, m.need | 0));
      const pcm = this.ctx.audioTx ? this.ctx.audioTx(want, this.sampleRate) : null;
      if (pcm && pcm.length) {
        const f = int16ToFloat(pcm, this._txGain());
        // ember_audio_tx returns exactly `max` samples mid-transmission, fewer only on the last call
        const more = pcm.length >= want;
        this._notePlayout(pcm.length);
        this.txSamples += pcm.length;
        if (this._burst) this._burst.samples += pcm.length;
        this.node.port.postMessage({ pcm: f, more }, [f.buffer]);
      } else {
        this.node.port.postMessage({ more: false });
      }
    }
  }

  /**
   * Predicts when the last queued TX sample leaves the speaker. `block`: a ScriptProcessor buffer
   * size — the buffer filled in a callback plays after the one playing now, one block later.
   */
  _notePlayout(samples, block = 0) {
    const now = this.ac ? this.ac.currentTime : 0;
    const rate = this.sampleRate || 48000;
    this._playEnd = Math.max(this._playEnd || 0, now + block / rate) + samples / rate;
  }

  _outLatency() { return this.ac ? (Number(this.ac.outputLatency) || 0) + (Number(this.ac.baseLatency) || 0) : 0; }

  /** Seconds until queued TX audio has been played, plus the device output latency. */
  pttHangSeconds() {
    if (!this.ac) return 0;
    return Math.max(0, (this._playEnd || 0) - this.ac.currentTime) + this._outLatency() + PTT_TAIL_S;
  }

  /** Playback statistics from the worklet: {played, underruns, underrun_samples, queued, chunks}. */
  stats() {
    if (!this.node || !this.node.port || typeof this.env.AudioWorkletNode !== 'function') return Promise.resolve(null);
    return new Promise((resolve) => {
      this._statsWait = resolve;
      this.node.port.postMessage({ stats: true });
      setTimeout(() => { if (this._statsWait === resolve) { this._statsWait = null; resolve(null); } }, 1000);
    });
  }

  /**
   * link.ptt from the core. PTT on keys at once; PTT off is held until the audio already queued
   * has played out (worklet: until it reports its queue empty), or the tail is cut off. Bounded:
   * see PTT_STALL_S / PTT_MAX_HANG_S.
   */
  async setPtt(on) {
    on = !!on;
    this.ptt = on;
    this.pttEvents.push({ on, t: Date.now() });
    if (this.pttEvents.length > 64) this.pttEvents.shift();
    clearTimeout(this._pttTimer);
    if (this._pttCancel) { const c = this._pttCancel; this._pttCancel = null; c(); } // a new key-up supersedes a pending release
    this._drainWait = null;
    const now = () => (this.ac ? this.ac.currentTime : 0);
    if (on) {
      if (!this._burst) this._burst = { samples: 0, on_audio: now(), on_ms: Date.now() };
    }
    const finish = () => {
      // Burst record (tests, diagnostics): release time vs. the moment the last sample plays.
      const b = this._burst;
      if (b && !this.ptt) {
        this._burst = null;
        b.off_audio = now(); b.off_ms = Date.now(); b.play_end = Math.max(this._playEnd || 0, this._drainedAt || 0);
        if (this._drainedAt) b.drained_audio = this._drainedAt;
        this.bursts.push(b);
        if (this.bursts.length > 32) this.bursts.shift();
      }
    };
    const sig = this.pttPort ? pttSignals({ ptt: this.config.ptt === 'serial_rts' ? 'rts' : 'dtr', ptt_invert: this.config.ptt_invert }, on) : null;
    const apply = async () => {
      if (!on) finish(); // the release is decided now: a key-up arriving during setSignals is a new burst
      if (sig) { try { await this.pttPort.setSignals(sig); } catch (e) { this._log('warn', 'PTT failed', e); } }
    };
    if (on || this.closed) { await apply(); return; }
    const hang = this.pttHangSeconds();
    const port = this.mode === 'worklet' && this.node && this.node.port;
    // nothing was keyed (the line is driven low on open, or a repeated release): nothing to wait for
    if (!this._burst || (!port && !(hang > 0))) { await apply(); return; }
    this._drainedAt = 0;
    await new Promise((resolve) => {
      let done = false, tail = 0;
      const end = () => { done = true; clearTimeout(this._pttTimer); clearTimeout(tail); this._pttCancel = null; this._drainWait = null; };
      const go = () => { if (done) return; end(); apply().then(resolve, resolve); };
      this._pttCancel = () => { if (!done) { end(); resolve(); } };
      // ScriptProcessor: the prediction. Worklet: the hard bound, if the drain report never comes.
      const bound = port ? Math.min(hang + PTT_STALL_S, PTT_MAX_HANG_S) : Math.min(hang, PTT_MAX_HANG_S);
      const asked = Date.now();
      // ScriptProcessor: release only once the audio clock has reached the end of the last sample.
      const played = () => {
        if (done) return;
        if (!this.ac || this.ac.state !== 'running') { go(); return; } // a stopped clock never arrives
        const left = (this._playEnd || 0) + this._outLatency() + PTT_TAIL_S - now();
        const room = PTT_MAX_HANG_S - (Date.now() - asked) / 1000;
        if (left > 0.001 && room > 0) { this._pttTimer = setTimeout(played, Math.ceil(Math.min(left, room) * 1000)); return; }
        go();
      };
      this._pttTimer = setTimeout(port ? go : played, Math.ceil(bound * 1000));
      if (!port) return;
      const id = ++this._drainSeq;
      this._drainWait = { id, fn: (m) => { this._drainedAt = Number(m.t) || now(); tail = setTimeout(go, Math.ceil((this._outLatency() + PTT_TAIL_S) * 1000)); } };
      // messages to the worklet arrive in order: the drain request follows every chunk already sent
      try { port.postMessage({ drain: id }); } catch (e) { this._log('warn', 'PTT drain request failed', e); go(); }
    });
  }

  send() { return false; } // audio links move PCM, not bytes

  async close() {
    this.closed = true;
    if (this._resume && this.env.document) {
      this.env.document.removeEventListener('pointerdown', this._resume, { capture: true });
      this.env.document.removeEventListener('keydown', this._resume, { capture: true });
    }
    clearTimeout(this._pttTimer);
    this._playEnd = 0;
    try { await this.setPtt(false); } catch {}
    try { if (this.pttPort) await this.pttPort.close(); } catch {}
    try { if (this.node && this.node.port) this.node.port.postMessage({ stop: true }); } catch {}
    try { if (this.node) this.node.disconnect(); } catch {}
    if (this.stream) for (const t of this.stream.getTracks()) t.stop();
    try { if (this.ac) await this.ac.close(); } catch {}
    this.ac = null; this.node = null; this.stream = null;
    this._setState('down');
  }
}

export function audioUnavailableReason(env) {
  const nav = env.navigator || {};
  const AC = env.AudioContext || env.webkitAudioContext;
  if (!AC) return 'This browser has no Web Audio support';
  if (!isSecure(env)) return 'The microphone needs a secure page: HTTPS, localhost, or the single-file version opened from disk';
  if (!nav.mediaDevices || !nav.mediaDevices.getUserMedia) return 'This browser does not allow microphone access';
  return null;
}

export default {
  kind: 'audio',
  title: 'Sound card / radio audio',
  description: 'Soft modem through the microphone and speaker or an audio cable to a radio (VOX or serial PTT).',
  datagram: false,
  scannable: true,
  autoRestore: true,
  linkTypes: ['audio'],
  defaultLinkType: 'audio',
  detect(env) {
    const r = audioUnavailableReason(env);
    return r ? { available: false, reason: r } : { available: true };
  },
  config: schema,
  async scan(_params = {}, env = globalThis) {
    const md = env.navigator && env.navigator.mediaDevices;
    if (!md || !md.enumerateDevices) return { devices: [] };
    let list = await md.enumerateDevices();
    if (!list.some((d) => d.label) && md.getUserMedia) {
      // Labels are hidden until the user grants microphone access once.
      try {
        const s = await md.getUserMedia({ audio: true });
        for (const t of s.getTracks()) t.stop();
        list = await md.enumerateDevices();
      } catch {}
    }
    const devices = [];
    for (const d of list) {
      if (d.kind === 'audioinput') devices.push({ id: `in:${d.deviceId}`, name: d.label || 'Microphone', detail: 'input', config: { input_device: d.deviceId } });
      if (d.kind === 'audiooutput') devices.push({ id: `out:${d.deviceId}`, name: d.label || 'Speaker', detail: 'output', config: { output_device: d.deviceId } });
    }
    return { devices };
  },
  create: (config, ctx) => new AudioEndpoint(config, ctx),
};
