// Mock "meaning" ranking for guides.search / guides.related / guides.ask (docs/RPC_API.md §7), so
// the UI can be demonstrated before the core's local embedding model lands. Not a model: a small
// concept table (synonyms, lay wording) plus character trigrams for typos. Scores are 0..1.

const CONCEPTS = {
  bleed: 'bleeding bleed bleeds blood hemorrhage haemorrhage wound wounds cut gash tourniquet pressure',
  breath: 'breathing breathe breath breaths cpr airway resuscitation compressions unresponsive unconscious choking gasping',
  burn: 'burn burns burned burnt scald scalds scalded blister blisters',
  water: 'water drink drinking purify purifying purification boil boiling disinfect unsafe dirty clean contaminated',
  shelter: 'shelter cold warm tarp insulation freezing hypothermia rain wind',
  nav: 'lost compass map maps direction north navigation bearing',
  radio: 'radio handheld channel squelch frequency transmit talk walkie',
  mesh: 'embermesh mesh app message messages connect link',
  stop: 'stop stops stopping wont control',
};
const WORD2C = new Map();
for (const [c, ws] of Object.entries(CONCEPTS)) for (const w of ws.split(' ')) WORD2C.set(w, c);

const words = s => String(s || '').toLowerCase().replace(/[’']/g, '').split(/[^a-z0-9]+/).filter(w => w.length > 1);
const stem = w => w.replace(/(ings|ing|ed|es|s)$/, '');

/** Sparse vector: concepts weigh most, stems a little, trigrams for typos. */
export function vectorize(text) {
  const v = new Map();
  const add = (k, x) => v.set(k, (v.get(k) || 0) + x);
  for (const w of words(text)) {
    const c = WORD2C.get(w) || WORD2C.get(stem(w));
    if (c) add('c:' + c, 3);
    add('s:' + stem(w), 1);
    const p = '_' + w + '_';
    for (let i = 0; i + 3 <= p.length; i++) add('t:' + p.slice(i, i + 3), 0.15);
  }
  return v;
}

export function cosine(a, b) {
  let dot = 0, na = 0, nb = 0;
  for (const [k, x] of a) { na += x * x; const y = b.get(k); if (y) dot += x * y; }
  for (const y of b.values()) nb += y * y;
  return na && nb ? dot / Math.sqrt(na * nb) : 0;
}

const plain = md => String(md).replace(/```[\s\S]*?```/g, ' ').replace(/[#>*_`|]+/g, ' ').replace(/\[([^\]]*)\]\([^)]*\)/g, '$1').replace(/\s+/g, ' ').trim();

/** Splits a guide into heading-delimited passages (≤ 600 chars of plain text). */
export function passages(g) {
  const out = [];
  let heading = '', buf = [];
  const flush = () => { const t = plain(buf.join('\n')); if (t) out.push({ heading, text: t.length > 600 ? t.slice(0, 597) + '…' : t }); buf = []; };
  for (const line of String(g.markdown || '').split('\n')) {
    const m = /^#{1,6}\s+(.*)$/.exec(line);
    if (m) { flush(); heading = m[1].trim(); } else buf.push(line);
  }
  flush();
  return out.map(p => ({ ...p, vec: vectorize(g.title + ' ' + p.heading + ' ' + p.text) }));
}

const cache = new WeakMap();
const index = g => { if (!cache.has(g)) cache.set(g, { passages: passages(g), vec: vectorize(g.title + ' ' + g.summary + ' ' + (g.tags || []).join(' ') + ' ' + plain(g.markdown)) }); return cache.get(g); };

/** Best passage of a guide for a query: {heading, text, score}. */
export function bestPassage(g, query) {
  const q = vectorize(query);
  let best = null;
  for (const p of index(g).passages) { const s = cosine(q, p.vec); if (!best || s > best.score) best = { heading: p.heading, text: p.text, score: s }; }
  return best;
}

export function related(guides, id, limit = 8) {
  const g = guides.find(x => x.id === id);
  if (!g) return null;
  const v = index(g).vec;
  return guides.filter(x => x !== g).map(x => ({ g: x, score: Math.round(cosine(v, index(x).vec) * 1000) / 1000 })).filter(x => x.score > 0.12).sort((a, b) => b.score - a.score).slice(0, limit);
}

export function ask(guides, question, { category, limit = 5 } = {}) {
  const q = vectorize(question);
  const out = [];
  for (const g of guides) {
    if (category && g.category !== category) continue;
    for (const p of index(g).passages) { const s = cosine(q, p.vec); if (s >= 0.2) out.push({ id: g.id, title: g.title, category: g.category, heading: p.heading, text: p.text, score: Math.round(s * 1000) / 1000 }); }
  }
  return out.sort((a, b) => b.score - a.score).slice(0, limit);
}

export const MODEL_INFO = { model: 'mock-concepts', dims: 0, bits: 4 };
