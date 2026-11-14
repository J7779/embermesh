// Morse: Send (flashlight / screen / tone, ./morse_send.js) | Read by camera | Read by sound. Reading uses the core's
// decoder (morse.rx.*, docs/RPC_API.md §6); capture is ui/js/lib/capture*.js, or the host's native
// capture (host.capture.*) when the web view cannot use getUserMedia.
import { h, clear, append, debounce } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { toast, toastError, copyText, segmented, errorText } from '../lib/ui.js';
import { sendPanel } from './morse_send.js';
import { pcmToB64, confidenceWord } from '../lib/capture.js';
import { CameraReader, MicReader, unavailableReason, mediaErrorText } from '../lib/capture_media.js';

const TABS = [{ value: 'send', label: 'Send' }, { value: 'camera', label: 'Read by camera' }, { value: 'sound', label: 'Read by sound' }];
const AUDIO_RATE = 8000;

export default async function render(app, params, root) {
  const tab = (params.query && params.query.get('tab')) || 'send';
  const page = h('div', { class: 'page morse-page' });
  root.appendChild(page);
  append(page,
    h('div', { class: 'page-head' }, h('a', { class: 'btn ghost icon-only', href: '#/tools', 'aria-label': 'Back' }, icon('back')), h('h1', null, 'Morse'),
      h('a', { class: 'btn ghost icon-only', href: '#/help/read/embermesh/morse-reading', 'aria-label': 'How to send and read Morse' }, icon('info'))),
    segmented(TABS, tab, v => app.nav('#/tools/morse?tab=' + v), 'Morse mode'));
  const body = h('div');
  page.appendChild(body);
  if (tab === 'camera') return readPanel(app, body, 'light');
  if (tab === 'sound') return readPanel(app, body, 'audio');
  return sendPanel(app, body);
}

// ---------------------------------------------------------------- reading

/** The plain state line for a camera reader status (§12 target). Pure; exported for tests. */
export function cameraStateText(st, running) {
  if (!running) return 'Point the camera at a blinking light, then press Start reading.';
  const t = st && st.target;
  if (!t) return st && st.chars ? 'Reading the light' : 'Looking for a blinking light…';
  if (t.state === 'locked') return 'Reading the light';
  if (t.state === 'lost') return 'Lost the light — keep the camera on it';
  // a tap with no light there yet: the pin waits (§12), so say so rather than "looking"
  if (t.pinned) return 'Waiting for a light where you tapped… Tap Automatic to search everywhere.';
  return 'Looking for a blinking light…';
}

/** "12 WPM · signal good" (no percentages). Pure; exported for tests. */
export function readerMeta(st, isCam) {
  if (!st) return '';
  const parts = [];
  if (st.wpm) parts.push(Math.round(st.wpm) + ' WPM');
  const c = isCam ? (st.target ? st.target.confidence : null) : snrConfidence(st.snr_db);
  const w = confidenceWord(c);
  if (w && (st.wpm || (st.target && st.target.state === 'locked'))) parts.push('signal ' + w);
  if (!isCam && st.tone_hz) parts.push('tone ' + Math.round(st.tone_hz) + ' Hz');
  return parts.join(' · ');
}
const snrConfidence = snr => (Number.isFinite(snr) ? Math.max(0, Math.min(1, (snr - 3) / 15)) : null);

/**
 * Ring geometry in the preview's own pixels from a target — the same ring a host draws on a native
 * preview (§12): centred on x, y, diameter max(size × preview width, 28), only when locked (dimmed
 * when lost, absent while searching). Pure; exported for tests.
 */
export function ringGeometry(target, viewW, viewH) {
  if (!target || (target.state !== 'locked' && target.state !== 'lost') || !Number.isFinite(target.x) || !Number.isFinite(target.y)) return null;
  const d = Math.max(28, (Number(target.size) || 0) * viewW);
  return { x: target.x * viewW - d / 2, y: target.y * viewH - d / 2, d, lost: target.state === 'lost' };
}

