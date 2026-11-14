#!/usr/bin/env node
// Packages the web app: dist/web/ static site + PWA, and dist/web/embermesh.html single file.
// Zero dependencies; deterministic output for identical inputs (no timestamps).
//
//   node apps/web/tools/package.mjs [--ui ui] [--wasm build/web/wasm] [--out dist/web]
//        [--guides path/to/guides.pack] [--inline-guides] [--no-single]
// Env equivalents: EMBER_GUIDES_PACK, EMBER_INLINE_GUIDES=1.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { buildSingleFile } from './singlefile.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const webDir = path.resolve(here, '..');
const root = path.resolve(webDir, '../..');

function parseArgs(argv) {
  const a = { ui: 'ui', wasm: 'build/web/wasm', out: 'dist/web', guides: process.env.EMBER_GUIDES_PACK || '', inlineGuides: process.env.EMBER_INLINE_GUIDES === '1', single: true };
  for (let i = 0; i < argv.length; i++) {
    const k = argv[i];
    const v = () => argv[++i];
    if (k === '--ui') a.ui = v();
    else if (k === '--wasm') a.wasm = v();
    else if (k === '--out') a.out = v();
    else if (k === '--guides') a.guides = v();
    else if (k === '--inline-guides') a.inlineGuides = true;
    else if (k === '--no-single') a.single = false;
    else { console.error(`unknown argument ${k}`); process.exit(2); }
  }
  for (const k of ['ui', 'wasm', 'out']) a[k] = path.resolve(root, a[k]);
  if (a.guides) a.guides = path.resolve(process.cwd(), a.guides);
  return a;
}

function copyTree(src, dst) {
  fs.mkdirSync(dst, { recursive: true });
  for (const name of fs.readdirSync(src).sort()) {
    // dotfiles and the UI's own test files are not part of the app
    if (name.startsWith('.') || /\.test\.m?js$/.test(name)) continue;
    const s = path.join(src, name), d = path.join(dst, name);
    if (fs.statSync(s).isDirectory()) { if (name !== 'tests' && name !== 'test') copyTree(s, d); }
    else fs.copyFileSync(s, d);
  }
}

function walk(dir, base = dir, out = []) {
  for (const name of fs.readdirSync(dir).sort()) {
    const p = path.join(dir, name);
    if (fs.statSync(p).isDirectory()) walk(p, base, out);
    else out.push(path.relative(base, p).split(path.sep).join('/'));
  }
  return out;
}

