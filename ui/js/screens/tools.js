// Tools: schema-driven runner for every tools.list entry, band-plan browser (rf.bands), Morse
// flash/tone signalling (host.signal.*, with on-screen/WebAudio fallback), station profiles (kv.*).
import { h, clear, debounce, append } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { toast, toastError, sheet, confirmDialog, empty, errorText } from '../lib/ui.js';
import { renderForm } from '../lib/form.js';
import { formatFreq, trimNum } from '../lib/format.js';
import { REGIONS } from './setup.js';

const CAT_TITLES = { antenna: 'Antennas', rf: 'Radio & propagation', power: 'Power', navigation: 'Navigation', signal: 'Signalling', survival: 'Survival', crypto: 'Crypto & codes' };
const STATION_NS = 'stations';

export default async function render(app, params, root) {
  const [mode, ...rest] = params.rest;
  if (mode === 'run' && rest.length) return runner(app, rest.join('/'), root);
  if (mode === 'bands') return bands(app, root);
  if (mode === 'morse') return (await import('./morse.js')).default(app, { rest, query: params.query }, root);
  if (mode === 'stations') return stations(app, root);
  return list(app, root);
}

async function list(app, root) {
  const page = h('div', { class: 'page' });
  root.appendChild(page);
  append(page, h('div', { class: 'page-head' }, h('h1', null, 'Tools')),
    h('div', { class: 'cat-grid' },
      h('a', { class: 'btn', href: '#/tools/bands' }, icon('signal'), h('span', { class: 'grow' }, 'Band plans')),
      h('a', { class: 'btn', href: '#/tools/morse' }, icon('flash'), h('span', { class: 'grow' }, 'Morse send & read')),
      h('a', { class: 'btn', href: '#/tools/stations' }, icon('radar'), h('span', { class: 'grow' }, 'Station profiles'))));
  const r = await app.api.try('tools.list', {}, null, true);
  if (!r) { page.appendChild(empty('Calculators are not available in this build yet.')); return; }
  const groups = new Map();
  for (const t of r.tools || []) {
    const c = t.category || 'other';
    if (!groups.has(c)) groups.set(c, []);
    groups.get(c).push(t);
  }
  for (const [c, ts] of groups) {
    append(page, h('h2', { class: 'section-title' }, CAT_TITLES[c] || c),
      h('ul', { class: 'list card' }, ts.map(t => h('li', null, h('a', { class: 'item', href: '#/tools/run/' + encodeURIComponent(t.id) },
        h('div', { class: 'grow' }, h('div', { class: 'title' }, String(t.title || t.id)), h('div', { class: 'sub' }, String(t.description || ''))), icon('next'))))));
  }
}

// ---------------------------------------------------------------- station profiles
export const FEEDLINES = [
  { id: 'none', label: 'None / direct', db100: { 150: 0, 450: 0, 900: 0 } },
  { id: 'rg58', label: 'RG-58', db100: { 30: 4.9, 150: 11.2, 450: 21, 900: 31 } },
  { id: 'rg8x', label: 'RG-8X', db100: { 30: 3.6, 150: 8.5, 450: 15.4, 900: 22.6 } },
  { id: 'rg213', label: 'RG-213 / RG-8', db100: { 30: 2.2, 150: 5.4, 450: 10, 900: 15.4 } },
  { id: 'lmr400', label: 'LMR-400', db100: { 30: 1.3, 150: 2.9, 450: 5.1, 900: 7.3 } },
  { id: 'ladder', label: 'Ladder / window line', db100: { 30: 0.3, 150: 0.7, 450: 1.3, 900: 2 } },
];