function readPanel(app, box, source) {
  const isCam = source === 'light';
  const kindWord = isCam ? 'camera' : 'microphone';
  const page = box.closest('.morse-page');
  const main = document.getElementById('main');
  const msg = h('div', { class: 'morse-msg', 'aria-live': 'polite' });
  const lamp = h('span', { class: 'morse-lamp', role: 'img', 'aria-label': 'Signal off' });
  const stateLine = h('p', { class: 'reader-state-text', role: 'status' });
  const meta = h('p', { class: 'reader-meta' });
  const pending = h('span', { class: 'morse-pending mono', 'aria-label': 'Character being received' });
  const transcript = h('div', { class: 'morse-transcript', 'aria-live': 'polite', 'aria-label': 'Transcript' });
  const startBtn = h('button', { class: 'btn primary block morse-start', onclick: () => (running ? stopAll() : start()) });
  const setStartLabel = () => append(clear(startBtn), icon(running ? 'close' : isCam ? 'scan' : 'sound'), running ? 'Stop' : 'Start reading');
  // camera bits: preview, ring drawn by the UI on the tracked light, zoom, "Automatic" after a tap
  const video = isCam ? h('video', { class: 'morse-video', muted: true, playsinline: '', autoplay: '', 'aria-hidden': 'true' }) : null;
  const ring = h('div', { class: 'reader-ring', hidden: true, 'aria-hidden': 'true' });
  const idle = h('div', { class: 'reader-idle' }, icon(isCam ? 'scan' : 'sound', { size: 40 }));
  const autoBtn = h('button', { class: 'btn small reader-auto', type: 'button', hidden: true }, 'Automatic');
  const zoomBar = h('div', { class: 'reader-zoom', role: 'group', 'aria-label': 'Zoom', hidden: true });
  const tapHint = h('p', { class: 'reader-hint', hidden: true }, 'Several lights blink: tap the one to read.');
  // the picture only: with a native preview the host draws over this rectangle, so nothing the user
  // needs goes inside it (zoom and "Automatic" sit below)
  const view = isCam ? h('div', { class: 'reader-view', 'aria-label': 'Camera picture. Tap a light to read it.' }, video, idle, ring) : null;
  const tools = isCam ? h('div', { class: 'reader-tools' }, zoomBar, autoBtn) : null;
  const saveNotes = h('input', { type: 'checkbox', class: 'switch' });

  let session = null, running = false, reader = null, native = null, poll = 0, lastChars = 0, text = '';
  let feeding = Promise.resolve();
  let last = null, mode = 'grid', checkedFrames = false, locked = false, nativeZoom = [];
  // §12: a core with the light tracker says so; without it, the old watched-region feed and no ring
  const coreFrames = () => { const f = (app.store.get('info') || {}).features; return Array.isArray(f) && f.includes('morse_frames'); };
  // a native preview is drawn above the page: the host draws the ring there (§12), not the page
  // (the mock host draws nothing, so the page stands in for it)
  const pageDrawsRing = () => !native || app.backend.kind === 'mock';
  const setMsg = (t, kind) => { clear(msg); if (t) msg.appendChild(h('div', { class: 'notice ' + (kind || 'info') + ' small' }, t)); };
  setStartLabel();

  function render(st) {
    if (st) last = st;
    const s = last;
    const on = !!(s && s.on);
    lamp.classList.toggle('on', on);
    lamp.setAttribute('aria-label', on ? 'Signal on' : 'Signal off');
    stateLine.textContent = isCam ? cameraStateText(s, running) : !running ? 'Hold the device close to the sound, then press Start reading.' : s && (s.chars || s.wpm) ? 'Reading the sound' : 'Listening…';
    meta.textContent = readerMeta(s, isCam);
    pending.textContent = String((s && s.pending) || '').replace(/\./g, '·').replace(/-/g, '−');
    if (s && typeof s.transcript === 'string' && (s.chars !== lastChars || s.transcript !== text)) {
      lastChars = s.chars; text = s.transcript;
      transcript.textContent = text;
    }
    if (!isCam) return;
    // the ring: only from a §12 target (an old core has none: no ring rather than a wrong one)
    const t = s && s.target;
    const g = running && t && pageDrawsRing() ? ringGeometry(t, view.clientWidth, view.clientHeight) : null;
    ring.hidden = !g;
    if (g) {
      ring.style.width = ring.style.height = Math.round(g.d) + 'px';
      ring.style.transform = 'translate(' + g.x.toFixed(1) + 'px, ' + g.y.toFixed(1) + 'px)';
      ring.classList.toggle('lost', g.lost);
      ring.classList.toggle('lit', on);
    }
    view.classList.toggle('has-target', !!g);
    tapHint.hidden = !(running && t && t.candidates > 1);
    autoBtn.hidden = !(running && t && t.pinned); // the core says whether a tap pinned it (§12)
    tools.hidden = zoomBar.hidden && autoBtn.hidden;
    // exposure locked once the light is locked (§12), released otherwise
    const isLocked = !!(t && t.state === 'locked');
    if (reader && isLocked !== locked) { locked = isLocked; reader.lockExposure(isLocked); }
  }

  const feed = params => {
    if (!session) return;
    const s = session;
    feeding = feeding.then(async () => {
      if (s !== session) return;
      if (params.frames && mode !== 'grid') return; // queued before the fallback: never mix frames and levels
      try {
        const st = await app.api.call('morse.rx.feed', { session: s, ...params });
        // a core without §12 frames ignores them (no target): read with the old watched-region path
        if (params.frames && !checkedFrames) { checkedFrames = true; if (!st || !st.target) { await fallback(); return; } }
        render(st);
      } catch (e) {
        if (e.code === 'not_found') { session = null; setMsg('Reading stopped after a long pause. Press Start reading to go on.', 'warn'); stopAll(); }
        else if (params.frames && (e.code === 'bad_params' || e.code === 'unsupported' || e.code === 'not_implemented')) await fallback();
      }
    });
  };
  // a session fed with frames refuses levels (§12), so the fallback starts a fresh session for them
  async function fallback() {
    if (mode !== 'grid' || !reader) return;
    mode = 'levels';
    reader.setMode('levels');
    await endSession();
    const r = await app.api.try('morse.rx.start', { source: 'light' }, null, true);
    if (!running) { if (r) await app.api.try('morse.rx.stop', { session: r.session }, null, true); return; }
    if (!r) { setMsg('Could not start reading. Press Start reading to try again.', 'bad'); stopAll(); return; }
    session = r.session; last = null; lastChars = 0;
    render();
  }

  async function start() {
    setMsg('');
    let r;
    try { r = await app.api.call('morse.rx.start', isCam ? { source: 'light' } : { source: 'audio', sample_rate: AUDIO_RATE }); } catch (e) {
      setMsg(e.code === 'not_implemented' ? 'Reading Morse needs a newer version of EmberMesh. Update the app.' : 'Could not start reading: ' + errorText(e), 'bad');
      return;
    }
    session = r.session; lastChars = 0; text = ''; transcript.textContent = ''; last = null; checkedFrames = false; locked = false; mode = coreFrames() ? 'grid' : 'levels';
    delete transcript.dataset.live;
    const why = unavailableReason(isCam ? 'camera' : 'mic');
    const nativeKind = isCam ? 'capture_camera_luma' : 'capture_mic_pcm';
    // the app's own capture when it has one (it can lock exposure and focus), else the page's
    const preferNative = app.has(nativeKind) && (isCam || !!why);
    // the screen stays still while the camera runs (the preview rectangle must not move)
    if (isCam && main) { main.scrollTop = 0; main.classList.add('morse-fixed'); }
    page && page.classList.add('reading');
    running = true;
    setStartLabel();
    render();
    try {
      if (preferNative) await startNative();
      else if (why) { setMsg(why, 'bad'); await stopAll(); return; }
      else await startWeb();
    } catch (e) {
      setMsg(e && e.name ? mediaErrorText(e, kindWord) : errorText(e), 'bad');
      await stopAll();
      return;
    }
    app.api.try('host.keep_awake', { enabled: true }, null, true);
    clearInterval(poll);
    poll = setInterval(async () => {
      if (!session) return;
      if (native) { const st = await app.api.try('morse.rx.status', { session }, null, true); if (st) render(st); }
      else render();
    }, 250);
  }

  async function startWeb() {
    if (isCam) {
      reader = new CameraReader({
        video, mode,
        onBatch: b => feed(b),
        onFrame: () => { if (!transcript.dataset.live) { transcript.dataset.live = '1'; view.classList.add('live'); showZoom(); } },
        onError: e => console.warn('[morse camera]', e),
        onStopped: () => { if (running) { setMsg('The camera stopped when the app went to the background. Press Start reading to go on.', 'info'); stopAll(); } },
      });
      const settings = await reader.start('environment');
      if (settings && settings.width && settings.height) { view.style.setProperty('--ar', settings.width + ' / ' + settings.height); view.style.setProperty('--arn', String(settings.width / settings.height)); }
    } else {
      reader = new MicReader({
        rate: AUDIO_RATE,
        onLive: () => { transcript.dataset.live = '1'; render(); },
        onPcm: (() => { let buf = []; let n = 0; return pcm => { buf.push(pcm); n += pcm.length; if (n >= AUDIO_RATE / 10) { const all = new Int16Array(n); let o = 0; for (const p of buf) { all.set(p, o); o += p.length; } buf = []; n = 0; feed({ pcm_b64: pcmToB64(all) }); } }; })(),
      });
      await reader.start();
    }
  }

  // zoom steps: the track's zoom capability (in-page) or host.capture.start's zoom_steps (native)
  function showZoom() {
    const steps = native ? nativeZoom : reader && reader.zoomSteps ? reader.zoomSteps() : [];
    clear(zoomBar);
    zoomBar.hidden = steps.length < 2;
    for (const z of steps) zoomBar.appendChild(h('button', { class: 'btn small', type: 'button', 'aria-pressed': String(z === 1), 'aria-label': 'Zoom ' + z + ' times', onclick: async e => {
      const ok = native ? !!(await app.api.try('host.capture.update', { kind: native, zoom: z }, null, true)) : await reader.setZoom(z);
      if (ok) for (const b of zoomBar.children) b.setAttribute('aria-pressed', String(b === e.currentTarget));
    } }, z + '×'));
  }

  const rectOf = () => { const r = view.getBoundingClientRect(); return { x: Math.round(r.left), y: Math.round(r.top), w: Math.round(r.width), h: Math.round(r.height) }; };
  async function startNative() {
    const kind = isCam ? 'camera_luma' : 'mic_pcm';
    if (isCam) { view.classList.add('native', 'live'); if (app.backend.kind === 'mock') view.classList.add('mock-preview'); }
    const r = await app.api.call('host.capture.start', { kind, session, ...(isCam ? { rect: rectOf(), camera: 'back' } : { sample_rate: AUDIO_RATE }) });
    native = kind;
    nativeZoom = r && Array.isArray(r.zoom_steps) ? r.zoom_steps.filter(z => Number.isFinite(z)) : [];
    if (isCam) showZoom();
    transcript.dataset.live = '1';
  }
  // the native preview follows the rectangle on layout change and rotation (§12)
  const onResize = debounce(() => { if (native === 'camera_luma') app.api.try('host.capture.update', { kind: native, rect: rectOf() }, null, true); render(); }, 150);
  window.addEventListener('resize', onResize);
  window.addEventListener('orientationchange', onResize);

  async function endSession() {
    const s = session; session = null;
    if (s) await app.api.try('morse.rx.stop', { session: s }, null, true);
  }

  async function stopAll() {
    const was = running;
    running = false;
    clearInterval(poll);
    if (reader) { reader.stop(); reader = null; }
    if (native) { await app.api.try('host.capture.stop', { kind: native }, null, true); native = null; }
    if (view) { view.classList.remove('native', 'live', 'mock-preview', 'has-target'); zoomBar.hidden = true; }
    if (main) main.classList.remove('morse-fixed');
    page && page.classList.remove('reading');
    delete transcript.dataset.live;
    await feeding;
    await endSession();
    app.api.try('host.keep_awake', { enabled: false }, null, true);
    setStartLabel();
    lamp.classList.remove('on');
    render();
    if (was && saveNotes.checked && text.trim()) saveToNotes(app, text.trim(), true);
  }

  // tap the picture to read another light (only when several blink); "Automatic" releases it
  if (isCam) {
    view.addEventListener('click', async e => {
      if (!session || !last || !last.target || !(last.target.candidates > 1) || e.target === autoBtn || zoomBar.contains(e.target)) return;
      const r = view.getBoundingClientRect();
      const st = await app.api.try('morse.rx.target', { session, x: (e.clientX - r.left) / r.width, y: (e.clientY - r.top) / r.height }, null, true);
      if (st) render(st);
    });
    autoBtn.addEventListener('click', async () => { const st = await app.api.try('morse.rx.target', { session, auto: true }, null, true); if (st) render(st); });
  }

  const actions = h('div', { class: 'row reader-actions' },
    h('button', { class: 'btn small', onclick: () => copyText(transcript.textContent) }, icon('copy', { size: 16 }), 'Copy'),
    h('button', { class: 'btn small', onclick: () => sendAsMessage(app, transcript.textContent) }, icon('send', { size: 16 }), 'Send as message…'),
    h('button', { class: 'btn small', onclick: async () => { if (session) await app.api.try('morse.rx.reset', { session }, null, true); text = ''; lastChars = 0; transcript.textContent = ''; render(); } }, icon('trash', { size: 16 }), 'Clear'));

  append(box,
    h('section', { class: 'card morse-reader' + (isCam ? ' camera' : ' sound'), 'aria-label': isCam ? 'Read Morse by camera' : 'Read Morse by sound' },
      isCam ? view : h('div', { class: 'reader-ear' }, lamp),
      tools,
      h('div', { class: 'reader-state' }, isCam ? lamp : null, h('div', { class: 'grow' }, stateLine, meta), pending),
      isCam ? tapHint : null,
      msg,
      h('div', { class: 'reader-text' }, transcript),
      startBtn,
      actions,
      h('label', { class: 'check reader-save' }, saveNotes, h('span', null, 'Save to Notes when I stop'))),
    h('p', { class: 'muted small reader-tip' }, isCam
      ? 'Hold the phone as still as you can, or rest it on something, and keep the light in the picture. Darker surroundings make a distant light easier to read.'
      : 'Works with a radio speaker, a whistle or a tone sender. Quiet surroundings help.'));
  render();

  const onVis = () => { if (document.visibilityState === 'hidden' && running) stopAll(); };
  document.addEventListener('visibilitychange', onVis);
  const onPageHide = () => { if (running) stopAll(); };
  window.addEventListener('pagehide', onPageHide);
  // a native capture that stopped by itself (permission revoked, device lost, interruption)
  app.on('host.capture.state', d => {
    if (!d || d.running !== false || !native || (d.kind && d.kind !== native)) return;
    native = null;
    setMsg('The ' + kindWord + ' stopped' + (d.error ? ': ' + d.error : '') + '. Press Start reading to try again.', 'bad');
    stopAll();
  });
  app.on('morse.text', d => { if (d && d.session === session && native) app.api.try('morse.rx.status', { session }, null, true).then(render); });
  app.on('morse.target', d => { if (d && d.session === session && last) render({ ...last, target: { ...(last.target || {}), ...d } }); });
  return () => {
    document.removeEventListener('visibilitychange', onVis);
    window.removeEventListener('pagehide', onPageHide);
    window.removeEventListener('resize', onResize);
    window.removeEventListener('orientationchange', onResize);
    onResize.cancel();
    stopAll();
  };
}

async function saveToNotes(app, t, quiet) {
  try { await app.kvSet('notes', 'morse-' + Date.now().toString(36), { title: 'Morse received ' + new Date().toLocaleString(), body: t, ts: Date.now() }); toast('Saved to Notes', { kind: 'ok' }); }
  catch (e) { if (!quiet) toastError(e, 'Not saved'); }
}

async function sendAsMessage(app, text) {
  const t = String(text || '').trim();
  if (!t) { toast('Nothing to send yet'); return; }
  const r = await app.api.try('conversations.list', {}, { conversations: [] }, true);
  const convs = (r.conversations || []).slice(0, 12);
  const { sheet } = await import('../lib/ui.js');
  const s = sheet('Send transcript to…', h('ul', { class: 'list' }, convs.map(c => h('li', null, h('button', {
    class: 'item', onclick: async () => {
      try { await app.api.call('messages.send', { conv: c.id, text: t }); s.close(); toast('Sent', { kind: 'ok' }); } catch (e) { toastError(e, 'Not sent'); }
    },
  }, h('span', { class: 'grow' }, String(c.title || c.id)), icon('send', { size: 16 }))))));
}
