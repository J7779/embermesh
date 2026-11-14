// Shared helpers for end-to-end tests that drive real browser guests (WASM nodes) in headless Chrome
// over the Chrome DevTools Protocol, with no npm dependencies (Node >= 22 built-in WebSocket).
// Used by apps/desktop/tests/hub_guests_chrome.mjs and tools/e2e/all_together.mjs.
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import net from 'node:net';

export const sleep = ms => new Promise(r => setTimeout(r, ms));

export async function freePort() {
  return new Promise(res => { const s = net.createServer(); s.listen(0, '127.0.0.1', () => { const p = s.address().port; s.close(() => res(p)); }); });
}

export async function waitFor(fn, ms, step = 200) {
  const end = Date.now() + ms;
  for (;;) {
    try { const v = await fn(); if (v) return v; } catch {}
    if (Date.now() > end) return null;
    await sleep(step);
  }
}

export function findChrome(explicit) {
  return explicit || ['/Applications/Google Chrome.app/Contents/MacOS/Google Chrome', '/usr/bin/google-chrome',
    '/usr/bin/chromium', '/usr/bin/chromium-browser'].find(p => fs.existsSync(p));
}

// Starts headless Chrome with its profile and log in `dir`, pushes the process onto `procs` (the
// caller kills it), and connects to its DevTools endpoint. Returns null if Chrome did not come up.
// The returned object: { ws, cdp, openPage(url), consoleLines (Map sessionId -> [error lines]) }.
export async function launchChrome({ chrome, dir, procs }) {
  const prof = path.join(dir, 'chrome');
  const clog = fs.openSync(path.join(dir, 'chrome.log'), 'w');
  const ch = spawn(chrome, ['--headless=new', '--remote-debugging-port=0', `--user-data-dir=${prof}`, '--no-first-run',
    '--no-default-browser-check', '--disable-gpu', '--disable-extensions', 'about:blank'], { stdio: ['ignore', clog, clog] });
  procs.push(ch);
  const portFile = path.join(prof, 'DevToolsActivePort');
  const devtools = await waitFor(() => fs.existsSync(portFile) && fs.readFileSync(portFile, 'utf8').split('\n'), 20000);
  if (!devtools) return null;
  const ws = new WebSocket(`ws://127.0.0.1:${devtools[0].trim()}${devtools[1].trim()}`);
  await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
  let seq = 0;
  const pending = new Map();
  const consoleLines = new Map();
  ws.onmessage = ev => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) { const p = pending.get(m.id); pending.delete(m.id); m.error ? p.rej(new Error(m.error.message)) : p.res(m.result); }
    if (m.method === 'Runtime.exceptionThrown' || (m.method === 'Runtime.consoleAPICalled' && m.params.type === 'error')) {
      const l = consoleLines.get(m.sessionId) || []; l.push(JSON.stringify(m.params).slice(0, 300)); consoleLines.set(m.sessionId, l);
    }
  };
  const cdp = (method, params = {}, sessionId) => new Promise((res, rej) => {
    const id = ++seq; pending.set(id, { res, rej }); ws.send(JSON.stringify({ id, method, params, sessionId }));
  });

  // Each page gets its own browser context = separate storage = a separate WASM node.
  async function openPage(url) {
    const { browserContextId } = await cdp('Target.createBrowserContext', { disposeOnDetach: true });
    const { targetId } = await cdp('Target.createTarget', { url: 'about:blank', browserContextId });
    const { sessionId } = await cdp('Target.attachToTarget', { targetId, flatten: true });
    await cdp('Runtime.enable', {}, sessionId);
    await cdp('Page.enable', {}, sessionId);
    await cdp('Page.navigate', { url }, sessionId);
    const ev = async (expr) => {
      const r = await cdp('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true }, sessionId);
      if (r.exceptionDetails) throw new Error(r.exceptionDetails.exception ? r.exceptionDetails.exception.description : r.exceptionDetails.text);
      return r.result.value;
    };
    return { sessionId, ev, call: (m, p = {}) => ev(`window.__ember.api.call(${JSON.stringify(m)}, ${JSON.stringify(p)})`) };
  }
  return { ws, cdp, openPage, consoleLines };
}

// Completes the first-run screens of a guest page through the DOM. `ok(cond, what)` records a check.
// Returns the guest's node.info, or null if the first-run screen never appeared.
export async function firstRun(page, name, ok, consoleLines) {
  if (!ok(await waitFor(() => page.ev(`!!document.getElementById('setup-name')`), 30000), `${name}: first-run screen shown (WASM core started)`)) {
    console.log('     page says: ' + JSON.stringify((await page.ev('document.body.innerText')).slice(0, 400)));
    console.log('     console: ' + ((consoleLines && consoleLines.get(page.sessionId)) || []).slice(0, 3).join(' | '));
    return null;
  }
  await page.ev(`(() => { const i = document.getElementById('setup-name'); i.value = ${JSON.stringify(name)}; i.dispatchEvent(new Event('input')); })()`);
  await page.ev(`[...document.querySelectorAll('button')].find(b => b.textContent.includes('Continue')).click()`);
  ok(await waitFor(() => page.ev(`!![...document.querySelectorAll('button')].find(b => b.textContent.includes('Start using EmberMesh'))`), 10000), `${name}: identity step`);
  await page.ev(`[...document.querySelectorAll('button')].find(b => b.textContent.includes('Start using EmberMesh')).click()`);
  const info = await waitFor(async () => { const i = await page.call('node.info'); return i.name === name && i; }, 10000);
  ok(!!info, `${name}: profile saved, node ${info && info.node_id}`);
  return info;
}
