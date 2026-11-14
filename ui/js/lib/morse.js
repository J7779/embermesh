// International Morse code → on/off timing patterns for host.signal.flash / host.signal.tone.
// Pure. pattern_ms = [on, off, on, off, …] (always starts with "on").

export const MORSE = {
  A: '.-', B: '-...', C: '-.-.', D: '-..', E: '.', F: '..-.', G: '--.', H: '....', I: '..', J: '.---',
  K: '-.-', L: '.-..', M: '--', N: '-.', O: '---', P: '.--.', Q: '--.-', R: '.-.', S: '...', T: '-',
  U: '..-', V: '...-', W: '.--', X: '-..-', Y: '-.--', Z: '--..',
  0: '-----', 1: '.----', 2: '..---', 3: '...--', 4: '....-', 5: '.....', 6: '-....', 7: '--...', 8: '---..', 9: '----.',
  '.': '.-.-.-', ',': '--..--', '?': '..--..', "'": '.----.', '!': '-.-.--', '/': '-..-.', '(': '-.--.', ')': '-.--.-',
  '&': '.-...', ':': '---...', ';': '-.-.-.', '=': '-...-', '+': '.-.-.', '-': '-....-', '_': '..--.-', '"': '.-..-.',
  '@': '.--.-.',
};

/** Text → Morse string with letters separated by ' ' and words by ' / '. Unknown chars dropped. */
export function toMorse(text) {
  const s = String(text || '').toUpperCase();
  if (/^\s*SOS\s*$/.test(s)) return '...---...';
  return s.trim().split(/\s+/).map(w => [...w].map(ch => MORSE[ch]).filter(Boolean).join(' ')).filter(Boolean).join(' / ');
}

/** Dot length in ms for words-per-minute (PARIS standard). */
export const dotMs = wpm => Math.round(1200 / Math.max(1, Math.min(60, wpm || 12)));

/** Morse string (from toMorse) → [on, off, …] in ms. Inter-element 1, letter 3, word 7 units. */
export function morsePattern(morse, wpm = 12) {
  const u = dotMs(wpm);
  const out = [];
  const words = String(morse).split(' / ');
  words.forEach((w, wi) => {
    const letters = w.split(' ');
    letters.forEach((l, li) => {
      [...l].forEach((sym, si) => {
        if (sym !== '.' && sym !== '-') return;
        out.push(sym === '.' ? u : 3 * u);
        const lastSym = si === l.length - 1;
        const lastLetter = li === letters.length - 1;
        const lastWord = wi === words.length - 1;
        if (!lastSym) out.push(u);
        else if (!lastLetter) out.push(3 * u);
        else if (!lastWord) out.push(7 * u);
      });
    });
  });
  // Pattern must alternate on/off; drop a trailing gap.
  if (out.length % 2 === 0) out.pop();
  return out;
}

export const patternDuration = p => p.reduce((a, b) => a + b, 0);

// ------------------------------------------------------------------ sending by light (RPC_API §10)
//
// Speeds, from the camera reader's measured limits (core/src/morse/README.md "Measured ranges";
// core/src/morse/rx.cpp): the light decoder holds CER ≤ 5 % from about 2 camera frames per dot
// (30 fps → ~20 WPM, 24 fps → ~15 WPM, 15 fps → ~9 WPM) and breaks down below ~1.8 frames per dot;
// it bridges drop-outs of up to 1.6 frame intervals. A phone camera adds auto-exposure lag (an edge
// can land a frame late) and frame-time jitter, so we keep ≥ 3 frames per dot at 30 fps for the
// default — 12 WPM (100 ms dot = 3.0 frames at 30 fps, 2.4 at 24 fps) — and cap the torch at
// 15 WPM (80 ms dot = 2.4 frames at 30 fps: one late frame still leaves > 1.8, and a 24 fps camera
// is at its measured ~15 WPM limit). Slower than 3 WPM only makes a message long (the decoder's
// floor is 2 WPM). Screen flashing is a photosensitivity limit, not a camera one: at most three
// flashes per second (WCAG 2.3.1) → a dot of at least 170 ms → 7 WPM (171 ms dot; a run of dots
// flashes 2.9 times a second).
export const MIN_WPM = 3;
export const TORCH_DEFAULT_WPM = 12;
export const TORCH_MAX_WPM = 15;
export const SCREEN_MAX_WPM = 7;
export const SCREEN_MIN_DOT_MS = 170;