/** Loss (dB) for a feedline over length_m at freq_mhz by log interpolation of the table. Pure. */
export function feedlineLoss(typeId, lengthM, freqMhz) {
  const f = FEEDLINES.find(x => x.id === typeId);
  if (!f || !Number.isFinite(lengthM) || lengthM <= 0) return 0;
  const pts = Object.entries(f.db100).map(([k, v]) => [Number(k), v]).sort((a, b) => a[0] - b[0]);
  const fm = Number.isFinite(freqMhz) && freqMhz > 0 ? freqMhz : 150;
  let per;
  if (fm <= pts[0][0]) per = pts[0][1] * Math.sqrt(fm / pts[0][0]);
  else if (fm >= pts[pts.length - 1][0]) per = pts[pts.length - 1][1] * Math.sqrt(fm / pts[pts.length - 1][0]);
  else {
    for (let i = 0; i < pts.length - 1; i++) {
      const [f1, l1] = pts[i], [f2, l2] = pts[i + 1];
      if (fm >= f1 && fm <= f2) { const t = Math.log(fm / f1) / Math.log(f2 / f1); per = l1 + (l2 - l1) * t; break; }
    }
  }
  return Math.round(per * lengthM / 100 * 100) / 100;
}

/**
 * Map a station profile onto a tool's input keys. side = 'tx' | 'rx'. Matches keys by name so any
 * calculator that uses tx_ / rx_ prefixed power/gain/height/cable-loss inputs can use profiles. Pure.
 */
export function profileToInputs(profile, inputs, side, freqMhz) {
  const out = {};
  const keys = (inputs || []).map(f => f.key);
  const pick = re => keys.find(k => k.startsWith(side + '_') && re.test(k));
  const set = (re, v) => { const k = pick(re); if (k && Number.isFinite(v)) out[k] = v; };
  set(/power/, profile.power_w);
  set(/gain/, profile.antenna_gain_dbi);
  set(/height/, profile.height_m);
  set(/(cable|feed|line).*loss|loss/, feedlineLoss(profile.feedline, profile.feedline_m, freqMhz));
  return out;
}

async function loadStations(app) {
  const r = await app.api.try('kv.list', { ns: STATION_NS }, { keys: [] }, true);
  const out = [];
  for (const k of r.keys || []) {
    const v = await app.kvGet(STATION_NS, k, null);
    if (v && typeof v === 'object') out.push({ ...v, key: k });
  }
  return out.sort((a, b) => String(a.name).localeCompare(String(b.name)));
}

const STATION_FIELDS = [
  { key: 'name', label: 'Profile name', type: 'text', required: true },
  { key: 'radio', label: 'Radio', type: 'text', help: 'e.g. Baofeng UV-5R, Yaesu FT-891, T-Beam' },
  { key: 'power_w', label: 'Transmit power', type: 'number', unit: 'W', min: 0, max: 2000, default: 5 },
  { key: 'feedline', label: 'Feedline', type: 'select', options: FEEDLINES.map(f => ({ value: f.id, label: f.label })), default: 'none' },
  { key: 'feedline_m', label: 'Feedline length', type: 'number', unit: 'm', min: 0, max: 500, default: 0 },
  { key: 'antenna', label: 'Antenna', type: 'text', help: 'e.g. stock rubber duck, roll-up J-pole, 3-element Yagi' },
  { key: 'antenna_gain_dbi', label: 'Antenna gain', type: 'number', unit: 'dBi', min: -20, max: 30, default: 0, help: 'Rubber duck ≈ −2 dBi, ¼-wave ≈ 2, J-pole ≈ 3, small Yagi ≈ 7–10.' },
  { key: 'height_m', label: 'Antenna height above ground', type: 'number', unit: 'm', min: 0, max: 1000, default: 1.5 },
  { key: 'notes', label: 'Notes', type: 'textarea' },
];

