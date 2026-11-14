// Notes & lists stored with kv.*: personal notes, checklists (with templates), supply inventory.
// kv storage is encrypted at rest by the core once a storage passphrase is set (Settings → Security).
import { h, clear, debounce } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { toast, toastError, sheet, confirmDialog, empty, promptDialog } from '../lib/ui.js';
import { timeAgo, shortDate } from '../lib/format.js';

export const TEMPLATES = {
  bugout: { title: 'Bug-out bag (72 h)', items: ['Water 3 L + filter or tablets', 'Food for 3 days (no-cook)', 'First-aid kit + personal medicines (7 days)', 'Copies of ID and documents (waterproof bag)', 'Cash in small notes', 'Flashlight / headlamp + spare batteries', 'Phone, charger, power bank', 'Handheld radio + spare battery, programmed with net plan', 'Rain jacket, warm layer, hat, gloves', 'Sturdy shoes and socks', 'Emergency blanket / tarp + cord', 'Knife or multitool', 'Lighter + matches in waterproof case', 'Whistle', 'Paper map of the area + compass', 'Hygiene kit, masks, sanitizer', 'Notebook and pencil', 'Written contact list and meeting points'] },
  comms: { title: 'Communications plan', items: ['Primary meeting point agreed', 'Secondary meeting point (outside the area)', 'Out-of-area contact person everyone knows', 'Primary radio frequency / channel written down', 'Backup frequency / channel', 'Check-in schedule (e.g. top of every hour, 5 min)', 'Everyone has verified fingerprints of the group', 'Group channel created and invite shared', 'Code words for "safe", "need help", "do not come"', 'Relay node location agreed (high ground)', 'Batteries / solar charging plan for radios', 'Paper copy of this plan in every bag'] },
  firstaid: { title: 'First-aid kit', items: ['Adhesive bandages (assorted)', 'Sterile gauze pads', 'Roller bandages', 'Elastic bandage', 'Triangular bandage', 'Medical tape', 'Tourniquet', 'Trauma shears', 'Tweezers', 'Disposable gloves', 'Antiseptic wipes / iodine', 'Pain relievers', 'Antihistamines', 'Oral rehydration salts', 'Burn gel', 'Thermometer', 'Emergency blanket', 'First-aid guide (printed)'] },
  shelter: { title: 'Shelter in place', items: ['Water: 4 L per person per day for 14 days', 'Food for 14 days', 'Manual can opener', 'Battery or crank radio', 'Lighting without flames', 'Toilet plan (bags, bucket, absorbent)', 'Medicines for 30 days', 'Heating / cooling plan', 'Plastic sheeting and tape', 'Fire extinguisher', 'Know how to shut off gas, water, power'] },
  vehicle: { title: 'Vehicle kit', items: ['Fuel above half tank', 'Jumper cables', 'Tow strap', 'Tire inflator / spare checked', 'Water and snacks', 'Blanket', 'First-aid kit', 'Flashlight', 'Paper road map', 'Phone charger (12 V)', 'Shovel', 'Reflective triangle / vest'] },
};

export default async function render(app, params, root) {
  const [mode, id] = params.rest;
  if (mode === 'notes') return notesList(app, root);
  if (mode === 'note' && id) return noteEditor(app, root, id);
  if (mode === 'checklists') return checklists(app, root);
  if (mode === 'checklist' && id) return checklist(app, root, id);
  if (mode === 'inventory') return inventory(app, root);
  return overview(app, root);
}

const back = (href = '#/extras') => h('a', { class: 'btn ghost icon-only', href, 'aria-label': 'Back' }, icon('back'));

async function encryptionNotice(app) {
  const s = await app.api.try('security.status', {}, null, true);
  if (!s) return null;
  return s.has_passphrase
    ? h('div', { class: 'notice info small' }, icon('lock', { size: 16 }), ' Stored encrypted on this device (storage passphrase is set).')
    : h('div', { class: 'notice small' }, 'Not protected by a passphrase yet. ', h('a', { href: '#/settings' }, 'Set a storage passphrase'), ' to encrypt notes and lists at rest.');
}

