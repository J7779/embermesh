// Bulletins: persistent notice board by category, post with expiry, delete own.
import { h, clear, debounce } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { toast, toastError, sheet, confirmDialog, chips, empty } from '../lib/ui.js';
import { timeAgo, distance, bearing, compass, formatDistance, validPos } from '../lib/format.js';
import { ownPosition } from './people.js';
import { unverified } from '../lib/ratelimit.js';

export const CATEGORIES = [
  ['water', 'Water'], ['food', 'Food'], ['medical', 'Medical'], ['shelter', 'Shelter'], ['hazard', 'Hazard'],
  ['trade', 'Trade'], ['missing', 'Missing'], ['transport', 'Transport'], ['info', 'Info'],
];
const CAT_LABEL = Object.fromEntries(CATEGORIES);
const EXPIRY = [[3600, '1 hour'], [6 * 3600, '6 hours'], [86400, '1 day'], [3 * 86400, '3 days'], [7 * 86400, '1 week'], [30 * 86400, '30 days']];

export default async function render(app, params, root) {
  let cat = params.query.get('cat') || '';
  const listEl = h('div');
  root.appendChild(h('div', { class: 'page' },
    h('div', { class: 'page-head' }, h('h1', null, 'Bulletins'), h('button', { class: 'btn primary', onclick: () => post(app, cat, load) }, icon('plus'), 'Post')),
    h('p', { class: 'muted small' }, 'Notices flood the whole mesh and stay until they expire — like a community notice board.'),
    chips([{ value: '', label: 'All' }, ...CATEGORIES.map(([v, l]) => ({ value: v, label: l }))], cat, v => { cat = v; load(); }, 'Category'),
    listEl));
  const me = await ownPosition(app);

  async function load() {
    const r = await app.api.try('bulletins.list', cat ? { category: cat } : {}, null, true);
    clear(listEl);
    if (!r) { listEl.appendChild(empty('Bulletins are not available yet.')); return; }
    const now = Date.now();
    const bs = (r.bulletins || []).filter(b => !b.expires_ts || b.expires_ts > now).sort((a, b) => b.ts - a.ts);
    if (!bs.length) { listEl.appendChild(empty(cat ? 'No ' + CAT_LABEL[cat] + ' bulletins.' : 'No bulletins yet.')); return; }
    for (const b of bs) {
      const geo = validPos(me) && validPos(b.position) ? formatDistance(distance(me, b.position), app.units()) + ' ' + compass(bearing(me, b.position)) : '';
      listEl.appendChild(h('article', { class: 'card stack bulletin cat-' + b.category },
        h('div', { class: 'row nowrap' },
          h('span', { class: 'tag ' + (b.category === 'hazard' || b.category === 'missing' ? 'bad' : b.category === 'medical' ? 'warn' : 'accent') }, CAT_LABEL[b.category] || b.category),
          h('h2', { class: 'grow' }, String(b.title || '')),
          unverified(b) ? h('span', { class: 'tag warn', title: 'The sender\'s identity could not be verified' }, 'unverified sender') : null,
          b.mine ? h('button', {
            class: 'btn small ghost icon-only', 'aria-label': 'Delete bulletin', onclick: async () => {
              if (!await confirmDialog('Delete bulletin?', 'A deletion notice is sent, but copies may survive on devices that are out of range.', { ok: 'Delete', danger: true })) return;
              try { await app.api.call('bulletins.delete', { id: b.id }); load(); } catch (e) { toastError(e); }
            },
          }, icon('trash', { size: 18 })) : null),
        h('p', { class: 'pre-wrap' }, String(b.text || '')),
        h('div', { class: 'small muted' }, (b.mine ? 'You' : (b.from_name || String(b.from || '').slice(0, 8))) + ' · ' + timeAgo(b.ts) +
          (b.expires_ts ? ' · expires ' + timeAgo(b.expires_ts) : '') + (geo ? ' · ' + geo : ''))));
    }
  }
  await load();
  const reload = debounce(load, 500);
  app.on('bulletin.new', reload);
  return () => reload.cancel();
}

function post(app, cat, done) {
  const category = h('select', null, CATEGORIES.map(([v, l]) => h('option', { value: v, selected: v === (cat || 'info') }, l)));
  const title = h('input', { type: 'text', maxlength: 80, placeholder: 'Short headline, e.g. "Clean water at Lincoln school"' });
  const text = h('textarea', { rows: 4, maxlength: 1000, placeholder: 'Details: where, when, who to contact' });
  const expires = h('select', null, EXPIRY.map(([v, l]) => h('option', { value: String(v), selected: v === 86400 }, l)));
  const withPos = h('input', { type: 'checkbox' });
  const err = h('div', { class: 'field-error', role: 'alert' });
  const s = sheet('Post bulletin', h('div', { class: 'stack' },
    h('label', { class: 'field-label' }, 'Category', category),
    h('label', { class: 'field-label' }, 'Title', title),
    h('label', { class: 'field-label' }, 'Details', text),
    h('label', { class: 'field-label' }, 'Keep for', expires),
    h('label', { class: 'check' }, withPos, h('span', null, 'Attach my position')),
    h('p', { class: 'muted small' }, 'Bulletins are public and cleartext. Do not post anything that puts people at risk.'),
    err,
    h('div', { class: 'row end' }, h('button', {
      class: 'btn primary', onclick: async () => {
        if (!title.value.trim()) { err.textContent = 'Add a title.'; title.focus(); return; }
        const p = { category: category.value, title: title.value.trim(), text: text.value.trim(), expires_s: Number(expires.value) };
        if (withPos.checked) { const me = await ownPosition(app); if (validPos(me)) p.position = me; }
        try { await app.api.call('bulletins.post', p); s.close(); toast('Bulletin posted', { kind: 'ok' }); done(); } catch (e) { toastError(e, 'Not posted'); }
      },
    }, 'Post'))));
}