async function stations(app, root) {
  const page = h('div', { class: 'page' });
  root.appendChild(page);
  async function draw() {
    const list = await loadStations(app);
    append(clear(page), 
      h('div', { class: 'page-head' }, h('a', { class: 'btn ghost icon-only', href: '#/tools', 'aria-label': 'Back' }, icon('back')), h('h1', null, 'Station profiles'),
        h('button', { class: 'btn primary', onclick: () => edit(null) }, icon('plus'), 'New')),
      h('p', { class: 'muted small' }, 'Save your radio, feedline, antenna and height once and reuse them in the link-budget calculator.'),
      list.length ? h('ul', { class: 'list card' }, list.map(s => h('li', null, h('button', { class: 'item', onclick: () => edit(s) },
        h('div', { class: 'grow' }, h('div', { class: 'title' }, String(s.name)), h('div', { class: 'sub' }, [s.radio, s.power_w != null ? s.power_w + ' W' : '', s.antenna, s.height_m != null ? s.height_m + ' m high' : ''].filter(Boolean).join(' · '))), icon('edit')))))
        : empty('No station profiles yet.'));
  }
  function edit(s) {
    const form = renderForm(STATION_FIELDS, s || {});
    const sh = sheet(s ? 'Edit station' : 'New station', h('div', { class: 'stack' }, form.el,
      h('div', { class: 'row end' },
        s ? h('button', { class: 'btn danger', onclick: async () => { if (await confirmDialog('Delete profile?', String(s.name), { ok: 'Delete', danger: true })) { await app.api.try('kv.delete', { ns: STATION_NS, key: s.key }, null, true); sh.close(); draw(); } } }, 'Delete') : null,
        h('button', {
          class: 'btn primary', onclick: async () => {
            if (Object.keys(form.validate()).length) return;
            const key = s ? s.key : 'st-' + Date.now().toString(36);
            try { await app.kvSet(STATION_NS, key, form.values()); sh.close(); draw(); } catch (e) { toastError(e, 'Not saved'); }
          },
        }, 'Save'))));
  }
  await draw();
}

// ---------------------------------------------------------------- runner
async function runner(app, id, root) {
  const page = h('div', { class: 'page' });
  root.appendChild(page);
  const r = await app.api.try('tools.list', {}, { tools: [] }, true);
  const tool = (r.tools || []).find(t => t.id === id);
  page.appendChild(h('div', { class: 'page-head' }, h('a', { class: 'btn ghost icon-only', href: '#/tools', 'aria-label': 'Back' }, icon('back')), h('h1', null, tool ? String(tool.title || id) : id)));
  if (!tool) { page.appendChild(empty('Tool not found.')); return; }
  const saved = await app.kvGet('tool_inputs', id, {});
  const out = h('div', { 'aria-live': 'polite' });
  const form = renderForm(tool.inputs || [], saved || {}, { onChange: () => auto() });
  const runBtn = h('button', { class: 'btn primary block', onclick: () => run(true) }, 'Calculate');

  // station profiles for tx_/rx_ inputs
  const hasSides = (tool.inputs || []).some(f => /^tx_/.test(f.key)) || (tool.inputs || []).some(f => /^rx_/.test(f.key));
  let profBox = null;
  if (hasSides) {
    const st = await loadStations(app);
    profBox = h('div', { class: 'card stack' }, h('h2', null, 'Use a station profile'),
      st.length ? h('div', { class: 'stack' }, ['tx', 'rx'].filter(side => (tool.inputs || []).some(f => f.key.startsWith(side + '_'))).map(side => {
        const sel = h('select', { 'aria-label': side === 'tx' ? 'Transmitting station' : 'Receiving station' }, h('option', { value: '' }, side === 'tx' ? 'Transmitter: choose…' : 'Receiver: choose…'), st.map(s => h('option', { value: s.key }, String(s.name))));
        sel.addEventListener('change', () => {
          const p = st.find(s => s.key === sel.value);
          if (!p) return;
          const freq = form.values().freq_mhz;
          form.set(profileToInputs(p, tool.inputs, side, freq));
          run(false);
        });
        return sel;
      })) : h('p', { class: 'muted small' }, 'No saved stations. ', h('a', { href: '#/tools/stations' }, 'Create one'), ' to fill these fields quickly.'));
  }

  append(page, 
    tool.description ? h('p', { class: 'muted' }, String(tool.description)) : null,
    profBox,
    h('div', { class: 'card' }, form.el, runBtn),
    out);

  let seq = 0;
  async function run(explicit) {
    if (explicit && Object.keys(form.validate()).length) return;
    const inputs = form.values();
    const my = ++seq;
    try {
      const res = await app.api.call('tools.run', { id, inputs });
      if (my !== seq) return;
      app.api.try('kv.set', { ns: 'tool_inputs', key: id, value: inputs }, null, true);
      showResult(res);
    } catch (e) {
      if (my !== seq) return;
      if (explicit || e.code !== 'bad_params') clear(out).appendChild(h('div', { class: 'notice bad' }, errorText(e)));
    }
  }
  const auto = debounce(() => run(false), 500);
  function showResult(res) {
    clear(out);
    const outputs = res.outputs || [];
    if (outputs.length) out.appendChild(h('div', { class: 'card' }, h('h2', null, 'Result'), h('table', { class: 'data' }, h('tbody', null, outputs.map(o => h('tr', null,
      h('th', { scope: 'row' }, String(o.label || o.key)),
      h('td', { class: 'num' }, typeof o.value === 'number' ? trimNum(o.value, Math.abs(o.value) >= 100 ? 1 : 3) : String(o.value ?? '')),
      h('td', null, String(o.unit || ''))))))));
    if (res.text) out.appendChild(h('div', { class: 'card' }, h('pre', { class: 'out', tabindex: '0', 'aria-label': 'Diagram' }, String(res.text))));
    if (res.notes && res.notes.length) out.appendChild(h('div', { class: 'card' }, h('ul', null, res.notes.map(n => h('li', null, String(n))))));
  }
  if (saved && Object.keys(saved).length) run(false);
  return () => auto.cancel();
}

