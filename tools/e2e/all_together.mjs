#!/usr/bin/env node
// All four apps on ONE mesh at the same time: one desktop hub (emberd), one headless-Chrome browser
// guest (the real WASM core, joined through /hub.json + /ws and connected for the whole run), the
// Android app's EmberdInteropTest on an emulator and the iOS app's EmberdInteropTests
// testWebSocketToEmberd in a simulator, both over WebSocket to that same emberd. Then it checks that
// messages really crossed apps through the hub, verified, in both directions (phone -> hub ->
// browser, and browser -> hub -> each phone's own Public channel), not only pairwise.
//
//   node tools/e2e/all_together.mjs [--skip android|ios|browser[,…]] [--serial emulator-5554]
//        [--simulator NAME|UDID] [--sequential] [--keep] [--host-ip 10.0.2.2] [--emberd PATH]
//        [--web DIR] [--chrome PATH] [--android-timeout S] [--ios-timeout S] [--relay-timeout S]
//
// See tools/e2e/README.md. No npm dependencies (Node >= 22); shells out to python3 + adb (Android,
// through apps/android/scripts/e2e_emberd.py --attach-port) and xcodebuild (iOS).
// Exit code 0 only if every check passed; skipped apps are reported as skipped, never as passed.
import { spawn, execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { sleep, freePort, waitFor, findChrome, launchChrome, firstRun } from '../../apps/desktop/tests/chrome_guest.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const repo = path.resolve(here, '../..');
const rel = p => path.relative(repo, p) || '.';

// ---- options --------------------------------------------------------------------------------------
const argv = process.argv.slice(2);
function opts(name) {  // every value of --name V / --name=V
  const out = [];
  argv.forEach((a, i) => {
    if (a === name && i + 1 < argv.length) out.push(argv[i + 1]);
    else if (a.startsWith(name + '=')) out.push(a.slice(name.length + 1));
  });
  return out;
}
const opt = (name, d) => { const v = opts(name); return v.length ? v[v.length - 1] : d; };
const flag = name => argv.includes(name);
if (flag('--help') || flag('-h')) {
  console.log(fs.readFileSync(fileURLToPath(import.meta.url), 'utf8').split('\n').slice(1, 15).map(l => l.replace(/^\/\/ ?/, '')).join('\n'));
  process.exit(0);
}
const APPS = ['browser', 'android', 'ios'];
const skip = new Set(opts('--skip').flatMap(v => v.split(',')).map(s => s.trim().toLowerCase()).filter(Boolean));
for (const s of skip) if (!APPS.includes(s)) { console.error(`--skip: unknown app '${s}' (use ${APPS.join(', ')})`); process.exit(2); }
const keep = flag('--keep');
const sequential = flag('--sequential');
const serial = opt('--serial', process.env.ANDROID_SERIAL || 'emulator-5554');
const simulator = opt('--simulator', process.env.IOS_SIMULATOR || '');
const hostIp = opt('--host-ip', '10.0.2.2');
const androidTimeout = Number(opt('--android-timeout', 420)) * 1000;
const iosTimeout = Number(opt('--ios-timeout', 1800)) * 1000;
const emberdBin = opt('--emberd') || [path.join(repo, 'dist/desktop/emberd'), path.join(repo, 'build/desktop/apps/desktop/emberd')].find(p => fs.existsSync(p));
const web = opt('--web', path.join(repo, 'dist/web'));
const chromePath = findChrome(opt('--chrome'));
const want = app => !skip.has(app);

// ---- bookkeeping ----------------------------------------------------------------------------------
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'ember-all-'));
const procs = [];          // { kill(sig) } — everything we started; killed in finish()
let failures = 0, checksRun = 0;
const notChecked = [];
const status = { browser: 'skipped', android: 'skipped', ios: 'skipped' };  // -> 'took part' | 'unavailable' | 'failed'
const ok = (cond, what) => { checksRun++; console.log(`${cond ? 'ok  ' : 'FAIL'} ${what}`); if (!cond) failures++; return !!cond; };
const na = (what, why) => { notChecked.push(what); console.log(`n/a  ${what} (not checked: ${why})`); };
const note = s => console.log(`     ${s}`);
let finishing = false;
process.on('uncaughtException', e => { ok(false, 'exception: ' + (e && e.stack || e)); finish(); });
process.on('unhandledRejection', e => { ok(false, 'exception: ' + (e && e.stack || e)); finish(); });
for (const sig of ['SIGINT', 'SIGTERM']) process.on(sig, () => { ok(false, `interrupted (${sig})`); finish(); });

