// Instructions: the operator manual (guide category "embermesh", book embermesh/manual) as a
// task-grouped table of contents with this platform's chapters first, its own search, the
// "What you need" hardware matrix, and quick actions. Chapters are linked by id; ids that are not in
// the installed library are simply left out.
import { h, clear, append, debounce, highlight } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { empty, errorText } from '../lib/ui.js';
import { reader } from './guides.js';
import { hardwarePage, platformOf, PLATFORMS } from './hardware.js';
import { askBox } from './ask.js';

const enc = s => String(s).split('/').map(encodeURIComponent).join('/');
const read = id => '#/help/read/' + enc(id);

/** Task groups (slugs under embermesh/). Unknown chapters go to "More". Exported for tests. */
export const GROUPS = [
  ['Start here', ['quick-start', 'screens-tour', 'library-search', 'how-it-works', 'quick-reference']],
  ['Connect devices', ['what-you-need', 'phone-to-phone', 'hub-setup', 'spreading-the-app']],
  ['Radios', ['radio-gateway', 'handheld-audio-modem', 'baofeng-uv5r', 'kiss-tnc-packet', 'lora', 'sdr-listening', 'antennas', 'relays-and-repeaters', 'running-a-net']],
  ['Emergencies and the field', ['emergency-use', 'morse-reading', 'power-and-field', 'sneakernet']],
  ['Settings, accessibility and security', ['settings-reference', 'accessibility', 'security']],
  ['When something does not work', ['troubleshooting']],
  ['About', ['building-from-source', 'licences']],
];
const APP_CHAPTER = { android: 'app-android', ios: 'app-ios', web: 'app-web', desktop: 'app-desktop' };

/** Groups the installed manual chapters; this platform's app chapter first. Pure. */
export function groupChapters(guides, platform) {
  const bySlug = new Map((guides || []).filter(g => g && typeof g.id === 'string' && g.id.startsWith('embermesh/')).map(g => [g.id.slice(10), g]));
  const used = new Set();
  const out = [];
  const mine = APP_CHAPTER[platform];
  const apps = Object.values(APP_CHAPTER).filter(s => bySlug.has(s)).sort((a, b) => (b === mine) - (a === mine));
  if (apps.length) { out.push(['Your device', apps.map(s => bySlug.get(s))]); apps.forEach(s => used.add(s)); }
  for (const [title, slugs] of GROUPS) {
    const gs = slugs.filter(s => bySlug.has(s) && !used.has(s)).map(s => { used.add(s); return bySlug.get(s); });
    if (gs.length) out.push([title, gs]);
  }
  const rest = [...bySlug.entries()].filter(([s]) => !used.has(s)).map(([, g]) => g).sort((a, b) => (a.order ?? 999) - (b.order ?? 999) || String(a.title).localeCompare(String(b.title)));
  if (rest.length) out.push(['More', rest]);
  return out;
}

/** Strips the "EmberMesh 7: " book prefix from chapter titles. */
const shortTitle = t => String(t || '').replace(/^EmberMesh\s+\d+\s*[:.-]\s*/i, '');

export default async function render(app, params, root) {
  const [mode, ...rest] = params.rest || [];
  if (mode === 'read' && rest.length) return reader(app, rest.join('/'), root, { prefix: '#/help/read/', back: '#/help' });
  if (mode === 'hardware') return hardwarePage(app, root);
  return home(app, root, params.query ? params.query.get('q') || '' : '');
}

async function home(app, root, q0) {
  const platform = platformOf(app);
  const plat = (PLATFORMS.find(p => p[0] === platform) || [, 'this device'])[1];
  const input = h('input', { type: 'search', placeholder: 'Search the instructions', 'aria-label': 'Search the instructions', value: q0, enterkeyhint: 'search' });
  const results = h('div', { 'aria-live': 'polite' });
  const toc = h('div');
  const askSlot = h('div');
  askBox(app, { category: 'embermesh', title: 'Ask the instructions', placeholder: 'e.g. how do I connect two phones?' }).then(b => { if (b) askSlot.appendChild(b); });
  const page = h('div', { class: 'page help-page' },
    h('div', { class: 'page-head' }, h('h1', null, 'Instructions'), h('button', { class: 'btn ghost small no-print', onclick: () => window.print() }, 'Print')),
    h('p', { class: 'muted' }, 'How to use EmberMesh and set everything up — on ' + plat + ' first. Everything here works offline.'),
    input, results, askSlot,
    h('div', { class: 'cat-grid help-actions' },
      h('a', { class: 'btn', href: '#/links/radio' }, icon('signal'), h('span', { class: 'grow' }, 'Connect a radio')),
      h('a', { class: 'btn', href: '#/help/hardware' }, icon('tool'), h('span', { class: 'grow' }, 'What you need')),
      h('a', { class: 'btn', href: '#/links/add' }, icon('link'), h('span', { class: 'grow' }, 'Add a connection')),
      h('a', { class: 'btn', href: '#/tools/morse?tab=camera' }, icon('flash'), h('span', { class: 'grow' }, 'Read Morse code'))),
    toc);
  root.appendChild(page);

  let guides = [];
  try {
    const r = await app.api.call('guides.list', { category: 'embermesh', limit: 500 });
    guides = r.guides || [];
  } catch (e) { if (e.code !== 'not_implemented') toc.appendChild(empty(errorText(e))); }
  if (!guides.length) {
    toc.appendChild(empty('The operator manual is not installed in this build, or the library is still loading. The buttons above still work.'));
  } else {
    for (const [title, gs] of groupChapters(guides, platform)) {
      toc.appendChild(h('section', { class: 'help-group' }, h('h2', { class: 'section-title' }, title),
        h('ul', { class: 'list card' }, gs.map(g => h('li', null, h('a', { class: 'item', href: read(g.id) },
          h('div', { class: 'grow' }, h('div', { class: 'title' }, shortTitle(g.title) || g.id), g.summary ? h('div', { class: 'sub' }, String(g.summary)) : null), icon('next')))))));
    }
  }

  let seq = 0;
  const search = debounce(async () => {
    const q = input.value.trim();
    const my = ++seq;
    if (q.length < 2) { clear(results); toc.hidden = false; return; }
    let r;
    try { r = await app.api.call('guides.search', { query: q, category: 'embermesh', limit: 20 }); } catch (e) { if (my === seq) clear(results).appendChild(h('p', { class: 'bad-text' }, errorText(e))); return; }
    if (my !== seq) return;
    toc.hidden = true;
    const rs = (r.results || []).filter(x => String(x.id).startsWith('embermesh/'));
    clear(results);
    results.appendChild(h('h2', { class: 'section-title' }, rs.length + ' result' + (rs.length === 1 ? '' : 's') + ' in the instructions'));
    if (!rs.length) { results.appendChild(h('p', { class: 'muted' }, 'Nothing found. Try other words, or search the whole library under Guides.')); return; }
    results.appendChild(h('ul', { class: 'list' }, rs.map(x => h('li', null, h('a', { class: 'item', href: read(x.id) + '?q=' + encodeURIComponent(q) },
      h('div', { class: 'grow' }, h('div', { class: 'title' }, highlight(shortTitle(x.title), x.terms || q.split(/\s+/))), h('div', { class: 'snippet' }, highlight(String(x.snippet || ''), x.terms || q.split(/\s+/)))))))));
  }, 180);
  input.addEventListener('input', search);
  if (q0) search();
  return () => search.cancel();
}
