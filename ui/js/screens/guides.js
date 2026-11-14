// Guides: categories, lists, multi-chapter books, search-as-you-type with highlighted snippets,
// reader (safe Markdown, adjustable text size, prev/next), bookmarks + recently read (kv.*).
import { h, clear, debounce, highlight, append } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { toast, sheet, copyText, empty, errorText, setFill } from '../lib/ui.js';
import { parseMarkdown, toDomChunks, outline, slugify } from '../lib/markdown.js';
import { formatBytes, timeAgo, clamp } from '../lib/format.js';
import { resultSnippet, licenceLines, EMERGENCY_QUESTIONS, strongRelated } from '../lib/library.js';
import { semanticInfo, askBox } from './ask.js';

const NS = 'ui';
const enc = s => s.split('/').map(encodeURIComponent).join('/');

export default async function render(app, params, root) {
  const [mode, ...rest] = params.rest;
  if (mode === 'read' && rest.length) return reader(app, rest.join('/'), root);
  if (mode === 'cat' && rest.length) return listing(app, root, { category: rest[0] });
  if (mode === 'book' && rest.length) return listing(app, root, { part_of: rest.join('/') });
  return home(app, root, params.query.get('q') || '');
}

async function getList(app, key) {
  const v = await app.kvGet(NS, key, []);
  return Array.isArray(v) ? v : [];
}

