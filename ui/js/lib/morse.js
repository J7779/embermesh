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
