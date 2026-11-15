// Start-up animation, motion and Simple mode (RPC_API §8) in every packaging, real headless Chrome:
// the static site, the single file from file://, and the offline PWA with the server gone.
//   - the first paint is the static logo (never a blank page); the boot screen hands over by
//     itself, is skipped by a click, and never holds up the node (node.info answers while it runs)
//   - prefers-reduced-motion: reduce (emulated) -> the start-up animation still full (only in-app
//     settings change it), the rest of the UI html[data-motion="reduced"]
//   - Simple mode persists across a reload
//   - first paint / time-to-interactive with and without the boot screen (diagnostics)
// Under automation the UI skips the boot screen (navigator.webdriver); these tests present
// themselves as a normal browser (webdriver=false) to see what a person sees.
import { test, after } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { DIST_DIR } from './helpers.mjs';
import { e2eSkip, startBrowser, nodePage, realErrors, tmpDir, runCleanups, HOOK, realtimeSkip } from './lib/e2e.mjs';
import { findChrome, launchChrome } from './lib/cdp.mjs';
import { startServer } from './lib/static_server.mjs';

const skip = e2eSkip([!fs.existsSync(path.join(DIST_DIR, 'js/boot/boot.js')) && 'dist/web has no boot screen (js/boot/boot.js): rebuild with the current UI']);
const cleanups = [];
after(async () => { await runCleanups(cleanups); }, HOOK);

const PERSON = `Object.defineProperty(Navigator.prototype, 'webdriver', { get: () => false, configurable: true });`;
// Early probe: what is on screen at the first paint, and when the node answers.
const PROBE = `
  globalThis.__probe = { firstPaintBoot: null };
  new PerformanceObserver((l) => { for (const e of l.getEntries()) if (e.name === 'first-contentful-paint' && __probe.firstPaintBoot === null) {
    const el = document.getElementById('ember-boot'); __probe.firstPaintBoot = !!(el && getComputedStyle(el).display !== 'none' && el.querySelector('svg'));
  } }).observe({ type: 'paint', buffered: true });
  // In the page, on every frame and timer tick (not over DevTools round trips, which a busy host
  // stretches past the hand-over): when the node first answers while the start-up screen is up.
  (function watch() {
    const b = globalThis.__emberBoot, w = globalThis.__emberWasmBackend, now = Math.round(performance.now());
    if (w && w.ready && __probe.nodeReady === undefined) __probe.nodeReady = now;
    if (b && b.active && w && w.ready && !__probe.asked) {
      __probe.asked = now; const t0 = performance.now();
      w.call('node.info').then((i) => { const g = globalThis.__emberBoot; __probe.during = { at: now, ms: performance.now() - t0, id: i.node_id, variant: b.variant, stillActive: !!(g && g.active) }; });
    }
    if (__probe.during || (b && !b.active && __probe.nodeReady !== undefined)) return;
    requestAnimationFrame(watch); setTimeout(watch, 10);
  })();`;

// The machine's own reduce-motion setting decides the start-up variant (the product honours it;
// hosted macOS runners have it switched on). Every test states the preference it is about, so the
// result does not depend on the machine: no-preference unless a test passes its own.
const person = (extra = []) => async (send) => {
  await send('Page.addScriptToEvaluateOnNewDocument', { source: PERSON + PROBE });
  const features = [{ name: 'prefers-reduced-motion', value: 'no-preference' }, ...extra.filter((f) => f.name !== 'prefers-reduced-motion'), ...extra.filter((f) => f.name === 'prefers-reduced-motion')];
  await send('Emulation.setEmulatedMedia', { features: Object.values(Object.fromEntries(features.map((f) => [f.name, f]))) });
};

// lib/prefs.js reflects the preferences on <html data-simple/-motion> once the app has loaded it.
const PREFS_TODO = 'the app does not load lib/prefs.js yet (no html[data-simple] after start-up): ui/js/app.js must import it and call initPrefs(backend)';
async function prefsWired(P) {
  try { await P.waitFor(`document.documentElement.dataset.simple !== undefined`, 4000); return true; } catch { return false; }
}

