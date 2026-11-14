// Morse → Send: type a message and flash it with this device's flashlight (torch) or screen for
// another device's camera reader (Morse → Read by camera), or play it as a tone.
// Contract: docs/RPC_API.md §10 (host.signal.flash {pattern_ms, mode} → {used, stopped, timing},
// host.signal.stop, host.info.signal). UI rules kept here:
//   * light is not private — said plainly;
//   * screen flashing ≤ 3 flashes/s (speed clamped to SCREEN_MAX_WPM and said so), never automatic,
//     a warning before the first one (confirmed every time with reduced motion or Simple mode);
//   * camera-friendly default speed, progress (current character), time remaining, Stop at once;
//   * the typed text stays in this page: never stored (no kv, no localStorage), never logged.
import { h, clear, append } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { toastError, confirmDialog, segmented } from '../lib/ui.js';
import { buildSignal, lightWpm, maxFlashesPerSecond, dotMs, TORCH_DEFAULT_WPM, TORCH_MAX_WPM, SCREEN_MAX_WPM, MIN_WPM } from '../lib/morse.js';
import { announce } from '../lib/a11y.js';

const UNAVAILABLE = new Set(['unsupported', 'not_implemented']);
const MAX_TEXT = 200;
const SPEEDS = [
  { value: 6, label: 'Slow' },
  { value: TORCH_DEFAULT_WPM, label: 'Normal' },
  { value: TORCH_MAX_WPM, label: 'Fast' },
];
// "warned once" for screen flashing, for this page only (not stored)
let screenWarned = false;

/** host.info.signal.torch, or "no" for hosts that predate §10. Pure. */
export function torchState(host) {
  const t = host && host.signal && host.signal.torch;
  return t === 'yes' || t === 'ask' ? t : 'no';
}

/** Primary action for the torch state: {light, label}. Pure. */
export function primaryAction(torch) {
  return torch === 'yes' || torch === 'ask' ? { light: 'torch', label: 'Flash with flashlight' } : { light: 'screen', label: 'Flash with screen' };
}

/** What the result of a flash means, for the line shown afterwards. Pure. */
export function resultText(r, light) {
  const used = r && r.used;
  if (r && r.stopped) return 'Stopped.';
  const via = used === 'torch' ? 'the flashlight' : used === 'screen' ? 'the screen' : light === 'tone' ? 'sound' : '';
  let s = via ? 'Sent with ' + via + '.' : 'Sent.';
  if (used === 'screen' && r.fallback === 'no_torch') s += ' This device has no flashlight the app can switch, so the screen was used.';
  if (used === 'screen' && r.fallback === 'permission_denied') s += ' The camera permission was not given (the browser needs it to switch the flashlight), so the screen was used.';
  return s;
}

/** Why a torch flash failed, in words. Pure. */
export function torchErrorText(e) {
  if (e && e.code === 'permission_denied') return 'The flashlight needs the camera permission, and it was not given. You can allow the camera for this app in the browser or system settings — it is used only to switch the light on, nothing is recorded.';
  if (e && UNAVAILABLE.has(e.code)) return 'No flashlight the app can switch was found on this device.';
  return 'The flashlight could not be used: ' + ((e && e.message) || 'unknown error') + '.';
}

const fmtSecs = ms => { const s = Math.max(0, Math.ceil(ms / 1000)); return s >= 60 ? Math.floor(s / 60) + ' min ' + (s % 60) + ' s' : s + ' s'; };
const symbols = code => code.replace(/\./g, '·').replace(/-/g, '−');

