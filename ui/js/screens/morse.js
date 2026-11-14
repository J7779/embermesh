// Morse: Send (flashlight / screen / tone, ./morse_send.js) | Read by camera | Read by sound. Reading uses the core's
// decoder (morse.rx.*, docs/RPC_API.md §6); capture is ui/js/lib/capture*.js, or the host's native
// capture (host.capture.*) when the web view cannot use getUserMedia.
import { h, clear, append } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { toast, toastError, copyText, segmented, errorText, setFill } from '../lib/ui.js';
import { sendPanel } from './morse_send.js';
import { lightAdvice, pcmToB64, roiAt, clampRoi, DEFAULT_ROI } from '../lib/capture.js';
import { CameraReader, MicReader, unavailableReason, mediaErrorText } from '../lib/capture_media.js';

const TABS = [{ value: 'send', label: 'Send' }, { value: 'camera', label: 'Read by camera' }, { value: 'sound', label: 'Read by sound' }];
const AUDIO_RATE = 8000;

export default async function render(app, params, root) {
  const tab = (params.query && params.query.get('tab')) || 'send';
  const page = h('div', { class: 'page morse-page' });
  root.appendChild(page);
  append(page,
    h('div', { class: 'page-head' }, h('a', { class: 'btn ghost icon-only', href: '#/tools', 'aria-label': 'Back' }, icon('back')), h('h1', null, 'Morse'),
      h('a', { class: 'btn ghost small', href: '#/help/read/embermesh/morse-reading', 'aria-label': 'Help: reading Morse' }, icon('book', { size: 16 }), '?')),
    segmented(TABS, tab, v => app.nav('#/tools/morse?tab=' + v), 'Morse mode'));
  const body = h('div');
  page.appendChild(body);
  if (tab === 'camera') return readPanel(app, body, 'light');
  if (tab === 'sound') return readPanel(app, body, 'audio');
  return sendPanel(app, body);
}

