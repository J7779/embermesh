// Shared UI components: toasts, sheets (modal dialogs), confirm/prompt, QR view, avatars.
import { h, svg, clear, append } from './dom.js';
import { icon } from './icons.js';
import { initials, hueFor } from './format.js';
import { leave, ghostLeave, playClass, motionLevel } from './motion.js';

// ---------------------------------------------------------------- toasts
export function toast(text, opts = {}) {
  const host = document.getElementById('toasts');
  if (!host) return;
  const t = h('div', { class: 'toast ' + (opts.kind || ''), role: opts.kind === 'bad' || opts.kind === 'sos' ? 'alert' : 'status' },
    opts.icon ? icon(opts.icon) : null,
    h('div', { class: 'grow' }, opts.title ? h('strong', null, opts.title, h('br')) : null, String(text)),
    opts.action ? h('button', { class: 'btn small', onclick: () => { opts.action.run(); leave(t); } }, opts.action.label) : null,
    h('button', { class: 'btn small ghost icon-only', 'aria-label': 'Dismiss', onclick: () => leave(t) }, icon('close', { size: 18 })));
  host.appendChild(t);
  // Oldest go first; ones already leaving do not count.
  const live = [...host.children].filter(x => !x.classList.contains('leaving'));
  for (let i = 0; i < live.length - 4; i++) live[i].remove();
  setTimeout(() => leave(t), opts.ms || (opts.kind === 'sos' ? 15000 : 4500));
}

export function errorText(e) {
  if (!e) return 'Unknown error';
  const code = e.code ? String(e.code) : '';
  const msg = e.message && e.message !== code ? String(e.message) : '';
  const friendly = {
    not_implemented: 'Not available in this version yet.', unsupported: 'Not supported on this device.',
    locked: 'Storage is locked.', denied: 'Not allowed.', busy: 'Busy, try again shortly.', too_large: 'Too large.',
    bad_params: 'Invalid input.', not_found: 'Not found.', io_error: 'Storage or device error.', internal: 'Internal error.',
  }[code];
  return (friendly ? friendly + (msg ? ' ' + msg : '') : msg || code || String(e)).slice(0, 300);
}

export function toastError(e, prefix) {
  toast((prefix ? prefix + ': ' : '') + errorText(e), { kind: 'bad', icon: 'alert' });
}

// ---------------------------------------------------------------- sheets
let openSheets = [];
export function sheet(title, body, opts = {}) {
  const prevFocus = document.activeElement;
  const closeBtn = h('button', { class: 'btn ghost icon-only', 'aria-label': 'Close', onclick: () => close() }, icon('close'));
  const titleId = 'sheet-' + Math.random().toString(36).slice(2);
  const box = h('div', { class: 'sheet', role: 'dialog', 'aria-modal': 'true', 'aria-labelledby': titleId },
    h('div', { class: 'sheet-head' }, h('h2', { id: titleId }, title), closeBtn), body);
  const ov = h('div', { class: 'overlay', onclick: e => { if (e.target === ov && !opts.modal) close(); } }, box);
  const onKey = e => {
    if (e.key === 'Escape' && !opts.modal) { e.stopPropagation(); close(); }
    if (e.key === 'Tab') trapFocus(box, e);
  };
  ov.addEventListener('keydown', onKey);
  document.body.appendChild(ov);
  openSheets.push(close);
  let closed = false;
  function close(result) {
    if (closed) return;
    closed = true;
    // Gone for every purpose at once (focus, back button, tests); only its picture animates out.
    ov.removeEventListener('keydown', onKey);
    ghostLeave(ov, 'overlay', 'overlay-ghost', 240, [['sheet', 'sheet-ghost']]);
    openSheets = openSheets.filter(c => c !== close);
    if (prevFocus && prevFocus.focus) try { prevFocus.focus(); } catch { /* gone */ }
    opts.onClose && opts.onClose(result);
  }
  requestAnimationFrame(() => {
    const f = box.querySelector('[autofocus], input, select, textarea, button:not([aria-label="Close"])') || closeBtn;
    f.focus();
  });
  return { close, el: box };
}

export function closeAllSheets() { for (const c of [...openSheets]) c(); }

/** Closes the most recently opened sheet; false when none is open (Android back button). */
export function closeTopSheet() {
  const c = openSheets[openSheets.length - 1];
  if (!c) return false;
  c();
  return true;
}

function trapFocus(box, e) {
  const els = [...box.querySelectorAll('button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])')].filter(x => !x.disabled && x.offsetParent !== null);
  if (!els.length) return;
  const first = els[0], last = els[els.length - 1];
  if (e.shiftKey && document.activeElement === first) { last.focus(); e.preventDefault(); }
  else if (!e.shiftKey && document.activeElement === last) { first.focus(); e.preventDefault(); }
}