// A child in its own process group, so a timeout or teardown also stops what it started (adb shell,
// the xcodebuild test runner). Output goes to <tmp>/<log> and is kept in memory for parsing.
function runChild(cmd, args, { env = {}, timeoutMs, log, onLine }) {
  return new Promise(res => {
    const fd = fs.openSync(path.join(tmp, log), 'w');
    let text = '', timedOut = false, partial = '';
    let p;
    const fullEnv = { ...process.env, ...env };
    for (const k of Object.keys(fullEnv)) if (fullEnv[k] === undefined) delete fullEnv[k];   // undefined = unset
    try {
      p = spawn(cmd, args, { cwd: repo, env: fullEnv, stdio: ['ignore', 'pipe', 'pipe'], detached: true });
    } catch (e) { fs.closeSync(fd); return res({ code: -1, text: String(e), timedOut }); }
    const kill = sig => { try { process.kill(-p.pid, sig); } catch { try { p.kill(sig); } catch {} } };
    const entry = { kill, child: p };
    procs.push(entry);
    const onData = b => {
      const s = b.toString(); text += s; fs.writeSync(fd, s);
      if (onLine) { partial += s; const lines = partial.split('\n'); partial = lines.pop(); lines.forEach(onLine); }
    };
    p.stdout.on('data', onData); p.stderr.on('data', onData);
    const timer = setTimeout(() => { timedOut = true; kill('SIGINT'); setTimeout(() => kill('SIGKILL'), 15000).unref(); }, timeoutMs);
    p.on('error', e => { text += String(e); });
    p.on('close', code => {
      clearTimeout(timer); fs.closeSync(fd);
      procs.splice(procs.indexOf(entry), 1);
      res({ code, text, timedOut });
    });
  });
}
const sh = (cmd, args, timeout = 20000) => execFileSync(cmd, args, { encoding: 'utf8', timeout, stdio: ['ignore', 'pipe', 'pipe'] });

// ---- preflight ------------------------------------------------------------------------------------
console.log(`== all apps on one mesh — taking part: ${APPS.filter(want).join(', ') || 'none'}${skip.size ? `; skipped: ${[...skip].join(', ')}` : ''}`);
if (!ok(APPS.some(want), 'at least one app besides emberd takes part')) await finish();
if (!ok(!!emberdBin && fs.existsSync(emberdBin), `emberd binary found (${emberdBin ? rel(emberdBin) : 'dist/desktop/emberd or build/desktop/apps/desktop/emberd'})`)) await finish();
if (!ok(fs.existsSync(path.join(web, 'index.html')), `web app found (${rel(web)}/index.html)`)) await finish();

if (want('browser') && !ok(!!chromePath, 'browser: Chrome/Chromium found (--chrome PATH)')) status.browser = 'unavailable';

const sdk = process.env.ANDROID_HOME || process.env.ANDROID_SDK_ROOT || path.join(os.homedir(), 'Library/Android/sdk');
const adb = fs.existsSync(path.join(sdk, 'platform-tools', 'adb')) ? path.join(sdk, 'platform-tools', 'adb') : 'adb';
if (want('android')) {
  let state = '', pkgs = '';
  try { state = sh(adb, ['-s', serial, 'get-state']).trim(); } catch (e) { state = String(e.stderr || e.message).trim().split('\n')[0]; }
  if (state === 'device') { try { pkgs = sh(adb, ['-s', serial, 'shell', 'pm', 'list', 'packages', 'org.embermesh.app']); } catch {} }
  const have = n => pkgs.split(/\r?\n/).some(l => l.trim() === 'package:' + n);
  if (!ok(state === 'device', `android: device ${serial} is online (adb get-state: ${state || '?'})`)) status.android = 'unavailable';
  else if (!ok(have('org.embermesh.app') && have('org.embermesh.app.test'),
    'android: app and test APK installed (else: cd apps/android && ./gradlew installDebug installDebugAndroidTest)')) status.android = 'unavailable';
}

let simId = '', simName = '';
if (want('ios')) {
  try {
    sh('xcodebuild', ['-version']);
    const devs = Object.values(JSON.parse(sh('xcrun', ['simctl', 'list', 'devices', 'available', '-j'])).devices).flat();
    const iphones = devs.filter(d => /^iPhone/.test(d.name));
    const booted = l => l.find(d => d.state === 'Booted') || l[0];
    const d = /^[0-9A-Fa-f-]{36}$/.test(simulator) ? devs.find(x => x.udid.toUpperCase() === simulator.toUpperCase())
      : simulator ? booted(devs.filter(x => x.name === simulator)) : booted(iphones);
    if (d) { simId = d.udid; simName = d.name; }
  } catch (e) { note(`ios: ${String(e.message).split('\n')[0]}`); }
  if (!ok(!!simId, `ios: xcodebuild and simulator ${simName || simulator || '(booted iPhone)'} available`)) status.ios = 'unavailable';
}

