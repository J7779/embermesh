// Shared plumbing for the end-to-end browser tests (real headless Chrome + the real WASM core).
//
// Missing build output / Chrome is a skip, except with EMBER_REQUIRE_BROWSER=1, where every such
// condition is a hard failure (CI and "prove it" runs must never pass by skipping).
import { after } from 'node:test';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { haveDist, NO_DIST } from '../helpers.mjs';
import { findChrome, launchChrome, openPage, killAllChrome } from './cdp.mjs';

export const REQUIRE = process.env.EMBER_REQUIRE_BROWSER === '1';

// Whole-file watchdog: node --test runs each file in its own process. If anything still keeps that
// process alive past this deadline (a hook waiting on a dead browser, a server left listening), the
// file fails loudly instead of hanging the suite. unref(): it never keeps a finished file alive.
const WATCHDOG_MS = Number(process.env.EMBER_E2E_WATCHDOG_MS) || 10 * 60 * 1000;
setTimeout(() => {
  console.error(`e2e watchdog: ${path.basename(process.argv[1] || 'test')} still running after ${WATCHDOG_MS / 1000} s — killing browsers and failing`);
  killAllChrome();
  process.exit(1);
}, WATCHDOG_MS).unref();

if (process.env.EMBER_E2E_DEBUG) {
  setInterval(() => console.error('[e2e debug] active:', JSON.stringify(process.getActiveResourcesInfo())), 10000).unref();
}

// Last line of defence for every e2e file: when all its tests and hooks are done, no browser this
// process started may survive (an open DevTools socket would keep the file running until the watchdog).
after(() => { killAllChrome(); });

/** Runs every cleanup (newest first), each bounded by a timeout; never throws, never hangs. */
export async function runCleanups(list, eachMs = 15000) {
  const dbg = process.env.EMBER_E2E_DEBUG ? (...a) => console.error('[e2e debug]', ...a) : () => {};
  for (const c of list.splice(0).reverse()) {
    let t;
    dbg('cleanup start', String(c).slice(0, 60));
    try { await Promise.race([Promise.resolve().then(c), new Promise((r) => { t = setTimeout(r, eachMs); })]); } catch {} finally { clearTimeout(t); }
    dbg('cleanup done');
  }
  dbg('all cleanups done');
}

/** Default timeouts for hooks and tests in e2e files. */
export const HOOK = { timeout: 90000 };

/** Returns a skip reason (string) or false. Throws instead when EMBER_REQUIRE_BROWSER=1. */
export function e2eSkip(extra = []) {
  const chrome = findChrome();
  const reasons = [!haveDist() && NO_DIST, !chrome && 'Chrome not found (set CHROME=...)', ...extra].filter(Boolean);
  if (!reasons.length) return false;
  if (REQUIRE) throw new Error(`EMBER_REQUIRE_BROWSER=1 but: ${reasons.join('; ')}`);
  return reasons.join('; ');
}

/** Launches Chrome or returns {error}; with EMBER_REQUIRE_BROWSER=1 a failed launch throws. */
export async function startBrowser(extraArgs = [], opts = {}) {
  try {
    return await launchChrome(findChrome(), { extraArgs, ...opts });
  } catch (e) {
    if (REQUIRE) throw e;
    return { error: String(e.message || e).split('\n')[0] };
  }
}

// Errors that come from the browser environment, not from our code.
const IGNORABLE = [
  /favicon\.ico/,
  // the offline install / update check runs in the background; when a test cuts the network the
  // browser logs the interrupted request (the app retries later)
  /net::ERR_[A-Z_]+ \S+\?network=/,
];
export const realErrors = (errs) => errs.filter((e) => !IGNORABLE.some((r) => r.test(e)));

const READY = `(() => { const b = globalThis.__emberWasmBackend; return !!(b && b.ready && b.node); })()`;

/**
 * Opens a page running the real UI + WASM node and returns helpers:
 *   call(method, params)  backend.call in the page (rejects with the core's error code)
 *   events(name)          events seen since the page loaded (recorded from the backend)
 *   waitCall(m, p, pred)  polls call() until pred(result) is truthy
 */
export async function nodePage(browser, url, { timeoutMs = 30000, beforeNavigate, newWindow = false } = {}) {
  const page = await openPage(browser.cdp, url, { beforeNavigate, newWindow });
  await page.waitFor(READY, timeoutMs);
  await page.evaluate(`(() => { const b = globalThis.__emberWasmBackend; if (!globalThis.__ev) { globalThis.__ev = []; b.on('*', (e, d) => { if (globalThis.__ev.length < 5000) globalThis.__ev.push([e, d]); }); } return true; })()`);
  const call = async (method, params = {}) => {
    const r = await page.evaluate(`globalThis.__emberWasmBackend.call(${JSON.stringify(method)}, ${JSON.stringify(params)}).then(r => ({ok: true, r}), e => ({ok: false, code: e.code, message: e.message}))`);
    if (!r.ok) throw Object.assign(new Error(`${method}: ${r.code}: ${r.message}`), { code: r.code });
    return r.r;
  };
  const events = (name) => page.evaluate(`(globalThis.__ev || []).filter(x => x[0] === ${JSON.stringify(name)}).map(x => x[1])`);
  const waitCall = async (method, params, pred, ms = 20000, what = method) => {
    const t0 = Date.now();
    let last;
    for (;;) {
      try { last = await call(method, params); if (pred(last)) return last; } catch (e) { last = e.message; }
      if (Date.now() - t0 > ms) throw new Error(`timeout waiting for ${what}; last: ${JSON.stringify(last).slice(0, 600)}\nerrors: ${page.errors.join('\n')}`);
      await new Promise((r) => setTimeout(r, 150));
    }
  };
  const waitEvent = async (name, pred = () => true, ms = 20000) => {
    const t0 = Date.now();
    for (;;) {
      const evs = await events(name);
      const hit = evs.find(pred);
      if (hit) return hit;
      if (Date.now() - t0 > ms) throw new Error(`timeout waiting for event ${name}\nerrors: ${page.errors.join('\n')}`);
      await new Promise((r) => setTimeout(r, 150));
    }
  };
  const reload = async () => {
    await page.evaluate('globalThis.__oldPage = true').catch(() => {});
    await page.reload();
    await page.waitFor(`!globalThis.__oldPage && ${READY}`, timeoutMs);
    await page.evaluate(`(() => { const b = globalThis.__emberWasmBackend; globalThis.__ev = []; b.on('*', (e, d) => { if (globalThis.__ev.length < 5000) globalThis.__ev.push([e, d]); }); return true; })()`);
  };
  return Object.assign(page, { call, events, waitCall, waitEvent, reloadNode: reload });
}

export function tmpDir(prefix) {
  return fs.mkdtempSync(path.join(os.tmpdir(), prefix));
}