async function overview(app, root) {
  const [n, c, inv] = await Promise.all([
    app.api.try('kv.list', { ns: 'notes' }, { keys: [] }, true),
    app.api.try('kv.list', { ns: 'checklists' }, { keys: [] }, true),
    app.kvGet('inventory', 'items', []),
  ]);
  const items = [
    ['#/extras/notes', 'notes', 'Personal notes', (n.keys || []).length + ' notes'],
    ['#/extras/checklists', 'check', 'Checklists', (c.keys || []).length + ' lists · bug-out bag, comms plan, first aid…'],
    ['#/extras/inventory', 'box', 'Supply inventory', (Array.isArray(inv) ? inv.length : 0) + ' items · track quantities and expiry dates'],
  ];
  root.appendChild(h('div', { class: 'page' }, h('div', { class: 'page-head' }, h('h1', null, 'Notes & lists')), await encryptionNotice(app),
    h('ul', { class: 'list card' }, items.map(([href, ic, t, sub]) => h('li', null, h('a', { class: 'item', href }, icon(ic, { size: 28 }), h('div', { class: 'grow' }, h('div', { class: 'title' }, t), h('div', { class: 'sub' }, sub)), icon('next')))))));
}

async function loadNs(app, ns) {
  const r = await app.api.try('kv.list', { ns }, { keys: [] }, true);
  const vals = await Promise.all((r.keys || []).map(async k => ({ key: k, value: await app.kvGet(ns, k, null) })));
  return vals.filter(v => v.value && typeof v.value === 'object');
}

// ---------------------------------------------------------------- notes
async function notesList(app, root) {
  const notes = (await loadNs(app, 'notes')).sort((a, b) => (b.value.ts || 0) - (a.value.ts || 0));
  root.appendChild(h('div', { class: 'page' },
    h('div', { class: 'page-head' }, back(), h('h1', null, 'Notes'), h('a', { class: 'btn primary', href: '#/extras/note/n-' + Date.now().toString(36) }, icon('plus'), 'New')),
    await encryptionNotice(app),
    notes.length ? h('ul', { class: 'list card' }, notes.map(n => h('li', null, h('a', { class: 'item', href: '#/extras/note/' + encodeURIComponent(n.key) },
      h('div', { class: 'grow' }, h('div', { class: 'title' }, String(n.value.title || 'Untitled')), h('div', { class: 'sub' }, String(n.value.body || '').slice(0, 100))),
      h('div', { class: 'meta' }, timeAgo(n.value.ts))))))
      : empty('No notes yet.')));
}

async function noteEditor(app, root, key) {
  const v = (await app.kvGet('notes', key, null)) || { title: '', body: '', ts: Date.now() };
  const title = h('input', { type: 'text', value: String(v.title || ''), placeholder: 'Title', 'aria-label': 'Title' });
  const body = h('textarea', { value: String(v.body || ''), placeholder: 'Write anything: addresses, medical info, plans…', 'aria-label': 'Note text' });
  body.style.minHeight = '50vh';
  const status = h('span', { class: 'small muted', 'aria-live': 'polite' });
  let dirty = false;
  const save = debounce(async () => {
    if (!title.value.trim() && !body.value.trim()) return;
    try { await app.kvSet('notes', key, { title: title.value, body: body.value, ts: Date.now() }); dirty = false; status.textContent = 'Saved'; } catch (e) { status.textContent = 'Not saved: ' + (e.message || e.code); }
  }, 600);
  const onInput = () => { dirty = true; status.textContent = 'Editing…'; save(); };
  title.addEventListener('input', onInput);
  body.addEventListener('input', onInput);
  root.appendChild(h('div', { class: 'page stack' },
    h('div', { class: 'page-head' }, back('#/extras/notes'), h('h1', null, 'Note'), status,
      h('button', { class: 'btn danger icon-only', 'aria-label': 'Delete note', onclick: async () => { if (await confirmDialog('Delete note?', 'This cannot be undone.', { ok: 'Delete', danger: true })) { save.cancel(); await app.api.try('kv.delete', { ns: 'notes', key }, null, true); app.nav('#/extras/notes'); } } }, icon('trash'))),
    title, body));
  return () => { if (dirty) { save.cancel(); app.kvSet('notes', key, { title: title.value, body: body.value, ts: Date.now() }).catch(() => {}); } };
}

