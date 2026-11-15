#!/usr/bin/env node
// Headless-Chrome smoke test for the UI.
// Mock mode (default): serves ui/ on a random local port, opens every main screen at phone and
// desktop sizes against ?mock=1, fails (exit 1) on any console error, uncaught exception, failed
// request or CSP violation.
// Real mode (--real URL | --real auto): runs against the real WebAssembly core with a fresh
// browser profile: first-run setup, every screen at both sizes, send a Public message, post a
// bulletin, SOS start/stop, add a loopback link, open a guide found by search, run a tool, set a
// passphrase → lock → unlock → remove it, reload and verify everything persisted, then a duress
// unlock must look like a normal one (no first-run screen, profile kept).
//   --real auto   serve ui/ + the WASM build (EMBER_WASM_DIR, else dist/web/wasm, else
//                 build/web/wasm) + the guide pack (EMBER_GUIDES_PACK, else dist/web/data/guides.pack,
//                 else build/guides.pack) — tests the current ui/ sources against the real core
//   --real URL    an already running server, e.g. dist/web served by `python3 -m http.server`
//
//   node ui/tests/smoke_chrome.mjs [--shots DIR] [--chrome PATH] [--only route,route] [--real URL|auto]
//
// Not picked up by `node --test` (file name does not match the test patterns) because it needs a
// Chrome binary; CI can run it explicitly. Uses only Node built-ins (http, fs, child_process,
// global WebSocket from Node ≥ 22).
import http from 'node:http';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const UI = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const args = process.argv.slice(2);
const opt = k => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : null; };
const SHOTS = opt('--shots');
const ONLY = opt('--only');
const REAL = opt('--real');
const ROOT = path.resolve(UI, '..');
const CHROME = opt('--chrome') || process.env.CHROME || [
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  '/usr/bin/google-chrome', '/usr/bin/chromium', '/usr/bin/chromium-browser',
].find(p => fs.existsSync(p));

export const ROUTES = [
  ['chats', '#/chats'], ['thread-public', '#/chats/ch%3Apublic'], ['thread-dm', '#/chats/dm%3Ab7e4c1d2a9f03e56'], ['thread-io', '#/chats/io%3Aaprs'],
  ['people', '#/people'], ['contact', '#/people/contact/b7e4c1d2a9f03e56'], ['peers', '#/people/peers'], ['radar', '#/people/radar'], ['stats', '#/people/stats'],
  ['sos', '#/sos'], ['board', '#/board'], ['links', '#/links'], ['links-add', '#/links/add'], ['links-add-audio', '#/links/add?kind=audio'], ['links-add-ble', '#/links/add?kind=ble'],
  ['guides', '#/guides'], ['guides-search', '#/guides?q=water'], ['guides-cat', '#/guides/cat/radio'], ['guide-book', '#/guides/book/radio/handheld-basics'],
  ['guide-read', '#/guides/read/radio/handheld-basics-1'], ['guide-sampler', '#/guides/read/reference/markdown-sampler'],
  ['tools', '#/tools'], ['tool-linkbudget', '#/tools/run/link_budget'], ['tool-dipole', '#/tools/run/dipole'], ['bands', '#/tools/bands'], ['morse', '#/tools/morse'], ['stations', '#/tools/stations'],
  ['extras', '#/extras'], ['notes', '#/extras/notes'], ['checklists', '#/extras/checklists'], ['inventory', '#/extras/inventory'],
  ['settings', '#/settings'], ['more', '#/more'],
  ['help', '#/help'], ['help-hardware', '#/help/hardware'], ['help-read', '#/help/read/embermesh/quick-start'], ['links-radio', '#/links/radio'],
  ['morse-camera', '#/tools/morse?tab=camera'], ['morse-sound', '#/tools/morse?tab=sound'],
];
const SPECIAL = [
  ['setup', '?mock=1&fresh=1', '#/chats'],
  ['lock', '?mock=1&locked=1', '#/chats'],
  ['degraded', '?mock=1&unsupported=guides.categories,tools.list,files.send,host.hub.status,links.list,bulletins.list,sos.list,messages.search', '#/guides'],
];
import { MORSE_SENT, MORSE_OK, CAPTURE_HOOK, cameraWpm, SOUND_TWIN, CAMERA_TWIN, cameraTwinProblems, TWIN_TIMEOUT_MS, TWIN_PROGRESS } from './capture_hook.mjs';
// Steps that need real-time media (live camera light, live tone) run only when the host passes the
// real-time probe (docs/CI.md); otherwise they are reported as SKIPPED with the measured numbers.
// Their deterministic twins (sample / frame time) always run.
import { REALTIME_PROBE, verdict, probeLine, LIMITS } from './realtime_probe.mjs';
export { MORSE_SENT, MORSE_OK, CAPTURE_HOOK, cameraWpm };

/**
 * Runs a deterministic twin (capture_hook.mjs) to completion: no step deadline, only a safety
 * bound that reports "host too slow to finish" with how far the twin got.
 */
async function runTwin(c, expr) {
  try {
    const r = await c.send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true }, TWIN_TIMEOUT_MS);
    if (r.exceptionDetails) throw new Error('twin failed: ' + (r.exceptionDetails.exception?.description || r.exceptionDetails.text));
    return r.result.value;
  } catch (e) {
    if (!/timed out/.test(e.message)) throw e;
    const pr = await c.send('Runtime.evaluate', { expression: TWIN_PROGRESS, returnByValue: true }).catch(() => null);
    throw new Error(`host too slow to finish the twin within ${TWIN_TIMEOUT_MS / 60000} min (progress ${pr && pr.result ? pr.result.value : '?'})`);
  }
}

/** Runs the real-time probe in the page (ev = the walk's evaluate) and prints its line. */
async function probe(ev, where, parts) {
  const p = await ev(REALTIME_PROBE(parts));
  console.log(probeLine(where, p));
  return p;
}

const SIZES = [['phone', 390, 844, 3, true], ['desktop', 1280, 800, 1, false]];

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.wasm': 'application/wasm', '.svg': 'image/svg+xml', '.png': 'image/png' };

const firstExisting = list => list.filter(Boolean).find(p => fs.existsSync(p)) || null;
const WASM_DIR = () => { const f = firstExisting([process.env.EMBER_WASM_DIR, path.join(ROOT, 'dist/web/wasm'), path.join(ROOT, 'build/web/wasm')].filter(Boolean).map(d => path.join(d, 'ember.wasm'))); return f && path.dirname(f); };
const GUIDES = () => firstExisting([process.env.EMBER_GUIDES_PACK, path.join(ROOT, 'dist/web/data/guides.pack'), path.join(ROOT, 'build/guides.pack')]);