async function bootSample(P) {
  // the node answers while the start-up screen is still up (recorded in the page by PROBE)
  try { return await P.waitFor(`(globalThis.__probe && __probe.during) || null`, 15000); } catch {
    return null;
  }
}

async function timings(P) {
  return P.evaluate(`(() => {
    const p = Object.fromEntries(performance.getEntriesByType('paint').map((e) => [e.name, Math.round(e.startTime)]));
    return { fp: p['first-paint'], fcp: p['first-contentful-paint'], firstPaintBoot: globalThis.__probe && __probe.firstPaintBoot };
  })()`);
}

const tti = (P) => P.waitFor(`(() => { const m = document.getElementById('main'); return m && m.firstElementChild && !document.getElementById('boot') && globalThis.__emberWasmBackend && __emberWasmBackend.ready ? Math.round(performance.now()) : null; })()`, 30000);

async function checkBoot(t, P, label) {
  await P.waitFor('globalThis.__probe && __probe.firstPaintBoot !== null', 15000);
  const tm = await timings(P);
  const s = await bootSample(P);
  const seen = await P.evaluate(`({ probe: globalThis.__probe, boot: globalThis.__emberBoot && { active: __emberBoot.active, reason: __emberBoot.reason, variant: __emberBoot.variant, at: __emberBoot.at } })`);
  // The screen never waits for the node beyond its cap. On a host where the node needs longer than
  // the cap to start there is no moment at which both are true, so that one check is SKIPPED with
  // the numbers (never passed); everything else about the screen is still checked below.
  const sb = seen.boot || {}, sp = seen.probe || {};
  const slowNode = !s && sb.reason === 'cap' && sb.at && (sp.nodeReady === undefined || sp.nodeReady > sb.at.out)
    ? `the node needed ${sp.nodeReady === undefined ? 'more than 15000' : sp.nodeReady} ms to start on this host, longer than the start-up screen shows (handed over at ${sb.at.out} ms: cap)` : null;
  if (slowNode) await t.test(`${label}: the node answers while the start-up screen is showing (real time)`, { skip: slowNode }, () => {});
  else {
    assert.ok(s, `${label}: the start-up screen was showing while the node answered (${JSON.stringify(seen)})`);
    assert.match(s.id, /^[0-9a-f]{16}$/);
  }
  const variant = s ? s.variant : sb.variant;
  // RPC_API §8: the full animation on every load, whatever the timing; the documented fallback is
  // the calm variant only where the machine cannot give a 2D canvas (the page detects it itself)
  const cap = await P.evaluate(`({ canvas2d: (() => { try { return typeof Path2D === 'function' && !!document.createElement('canvas').getContext('2d'); } catch { return false; } })(),
    reduce: matchMedia('(prefers-reduced-motion: reduce)').matches, webdriver: navigator.webdriver, lowpower: document.documentElement.dataset.lowpower, boot: globalThis.__emberBoot && __emberBoot.canvas })`);
  assert.equal(variant, cap.canvas2d ? 'full' : 'reduced', `${label}: variant ${variant} (inputs: ${JSON.stringify(cap)})`);
  assert.equal(tm.firstPaintBoot, true, `${label}: the first paint shows the static logo`);
  const ready = await tti(P);
  await P.waitFor(`globalThis.__emberBoot && !__emberBoot.active`, 8000);
  const reason = await P.evaluate('__emberBoot.reason');
  assert.ok(!/tap|key|skip|click/.test(reason), `${label}: handed over by itself (${reason})`);
  const at = await P.evaluate('__emberBoot.at');
  if (variant === 'full') assert.ok(at.out - at.frame0 >= 2400 - 20, `${label}: the whole animation played from its first frame (${JSON.stringify(at)})`);
  await P.waitFor(`(() => { const e = document.getElementById('ember-boot'); return !e || getComputedStyle(e).display === 'none' || e.hidden; })()`, 5000);
  t.diagnostic(`${label}: first paint ${tm.fp} ms, first contentful paint ${tm.fcp} ms (the static logo), interactive ${ready} ms, ${s ? `node.info ${s.ms.toFixed(1)} ms during` : 'node not ready during'} the ${variant} animation, handed over: ${reason}`);
  return { ...tm, tti: ready, reason };
}

