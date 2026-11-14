// Settings → Accessibility (#/settings/accessibility): Simple mode, text size, contrast, motion,
// start-up animation, reading spacing, haptics, keyboard shortcuts, what screen readers hear.
// Preferences live in js/lib/prefs.js (kv `ui` + local-storage mirror, docs/RPC_API.md §8).
// Also exports the pieces other screens embed: settingsEntry() for the top of Settings and
// simpleOffer(app) for first-run setup.
import { h, clear, append } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { getPrefs, setPref, onPrefsChange, TEXT_SCALE_MIN, TEXT_SCALE_MAX } from '../lib/prefs.js';
import { announce, haptic, hapticSupported, showShortcuts } from '../lib/a11y.js';

const pct = v => Math.round(v * 100) + ' %';
let uid = 0;

/** Radio group drawn as a row of large buttons (native radios: arrow keys, one tab stop). */
function choice(legend, name, options, value, onChange, note) {
  const id = 'a11y-' + name + '-' + (++uid);
  const fs = h('fieldset', { class: 'choice' }, h('legend', { class: 'field-label' }, legend));
  const row = h('div', { class: 'choice-row' });
  for (const o of options) {
    const inp = h('input', { type: 'radio', name: id, value: o.value, checked: o.value === value });
    inp.addEventListener('change', () => { if (inp.checked) onChange(o.value); });
    row.appendChild(h('label', { class: 'choice-opt' }, inp, h('span', null, o.label)));
  }
  append(fs, row, note || null);
  return fs;
}

function switchRow(label, help, checked, onChange) {
  const inp = h('input', { type: 'checkbox', checked: !!checked });
  inp.addEventListener('change', () => onChange(inp.checked));
  const helpId = help ? 'a11y-help-' + (++uid) : null;
  if (helpId) inp.setAttribute('aria-describedby', helpId);
  return { el: h('label', { class: 'switch-row' }, inp, h('span', { class: 'grow' }, h('strong', null, label), help ? h('span', { class: 'field-help', id: helpId, style: { display: 'block' } }, help) : null)), input: inp };
}

const save = (key, value, spoken) => setPref(key, value).then(() => { if (spoken) announce(spoken); }).catch(e => console.error(e));