// ---------------------------------------------------------------- reading
function readPanel(app, box, source) {
  const isCam = source === 'light';
  const kindWord = isCam ? 'camera' : 'microphone';
  const msg = h('div', { class: 'morse-msg', 'aria-live': 'polite' });
  const lamp = h('span', { class: 'morse-lamp', role: 'img', 'aria-label': 'Signal off' });
  const fill = h('i');
  const thr = h('b', { class: 'thr', 'aria-hidden': 'true' });
  const levelBar = h('div', { class: 'meter-bar', role: 'meter', 'aria-label': 'Signal level', 'aria-valuemin': '0', 'aria-valuemax': '100' }, fill, thr);
  const pending = h('span', { class: 'morse-pending mono', 'aria-label': 'Character being received' });
  const stats = h('div', { class: 'row small muted morse-stats' });
  const transcript = h('div', { class: 'morse-transcript', 'aria-live': 'polite', 'aria-label': 'Transcript' });
  const startBtn = h('button', { class: 'btn primary', onclick: () => (running ? stopAll() : start()) }, icon(isCam ? 'scan' : 'sound'), 'Start reading');
  // camera bits
  const video = isCam ? h('video', { class: 'morse-video', muted: true, playsinline: '', autoplay: '' }) : null;
  const roiBox = isCam ? h('div', { class: 'morse-roi', 'aria-hidden': 'true' }) : null;
  const camWrap = isCam ? h('div', { class: 'morse-cam', title: 'Tap or drag to place the box on the light' }, video, roiBox, h('div', { class: 'morse-cam-hint' }, 'Tap the light, or drag the box onto it')) : null;
  const autoBox = h('input', { type: 'checkbox' });
  // keep the preview box at the video's own aspect ratio so the watched box maps 1:1 onto the frame
  if (video) video.addEventListener('loadedmetadata', () => { if (video.videoWidth && video.videoHeight) camWrap.style.aspectRatio = video.videoWidth + ' / ' + video.videoHeight; });
  // hosts with both native capture and getUserMedia let the user fall back to the in-page camera
  const inPage = h('input', { type: 'checkbox' });
  const inPageRow = isCam && app.has('capture_camera_luma') && app.has('getusermedia') ? h('label', { class: 'check' }, inPage, h('span', null, 'Use the in-page camera instead of the app’s')) : null;
  const camSel = h('select', { 'aria-label': 'Camera' }, h('option', { value: 'environment' }, 'Back camera'), h('option', { value: 'user' }, 'Front camera'));

  let session = null, running = false, reader = null, native = null, poll = 0, lastChars = 0, text = '';
  let feeding = Promise.resolve();
  const history = [];
  let roi = DEFAULT_ROI;
  const setMsg = (t, kind) => { clear(msg); if (t) msg.appendChild(h('div', { class: 'notice ' + (kind || 'info') + ' small' }, t)); };

  const drawRoi = r => { if (!roiBox) return; roiBox.style.left = r.x * 100 + '%'; roiBox.style.top = r.y * 100 + '%'; roiBox.style.width = r.w * 100 + '%'; roiBox.style.height = r.h * 100 + '%'; };
  drawRoi(roi);

  function show(st) {
    if (!st) return;
    lamp.classList.toggle('on', !!st.on);
    lamp.setAttribute('aria-label', st.on ? 'Signal on' : 'Signal off');
    const lv = Math.max(0, Math.min(1, Number(st.level) || 0));
    setFill(fill, lv * 100);
    levelBar.setAttribute('aria-valuenow', String(Math.round(lv * 100)));
    if (Number.isFinite(st.threshold)) { thr.hidden = false; thr.style.left = 'calc(' + st.threshold * 100 + '% - 1px)'; } else thr.hidden = true;
    pending.textContent = String(st.pending || '').replace(/\./g, '·').replace(/-/g, '–') || ' ';
    clear(stats);
    append(stats, h('span', null, st.wpm ? Math.round(st.wpm) + ' WPM' : 'speed: listening…'),
      h('span', null, 'quality ' + Math.round((Number(st.quality) || 0) * 100) + ' %'),
      !isCam ? h('span', null, 'tone ' + (st.tone_hz ? Math.round(st.tone_hz) + ' Hz' : '—')) : null,
      !isCam ? h('span', null, 'SNR ' + (st.snr_db != null ? st.snr_db + ' dB' : '—')) : null);
    if (typeof st.transcript === 'string' && (st.chars !== lastChars || st.transcript !== text)) {
      lastChars = st.chars; text = st.transcript;
      transcript.textContent = text;
      transcript.scrollTop = transcript.scrollHeight;
    }
  }

  const feed = params => {
    if (!session) return;
    const s = session;
    feeding = feeding.then(async () => {
      if (s !== session) return;
      try { show(await app.api.call('morse.rx.feed', { session: s, ...params })); } catch (e) { if (e.code === 'not_found') { session = null; setMsg('The reading session ended (idle too long). Press Start again.', 'bad'); stopAll(); } }
    });
  };

  async function start() {
    setMsg('');
    let r;
    try { r = await app.api.call('morse.rx.start', isCam ? { source: 'light' } : { source: 'audio', sample_rate: AUDIO_RATE }); } catch (e) {
      setMsg(e.code === 'not_implemented' ? 'Morse reading needs a newer EmberMesh core than this one. Update the app.' : 'Could not start the decoder: ' + errorText(e), 'bad');
      return;
    }
    session = r.session; lastChars = 0; text = ''; transcript.textContent = '';
    delete transcript.dataset.live;
    if (!isCam) setMsg('Starting the microphone…', 'info');
    const why = unavailableReason(isCam ? 'camera' : 'mic');
    const nativeKind = isCam ? 'capture_camera_luma' : 'capture_mic_pcm';
    // Native capture is preferred for light: it can lock exposure, focus and white balance.
    const preferNative = app.has(nativeKind) && (isCam ? !inPage.checked : !!why);
    try {
      if (preferNative) await startNative();
      else if (why) { setMsg(why, 'bad'); await endSession(); return; }
      else await startWeb();
    } catch (e) {
      setMsg(e && e.name ? mediaErrorText(e, isCam ? 'camera' : 'mic') : errorText(e), 'bad');
      await endSession();
      return;
    }
    running = true;
    append(clear(startBtn), icon('close'), 'Stop');
    app.api.try('host.keep_awake', { enabled: true }, null, true);
    clearInterval(poll);
    poll = setInterval(async () => {
      if (!session) return;
      if (native) { const st = await app.api.try('morse.rx.status', { session }, null, true); if (st) show(st); }
      if (isCam && history.length > 20) {
        const adv = lightAdvice(history.slice(-60));
        if (adv === 'too_dark') setMsg('Too dark to see a light. Point the camera at the light, or move closer.', 'warn');
        else if (adv === 'saturated') setMsg('The watched area is fully bright. Tap a darker spot next to the light, or place the box exactly on it.', 'warn');
        else if (adv === 'no_blink') setMsg(autoBox.checked ? 'Nothing is blinking yet. Keep the light in view.' : 'Nothing is blinking in the box. Tap the light, or turn on “Find the blinking light”.', 'info');
        else setMsg('');
      }
    }, 250);
  }

  async function startWeb() {
    if (isCam) {
      reader = new CameraReader({
        video,
        onBatch: b => feed(b),
        onFrame: f => { if (!transcript.dataset.live) transcript.dataset.live = '1'; history.push(f.level); if (history.length > 120) history.shift(); if (f.roi !== roi) { roi = f.roi; drawRoi(roi); } },
        onError: e => console.warn('[morse camera]', e),
      });
      reader.roi = roi;
      reader.setAuto(autoBox.checked);
      await reader.start(camSel.value);
    } else {
      reader = new MicReader({
        rate: AUDIO_RATE,
        onLive: () => { transcript.dataset.live = '1'; setMsg('Listening…', 'info'); },
        onPcm: (() => { let buf = []; let n = 0; return pcm => { buf.push(pcm); n += pcm.length; if (n >= AUDIO_RATE / 10) { const all = new Int16Array(n); let o = 0; for (const p of buf) { all.set(p, o); o += p.length; } buf = []; n = 0; feed({ pcm_b64: pcmToB64(all) }); } }; })(),
      });
      await reader.start();
    }
  }

  async function startNative() {
    const kind = isCam ? 'camera_luma' : 'mic_pcm';
    const rect = isCam ? camWrap.getBoundingClientRect() : null;
    await app.api.call('host.capture.start', { kind, session, ...(rect ? { rect: { x: rect.left, y: rect.top, w: rect.width, h: rect.height }, roi, camera: camSel.value === 'user' ? 'front' : 'back' } : { sample_rate: AUDIO_RATE }) });
    native = kind;
    transcript.dataset.live = '1';
    setMsg('Using the ' + kindWord + ' through the app (native capture).', 'info');
  }

  async function endSession() {
    const s = session; session = null;
    if (s) await app.api.try('morse.rx.stop', { session: s }, null, true);
  }

  async function stopAll() {
    running = false;
    clearInterval(poll);
    if (reader) { reader.stop(); reader = null; }
    if (native) { await app.api.try('host.capture.stop', { kind: native }, null, true); native = null; }
    delete transcript.dataset.live;
    await feeding;
    await endSession();
    app.api.try('host.keep_awake', { enabled: false }, null, true);
    append(clear(startBtn), icon(isCam ? 'scan' : 'sound'), 'Start reading');
    lamp.classList.remove('on');
  }

  // placing the watched region: tap = centre on the light, drag = move
  if (isCam) {
    let dragging = false;
    const place = e => {
      const r = camWrap.getBoundingClientRect();
      roi = roiAt((e.clientX - r.left) / r.width, (e.clientY - r.top) / r.height, roi.w, roi.h);
      drawRoi(roi);
      autoBox.checked = false;
      if (reader) reader.setRoi(roi);
      if (native) app.api.try('host.capture.update', { kind: native, roi }, null, true);
    };
    camWrap.addEventListener('pointerdown', e => { dragging = true; try { camWrap.setPointerCapture(e.pointerId); } catch { /* ignore */ } place(e); });
    camWrap.addEventListener('pointermove', e => { if (dragging) place(e); });
    camWrap.addEventListener('pointerup', () => { dragging = false; });
    autoBox.addEventListener('change', () => { if (reader) reader.setAuto(autoBox.checked); if (!autoBox.checked && reader) reader.setRoi(clampRoi(roi)); });
    camSel.addEventListener('change', () => { if (reader && running) reader.start(camSel.value).catch(e => setMsg(mediaErrorText(e, 'camera'), 'bad')); });
  }

  const actions = h('div', { class: 'row' },
    h('button', { class: 'btn small', onclick: async () => { if (session) await app.api.try('morse.rx.reset', { session }, null, true); text = ''; lastChars = 0; transcript.textContent = ''; } }, icon('trash', { size: 16 }), 'Clear'),
    h('button', { class: 'btn small', onclick: () => copyText(transcript.textContent) }, icon('copy', { size: 16 }), 'Copy'),
    h('button', { class: 'btn small', onclick: () => sendAsMessage(app, transcript.textContent) }, icon('send', { size: 16 }), 'Send as message…'),
    h('button', { class: 'btn small', onclick: async () => {
      const t = transcript.textContent.trim();
      if (!t) return toast('Nothing to save yet');
      try { await app.kvSet('notes', 'morse-' + Date.now().toString(36), { title: 'Morse received ' + new Date().toLocaleString(), body: t, ts: Date.now() }); toast('Saved to notes', { kind: 'ok' }); } catch (e) { toastError(e, 'Not saved'); }
    } }, icon('notes', { size: 16 }), 'Save to notes'));

  append(box,
    h('p', { class: 'muted small' }, isCam
      ? 'Point the camera at a flashlight, a lamp or another phone’s screen flash. Hold the device still. Works from a few metres up to as far as the camera can see the light.'
      : 'Listen to a radio speaker or a whistle/tone sender. Hold the device close to the sound; quiet surroundings help.'),
    camWrap,
    msg,
    h('div', { class: 'card stack morse-status' },
      h('div', { class: 'row nowrap' }, lamp, h('div', { class: 'grow' }, levelBar), pending),
      stats),
    transcript,
    h('div', { class: 'row' }, startBtn, isCam ? h('label', { class: 'check' }, autoBox, h('span', null, 'Find the blinking light automatically')) : null, isCam ? camSel : null, inPageRow),
    actions);

  const onVis = () => { if (document.visibilityState === 'hidden' && running) stopAll(); };
  document.addEventListener('visibilitychange', onVis);
  // a native capture that stopped by itself (permission revoked, device lost, interruption)
  app.on('host.capture.state', d => {
    if (!d || d.running !== false || !native || (d.kind && d.kind !== native)) return;
    native = null;
    setMsg('The ' + kindWord + ' stopped: ' + (d.error || 'unknown reason') + '. Press Start reading to try again.', 'bad');
    stopAll();
  });
  app.on('morse.text', d => { if (d && d.session === session && native) app.api.try('morse.rx.status', { session }, null, true).then(show); });
  return () => { document.removeEventListener('visibilitychange', onVis); stopAll(); };
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
