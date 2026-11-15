// The real-time probe (ui/tests/realtime_probe.mjs) once at the start of the web suite, so the log
// and the CI job summary show what this host could do. It only reports: each step that needs real
// time re-checks the probe just before it runs and is SKIPPED (with the numbers) when the host
// cannot hold real time; docs/CI.md lists those steps and their always-on twins.
import { test, after } from 'node:test';
import assert from 'node:assert/strict';
import { DIST_DIR } from './helpers.mjs';
import { e2eSkip, startBrowser, realErrors, runCleanups, HOOK, realtimeSkip } from './lib/e2e.mjs';
import { openPage } from './lib/cdp.mjs';
import { startServer } from './lib/static_server.mjs';

const skip = e2eSkip();
const cleanups = [];
after(async () => { await runCleanups(cleanups); }, HOOK);

test('real-time probe: camera frames, audio callbacks and timers on this host (reported; gates nothing here)', { skip, timeout: 90000 }, async (t) => {
  const browser = await startBrowser(['--autoplay-policy=no-user-gesture-required']);
  if (browser.error) { t.skip(`Chrome could not start: ${browser.error}`); return; }
  cleanups.push(() => browser.close());
  const srv = await startServer(DIST_DIR);
  cleanups.push(() => srv.close());
  const P = await openPage(browser.cdp, `${srv.url}index.html?nohub=1&profile=probe`);
  cleanups.push(() => P.close());
  await P.waitFor('document.readyState === "complete"', 20000);
  const why = await realtimeSkip(t, P, ['camera', 'audio', 'timers'], 'web suite start');
  t.diagnostic(why ? `real-time steps on this host will be skipped: ${why}` : 'this host holds real time: real-time steps run and must pass');
  assert.deepEqual(realErrors(P.errors), []);
});
