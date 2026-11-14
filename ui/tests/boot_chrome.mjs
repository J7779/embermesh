#!/usr/bin/env node
// Headless-Chrome test of the start-up animation (js/boot/): it appears on a normal load and leaves
// by itself, a click or a key skips it, the app is usable afterwards, ?noboot=1 and automation skip
// it, reduced motion gets the calm version, and nothing is left behind (no overlay, canvas,
// animation frames or timers from the boot module). Fails on console errors or CSP violations.
//   node ui/tests/boot_chrome.mjs [--chrome PATH]
// Needs Chrome/Chromium and Node ≥ 22; not picked up by `node --test` (like smoke_chrome.mjs).
import http from 'node:http';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const UI = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const args = process.argv.slice(2);
const CHROME = (args.includes('--chrome') && args[args.indexOf('--chrome') + 1]) || process.env.CHROME || [
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome', '/usr/bin/google-chrome', '/usr/bin/chromium', '/usr/bin/chromium-browser',
].find(p => fs.existsSync(p));
const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.svg': 'image/svg+xml' };
const sleep = ms => new Promise(r => setTimeout(r, ms));

function serve() {
  return new Promise(res => {
    const srv = http.createServer((req, resp) => {
      let p = path.normalize(path.join(UI, decodeURIComponent(new URL(req.url, 'http://x').pathname)));
      if (!p.startsWith(UI)) { resp.writeHead(403); return resp.end(); }
      if (fs.existsSync(p) && fs.statSync(p).isDirectory()) p = path.join(p, 'index.html');
      fs.readFile(p, (err, data) => {
        if (err) { resp.writeHead(404); return resp.end(); }
        resp.writeHead(200, { 'content-type': MIME[path.extname(p)] || 'application/octet-stream', 'cache-control': 'no-store' });
        resp.end(data);
      });
    });
    srv.listen(0, '127.0.0.1', () => res(srv));
  });
}

async function launch() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ember-boot-test-'));
  const proc = spawn(CHROME, ['--headless=new', '--remote-debugging-port=0', '--user-data-dir=' + dir, '--no-first-run', '--no-default-browser-check', '--hide-scrollbars', 'about:blank'], { stdio: 'ignore' });
  const portFile = path.join(dir, 'DevToolsActivePort');
  for (let i = 0; i < 150 && !fs.existsSync(portFile); i++) await sleep(100);
  const [port] = fs.readFileSync(portFile, 'utf8').split('\n');
  const page = (await (await fetch(`http://127.0.0.1:${port}/json/list`)).json()).find(t => t.type === 'page');
  const ws = new WebSocket(page.webSocketDebuggerUrl);
  let id = 0;
  const pending = new Map(), listeners = [];
  ws.onmessage = ev => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) { const p = pending.get(m.id); pending.delete(m.id); m.error ? p.rej(new Error(m.error.message)) : p.res(m.result); }
    else if (m.method) for (const l of listeners) l(m.method, m.params);
  };
  await new Promise(r => { ws.onopen = r; });
  return {
    send: (method, params = {}) => new Promise((r, j) => { const i = ++id; pending.set(i, { res: r, rej: j }); ws.send(JSON.stringify({ id: i, method, params })); }),
    on: fn => listeners.push(fn),
    close() { ws.close(); proc.kill('SIGKILL'); setTimeout(() => fs.rmSync(dir, { recursive: true, force: true }), 300); },
  };
}

// Counts requestAnimationFrame / setTimeout use by the boot module (by call stack) so the test can
// prove nothing keeps running after the hand-over.
const PROBE = `(() => {
  const fromBoot = () => /\\/js\\/boot\\//.test(new Error().stack || '');
  const P = window.__probe = { raf: 0, rafAfter: 0, timers: new Set(), done: 0 };
  const raf = window.requestAnimationFrame.bind(window), st = window.setTimeout.bind(window), ct = window.clearTimeout.bind(window);
  window.requestAnimationFrame = cb => { if (fromBoot()) { P.raf++; if (P.done) P.rafAfter++; } return raf(cb); };
  window.setTimeout = (cb, ms, ...a) => { const boot = fromBoot(); const id = st((...x) => { P.timers.delete(id); cb(...x); }, ms, ...a); if (boot) P.timers.add(id); return id; };
  window.clearTimeout = id => { P.timers.delete(id); return ct(id); };
  document.addEventListener('ember:bootdone', () => { P.done = performance.now(); });
})()`;
const NOT_AUTOMATED = `Object.defineProperty(Navigator.prototype, 'webdriver', { get: () => false, configurable: true });`;

