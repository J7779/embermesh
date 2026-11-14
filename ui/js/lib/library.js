// Pure helpers for the library's smart (meaning-based) search, Ask and Related (docs/RPC_API.md §7).
// Tested in ui/tests/library.test.js.

/** One-tap emergency questions (SOS screen, medical category). */
export const EMERGENCY_QUESTIONS = [
  { label: 'Bleeding won’t stop', q: 'bleeding won’t stop', category: 'medical' },
  { label: 'Not breathing', q: 'person not breathing', category: 'medical' },
  { label: 'Burn', q: 'how to treat a burn', category: 'medical' },
  { label: 'Unsafe water', q: 'how to make unsafe water safe to drink' },
];

/** guides.info (or null when the core lacks it) → {available, info}. Smart features need available. */
export function semanticState(info) {
  const sem = info && info.semantic && typeof info.semantic === 'object' ? info.semantic : null;
  return { available: !!(sem && sem.available === true), info: info || null };
}

/** Confidence threshold of the core's calibrated scores (docs/RPC_API.md §7). */
export const MIN_SCORE = 0.30;

/**
 * Passages worth showing for an Ask, so a poor match is never presented as an answer.
 * `weak` is the core's verdict (guides.ask / guides.related return weak:true when even the best
 * passage is below its confidence threshold): when present it decides, and the rest are kept if
 * they score at least half the best. Without it (older cores), scores of 0..1 need ≥ MIN_SCORE;
 * any other scale keeps those ≥ half the best.
 */
export function strongPassages(passages, weak, minScore = MIN_SCORE) {
  if (typeof weak === 'number') { minScore = weak; weak = undefined; } // old call shape (passages, minScore)
  if (weak === true) return [];
  const ps = (passages || []).filter(p => p && typeof p.text === 'string' && p.text.trim() && p.id);
  if (!ps.length) return [];
  const top = Math.max(...ps.map(p => Number(p.score) || 0));
  if (weak === false) return ps.filter(p => (Number(p.score) || 0) >= top * 0.5);
  if (top <= 1 && top < minScore) return [];
  return ps.filter(p => (Number(p.score) || 0) >= Math.max(top * 0.5, top <= 1 ? minScore : 0));
}

/** Related guides worth listing: none when the core marks the set weak (or, without the flag, the best scores below the cut-off). */
export function strongRelated(r, minScore = MIN_SCORE) {
  if (!r || r.weak === true) return [];
  const rs = (r.results || []).filter(x => x && x.id);
  if (r.weak === false) return rs;
  const top = Math.max(0, ...rs.map(x => Number(x.score)).filter(Number.isFinite));
  return top && top <= 1 && top < minScore ? [] : rs;
}

/** What to show under a search result: the best passage (with its heading) or the keyword snippet. */
export function resultSnippet(r) {
  const p = r && r.passage && typeof r.passage.text === 'string' && r.passage.text.trim() ? r.passage : null;
  return {
    heading: p && typeof p.heading === 'string' ? p.heading : '',
    text: p ? p.text : String((r && r.snippet) || ''),
    byMeaning: !!r && r.match === 'meaning',
  };
}

/** Licence breakdown lines from guides.info, largest first. */
export function licenceLines(info) {
  return ((info && info.licences) || []).filter(l => l && l.name).slice().sort((a, b) => (b.count || 0) - (a.count || 0));
}