export function confirmDialog(title, text, opts = {}) {
  return new Promise(resolve => {
    let done = false;
    const s = sheet(title, h('div', { class: 'stack' },
      h('p', null, text),
      h('div', { class: 'row end' },
        h('button', { class: 'btn', onclick: () => { done = true; s.close(); resolve(false); } }, opts.cancel || 'Cancel'),
        h('button', { class: 'btn ' + (opts.danger ? 'danger solid' : 'primary'), onclick: () => { done = true; s.close(); resolve(true); } }, opts.ok || 'OK'))),
    { onClose: () => { if (!done) resolve(false); } });
  });
}

export function promptDialog(title, opts = {}) {
  return new Promise(resolve => {
    let done = false;
    const input = opts.multiline
      ? h('textarea', { rows: 4, value: opts.value || '', placeholder: opts.placeholder || '', autofocus: true })
      : h('input', { type: opts.password ? 'password' : 'text', value: opts.value || '', placeholder: opts.placeholder || '', autofocus: true, autocomplete: 'off' });
    const submit = () => { done = true; s.close(); resolve(input.value); };
    if (!opts.multiline) input.addEventListener('keydown', e => { if (e.key === 'Enter') submit(); });
    const s = sheet(title, h('div', { class: 'stack' },
      opts.text ? h('p', null, opts.text) : null,
      h('label', { class: 'field-label' }, opts.label || title, input),
      h('div', { class: 'row end' },
        h('button', { class: 'btn', onclick: () => { done = true; s.close(); resolve(null); } }, 'Cancel'),
        h('button', { class: 'btn primary', onclick: submit }, opts.ok || 'OK'))),
    { onClose: () => { if (!done) resolve(null); } });
  });
}

// ---------------------------------------------------------------- QR
/** rows: ["0101…"] from tools.qr → SVG element (quiet zone 4). */
export function qrSvg(rows, label = 'QR code') {
  const size = rows.length;
  const q = 4, n = size + 2 * q;
  let d = '';
  for (let y = 0; y < size; y++) {
    const r = String(rows[y] || '');
    let x = 0;
    while (x < size) {
      if (r[x] === '1') {
        let w = 1;
        while (x + w < size && r[x + w] === '1') w++;
        d += 'M' + (x + q) + ' ' + (y + q) + 'h' + w + 'v1h-' + w + 'z';
        x += w;
      } else x++;
    }
  }
  return svg('svg', { viewBox: '0 0 ' + n + ' ' + n, class: 'qr', role: 'img', 'aria-label': label, 'shape-rendering': 'crispEdges' },
    svg('rect', { width: n, height: n, fill: '#fff' }), svg('path', { d, fill: '#000' }));
}

/** Show text as QR + copyable text. Uses tools.qr; degrades to text only. */
export async function showQr(app, title, text, opts = {}) {
  const box = h('div', { class: 'center' }, h('div', { class: 'muted small' }, 'Generating QR…'));
  const copyBtn = h('button', { class: 'btn', onclick: () => copyText(text) }, icon('copy'), 'Copy text');
  const body = h('div', { class: 'stack' },
    opts.intro ? h('p', null, opts.intro) : null,
    box,
    h('div', { class: 'copybox', tabindex: '0' }, text),
    h('div', { class: 'row end' }, opts.extra || null, copyBtn));
  sheet(title, body);
  try {
    const r = await app.api.call('tools.qr', { text, ecc: opts.ecc || 'M' });
    clear(box).appendChild(qrSvg(r.rows, title));
  } catch (e) {
    clear(box).appendChild(h('p', { class: 'muted small' }, 'QR not available (' + errorText(e) + '). Copy the text instead.'));
  }
}

export async function copyText(text) {
  try {
    if (navigator.clipboard && window.isSecureContext) { await navigator.clipboard.writeText(text); toast('Copied', { kind: 'ok' }); return true; }
  } catch { /* fall through */ }
  try {
    const ta = h('textarea', { value: text, readOnly: true });
    ta.style.position = 'fixed'; ta.style.opacity = '0';
    document.body.appendChild(ta);
    ta.select();
    const ok = document.execCommand('copy');
    ta.remove();
    if (ok) { toast('Copied', { kind: 'ok' }); return true; }
  } catch { /* ignore */ }
  toast('Copy failed — select the text and copy manually', { kind: 'bad' });
  return false;
}

// ---------------------------------------------------------------- misc
/** Current motion level (re-exported for screens that only need to know whether to animate). */
export const motion = () => motionLevel();

