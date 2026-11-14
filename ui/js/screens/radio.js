// Connect a radio: guided wizard for a handheld (Baofeng UV-5R …) or any other radio.
// radio (rf.radios) → how it is connected → you will need (rf.hardware) → settings on the radio →
// band / licence → device details (host.link.create with the right audio `preset`) → levels →
// on-air test → done. Methods this host cannot do are shown disabled with the reason.
import { h, clear, append, debounce } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { toast, toastError, empty, errorText } from '../lib/ui.js';
import { renderForm } from '../lib/form.js';
import { linkRefOptions, usableCallsign, audioMeter, manualFor } from '../lib/core_spec.js';
import { legalNotice, callsignPrompt, levelMeter, MeterPoller, manualLink } from './links.js';
import { loadHardware, methodFor, needsCard, platformOf } from './hardware.js';

const GENERIC = [{ id: 'generic-handheld', maker: 'Generic', model: 'Any handheld radio', kind: 'handheld', vox: true, data_port: false, builtin_tnc: false, bluetooth: false, verified: false,
  settings: [{ label: 'Squelch', value: 'Low but closed on an empty channel', how: 'Radio menu' }, { label: 'Dual watch / scan', value: 'Off', how: 'Radio menu' }, { label: 'Roger and key beeps', value: 'Off', how: 'Radio menu' }],
  cable_notes: '', notes: '', recommended: { preset: 'handheld_vox', link: { type: 'audio', config: { preset: 'handheld_vox' } } } }];

/** Connection methods; `need` says which host endpoint kinds make it possible. Exported for tests. */
export const METHODS = [
  { id: 'handheld_vox', type: 'audio', kinds: ['audio'], hw: 'radio_vox', title: 'Audio cable, radio keys itself (VOX)', sub: 'A cable from the device’s headset socket to the radio. Simplest with a cable.' },
  { id: 'handheld_ptt', type: 'audio', kinds: ['audio'], needPtt: true, hw: 'radio_ptt', title: 'Cable or interface with a PTT line', sub: 'An interface (AIOC, Digirig class) or serial adapter keys the transmitter. Most reliable.' },
  { id: 'acoustic', type: 'audio', kinds: ['audio'], hw: 'radio_acoustic', title: 'No cable — hold the device to the radio', sub: 'Speaker to the radio’s microphone; you press PTT when asked. Slow, but works with anything.' },
  { id: 'tnc', type: 'kiss', kinds: ['ble', 'bt_spp', 'serial', 'tcp'], hw: 'bt_tnc', title: 'Bluetooth or USB TNC, or a radio with a built-in TNC', sub: 'A packet modem does the sound; the device talks KISS to it.' },
  { id: 'sound_card', type: 'audio', kinds: ['audio'], hw: 'radio_ptt', title: 'Base or mobile radio with a data port', sub: 'Sound-card interface on the radio’s data (mini-DIN) socket.' },
];

/**
 * Which methods this host can do: [{...m, ok, why}]. Uses host capability hints generically
 * (host.info.audio = {serial_ptt?, presets_supported?, presets_unsupported?:{preset: reason}}). Pure.
 */
export function methodsFor(hostKinds, radio, audio) {
  const au = audio && typeof audio === 'object' ? audio : {};
  const avail = new Map((hostKinds || []).map(k => [k.kind, k]));
  return METHODS.map(m => {
    const ks = m.kinds.filter(k => avail.has(k) && avail.get(k).available !== false);
    let ok = ks.length > 0, why = '';
    if (!ok) why = m.type === 'kiss' ? 'This device has no Bluetooth, serial or TCP connection for a TNC.' : (avail.get('audio') && avail.get('audio').unavailable_reason) || 'This device offers no sound-card connection.';
    const unsup = au.presets_unsupported && typeof au.presets_unsupported === 'object' ? au.presets_unsupported[m.id] : undefined;
    if (ok && unsup !== undefined) { ok = false; why = typeof unsup === 'string' && unsup ? unsup : 'Not supported on this device.'; }
    if (ok && m.type === 'audio' && Array.isArray(au.presets_supported) && !au.presets_supported.includes(m.id)) { ok = false; why = 'Not supported on this device.'; }
    if (ok && m.needPtt && au.serial_ptt === false) { ok = false; why = 'This device cannot key a PTT line — use the radio’s VOX or press PTT yourself.'; }
    if (ok && m.needPtt) {
      const a = avail.get('audio');
      const pttField = a && (a.config || []).some(f => f.key === 'ptt' || /link_id$/.test(f.key));
      const serial = avail.has('serial') && avail.get('serial').available !== false;
      if (!pttField && !serial) { ok = false; why = 'This device cannot drive a PTT line (no serial port access) — use VOX or no cable.'; }
    }
    if (ok && m.id === 'sound_card' && radio && radio.kind === 'handheld' && !radio.data_port) why = 'Handhelds usually have no data port.';
    if (ok && m.id === 'tnc' && radio && !radio.builtin_tnc) why = why || 'Needs a separate TNC unless the radio has one built in.';
    return { ...m, kindsOk: ks, ok, why };
  });
}

