// "More" menu for phones (the wide sidebar shows these directly).
import { h } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { getPrefs } from '../lib/prefs.js';

const ITEMS = [
  ['board', 'board', 'Bulletins', 'Water, food, medical, hazards, trade, missing persons'],
  ['links', 'link', 'Links & radios', 'Connections, radios, hub, WebRTC pairing, net plan'],
  ['tools', 'tool', 'Tools', 'Antenna and link calculators, band plans, Morse signalling'],
  ['extras', 'notes', 'Notes & lists', 'Personal notes, checklists, supply inventory'],
  ['settings', 'gear', 'Settings', 'Profile, security, backup, sneakernet bundles'],
  ['people/stats', 'signal', 'Mesh statistics', 'Relayed, stored, airtime per link'],
];

// Simple mode takes these out of the navigation; More lists them so everything stays reachable.
const SIMPLE_EXTRA = [['people', 'people', 'People', 'People you know, nearby devices, map']];

export default async function render(app, _p, root) {
  let simple = false;
  try { simple = !!getPrefs().simple_mode; } catch { /* prefs unavailable */ }
  const items = simple ? [...SIMPLE_EXTRA, ...ITEMS] : ITEMS;
  root.appendChild(h('div', { class: 'page' },
    h('div', { class: 'page-head' }, h('h1', null, 'More')),
    h('ul', { class: 'list card' }, items.map(([href, ic, title, sub]) => h('li', null,
      h('a', { class: 'item', href: '#/' + href }, icon(ic, { size: 28 }),
        h('div', { class: 'grow' }, h('div', { class: 'title' }, title), h('div', { class: 'sub' }, sub)), icon('next')))))));
}