/** Static server for ui/; with `real`, /wasm/* and /data/guides.pack come from the build. */
function serve(real) {
  const wasm = real ? WASM_DIR() : null;
  const pack = real ? GUIDES() : null;
  if (real && !wasm) throw new Error('no WASM build found (set EMBER_WASM_DIR or run ./build.sh web)');
  return new Promise(res => {
    const srv = http.createServer((req, resp) => {
      const u = new URL(req.url, 'http://x');
      if (real && u.pathname === '/wasm/build.json') { resp.writeHead(200, { 'content-type': 'application/json', 'cache-control': 'no-store' }); return resp.end(JSON.stringify({ version: '0.0.0', build: 'smoke', guides: pack ? 'data/guides.pack' : null })); }
      if (real && u.pathname === '/hub.json') { resp.writeHead(200, { 'content-type': 'application/json' }); return resp.end('{"embermesh_hub":false}'); }
      let p = path.normalize(path.join(UI, decodeURIComponent(u.pathname)));
      if (real && u.pathname.startsWith('/wasm/')) p = path.join(wasm, path.basename(u.pathname));
      else if (real && u.pathname === '/data/guides.pack' && pack) p = pack;
      else if (!p.startsWith(UI)) { resp.writeHead(403); return resp.end(); }
      if (fs.existsSync(p) && fs.statSync(p).isDirectory()) p = path.join(p, 'index.html');
      fs.readFile(p, (err, data) => {
        if (err) { resp.writeHead(404); return resp.end('not found'); }
        resp.writeHead(200, { 'content-type': MIME[path.extname(p)] || 'application/octet-stream', 'cache-control': 'no-store' });
        resp.end(data);
      });
    });
    srv.listen(0, '127.0.0.1', () => res(srv));
  });
}

async function launch() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ember-smoke-'));
  const proc = spawn(CHROME, ['--headless=new', '--remote-debugging-port=0', '--user-data-dir=' + dir, '--no-first-run', '--no-default-browser-check', '--disable-gpu', '--hide-scrollbars', '--autoplay-policy=no-user-gesture-required',
    // timers, rAF and the capture/audio graph must not be throttled mid-walk (Morse reader steps)
    '--disable-background-timer-throttling', '--disable-renderer-backgrounding', '--disable-backgrounding-occluded-windows', 'about:blank'], { stdio: 'ignore' });
  // Chrome creates DevToolsActivePort before it has written the port into it, and the endpoint can
  // lag behind the file: wait for a numeric port and a page target (up to 20 s).
  const portFile = path.join(dir, 'DevToolsActivePort');
  let page = null;
  for (let i = 0; i < 200 && !page; i++) {
    await sleep(100);
    let port = '';
    try { port = fs.readFileSync(portFile, 'utf8').split('\n')[0].trim(); } catch { continue; }
    if (!/^\d+$/.test(port)) continue;
    try { page = (await (await fetch(`http://127.0.0.1:${port}/json/list`)).json()).find(t => t.type === 'page') || null; } catch { /* not listening yet */ }
  }
  if (!page) { proc.kill(); throw new Error('Chrome did not start (no DevTools page after 20 s)'); }
  return { proc, dir, wsUrl: page.webSocketDebuggerUrl };
}

const sleep = ms => new Promise(r => setTimeout(r, ms));

function cdp(wsUrl) {
  const ws = new WebSocket(wsUrl);
  let id = 0;
  const pending = new Map();
  const listeners = [];
  ws.onmessage = ev => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) { const p = pending.get(m.id); pending.delete(m.id); m.error ? p.rej(new Error(m.error.message)) : p.res(m.result); }
    else if (m.method) for (const l of listeners) l(m.method, m.params);
  };
  return new Promise(res => {
    ws.onopen = () => res({
      // every DevTools call fails after 60 s instead of hanging the walk (and CI) forever
      send: (method, params = {}, ms = 60000) => new Promise((r, j) => {
        const i = ++id;
        const t = setTimeout(() => { if (pending.delete(i)) j(new Error(`${method} timed out after ${ms / 1000} s`)); }, ms);
        pending.set(i, { res: v => { clearTimeout(t); r(v); }, rej: e => { clearTimeout(t); j(e); } });
        ws.send(JSON.stringify({ id: i, method, params }));
      }),
      on: fn => listeners.push(fn),
      close: () => ws.close(),
    });
  });
}