// ---------------------------------------------------------------- home + search
async function home(app, root, q0) {
  const input = h('input', { type: 'search', placeholder: 'Search all guides (e.g. purify water, splint, antenna)', 'aria-label': 'Search guides', value: q0, enterkeyhint: 'search' });
  const results = h('div', { 'aria-live': 'polite' });
  const browse = h('div');
  const catSel = h('select', { 'aria-label': 'Search in category', class: 'auto-width', 'data-secondary': '' }, h('option', { value: '' }, 'All categories'));
  const smartBox = h('div');
  const askSlot = h('div');
  root.appendChild(h('div', { class: 'page' }, h('div', { class: 'page-head' }, h('h1', null, 'Survival library')), h('div', { class: 'row nowrap' }, h('div', { class: 'grow' }, input), catSel), smartBox, results, askSlot, browse));

  let cats = await app.api.try('guides.categories', {}, null, true);
  if (!cats) { browse.appendChild(empty('The guide library is not available in this build yet.')); input.disabled = true; return; }
  if (!cats.total) {
    // The pack may still be downloading / loading (browser): show progress and keep the app usable.
    const bar = h('div', { class: 'bar' }, setFill(h('i'), 0));
    const label = h('p', { class: 'muted' }, 'The library is still loading. You can use the rest of the app meanwhile.');
    browse.appendChild(h('div', { class: 'card stack lib-progress' }, h('strong', null, 'Loading the survival library…'), bar, label));
    input.disabled = true;
    const offProg = app.on('host.guides.progress', d => {
      if (d && d.total > 0) { setFill(bar.firstChild, d.loaded / d.total * 100); label.textContent = formatBytes(d.loaded) + ' of ' + formatBytes(d.total) + ' — you can use the rest of the app meanwhile.'; }
    });
    for (let i = 0; i < 240 && (!cats || !cats.total); i++) {
      await new Promise(r => setTimeout(r, 1000));
      if (!root.isConnected) return;
      cats = await app.api.try('guides.categories', {}, null, true);
    }
    offProg();
    if (!cats || !cats.total) { clear(browse).appendChild(empty('No guide library is included in this build.')); return; }
    clear(browse);
    input.disabled = false;
  }
  const [bookmarks, recent, sem] = await Promise.all([getList(app, 'guide_bookmarks'), getList(app, 'guide_recent'), semanticInfo(app)]);
  // Smart (meaning-based) search: only offered when the pack has a semantic section.
  let smart = false;
  if (sem.available) {
    smart = (await app.kvGet(NS, 'smart_search', true)) !== false;
    const sw = h('input', { type: 'checkbox', checked: smart, 'aria-describedby': 'smart-help' });
    sw.addEventListener('change', () => { smart = sw.checked; app.api.try('kv.set', { ns: NS, key: 'smart_search', value: smart }, null, true); if (input.value.trim().length >= 2) runSearch(); });
    smartBox.appendChild(h('label', { class: 'check smart-switch', 'data-secondary': '' }, sw, h('span', null, h('strong', null, 'Smart search'), ' ', h('span', { class: 'muted small', id: 'smart-help' }, 'Also finds guides by meaning: helps most with questions and everyday wording (English). Exact words still come first. Runs on this device; nothing is sent anywhere.'))));
    const box = await askBox(app, {});
    if (box) askSlot.appendChild(box);
  }
  const catTitle = Object.fromEntries((cats.categories || []).map(c => [c.id, c.title]));
  for (const c of cats.categories || []) catSel.appendChild(h('option', { value: c.id }, String(c.title || c.id)));
  catSel.addEventListener('change', () => { if (input.value.trim().length >= 2) runSearch(); });

  const hasManual = (cats.categories || []).some(c => c.id === 'embermesh');
  append(browse,
    hasManual ? h('a', { class: 'card manual-entry row nowrap', href: '#/help' },
      icon('book', { size: 32 }), h('div', { class: 'grow' }, h('div', { class: 'title' }, 'How to use EmberMesh'),
        h('div', { class: 'sub muted small' }, 'The operator manual: quick start, phone to phone, radios, hubs, troubleshooting')), icon('next')) : null,
    bookmarks.length ? section('Bookmarks', bookmarks) : null,
    recent.length ? section('Recently read', recent.slice(0, 6)) : null,
    h('h2', { class: 'section-title' }, 'Library · ' + (cats.total || 0).toLocaleString() + ' guides in ' + (cats.categories || []).length + ' categories'),
    overview(sem.info),
    h('div', { class: 'cat-grid' }, (cats.categories || []).map(c => h('a', { class: 'btn', href: '#/guides/cat/' + encodeURIComponent(c.id) }, h('span', null, String(c.title || c.id)), h('span', { class: 'badge' }, String(c.count || 0))))));

  function overview(info) {
    if (!info) return null;
    const lic = licenceLines(info);
    return h('details', { class: 'card lib-overview', 'data-secondary': '' }, h('summary', null, (info.guides || 0).toLocaleString() + ' guides · ' + formatBytes(info.bytes || 0) + ' · smart search ' + (info.semantic && info.semantic.available ? 'available' : 'not in this library')),
      lic.length ? h('table', { class: 'data' }, h('thead', null, h('tr', null, h('th', null, 'Licence'), h('th', { class: 'num' }, 'Guides'))),
        h('tbody', null, lic.map(l => h('tr', null, h('td', null, String(l.name)), h('td', { class: 'num' }, String(l.count || 0)))))) : null,
      info.semantic && info.semantic.available ? h('p', { class: 'small muted' }, 'Meaning search model: ' + [info.semantic.model, info.semantic.dims ? info.semantic.dims + ' dimensions' : '', info.semantic.bits ? info.semantic.bits + '-bit' : '', info.semantic.passages ? info.semantic.passages.toLocaleString() + ' passages' : ''].filter(Boolean).join(', ') + '. It runs on this device.') : null);
  }

  function section(title, items) {
    return h('div', null, h('h2', { class: 'section-title' }, title), h('ul', { class: 'list card' }, items.map(b => h('li', null,
      h('a', { class: 'item', href: '#/guides/read/' + enc(String(b.id)) }, icon('book'), h('div', { class: 'grow' }, h('div', { class: 'title' }, String(b.title || b.id)), h('div', { class: 'sub' }, (catTitle[b.category] || b.category || '') + (b.ts ? ' · ' + timeAgo(b.ts) : ''))))))));
  }

  // Search as you type: debounced, at most one query in flight; a query typed while one runs
  // replaces any queued one, and results of a query that is no longer current are dropped.
  let seq = 0;
  let inFlight = false, queued = false;
  const runSearch = async () => {
    if (inFlight) { queued = true; return; }
    const q = input.value.trim();
    history.replaceState(null, '', '#/guides' + (q ? '?q=' + encodeURIComponent(q) : ''));
    const my = ++seq;
    if (q.length < 2) { clear(results); browse.hidden = false; return; }
    let r;
    inFlight = true;
    try { r = await app.api.call('guides.search', { query: q, limit: 30, ...(catSel.value ? { category: catSel.value } : {}), ...(sem.available ? { semantic: smart } : {}) }); } catch (e) { if (my === seq && !queued) clear(results).appendChild(h('p', { class: 'bad-text' }, errorText(e))); return; } finally {
      inFlight = false;
      if (queued) { queued = false; runSearch(); }
    }
    if (my !== seq || input.value.trim() !== q) return;
    browse.hidden = true;
    clear(results);
    const rs = r.results || [];
    results.appendChild(h('h2', { class: 'section-title' }, rs.length + ' result' + (rs.length === 1 ? '' : 's') + ' for “' + q + '”'));
    if (!rs.length) { results.appendChild(h('p', { class: 'muted' }, 'Nothing found. Try fewer or simpler words.')); return; }
    const terms = (r.results[0] && Array.isArray(rs[0].terms) && rs[0].terms.length) ? rs[0].terms : q.split(/\s+/);
    results.appendChild(h('ul', { class: 'list' }, rs.map(x => {
      const sn = resultSnippet(x);
      return h('li', null, h('a', { class: 'item', href: '#/guides/read/' + enc(String(x.id)) + '?q=' + encodeURIComponent(q) },
        h('div', { class: 'grow' },
          h('div', { class: 'title' }, highlight(String(x.title || x.id), x.terms || terms), sn.byMeaning ? h('span', { class: 'tag by-meaning', title: 'Found by meaning, not by the words you typed' }, '≈ by meaning') : null),
          sn.heading ? h('div', { class: 'sub passage-heading' }, sn.heading) : null,
          h('div', { class: 'snippet' }, highlight(sn.text, x.terms || terms)),
          h('div', { class: 'sub' }, catTitle[x.category] || x.category || ''))));
    })));
  };
  const search = debounce(runSearch, 180);
  input.addEventListener('input', search);
  input.addEventListener('keydown', e => { if (e.key === 'Escape') { input.value = ''; search(); } });
  if (q0) search();
  setTimeout(() => { if (!matchMedia('(pointer: coarse)').matches) input.focus(); }, 30);
  return () => search.cancel();
}

