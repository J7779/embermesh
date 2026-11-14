// Minimal headless-Chrome driver over the DevTools protocol. No dependencies: uses Node's built-in
// WebSocket client (Node >= 22) and child_process.
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

// Every Chrome we start is killed when this process exits, whatever the reason (test timeout,
// watchdog, crash, Ctrl-C): no orphaned browsers, and no test can wait on a browser that is gone.
const LIVE = new Set();
/** Kills Chrome's whole process group (helpers too: an orphaned audio/GPU helper keeps our stderr
 *  pipe open, and an open pipe keeps the test process alive after every test has finished). */
function killTree(p) {
  try { process.kill(-p.pid, 'SIGKILL'); } catch {}
  try { p.kill('SIGKILL'); } catch {}
  try { p.stderr && p.stderr.destroy(); } catch {}
}
export function killAllChrome() {
  for (const p of LIVE) killTree(p);
  LIVE.clear();
}
process.on('exit', killAllChrome);
for (const sig of ['SIGINT', 'SIGTERM', 'SIGHUP']) {
  process.once(sig, () => { killAllChrome(); process.exit(128 + 2); });
}

const SEND_TIMEOUT_MS = 30000;

export function findChrome() {
  const c = [
    process.env.CHROME,
    '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
    '/Applications/Chromium.app/Contents/MacOS/Chromium',
    '/usr/bin/google-chrome', '/usr/bin/google-chrome-stable', '/usr/bin/chromium', '/usr/bin/chromium-browser',
    'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
  ].filter(Boolean);
  return c.find((p) => fs.existsSync(p)) || null;
}

export async function launchChrome(chromePath, { extraArgs = [], muteAudio = true, userDataDir = null } = {}) {
  // userDataDir: reuse a profile (cold restart of the same browser profile); the caller deletes it
  const userDir = userDataDir || fs.mkdtempSync(path.join(os.tmpdir(), 'ember-chrome-'));
  const proc = spawn(chromePath, [
    '--headless=new', '--remote-debugging-port=0', `--user-data-dir=${userDir}`,
    '--no-first-run', '--no-default-browser-check', '--disable-background-networking',
    '--disable-component-update', '--disable-sync', '--metrics-recording-only', ...(muteAudio ? ['--mute-audio'] : []),
    ...extraArgs, 'about:blank',
  ], { stdio: ['ignore', 'ignore', 'pipe'], detached: process.platform !== 'win32' }); // own process group
  LIVE.add(proc);
  proc.once('exit', () => {
    LIVE.delete(proc);
    killTree(proc); // reap helpers that outlive the browser process; release the pipe
  });
  const wsUrl = await new Promise((resolve, reject) => {
    let buf = '';
    const t = setTimeout(() => reject(new Error('Chrome did not start')), 20000);
    proc.stderr.on('data', (d) => {
      buf = (buf + d).slice(-16384); // keep draining (a full pipe would block Chrome), keep only the tail
      const m = buf.match(/DevTools listening on (ws:\/\/\S+)/);
      if (m) { clearTimeout(t); resolve(m[1]); }
    });
    proc.on('exit', (code) => { clearTimeout(t); reject(new Error(`Chrome exited (${code}): ${buf.slice(-500)}`)); });
  }).catch((e) => { killTree(proc); throw e; });
  let cdp;
  try { cdp = await connect(wsUrl); } catch (e) { killTree(proc); throw e; }
  // If the browser dies, every pending and future CDP call fails at once instead of waiting.
  proc.once('exit', (code, sig) => cdp.fail(new Error(`Chrome exited (${code ?? sig})`)));
  return {
    cdp,
    proc,
    get alive() { return proc.exitCode === null && proc.signalCode === null; },
    async close() {
      try { await Promise.race([cdp.send('Browser.close'), new Promise((r) => setTimeout(r, 5000))]); } catch {}
      cdp.close();
      await new Promise((r) => { if (proc.exitCode !== null || proc.signalCode !== null) r(); else { proc.once('exit', r); setTimeout(() => { killTree(proc); r(); }, 5000); } });
      killTree(proc);
      if (!userDataDir) fs.rmSync(userDir, { recursive: true, force: true });
    },
  };
}

