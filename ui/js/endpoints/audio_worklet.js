// AudioWorklet processor for the audio endpoint (ui/js/endpoints/audio.js). A separate file so it
// loads under the UI's strict CSP (script-src 'self'); the single-file build maps it to a blob: URL.
// Capture: batches 1024 input frames and posts them to the main thread ({rx: Float32Array}).
// Playback: plays queued {pcm: Float32Array, more} chunks, asks for more ({need: n}) below LOW_S of
// queued audio, topping up to twice that. LOW_S is the longest main-thread round trip (worklet asks,
// main thread runs the core, answer arrives) that plays without a gap: 250 ms rides out a busy phone
// or a slow CI runner (120 ms underran at 60 ms each way). The cost: up to 2 × LOW_S of audio queued
// ahead, which only delays the PTT release, and that waits for the queue to drain ({drain} below).
// PTT release: {drain: id} -> {drained: id, t} once the last queued sample has been rendered.
// `more:true` = the core is mid-transmission (ember_audio_tx returned everything asked for), so an
// empty queue in that state is an audible gap: counted in stats.underruns ({stats:true} -> {stats}).
const LOW_S = 0.25;
class EmberModemProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.buf = new Float32Array(1024); this.n = 0;
    this.q = []; this.off = 0; this.queued = 0;
    this.waiting = false; this.lastAsk = -1; this.low = Math.ceil(sampleRate * LOW_S); this.drainId = null; this.stop = false;
    this.more = false; this.st = { played: 0, underruns: 0, underrun_samples: 0, chunks: 0 };
    this.port.onmessage = (e) => {
      const m = e.data || {};
      if (m.stats) { this.port.postMessage({ stats: { ...this.st, queued: this.queued, sample_rate: sampleRate } }); return; }
      if (m.pcm && m.pcm.length) { this.q.push(m.pcm); this.queued += m.pcm.length; this.st.chunks++; }
      if (m.pcm !== undefined || m.more !== undefined) this.more = !!m.more;
      if (m.stop) this.stop = true;
      if (m.flush) { this.q = []; this.off = 0; this.queued = 0; }
      if (m.drain !== undefined) this.drainId = m.drain; // answered from process(), after the audio is written
      this.waiting = false;
    };
  }
  process(inputs, outputs) {
    const inp = inputs[0] && inputs[0][0];
    if (inp) {
      for (let i = 0; i < inp.length; i++) {
        this.buf[this.n++] = inp[i];
        if (this.n === this.buf.length) {
          this.port.postMessage({ rx: this.buf }, [this.buf.buffer]);
          this.buf = new Float32Array(1024); this.n = 0;
        }
      }
    }
    const outs = outputs[0] || [];
    const out = outs[0];
    if (out) {
      let i = 0;
      while (i < out.length && this.q.length) {
        const head = this.q[0];
        const take = Math.min(out.length - i, head.length - this.off);
        out.set(head.subarray(this.off, this.off + take), i);
        i += take; this.off += take; this.queued -= take;
        if (this.off >= head.length) { this.q.shift(); this.off = 0; }
      }
      this.st.played += i;
      if (i < out.length) {
        if (this.more) { this.st.underruns++; this.st.underrun_samples += out.length - i; }
        out.fill(0, i);
      }
      for (let c = 1; c < outs.length; c++) outs[c].set(out);
    }
    if (this.drainId !== null && this.queued === 0) {
      this.port.postMessage({ drained: this.drainId, t: currentTime });
      this.drainId = null;
    }
    if (!this.waiting && this.queued < this.low && currentTime - this.lastAsk > 0.02) {
      this.waiting = true; this.lastAsk = currentTime;
      this.port.postMessage({ need: this.low * 2 - this.queued });
    }
    return !this.stop;
  }
}
registerProcessor('ember-modem', EmberModemProcessor);