// ---------------------------------------------------------------- listing
/** Loads all metadata of a category / book in pages of 500. */
async function loadAll(app, filter) {
  const out = [];
  let total = 0;
  for (let offset = 0; offset < 100000; offset += 500) {
    const r = await app.api.call('guides.list', { ...filter, offset, limit: 500 });
    out.push(...(r.guides || []));
    total = r.total || out.length;
    if (!(r.guides || []).length || out.length >= total) break;
  }
  return out;
}

/**
 * Category items: books (part_of) collapsed into one entry with their chapters, loose guides
 * alone. Sorted by `sort`: "title" (A–Z, books and guides mixed) | "books" (books first) |
 * "size". Pure; exported for tests.
 */
export function groupCategory(guides, sort = 'title') {
  const books = new Map();
  const items = [];
  for (const g of guides || []) {
    if (g.part_of) {
      if (!books.has(g.part_of)) { const it = { book: g.part_of, chapters: [] }; books.set(g.part_of, it); items.push(it); }
      books.get(g.part_of).chapters.push(g);
    } else items.push({ g });
  }
  for (const it of books.values()) {
    it.chapters.sort((a, b) => (a.order ?? 0) - (b.order ?? 0) || String(a.title).localeCompare(String(b.title)));
    it.title = bookTitle(it.chapters);
    it.bytes = it.chapters.reduce((n, c) => n + (c.bytes || 0), 0);
  }
  const key = it => String(it.book ? it.title : (it.g.title || it.g.id)).replace(/^(the|a|an)\s+/i, '').toLowerCase();
  items.sort((a, b) => sort === 'books' ? ((!!b.book - !!a.book) || key(a).localeCompare(key(b)))
    : sort === 'size' ? ((b.book ? b.bytes : b.g.bytes || 0) - (a.book ? a.bytes : a.g.bytes || 0))
      : key(a).localeCompare(key(b)));
  return items;
}

