#!/usr/bin/env node
// Accessibility audit of the shared UI in headless Chrome (mock backend). Zero dependencies.
//
//   node ui/tests/a11y_audit.mjs [--shots DIR] [--json FILE] [--only screen,screen] [--update-baseline]
//                                 [--chrome PATH] [--quick]
//
// For every main screen at 360 px and 1280 px it checks, with the DevTools Accessibility domain
// and the in-page checks of a11y_page_checks.js:
//   names      interactive accessibility-tree nodes without a name; fields without labels;
//              SVG/img/canvas without alternatives; clickable things that are not controls
//   structure  landmarks, h1 + heading order, section titles that are not headings, live
//              regions, positive tabindex
//   contrast   every visible text node, in dark / light / night × normal / high contrast
//   targets    touch targets under 44 px (48 px in Simple mode)
//   keyboard   Tab through the screen: focus visible on every stop, no traps; skip link first;
//              a sheet keeps focus, Escape closes it and focus returns; route change focuses the
//              heading and Back restores the previous control
//   reflow     sideways scrolling at 320 px; clipped text, overlapping controls and sideways
//              scrolling with text scale 1.6, Simple mode and high contrast (360 px + desktop)
//   colour     status shown by colour only; motion still running with data-motion="off"
// Counts per screen and rule are compared with tests/a11y_baseline.json: any increase fails
// (exit 1), so new problems are caught while the known ones are being fixed. --update-baseline
// rewrites it. Screenshots of every mode go to --shots DIR (keep it outside the repository).
import fs from 'node:fs';
import path from 'node:path';
import { UI, findChrome, serve, launch, evaluate, sleep, INIT_A11Y, setPrefs, shot } from './a11y_chrome.mjs';

const args = process.argv.slice(2);
const opt = k => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : null; };
const flag = k => args.includes(k);
const SHOTS = opt('--shots');
const JSON_OUT = opt('--json');
const ONLY = opt('--only') ? new Set(opt('--only').split(',')) : null;
const QUICK = flag('--quick');
const BASELINE = path.join(UI, 'tests', 'a11y_baseline.json');
const CHECKS = fs.readFileSync(path.join(UI, 'tests', 'a11y_page_checks.js'), 'utf8');

/** [name, query, hash, screen file] */
export const SCREENS = [
  ['chats', '?mock=1', '#/chats', 'screens/chats.js'],
  ['thread-public', '?mock=1', '#/chats/ch%3Apublic', 'screens/chats.js'],
  ['thread-dm', '?mock=1', '#/chats/dm%3Ab7e4c1d2a9f03e56', 'screens/chats.js'],
  ['people', '?mock=1', '#/people', 'screens/people.js'],
  ['contact', '?mock=1', '#/people/contact/b7e4c1d2a9f03e56', 'screens/people.js'],
  ['peers', '?mock=1', '#/people/peers', 'screens/people.js'],
  ['radar', '?mock=1', '#/people/radar', 'screens/people.js'],
  ['sos', '?mock=1', '#/sos', 'screens/sos.js'],
  ['board', '?mock=1', '#/board', 'screens/bulletins.js'],
  ['links', '?mock=1', '#/links', 'screens/links.js'],
  ['links-add', '?mock=1', '#/links/add', 'screens/links.js'],
  ['links-radio', '?mock=1', '#/links/radio', 'screens/radio.js'],
  ['guides', '?mock=1', '#/guides', 'screens/guides.js'],
  ['guides-search', '?mock=1', '#/guides?q=water', 'screens/guides.js'],
  ['guide-read', '?mock=1', '#/guides/read/radio/handheld-basics-1', 'screens/guides.js'],
  ['tools', '?mock=1', '#/tools', 'screens/tools.js'],
  ['tool-run', '?mock=1', '#/tools/run/link_budget', 'screens/tools.js'],
  ['morse', '?mock=1', '#/tools/morse', 'screens/morse.js'],
  ['extras', '?mock=1', '#/extras', 'screens/extras.js'],
  ['checklists', '?mock=1', '#/extras/checklists', 'screens/extras.js'],
  ['settings', '?mock=1', '#/settings', 'screens/settings.js'],
  ['accessibility', '?mock=1', '#/settings/accessibility', 'screens/accessibility.js'],
  ['more', '?mock=1', '#/more', 'screens/more.js'],
  ['help', '?mock=1', '#/help', 'screens/help.js'],
  ['help-hardware', '?mock=1', '#/help/hardware', 'screens/hardware.js'],
  ['setup', '?mock=1&fresh=1', '#/chats', 'screens/setup.js'],
  ['lock', '?mock=1&locked=1', '#/chats', 'screens/lock.js'],
];
const QUICK_SET = new Set(['chats', 'thread-public', 'sos', 'guides', 'settings', 'accessibility', 'more', 'links', 'setup']);
const MODES = [
  ['simple', { simple_mode: true }],
  ['text160', { text_scale: 1.6 }],
  ['contrast', { high_contrast: true }],
];
const DEFAULTS = { simple_mode: false, text_scale: 1, high_contrast: null, motion: 'system', reading_spacing: false };

