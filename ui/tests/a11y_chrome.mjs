// Headless-Chrome plumbing for the accessibility audit (a11y_audit.mjs): static server for ui/,
// Chrome over the DevTools protocol, page helpers. Node built-ins only (Node ≥ 22).
import http from 'node:http';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';

export const UI = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
export const sleep = ms => new Promise(r => setTimeout(r, ms));

export function findChrome(explicit) {
  return explicit || process.env.CHROME || [
    '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
    '/Applications/Chromium.app/Contents/MacOS/Chromium',
    '/usr/bin/google-chrome', '/usr/bin/google-chrome-stable', '/usr/bin/chromium', '/usr/bin/chromium-browser',
  ].find(p => fs.existsSync(p));
}

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.svg': 'image/svg+xml', '.png': 'image/png' };

export function serve(root = UI) {
  return new Promise(res => {
    const srv = http.createServer((req, resp) => {
      const u = new URL(req.url, 'http://x');
      let p = path.normalize(path.join(root, decodeURIComponent(u.pathname)));
      if (!p.startsWith(root)) { resp.writeHead(403); return resp.end(); }
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

export async function launch(chrome) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ember-a11y-'));
  const proc = spawn(chrome, ['--headless=new', '--remote-debugging-port=0', '--user-data-dir=' + dir, '--no-first-run', '--no-default-browser-check', '--disable-gpu', '--hide-scrollbars', 'about:blank'], { stdio: 'ignore' });
  const portFile = path.join(dir, 'DevToolsActivePort');
  for (let i = 0; i < 150 && !fs.existsSync(portFile); i++) await sleep(100);
  if (!fs.existsSync(portFile)) throw new Error('Chrome did not start');
  // The file can exist before Chrome has written the port into it: re-read it until it is numeric.
  let page;
  for (let i = 0; i < 200 && !page; i++) {
    const port = (fs.readFileSync(portFile, 'utf8').split('\n')[0] || '').trim();
    if (/^\d+$/.test(port)) try { page = (await (await fetch(`http://127.0.0.1:${port}/json/list`)).json()).find(t => t.type === 'page'); } catch { /* not yet */ }
    if (!page) await sleep(100);
  }
  if (!page) { proc.kill(); throw new Error('Chrome did not start (no DevTools page after 20 s)'); }
  const c = await cdp(page.webSocketDebuggerUrl);
  return {
    c, proc, dir,
    close() { try { c.close(); } catch { /* ignore */ } proc.kill(); try { fs.rmSync(dir, { recursive: true, force: true }); } catch { /* ignore */ } },
  };
}

export function cdp(wsUrl) {
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
      send: (method, params = {}) => new Promise((r, j) => { const i = ++id; pending.set(i, { res: r, rej: j }); ws.send(JSON.stringify({ id: i, method, params })); }),
      on: fn => listeners.push(fn),
      close: () => ws.close(),
    });
  });
}

/** Evaluate an expression (string) in the page, awaiting promises, returning by value. */
export async function evaluate(c, expr) {
  const r = await c.send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true });
  if (r.exceptionDetails) throw new Error('page: ' + (r.exceptionDetails.exception?.description || r.exceptionDetails.text));
  return r.result.value;
}

/**
 * Bring up the accessibility modules on the loaded page. Until app.js calls initPrefs/initA11y/
 * initSimple itself (phase 2), the audit starts them; afterwards this is a no-op (idempotent).
 */
export const INIT_A11Y = `(async () => {
  for (let i = 0; i < 60 && !window.__ember; i++) await new Promise(r => setTimeout(r, 50));
  const base = new URL('js/lib/', location.href).href;
  const P = await import(base + 'prefs.js');
  const A = await import(base + 'a11y.js');
  const S = await import(base + 'simple.js');
  if (window.__ember) { await P.initPrefs(window.__ember.backend); A.initA11y(window.__ember); S.initSimple(window.__ember); }
  window.__emberPrefs = P;
  return true;
})()`;

export const setPrefs = obj => `(async () => { const P = window.__emberPrefs; for (const [k, v] of Object.entries(${JSON.stringify(obj)})) await P.setPref(k, v); await new Promise(r => setTimeout(r, 350)); return P.getPrefs().effective; })()`;

export async function shot(c, file) {
  const s = await c.send('Page.captureScreenshot', { format: 'png' });
  fs.writeFileSync(file, Buffer.from(s.data, 'base64'));
}