export default async function render(app, _params, root) {
  const page = h('div', { class: 'page a11y-page' });
  root.appendChild(page);
  const p = getPrefs();

  // ---- Simple mode
  const simple = switchRow('Simple mode', 'Bigger text and buttons, fewer choices on each screen, plain words. Everything else stays under “More options” and “More”.', p.simple_mode,
    v => save('simple_mode', v, v ? 'Simple mode on' : 'Simple mode off'));
  simple.el.classList.add('simple-big');

  // ---- text size
  const range = h('input', { type: 'range', min: TEXT_SCALE_MIN, max: TEXT_SCALE_MAX, step: 0.05, value: p.text_scale, 'aria-label': 'Text size', 'aria-describedby': 'a11y-scale-out' });
  const out = h('output', { class: 'scale-value', id: 'a11y-scale-out' }, pct(p.text_scale));
  const setScale = (v, spoken) => { range.value = v; out.textContent = pct(Number(v)); range.setAttribute('aria-valuetext', pct(Number(v))); save('text_scale', Number(v), spoken ? 'Text size ' + pct(Number(v)) : null); };
  range.setAttribute('aria-valuetext', pct(p.text_scale));
  range.addEventListener('input', () => setScale(range.value, false));
  range.addEventListener('change', () => setScale(range.value, true));
  const presets = h('div', { class: 'row', role: 'group', 'aria-label': 'Text size presets' },
    [[1, 'Normal'], [1.25, 'Larger'], [1.5, 'Largest']].map(([v, l]) => h('button', { type: 'button', class: 'btn', onclick: () => setScale(v, true) }, l + ' (' + pct(v) + ')')));
  const fontNote = h('p', { class: 'follow' });
  const text = h('section', { class: 'card stack', 'aria-labelledby': 'a11y-h-text' },
    h('h2', { id: 'a11y-h-text' }, 'Text size'),
    h('div', { class: 'row nowrap' }, h('div', { class: 'grow' }, range), out),
    presets,
    h('div', { class: 'preview', 'aria-hidden': 'true' },
      h('div', { class: 'pv-title' }, 'Preview'),
      h('p', null, 'Boil water for one minute before drinking. Above 2,000 m, boil for three minutes.'),
      h('div', { class: 'msg' }, h('div', { class: 'from' }, 'Sam'), h('div', { class: 'text' }, 'I’m at the school shelter. We have water.'))),
    fontNote);

  // ---- contrast
  const contrastNote = h('p', { class: 'follow' });
  const contrast = h('section', { class: 'card stack', 'aria-labelledby': 'a11y-h-contrast' },
    h('h2', { id: 'a11y-h-contrast' }, 'Contrast'),
    choice('High contrast', 'contrast', [{ value: 'system', label: 'Follow system' }, { value: 'on', label: 'On' }, { value: 'off', label: 'Off' }],
      p.high_contrast === null ? 'system' : p.high_contrast ? 'on' : 'off',
      v => save('high_contrast', v === 'system' ? null : v === 'on', v === 'on' ? 'High contrast on' : v === 'off' ? 'High contrast off' : 'High contrast follows the system'), contrastNote),
    h('p', { class: 'muted small' }, 'Stronger text, borders and focus outlines in every theme, including the red night theme. Choose the theme in Settings → Region & display.'));

  // ---- motion
  const motionNote = h('p', { class: 'follow' });
  const motion = h('section', { class: 'card stack', 'aria-labelledby': 'a11y-h-motion' },
    h('h2', { id: 'a11y-h-motion' }, 'Motion'),
    choice('Animation', 'motion', [{ value: 'system', label: 'Follow system' }, { value: 'full', label: 'Full' }, { value: 'reduced', label: 'Reduced' }, { value: 'off', label: 'Off' }],
      p.motion, v => save('motion', v, 'Animation: ' + v), motionNote),
    choice('Start-up animation', 'boot', [{ value: 'full', label: 'Full' }, { value: 'short', label: 'Short' }, { value: 'off', label: 'Off' }],
      p.boot_animation, v => save('boot_animation', v, 'Start-up animation: ' + v),
      h('p', { class: 'muted small' }, 'Plays each time the app starts, also when your device is set to reduce motion: Full about 2.5 seconds, Short about 1 second. Tap or press any key to skip it. To stop it, choose Off here, set Animation to Reduced or Off, or turn on Simple mode.')));

  // ---- reading and touch
  const spacing = switchRow('Extra reading space', 'More space between letters, words and lines. Can help with dyslexia. No extra fonts are downloaded.', p.reading_spacing,
    v => save('reading_spacing', v, v ? 'Extra reading space on' : 'Extra reading space off'));
  const canHaptic = hapticSupported(app);
  const hap = canHaptic ? switchRow('Vibrate on touch', 'A short vibration confirms each button press, a pattern for SOS and for errors.', p.haptics,
    v => { save('haptics', v); if (v) haptic('tap'); }) : null;
  const keys = switchRow('Keyboard shortcuts', 'Press g and a letter to jump to a screen, ? for the list. Turn off if they clash with your screen reader or switch control.', p.shortcuts,
    v => save('shortcuts', v, v ? 'Shortcuts on' : 'Shortcuts off'));
  const reading = h('section', { class: 'card stack', 'aria-labelledby': 'a11y-h-reading' },
    h('h2', { id: 'a11y-h-reading' }, 'Reading, touch and keys'),
    spacing.el,
    hap ? hap.el : h('p', { class: 'muted small' }, 'This device cannot vibrate, so there is no touch feedback setting.'),
    keys.el,
    h('button', { type: 'button', class: 'btn', onclick: () => showShortcuts() }, 'Show keyboard shortcuts'));

  // ---- screen readers
  const sr = h('section', { class: 'card stack', 'aria-labelledby': 'a11y-h-sr' },
    h('h2', { id: 'a11y-h-sr' }, 'Screen reader'),
    h('p', null, 'EmberMesh works with TalkBack, VoiceOver, NVDA, JAWS and Narrator. It reads out:'),
    h('ul', null,
      h('li', null, 'new messages in the conversation you have open, and a short notice for other conversations;'),
      h('li', null, 'your SOS turning on or off, and each person who heard it — straight away;'),
      h('li', null, 'messages that could not be sent, and when a message arrived;'),
      h('li', null, 'connections that start or stop working, and when the app locks.')),
    h('p', null, 'When many things happen at once, it says “more updates” instead of reading every one. After moving to another screen, the screen’s title is read first; “Skip to content” is the first thing on every page.'),
    h('p', { class: 'follow', id: 'a11y-sr-state' }));

  append(page,
    h('div', { class: 'page-head' }, h('a', { class: 'btn ghost icon-only', href: '#/settings', 'aria-label': 'Back to settings' }, icon('back')), h('h1', null, 'Accessibility')),
    h('section', { class: 'card stack simple-offer', 'aria-label': 'Simple mode' }, simple.el),
    text, contrast, motion, reading, sr);

  // ---- "following the system" notes, kept live
  const notes = () => {
    const q = getPrefs();
    const e = q.effective;
    const sys = e.system;
    fontNote.textContent = sys.fontScale && sys.fontScale !== 1 ? 'Your device’s own text size (' + pct(sys.fontScale) + ') applies as well.' : 'Your device’s own text size setting applies as well.';
    contrastNote.textContent = q.high_contrast === null ? 'Following the system: high contrast is ' + (sys.highContrast ? 'on' : 'off') + (e.forcedColors ? ' (system colours are forced).' : '.') : 'Your choice: high contrast is ' + (e.contrast === 'high' ? 'on' : 'off') + '.';
    contrastNote.classList.toggle('on', q.high_contrast === null);
    const why = { system: 'your system asks for reduced motion', low_power: 'low-power mode limits animation', simple_mode: 'Simple mode limits animation', test: 'motion is switched off for testing', default: '', preference: '' }[e.why.motion] || '';
    motionNote.textContent = (q.motion === 'system' ? 'Following the system: ' : 'Now: ') + ({ full: 'full animation', reduced: 'reduced animation', off: 'no animation' }[e.motion]) + (why ? ' — ' + why + '.' : '.');
    // the launch animation does not follow the system setting (RPC_API §8): say so where it matters
    if (q.motion === 'system' && e.motion !== 'full') motionNote.textContent += ' The start-up animation still plays (see below).';
    motionNote.classList.toggle('on', q.motion === 'system');
    const srEl = page.querySelector('#a11y-sr-state');
    if (srEl) srEl.textContent = e.screenReader ? 'A screen reader is running on this device.' : '';
    for (const [inp, v] of [[simple.input, q.simple_mode], [spacing.input, q.reading_spacing], [keys.input, q.shortcuts], [hap && hap.input, q.haptics]]) if (inp && inp.checked !== !!v) inp.checked = !!v;
    if (Number(range.value) !== q.text_scale && document.activeElement !== range) { range.value = q.text_scale; out.textContent = pct(q.text_scale); }
  };
  notes();
  const off = onPrefsChange(notes);
  return () => off();
}