async function connect(url) {
  const ws = new WebSocket(url);
  await new Promise((res, rej) => { ws.onopen = res; ws.onerror = () => rej(new Error('CDP connect failed')); });
  let next = 1;
  const pending = new Map();
  const handlers = new Set();
  let dead = null;
  const fail = (err) => {
    if (!dead) dead = err;
    for (const [, p] of pending) { clearTimeout(p.timer); p.reject(err); }
    pending.clear();
  };
  ws.onclose = () => fail(new Error('CDP connection closed'));
  ws.onerror = () => fail(new Error('CDP connection error'));
  ws.onmessage = (ev) => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) {
      const p = pending.get(m.id); pending.delete(m.id);
      clearTimeout(p.timer);
      if (m.error) p.reject(new Error(`${p.method}: ${m.error.message}`)); else p.resolve(m.result);
    } else if (m.method) for (const h of handlers) h(m);
  };
  return {
    /** Every call settles: reply, error, connection loss, or timeout (default 30 s). */
    send(method, params = {}, sessionId, timeoutMs = SEND_TIMEOUT_MS) {
      if (dead) return Promise.reject(dead);
      const id = next++;
      return new Promise((resolve, reject) => {
        const timer = setTimeout(() => { pending.delete(id); reject(new Error(`${method}: no CDP reply within ${timeoutMs} ms`)); }, timeoutMs);
        pending.set(id, { resolve, reject, method, timer });
        try { ws.send(JSON.stringify({ id, method, params, sessionId })); } catch (e) { clearTimeout(timer); pending.delete(id); reject(e); }
      });
    },
    fail,
    onEvent(h) { handlers.add(h); return () => handlers.delete(h); },
    close() { try { ws.close(); } catch {} },
  };
}

/** Opens a tab, collects console errors / exceptions, and offers evaluate(). */
export async function openPage(cdp, url, { beforeNavigate, newWindow = false } = {}) {
  // newWindow: a page of its own that is never a background tab (background tabs get throttled timers)
  const { targetId } = await cdp.send('Target.createTarget', { url: 'about:blank', newWindow });
  const { sessionId } = await cdp.send('Target.attachToTarget', { targetId, flatten: true });
  const errors = [];
  const logs = [];
  const off = cdp.onEvent((m) => {
    if (m.sessionId !== sessionId) return;
    if (m.method === 'Runtime.exceptionThrown') {
      const d = m.params.exceptionDetails;
      errors.push(`exception: ${(d.exception && d.exception.description) || d.text}`);
    } else if (m.method === 'Runtime.consoleAPICalled') {
      const text = m.params.args.map((a) => a.value ?? a.description ?? '').join(' ');
      logs.push(`${m.params.type}: ${text}`);
      if (m.params.type === 'error' || m.params.type === 'assert') errors.push(`console.${m.params.type}: ${text}`);
    } else if (m.method === 'Log.entryAdded') {
      const e = m.params.entry;
      logs.push(`log.${e.level}: ${e.text} ${e.url || ''}`);
      if (e.level === 'error') errors.push(`log: ${e.text} ${e.url || ''}`);
    }
  });
  const s = (method, params, timeoutMs) => cdp.send(method, params, sessionId, timeoutMs);
  await s('Runtime.enable');
  await s('Log.enable');
  await s('Page.enable');
  await s('Network.enable');
  if (newWindow) await s('Emulation.setFocusEmulationEnabled', { enabled: true }).catch(() => {});
  if (beforeNavigate) await beforeNavigate(s);
  await s('Page.navigate', { url });
  return {
    errors, logs, send: s,
    async evaluate(expression, timeoutMs = 60000) {
      // Never hang a test run on a page promise that does not settle.
      let r;
      try { r = await s('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true }, timeoutMs); } catch (e) {
        throw new Error(`evaluate failed (${e.message}): ${expression.slice(0, 120)}`);
      }
      if (r.exceptionDetails) throw new Error(r.exceptionDetails.exception ? r.exceptionDetails.exception.description : r.exceptionDetails.text);
      return r.result.value;
    },
    async waitFor(expression, timeoutMs = 20000) {
      const t0 = Date.now();
      for (;;) {
        let v = null;
        let gone = false;
        try { v = await this.evaluate(expression, Math.max(1000, Math.min(15000, timeoutMs - (Date.now() - t0)))); } catch (e) { v = null; gone = /Chrome exited|CDP connection/.test(e.message); }
        if (gone) throw new Error(`browser gone while waiting for: ${expression.slice(0, 160)}`);
        if (v) return v;
        if (Date.now() - t0 > timeoutMs) throw new Error(`timeout waiting for: ${expression}\nerrors: ${errors.join('\n')}\nlogs: ${logs.slice(-20).join('\n')}`);
        await new Promise((r) => setTimeout(r, 150));
      }
    },
    async reload() { await s('Page.reload', { ignoreCache: false }); },
    async close() { off(); await cdp.send('Target.closeTarget', { targetId }, undefined, 5000).catch(() => {}); },
  };
}