async function main() {
  const chrome = findChrome(opt('--chrome'));
  if (!chrome) { console.log('SKIP: no Chrome found (set CHROME=/path/to/chrome)'); return; }
  const srv = await serve();
  const base = `http://127.0.0.1:${srv.address().port}/index.html`;
  const br = await launch(chrome);
  const { c } = br;
  const errors = [];
  let current = '';
  c.on((m, p) => {
    if (m === 'Runtime.exceptionThrown') errors.push(`[${current}] exception: ${p.exceptionDetails.exception?.description || p.exceptionDetails.text}`);
    if (m === 'Runtime.consoleAPICalled' && p.type === 'error') errors.push(`[${current}] console.error: ${p.args.map(a => a.value ?? a.description ?? '').join(' ')}`);
  });
  await c.send('Runtime.enable');
  await c.send('Page.enable');
  await c.send('DOM.enable');
  await c.send('Accessibility.enable');
  if (SHOTS) fs.mkdirSync(SHOTS, { recursive: true });

  const findings = []; // {screen, file, rule, sev, sel, msg, ctx}
  const add = (screen, file, ctx, list) => { for (const f of list || []) findings.push({ screen, file, ctx, ...f }); };
  const ev = expr => evaluate(c, expr);
  const metrics = (w, hh, mobile) => c.send('Emulation.setDeviceMetricsOverride', { width: w, height: hh, deviceScaleFactor: SHOTS ? 2 : 1, mobile });
  let n = 0;
  const load = async (q, hash) => {
    await c.send('Page.navigate', { url: base + q + '&a11y=' + (++n) + hash });
    await sleep(1400);
    await ev(INIT_A11Y);
    await ev(CHECKS);
    await ev(setPrefs(DEFAULTS));
    await sleep(300);
  };
  const tokens = {};

  const list = SCREENS.filter(s => (!ONLY || ONLY.has(s[0])) && (!QUICK || QUICK_SET.has(s[0])));
  for (const [name, q, hash, file] of list) {
    for (const [size, w, hh, mobile] of [['phone', 360, 780, true], ['desktop', 1280, 800, false]]) {
      current = name + '@' + size;
      await metrics(w, hh, mobile);
      await load(q, hash);
      const ctx = size;
      // ---- structure, names, targets, colour-only (default look)
      add(name, file, ctx, await ev('__a11yChecks.structure()'));
      add(name, file, ctx, await ev('__a11yChecks.names()'));
      add(name, file, ctx, await axNames(c));
      add(name, file, ctx, await ev('__a11yChecks.targets(44)'));
      add(name, file, ctx, await ev('__a11yChecks.colourOnly()'));
      add(name, file, ctx, await ev('__a11yChecks.overflow("default")'));
      if (size === 'desktop' || name === 'setup') add(name, file, ctx, await keyboard(c, ev));
      // ---- contrast in every theme × contrast (phone only: same CSS at both sizes)
      if (size === 'phone') {
        for (const theme of ['dark', 'light', 'night']) for (const hc of ['normal', 'high']) {
          await ev(`(() => { const r = document.documentElement; r.dataset.theme = '${theme}'; r.dataset.contrast = '${hc}'; return true; })()`);
          await sleep(40);
          const tag = theme + '/' + hc;
          add(name, file, tag, (await ev(`__a11yChecks.contrast(${JSON.stringify(tag)})`)).map(f => ({ ...f, sev: hc === 'high' && f.ratio < 7 && f.ratio >= 4.5 ? 'minor' : f.sev })));
          if (!tokens[tag]) tokens[tag] = await ev('__a11yChecks.tokens()');
        }
        await ev(`(() => { const r = document.documentElement; r.dataset.theme = 'dark'; window.__emberPrefs.setPref('high_contrast', null); return true; })()`);
        // reflow at 320 px
        await metrics(320, 640, true);
        await sleep(250);
        add(name, file, '320px', await ev('__a11yChecks.overflow("320px")'));
        await metrics(w, hh, mobile);
      }
      // ---- motion off: nothing may keep animating
      await ev(setPrefs({ motion: 'off' }));
      add(name, file, ctx, await ev('__a11yChecks.motion()'));
      await ev(setPrefs({ motion: 'system' }));
      if (SHOTS) await shot(c, path.join(SHOTS, `${name}-${size}-default.png`));
      // ---- Simple mode, text 1.6, high contrast
      for (const [mode, prefs] of MODES) {
        await ev(setPrefs({ ...DEFAULTS, ...prefs }));
        await sleep(250);
        const tag = mode + '@' + size;
        add(name, file, tag, await ev(`__a11yChecks.overflow(${JSON.stringify(tag)})`));
        if (mode !== 'contrast') add(name, file, tag, await ev(`__a11yChecks.clipped(${JSON.stringify(tag)})`));
        add(name, file, tag, await ev(`__a11yChecks.overlap(${JSON.stringify(tag)})`));
        if (mode === 'simple') add(name, file, tag, (await ev('__a11yChecks.targets(48)')).map(f => ({ ...f, rule: 'target-size-simple' })));
        if (SHOTS) await shot(c, path.join(SHOTS, `${name}-${size}-${mode}.png`));
      }
      await ev(setPrefs(DEFAULTS));
    }
  }
  // ---- flows: dialog focus, route focus + restore
  if (!ONLY || ONLY.has('flows')) {
    current = 'flows';
    await metrics(1280, 800, false);
    add('flows', 'lib/ui.js', 'dialog', await dialogFlow(c, ev, load));
    add('flows', 'lib/a11y.js', 'route', await routeFlow(c, ev, load));
  }

  br.close();
  srv.close();

  // ---- report
  const dedupe = new Map();
  for (const f of findings) {
    const k = [f.screen, f.rule, f.sel, f.msg.replace(/^[^:]+: /, '')].join('|');
    if (!dedupe.has(k)) dedupe.set(k, { ...f, contexts: [f.ctx] }); else dedupe.get(k).contexts.push(f.ctx);
  }
  const uniq = [...dedupe.values()];
  const counts = {};
  for (const f of uniq) { const k = f.screen + '|' + f.rule; counts[k] = (counts[k] || 0) + 1; }
  const bySev = {};
  for (const f of uniq) { const s = (bySev[f.screen] = bySev[f.screen] || { critical: 0, serious: 0, moderate: 0, minor: 0 }); s[f.sev]++; }
  console.log('accessibility audit: ' + uniq.length + ' distinct findings on ' + list.length + ' screens');
  console.log(['screen'.padEnd(16), 'crit', 'ser', 'mod', 'min'].join(' '));
  for (const [s, v] of Object.entries(bySev)) console.log([s.padEnd(16), String(v.critical).padStart(4), String(v.serious).padStart(3), String(v.moderate).padStart(3), String(v.minor).padStart(3)].join(' '));
  console.log('\ntheme token contrast (text pairs):');
  for (const [tag, ps] of Object.entries(tokens)) console.log('  ' + tag.padEnd(13) + ps.map(p => p.pair.replace(/--/g, '') + ' ' + p.ratio).join(' · '));
  if (JSON_OUT) fs.writeFileSync(JSON_OUT, JSON.stringify({ findings: uniq, counts, bySev, tokens, errors }, null, 1));
  if (errors.length) console.log('\npage errors:\n' + errors.slice(0, 20).join('\n'));

  if (flag('--update-baseline')) {
    const prev = fs.existsSync(BASELINE) ? JSON.parse(fs.readFileSync(BASELINE, 'utf8')) : { counts: {} };
    const merged = ONLY || QUICK ? { ...prev.counts, ...counts } : counts;
    fs.writeFileSync(BASELINE, JSON.stringify({ note: 'Known accessibility findings per screen|rule (node ui/tests/a11y_audit.mjs --update-baseline). Lower these as fixes land; the audit fails when a count grows.', counts: Object.fromEntries(Object.entries(merged).sort()) }, null, 1) + '\n');
    console.log('baseline written: ' + BASELINE);
    return;
  }
  const base0 = fs.existsSync(BASELINE) ? JSON.parse(fs.readFileSync(BASELINE, 'utf8')).counts : {};
  const worse = Object.entries(counts).filter(([k, v]) => v > (base0[k] || 0));
  const better = Object.entries(base0).filter(([k, v]) => (counts[k] || 0) < v && (!ONLY || ONLY.has(k.split('|')[0])) && (!QUICK || QUICK_SET.has(k.split('|')[0])));
  if (better.length) console.log('\nimproved (lower the baseline with --update-baseline): ' + better.map(([k, v]) => k + ' ' + v + '→' + (counts[k] || 0)).join(', '));
  if (worse.length || errors.length) {
    console.error('\nA11Y AUDIT FAILED — new findings beyond tests/a11y_baseline.json:');
    for (const [k, v] of worse) {
      console.error('  ' + k + ': ' + v + ' (baseline ' + (base0[k] || 0) + ')');
      for (const f of uniq.filter(f => f.screen + '|' + f.rule === k).slice(0, 4)) console.error('     ' + f.sev + ' ' + f.sel + ' — ' + f.msg);
    }
    process.exit(1);
  }
  console.log('\na11y audit ok (no regressions against the baseline)');
}