/** Count of guides per source publication. Pure; exported for tests. */
export function sourcesOf(guides) {
  const m = new Map();
  for (const g of guides || []) {
    const k = String(g.source_title || g.source || 'Unknown source').trim() || 'Unknown source';
    const e = m.get(k) || { source_title: k, count: 0, license: g.license || '', bytes: 0 };
    e.count++; e.bytes += g.bytes || 0;
    m.set(k, e);
  }
  return [...m.values()].sort((a, b) => b.count - a.count || a.source_title.localeCompare(b.source_title));
}

const PAGE = 60;

async function listing(app, root, filter) {
  const page = h('div', { class: 'page' });
  root.appendChild(page);
  const t0 = performance.now();
  const cats = await app.api.try('guides.categories', {}, { categories: [] }, true);
  const catTitle = Object.fromEntries((cats.categories || []).map(c => [c.id, c.title]));
  let guides;
  try { guides = await loadAll(app, filter); } catch (e) { page.appendChild(empty(errorText(e))); return; }
  const isBook = !!filter.part_of;
  const title = isBook ? (guides.length ? bookTitle(guides) : filter.part_of) : (catTitle[filter.category] || filter.category);
  page.appendChild(h('div', { class: 'page-head' },
    h('a', { class: 'btn ghost icon-only', href: isBook && guides[0] ? '#/guides/cat/' + encodeURIComponent(guides[0].category) : '#/guides', 'aria-label': 'Back' }, icon('back')),
    h('h1', null, String(title))));
  if (!guides.length) { page.appendChild(empty('No guides here.')); return; }

  const listEl = h('ul', { class: 'list card' });
  const more = h('div', { class: 'center', 'aria-hidden': 'true' });
  let items = [];
  let shown = 0;
  let io = null;
  // Render in pages of 60; a sentinel below the list loads the next page when it scrolls into view.
  const renderMore = () => {
    const next = items.slice(shown, shown + PAGE);
    for (const it of next) listEl.appendChild(itemEl(it));
    shown += next.length;
    clear(more);
    if (shown < items.length) more.appendChild(h('button', { class: 'btn', onclick: renderMore }, 'Show more (' + (items.length - shown) + ' left)'));
  };
  const reset = list => { items = list; shown = 0; clear(listEl); renderMore(); };
  const itemEl = it => {
    if (it.book) {
      return h('li', null, h('a', { class: 'item', href: '#/guides/book/' + enc(it.book) }, icon('book'),
        h('div', { class: 'grow' }, h('div', { class: 'title' }, it.title), h('div', { class: 'sub' }, it.chapters.length + ' chapters · ' + formatBytes(it.bytes) + ' · ' + String(it.chapters[0].license || ''))), icon('next')));
    }
    const g = it.g;
    return h('li', null, h('a', { class: 'item', href: '#/guides/read/' + enc(String(g.id)) },
      h('div', { class: 'grow' }, h('div', { class: 'title' }, (it.chapter ? it.chapter + '. ' : '') + String(g.title || g.id)),
        g.summary ? h('div', { class: 'sub clamp2' }, String(g.summary)) : null, h('div', { class: 'sub' }, String(g.license || '') + ' · ' + formatBytes(g.bytes))), icon('next')));
  };

  if (isBook) {
    guides.sort((a, b) => (a.order ?? 0) - (b.order ?? 0));
    const st = sourcesOf(guides)[0];
    const bq = h('input', { type: 'search', placeholder: 'Search in this book', 'aria-label': 'Search in this book' });
    append(page, h('p', { class: 'muted small' }, guides.length + ' chapters · ' + formatBytes(guides.reduce((n, g) => n + (g.bytes || 0), 0)) + (st ? ' · ' + st.license : '')), guides.length > 3 ? bq : null, listEl, more);
    const chapters = guides.map((g, i) => ({ g, chapter: i + 1 }));
    reset(chapters);
    const inBook = new Map(chapters.map(c => [c.g.id, c]));
    let bseq = 0;
    bq.addEventListener('input', debounce(async () => {
      const text = bq.value.trim();
      const my = ++bseq;
      if (text.length < 2) { reset(chapters); return; }
      const r = await app.api.try('guides.search', { query: text, category: guides[0].category, limit: 200 }, { results: [] }, true);
      if (my !== bseq) return;
      const hits = (r.results || []).filter(x => inBook.has(x.id)).map(x => inBook.get(x.id));
      reset(hits);
      if (!hits.length) listEl.appendChild(h('li', null, empty('Nothing found in this book.')));
    }, 180));
  } else {
    const sortSel = h('select', { 'aria-label': 'Sort', class: 'auto-width' }, h('option', { value: 'title' }, 'A–Z'), h('option', { value: 'books' }, 'Books first'), h('option', { value: 'size' }, 'Longest first'));
    const q = h('input', { type: 'search', placeholder: 'Search in ' + title, 'aria-label': 'Search in this category' });
    const tabs = h('div', { class: 'chips', role: 'group', 'aria-label': 'View' });
    const grouped = () => groupCategory(guides, sortSel.value);
    const nBooks = new Set(guides.filter(g => g.part_of).map(g => g.part_of)).size;
    if (filter.category === 'medical') {
      const box = await askBox(app, { category: 'medical', title: 'Ask the medical guides', shortcuts: EMERGENCY_QUESTIONS.filter(x => x.category === 'medical') });
      if (box) page.appendChild(box);
    }
    append(page, h('p', { class: 'muted small' }, guides.length + ' guides' + (nBooks ? ', ' + nBooks + ' books' : '') + ' · ' + formatBytes(guides.reduce((n, g) => n + (g.bytes || 0), 0))),
      q, h('div', { class: 'row' }, tabs, h('div', { class: 'grow' }), sortSel), listEl, more);
    let view = 'all';
    const showSources = () => {
      clear(listEl); clear(more);
      for (const sct of sourcesOf(guides)) listEl.appendChild(h('li', null, h('div', { class: 'item' }, h('div', { class: 'grow' }, h('div', { class: 'title' }, sct.source_title), h('div', { class: 'sub' }, sct.license + ' · ' + formatBytes(sct.bytes))), h('span', { class: 'badge' }, String(sct.count)))));
    };
    const tab = (v, label) => h('button', { class: 'btn small', 'aria-pressed': String(v === view), onclick: e => { view = v; for (const b of tabs.children) b.setAttribute('aria-pressed', String(b === e.currentTarget)); v === 'sources' ? showSources() : reset(grouped()); } }, label);
    append(tabs, tab('all', 'Guides & books'), tab('sources', 'Sources'));
    sortSel.addEventListener('change', () => { if (view === 'all') reset(grouped()); });
    let seq = 0;
    q.addEventListener('input', debounce(async () => {
      const text = q.value.trim();
      const my = ++seq;
      if (text.length < 2) { view === 'all' ? reset(grouped()) : showSources(); return; }
      const r = await app.api.try('guides.search', { query: text, category: filter.category, limit: 100 }, { results: [] }, true);
      if (my !== seq) return;
      const byId = new Map(guides.map(g => [g.id, g]));
      clear(listEl); clear(more);
      const rs = r.results || [];
      if (!rs.length) listEl.appendChild(h('li', null, empty('Nothing found in this category.')));
      for (const x of rs) listEl.appendChild(h('li', null, h('a', { class: 'item', href: '#/guides/read/' + enc(String(x.id)) + '?q=' + encodeURIComponent(text) },
        h('div', { class: 'grow' }, h('div', { class: 'title' }, highlight(String(x.title || x.id), x.terms || text.split(/\s+/))),
          h('div', { class: 'snippet' }, highlight(String(x.snippet || ''), x.terms || text.split(/\s+/))),
          byId.get(x.id) && byId.get(x.id).part_of ? h('div', { class: 'sub' }, 'in ' + bookTitle(guides.filter(g => g.part_of === byId.get(x.id).part_of))) : null))));
    }, 180));
    reset(grouped());
  }
  if (typeof IntersectionObserver === 'function') {
    io = new IntersectionObserver(es => { if (es.some(e => e.isIntersecting) && shown < items.length) renderMore(); }, { rootMargin: '600px' });
    io.observe(more);
  }
  page.dataset.loadMs = String(Math.round(performance.now() - t0));
  return () => { if (io) io.disconnect(); };
}