async function main() {
  if (!CHROME) { console.log('SKIP: no Chrome found (set CHROME=/path/to/chrome)'); return; }
  if (REAL) return realMain();
  const srv = await serve(false);
  const base = `http://127.0.0.1:${srv.address().port}/index.html`;
  const { proc, dir, wsUrl } = await launch();
  const c = await cdp(wsUrl);
  const problems = [];
  const skipped = [];
  let current = '';
  c.on((method, p) => {
    if (method === 'Runtime.exceptionThrown') problems.push(`[${current}] exception: ${p.exceptionDetails.exception?.description || p.exceptionDetails.text}`);
    if (method === 'Runtime.consoleAPICalled' && (p.type === 'error' || p.type === 'assert')) problems.push(`[${current}] console.${p.type}: ${p.args.map(a => a.value ?? a.description ?? '').join(' ')}`);
    if (method === 'Log.entryAdded' && p.entry.level === 'error') problems.push(`[${current}] log: ${p.entry.text} ${p.entry.url || ''}`);
  });
  await c.send('Runtime.enable');
  await c.send('Log.enable');
  await c.send('Page.enable');
  if (SHOTS) fs.mkdirSync(SHOTS, { recursive: true });
  // what this host can do in real time, measured once at the start (re-checked before each step)
  await c.send('Page.navigate', { url: base + '?mock=1#/tools' });
  await sleep(1500);
  await probe(async (expr) => (await c.send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true })).result.value, 'mock walk start');

  const visit = async (name, query, hash, size) => {
    const [sname, w, hgt, scale, mobile] = size;
    current = name + '@' + sname;
    await c.send('Emulation.setDeviceMetricsOverride', { width: w, height: hgt, deviceScaleFactor: SHOTS ? Math.min(scale, 2) : 1, mobile });
    await c.send('Page.navigate', { url: base + query + hash });
    await sleep(1800);
    // detect horizontal overflow on the page (phones must never scroll sideways)
    const r = await c.send('Runtime.evaluate', { expression: 'JSON.stringify({sw: document.documentElement.scrollWidth, cw: document.documentElement.clientWidth, main: !!document.querySelector("#main") && document.querySelector("#main").textContent.length})', returnByValue: true });
    const v = JSON.parse(r.result.value);
    if (v.sw > v.cw + 1) problems.push(`[${current}] horizontal overflow: scrollWidth ${v.sw} > ${v.cw}`);
    if (!v.main) problems.push(`[${current}] empty main`);
    if (SHOTS) {
      const shot = await c.send('Page.captureScreenshot', { format: 'png' });
      fs.writeFileSync(path.join(SHOTS, `${name}-${sname}.png`), Buffer.from(shot.data, 'base64'));
    }
  };

  const only = ONLY ? new Set(ONLY.split(',')) : null;
  for (const size of SIZES) {
    // first load establishes mock state; reload per route keeps each screen independent
    for (const [name, hash] of ROUTES) if (!only || only.has(name)) await visit(name, '?mock=1', hash, size);
    for (const [name, q, hash] of SPECIAL) if (!only || only.has(name)) await visit(name, q, hash, size);
  }
  // themes
  for (const theme of ['light', 'night']) {
    if (only && !only.has('theme-' + theme)) continue;
    await c.send('Page.navigate', { url: base + '?mock=1#/chats' });
    await sleep(1200);
    await c.send('Runtime.evaluate', { expression: `window.__ember.saveSettings({theme:'${theme}'})`, awaitPromise: true });
    for (const [name, hash] of [['chats', '#/chats/ch%3Apublic'], ['sos', '#/sos'], ['guide', '#/guides/read/reference/markdown-sampler']]) await visit(`theme-${theme}-${name}`, '?mock=1', hash, SIZES[0]);
    await c.send('Runtime.evaluate', { expression: `window.__ember.saveSettings({theme:'dark'})`, awaitPromise: true });
  }
  // interactions: send a DM and watch it progress + get a reply; run a tool; search guides; SOS hold
  const ev = async (expr, wait = 0) => { const r = await c.send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true }); if (wait) await sleep(wait); return r.result.value; };
  // Wait for a condition / element instead of guessing with fixed sleeps (screens and dialogs
  // render asynchronously; animations never gate them, but data loads do).
  const waitFor = async (expr, ms = 8000) => { const end = Date.now() + ms; while (Date.now() < end) { if (await ev(expr)) return true; await sleep(100); } return false; };
  const waitSel = (sel, ms) => waitFor(`!!document.querySelector(${JSON.stringify(sel)})`, ms);
  const open = async (hash, sel, q = '?mock=1') => { await c.send('Page.navigate', { url: base + q + hash }); await sleep(300); if (!await waitSel(sel)) problems.push(`[${current}] ${sel} never appeared`); };
  if (!only || only.has('interact')) {
    await c.send('Emulation.setDeviceMetricsOverride', { width: 390, height: 844, deviceScaleFactor: 1, mobile: true });
    current = 'interact-send';
    await open('#/chats/dm%3Ae8a2b5c4d3f10967', '.compose textarea');
    await ev(`(() => { const ta = document.querySelector('.compose textarea'); ta.value = 'Smoke test hello'; ta.dispatchEvent(new Event('input')); document.querySelector('.compose .btn.primary').click(); })()`, 7000);
    const st = await ev(`(() => { const m = [...document.querySelectorAll('.msg.out')].find(e => e.textContent.includes('Smoke test hello')); return m ? (m.querySelector('[class^="status-"]') || {}).className || 'no-status' : 'missing'; })()`);
    if (!/status-(read|delivered)/.test(st)) problems.push('[interact-send] outgoing message status: ' + st);
    const hint = await ev(`(document.querySelector('.compose .hint') || {}).textContent || ''`);
    if (!/Encrypted/.test(hint)) problems.push('[interact-send] compose hint missing: ' + hint);
    const replies = await ev(`document.querySelectorAll('.msg.in').length`);
    if (!(replies > 0)) problems.push('[interact-send] no incoming reply rendered');
    if (SHOTS) fs.writeFileSync(path.join(SHOTS, 'interact-send-phone.png'), Buffer.from((await c.send('Page.captureScreenshot', { format: 'png' })).data, 'base64'));

    current = 'interact-tool';
    await open('#/tools/run/dipole', '.schema-form + .btn.primary, .card .btn.primary.block');
    await ev(`document.querySelector('.schema-form + .btn.primary, .card .btn.primary.block').click()`);
    await waitFor(`((document.querySelector('pre.out') || {}).textContent || '').trim().length > 0`);
    const out = await ev(`(document.querySelector('pre.out') || {}).textContent || ''`);
    if (!out.trim()) problems.push('[interact-tool] dipole produced no diagram');
    if (SHOTS) fs.writeFileSync(path.join(SHOTS, 'interact-tool-phone.png'), Buffer.from((await c.send('Page.captureScreenshot', { format: 'png' })).data, 'base64'));

    current = 'interact-search';
    // typed the moment the box appears, while the library may still be loading: still searched
    await c.send('Page.navigate', { url: base + '?mock=1#/guides' });
    if (!await waitFor(`(() => { const i = document.querySelector('#main input[aria-label="Search guides"]'); if (!i) return false; i.value = 'radio'; i.dispatchEvent(new Event('input')); return true; })()`, 8000)) problems.push(`[${current}] no search box`);
    const loadedWhenTyped = await ev(`!!document.querySelector('.cat-grid a')`);
    console.log(`interact-search: typed ${loadedWhenTyped ? 'after' : 'before'} the library had loaded`);
    await waitFor(`document.querySelectorAll('.snippet mark, .title mark').length > 0`, 15000);
    const marks = await ev(`document.querySelectorAll('.snippet mark, .title mark').length`);
    if (!(marks > 0)) problems.push(`[interact-search] no highlighted results (typed ${loadedWhenTyped ? 'after' : 'before'} the library had loaded)`);

    current = 'interact-sos';
    await open('#/sos', '.sos-hold');
    await ev(`(() => { const b = document.querySelector('.sos-hold'); b.dispatchEvent(new KeyboardEvent('keydown', { key: ' ', bubbles: true })); })()`, 3600);
    const active = await ev(`!!document.querySelector('.sos-banner:not(.incoming)')`);
    if (!active) problems.push('[interact-sos] SOS did not activate after a 3 s hold');
    if (SHOTS) fs.writeFileSync(path.join(SHOTS, 'interact-sos-phone.png'), Buffer.from((await c.send('Page.captureScreenshot', { format: 'png' })).data, 'base64'));
    await ev(`window.__ember.api.call('sos.stop')`);

    current = 'interact-meter';
    await open('#/links', '.meter');
    const meterExpr = `(() => { const m = document.querySelector('.meter'); if (!m) return 'no meter'; const i = m.querySelector('.meter-bar > i'); const w = i && (i.style.transform || i.style.width); return m.hidden ? 'hidden' : (w ? 'ok' : 'no level'); })()`;
    await waitFor(meterExpr + " === 'ok'", 15000);
    const meter = await ev(meterExpr);
    if (meter !== 'ok') problems.push('[interact-meter] audio link level meter: ' + meter);

    current = 'interact-actions';
    await open('#/settings', 'input[data-action="start_on_boot"]');
    const acts = await ev(`document.querySelectorAll('[data-action]').length`);
    if (!(acts >= 3)) problems.push('[interact-actions] host.info.actions not rendered: ' + acts);
    await ev(`document.querySelector('input[data-action="start_on_boot"]').click()`);
    if (!await waitFor(`(async () => (await window.__ember.api.call('host.info')).actions.find(a => a.id === 'start_on_boot').checked)()`)) problems.push('[interact-actions] switch did not call its method');

    // deterministic twins of the live reader steps below: always run, any host speed. The camera
    // twin: a 4 px keyed light and a steady lamp in a 1280×720 picture, found without a tap and
    // read; with only the lamp, nothing is locked or read.
    for (const [name, expr, keyed] of [['interact-morse-camera-frametime', CAMERA_TWIN(MORSE_SENT, 12), true], ['interact-morse-camera-nolight-frametime', CAMERA_TWIN(MORSE_SENT, 12, { keyed: false }), false]]) {
      current = name;
      await open('#/tools', '#main');
      try { for (const p of cameraTwinProblems(await runTwin(c, expr), { keyed })) problems.push(`[${current}] ${p}`); } catch (e) { problems.push(`[${current}] ${e.message}`); }
    }
    current = 'interact-morse-sound-sampletime';
    await open('#/tools', '#main');
    const snd = await runTwin(c, SOUND_TWIN(MORSE_SENT, 12)).catch(e => { problems.push(`[${current}] ${e.message}`); return null; });
    if (snd && !MORSE_OK.test(snd.transcript)) problems.push(`[${current}] transcript ${JSON.stringify(snd && snd.transcript)} (expected it to end with "CQ TEST")`);
    // the screen with only a steady lamp in the picture: keeps looking, reads nothing (no real time needed)
    current = 'interact-morse-camera-nolight';
    await open('#/tools/morse?tab=camera', '.morse-start');
    await ev(CAPTURE_HOOK(MORSE_SENT, 12, { keyed: false }));
    await ev(`document.querySelector('.morse-start').click()`);
    if (!await waitSel('.morse-transcript[data-live]', 8000)) problems.push(`[${current}] reader never reported live frames`);
    await sleep(6000);
    const idle = await ev(`({ state: (document.querySelector('.reader-state-text') || {}).textContent, text: (document.querySelector('.morse-transcript') || {}).textContent, ring: !(document.querySelector('.reader-ring') || {}).hidden })`);
    if (idle.state !== 'Looking for a blinking light…' || idle.text || idle.ring) problems.push(`[${current}] with no keyed light: ${JSON.stringify(idle)}`);
    await ev(`document.querySelector('.morse-start').click()`);
    for (const tab of ['camera', 'sound']) {
      current = 'interact-morse-' + tab;
      await open('#/tools/morse?tab=' + tab, '.morse-transcript');
      const why = verdict(await probe(ev, current, [tab === 'camera' ? 'camera' : 'audio']))[tab === 'camera' ? 'camera' : 'audio'];
      if (why) { skipped.push(`[${current}] ${why}`); continue; }
      await ev(CAPTURE_HOOK(MORSE_SENT, 12));
      await ev(`document.querySelector('.morse-start').click()`);
      if (!await waitSel('.morse-transcript[data-live]', 8000)) problems.push(`[${current}] reader never reported live samples`);
      await sleep(2000); // measure the light's real frame rate before choosing the speed
      const fps = tab === 'camera' ? await ev('window.__emberCaptureHook.fps()') : 0;
      if (tab === 'camera' && fps < LIMITS.camera_fps) {
        // the reader's own page paints the light slower than the probe saw: same probe, same skip
        skipped.push(`[${current}] the test camera painted ${fps} fps while reading, a camera reader needs ${LIMITS.camera_fps}`);
        await ev(`[...document.querySelectorAll('button')].find(b => /Stop/.test(b.textContent))?.click()`);
        continue;
      }
      const wpm = tab === 'camera' ? cameraWpm(fps) : 12;
      const keyMs = await ev(`window.__emberCaptureHook.go(${wpm})`);
      await waitFor(`new RegExp(${JSON.stringify(MORSE_OK.source)}).test(((document.querySelector('.morse-transcript') || {}).textContent || '').trim())`, keyMs + 12000);
      const tr = await ev(`((document.querySelector('.morse-transcript') || {}).textContent || '').trim()`);
      if (!MORSE_OK.test(tr)) problems.push(`[interact-morse-${tab}] transcript ${JSON.stringify(tr)} (expected it to end with "CQ TEST")`);
      if (tab === 'camera') {
        // found by itself (no tap): the state line and the ring on the light
        const st = await ev(`({ state: (document.querySelector('.reader-state-text') || {}).textContent, ring: !(document.querySelector('.reader-ring') || {}).hidden })`);
        if (!/Reading the light|Lost the light/.test(st.state || '')) problems.push(`[${current}] state line ${JSON.stringify(st.state)} after reading`);
      }
      if (SHOTS) fs.writeFileSync(path.join(SHOTS, `interact-morse-${tab}-phone.png`), Buffer.from((await c.send('Page.captureScreenshot', { format: 'png' })).data, 'base64'));
    }

    // Morse → Send (RPC_API §10): the four torch states of the mock host (?torch=yes|no|refused).
    const sendQ = (torch) => `?mock=1&noboot=1&torch=${torch}`;
    const sendText = (v) => ev(`(() => { const t = document.querySelector('#morse-send-text'); t.value = ${JSON.stringify(v)}; t.dispatchEvent(new Event('input')); return true; })()`);
    const txt = (sel) => ev(`((document.querySelector(${JSON.stringify(sel)}) || {}).textContent || '').trim()`);
    const btnShown = (label) => ev(`[...document.querySelectorAll('.morse-send button')].some(b => b.textContent.trim() === ${JSON.stringify(label)} && !b.hidden && b.offsetParent !== null)`);
    current = 'interact-morse-send-torch';
    await open('#/tools/morse?tab=send', '.morse-go', sendQ('yes'));
    if (!/^Flash with flashlight$/.test(await txt('.morse-go'))) problems.push(`[${current}] primary button: ${JSON.stringify(await txt('.morse-go'))}`);
    if (!await btnShown('Use the screen instead')) problems.push(`[${current}] "Use the screen instead" missing`);
    await sendText('Hé');
    if (!await waitFor(`[...document.querySelectorAll('.morse-send .notice')].some(n => n.textContent.includes('Morse has no sign for “É”'))`, 3000)) problems.push(`[${current}] no notice for an unsendable character`);
    const mch = await ev(`document.querySelectorAll('.morse-preview .mch').length`);
    if (mch !== 1) problems.push(`[${current}] preview characters for "Hé": ${mch} (expected 1)`);
    await sendText('CQ CQ TEST');
    await ev(`document.querySelector('.morse-go').click()`);
    if (!await waitFor(`(() => { const s = document.querySelector('.morse-sending'); return !!s && !s.hidden && !!document.querySelector('.morse-preview .mch.now'); })()`, 5000)) problems.push(`[${current}] sending state / current character not shown`);
    // Stop mid-message. (Pressed in the first moments, before the host has started the flash, the
    // stop is lost and the message runs to its end: reported against screens/morse_send.js.)
    await sleep(500);
    await ev(`document.querySelector('.morse-stop').click()`);
    if (!await waitFor(`((document.querySelector('.morse-result') || {}).textContent || '').trim() === 'Stopped.'`, 2000)) problems.push(`[${current}] after Stop: ${JSON.stringify(await txt('.morse-result'))}`);

    current = 'interact-morse-send-screen';
    await open('#/tools/morse?tab=send', '.morse-go', sendQ('no'));
    if (!/^Flash with screen$/.test(await txt('.morse-go'))) problems.push(`[${current}] primary button: ${JSON.stringify(await txt('.morse-go'))}`);
    if (await btnShown('Use the screen instead')) problems.push(`[${current}] "Use the screen instead" shown without a torch`);
    await sendText('E');
    await ev(`document.querySelectorAll('.morse-send .segmented [role=radio]')[2].click()`);
    await ev(`document.querySelector('.morse-go').click()`);
    if (!await waitFor(`((document.querySelector('.sheet h2') || {}).textContent || '').includes('The whole screen will flash')`, 3000)) problems.push(`[${current}] no warning before flashing the screen`);
    await ev(`[...document.querySelectorAll('.sheet button')].find(b => b.textContent.trim() === 'Flash the screen').click()`);
    if (!await waitFor(`((document.querySelector('.morse-sending-title') || {}).textContent || '').includes('at 7 words per minute')`, 3000)) problems.push(`[${current}] screen speed not capped: ${JSON.stringify(await txt('.morse-sending-title'))}`);
    if (!await waitFor(`((document.querySelector('.morse-result') || {}).textContent || '').trim() === 'Sent with the screen.'`, 8000)) problems.push(`[${current}] result: ${JSON.stringify(await txt('.morse-result'))}`);

    current = 'interact-morse-send-refused';
    await open('#/tools/morse?tab=send', '.morse-go', sendQ('refused'));
    await ev(`document.querySelector('.morse-go').click()`);
    if (!await waitFor(`/camera permission/.test((document.querySelector('.morse-result') || {}).textContent || '')`, 5000)) problems.push(`[${current}] result: ${JSON.stringify(await txt('.morse-result'))}`);

    current = 'interact-smart-search';
    await open('#/guides', 'input[type=search]');
    if (!await waitSel('.smart-switch input:checked', 15000)) problems.push('[smart] switch missing or off');
    await ev(`(() => { const i = document.querySelector('input[type=search]'); i.value = 'hemorrhage'; i.dispatchEvent(new Event('input')); })()`);
    await waitFor(`!!document.querySelector('#main .list a.item .by-meaning')`, 15000);
    const m1 = await ev(`(() => { const a = document.querySelector('#main .list a.item'); return a ? (a.getAttribute('href') + '|' + !!a.querySelector('.by-meaning') + '|' + !!a.querySelector('.passage-heading')) : 'none'; })()`);
    if (!/bleeding-control[^|]*\|true\|true/.test(m1)) problems.push('[smart] meaning-only query: ' + m1);
    await ev(`(() => { const i = document.querySelector('input[type=search]'); i.value = 'tourniquet'; i.dispatchEvent(new Event('input')); })()`);
    await waitFor(`(() => { const a = document.querySelector('#main .list a.item'); return !!a && !a.querySelector('.by-meaning'); })()`, 15000);
    const m2 = await ev(`(() => { const a = document.querySelector('#main .list a.item'); return a ? (a.getAttribute('href') + '|' + !!a.querySelector('.by-meaning')) : 'none'; })()`);
    if (!/bleeding-control[^|]*\|false/.test(m2)) problems.push('[smart] exact-term query: ' + m2);
    current = 'interact-ask';
    await open('#/sos', '.ask-shortcuts button');
    await ev(`[...document.querySelectorAll('.ask-shortcuts button')].find(b => /Not breathing/.test(b.textContent)).click()`);
    await waitFor(`/Rescue Breathing/.test((document.querySelector('.ask-passage') || document.querySelector('.ask-out') || {}).textContent || '')`, 15000);
    const ask = await ev(`(() => { const f = document.querySelector('.ask-passage'); return f ? f.textContent : (document.querySelector('.ask-out') || {}).textContent || 'none'; })()`);
    if (!/Rescue Breathing/.test(ask)) problems.push('[ask] emergency shortcut: ' + ask.slice(0, 120));
    if (SHOTS) fs.writeFileSync(path.join(SHOTS, 'interact-ask-phone.png'), Buffer.from((await c.send('Page.captureScreenshot', { format: 'png' })).data, 'base64'));
    current = 'interact-related';
    await open('#/guides/read/radio/handheld-basics-1', '.reader');
    if (!await waitFor(`document.querySelectorAll('.related a.item').length > 0`, 15000)) problems.push('[related] no related guides');
    current = 'interact-no-semantic';
    await open('#/guides', 'input[type=search]', '?mock=1&nosemantic=1');
    await sleep(500);
    if (await ev(`!!document.querySelector('.smart-switch, .ask-box')`)) problems.push('[no-semantic] smart controls shown without a semantic section');

    current = 'interact-radio';
    await open('#/links/radio', '#main .list .item');
    await ev(`document.querySelector('#main .list .item').click()`);
    await waitSel('[data-method=handheld_vox]');
    await ev(`document.querySelector('[data-method=handheld_vox]').click()`);
    if (!await waitSel('.hw-card', 15000)) problems.push('[interact-radio] no "You will need" step');
    await ev(`[...document.querySelectorAll('button')].find(b => /I have these/.test(b.textContent)).click()`);
    if (!await waitFor(`document.querySelectorAll('.radio-checklist li').length > 3`, 15000)) problems.push('[interact-radio] radio checklist missing');

    current = 'interact-link';
    await open('#/links/add?kind=ble', 'button');
    await waitFor(`[...document.querySelectorAll('button')].some(b => /Scan for devices/.test(b.textContent))`);
    await ev(`[...document.querySelectorAll('button')].find(b => /Scan for devices/.test(b.textContent)).click()`);
    await waitSel('.devices .item', 6000);
    await ev(`document.querySelector('.devices .item').click()`, 300);
    await ev(`(() => { const i = [...document.querySelectorAll('input')].find(x => x.closest('.field') && /call/i.test(x.closest('.field').textContent)); if (i) { i.value = 'N0CALL'; i.dispatchEvent(new Event('input')); } [...document.querySelectorAll('button')].find(b => b.textContent === 'Connect').click(); })()`);
    await waitFor(`/#\\/links$/.test(location.hash)`, 15000);
    if (!/#\/links$/.test(await ev('location.hash'))) problems.push('[interact-link] wizard did not finish: ' + await ev(`(document.querySelector('.field-error:not(:empty)') || {}).textContent`));
  }

  // XSS canary: the hostile markdown sampler must not create script/iframe/img/on* elements
  current = 'xss';
  await c.send('Page.navigate', { url: base + '?mock=1#/guides/read/reference/markdown-sampler' });
  await sleep(300);
  for (let i = 0; i < 80; i++) { const r = await c.send('Runtime.evaluate', { expression: '!!document.querySelector(".reader")', returnByValue: true }); if (r.result.value) break; await sleep(100); }
  const x = await c.send('Runtime.evaluate', { expression: `(() => { const r = document.querySelector('.reader'); if (!r) return 'no reader'; const bad = r.querySelectorAll('script,iframe,img,object,embed,svg,style,link,form,input'); const on = [...r.querySelectorAll('*')].filter(e => [...e.attributes].some(a => /^on/i.test(a.name) || (a.name==='href'))); return bad.length + on.length ? 'unsafe nodes: ' + bad.length + '/' + on.length : 'ok'; })()`, returnByValue: true });
  if (x.result.value !== 'ok') problems.push('[xss] ' + x.result.value);
  const pwned = await c.send('Runtime.evaluate', { expression: 'window.__pwned === undefined', returnByValue: true });
  if (!pwned.result.value) problems.push('[xss] window.__pwned was set');

  c.close();
  proc.kill();
  srv.close();
  try { fs.rmSync(dir, { recursive: true, force: true }); } catch { /* ignore */ }
  for (const sk of skipped) console.log('SKIP ' + sk);
  if (problems.length) {
    console.error('SMOKE FAILED:\n' + problems.join('\n'));
    process.exit(1);
  }
  console.log('smoke ok: ' + (ROUTES.length + SPECIAL.length) * SIZES.length + ' screen loads, no console errors; ' + `${skipped.length} step(s) SKIPPED (not passed)` + (skipped.length ? ': ' + skipped.map(x => x.split(']')[0].slice(1)).join(', ') : '') + (SHOTS ? ', screenshots in ' + SHOTS : ''));
  // ---- accessibility audit (owned by tests/a11y_audit.mjs; --quick subset, baseline compared)
  if (!ONLY && !process.env.EMBER_SKIP_A11Y) {
    const { spawnSync } = await import('node:child_process');
    const r = spawnSync(process.execPath, [path.join(UI, 'tests', 'a11y_audit.mjs'), '--quick', '--chrome', CHROME], { stdio: 'inherit' });
    if (r.status !== 0) { console.error('SMOKE FAILED: accessibility audit regressed (see above)'); process.exit(1); }
  }
}