/** Interactive AX nodes without a name. */
async function axNames(c) {
  const out = [];
  const { nodes } = await c.send('Accessibility.getFullAXTree');
  const ROLES = new Set(['button', 'link', 'textbox', 'searchbox', 'combobox', 'checkbox', 'radio', 'slider', 'spinbutton', 'tab', 'menuitem', 'switch', 'listbox']);
  for (const nd of nodes) {
    if (nd.ignored || !nd.role || !ROLES.has(nd.role.value)) continue;
    const name = nd.name && String(nd.name.value || '').trim();
    if (name) continue;
    let s = nd.role.value;
    if (nd.backendDOMNodeId) {
      try {
        const { object } = await c.send('DOM.resolveNode', { backendNodeId: nd.backendDOMNodeId });
        const r = await c.send('Runtime.callFunctionOn', { objectId: object.objectId, functionDeclaration: 'function () { return window.__a11yChecks.sel(this.nodeType === 1 ? this : this.parentElement); }', returnByValue: true });
        s = r.result.value || s;
      } catch { /* node gone */ }
    }
    out.push({ rule: 'name', sev: 'critical', sel: s, msg: nd.role.value + ' without an accessible name' });
  }
  return out;
}

const key = (c, k, shift) => {
  const base = { key: k, code: k === 'Tab' ? 'Tab' : k === 'Escape' ? 'Escape' : k, windowsVirtualKeyCode: k === 'Tab' ? 9 : k === 'Escape' ? 27 : k === 'Enter' ? 13 : 0, modifiers: shift ? 8 : 0 };
  return c.send('Input.dispatchKeyEvent', { type: 'keyDown', ...base }).then(() => c.send('Input.dispatchKeyEvent', { type: 'keyUp', ...base }));
};