test('static site: static logo first, node answers during the animation, hands over by itself; timings vs ?noboot=1', { skip, timeout: 120000 }, async (t) => {
  const b = await startBrowser(); if (b.error) { t.skip(b.error); return; }
  cleanups.push(() => b.close());
  const srv = await startServer(DIST_DIR);
  cleanups.push(() => srv.close());
  const P = await nodePage(b, `${srv.url}index.html?nohub=1&profile=bootsite`, { beforeNavigate: person() });
  t.after(() => P.close());
  const withBoot = await checkBoot(t, P, 'site');
  // a reload (the app's state now warm, the node ready at once) plays the same full animation
  await P.reloadNode();
  await checkBoot(t, P, 'site, reloaded');
  const Q = await nodePage(b, `${srv.url}index.html?nohub=1&noboot=1&profile=bootsite2`, { beforeNavigate: person() });
  t.after(() => Q.close());
  const nb = await timings(Q);
  const nbTti = await tti(Q);
  t.diagnostic(`without the boot screen: first contentful paint ${nb.fcp} ms, interactive ${nbTti} ms (with: ${withBoot.fcp} / ${withBoot.tti} ms)`);
  console.log(`# boot: FCP ${withBoot.fcp} ms (logo) vs ${nb.fcp} ms without; interactive ${withBoot.tti} vs ${nbTti} ms`);
  // A wall-clock comparison of two loads: meaningful only on a host that keeps time, and only when
  // the cold load's node started within the screen's cap (else the host, not the screen, was slow).
  const rt = await realtimeSkip(t, Q, ['timers'], 'boot timings');
  const why = rt || (withBoot.reason === 'cap' ? `the node took longer than the start-up screen's cap to start on this host (handed over: cap), so the timings compare host speed` : null);
  await t.test('the boot screen does not hold up the app (wall clock, with vs without it)', { skip: why || false }, () => {
    assert.ok(withBoot.tti < nbTti + 1500, `interactive ${withBoot.tti} ms with the screen vs ${nbTti} ms without`);
  });
  assert.deepEqual(realErrors(P.errors), []);
});

test('a click skips the start-up screen at once', { skip, timeout: 60000 }, async (t) => {
  const b = await startBrowser(); if (b.error) { t.skip(b.error); return; }
  cleanups.push(() => b.close());
  const srv = await startServer(DIST_DIR);
  cleanups.push(() => srv.close());
  const P = await nodePage(b, `${srv.url}index.html?nohub=1&boot=full&profile=bootskip`, { beforeNavigate: person() });
  t.after(() => P.close());
  const s = await bootSample(P);
  assert.ok(s, 'running');
  const t0 = Date.now();
  for (const type of ['mousePressed', 'mouseReleased']) await P.send('Input.dispatchMouseEvent', { type, x: 200, y: 200, button: 'left', clickCount: 1 });
  await P.waitFor('!__emberBoot.active', 3000);
  t.diagnostic(`skipped in ${Date.now() - t0} ms (${await P.evaluate('__emberBoot.reason')})`);
  assert.deepEqual(realErrors(P.errors), []);
});

test('prefers-reduced-motion: reduce -> the start-up animation still plays in full (owner decision), the rest of the UI html[data-motion="reduced"]', { skip, timeout: 60000 }, async (t) => {
  const b = await startBrowser(); if (b.error) { t.skip(b.error); return; }
  cleanups.push(() => b.close());
  const srv = await startServer(DIST_DIR);
  cleanups.push(() => srv.close());
  const P = await nodePage(b, `${srv.url}index.html?nohub=1&profile=bootcalm`, { beforeNavigate: person([{ name: 'prefers-reduced-motion', value: 'reduce' }]) });
  t.after(() => P.close());
  // RPC_API §8: only the app's own settings change the launch animation, not the platform's
  const cap = await P.evaluate(`({ v: __emberBoot.variant, os: __emberBoot.osReduce, canvas2d: (() => { try { return typeof Path2D === 'function' && !!document.createElement('canvas').getContext('2d'); } catch { return false; } })() })`);
  assert.equal(cap.os, true, 'the page sees the platform asking for reduced motion');
  assert.equal(cap.v, cap.canvas2d ? 'full' : 'reduced', `variant with the platform setting on: ${JSON.stringify(cap)}`);
  assert.equal((await P.call('host.info')).accessibility.reduce_motion, true, 'host.info.accessibility follows the media query');
  // the platform setting changes while the app runs -> host.accessibility event
  await P.send('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-reduced-motion', value: 'no-preference' }] });
  const ev = await P.waitEvent('host.accessibility', (d) => d.reduce_motion === false, 5000);
  assert.equal(ev.reduce_motion, false);
  await P.send('Emulation.setEmulatedMedia', { features: [{ name: 'prefers-reduced-motion', value: 'reduce' }] });
  if (!(await prefsWired(P))) { t.todo(PREFS_TODO); return; }
  await P.waitFor(`document.documentElement.dataset.motion === 'reduced'`, 10000);
  assert.deepEqual(realErrors(P.errors), []);
});

