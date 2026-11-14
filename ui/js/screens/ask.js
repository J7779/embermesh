// "Ask the library": guides.ask shows the passages of the guides that best answer a question.
// Always quoted text from the guides, never generated answers. Also the cached guides.info lookup.
import { h, clear, append } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { errorText } from '../lib/ui.js';
import { semanticState, strongPassages } from '../lib/library.js';

let infoPromise = null;
let hooked = false;
/** {available, info} from guides.info, cached until the library reloads (node.ready). */
export function semanticInfo(app) {
  if (!infoPromise) {
    infoPromise = app.api.try('guides.info', {}, null, true).then(semanticState);
    if (!hooked) { hooked = true; app.backend.on('node.ready', () => { infoPromise = null; }); }
  }
  return infoPromise;
}

const href = (id, q) => '#/guides/read/' + String(id).split('/').map(encodeURIComponent).join('/') + (q ? '?q=' + encodeURIComponent(q) : '');

/**
 * Ask box. opts: {category?, title?, placeholder?, shortcuts?:[{label, q, category?}], compact?}
 * Returns null when the core cannot answer (no semantic section / old core): no dead controls.
 */
export async function askBox(app, opts = {}) {
  const st = await semanticInfo(app);
  if (!st.available) return null;
  const input = h('input', { type: 'search', placeholder: opts.placeholder || 'e.g. how do I stop bleeding?', 'aria-label': 'Ask the library', enterkeyhint: 'search', maxlength: 300 });
  const out = h('div', { class: 'ask-out', 'aria-live': 'polite' });
  let seq = 0;
  const run = async (question, category) => {
    const q = String(question || '').trim();
    if (q.length < 3) { input.focus(); return; }
    const my = ++seq;
    clear(out).appendChild(h('p', { class: 'muted small' }, 'Searching the guides…'));
    let r;
    try { r = await app.api.call('guides.ask', { question: q, limit: 5, ...(category ? { category } : {}) }); } catch (e) { if (my === seq) clear(out).appendChild(h('p', { class: 'bad-text' }, errorText(e))); return; }
    if (my !== seq) return;
    const ps = strongPassages(r && r.passages, r ? r.weak : undefined);
    clear(out);
    if (!ps.length) {
      out.appendChild(h('p', { class: 'notice small' }, 'No good passage found for “' + q + '”. Try asking it another way, or search by keywords. (Asking works in English.)'));
      return;
    }
    append(out, h('p', { class: 'muted small' }, 'Passages quoted from the guides (not generated answers):'),
      ps.map(p => h('figure', { class: 'ask-passage card' },
        h('blockquote', null, String(p.text)),
        h('figcaption', { class: 'small' }, h('strong', null, String(p.title || p.id)), p.heading && p.heading !== p.title ? ' — ' + p.heading : '', ' · ',
          h('a', { href: href(p.id, q) }, 'read the whole guide')))));
  };
  input.addEventListener('keydown', e => { if (e.key === 'Enter') { e.preventDefault(); run(input.value, opts.category); } });
  return h('section', { class: 'card stack ask-box', 'aria-label': opts.title || 'Ask the library' },
    h('h2', null, icon('search', { size: 20 }), ' ', opts.title || 'Ask the library'),
    h('div', { class: 'row nowrap' }, h('div', { class: 'grow' }, input), h('button', { class: 'btn primary', onclick: () => run(input.value, opts.category) }, 'Ask')),
    (opts.shortcuts || []).length ? h('div', { class: 'row ask-shortcuts' }, opts.shortcuts.map(s => h('button', { class: 'btn small', onclick: () => { input.value = s.q; run(s.q, s.category); } }, s.label))) : null,
    h('p', { class: 'muted small' }, 'Finds the passages of the guides that answer your question — works best with plain questions in English. Quotes the guides, never makes up answers. Runs on this device; nothing is sent anywhere.'),
    out);
}