export function avatar(id, name, opts = {}) {
  const a = h('span', { class: 'avatar' + (opts.channel ? ' ch' : ''), 'aria-hidden': 'true' }, opts.channel ? '#' : initials(name));
  const theme = document.documentElement.dataset.theme;
  if (theme !== 'night') a.style.borderColor = 'hsl(' + hueFor(id) + ' 60% 50%)';
  return a;
}

/** Empty state: icon in a soft tile, one line of text, optional action. */
export function empty(text, action, ic = 'info') {
  return h('div', { class: 'empty' }, ic ? h('span', { class: 'empty-icon', 'aria-hidden': 'true' }, icon(ic, { size: 32 })) : null, h('p', null, text), action || null);
}

/** Loading placeholder rows (shimmer only with full motion); replaced by the real list. */
export function skeletonRows(n = 4, tag = 'div') {
  const rows = [];
  for (let i = 0; i < n; i++) {
    rows.push(h(tag, { class: 'skel-row', 'aria-hidden': 'true' }, h('span', { class: 'skel skel-av' }),
      h('span', { class: 'skel-lines' }, h('span', { class: 'skel skel-line short', style: { display: 'block' } }), h('span', { class: 'skel skel-line long', style: { display: 'block' } }))));
  }
  return rows;
}

/** Sets a .bar / .meter-bar fill (an <i>) to pct 0–100 with a GPU-friendly transform. */
export function setFill(i, pct) {
  const v = Math.max(0, Math.min(100, Number(pct) || 0));
  i.style.transform = 'scaleX(' + (v / 100).toFixed(4) + ')';
  return i;
}

/** <div class="bar"><i/></div> filled to pct. */
export function bar(pct, cls = '', attrs = {}) {
  return h('div', { class: 'bar' + (cls ? ' ' + cls : ''), ...attrs }, setFill(h('i'), pct));
}

/** Signal strength 0–4 from SNR (dB) or, without it, RSSI (dBm). Pure; exported for tests. */
export function signalLevel(snr, rssi) {
  if (Number.isFinite(snr)) return snr >= 5 ? 4 : snr >= 0 ? 3 : snr >= -7 ? 2 : snr >= -15 ? 1 : 0;
  if (Number.isFinite(rssi)) return rssi >= -70 ? 4 : rssi >= -85 ? 3 : rssi >= -100 ? 2 : rssi >= -115 ? 1 : 0;
  return null;
}

/** Four signal bars; the level animates when it changes (full motion). */
export function signalBars(snr, rssi) {
  const lvl = signalLevel(snr, rssi);
  if (lvl == null) return null;
  return h('span', { class: 'sig l' + lvl, role: 'img', 'aria-label': 'Signal ' + lvl + ' of 4', title: [rssi != null ? rssi + ' dBm' : '', snr != null ? 'SNR ' + snr + ' dB' : ''].filter(Boolean).join(', ') },
    h('i'), h('i'), h('i'), h('i'));
}

/**
 * One-of-several choice drawn as a segmented control: a radio group (arrow keys move and select,
 * one Tab stop), not tabs.
 */
export function segmented(options, value, onChange, label) {
  const wrap = h('div', { class: 'segmented', role: 'radiogroup', 'aria-label': label || '' });
  const select = (i, focus) => {
    btns.forEach((b, k) => { b.setAttribute('aria-checked', String(k === i)); b.tabIndex = k === i ? 0 : -1; });
    if (focus) btns[i].focus();
    onChange(options[i].value);
  };
  const btns = options.map((o, i) => h('button', {
    role: 'radio', 'aria-checked': String(o.value === value), type: 'button', tabindex: o.value === value ? '0' : '-1',
    onclick: () => select(i, false),
    onkeydown: e => {
      const d = e.key === 'ArrowRight' || e.key === 'ArrowDown' ? 1 : e.key === 'ArrowLeft' || e.key === 'ArrowUp' ? -1 : 0;
      if (d) { e.preventDefault(); select((i + d + options.length) % options.length, true); }
    },
  }, o.label));
  if (!options.some(o => o.value === value) && btns[0]) btns[0].tabIndex = 0;
  append(wrap, ...btns);
  return wrap;
}

export function chips(options, value, onChange, label) {
  const wrap = h('div', { class: 'chips', role: 'group', 'aria-label': label || '' });
  const btns = options.map(o => h('button', {
    class: 'btn', type: 'button', 'aria-pressed': String(o.value === value),
    onclick: () => { for (const b of btns) b.setAttribute('aria-pressed', 'false'); btns[options.indexOf(o)].setAttribute('aria-pressed', 'true'); onChange(o.value); },
  }, o.label));
  append(wrap, ...btns);
  return wrap;
}