export function sendPanel(app, box) {
  let torch = torchState(app.store.get('host'));
  let speed = TORCH_DEFAULT_WPM;
  let running = null; // {light, cancel(), started, signal}

  const text = h('textarea', { id: 'morse-send-text', rows: 3, maxlength: MAX_TEXT, autocomplete: 'off', autocapitalize: 'characters', spellcheck: 'false', value: 'SOS' });
  const preview = h('div', { class: 'morse-preview', 'aria-hidden': 'true' });
  const previewSr = h('p', { class: 'sr-only', id: 'morse-preview-sr' });
  const skippedEl = h('div', { class: 'notice small', hidden: true, role: 'status' });
  const estimate = h('p', { class: 'muted small' });
  const wpmInput = h('input', { type: 'number', id: 'morse-wpm', min: MIN_WPM, max: TORCH_MAX_WPM, step: 1, value: String(speed), inputmode: 'numeric' });
  const repeat = h('input', { type: 'checkbox', class: 'switch' });
  const screenNote = h('p', { class: 'small muted', hidden: true });
  const primary = h('button', { class: 'btn primary block morse-go', type: 'button' });
  const alt = h('button', { class: 'btn', type: 'button' });
  const toneBtn = h('button', { class: 'btn', type: 'button', 'data-secondary': '' }, icon('sound'), 'Play as sound');
  const askNote = h('p', { class: 'small muted', hidden: true }, 'The first time, the browser asks for the camera: it is needed only to switch its light on. Nothing is recorded or sent.');
  const actions = h('div', { class: 'stack morse-actions' }, primary, askNote, h('div', { class: 'row' }, alt, toneBtn));
  // while sending
  const statusLine = h('p', { class: 'morse-sending-title' });
  const remaining = h('p', { class: 'muted small', 'aria-live': 'off' });
  const stopBtn = h('button', { class: 'btn danger block morse-stop', type: 'button' }, icon('close'), 'Stop');
  const sending = h('div', { class: 'stack morse-sending', hidden: true }, statusLine, remaining, stopBtn);
  const result = h('div', { class: 'small morse-result', role: 'status' });

  const speedSeg = segmented(SPEEDS.map(s => ({ value: s.value, label: s.label + ' · ' + s.value })), speed, v => { speed = v; wpmInput.value = String(v); update(); }, 'Speed');

  function lightFor(kind) { return kind === 'torch' ? 'torch' : 'screen'; }

  function update() {
    const { label, light } = primaryAction(torch);
    append(clear(primary), icon(light === 'torch' ? 'flash' : 'sun'), label);
    append(clear(alt), icon('sun'), 'Use the screen instead');
    alt.hidden = light !== 'torch';
    askNote.hidden = torch !== 'ask';
    const sig = buildSignal(text.value, speed);
    // preview: each character with its dots and dashes
    clear(preview);
    sig.spans.forEach((sp, i) => {
      if (i && sp.word !== sig.spans[i - 1].word) preview.appendChild(h('span', { class: 'mgap' }));
      preview.appendChild(h('span', { class: 'mch' }, h('b', null, sp.ch), h('i', null, symbols(sp.code))));
    });
    previewSr.textContent = sig.morse ? 'In Morse: ' + sig.morse.replace(/\./g, 'dot ').replace(/-/g, 'dash ').replace(/ \/ /g, ', next word, ') : '';
    if (!sig.spans.length) preview.appendChild(h('span', { class: 'muted' }, 'Type a message to see it in Morse.'));
    skippedEl.hidden = !sig.skipped.length;
    skippedEl.textContent = sig.skipped.length ? 'Morse has no sign for ' + sig.skipped.map(c => '“' + c + '”').join(' ') + ' — ' + (sig.skipped.length === 1 ? 'it' : 'they') + ' will be skipped.' : '';
    const screenWpm = lightWpm(speed, 'screen');
    estimate.textContent = sig.spans.length ? 'About ' + fmtSecs(buildSignal(text.value, light === 'screen' ? screenWpm : lightWpm(speed, 'torch')).durationMs) + ' at ' + (light === 'screen' ? screenWpm : lightWpm(speed, 'torch')) + ' words per minute.' : '';
    screenNote.hidden = speed <= SCREEN_MAX_WPM;
    screenNote.textContent = 'With the screen: at most ' + SCREEN_MAX_WPM + ' words per minute (no more than three flashes a second, for people with photosensitive epilepsy).';
    primary.disabled = alt.disabled = toneBtn.disabled = !sig.spans.length;
  }

  text.addEventListener('input', () => { result.textContent = ''; update(); });
  wpmInput.addEventListener('change', () => {
    const v = lightWpm(wpmInput.value, 'torch');
    wpmInput.value = String(v);
    speed = v;
    for (const b of speedSeg.querySelectorAll('[role=radio]')) { const on = Number(b.textContent.split('·')[1]) === v; b.setAttribute('aria-checked', String(on)); b.tabIndex = on ? 0 : -1; }
    update();
  });

  async function screenOk() {
    const reduced = document.documentElement.dataset.motion !== 'full' || document.documentElement.dataset.simple === 'true';
    if (screenWarned && !reduced) return true;
    const ok = await confirmDialog('The whole screen will flash',
      'It flashes up to three times a second. Flashing light can trigger seizures in people with photosensitive epilepsy: do not watch it if that affects you, and warn people near you. Tap the screen to stop at any time.',
      { ok: 'Flash the screen', cancel: 'Cancel' });
    if (ok) screenWarned = true;
    return ok;
  }

  async function send(kind) {
    if (running) return;
    result.textContent = '';
    const light = kind === 'tone' ? 'tone' : lightFor(kind);
    if (light === 'screen' && !await screenOk()) return;
    const wpm = light === 'screen' ? lightWpm(speed, 'screen') : lightWpm(speed, 'torch');
    const sig = buildSignal(text.value, wpm);
    if (!sig.pattern.length) return;
    if (light === 'screen' && maxFlashesPerSecond(sig.pattern) > 3) return; // never: clamped above
    if (sig.durationMs > 600000) { result.textContent = 'This message takes longer than 10 minutes at ' + wpm + ' words per minute. Shorten it or choose a faster speed.'; return; }
    let cancelled = false;
    const run = { light, cancel: () => { cancelled = true; } };
    running = run;
    sending.hidden = false;
    actions.hidden = true;
    const how = light === 'torch' ? 'flashlight' : light === 'screen' ? 'screen' : 'sound';
    statusLine.textContent = 'Sending with the ' + how + (wpm !== speed && light === 'screen' ? ' at ' + wpm + ' words per minute' : '') + '…';
    announce('Sending with the ' + how + '.');
    stopBtn.focus();
    let last = null;
    try {
      do {
        const t0 = performance.now();
        const tick = () => {
          if (running !== run) return;
          const el = performance.now() - t0;
          remaining.textContent = fmtSecs(sig.durationMs - el) + ' left' + (repeat.checked ? ' in this round' : '');
          const spans = preview.querySelectorAll('.mch');
          sig.spans.forEach((sp, i) => { if (spans[i]) spans[i].classList.toggle('now', el >= sp.start && el < (sig.spans[i + 1] ? sig.spans[i + 1].start : sp.end + 1)); });
          if (el < sig.durationMs) run.timer = setTimeout(tick, Math.min(250, dotMs(wpm)));
        };
        tick();
        if (light === 'tone') last = await playTone(app, sig.pattern, () => cancelled);
        else {
          try {
            last = await app.api.call('host.signal.flash', { pattern_ms: sig.pattern, mode: light });
          } catch (e) {
            if (light === 'torch') {
              // explain, refresh what the host now knows (a browser remembers "no torch"), offer the screen
              const hi = await app.api.try('host.info', {}, null, true);
              if (hi) { app.store.set({ host: hi }); torch = torchState(hi); }
              result.textContent = torchErrorText(e);
              announce(torchErrorText(e));
              break;
            }
            if (UNAVAILABLE.has(e.code)) last = await pageScreenFlash(sig.pattern, () => cancelled);
            else throw e;
          }
        }
        clearTimeout(run.timer);
        if (last && last.stopped) cancelled = true;
        if (repeat.checked && !cancelled) await pause(Math.max(2000, 7 * dotMs(wpm)), () => cancelled);
      } while (repeat.checked && !cancelled);
      if (last) {
        const r = cancelled ? { ...last, stopped: true } : last;
        let msg = resultText(r, light);
        const t = last.timing;
        if (!r.stopped && t && Number(t.max_late_ms) > dotMs(wpm) * 0.3) msg += ' The device was busy and the timing slipped by up to ' + Math.round(t.max_late_ms) + ' ms: if the reader missed letters, send again.';
        result.textContent = msg;
        announce(msg);
      }
    } catch (e) {
      toastError(e, 'Could not send');
    } finally {
      clearTimeout(run.timer);
      running = null;
      sending.hidden = true;
      actions.hidden = false;
      for (const m of preview.querySelectorAll('.mch.now')) m.classList.remove('now');
      update();
      try { primary.focus(); } catch { /* gone */ }
    }
  }

  stopBtn.addEventListener('click', async () => {
    if (!running) return;
    running.cancel();
    if (running.light === 'tone' && !app.has('tone')) return; // page tone: cancelled above
    try { await app.api.call('host.signal.stop'); } catch (e) {
      if (UNAVAILABLE.has(e.code)) { statusLine.textContent = 'This device cannot stop the light early: it stops when the message ends.'; announce(statusLine.textContent); }
    }
  });
  primary.addEventListener('click', () => send(primaryAction(torch).light));
  alt.addEventListener('click', () => send('screen'));
  toneBtn.addEventListener('click', () => send('tone'));

  update();
  append(box,
    h('section', { class: 'card stack morse-send', 'aria-labelledby': 'morse-send-h' },
      h('h2', { id: 'morse-send-h' }, 'Send a message by light'),
      h('div', { class: 'notice info small' }, icon('globe', { size: 16 }), ' Light is not private: anyone who can see it can read it. It is not encrypted.'),
      h('div', null, h('label', { class: 'field-label', for: 'morse-send-text' }, 'Message'), text,
        h('div', { class: 'field-help' }, 'Letters, numbers and common punctuation. Prosigns as <AR>, <SK>. Kept on this page only.')),
      h('div', { class: 'morse-preview-wrap' }, h('div', { class: 'field-label' }, 'In Morse'), preview, previewSr), skippedEl, estimate,
      h('div', null, h('div', { class: 'field-label', id: 'morse-speed-label' }, 'Speed'), speedSeg,
        h('p', { class: 'field-help' }, 'Normal (' + TORCH_DEFAULT_WPM + ' words per minute) is what a phone camera reads reliably.'), screenNote,
        h('details', { class: 'advanced', 'data-secondary': '' }, h('summary', null, 'Exact speed'),
          h('label', { class: 'field-label', for: 'morse-wpm' }, 'Words per minute (' + MIN_WPM + '–' + TORCH_MAX_WPM + ')'), wpmInput)),
      h('label', { class: 'check' }, repeat, h('span', null, 'Repeat until I stop')),
      actions, sending, result),
    h('section', { class: 'card stack morse-howto' },
      h('h2', null, 'How the other person reads it'),
      h('p', { class: 'small' }, 'On their EmberMesh: Tools → Morse → Read by camera, and point the camera at your light. Start sending after they press Start reading. In the dark a flashlight carries much further than a screen.'),
      h('a', { class: 'btn', href: '#/tools/morse?tab=camera' }, icon('scan'), 'Open the camera reader')));
  return () => { if (running) { running.cancel(); app.api.try('host.signal.stop', {}, null, true); } };
}