// ---- emberd: the one hub --------------------------------------------------------------------------
const port = await freePort();
const base = `http://127.0.0.1:${port}`;
// The emulator reaches the host's loopback as 10.0.2.2, so loopback is enough unless a real phone is used.
const bind = hostIp === '10.0.2.2' ? '127.0.0.1' : '0.0.0.0';
const dlog = fs.openSync(path.join(tmp, 'emberd.log'), 'w');
const d = spawn(emberdBin, ['--data-dir', path.join(tmp, 'hub'), '--port', String(port), '--bind', bind, '--local-no-token',
  '--no-udp', '--no-qr', '--name', 'Hub', '--web-root', web, '--log-level', 'info'], { stdio: ['ignore', dlog, dlog] });
procs.push({ kill: sig => { try { d.kill(sig); } catch {} }, child: d });
const rpc = async (method, params = {}) => {
  const r = await fetch(`${base}/rpc`, { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ method, params }), signal: AbortSignal.timeout(10000) });
  const j = await r.json();
  if (!j.ok) throw new Error(`${method}: ${j.error && j.error.message}`);
  return j.result;
};
if (!ok(await waitFor(async () => (await fetch(`${base}/hub.json`)).ok && await rpc('node.info'), 15000), `emberd hub is up on :${port} (bind ${bind}, /rpc without token from this computer)`)) await finish();
const hubInfo = await rpc('node.info');
note(`hub node ${hubInfo.node_id} "${hubInfo.name}", data ${tmp}`);

const nonce = 'all-' + Math.random().toString(16).slice(2, 8);
const publicList = async (call, conv = 'ch:public') => ((await call('messages.list', { conv, limit: 1000 })).messages || []);
const find = async (call, pred) => (await publicList(call)).find(pred);

// ---- browser guest (stays connected for the whole run) --------------------------------------------
let page = null, browserInfo = null;
const browserText = `browser public ${nonce}`;
const linkUp = async () => (await page.call('links.list')).links.find(x => x.endpoint && x.endpoint.kind === 'websocket' && x.state === 'up');
if (want('browser') && status.browser !== 'unavailable') {
  status.browser = 'failed';
  const browser = await launchChrome({ chrome: chromePath, dir: tmp, procs: { push: ch => procs.push({ kill: sig => { try { ch.kill(sig); } catch {} }, child: ch }) } });
  if (ok(!!browser, 'browser: headless Chrome started')) {
    page = await browser.openPage(`${base}/`);
    browserInfo = await firstRun(page, 'Browser guest', ok, browser.consoleLines);
    if (browserInfo) {
      const l = await waitFor(linkUp, 15000);
      if (ok(!!l, `browser: hub link up (${l && l.endpoint.config && l.endpoint.config.url})`)) {
        ok(await waitFor(async () => (await rpc('host.hub.status')).clients >= 1, 10000), 'emberd counts the browser guest');
        await page.call('messages.send', { conv: 'ch:public', text: browserText });
        const m = await waitFor(() => find(rpc, x => x.text === browserText && !x.outgoing), 20000);
        ok(m && m.from === browserInfo.node_id, `emberd Public has the browser guest's message (from ${m && m.from}, outgoing ${m && m.outgoing})`);
        status.browser = 'took part';
      }
    }
    if (status.browser !== 'took part') page = null;
  }
}

// ---- phones, against the same emberd --------------------------------------------------------------
// While they run: remember every node emberd lists in mesh.peers (a phone's test node may go away
// when its test ends), the most simultaneous hub clients, and whether the browser link ever dropped.
const seenPeers = new Map();
let maxClients = 0, browserDown = 0, browserPolls = 0, monitoring = true;
const monitor = (async () => {
  while (monitoring) {
    try { for (const p of (await rpc('mesh.peers')).peers || []) seenPeers.set(p.node_id, p); } catch {}
    try { maxClients = Math.max(maxClients, (await rpc('host.hub.status')).clients || 0); } catch {}
    if (page) { browserPolls++; try { if (!(await linkUp())) browserDown++; } catch { browserDown++; } }
    await sleep(1500);
  }
})();