// ---------------------------------------------------------------- band plans
async function bands(app, root) {
  const page = h('div', { class: 'page' });
  root.appendChild(page);
  const region = h('select', { 'aria-label': 'Region' }, REGIONS.map(([v, l]) => h('option', { value: v, selected: v === (app.store.get('settings').region || 'US') }, l)));
  const filter = h('input', { type: 'search', placeholder: 'Filter (e.g. GMRS, 446, LoRa)', 'aria-label': 'Filter services' });
  const out = h('div');
  append(page, h('div', { class: 'page-head' }, h('a', { class: 'btn ghost icon-only', href: '#/tools', 'aria-label': 'Back' }, icon('back')), h('h1', null, 'Band plans')),
    h('div', { class: 'row nowrap' }, h('div', { class: 'grow' }, region), h('div', { class: 'grow' }, filter)), out);
  let services = [];
  async function load() {
    const r = await app.api.try('rf.bands', { region: region.value }, null, true);
    if (!r) { clear(out).appendChild(empty('Band plans are not available yet.')); return; }
    services = r.services || [];
    draw();
  }
  function draw() {
    const q = filter.value.trim().toLowerCase();
    const list = services.filter(s => !q || JSON.stringify(s).toLowerCase().includes(q));
    clear(out);
    if (!list.length) { out.appendChild(empty('No services for this region.')); return; }
    for (const s of list) {
      out.appendChild(h('details', { class: 'card' },
        h('summary', null, h('span', { class: 'grow' }, String(s.name), ' ', h('span', { class: 'tag' + (/none|licen[cs]e-free|no licen/i.test(String(s.license)) ? ' ok' : ' warn') }, String(s.license || '')), s.max_power_w ? h('span', { class: 'tag' }, '≤ ' + s.max_power_w + ' W') : null)),
        s.notes ? h('p', { class: 'small muted' }, String(s.notes)) : null,
        h('div', { class: 'table-wrap' }, h('table', { class: 'data' },
          h('thead', null, h('tr', null, h('th', null, 'Channel'), h('th', null, 'Frequency'), h('th', null, 'Width'), h('th', null, 'Notes'))),
          h('tbody', null, (s.channels || []).map(c => h('tr', null, h('td', null, String(c.name)), h('td', { class: 'num mono' }, formatFreq(c.freq_hz)), h('td', { class: 'num' }, c.bw_hz ? formatFreq(c.bw_hz) : ''), h('td', { class: 'small' }, String(c.notes || '')))))))));
    }
  }
  region.addEventListener('change', load);
  filter.addEventListener('input', debounce(draw, 150));
  await load();
}

