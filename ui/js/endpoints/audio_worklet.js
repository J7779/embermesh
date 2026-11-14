// AudioWorklet processor for the audio endpoint (ui/js/endpoints/audio.js). A separate file so it
// loads under the UI's strict CSP (script-src 'self'); the single-file build maps it to a blob: URL.
// Capture: batches 1024 input frames and posts them to the main thread ({rx: Float32Array}).
// Playback: plays queued {pcm: Float32Array, more} chunks, asks for more ({need: n}) below ~120 ms.
// `more:true` = the core is mid-transmission (ember_audio_tx returned everything asked for), so an
// empty queue in that state is an audible gap: counted in stats.underruns ({stats:true} -> {stats}).
class EmberModemProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.buf = new Float32Array(1024); this.n = 0;
    this.q = []; this.off = 0; this.queued = 0;
    this.waiting = false; this.lastAsk = -1; this.low = Math.ceil(sampleRate * 0.12); this.stop = false;
    this.more = false; this.st = { played: 0, underruns: 0, underrun_samples: 0, chunks: 0 };
    this.port.onmessage = (e) => {
      const m = e.data || {};
      if (m.stats) { this.port.postMessage({ stats: { ...this.st, queued: this.queued, sample_rate: sampleRate } }); return; }
      if (m.pcm && m.pcm.length) { this.q.push(m.pcm); this.queued += m.pcm.length; this.st.chunks++; }
      if (m.pcm !== undefined || m.more !== undefined) this.more = !!m.more;
      if (m.stop) this.stop = true;
      if (m.flush) { this.q = []; this.off = 0; this.queued = 0; }
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
    if (!this.waiting && this.queued < this.low && currentTime - this.lastAsk > 0.02) {
      this.waiting = true; this.lastAsk = currentTime;
      this.port.postMessage({ need: this.low * 2 - this.queued });
    }
    return !this.stop;
  }
}
registerProcessor('ember-modem', EmberModemProcessor);