/** Book title: the shared source publication, else the chapters' common title prefix, else the id. Exported for tests. */
export function bookTitle(chapters) {
  const cs = (chapters || []).filter(Boolean);
  if (!cs.length) return '';
  const st = String(cs[0].source_title || '').trim();
  if (st && cs.every(c => String(c.source_title || '').trim() === st)) return st;
  let pre = String(cs[0].title || '');
  for (const c of cs.slice(1)) { const t = String(c.title || ''); let i = 0; while (i < pre.length && i < t.length && pre[i] === t[i]) i++; pre = pre.slice(0, i); }
  if (cs.length > 1) pre = pre.replace(/\s*\S*$/, ''); // cut at a word boundary
  pre = pre.replace(/[\s:—–(\-]*(\d+|part|ch\.?)?[\s:—–(\-]*$/i, '').trim();
  if (pre.length >= 4) return pre;
  const slug = String(cs[0].part_of || cs[0].id || '').split('/').pop().replace(/[-_]+/g, ' ');
  return slug.charAt(0).toUpperCase() + slug.slice(1);
}

// ---------------------------------------------------------------- reader
/** Guide reader. opts.prefix = route prefix for prev/next and internal links ('#/guides/read/' or '#/help/read/'), opts.back. */
export async function reader(app, id, root, opts = {}) {
  const prefix = opts.prefix || '#/guides/read/';
  const page = h('div', { class: 'page' });
  root.appendChild(page);
  let g;
  try { g = await app.api.call('guides.get', { id }); } catch (e) {
    append(page, h('div', { class: 'page-head' }, h('a', { class: 'btn ghost icon-only', href: opts.back || '#/guides', 'aria-label': 'Back' }, icon('back')), h('h1', null, 'Guide')),
      empty(e.code === 'not_found' ? (String(id).startsWith('embermesh/') ? 'This chapter is not in this version of the manual yet.' : 'This guide is not in the library.') : errorText(e),
        String(id).startsWith('embermesh/') ? h('a', { class: 'btn', href: '#/help' }, 'All instructions') : null));
    return;
  }
  const q = new URLSearchParams((location.hash.split('?')[1]) || '').get('q') || '';
  let scale = clamp(Number(await app.kvGet(NS, 'reader_scale', 1)) || 1, 0.8, 2);
  const bookmarks = await getList(app, 'guide_bookmarks');
  let marked = bookmarks.some(b => b.id === g.id);

  // recently read
  const recent = (await getList(app, 'guide_recent')).filter(r => r.id !== g.id);
  recent.unshift({ id: g.id, title: g.title, category: g.category, ts: Date.now() });
  app.api.try('kv.set', { ns: NS, key: 'guide_recent', value: recent.slice(0, 20) }, null, true);

  const body = h('article', { class: 'reader' });
  body.style.setProperty('--reader-scale', String(scale));
  const ast = parseMarkdown(String(g.markdown || ''));
  // Long documents render incrementally so slow phones show the first screen at once.
  const chunks = toDomChunks(ast, 40);
  body.appendChild(chunks[0]());
  let alive = true;
  const rest = chunks.slice(1);
  const done = new Promise(resolve => {
    const idle = window.requestIdleCallback ? fn => window.requestIdleCallback(fn, { timeout: 200 }) : fn => setTimeout(fn, 16);
    const step = () => {
      if (!alive) return resolve(false);
      const t0 = performance.now();
      while (rest.length && performance.now() - t0 < 12) body.appendChild(rest.shift()());
      if (rest.length) idle(step); else resolve(true);
    };
    if (rest.length) idle(step); else resolve(true);
  });
  if (q) done.then(ok => { if (ok) markTerms(body, q.split(/\s+/).filter(t => t.length > 1)); });

  const setScale = d => {
    scale = clamp(Math.round((scale + d) * 10) / 10, 0.8, 2);
    body.style.setProperty('--reader-scale', String(scale));
    app.api.try('kv.set', { ns: NS, key: 'reader_scale', value: scale }, null, true);
  };
  const bmBtn = h('button', { class: 'btn small icon-only' + (marked ? ' on' : ''), 'aria-pressed': String(marked), 'aria-label': 'Bookmark', onclick: () => toggleBookmark() }, icon('bookmark', { size: 18, fill: marked }));
  async function toggleBookmark() {
    const list = (await getList(app, 'guide_bookmarks')).filter(b => b.id !== g.id);
    marked = !marked;
    if (marked) list.unshift({ id: g.id, title: g.title, category: g.category, ts: Date.now() });
    try { await app.kvSet(NS, 'guide_bookmarks', list.slice(0, 200)); } catch (e) { toast(errorText(e), { kind: 'bad' }); }
    bmBtn.setAttribute('aria-pressed', String(marked));
    bmBtn.classList.toggle('on', marked);
    clear(bmBtn).appendChild(icon('bookmark', { size: 18, fill: marked }));
    toast(marked ? 'Bookmarked' : 'Bookmark removed');
  }
  const toc = outline(ast).filter(o => o.level <= 3);
  const relatedBox = h('nav', { class: 'related', 'aria-label': 'Related guides' });
  const back = opts.back ? opts.back : g.part_of ? '#/guides/book/' + enc(String(g.part_of)) : '#/guides/cat/' + encodeURIComponent(String(g.category || ''));
  const nav = (dir, gid) => gid ? h('a', { class: 'btn' + (dir === 'next' ? ' primary' : ''), href: prefix + enc(String(gid)) }, dir === 'prev' ? icon('back') : null, dir === 'prev' ? 'Previous' : 'Next chapter', dir === 'next' ? icon('next') : null) : h('span');

  append(page, 
    h('div', { class: 'reader-bar' },
      h('a', { class: 'btn small ghost icon-only', href: back, 'aria-label': 'Back' }, icon('back')),
      h('div', { class: 'grow small muted' }, String(g.category || '')),
      toc.length > 2 ? h('button', { class: 'btn small', onclick: () => showToc() }, icon('list', { size: 18 }), 'Contents') : null,
      h('button', { class: 'btn small icon-only', 'aria-label': 'Smaller text', onclick: () => setScale(-0.1) }, 'A−'),
      h('button', { class: 'btn small icon-only', 'aria-label': 'Larger text', onclick: () => setScale(0.1) }, 'A+'),
      bmBtn),
    g.summary ? h('p', { class: 'muted' }, String(g.summary)) : null,
    body,
    h('div', { class: 'row', style: { justifyContent: 'space-between', marginTop: '1.5rem' } }, nav('prev', g.prev), nav('next', g.next)),
    relatedBox,
    attribution(g));
  // Related guides (meaning-based, guides.related); nothing shown when the library has no semantic data.
  app.api.try('guides.related', { id: g.id, limit: 6 }, null, true).then(r => {
    const rs = strongRelated(r).filter(x => x.id !== g.id);
    if (!rs.length || !relatedBox.isConnected) return;
    append(relatedBox, h('h2', { class: 'section-title' }, 'Related guides'), h('ul', { class: 'list card' }, rs.map(x => h('li', null,
      h('a', { class: 'item', href: prefix + enc(String(x.id)) }, h('div', { class: 'grow' }, h('div', { class: 'title' }, String(x.title || x.id)), x.summary ? h('div', { class: 'sub clamp2' }, String(x.summary)) : null), icon('next'))))));
  });

  function showToc() {
    const s = sheet('Contents', h('ul', { class: 'list' }, toc.map(o => h('li', null, h('button', {
      class: 'item', onclick: () => { s.close(); done.then(() => scrollToId(o.id)); },
    }, h('span', { class: 'grow' }, (o.level > 2 ? '   ' : '') + o.text))))));
  }

  // link handling: nothing auto-navigates off-app
  const onClick = e => {
    const a = e.target.closest && e.target.closest('.md-link');
    if (!a || !body.contains(a)) return;
    e.preventDefault();
    follow(a.getAttribute('data-href') || '', a.getAttribute('data-kind'));
  };
  const onKey = e => { if ((e.key === 'Enter' || e.key === ' ') && e.target.classList && e.target.classList.contains('md-link')) { e.preventDefault(); follow(e.target.getAttribute('data-href') || '', e.target.getAttribute('data-kind')); } };
  body.addEventListener('click', onClick);
  body.addEventListener('keydown', onKey);

  function follow(href, kind) {
    if (kind === 'anchor') return done.then(() => scrollToId('md-' + slugify(decodeURIComponent(href.slice(1)))));
    if (kind === 'internal') {
      const target = resolveGuideLink(g.id, href);
      if (target) app.nav(prefix + enc(target.id) + (target.anchor ? '?a=' + encodeURIComponent(target.anchor) : ''));
      return;
    }
    sheet('External link', h('div', { class: 'stack' },
      h('p', null, 'This link points outside EmberMesh. It is shown for reference and is not opened automatically (there may be no internet, and links can be unsafe).'),
      h('div', { class: 'copybox' }, href),
      h('div', { class: 'row end' }, h('button', { class: 'btn', onclick: () => copyText(href) }, icon('copy'), 'Copy address'))));
  }
  function scrollToId(hid) {
    const el = body.querySelector('#' + CSS.escape(hid));
    if (el) { el.scrollIntoView({ block: 'start' }); el.setAttribute('tabindex', '-1'); el.focus({ preventScroll: true }); }
  }
  const anchor = new URLSearchParams((location.hash.split('?')[1]) || '').get('a');
  if (anchor) done.then(ok => { if (ok) scrollToId('md-' + slugify(anchor)); });
  return () => { alive = false; };
}

/** Resolve a relative/`guide:` link in guide `fromId` to a guide id (+ optional anchor). Exported for tests. */
export function resolveGuideLink(fromId, href) {
  let s = String(href || '').trim();
  let anchor = '';
  const hash = s.indexOf('#');
  if (hash >= 0) { anchor = s.slice(hash + 1); s = s.slice(0, hash); }
  if (/^guide:/i.test(s)) s = s.slice(6).replace(/^\/+/, '');
  else if (/^ember:/i.test(s)) return null;
  else {
    const base = String(fromId).split('/').slice(0, -1);
    const parts = s.startsWith('/') ? [] : base;
    for (const p of s.replace(/^\/+/, '').split('/')) {
      if (p === '..') parts.pop();
      else if (p && p !== '.') parts.push(p);
    }
    s = parts.join('/');
  }
  s = s.replace(/\.md$/i, '');
  if (!s || !/^[A-Za-z0-9._\-/]+$/.test(s)) return null;
  return { id: s, anchor };
}

function attribution(g) {
  const src = String(g.source || '');
  return h('footer', { class: 'attribution' },
    h('dl', { class: 'kv' },
      h('dt', null, 'Source'), h('dd', null, src === 'original' ? 'Original EmberMesh text' : (String(g.source_title || '') + (src ? ' — ' + src : ''))),
      h('dt', null, 'Licence'), h('dd', null, String(g.license || 'unknown')),
      h('dt', null, 'Attribution'), h('dd', null, String(g.attribution || '—')),
      g.tags && g.tags.length ? [h('dt', null, 'Tags'), h('dd', null, g.tags.map(String).join(', '))] : null,
      h('dt', null, 'ID'), h('dd', { class: 'mono' }, String(g.id))),
    h('p', { class: 'small' }, 'Information may be outdated or wrong for your situation. When in doubt, seek trained help.'));
}

/** Highlight search terms inside rendered text nodes (DOM-safe: wraps text nodes in <mark>). */
function markTerms(root, terms) {
  if (!terms.length) return;
  const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
  const nodes = [];
  const re = new RegExp(terms.map(t => t.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')).join('|'), 'i');
  while (walker.nextNode()) if (re.test(walker.currentNode.nodeValue)) nodes.push(walker.currentNode);
  let first = null;
  for (const n of nodes.slice(0, 500)) {
    const frag = highlight(n.nodeValue, terms);
    if (!first) first = frag.querySelector && frag.querySelector('mark');
    n.parentNode.replaceChild(frag, n);
  }
  if (first) setTimeout(() => first.scrollIntoView({ block: 'center' }), 50);
}