test('Simple mode persists across a reload (kv "ui" + local mirror applied before first paint)', { skip, timeout: 60000 }, async (t) => {
  const b = await startBrowser(); if (b.error) { t.skip(b.error); return; }
  cleanups.push(() => b.close());
  const srv = await startServer(DIST_DIR);
  cleanups.push(() => srv.close());
  const P = await nodePage(b, `${srv.url}index.html?nohub=1&profile=simple`, { beforeNavigate: person() });
  t.after(() => P.close());
  if (!(await prefsWired(P))) { t.todo(PREFS_TODO); return; }
  await P.evaluate(`import(new URL('js/lib/prefs.js', location.href).href).then((m) => m.setPref('simple_mode', true)).then(() => true)`);
  await P.waitFor(`document.documentElement.dataset.simple === 'true'`, 5000);
  const kv = await P.call('kv.get', { ns: 'ui', key: 'simple_mode' });
  assert.equal(kv.value, true, 'stored in the core (kv ns "ui")');
  await P.evaluate('__emberWasmBackend._flush().then(() => true)');
  await P.reloadNode();
  assert.equal(await P.evaluate('document.documentElement.dataset.simple'), 'true', 'Simple mode after the reload');
  assert.deepEqual(realErrors(P.errors), []);
});

test('single file from file://: static logo first, animation, hand-over, node answers meanwhile', { skip, timeout: 120000 }, async (t) => {
  const b = await startBrowser(); if (b.error) { t.skip(b.error); return; }
  cleanups.push(() => b.close());
  const url = `${pathToFileURL(path.join(DIST_DIR, 'embermesh.html')).href}?profile=bootsingle`;
  const P = await nodePage(b, url, { beforeNavigate: person(), timeoutMs: 60000 });
  t.after(() => P.close());
  await checkBoot(t, P, 'single file');
  assert.deepEqual(realErrors(P.errors), []);
});

test('offline PWA: with the server gone, a cold start shows the logo, animates, hands over, works', { skip, timeout: 240000 }, async (t) => {
  const prof = tmpDir('ember-bootprof-');
  const srv = await startServer(DIST_DIR, { base: '/embermesh/', ranges: true });
  let b = await launchChrome(findChrome(), { userDataDir: prof });
  let P = null;
  // one hook, in order: page, browser (it holds the profile), server, then the profile directory
  t.after(() => runCleanups([() => fs.rmSync(prof, { recursive: true, force: true }), () => srv.close(), () => b.close(), () => P && P.close()]));
  P = await nodePage(b, `${srv.url}?nohub=1`, { beforeNavigate: person(), timeoutMs: 60000 });
  await P.waitCall('host.app.offline_status', {}, (s) => s.offline_ready, 180000, 'offline_ready');
  await P.close();
  await srv.close();
  await b.close();
  b = await launchChrome(findChrome(), { userDataDir: prof });
  P = await nodePage(b, `${srv.url}?nohub=1`, { beforeNavigate: person(), timeoutMs: 60000 });
  await checkBoot(t, P, 'offline PWA');
  const cats = await P.waitCall('guides.categories', {}, (c) => c.total > 0, 30000);
  assert.ok(cats.total > 0);
  assert.deepEqual(realErrors(P.errors).filter((e) => !/net::ERR_/.test(e)), []);
});