async function runAndroid() {
  console.log(`== android: e2e_emberd.py --attach-port ${port} on ${serial} (log android.log)`);
  status.android = 'failed';
  const r = await runChild('python3', [path.join(repo, 'apps/android/scripts/e2e_emberd.py'), '--attach-port', String(port), '--serial', serial,
    '--host-ip', hostIp, '--transports', 'ws', '--nonce', nonce, ...(page ? ['--expect-public', browserText] : [])], { timeoutMs: androidTimeout, log: 'android.log', env: { ANDROID_HOME: sdk } });
  const res = (r.text.match(/^RESULT .*$/m) || [''])[0];
  expectSeen.android = (r.text.match(/interop expect-public: [^\n"]*/) || [''])[0].trim();
  if (ok(r.code === 0 && /^RESULT PASS/.test(res), `android: EmberdInteropTest over WebSocket passed against the shared emberd (${r.timedOut ? 'TIMED OUT' : res || 'exit ' + r.code})`)) status.android = 'took part';
  else note('android tail: ' + r.text.slice(-800).replace(/\n/g, '\n       '));
}

let iosSaw = '';
const expectSeen = {};   // app -> the device test's "interop expect-public: …" line (browser -> hub -> phone)
async function runIos() {
  console.log(`== ios: xcodebuild test testWebSocketToEmberd on ${simName} (log ios.log; builds first if needed)`);
  status.ios = 'failed';
  const r = await runChild('xcodebuild', ['-project', path.join(repo, 'apps/ios/EmberMesh.xcodeproj'), '-scheme', 'EmberMesh',
    '-destination', `platform=iOS Simulator,id=${simId}`, '-derivedDataPath', path.join(repo, 'build/ios/DerivedData'),
    '-resultBundlePath', path.join(tmp, 'ios.xcresult'), '-only-testing:EmberMeshTests/EmberdInteropTests/testWebSocketToEmberd', 'test'],
    { timeoutMs: iosTimeout, log: 'ios.log', env: { TEST_RUNNER_EMBERD_PORT: String(port), TEST_RUNNER_EMBERD_TOKEN: undefined, TEST_RUNNER_EMBERD_TCP_PORT: undefined,
      TEST_RUNNER_EXPECT_PUBLIC: page ? browserText : undefined },
      onLine: l => { if (/interop websocket: ok/.test(l)) iosSaw = l.trim(); const e = l.match(/interop expect-public: .*/); if (e) expectSeen.ios = e[0].trim(); } });
  const passed = /testWebSocketToEmberd[^\n]*\bpassed\b/.test(r.text);
  const skipped = /testWebSocketToEmberd[^\n]*\bskipped\b/.test(r.text);
  if (ok(r.code === 0 && passed && !skipped, `ios: testWebSocketToEmberd passed against the shared emberd (${r.timedOut ? 'TIMED OUT' : `exit ${r.code}${skipped ? ', SKIPPED' : ''}`})`)) status.ios = 'took part';
  else note('ios tail: ' + r.text.slice(-1200).replace(/\n/g, '\n       '));
  if (iosSaw) note(iosSaw);
}

const jobs = [];
if (want('android') && status.android !== 'unavailable') jobs.push(runAndroid);
if (want('ios') && status.ios !== 'unavailable') jobs.push(runIos);
if (sequential) for (const j of jobs) await j();
else await Promise.all(jobs.map(j => j()));
await sleep(2000);
monitoring = false;
await monitor;

// ---- cross-app delivery ---------------------------------------------------------------------------
console.log('== cross-app checks');
const androidText = `android->emberd public ${nonce}`;
const iosRe = /^ios→emberd websocket [0-9A-Fa-f-]{6}$/;   // EmberdInteropTests.run(kind: "websocket") tag
const origin = {};   // app -> {text, from}
if (status.android !== 'skipped' && status.android !== 'unavailable') {
  const m = await waitFor(() => find(rpc, x => x.text === androidText && !x.outgoing), 10000);
  if (ok(!!m, `emberd Public has the Android message "${androidText}" (outgoing false)`)) origin.android = { text: m.text, from: m.from, name: m.from_name };
}
if (status.ios !== 'skipped' && status.ios !== 'unavailable') {
  const m = await waitFor(() => find(rpc, x => iosRe.test(x.text) && !x.outgoing), 10000);
  if (ok(!!m, `emberd Public has the iOS message ${m ? `"${m.text}"` : '"ios→emberd websocket <tag>"'} (outgoing false)`)) origin.ios = { text: m.text, from: m.from, name: m.from_name };
}
if (browserInfo && status.browser === 'took part') {
  const m = await find(rpc, x => x.text === browserText && !x.outgoing);
  ok(!!m, `emberd Public still has the browser guest's message "${browserText}"`);
  origin.browser = { text: browserText, from: browserInfo.node_id, name: browserInfo.name };
}

// phone -> hub -> browser, verified and prompt. A recipient that lacks the sender's key holds a
// signed packet for up to 5 minutes while it asks (WHOIS), then delivers it marked unauthenticated
// (docs/PROTOCOL.md, "Verification"); such a late, unauthenticated delivery is a FAIL here.
const relayWaitMs = Number(opt('--relay-timeout', 45)) * 1000;
const relayStart = Date.now();
await Promise.all(['android', 'ios'].map(async app => {
  if (!origin[app]) return;
  if (!page) { na(`${app} -> hub -> browser relay`, 'the browser guest is not taking part'); return; }
  const m = await waitFor(() => find(page.call, x => x.text === origin[app].text && !x.outgoing), relayWaitMs, 1000);
  const s = ((Date.now() - relayStart) / 1000).toFixed(0);
  ok(m && m.from === origin[app].from && m.signed_ok === true, `browser guest's Public shows the ${app} message "${origin[app].text}" from ${origin[app].from}` +
    (m ? ` (${s} s after the phones finished, hops ${m.hops}, signed_ok ${m.signed_ok})` : ` (not within ${relayWaitMs / 1000} s)`));
}));
if (page) ok(browserDown === 0 && !!(await linkUp().catch(() => null)), `browser guest stayed connected to the hub for the whole run (${browserPolls} polls, ${browserDown} without the link up)`);

// emberd knows every app's node as a mesh peer (seen at any time during the run, or now)
try { for (const p of (await rpc('mesh.peers')).peers || []) seenPeers.set(p.node_id, p); } catch {}
let contacts = [];
try { contacts = (await rpc('contacts.list')).contacts || []; } catch {}
for (const app of ['browser', 'android', 'ios']) {
  if (!origin[app]) continue;
  const p = seenPeers.get(origin[app].from);
  const c = contacts.find(x => x.node_id === origin[app].from);
  ok(!!p, `emberd lists the ${app} node ${origin[app].from} ("${(p && p.name) || origin[app].name || '?'}") in mesh.peers${c ? ', also a contact' : ''}`);
}
note(`most guests on the hub at once: ${maxClients} (browser + phones overlap only when their tests run at the same time)`);

// browser -> phone
if (page && status.android === 'took part') {
  let line = '';
  try { line = sh(adb, ['-s', serial, 'logcat', '-d', '-s', 'EmberdInterop:I']).split('\n').filter(l => l.includes(`:${port}/ws`)).pop() || ''; } catch {}
  if (line) note(`android's mesh.peers at the end of its test ${line.includes(browserInfo.node_id) ? 'includes' : 'does not include'} the browser guest (informational)`);
}
// The device tests were given the browser's text (expect_public / TEST_RUNNER_EXPECT_PUBLIC) and
// report "interop expect-public: ok (from <node_id>, signed_ok <bool>)" or "… timeout".
for (const app of ['android', 'ios']) {
  if (status[app] === 'skipped' || status[app] === 'unavailable') continue;
  if (!page) { na(`browser -> hub -> ${app} delivery`, 'the browser guest is not taking part'); continue; }
  const line = expectSeen[app] || '';
  const e = line.match(/^interop expect-public: ok \(from ([0-9a-f]+), signed_ok (\w+)\)/);
  ok(e && e[1] === browserInfo.node_id && e[2] === 'true',
    `${app}'s own Public channel received the browser guest's message "${browserText}" from ${browserInfo.node_id} (device test: ${line || 'no expect-public line'})`);
}

await finish();

async function finish() {
  if (finishing) return;
  finishing = true;
  monitoring = false;
  for (const p of [...procs].reverse()) p.kill('SIGTERM');
  await sleep(1500);
  for (const p of procs) p.kill('SIGKILL');
  const tookPart = APPS.filter(a => status[a] === 'took part');
  const other = APPS.filter(a => status[a] !== 'took part').map(a => `${a} ${status[a]}`);
  const pass = failures === 0 && checksRun > 0;
  console.log(`\nRESULT ${pass ? 'PASS' : 'FAIL'} — ${checksRun - failures}/${checksRun} checks ok; on one mesh: emberd hub${tookPart.length ? ', ' + tookPart.join(', ') : ''}` +
    `${other.length ? `; not taking part: ${other.join(', ')}` : ''}${notChecked.length ? `; not checked: ${notChecked.join(', ')}` : ''}`);
  if (keep || !pass) console.log(`emberd data dir and logs kept in ${tmp}`);
  else fs.rmSync(tmp, { recursive: true, force: true });
  process.exit(pass ? 0 : 1);
}