// ---------------------------------------------------------------- checklists
async function checklists(app, root) {
  const lists = (await loadNs(app, 'checklists')).sort((a, b) => String(a.value.title).localeCompare(String(b.value.title)));
  const create = async (tpl) => {
    const key = 'c-' + Date.now().toString(36);
    let value;
    if (tpl) value = { title: TEMPLATES[tpl].title, items: TEMPLATES[tpl].items.map(t => ({ text: t, done: false })), ts: Date.now() };
    else {
      const t = await promptDialog('New checklist', { label: 'Name' });
      if (!t) return;
      value = { title: t, items: [], ts: Date.now() };
    }
    try { await app.kvSet('checklists', key, value); app.nav('#/extras/checklist/' + key); } catch (e) { toastError(e); }
  };
  root.appendChild(h('div', { class: 'page' },
    h('div', { class: 'page-head' }, back(), h('h1', null, 'Checklists'), h('button', { class: 'btn primary', onclick: () => create(null) }, icon('plus'), 'New')),
    lists.length ? h('ul', { class: 'list card' }, lists.map(l => {
      const items = Array.isArray(l.value.items) ? l.value.items : [];
      const done = items.filter(i => i.done).length;
      return h('li', null, h('a', { class: 'item', href: '#/extras/checklist/' + encodeURIComponent(l.key) },
        h('div', { class: 'grow' }, h('div', { class: 'title' }, String(l.value.title || 'Checklist')), h('div', { class: 'bar' }, h('i', { style: { width: (items.length ? done / items.length * 100 : 0) + '%' } }))),
        h('div', { class: 'meta' }, done + '/' + items.length)));
    })) : empty('No checklists yet. Start from a template below.'),
    h('h2', { class: 'section-title' }, 'Templates'),
    h('div', { class: 'cat-grid' }, Object.entries(TEMPLATES).map(([k, t]) => h('button', { class: 'btn', onclick: () => create(k) }, icon('plus', { size: 18 }), h('span', { class: 'grow' }, t.title))))));
}

async function checklist(app, root, key) {
  const v = await app.kvGet('checklists', key, null);
  if (!v) { root.appendChild(h('div', { class: 'page' }, h('div', { class: 'page-head' }, back('#/extras/checklists'), h('h1', null, 'Checklist')), empty('Not found.'))); return; }
  const items = Array.isArray(v.items) ? v.items.filter(i => i && typeof i.text === 'string') : [];
  const listEl = h('ul', { class: 'list card' });
  const progress = h('span', { class: 'muted' });
  const save = () => app.kvSet('checklists', key, { ...v, items, ts: Date.now() }).catch(e => toastError(e, 'Not saved'));
  const draw = () => {
    clear(listEl);
    progress.textContent = items.filter(i => i.done).length + ' of ' + items.length + ' done';
    items.forEach((it, i) => {
      const cb = h('input', { type: 'checkbox', checked: !!it.done, onchange: () => { it.done = cb.checked; save(); draw(); } });
      listEl.appendChild(h('li', null, h('div', { class: 'item' },
        h('label', { class: 'check grow' }, cb, h('span', { class: it.done ? 'muted' : '' }, it.text)),
        h('button', { class: 'btn small ghost icon-only', 'aria-label': 'Remove item', onclick: () => { items.splice(i, 1); save(); draw(); } }, icon('close', { size: 18 })))));
    });
  };
  const add = h('input', { type: 'text', placeholder: 'Add item', 'aria-label': 'New item', enterkeyhint: 'done' });
  const addItem = () => { const t = add.value.trim(); if (!t) return; items.push({ text: t, done: false }); add.value = ''; save(); draw(); };
  add.addEventListener('keydown', e => { if (e.key === 'Enter') addItem(); });
  root.appendChild(h('div', { class: 'page stack' },
    h('div', { class: 'page-head' }, back('#/extras/checklists'), h('h1', null, String(v.title || 'Checklist')),
      h('button', { class: 'btn danger icon-only', 'aria-label': 'Delete checklist', onclick: async () => { if (await confirmDialog('Delete checklist?', String(v.title), { ok: 'Delete', danger: true })) { await app.api.try('kv.delete', { ns: 'checklists', key }, null, true); app.nav('#/extras/checklists'); } } }, icon('trash'))),
    h('div', { class: 'row' }, progress, h('button', { class: 'btn small', onclick: () => { for (const i of items) i.done = false; save(); draw(); } }, 'Uncheck all')),
    listEl,
    h('div', { class: 'row nowrap' }, h('div', { class: 'grow' }, add), h('button', { class: 'btn', onclick: addItem }, icon('plus'), 'Add'))));
  draw();
}