export default async function render(app, params, root) {
  const page = h('div', { class: 'page radio-wizard' });
  root.appendChild(page);
  const host = app.store.get('host') || {};
  const kinds = host.endpoint_kinds || [];
  const tr = await app.api.try('links.types', {}, { types: [] }, true);
  const types = tr.types || [];
  let hw = null;
  try { hw = await loadHardware(app, { platform: platformOf(app) }); } catch { hw = null; }
  const st = { radio: null, method: null, licence: 'amateur', link: null };
  let stop = () => {};
  const head = (title, n) => h('div', { class: 'page-head' }, h('a', { class: 'btn ghost icon-only', href: '#/links', 'aria-label': 'Back to links' }, icon('back')), h('h1', null, title), n ? h('span', { class: 'tag' }, 'step ' + n + ' of 8') : null);
  const nav = (back, next, nextLabel = 'Next') => h('div', { class: 'row end' }, back ? h('button', { class: 'btn', onclick: back }, icon('back'), 'Back') : null, next ? h('button', { class: 'btn primary', onclick: next }, nextLabel, icon('next')) : null);

  // 1. radio
  async function stepRadio() {
    stop();
    const q = h('input', { type: 'search', placeholder: 'Search: UV-5R, Yaesu, Kenwood…', 'aria-label': 'Search radios' });
    const list = h('ul', { class: 'list card' });
    append(clear(page), head('Connect a radio', 1), h('p', { class: 'muted' }, 'Which radio do you have? Pick “Any handheld radio” if it is not listed.'), q, list);
    const load = async () => {
      let rs;
      try { rs = (await app.api.call('rf.radios', q.value.trim() ? { query: q.value.trim() } : {})).radios || []; } catch (e) {
        if (e.code !== 'not_implemented') { clear(list).appendChild(h('li', null, empty(errorText(e)))); return; }
        rs = GENERIC; // core without a radio database yet
      }
      if (!rs.some(r => r.id && r.id.startsWith('generic'))) rs = [...rs, ...GENERIC.filter(g => !q.value.trim())];
      clear(list);
      if (!rs.length) list.appendChild(h('li', null, empty('No radio matches. Choose “Any handheld radio”.')));
      for (const r of rs) list.appendChild(h('li', null, h('button', { class: 'item', onclick: () => { st.radio = r; const lc = r.recommended && r.recommended.link && r.recommended.link.config; st.licence = lc && lc.licence === 'licence_free' ? 'licence_free' : 'amateur'; stepMethod(); } },
        h('div', { class: 'grow' }, h('div', { class: 'title' }, r.maker && r.maker !== 'Generic' ? r.maker + ' ' + r.model : r.model),
          h('div', { class: 'sub' }, [r.kind, (r.bands || []).join('/'), r.connector, r.builtin_tnc ? 'built-in TNC' : '', r.bluetooth ? 'Bluetooth' : ''].filter(Boolean).join(' · '))), icon('next'))));
    };
    q.addEventListener('input', debounce(load, 200));
    await load();
  }

  // 2. how it is connected
  function stepMethod() {
    const ms = methodsFor(kinds, st.radio, host.audio);
    const recd = st.radio.recommended || {};
    const rec = recd.preset || (recd.link && recd.link.type === 'kiss' ? 'tnc' : 'handheld_vox');
    append(clear(page), head('How is it connected?', 2), h('p', { class: 'muted' }, radioName() + ' — choose how the radio is connected to this device.'),
      h('ul', { class: 'list card' }, ms.map(m => h('li', null, h('button', { class: 'item', disabled: !m.ok, 'aria-disabled': String(!m.ok), 'data-method': m.id, onclick: () => { st.method = m; stepNeeds(); } },
        h('div', { class: 'grow' }, h('div', { class: 'title' }, m.title, ' ', (m.id === rec || (rec === 'tnc' && m.id === 'tnc')) ? h('span', { class: 'tag ok' }, 'recommended') : null),
          h('div', { class: 'sub' }, m.ok ? m.sub : m.why), m.ok && m.why ? h('div', { class: 'sub warn-text' }, m.why) : null),
        m.ok ? icon('next') : h('span', { class: 'tag' }, 'not here'))))),
      nav(stepRadio));
  }

  // 3. you will need
  function stepNeeds() {
    const m = hw && (methodFor(hw, { id: st.method.hw }) || methodFor(hw, { preset: st.method.id }) || methodFor(hw, { link_type: st.method.type }));
    append(clear(page), head('You will need', 3),
      m ? needsCard(m, platformOf(app)) : h('div', { class: 'card' }, h('p', null, st.method.sub), h('p', { class: 'muted small' }, 'The parts list is not in this version yet; see the manual chapter.'), manualLink(st.method.type, st.method.kindsOk[0])),
      st.radio.cable_notes ? h('p', { class: 'notice info small' }, String(st.radio.cable_notes)) : null,
      nav(stepMethod, stepChecklist, 'I have these'));
  }

  // 4. settings to make on the radio
  function stepChecklist() {
    const r = st.radio;
    append(clear(page), head('Set up the radio', 4),
      h('div', { class: 'row' }, h('strong', { class: 'grow' }, radioName()), r.verified ? h('span', { class: 'tag ok' }, 'checked against the manual') : h('span', { class: 'tag warn', title: 'Not yet checked against the maker’s manual' }, 'unverified — check your radio’s manual')),
      (r.settings || []).length ? h('ul', { class: 'list card radio-checklist' }, r.settings.map(s => h('li', null, h('label', { class: 'item check' }, h('input', { type: 'checkbox' }),
        h('div', { class: 'grow' }, h('div', { class: 'title' }, String(s.label) + ': ' + String(s.value)), s.how ? h('div', { class: 'sub mono' }, String(s.how)) : null)))))
        : h('p', { class: 'muted' }, 'No specific settings known for this radio.'),
      r.notes ? h('p', { class: 'small' }, String(r.notes)) : null,
      nav(stepNeeds, stepLicence));
  }

  // 5. band / licence
  function stepLicence() {
    const box = h('div');
    const sel = h('select', { 'aria-label': 'Band and licence' },
      h('option', { value: 'amateur', selected: st.licence === 'amateur' }, 'Amateur (ham) band — I have a licence'),
      h('option', { value: 'licence_free', selected: st.licence === 'licence_free' }, 'Licence-free / emergency use (e.g. PMR446, FRS/GMRS where allowed)'));
    const draw = () => {
      st.licence = sel.value;
      clear(box);
      if (st.licence === 'amateur') {
        box.appendChild(legalNotice());
        if (!usableCallsign((app.store.get('info') || {}).callsign)) box.appendChild(callsignPrompt(app, () => draw()));
      } else box.appendChild(h('p', { class: 'muted small' }, 'Encryption is allowed. Check what your country permits on the channel you use (power, data modes).'));
    };
    sel.addEventListener('change', draw);
    append(clear(page), head('Band and licence', 5), sel, box, nav(stepMethod, () => {
      if (st.licence === 'amateur' && !usableCallsign((app.store.get('info') || {}).callsign)) { toast('Enter your callsign first', { kind: 'bad' }); return; }
      stepDevice();
    }));
    draw();
  }

  // 6. device details → host.link.create
  function stepDevice() {
    const m = st.method;
    const kindSel = h('select', { 'aria-label': 'Connection' }, m.kindsOk.map(k => h('option', { value: k }, (kinds.find(x => x.kind === k) || {}).title || k)));
    const formBox = h('div');
    let form = null;
    const drawForm = () => {
      const k = kinds.find(x => x.kind === kindSel.value) || { config: [] };
      const init = {};
      if (m.id === 'handheld_vox' || m.id === 'acoustic') { for (const f of k.config || []) if (f.key === 'ptt') init.ptt = 'vox'; }
      form = renderForm(k.config || [], init, { linkOptions: linkRefOptions(app.store.get('links'), 0) });
      clear(formBox).appendChild(form.el);
    };
    kindSel.addEventListener('change', drawForm);
    const err = h('div', { class: 'field-error', role: 'alert' });
    const btn = h('button', { class: 'btn primary block' }, 'Create the link');
    btn.addEventListener('click', async () => {
      err.textContent = '';
      if (Object.keys(form.validate()).length) return;
      const t = types.find(x => x.type === m.type);
      const keys = new Set(((t && t.config) || []).map(f => f.key));
      const want = { ...((st.radio.recommended && st.radio.recommended.link && st.radio.recommended.link.type === m.type && st.radio.recommended.link.config) || {}), preset: m.id, licence: st.licence };
      const info = app.store.get('info') || {};
      if (usableCallsign(info.callsign)) want.callsign = info.callsign;
      const config = Object.fromEntries(Object.entries(want).filter(([k]) => keys.has(k))); // only fields this core knows
      if (keys.has('preset') && t) { const pf = t.config.find(f => f.key === 'preset'); if (pf && !(pf.options || []).some(o => o.value === config.preset)) delete config.preset; }
      btn.disabled = true;
      const slow = setTimeout(() => { err.textContent = 'Still opening the device… answer any permission prompt (microphone, USB, Bluetooth).'; }, 6000);
      try {
        const r = await app.api.call('host.link.create', { name: radioName(), endpoint: { kind: kindSel.value, config: form.values() }, link: { type: m.type, config } });
        st.link = r.link;
        toast('Link created', { kind: 'ok' });
        if (page.isConnected) (m.type === 'audio' ? stepLevels : stepTest)();
      } catch (e) { err.textContent = errorText(e); } finally { clearTimeout(slow); btn.disabled = false; }
    });
    append(clear(page), head('This device', 6), h('p', { class: 'muted' }, 'Choose the sound card / port the radio is connected to. Plug everything in first.'),
      m.kindsOk.length > 1 ? kindSel : null, h('div', { class: 'card' }, formBox), err, btn, nav(stepLicence));
    drawForm();
  }

  // 7. levels
  function stepLevels() {
    const meter = levelMeter();
    append(clear(page), head('Set the audio level', 7),
      h('div', { class: 'card stack' }, h('p', null, 'Let the radio receive a busy channel (or have someone transmit). Turn the radio volume until the bar sits in the middle and the hint says “Level OK”; the CLIP lamp must stay off.'), meter.el),
      nav(null, stepTest));
    const poller = new MeterPoller(app, new Map([[st.link.id, { meter }]]));
    poller.watch(st.link.id, { meter });
    meter.set(audioMeter(st.link.status) || { level: -120, hint: 'no_audio' });
    stop = () => poller.stop();
  }

  // 8. on-air test
  function stepTest() {
    stop();
    const out = h('dl', { class: 'kv' });
    const pingSel = h('select', { 'aria-label': 'Node to ping' }, h('option', { value: '' }, 'Ping a device… (choose)'), (app.store.get('peers') || []).map(p => h('option', { value: p.node_id }, p.name || p.node_id)));
    const refresh = async () => {
      const l = ((await app.api.try('links.list', {}, { links: [] }, true)).links || []).find(x => x.id === st.link.id);
      if (!l) return;
      const s = l.stats || {};
      clear(out);
      append(out, h('dt', null, 'State'), h('dd', null, l.state), h('dt', null, 'Sent'), h('dd', null, (s.tx_frames || 0) + ' frames'), h('dt', null, 'Received'), h('dd', null, (s.rx_frames || 0) + ' frames'), h('dt', null, 'Errors'), h('dd', null, String(s.rx_errors || 0)));
    };
    const timer = setInterval(refresh, 1000);
    stop = () => clearInterval(timer);
    append(clear(page), head('On-air test', 8),
      h('p', null, 'Send a short test on the Public channel and watch the counters. A second device with EmberMesh on the same frequency should receive it; when it answers, “Received” goes up.'),
      h('div', { class: 'row' },
        h('button', { class: 'btn primary', onclick: async () => { try { await app.api.call('messages.send', { conv: 'ch:public', text: 'Radio test from ' + ((app.store.get('info') || {}).name || 'EmberMesh') + ' via ' + radioName() }); toast('Test queued — watch the radio transmit', { kind: 'ok' }); } catch (e) { toastError(e, 'Not sent'); } } }, icon('send'), 'Send a test'),
        pingSel, h('button', { class: 'btn', onclick: async () => { if (!pingSel.value) return pingSel.focus(); try { await app.api.call('mesh.ping', { node_id: pingSel.value }); toast('Ping sent'); } catch (e) { toastError(e); } } }, icon('ping'), 'Ping')),
      h('div', { class: 'card' }, out),
      h('div', { class: 'row end' }, h('button', { class: 'btn primary', onclick: stepDone }, 'Finish', icon('next'))));
    app.on('mesh.pong', d => toast('Pong from ' + String(d && d.node_id).slice(0, 8) + ' — the radio link works', { kind: 'ok' }));
    refresh();
  }

  function stepDone() {
    stop();
    const chapter = st.radio && /uv-?5r/i.test(st.radio.id + st.radio.model) ? 'embermesh/baofeng-uv5r' : manualFor(st.method.type, st.method.kindsOk[0]);
    append(clear(page), head('Done'),
      h('div', { class: 'card stack' }, h('p', null, radioName() + ' is connected. It appears under Links & radios, where you can change its settings and see the level meter.'),
        h('a', { href: '#/help/read/' + chapter.split('/').map(encodeURIComponent).join('/') }, icon('book', { size: 16 }), ' Read the manual chapter for this setup')),
      h('div', { class: 'row end' }, h('a', { class: 'btn primary', href: '#/links' }, 'Go to Links')));
  }

  const radioName = () => st.radio ? (st.radio.maker && st.radio.maker !== 'Generic' ? st.radio.maker + ' ' : '') + String(st.radio.model).replace(/\s*\(.*\)$/, '') : 'Radio';

  await stepRadio();
  return () => stop();
}