const results = [];
const check = (name, ok, detail = '') => { results.push([name, ok, detail]); console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${detail ? '  — ' + detail : ''}`); };

async function main() {
  if (!CHROME) { console.log('SKIP: no Chrome found (set CHROME=/path/to/chrome)'); return; }
  const srv = await serve();
  const base = `http://127.0.0.1:${srv.address().port}/index.html`;
  const c = await launch();
  const problems = [];
  c.on((m, p) => {
    if (m === 'Runtime.exceptionThrown') problems.push('exception: ' + (p.exceptionDetails.exception?.description || p.exceptionDetails.text));
    if (m === 'Runtime.consoleAPICalled' && (p.type === 'error' || p.type === 'warning')) problems.push(`console.${p.type}: ` + p.args.map(a => a.value ?? a.description).join(' '));
    if (m === 'Log.entryAdded' && p.entry.level === 'error') problems.push('log: ' + p.entry.text);
  });
  await c.send('Runtime.enable'); await c.send('Log.enable'); await c.send('Page.enable');
  await c.send('Emulation.setDeviceMetricsOverride', { width: 390, height: 844, deviceScaleFactor: 2, mobile: true });
  const ev = async expr => { const r = await c.send('Runtime.evaluate', { expression: expr, returnByValue: true, awaitPromise: true }); if (r.exceptionDetails) throw new Error(r.exceptionDetails.text); return r.result.value; };
  let scripts = [];
  const setInit = async list => {
    for (const s of scripts) await c.send('Page.removeScriptToEvaluateOnNewDocument', { identifier: s });
    scripts = [];
    for (const src of list) scripts.push((await c.send('Page.addScriptToEvaluateOnNewDocument', { source: src })).identifier);
  };
  const state = () => ev(`JSON.stringify({ overlay: !!document.getElementById('ember-boot'), status: !!document.getElementById('ember-boot-status'), canvas: document.querySelectorAll('#ember-boot canvas, canvas[aria-hidden]').length,
    boot: window.__emberBoot ? { variant: __emberBoot.variant, active: __emberBoot.active, reason: __emberBoot.reason } : null,
    probe: window.__probe ? { raf: __probe.raf, rafAfter: __probe.rafAfter, timers: __probe.timers.size, done: Math.round(__probe.done) } : null,
    app: !!document.querySelector('#main') && !document.getElementById('boot') })`).then(JSON.parse);
  let nav = 0;
  const load = async (query, waitMs) => { await c.send('Page.navigate', { url: base + query + '&n=' + ++nav + '#/chats' }); await sleep(waitMs); return state(); };
  const centre = async (type, extra = {}) => c.send('Input.dispatchMouseEvent', { type, x: 195, y: 422, button: 'left', clickCount: 1, ...extra });

  // 1. normal (non-automated) first launch: appears, plays, leaves by itself, cleans up
  await setInit([NOT_AUTOMATED, PROBE, `try { localStorage.removeItem('embermesh.ui.boot_seen'); } catch {}`]);
  let s = await load('?mock=1', 250);
  check('appears on a normal first launch (full variant, canvas running)', s.overlay && s.boot && s.boot.variant === 'full' && s.boot.active && s.canvas === 1, JSON.stringify(s.boot));
  check('live status announced once, outside the decorative overlay', await ev(`(() => { const st = document.getElementById('ember-boot-status'); return !!st && st.textContent === 'Starting EmberMesh…' && document.getElementById('ember-boot').getAttribute('aria-hidden') === 'true' && !document.getElementById('ember-boot').contains(st); })()`));
  check('focus is never inside the overlay', await ev(`!document.getElementById('ember-boot').contains(document.activeElement) && !document.querySelector('#ember-boot [tabindex], #ember-boot button, #ember-boot a')`));
  await sleep(3600);
  s = await state();
  check('disappears by itself (minimum time + app ready) within the cap', !s.overlay && !s.status && s.canvas === 0 && s.boot.reason === 'ready' && s.probe.done > 0 && s.probe.done < 3600, `reason ${s.boot.reason} done@${s.probe.done}ms`);
  await sleep(1200);
  s = await state();
  check('no animation frames or timers left running afterwards', s.probe.raf > 30 && s.probe.rafAfter === 0 && s.probe.timers === 0, JSON.stringify(s.probe));
  // the app takes input after the hand-over: the element at the centre is the app, and a tap navigates
  const hit = await ev(`(() => { const el = document.elementFromPoint(195, 422); return !!el && !!el.closest('#app'); })()`);
  await ev(`(() => { const a = [...document.querySelectorAll('#nav a, #nav button')].find(x => /people/i.test(x.textContent + (x.getAttribute('href') || ''))); if (!a) return false; const r = a.getBoundingClientRect(); window.__navAt = [r.left + r.width / 2, r.top + r.height / 2]; return true; })()`);
  const at = await ev('window.__navAt');
  if (at) { await c.send('Input.dispatchMouseEvent', { type: 'mousePressed', x: at[0], y: at[1], button: 'left', clickCount: 1 }); await c.send('Input.dispatchMouseEvent', { type: 'mouseReleased', x: at[0], y: at[1], button: 'left', clickCount: 1 }); await sleep(500); }
  check('never blocks the app after hand-over (hit-test and a real tap)', hit && (!at || /people/.test(await ev('location.hash'))), await ev('location.hash'));
  const seen = await ev(`localStorage.getItem('embermesh.ui.boot_seen')`);
  await setInit([NOT_AUTOMATED, PROBE]);
  s = await load('?mock=1', 200);
  check('first run recorded: next launch is the short version', seen === '1' && s.boot && s.boot.variant === 'short', `seen=${seen} variant=${s.boot && s.boot.variant}`);
  await sleep(2600);

  // 2. a click skips at once
  await setInit([NOT_AUTOMATED, PROBE]);
  await load('?mock=1&boot=full', 350);
  await centre('mousePressed'); await centre('mouseReleased');
  await sleep(450);
  s = await state();
  check('a click skips it immediately', !s.overlay && s.boot.reason === 'skip' && s.canvas === 0, JSON.stringify(s.boot));

  // 3. a key press skips at once
  await load('?mock=1&boot=full', 350);
  await c.send('Input.dispatchKeyEvent', { type: 'keyDown', key: 'Escape', code: 'Escape', windowsVirtualKeyCode: 27 });
  await c.send('Input.dispatchKeyEvent', { type: 'keyUp', key: 'Escape', code: 'Escape', windowsVirtualKeyCode: 27 });
  await sleep(450);
  s = await state();
  check('a key press skips it immediately', !s.overlay && s.boot.reason === 'skip', JSON.stringify(s.boot));
  await sleep(1200);
  s = await state();
  check('nothing left running after a skip', s.probe.rafAfter === 0 && s.probe.timers === 0, JSON.stringify(s.probe));

  // 4. reduced motion: calm version without canvas
  await c.send('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-reduced-motion', value: 'reduce' }] });
  s = await load('?mock=1', 200);
  check('reduced motion → calm version (no canvas)', s.boot && s.boot.variant === 'reduced' && s.canvas === 0 && s.overlay, JSON.stringify(s.boot));
  await sleep(1800);
  s = await state();
  check('calm version leaves by itself', !s.overlay && s.boot.reason === 'ready');
  await c.send('Emulation.setEmulatedMedia', { features: [] });

  // 4b. settings mirrored by lib/prefs.js (ember.ui.* JSON keys) take effect
  for (const [k, v, want] of [['boot_animation', '"off"', 'off'], ['boot_animation', '"short"', 'short'], ['motion', '"reduced"', 'reduced'], ['simple_mode', 'true', 'reduced']]) {
    await setInit([NOT_AUTOMATED, PROBE, `try { localStorage.removeItem('embermesh.ui.boot_seen'); for (const k of ['boot_animation','motion','simple_mode']) localStorage.removeItem('ember.ui.' + k); localStorage.setItem('ember.ui.${k}', '${v}'); } catch {}`]);
    s = await load('?mock=1', 60);
    check(`ember.ui.${k} = ${v} → ${want}`, s.boot && s.boot.variant === want, JSON.stringify(s.boot));
    await sleep(want === 'off' ? 300 : 2400);
  }
  await setInit([NOT_AUTOMATED, PROBE, `try { for (const k of ['boot_animation','motion','simple_mode']) localStorage.removeItem('ember.ui.' + k); } catch {}`]);
  await sleep(300);

  // 5. ?noboot=1 and real automation skip it entirely
  s = await load('?mock=1&noboot=1', 120);
  check('?noboot=1: absent', !s.overlay && !s.status && s.boot.variant === 'none' && s.probe.raf === 0, JSON.stringify(s));
  await setInit([PROBE]);
  const wd = await (async () => { await load('?mock=1', 120); return ev('navigator.webdriver === true'); })();
  s = await state();
  check('under automation (navigator.webdriver): absent', !wd || (!s.overlay && s.boot.variant === 'none'), `webdriver=${wd}`);

  // 6. a forced variant still works under automation (test hook), and so does the pause hook
  s = await load('?mock=1&boot=short&bootpause=500', 600);
  check('?boot=short&bootpause=… freezes a still', s.overlay && s.boot.variant === 'short' && s.boot.active, JSON.stringify(s.boot));

  check('no console errors, exceptions or CSP violations', problems.length === 0, problems.join(' | '));
  c.close(); srv.close();
  const failed = results.filter(r => !r[1]);
  console.log(`\n${results.length - failed.length}/${results.length} passed`);
  if (failed.length) process.exit(1);
}
main().catch(e => { console.error(e); process.exit(1); });