// ---------------------------------------------------------------- inventory
async function inventory(app, root) {
  let items = await app.kvGet('inventory', 'items', []);
  if (!Array.isArray(items)) items = [];
  items = items.filter(i => i && typeof i.name === 'string');
  const filter = h('input', { type: 'search', placeholder: 'Filter', 'aria-label': 'Filter inventory' });
  const tableBox = h('div');
  const save = () => app.kvSet('inventory', 'items', items).catch(e => toastError(e, 'Not saved'));
  const now = Date.now();
  const draw = () => {
    const q = filter.value.trim().toLowerCase();
    const list = items.filter(i => !q || (i.name + ' ' + (i.location || '') + ' ' + (i.category || '')).toLowerCase().includes(q))
      .sort((a, b) => String(a.category || '').localeCompare(String(b.category || '')) || a.name.localeCompare(b.name));
    clear(tableBox);
    if (!list.length) { tableBox.appendChild(empty(items.length ? 'No matches.' : 'Nothing tracked yet. Add water, food, medicine, batteries, fuel…')); return; }
    tableBox.appendChild(h('div', { class: 'table-wrap card' }, h('table', { class: 'data' },
      h('thead', null, h('tr', null, h('th', null, 'Item'), h('th', null, 'Qty'), h('th', null, 'Expires'), h('th', null, h('span', { class: 'sr-only' }, 'Actions')))),
      h('tbody', null, list.map(it => {
        const exp = it.expires ? Date.parse(it.expires) : NaN;
        const soon = Number.isFinite(exp) && exp - now < 30 * 86400e3;
        const qty = h('span', { class: 'mono' }, String(it.qty ?? 0) + (it.unit ? ' ' + it.unit : ''));
        const step = d => { it.qty = Math.max(0, (Number(it.qty) || 0) + d); qty.textContent = it.qty + (it.unit ? ' ' + it.unit : ''); save(); };
        return h('tr', null,
          h('td', null, h('div', null, it.name), h('div', { class: 'small muted' }, [it.category, it.location].filter(Boolean).join(' · '))),
          h('td', null, h('div', { class: 'row nowrap' }, h('button', { class: 'btn small icon-only', 'aria-label': 'Decrease ' + it.name, onclick: () => step(-1) }, '−'), qty, h('button', { class: 'btn small icon-only', 'aria-label': 'Increase ' + it.name, onclick: () => step(1) }, '+'))),
          h('td', { class: soon ? (exp < now ? 'bad-text' : 'warn-text') : '' }, Number.isFinite(exp) ? shortDate(exp) + (exp < now ? ' (expired)' : '') : '—'),
          h('td', null, h('button', { class: 'btn small ghost icon-only', 'aria-label': 'Edit ' + it.name, onclick: () => edit(it) }, icon('edit', { size: 18 }))));
      })))));
  };
  function edit(it) {
    const isNew = !it;
    const cur = it || { id: 'i-' + Date.now().toString(36), name: '', qty: 1, unit: '', category: '', location: '', expires: '' };
    const f = {
      name: h('input', { type: 'text', value: cur.name, placeholder: 'e.g. Bottled water' }),
      qty: h('input', { type: 'text', inputmode: 'decimal', value: String(cur.qty ?? '') }),
      unit: h('input', { type: 'text', value: cur.unit || '', placeholder: 'L, cans, packs…' }),
      category: h('input', { type: 'text', value: cur.category || '', placeholder: 'Water, food, medical, power…' }),
      location: h('input', { type: 'text', value: cur.location || '', placeholder: 'Where it is stored' }),
      expires: h('input', { type: 'text', value: cur.expires || '', placeholder: 'YYYY-MM-DD', inputmode: 'numeric' }),
    };
    const s = sheet(isNew ? 'Add item' : 'Edit item', h('div', { class: 'stack' },
      ...Object.entries({ name: 'Item', qty: 'Quantity', unit: 'Unit', category: 'Category', location: 'Location', expires: 'Expiry date' }).map(([k, l]) => h('label', { class: 'field-label' }, l, f[k])),
      h('div', { class: 'row end' },
        !isNew ? h('button', { class: 'btn danger', onclick: () => { items = items.filter(x => x !== it); save(); s.close(); draw(); } }, 'Delete') : null,
        h('button', {
          class: 'btn primary', onclick: () => {
            if (!f.name.value.trim()) return f.name.focus();
            if (f.expires.value && !/^\d{4}-\d{2}-\d{2}$/.test(f.expires.value.trim())) { toast('Use YYYY-MM-DD for the date', { kind: 'bad' }); return f.expires.focus(); }
            Object.assign(cur, { name: f.name.value.trim(), qty: Number(f.qty.value.replace(',', '.')) || 0, unit: f.unit.value.trim(), category: f.category.value.trim(), location: f.location.value.trim(), expires: f.expires.value.trim() });
            if (isNew) items.push(cur);
            save(); s.close(); draw();
          },
        }, 'Save'))));
  }
  filter.addEventListener('input', draw);
  root.appendChild(h('div', { class: 'page stack' },
    h('div', { class: 'page-head' }, back(), h('h1', null, 'Supply inventory'), h('button', { class: 'btn primary', onclick: () => edit(null) }, icon('plus'), 'Add')),
    filter, tableBox));
  draw();
}