// ------------------------------------------------------------------------------------ real core
export const REAL_ROUTES = [
  ['chats', '#/chats'], ['thread-public', '#/chats/ch%3Apublic'], ['people', '#/people'], ['peers', '#/people/peers'],
  ['radar', '#/people/radar'], ['stats', '#/people/stats'], ['sos', '#/sos'], ['board', '#/board'], ['links', '#/links'],
  ['links-add', '#/links/add'], ['links-add-loopback', '#/links/add?kind=loopback'], ['links-add-audio', '#/links/add?kind=audio'],
  ['guides', '#/guides'], ['guides-cat', '#/guides/cat/radio'], ['guide-manual', '#/guides/book/embermesh/manual'],
  ['guide-read', '#/guides/read/embermesh/quick-start'], ['guide-table', '#/guides/read/medical/ships-medicine-chest-patient-assessment'],
  ['tools', '#/tools'], ['tool-dipole', '#/tools/run/dipole'], ['bands', '#/tools/bands'], ['morse', '#/tools/morse'], ['stations', '#/tools/stations'],
  ['extras', '#/extras'], ['notes', '#/extras/notes'], ['checklists', '#/extras/checklists'], ['inventory', '#/extras/inventory'],
  ['settings', '#/settings'], ['more', '#/more'],
  ['help', '#/help'], ['help-hardware', '#/help/hardware'], ['links-radio', '#/links/radio'],
  ['morse-camera', '#/tools/morse?tab=camera'], ['morse-sound', '#/tools/morse?tab=sound'],
];