export function pageHead(title, ...actions) {
  return h('div', { class: 'page-head' }, h('h1', null, title), ...actions);
}

export function backButton(href, label = 'Back') {
  return h('a', { class: 'btn ghost icon-only', href, 'aria-label': label }, icon('back'));
}

/** Hold-to-confirm button. onDone fires after holdMs of continuous press (pointer or Space/Enter). */
export function holdButton(content, holdMs, onDone, opts = {}) {
  const ringC = 2 * Math.PI * 48;
  const prog = svg('circle', { cx: 50, cy: 50, r: 48, fill: 'none', stroke: 'currentColor', 'stroke-width': 4, 'stroke-dasharray': ringC, 'stroke-dashoffset': ringC });
  const ring = svg('svg', { class: 'ring', viewBox: '0 0 100 100', 'aria-hidden': 'true' }, prog);
  const ripple = h('span', { class: 'sos-ripple', 'aria-hidden': 'true' });
  const b = h('button', { class: opts.class || 'btn', type: 'button', 'aria-label': opts.label || null }, ring, ripple, content);
  let t0 = 0, raf = 0, timer = 0, step = 0;
  const stop = () => {
    cancelAnimationFrame(raf); clearTimeout(timer); clearInterval(step); t0 = 0; b.classList.remove('holding');
    prog.setAttribute('stroke-dashoffset', String(ringC));
  };
  // The ring is progress feedback, not decoration: it fills at every motion level (per frame;
  // in low-power mode in four steps).
  const draw = () => {
    if (!t0) return;
    const p = Math.min(1, (Date.now() - t0) / holdMs);
    prog.setAttribute('stroke-dashoffset', String(ringC * (1 - p)));
  };
  const tick = () => { draw(); if (t0) raf = requestAnimationFrame(tick); };
  const start = () => {
    if (t0 || b.disabled) return;
    t0 = Date.now();
    b.classList.add('holding');
    if (document.documentElement.dataset.lowpower === '1') { draw(); step = setInterval(draw, holdMs / 4); } else tick();
    // onDone runs on the timer, never after an animation: the ripple is fired alongside.
    timer = setTimeout(() => { stop(); playClass(ripple, 'run', 900); onDone(); }, holdMs);
  };
  b.addEventListener('pointerdown', e => { e.preventDefault(); try { b.setPointerCapture(e.pointerId); } catch { /* ignore */ } start(); });
  b.addEventListener('pointerup', stop);
  b.addEventListener('pointercancel', stop);
  b.addEventListener('pointerleave', stop);
  b.addEventListener('contextmenu', e => e.preventDefault());
  b.addEventListener('keydown', e => { if (e.key === ' ' || e.key === 'Enter') { e.preventDefault(); if (!e.repeat) start(); } });
  b.addEventListener('keyup', e => { if (e.key === ' ' || e.key === 'Enter') stop(); });
  b.addEventListener('blur', stop);
  return b;
}

export function b64FromBytes(bytes) {
  let s = '';
  for (let i = 0; i < bytes.length; i += 0x8000) s += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
  return btoa(s);
}

/** Pick a file: host.file.pick when the platform has it, else an <input type=file>. */
export function pickFile(app, accept) {
  if (app.has('file')) return app.api.call('host.file.pick', accept ? { accept } : {});
  return new Promise((resolve, reject) => {
    const inp = h('input', { type: 'file', accept: accept || null });
    inp.style.display = 'none';
    document.body.appendChild(inp);
    inp.addEventListener('change', async () => {
      const f = inp.files && inp.files[0];
      inp.remove();
      if (!f) return reject(Object.assign(new Error('No file chosen'), { code: 'bad_params' }));
      const buf = new Uint8Array(await f.arrayBuffer());
      resolve({ name: f.name, mime: f.type || 'application/octet-stream', data_b64: b64FromBytes(buf) });
    }, { once: true });
    inp.click();
  });
}

/** Save a file: host.file.save when available, else a download link. */
export async function saveFile(app, name, mime, data_b64) {
  if (app.has('file')) { await app.api.call('host.file.save', { name, mime, data_b64 }); return; }
  const bin = atob(data_b64);
  const bytes = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
  const url = URL.createObjectURL(new Blob([bytes], { type: mime }));
  const a = h('a', { href: url, download: name });
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 10000);
}

export const textToB64 = s => b64FromBytes(new TextEncoder().encode(s));
export function b64ToText(b) {
  const bin = atob(b);
  const bytes = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
  return new TextDecoder().decode(bytes);
}