function pause(ms, cancelled) {
  return new Promise(res => {
    const t0 = Date.now();
    const tick = () => (cancelled() || Date.now() - t0 >= ms) ? res() : setTimeout(tick, 50);
    tick();
  });
}

/** In-page screen flash for hosts without host.signal.flash (deadline-scheduled, tap to stop). */
async function pageScreenFlash(pattern, cancelled) {
  const ov = h('div', { class: 'flash-overlay', role: 'button', tabindex: '0', 'aria-label': 'Flashing Morse. Tap to stop' });
  const stop = () => { ov.dataset.stop = '1'; };
  ov.addEventListener('click', stop);
  ov.addEventListener('keydown', e => { if (e.key === 'Escape' || e.key === 'Enter' || e.key === ' ') stop(); });
  document.body.appendChild(ov);
  const hidden = () => { if (document.visibilityState === 'hidden') stop(); };
  document.addEventListener('visibilitychange', hidden);
  try { ov.focus(); } catch { /* old */ }
  try {
    const t0 = performance.now();
    let due = 0;
    for (let i = 0; i < pattern.length; i++) {
      if (cancelled() || ov.dataset.stop) break;
      ov.classList.toggle('on', i % 2 === 0);
      due += pattern[i];
      await pause(Math.max(0, t0 + due - performance.now()), () => cancelled() || !!ov.dataset.stop);
    }
  } finally {
    ov.remove();
    document.removeEventListener('visibilitychange', hidden);
  }
  return { used: 'screen', stopped: !!(cancelled() || ov.dataset.stop) };
}

async function playTone(app, pattern, cancelled) {
  if (app.has('tone')) {
    try { const r = await app.api.call('host.signal.tone', { pattern_ms: pattern, freq_hz: 700 }); return { ...(r || {}), stopped: !!(r && r.stopped) || cancelled() }; } catch (e) { if (!UNAVAILABLE.has(e.code)) throw e; }
  }
  const AC = window.AudioContext || window.webkitAudioContext;
  if (!AC) throw Object.assign(new Error('No audio output available'), { code: 'unsupported' });
  const ctx = new AC();
  const osc = ctx.createOscillator(), gain = ctx.createGain();
  osc.frequency.value = 700; gain.gain.value = 0;
  osc.connect(gain).connect(ctx.destination);
  let t = ctx.currentTime + 0.05;
  for (let i = 0; i < pattern.length; i++) { gain.gain.setValueAtTime(i % 2 === 0 ? 0.4 : 0, t); t += pattern[i] / 1000; }
  gain.gain.setValueAtTime(0, t);
  osc.start(); osc.stop(t + 0.05);
  await pause((t - ctx.currentTime) * 1000 + 100, cancelled);
  try { osc.stop(); } catch { /* stopped */ }
  ctx.close();
  return { stopped: cancelled() };
}