async function realMain() {
  const srv = REAL === 'auto' ? await serve(true) : null;
  const base = srv ? `http://127.0.0.1:${srv.address().port}/index.html` : new URL('index.html', REAL.endsWith('/') ? REAL : REAL + '/').href;
  const { proc, dir, wsUrl } = await launch();
  const c = await cdp(wsUrl);
  const problems = [];
  const steps = [];
  let current = 'boot';
  c.on((method, p) => {
    if (method === 'Runtime.exceptionThrown') problems.push(`[${current}] exception: ${p.exceptionDetails.exception?.description || p.exceptionDetails.text}`);
    if (method === 'Runtime.consoleAPICalled' && (p.type === 'error' || p.type === 'assert')) problems.push(`[${current}] console.${p.type}: ${p.args.map(a => a.value ?? a.description ?? '').join(' ')}`);
    if (method === 'Log.entryAdded' && p.entry.level === 'error') problems.push(`[${current}] log: ${p.entry.text} ${p.entry.url || ''}`);
  });
  await c.send('Runtime.enable');
  await c.send('Log.enable');
  await c.send('Page.enable');
  if (SHOTS) fs.mkdirSync(SHOTS, { recursive: true });
  const ev = async expr => {
    const r = await c.send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true });
    if (r.exceptionDetails) throw new Error(`[${current}] evaluate failed: ${r.exceptionDetails.exception?.description || r.exceptionDetails.text}`);
    return r.result.value;
  };
  const waitFor = async (expr, ms = 8000) => { const end = Date.now() + ms; while (Date.now() < end) { if (await ev(expr)) return true; await sleep(150); } return false; };
  const size = async ([, w, hgt, scale, mobile]) => c.send('Emulation.setDeviceMetricsOverride', { width: w, height: hgt, deviceScaleFactor: SHOTS ? Math.min(scale, 2) : 1, mobile });
  const shot = async name => { if (SHOTS) fs.writeFileSync(path.join(SHOTS, name + '.png'), Buffer.from((await c.send('Page.captureScreenshot', { format: 'png' })).data, 'base64')); };
  // fn returns false (failed), { skip: reason } (SKIPPED, never counted as ok) or anything else (ok)
  const skips = [];
  const step = async (name, fn) => {
    current = name;
    const before = problems.length;
    let skip = null;
    try { const ok = await fn(); if (ok === false) problems.push(`[${name}] failed`); else if (ok && ok.skip) skip = ok.skip; } catch (e) { problems.push(`[${name}] ${e.message}`); }
    const bad = await ev(`[...document.querySelectorAll('.toast.bad')].map(t => t.textContent).join(' | ')`).catch(() => '');
    if (bad) problems.push(`[${name}] error toast: ${bad}`);
    await ev(`document.querySelectorAll('.toast').forEach(t => t.remove())`).catch(() => {});
    if (problems.length === before && skip) { skips.push(name); steps.push(`SKIP ${name}: ${skip}`); return; }
    steps.push(`${problems.length === before ? 'ok  ' : 'FAIL'} ${name}`);
  };
  const go = async (hash, wait = 1500) => { await ev(`location.hash = ${JSON.stringify(hash)}`); await sleep(wait); };
  // click/type wait (up to 8 s) for their target, so a dialog or screen that is still rendering
  // is never read too early; they report a missing target instead of failing later on null.
  const findExpr = (sel, text) => `[...document.querySelectorAll(${JSON.stringify(sel)})].find(e => ${text ? `e.textContent.trim() === ${JSON.stringify(text)}` : 'true'})`;
  const waitSel = (sel, text, ms = 8000) => waitFor(`!!${findExpr(sel, text)}`, ms);
  const click = async (sel, text) => {
    if (!await waitSel(sel, text)) throw new Error(`no ${sel}${text ? ` "${text}"` : ''} to click`);
    return ev(`(() => { const b = ${findExpr(sel, text)}; if (!b) return false; b.click(); return true; })()`);
  };
  const type = async (sel, value) => {
    if (!await waitSel(sel)) throw new Error(`no ${sel} to type into`);
    return ev(`(() => { const i = document.querySelector(${JSON.stringify(sel)}); if (!i) return false; i.focus(); i.value = ${JSON.stringify(value)}; i.dispatchEvent(new Event('input', { bubbles: true })); return true; })()`);
  };
  const text = sel => `((document.querySelector(${JSON.stringify(sel)}) || {}).textContent || '')`;

  await size(SIZES[0]);
  await c.send('Page.navigate', { url: base + '#/chats' });
  await step('first-run setup', async () => {
    if (!await waitFor(`!!document.getElementById('setup-name')`, 20000)) throw new Error('setup screen not shown on a fresh node (node.info.name should be "")');
    await shot('real-setup-phone');
    await type('#setup-name', 'Smoke Tester');
    await click('.setup .btn.primary');
    if (!await waitFor(`/Your identity/.test(${text('#main')})`)) throw new Error('identity step missing');
    if (!await ev(`document.querySelectorAll('.setup .words span').length >= 6`)) throw new Error('fingerprint words missing');
    await click('.setup .btn.primary');
    return waitFor(`!!document.querySelector('.chat-list') && !document.getElementById('setup-name')`);
  });
  // what this host can do in real time, measured once at the start (re-checked before each step)
  await probe(ev, 'real walk start').catch(e => console.log('realtime probe (real walk start): failed: ' + e.message));

  for (const sz of SIZES) {
    await size(sz);
    for (const [name, hash] of REAL_ROUTES) {
      if (ONLY && !ONLY.split(',').includes(name)) continue;
      await step(`screen ${name}@${sz[0]}`, async () => {
        await go(hash, 1800);
        const v = JSON.parse(await ev(`JSON.stringify({sw: document.documentElement.scrollWidth, cw: document.documentElement.clientWidth, main: (document.querySelector('#main') || {}).textContent.length || 0, wrong: /Something went wrong|Could not load screen/.test((document.querySelector('#main') || {}).textContent || '')})`));
        if (v.sw > v.cw + 1) throw new Error(`horizontal overflow ${v.sw} > ${v.cw}`);
        if (!v.main || v.wrong) throw new Error('screen empty or failed');
        await shot(`real-${name}-${sz[0]}`);
      });
    }
  }
  await size(SIZES[0]);

  await step('send a Public message', async () => {
    await go('#/chats/ch%3Apublic');
    await type('.compose textarea', 'Real core smoke hello');
    await click('.compose .btn.primary');
    const ok = await waitFor(`[...document.querySelectorAll('.msg.out')].some(e => e.textContent.includes('Real core smoke hello'))`);
    await shot('real-sent-phone');
    return ok;
  });
  await step('post a bulletin', async () => {
    await go('#/board');
    await click('.page-head .btn.primary');
    await sleep(300);
    await type('.sheet input[type=text]', 'Smoke water point');
    await type('.sheet textarea', 'Clean water at the test bench');
    await click('.sheet .btn.primary', 'Post');
    return waitFor(`[...document.querySelectorAll('article.bulletin')].some(e => e.textContent.includes('Smoke water point'))`);
  });
  await step('SOS start and stop', async () => {
    await go('#/sos');
    await ev(`document.querySelector('.sos-hold').dispatchEvent(new KeyboardEvent('keydown', { key: ' ', bubbles: true }))`);
    await sleep(3600);
    if (!await waitFor(`!!document.querySelector('.sos-banner:not(.incoming)')`)) throw new Error('SOS did not activate');
    await shot('real-sos-active-phone');
    await click('#main .btn.block', 'I am safe — stop SOS');
    await sleep(300);
    await click('.sheet .btn.primary', 'Stop SOS');
    return waitFor(`!document.querySelector('.sos-banner:not(.incoming)') && !!document.querySelector('.sos-hold')`);
  });
  await step('add a loopback link', async () => {
    await go('#/links/add?kind=loopback', 2000);
    await type('.card input[maxlength="60"]', 'Smoke loopback');
    await click('button', 'Connect');
    if (!await waitFor(`location.hash === '#/links' && !!document.querySelector('.link-card')`)) throw new Error('wizard did not finish: ' + await ev(`(document.querySelector('.field-error:not(:empty)') || {}).textContent || ''`));
    await sleep(800);
    await shot('real-links-phone');
    return ev(`[...document.querySelectorAll('.link-card')].some(e => e.textContent.includes('Smoke loopback'))`);
  });
  await step('open a guide found by search', async () => {
    await go('#/guides', 0); // typed at once: the box searches even while the library loads
    await type('#main input[aria-label="Search guides"]', 'purify water');
    if (!await waitFor(`document.querySelectorAll('#main .list a.item').length > 0`)) throw new Error('no search results');
    await click('#main .list a.item');
    const ok = await waitFor(`!!document.querySelector('.reader') && document.querySelector('.reader').textContent.length > 200`);
    await shot('real-guide-phone');
    return ok;
  });
  await step('run a tool', async () => {
    await go('#/tools/run/dipole');
    await click('.card .btn.primary.block');
    return waitFor(`((document.querySelector('pre.out') || {}).textContent || '').trim().length > 0 || document.querySelectorAll('#main table.data td').length > 0`);
  });
  await step('set passphrase, lock, unlock, remove', async () => {
    await go('#/settings', 2000);
    await click('button', 'Set passphrase');
    await sleep(300);
    await ev(`(() => { const i = [...document.querySelectorAll('.sheet input[type=password]')]; i.forEach(x => { x.value = 'smoke-pass-1'; }); return i.length; })()`);
    await click('.sheet .btn.primary', 'Save');
    if (!await waitFor(`[...document.querySelectorAll('button')].some(b => b.textContent.trim() === 'Lock now')`)) throw new Error('passphrase not set');
    await click('button', 'Lock now');
    if (!await waitFor(`!!document.querySelector('.lock-screen')`)) throw new Error('lock screen not shown');
    await shot('real-locked-phone');
    await type('.lock-screen input[type=password]', 'wrong-pass');
    await click('.lock-screen button[type=submit]');
    if (!await waitFor(`/Wrong passphrase/.test(${text('.lock-screen .field-error')})`)) throw new Error('wrong passphrase not reported');
    await type('.lock-screen input[type=password]', 'smoke-pass-1');
    await click('.lock-screen button[type=submit]');
    if (!await waitFor(`!document.querySelector('.lock-screen') && !!document.querySelector('#nav:not([hidden])')`)) throw new Error('did not unlock');
    await go('#/settings', 2000);
    await click('button', 'Change passphrase');
    await sleep(300);
    await type('.sheet input[autocomplete=current-password]', 'smoke-pass-1');
    await click('.sheet .btn.primary', 'Save');
    await sleep(400);
    await click('.sheet .btn.danger', 'Remove');
    return waitFor(`[...document.querySelectorAll('button')].some(b => b.textContent.trim() === 'Set passphrase')`);
  });
  const morseRx = () => ev(`window.__ember.api.call('morse.rx.start', { source: 'light' }).then(r => window.__ember.api.call('morse.rx.stop', { session: r.session }).then(() => 'yes'), e => e.code)`);
  // deterministic twins of the live reader steps: always run, any host speed
  const frames = () => ev(`((window.__ember.store.get('info') || {}).features || []).includes('morse_frames')`);
  for (const [name, keyed] of [['read Morse by camera in frame time (4 px light + steady lamp, no tap)', true], ['camera in frame time, steady lamp only: nothing locked or read', false]]) {
    await step(name, async () => {
      if (await morseRx() === 'not_implemented') return { skip: 'this core has no morse.rx yet' };
      if (!await frames()) return { skip: 'this core has no light tracker (node.info.features lacks morse_frames)' };
      await go('#/tools', 800);
      const p = cameraTwinProblems(await runTwin(c, CAMERA_TWIN(MORSE_SENT, 12, { keyed })), { keyed });
      if (p.length) throw new Error(p.join('; '));
      return true;
    });
  }
  await step('read Morse by sound in sample time', async () => {
    if (await morseRx() === 'not_implemented') return { skip: 'this core has no morse.rx yet' };
    await go('#/tools', 800);
    const r = await runTwin(c, SOUND_TWIN(MORSE_SENT, 12));
    if (!MORSE_OK.test(r.transcript)) throw new Error('transcript ' + JSON.stringify(r.transcript));
    return true;
  });
  await step('Morse camera screen, steady lamp only: keeps looking, reads nothing', async () => {
    if (await morseRx() === 'not_implemented') return { skip: 'this core has no morse.rx yet' };
    await go('#/tools/morse?tab=camera', 1500);
    await ev(CAPTURE_HOOK(MORSE_SENT, 12, { keyed: false }));
    await click('button', 'Start reading');
    if (!await waitSel('.morse-transcript[data-live]', null, 8000)) throw new Error('reader never reported live frames');
    await sleep(6000);
    const idle = await ev(`({ state: ${text('.reader-state-text')}, text: ${text('.morse-transcript')}, ring: !(document.querySelector('.reader-ring') || {}).hidden })`);
    await click('button', 'Stop');
    if (idle.state !== 'Looking for a blinking light…' || idle.text || idle.ring) throw new Error('with no keyed light: ' + JSON.stringify(idle));
    return true;
  });
  for (const tab of ['camera', 'sound']) {
    await step('read Morse by ' + tab, async () => {
      if (await morseRx() === 'not_implemented') return { skip: 'this core has no morse.rx yet' };
      await go('#/tools/morse?tab=' + tab, 1500);
      const part = tab === 'camera' ? 'camera' : 'audio';
      const why = verdict(await probe(ev, 'real walk, read Morse by ' + tab, [part]))[part];
      if (why) return { skip: why };
      await ev(CAPTURE_HOOK(MORSE_SENT, 12));
      await click('button', 'Start reading');
      if (!await waitSel('.morse-transcript[data-live]', null, 8000)) throw new Error('reader never reported live samples');
      await sleep(2000); // measure the light's real frame rate before choosing the speed
      const fps = tab === 'camera' ? await ev('window.__emberCaptureHook.fps()') : 0;
      if (tab === 'camera' && fps < LIMITS.camera_fps) { await click('button', 'Stop'); return { skip: `the test camera painted ${fps} fps while reading, a camera reader needs ${LIMITS.camera_fps}` }; }
      const wpm = tab === 'camera' ? cameraWpm(fps) : 12;
      const keyMs = await ev(`window.__emberCaptureHook.go(${wpm})`);
      await waitFor(`new RegExp(${JSON.stringify(MORSE_OK.source)}).test(${text('.morse-transcript')}.trim())`, keyMs + 12000);
      const tr = (await ev(text('.morse-transcript'))).trim();
      await shot('real-morse-' + tab + '-phone');
      await click('button', 'Stop');
      if (!MORSE_OK.test(tr)) throw new Error('transcript ' + JSON.stringify(tr));
      return true;
    });
  }
  await step('connect a radio: up to the band step', async () => {
    await go('#/links/radio', 2000);
    await click('#main .list .item');
    await sleep(500);
    await click('[data-method=handheld_vox]');
    await sleep(500);
    await click('button', 'I have these');
    await sleep(300);
    await click('button', 'Next');
    await sleep(300);
    return waitFor(`/Band and licence/.test(${text('#main')})`, 15000);
  });
  await step('smart search, Ask and Related (or keyword search when the core lacks them)', async () => {
    const info = await ev(`window.__ember.api.call('guides.info').then(r => JSON.stringify(r.semantic || {}), e => e.code)`);
    await go('#/guides', 2000);
    await type('#main input[type=search]', 'tourniquet');
    if (!await waitFor(`[...document.querySelectorAll('#main .list a.item')].length > 0`)) throw new Error('exact-term query found nothing');
    if (info === 'not_implemented' || !/"available":true/.test(info)) {
      if (await ev(`!!document.querySelector('.smart-switch, .ask-box')`)) throw new Error('smart controls shown although guides.info says unavailable');
      steps.push('skip meaning query / Ask / Related: core ' + (info === 'not_implemented' ? 'has no guides.info yet' : 'pack has no semantic section'));
      return true;
    }
    await type('#main input[type=search]', 'my friend is bleeding a lot');
    if (!await waitFor(`!!document.querySelector('#main .list a.item')`)) throw new Error('meaning query found nothing');
    await go('#/sos', 1500);
    await click('.ask-shortcuts button', 'Not breathing');
    if (!await waitFor(`!!document.querySelector('.ask-passage, .ask-out .notice')`)) throw new Error('Ask showed nothing');
    await shot('real-ask-phone');
    await go('#/guides/read/medical/bleeding', 2500);
    if (!await waitFor(`document.querySelectorAll('.related a.item').length > 0`, 15000)) throw new Error('no related guides');
    return true;
  });
  await step('create a channel with a random key', async () => {
    await go('#/chats');
    await click('.page-head .btn.primary', 'New');
    await sleep(400);
    await click('.sheet button', 'Create channel');
    await sleep(400);
    await type('.sheet input[maxlength="40"]', 'Smoke camp');
    if (!await ev(`document.querySelector('.sheet input[name=chmode][value=random]').checked`)) throw new Error('random key is not the default');
    await click('.sheet .btn.primary', 'Create');
    return waitFor(`/^#\\/chats\\/ch%3A[0-9a-f]{8}$/.test(location.hash) && /Smoke camp/.test(${text('.thread-head')})`);
  });
  await step('reload keeps state', async () => {
    await sleep(1500); // storage.dirty → IndexedDB flush is debounced
    await c.send('Page.navigate', { url: base + '#/chats/ch%3Apublic' });
    if (!await waitFor(`[...document.querySelectorAll('.msg.out')].some(e => e.textContent.includes('Real core smoke hello'))`, 20000)) throw new Error('Public message lost after reload');
    const name = await ev(`(window.__ember.store.get('info') || {}).name`);
    if (name !== 'Smoke Tester') throw new Error('profile name lost: ' + name);
    await go('#/board');
    if (!await ev(`[...document.querySelectorAll('article.bulletin')].some(e => e.textContent.includes('Smoke water point'))`)) throw new Error('bulletin lost');
    await go('#/links', 2000);
    if (!await ev(`[...document.querySelectorAll('.link-card')].some(e => e.textContent.includes('Smoke loopback'))`)) throw new Error('link lost');
    await shot('real-after-reload-phone');
    return true;
  });

  await step('duress unlock looks normal (no first-run screen)', async () => {
    await go('#/settings', 2000);
    await click('button', 'Set passphrase');
    await sleep(300);
    await ev(`[...document.querySelectorAll('.sheet input[type=password]')].forEach(x => { x.value = 'smoke-pass-2'; })`);
    await click('.sheet .btn.primary', 'Save');
    if (!await waitFor(`[...document.querySelectorAll('button')].some(b => b.textContent.trim() === 'Set duress passphrase')`)) throw new Error('duress option missing');
    await click('button', 'Set duress passphrase');
    await sleep(300);
    await type('.sheet input[type=password]', 'smoke-duress-9');
    await click('.sheet .btn.primary', 'Save');
    await sleep(800);
    await click('button', 'Lock now');
    if (!await waitFor(`!!document.querySelector('.lock-screen')`)) throw new Error('lock screen not shown');
    await type('.lock-screen input[type=password]', 'smoke-duress-9');
    await click('.lock-screen button[type=submit]');
    if (!await waitFor(`!document.querySelector('.lock-screen') && !!document.querySelector('#nav:not([hidden])')`)) throw new Error('duress unlock did not look like an unlock');
    await sleep(1500);
    if (await ev(`!!document.getElementById('setup-name')`)) throw new Error('first-run setup shown after duress unlock');
    const name = await ev(`(window.__ember.store.get('info') || {}).name`);
    if (name !== 'Smoke Tester') throw new Error('profile not kept after duress unlock: ' + name);
    await shot('real-after-duress-phone');
    return true;
  });

  c.close();
  proc.kill();
  if (srv) srv.close();
  try { fs.rmSync(dir, { recursive: true, force: true }); } catch { /* ignore */ }
  console.log(steps.join('\n'));
  if (problems.length) { console.error('REAL SMOKE FAILED:\n' + problems.join('\n')); process.exit(1); }
  console.log('real-core smoke ok: ' + steps.filter(s => s.startsWith('ok')).length + ' steps passed, no console errors; ' + `${skips.length} step(s) SKIPPED (not passed)` + (skips.length ? ': ' + skips.join(', ') : '') + (SHOTS ? ', screenshots in ' + SHOTS : ''));
}

main().catch(e => { console.error(e); process.exit(1); });