function projectVersion() {
  const m = fs.readFileSync(path.join(root, 'CMakeLists.txt'), 'utf8').match(/project\(\s*\w+\s+VERSION\s+([\d.]+)/);
  return m ? m[1] : '0.0.0';
}

export const THEME = '#17110f'; // manifest theme/background colour == the boot screen's first frame

function injectPwa(html) {
  const add = [];
  if (!/rel=["']?manifest/i.test(html)) add.push('<link rel="manifest" href="manifest.webmanifest">');
  // App identity: SVG favicon with a PNG fallback, Apple touch icon; relative URLs (sub-path safe).
  if (!/href=["']?icons\/favicon\.svg/i.test(html)) add.push('<link rel="icon" href="icons/favicon.svg" type="image/svg+xml">');
  if (!/href=["']?icons\/favicon-32\.png/i.test(html)) add.push('<link rel="icon" href="icons/favicon-32.png" sizes="32x32" type="image/png">');
  if (!/rel=["']?apple-touch-icon/i.test(html)) add.push('<link rel="apple-touch-icon" href="icons/apple-touch-icon.png">');
  if (!/name=["']?apple-mobile-web-app-capable/i.test(html)) add.push('<meta name="apple-mobile-web-app-capable" content="yes">');
  if (!/name=["']?mobile-web-app-capable/i.test(html)) add.push('<meta name="mobile-web-app-capable" content="yes">');
  if (!/name=["']?apple-mobile-web-app-title/i.test(html)) add.push('<meta name="apple-mobile-web-app-title" content="EmberMesh">');
  if (!/name=["']?theme-color/i.test(html)) add.push(`<meta name="theme-color" content="${THEME}">`);
  else html = html.replace(/(<meta\s+name=["']?theme-color["']?\s+content=["'])#000000(["'])/i, `$1${THEME}$2`);
  if (!/pwa\.js/.test(html)) add.push('<script src="pwa.js" defer></script>');
  if (!add.length) return html;
  return html.replace(/<\/head>/i, `${add.join('\n')}\n</head>`);
}

const kb = (n) => `${(n / 1024).toFixed(1)} KiB`;

function main() {
  const a = parseArgs(process.argv.slice(2));
  const out = a.out;
  for (const f of ['ember.mjs', 'ember.wasm', 'ember.single.mjs']) {
    if (!fs.existsSync(path.join(a.wasm, f))) { console.error(`missing ${path.join(a.wasm, f)} — build the WASM first (apps/web/build.sh)`); process.exit(1); }
  }
  fs.rmSync(out, { recursive: true, force: true });
  fs.mkdirSync(out, { recursive: true });

  // 1. the shared UI tree
  let placeholder = false;
  if (fs.existsSync(a.ui)) copyTree(a.ui, out);
  if (!fs.existsSync(path.join(out, 'index.html'))) {
    fs.copyFileSync(path.join(webDir, 'site/index.html'), path.join(out, 'index.html'));
    placeholder = true;
  }
  const originalIndex = fs.readFileSync(path.join(out, 'index.html'), 'utf8');

  // 2. WASM core
  fs.mkdirSync(path.join(out, 'wasm'), { recursive: true });
  fs.copyFileSync(path.join(a.wasm, 'ember.mjs'), path.join(out, 'wasm/ember.mjs'));
  fs.copyFileSync(path.join(a.wasm, 'ember.wasm'), path.join(out, 'wasm/ember.wasm'));
  // optional WebAssembly-SIMD build of the same core (site/PWA only; the single file stays baseline)
  const simd = fs.existsSync(path.join(a.wasm, 'ember-simd.mjs')) && fs.existsSync(path.join(a.wasm, 'ember-simd.wasm'));
  if (simd) for (const f of ['ember-simd.mjs', 'ember-simd.wasm']) fs.copyFileSync(path.join(a.wasm, f), path.join(out, 'wasm', f));

  // 3. guide pack (optional)
  let guides = null;
  let guidesHash = null;
  let guidesBytes = 0;
  if (a.guides) {
    if (!fs.existsSync(a.guides)) { console.error(`guide pack not found: ${a.guides}`); process.exit(1); }
    fs.mkdirSync(path.join(out, 'data'), { recursive: true });
    fs.copyFileSync(a.guides, path.join(out, 'data/guides.pack'));
    guides = 'data/guides.pack';
    // The pack (tens of MB) has its own content-addressed cache so an app update that does not
    // change the library never downloads it again (see pwa/sw.js and ui/js/backends/wasm.js).
    guidesHash = crypto.createHash('sha256').update(fs.readFileSync(a.guides)).digest('hex').slice(0, 16);
    guidesBytes = fs.statSync(a.guides).size;
  }

  // 4. hub discovery default: a dumb static server answers "not a hub"; a hub serves its own.
  fs.writeFileSync(path.join(out, 'hub.json'), JSON.stringify({ embermesh_hub: false }) + '\n');

  // 5. PWA files
  for (const f of ['manifest.webmanifest', 'icon.svg', 'pwa.js']) fs.copyFileSync(path.join(webDir, 'pwa', f), path.join(out, f));
  fs.mkdirSync(path.join(out, 'icons'), { recursive: true });
  for (const f of fs.readdirSync(path.join(webDir, 'pwa/icons'))) fs.copyFileSync(path.join(webDir, 'pwa/icons', f), path.join(out, 'icons', f));
  fs.writeFileSync(path.join(out, 'index.html'), injectPwa(originalIndex));

  // 6. build info + content hash version (over everything except the files derived from it)
  const version = projectVersion();
  const hash = crypto.createHash('sha256');
  for (const f of walk(out)) { hash.update(f); hash.update(fs.readFileSync(path.join(out, f))); }
  const build = hash.digest('hex').slice(0, 12);

  // 7. single self-contained file
  let singleSize = 0;
  if (a.single) {
    const html = buildSingleFile({
      siteDir: out,
      indexHtml: originalIndex,
      singleWasm: path.join(a.wasm, 'ember.single.mjs'),
      guides: a.inlineGuides && guides ? path.join(out, guides) : null,
      exclude: ['sw.js', 'pwa.js', 'manifest.webmanifest', 'hub.json', 'embermesh.html', 'wasm/ember-simd.mjs', 'wasm/ember-simd.wasm'],
      buildInfo: { version, build, guides: a.inlineGuides && guides ? guides : null, guides_bytes: a.inlineGuides && guides ? guidesBytes : 0, single_file: true, simd: false },
      version: `${version}-${build}`,
    });
    fs.writeFileSync(path.join(out, 'embermesh.html'), html);
    singleSize = Buffer.byteLength(html);
  }

  // 8. build manifest: every file an offline install stores (size + sha256), see RPC_API §9 and
  //    ui/js/endpoints/offline.js. Not listed: the service worker (the browser fetches it itself),
  //    this manifest (stored next to the files) and the guide pack (stored in verified chunks).
  const files = walk(out)
    .filter((f) => f !== 'sw.js' && f !== 'wasm/build.json' && f !== guides)
    .map((f) => { const b = fs.readFileSync(path.join(out, f)); return { path: f, size: b.length, sha256: crypto.createHash('sha256').update(b).digest('hex') }; });
  const totalBytes = files.reduce((n, f) => n + f.size, 0) + guidesBytes;
  const buildInfo = { version, build, simd, guides, guides_hash: guidesHash, guides_bytes: guidesBytes, total_bytes: totalBytes, files };
  fs.writeFileSync(path.join(out, 'wasm/build.json'), JSON.stringify(buildInfo) + '\n');

  // 9. service worker: logic only; which build it serves is decided at run time (see pwa/sw.js)
  fs.copyFileSync(path.join(webDir, 'pwa/sw.js'), path.join(out, 'sw.js'));

  const size = (f) => fs.statSync(path.join(out, f)).size;
  let total = 0;
  for (const f of walk(out)) if (f !== 'embermesh.html') total += size(f);
  console.log(`dist: ${path.relative(root, out)}  version ${version}-${build}${placeholder ? '  (placeholder index.html: ui/index.html not found)' : ''}`);
  console.log(`  wasm/ember.wasm ${kb(size('wasm/ember.wasm'))}${simd ? `, wasm/ember-simd.wasm ${kb(size('wasm/ember-simd.wasm'))}` : ''}, wasm/ember.mjs ${kb(size('wasm/ember.mjs'))}, site total ${kb(total)}`);
  if (guides) console.log(`  data/guides.pack ${kb(size(guides))}${a.inlineGuides ? ' (inlined in single file)' : ''}`);
  if (a.single) console.log(`  embermesh.html ${kb(singleSize)} (single file)`);
}

main();