/** Tab through the page from the top. */
async function keyboard(c, ev) {
  const out = [];
  // Start sequential focus navigation at the top of the document.
  await ev('(() => { const s = document.createElement("span"); s.tabIndex = -1; s.id = "a11y-start"; document.body.insertBefore(s, document.body.firstChild); s.focus(); return true; })()');
  const stops = [];
  let stuck = 0;
  for (let i = 0; i < 160; i++) {
    await key(c, 'Tab');
    const f = await ev('__a11yChecks.focusInfo()');
    if (f.body) { if (stops.length) break; continue; }
    if (stops.length && stops[stops.length - 1].id === f.id) { if (++stuck >= 3) { out.push({ rule: 'keyboard-trap', sev: 'critical', sel: f.sel, msg: 'Tab does not move focus away from this control' }); break; } } else stuck = 0;
    if (stops.length > 2 && f.id === stops[0].id) break; // wrapped
    stops.push(f);
  }
  await ev('(() => { const s = document.getElementById("a11y-start"); if (s) s.remove(); return true; })()');
  if (stops.length && !stops[0].skip) out.push({ rule: 'skip-link', sev: 'moderate', sel: stops[0].sel, msg: 'First Tab stop is not a “Skip to content” link' });
  const seenNo = new Set();
  for (const f of stops) if (!f.visible && !f.skip && !seenNo.has(f.sel)) { seenNo.add(f.sel); out.push({ rule: 'focus-visible', sev: 'serious', sel: f.sel, msg: 'No visible focus indicator' }); }
  const navVisible = await ev('!!document.getElementById("nav") && !document.getElementById("nav").hidden');
  if (navVisible && stops.length >= 150 === false && !stops.some(s => s.inNav)) out.push({ rule: 'keyboard-trap', sev: 'critical', sel: stops.at(-1)?.sel || 'body', msg: 'Navigation never reached with Tab (' + stops.length + ' stops)' });
  return out;
}

