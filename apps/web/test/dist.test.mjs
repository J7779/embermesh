// Structural checks of the packaged output (no browser needed): every file the site references
// exists, the service worker precaches only existing files, and the single file is self-contained,
// syntactically valid, and its bootstrap (import map + fetch shim) works in a simulated page.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import crypto from 'node:crypto';
import path from 'node:path';
import vm from 'node:vm';
import { spawnSync } from 'node:child_process';
import { DIST_DIR, WEB_DIR, haveDist, NO_DIST } from './helpers.mjs';
import { pngChunks } from '../tools/make-icons.mjs';

const skip = !haveDist() && NO_DIST;
const read = (f) => fs.readFileSync(path.join(DIST_DIR, f), 'utf8');
const CANON = 'https://embermesh.invalid/';

function scripts(html) {
  const out = [];
  const re = /<script\b([^>]*)>([\s\S]*?)<\/script>/gi;
  let m;
  while ((m = re.exec(html))) {
    const attrs = m[1];
    out.push({ attrs, type: (attrs.match(/\btype=["']?([^"'\s>]+)/i) || [])[1] || 'classic', path: (attrs.match(/data-path="([^"]+)"/) || [])[1], body: m[2] });
  }
  return out;
}

// The web-platform files are checked strictly; other UI files only produce warnings
// — a broken one still breaks the app, so read the diagnostics.
const OWN = /^(wasm\/|js\/(backends\/(wasm|native|remote)\.js|endpoints\/))/;

function checkSyntax(code, asModule, label, t) {
  const r = spawnSync(process.execPath, ['--check', `--input-type=${asModule ? 'module' : 'commonjs'}`], { input: code, encoding: 'utf8' });
  if (r.status === 0) return;
  const msg = `${label}: ${r.stderr.split('\n').slice(0, 3).join(' | ')}`;
  if (t && label && !OWN.test(label)) { t.diagnostic(`WARNING syntax error in ${msg}`); return; }
  assert.fail(msg);
}

test('static site: required files, PWA wiring, precache list', { skip }, (t) => {
  for (const f of ['index.html', 'wasm/ember.mjs', 'wasm/ember.wasm', 'wasm/build.json', 'hub.json', 'sw.js', 'manifest.webmanifest', 'icon.svg', 'pwa.js', 'js/backends/wasm.js', 'js/endpoints/router.js']) {
    assert.ok(fs.existsSync(path.join(DIST_DIR, f)), f);
  }
  assert.deepEqual(JSON.parse(read('hub.json')), { embermesh_hub: false });
  const build = JSON.parse(read('wasm/build.json'));
  assert.match(build.version, /^\d+\.\d+\.\d+$/);
  assert.match(build.build, /^[0-9a-f]{12}$/);
  if (build.guides) assert.ok(fs.existsSync(path.join(DIST_DIR, build.guides)));
  const index = read('index.html');
  assert.match(index, /rel="manifest"/);
  assert.match(index, /pwa\.js/);
  const manifest = JSON.parse(read('manifest.webmanifest'));
  // relative everywhere: the same build works at /, under /<repo>/ (GitHub Pages) and from a hub
  for (const k of ['start_url', 'scope', 'id']) assert.equal(manifest[k], './', k);
  assert.equal(manifest.theme_color, '#17110f');
  assert.equal(manifest.background_color, '#17110f');
  const purposes = new Set(manifest.icons.map((i) => i.purpose));
  for (const p of ['any', 'maskable', 'monochrome']) assert.ok(purposes.has(p), `icon purpose ${p}`);
  for (const i of manifest.icons) {
    assert.ok(!/^\/|^[a-z]+:/i.test(i.src), `relative icon URL ${i.src}`);
    assert.ok(fs.existsSync(path.join(DIST_DIR, i.src)), i.src);
    if (i.src.endsWith('.png')) {
      const png = fs.readFileSync(path.join(DIST_DIR, i.src));
      assert.equal(png.readUInt32BE(16), Number(i.sizes.split('x')[0]), `${i.src} size`);
      assert.deepEqual([...new Set(pngChunks(png))].sort(), ['IDAT', 'IEND', 'IHDR'], `${i.src}: no metadata chunks`);
    }
  }
  for (const f of ['icons/favicon.svg', 'icons/favicon-32.png', 'icons/apple-touch-icon.png']) assert.ok(index.includes(`href="${f}"`), `index.html links ${f}`);
  assert.match(index, /name="theme-color" content="#17110f"/);
  assert.ok(!/(?:src|href)=["']\//.test(index), 'no root-absolute URLs in index.html');
  // build manifest: every file with size + sha256 (the offline install verifies against it)
  const listed = new Map(build.files.map((f) => [f.path, f]));
  for (const f of ['index.html', 'wasm/ember.wasm', 'wasm/ember.mjs', 'manifest.webmanifest', 'icons/icon-192.png']) assert.ok(listed.has(f), `build.json lists ${f}`);
  assert.ok(!listed.has('sw.js') && !listed.has('wasm/build.json') && !listed.has(build.guides), 'not listed: sw.js, build.json, the pack');
  for (const f of build.files) {
    const b = fs.readFileSync(path.join(DIST_DIR, f.path));
    assert.equal(b.length, f.size, f.path);
    assert.equal(crypto.createHash('sha256').update(b).digest('hex'), f.sha256, f.path);
  }
  assert.equal(build.total_bytes, build.files.reduce((n, f) => n + f.size, 0) + (build.guides_bytes || 0));
  if (build.guides) {
    assert.match(build.guides_hash, /^[0-9a-f]{16}$/);
    assert.equal(build.guides_bytes, fs.statSync(path.join(DIST_DIR, build.guides)).size);
  }
  // the service worker holds no build data (which build it serves is decided at run time)
  const sw = read('sw.js');
  assert.equal(sw, fs.readFileSync(path.join(WEB_DIR, 'pwa/sw.js'), 'utf8'));
  assert.ok(!/__[A-Z]+__/.test(sw));
  assert.ok(!fs.existsSync(path.join(DIST_DIR, 'tests')), 'UI tests are not shipped');
  checkSyntax(sw, false, 'sw.js');
  // every relative import in the shipped JS resolves to a shipped file
  const walk = (d) => fs.readdirSync(d, { withFileTypes: true }).flatMap((e) => (e.isDirectory() ? walk(path.join(d, e.name)) : [path.join(d, e.name)]));
  for (const f of walk(path.join(DIST_DIR, 'js'))) {
    const src = fs.readFileSync(f, 'utf8');
    for (const m of src.matchAll(/(?:from|import)\s*\(?\s*['"](\.{1,2}\/[^'"]+)['"]/g)) {
      if (fs.existsSync(path.resolve(path.dirname(f), m[1]))) continue;
      const rel = path.relative(DIST_DIR, f).split(path.sep).join('/');
      // strict for the web-platform files; other UI files only warn
      if (/^js\/(backends\/(wasm|native|remote)\.js|endpoints\/)/.test(rel)) assert.fail(`${rel} imports missing ${m[1]}`);
      t.diagnostic(`WARNING: ${rel} imports missing ${m[1]}`);
    }
  }
});

test('single file: self-contained and every embedded script parses', { skip }, (t) => {
  const html = read('embermesh.html');
  assert.match(html, /<meta charset="utf-8">/i);
  // nothing local is referenced by URL (only data:, blob: or canonical in-file URLs)
  const markup = html.replace(/(<script\b[^>]*>)[\s\S]*?<\/script>/gi, '$1</script>');
  for (const m of markup.matchAll(/<(?:script|link|img)\b[^>]*\b(?:src|href)=["']?([^"'\s>]+)/gi)) {
    assert.match(m[1], /^(data:|https?:)/, `external reference left in single file: ${m[1]}`);
  }
  assert.doesNotMatch(html, /rel="manifest"/);
  const all = scripts(html);
  const mods = all.filter((s) => s.type === 'text/x-ember-module');
  const paths = new Set(mods.map((s) => s.path));
  assert.ok(paths.has('wasm/ember.mjs') && paths.has('js/backends/wasm.js') && paths.has('js/endpoints/router.js'));
  for (const s of mods) {
    checkSyntax(s.body, true, s.path, t);
    // every canonical static import points at an embedded module
    for (const m of s.body.matchAll(/(?:from|import)\s*['"](https:\/\/embermesh\.invalid\/[^'"]+)['"]/g)) {
      const line = s.body.slice(s.body.lastIndexOf('\n', m.index) + 1, m.index);
      if (/^\s*(\/\/|\*)/.test(line)) continue; // an example in a comment (the rewriter is textual)
      assert.ok(paths.has(m[1].slice(CANON.length)), `${s.path} imports ${m[1]} which is not embedded`);
    }
  }
  for (const s of all.filter((x) => x.type === 'module')) checkSyntax(s.body, true, 'inline module');
  for (const s of all.filter((x) => x.type === 'classic' && x.body.trim())) checkSyntax(s.body, false, 'classic script');
  JSON.parse(all.find((s) => s.attrs.includes('id="ember-assets"')).body);
});

test('single file bootstrap: import map + fetch shim in a simulated file:// page', { skip }, async () => {
  const html = read('embermesh.html');
  const all = scripts(html);
  const assetsEl = all.find((s) => s.attrs.includes('id="ember-assets"'));
  const mods = all.filter((s) => s.type === 'text/x-ember-module');
  const boot = all.find((s) => s.type === 'classic' && s.body.includes('__EMBER_SINGLE__'));
  assert.ok(boot, 'bootstrap script present');
  const inserted = [];
  const guidesEl = all.find((s) => s.attrs.includes('id="ember-guides"'));
  const elements = { 'ember-assets': { textContent: assetsEl.body }, 'ember-guides': guidesEl ? { textContent: guidesEl.body } : null };
  const realFetchCalls = [];
  const win = {
    location: { href: 'file:///media/usb/embermesh.html' },
    document: {
      getElementById: (id) => elements[id] || null,
      querySelectorAll: () => mods.map((m) => ({ getAttribute: () => m.path, textContent: m.body })),
      createElement: () => ({}),
      currentScript: { after: (el) => inserted.push(el) },
    },
    fetch: async (u) => { realFetchCalls.push(String(u)); return new Response('real'); },
    URL, Blob, Response, atob, JSON, Uint8Array, Promise, String, decodeURIComponent,
  };
  win.window = win;
  vm.runInNewContext(boot.body, win);
  const im = JSON.parse(inserted[0].textContent);
  assert.equal(inserted[0].type, 'importmap');
  assert.equal(Object.keys(im.imports).length, mods.length);
  assert.ok(im.imports[`${CANON}js/backends/wasm.js`].startsWith('blob:'));
  assert.equal(typeof win.__emberImport, 'function');
  // canonical and document-relative asset URLs are served from the file
  const b1 = await (await win.fetch(`${CANON}wasm/build.json`)).json();
  assert.equal(b1.single_file, true);
  const b2 = await (await win.fetch('wasm/build.json')).json();
  assert.deepEqual(b1, b2);
  const icon = await win.fetch(new URL('icon.svg', 'file:///media/usb/embermesh.html'));
  assert.equal(icon.headers.get('content-type'), 'image/svg+xml');
  // unknown canonical URLs 404 locally (no network request, no console error)
  assert.equal((await win.fetch(`${CANON}hub.json`)).status, 404);
  assert.equal(realFetchCalls.length, 0);
  // other URLs go to the real fetch
  await win.fetch('https://example.org/x');
  assert.deepEqual(realFetchCalls, ['https://example.org/x']);
  if (b1.guides) assert.ok((await (await win.fetch(`${CANON}${b1.guides}`)).arrayBuffer()).byteLength > 0);
  for (const u of Object.values(im.imports)) URL.revokeObjectURL(u);
});