/** Prosigns typed as <XX> (letters run together, no letter gap). SOS alone is sent as one sign. */
export const PROSIGNS = { AR: '.-.-.', AS: '.-...', BT: '-...-', CT: '-.-.-', HH: '........', KN: '-.--.', SK: '...-.-', SN: '...-.', SOS: '...---...' };

/**
 * Text → {words:[[{ch, code}]], skipped:[chars], morse}. Letters/digits/punctuation of MORSE, <XX>
 * prosigns; anything Morse cannot express is listed in `skipped` (once each) and left out — the UI
 * shows it before sending. Pure.
 */
export function encodeText(text) {
  const s = String(text || '').toUpperCase();
  const skipped = [];
  const words = [];
  if (/^\s*SOS\s*$/.test(s)) return { words: [[{ ch: 'SOS', code: PROSIGNS.SOS, prosign: true }]], skipped, morse: PROSIGNS.SOS };
  for (const w of s.trim().split(/\s+/).filter(Boolean)) {
    const out = [];
    for (let i = 0; i < w.length; i++) {
      const pm = w[i] === '<' ? /^<([A-Z]{2,3})>/.exec(w.slice(i)) : null;
      if (pm && PROSIGNS[pm[1]]) { out.push({ ch: pm[0], code: PROSIGNS[pm[1]], prosign: true }); i += pm[0].length - 1; continue; }
      const code = MORSE[w[i]];
      if (code) out.push({ ch: w[i], code });
      else if (!skipped.includes(w[i])) skipped.push(w[i]);
    }
    if (out.length) words.push(out);
  }
  return { words, skipped, morse: words.map(w => w.map(c => c.code).join(' ')).join(' / ') };
}

/** Words per minute actually used for a light: clamped to [MIN_WPM, max for that light]. Pure. */
export function lightWpm(wpm, light = 'torch') {
  const max = light === 'screen' ? SCREEN_MAX_WPM : TORCH_MAX_WPM;
  const n = Math.round(Number(wpm));
  return Math.max(MIN_WPM, Math.min(max, Number.isFinite(n) ? n : TORCH_DEFAULT_WPM));
}

/**
 * The signal for a text at a speed: {pattern (on,off,… ms), spans:[{ch, code, word, start, end}] (when each
 * character is lit, ms from the start, for the progress highlight), durationMs, skipped, morse}.
 * Same timing as morsePattern (1/3/7 units). Pure.
 */
export function buildSignal(text, wpm = TORCH_DEFAULT_WPM) {
  const enc = encodeText(text);
  const u = dotMs(wpm);
  const pattern = [];
  const spans = [];
  let t = 0;
  enc.words.forEach((w, wi) => {
    w.forEach((c, ci) => {
      const start = t;
      [...c.code].forEach((sym, si) => {
        const on = sym === '.' ? u : 3 * u;
        pattern.push(on); t += on;
        const lastSym = si === c.code.length - 1;
        const lastChar = ci === w.length - 1;
        const lastWord = wi === enc.words.length - 1;
        const gap = !lastSym ? u : !lastChar ? 3 * u : !lastWord ? 7 * u : 0;
        if (gap) { pattern.push(gap); if (!lastSym) t += gap; }
      });
      spans.push({ ch: c.ch, code: c.code, word: wi, start, end: t });
      // the letter/word gap belongs to no character
      const last = pattern.length % 2 === 0 ? pattern[pattern.length - 1] : 0;
      t += last && c.code.length ? last : 0;
    });
  });
  if (pattern.length % 2 === 0) pattern.pop();
  return { pattern, spans, durationMs: patternDuration(pattern), skipped: enc.skipped, morse: enc.morse };
}

/** Most "on" edges in any 1-second window (photosensitivity check for the screen). Pure. */
export function maxFlashesPerSecond(pattern) {
  const starts = [];
  let t = 0;
  for (let i = 0; i < pattern.length; i++) { if (i % 2 === 0) starts.push(t); t += pattern[i]; }
  let best = 0;
  for (let i = 0, j = 0; i < starts.length; i++) { while (starts[i] - starts[j] >= 1000) j++; best = Math.max(best, i - j + 1); }
  return best;
}