async function dialogFlow(c, ev, load) {
  const out = [];
  await load('?mock=1', '#/chats/ch%3Apublic');
  await ev(`(async () => { for (let i = 0; i < 40 && !document.querySelector('[aria-label="Conversation options"]'); i++) await new Promise(r => setTimeout(r, 100)); return true; })()`);
  const opened = await ev(`(() => { const b = document.querySelector('[aria-label="Conversation options"]'); if (!b) return false; b.focus(); b.click(); return true; })()`);
  if (!opened) return [{ rule: 'dialog', sev: 'minor', sel: 'thread', msg: 'could not open the options sheet' }];
  await sleep(400);
  let inside = true;
  for (let i = 0; i < 25; i++) { await key(c, 'Tab'); if (!(await ev('__a11yChecks.focusInfo()')).inOverlay) inside = false; }
  for (let i = 0; i < 5; i++) { await key(c, 'Tab', true); if (!(await ev('__a11yChecks.focusInfo()')).inOverlay) inside = false; }
  if (!inside) out.push({ rule: 'dialog', sev: 'critical', sel: '.sheet', msg: 'Focus left the modal sheet with Tab' });
  const inert = await ev(`document.getElementById('app').hasAttribute('inert')`);
  if (!inert) out.push({ rule: 'dialog', sev: 'moderate', sel: '#app', msg: 'Page behind the sheet is not inert' });
  await key(c, 'Escape');
  await sleep(300);
  if (await ev(`!!document.querySelector('.overlay')`)) out.push({ rule: 'dialog', sev: 'serious', sel: '.sheet', msg: 'Escape does not close the sheet' });
  const back = await ev(`(document.activeElement && document.activeElement.getAttribute('aria-label')) || ''`);
  if (back !== 'Conversation options') out.push({ rule: 'dialog', sev: 'serious', sel: '.sheet', msg: 'Focus did not return to the button that opened the sheet (now: ' + back + ')' });
  return out;
}

async function routeFlow(c, ev, load) {
  const out = [];
  await load('?mock=1', '#/guides/cat/radio');
  await ev(`(async () => { for (let i = 0; i < 40 && !document.querySelector('#main a.item'); i++) await new Promise(r => setTimeout(r, 100)); return true; })()`);
  const href = await ev(`(() => { const a = document.querySelector('#main a.item'); if (!a) return ''; a.focus(); return a.getAttribute('href'); })()`);
  if (!href) return [{ rule: 'route-focus', sev: 'minor', sel: '#main', msg: 'no list item to follow' }];
  await key(c, 'Enter');
  await sleep(1800);
  const f1 = await ev(`(() => { const a = document.activeElement; return a ? a.tagName + '|' + (a.textContent || '').trim().slice(0, 40) : ''; })()`);
  if (!/^H1\|/.test(f1)) out.push({ rule: 'route-focus', sev: 'serious', sel: '#main h1', msg: 'After navigating, focus is not on the new screen’s heading (' + f1 + ')' });
  await ev('history.back()');
  await sleep(1800);
  const f2 = await ev(`(document.activeElement && document.activeElement.getAttribute('href')) || (document.activeElement && document.activeElement.tagName)`);
  if (f2 !== href) out.push({ rule: 'route-focus', sev: 'moderate', sel: href, msg: 'Back did not return focus to the item you came from (' + f2 + ')' });
  // announcer exists and the skip link is first in the body
  const ok = await ev(`!!document.querySelector('.a11y-live[aria-live="polite"]') && !!document.querySelector('.a11y-live[aria-live="assertive"]') && document.body.firstElementChild.classList.contains('skip-link')`);
  if (!ok) out.push({ rule: 'live', sev: 'serious', sel: 'body', msg: 'Announcer live regions or skip link missing' });
  return out;
}

main().catch(e => { console.error(e); process.exit(1); });