/** The entry at the top of Settings. */
export function settingsEntry() {
  return h('a', { class: 'card item a11y-entry', href: '#/settings/accessibility' }, icon('info', { size: 28 }),
    h('div', { class: 'grow' }, h('div', { class: 'title' }, 'Accessibility'), h('div', { class: 'sub' }, 'Simple mode, text size, contrast, motion')), icon('next'));
}

/** The one-time offer during first-run setup. */
export function simpleOffer() {
  const p = getPrefs();
  // Not .primary: the setup screen's own primary button (Continue) must stay the only one.
  const btn = h('button', { type: 'button', class: 'btn simple-offer-btn', 'aria-pressed': String(!!p.simple_mode) });
  const label = () => { const on = getPrefs().simple_mode; clear(btn).appendChild(document.createTextNode(on ? 'Simpler is on — tap to undo' : 'Make it simpler: bigger text and fewer buttons')); btn.setAttribute('aria-pressed', String(on)); };
  btn.addEventListener('click', async () => { const v = !getPrefs().simple_mode; await setPref('simple_mode', v); label(); announce(v ? 'Simple mode on' : 'Simple mode off'); });
  label();
  return h('section', { class: 'card stack simple-offer', 'aria-label': 'Make it simpler' }, btn,
    h('p', { class: 'muted small' }, 'You can change this any time in Settings → Accessibility.'));
}
