// Rate limiting for toasts / system notifications so a flood of (possibly forged) messages or SOS
// alerts cannot bury the screen or spam the user. Pure; tested in ui/tests/core_spec.test.js.
//
//   const rl = createLimiter({ perSourceMs: 60000, maxPerWindow: 5, windowMs: 60000 });
//   if (rl.allow('sos:' + nodeId)) toast(...); else rl.suppressed  // count of dropped ones
export function createLimiter({ perSourceMs = 60000, maxPerWindow = 5, windowMs = 60000, now = () => Date.now() } = {}) {
  const lastBySource = new Map();
  let recent = [];
  const st = {
    suppressed: 0,
    allow(source) {
      const t = now();
      recent = recent.filter(x => t - x < windowMs);
      const key = String(source || '');
      const last = lastBySource.get(key);
      if ((last !== undefined && t - last < perSourceMs) || recent.length >= maxPerWindow) { st.suppressed++; return false; }
      lastBySource.set(key, t);
      if (lastBySource.size > 500) lastBySource.delete(lastBySource.keys().next().value);
      recent.push(t);
      return true;
    },
    /** Returns and resets the number of suppressed notifications. */
    takeSuppressed() { const n = st.suppressed; st.suppressed = 0; return n; },
  };
  return st;
}

/** Sender is not verified: bad/unknown signature, or the core marks it unverified. */
export const unverified = x => !!x && (x.signed_ok === false || x.verified === false);

/** Random passphrase with ~100 bits of entropy, readable aloud (no 0/O/1/l/I). */
export function strongPassphrase(rand = n => { const b = new Uint8Array(n); globalThis.crypto.getRandomValues(b); return b; }) {
  const A = 'abcdefghjkmnpqrstuvwxyz23456789'; // 31 symbols
  // Redrawn until it has both a letter and a digit (about 1 draw in 400 has no digit): an all-letter
  // result reads like five short words, which passphraseStrength() below rates as a word list.
  // Costs under 0.01 of the ~99 bits.
  for (let tries = 0; ; tries++) {
    const out = [];
    let bytes = rand(64), k = 0;
    while (out.length < 20) {
      if (k >= bytes.length) { bytes = rand(64); k = 0; }
      const v = bytes[k++];
      if (v < 248) out.push(A[v % 31]); // 248 = 8 * 31: unbiased
    }
    const s = out.join('');
    if ((/[0-9]/.test(s) && /[a-z]/.test(s)) || tries >= 100) return s.match(/.{4}/g).join('-');
  }
}

/**
 * Rough offline-guessing strength of a channel passphrase before it is sent to the core (whose
 * `channels.create` → passphrase_strength is authoritative): "weak" | "fair" | "strong".
 */
export function passphraseStrength(p) {
  const s = String(p || '');
  if (!s) return 'weak';
  let pool = 0;
  if (/[a-z]/.test(s)) pool += 26;
  if (/[A-Z]/.test(s)) pool += 26;
  if (/[0-9]/.test(s)) pool += 10;
  if (/[^a-zA-Z0-9]/.test(s)) pool += 20;
  const uniq = new Set(s).size;
  let bits = Math.min(s.length, uniq * 2) * Math.log2(Math.max(pool, 2));
  const words = s.split(/[\s\-_.]+/).filter(Boolean);
  if (words.length >= 2 && words.every(w => /^[a-z]+$/i.test(w))) bits = Math.min(bits, words.length * 12); // dictionary words: ~12 bits each
  else if (/^[a-z]+\d{0,4}[!?.]?$/i.test(s)) bits = Math.min(bits, 30); // word + digits: dictionary attack
  return bits < 45 ? 'weak' : bits < 70 ? 'fair' : 'strong';
}
