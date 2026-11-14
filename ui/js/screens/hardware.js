// "What you need": hardware for each connection (rf.hardware, docs/RPC_API.md). Used by the
// Instructions section (full matrix), the Connect-a-radio wizard and Add connection (one entry).
import { h, clear, append } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { empty, errorText } from '../lib/ui.js';

export const PLATFORMS = [['android', 'Android'], ['ios', 'iPhone / iPad'], ['web', 'Browser'], ['desktop', 'Computer app']];
const PLATFORM_LABEL = Object.fromEntries(PLATFORMS);
const guideHref = id => '#/help/read/' + String(id).split('/').map(encodeURIComponent).join('/');

/** This device's platform key for rf.hardware (host.info.platform). */
export function platformOf(app) {
  const p = (app.store.get('host') || {}).platform;
  return PLATFORM_LABEL[p] ? p : 'web';
}

/** Loads rf.hardware; null when the core does not have it yet. */
export async function loadHardware(app, params = {}) {
  try { return (await app.api.call('rf.hardware', params)).methods || []; } catch (e) {
    if (e.code === 'not_implemented' || e.code === 'unsupported') return null;
    throw e;
  }
}

/** Matching method for a link type / endpoint kind / radio preset (best effort). Pure. */
export function methodFor(methods, { link_type, preset, id } = {}) {
  const ms = methods || [];
  return (id && ms.find(m => m.id === id)) || (preset && ms.find(m => m.preset === preset)) || (link_type && ms.find(m => m.link_type === link_type)) || null;
}

/** "You will need" card for one method on one platform. */
export function needsCard(m, platform, opts = {}) {
  const works = (m.platforms || []).includes(platform);
  const note = (m.platform_notes || {})[platform];
  const req = (m.items || []).filter(i => i.required), opt = (m.items || []).filter(i => !i.required);
  const item = i => h('li', null, h('strong', null, String(i.name)), i.why ? h('span', { class: 'muted' }, ' — ' + i.why) : null,
    (i.alternatives || []).length ? h('div', { class: 'small muted' }, 'or: ' + i.alternatives.join('; ')) : null);
  return h('div', { class: 'card stack hw-card', 'data-method': m.id },
    h('div', { class: 'row nowrap' }, h('h2', { class: 'grow' }, opts.title || 'You will need'),
      m.proven === 'untested hardware' ? h('span', { class: 'tag warn', title: 'Built to the specification but not yet tried with real devices' }, 'untested hardware') : h('span', { class: 'tag' }, String(m.proven || ''))),
    h('div', { class: 'muted small' }, String(m.title || '') + (m.what_it_connects ? ' · ' + m.what_it_connects : '')),
    works ? null : h('div', { class: 'notice bad small' }, 'Not possible on ' + (PLATFORM_LABEL[platform] || platform) + '.' + (note ? ' ' + note : '')),
    req.length ? [h('h3', null, 'Required'), h('ul', null, req.map(item))] : null,
    opt.length ? [h('h3', null, 'Optional'), h('ul', null, opt.map(item))] : null,
    works && note ? h('p', { class: 'notice info small' }, PLATFORM_LABEL[platform] + ': ' + note) : null,
    m.steps_summary ? h('p', { class: 'small' }, String(m.steps_summary)) : null,
    m.manual_chapter ? h('a', { href: guideHref(m.manual_chapter) }, icon('book', { size: 16 }), ' Step by step in the manual') : null);
}

/** Instructions → What you need: the whole list as a per-platform matrix with filters. */
export async function hardwarePage(app, root) {
  const page = h('div', { class: 'page hw-page' });
  root.appendChild(page);
  append(page, h('div', { class: 'page-head' }, h('a', { class: 'btn ghost icon-only', href: '#/help', 'aria-label': 'Back to instructions' }, icon('back')), h('h1', null, 'What you need')),
    h('p', { class: 'muted' }, 'Which devices, cables and adapters each kind of connection needs, on each kind of device. ',
      h('a', { href: guideHref('embermesh/what-you-need') }, 'Read the full chapter'), '.'));
  let methods;
  try { methods = await loadHardware(app); } catch (e) { page.appendChild(empty(errorText(e))); return; }
  if (!methods) { page.appendChild(empty('This version of the core has no hardware list yet. See the manual chapter instead.', h('a', { class: 'btn', href: guideHref('embermesh/what-you-need') }, 'Open the chapter'))); return; }
  const mine = platformOf(app);
  const have = h('select', { 'aria-label': 'I have' }, h('option', { value: '' }, 'Any device'), h('option', { value: 'phone' }, 'I have a phone'), h('option', { value: 'computer' }, 'I have a computer'),
    PLATFORMS.map(([v, l]) => h('option', { value: v, selected: false }, 'Only ' + l)));
  const want = h('select', { 'aria-label': 'I want to connect' }, h('option', { value: '' }, 'I want to connect anything'), methods.map(m => h('option', { value: m.id }, 'I want to connect: ' + m.title)));
  const out = h('div');
  const draw = () => {
    const cols = have.value === 'phone' ? ['android', 'ios'] : have.value === 'computer' ? ['web', 'desktop'] : have.value ? [have.value] : PLATFORMS.map(p => p[0]);
    const ms = methods.filter(m => !want.value || m.id === want.value);
    clear(out);
    out.appendChild(h('div', { class: 'table-wrap' }, h('table', { class: 'data hw-matrix' },
      h('thead', null, h('tr', null, h('th', null, 'Connection'), cols.map(c => h('th', { class: c === mine ? 'mine' : null }, PLATFORM_LABEL[c] + (c === mine ? ' (this device)' : ''))))),
      h('tbody', null, ms.map(m => h('tr', null,
        h('th', { scope: 'row' }, h('a', { href: '#/help/hardware?m=' + encodeURIComponent(m.id) }, String(m.title)), h('div', { class: 'small muted' }, String(m.what_it_connects || ''))),
        cols.map(c => {
          const ok = (m.platforms || []).includes(c);
          const note = (m.platform_notes || {})[c];
          return h('td', { class: ok ? 'ok-text' : 'bad-text' }, ok ? '✓ works' : '✗ no', note ? h('div', { class: 'small muted' }, note) : null);
        })))))));
    for (const m of ms) out.appendChild(needsCard(m, cols.includes(mine) ? mine : cols[0], { title: m.title }));
  };
  have.addEventListener('change', draw);
  want.addEventListener('change', draw);
  const pre = new URLSearchParams(location.hash.split('?')[1] || '').get('m');
  if (pre) want.value = pre;
  append(page, h('div', { class: 'row no-print' }, have, want), out);
  draw();
}
