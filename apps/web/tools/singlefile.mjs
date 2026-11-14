// Single-file bundler: turns the static site (dist/web) into ONE self-contained HTML file that
// works when double-clicked from a USB stick (file://, no server). Zero dependencies.
//
// How it works (browsers refuse to load ES modules from file:// URLs):
//   * every JS module is embedded as text in a <script type="text/x-ember-module"> element;
//   * a small classic bootstrap script turns each into a blob: URL and installs an import map that
//     maps a canonical URL  https://embermesh.invalid/<path>  -> blob: URL;
//   * module sources are rewritten so relative specifiers become canonical URLs, `import.meta.url`
//     becomes the canonical URL of that module, and dynamic `import(x)` becomes
//     `__emberImport(<canonical module url>, x)` (resolves computed specifiers at run time);
//   * other assets (JSON, SVG, CSS urls, optional guide pack) are embedded as base64 and served by
//     a fetch() shim for canonical or document-relative URLs;
//   * stylesheets are inlined as <style>, images/icons as data: URIs.
import fs from 'node:fs';
import crypto from 'node:crypto';
import path from 'node:path';

export const CANON = 'https://embermesh.invalid/';

const MIME = {
  '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.css': 'text/css',
  '.json': 'application/json', '.webmanifest': 'application/manifest+json', '.svg': 'image/svg+xml',
  '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.gif': 'image/gif', '.webp': 'image/webp',
  '.ico': 'image/x-icon', '.woff2': 'font/woff2', '.woff': 'font/woff', '.ttf': 'font/ttf', '.txt': 'text/plain',
  '.md': 'text/markdown', '.wasm': 'application/wasm', '.pack': 'application/octet-stream',
};
export const mimeOf = (p) => MIME[path.extname(p).toLowerCase()] || 'application/octet-stream';

const isRelative = (s) => s.startsWith('./') || s.startsWith('../') || s.startsWith('/');

/** Makes text safe inside a <script> element: no "</script" and no "<!--" sequences. */
export function escapeScript(text) {
  return text.replace(/<(?=\/script|!--)/gi, '\\x3C');
}

/** Rewrites a module's specifiers for the import map (see file header). */
export function rewriteModule(src, canonUrl) {
  const resolve = (spec) => (isRelative(spec) ? new URL(spec, canonUrl).href : spec);
  let out = src;
  // static: import x from '...';  import '...';  export {a} from '...';  export * from '...'
  out = out.replace(
    /(\bimport\s*(?:[\w$*{}\s,]+?\s*from\s*)?|\bexport\s*(?:\*(?:\s*as\s+[\w$]+)?|\{[^}]*\})\s*from\s*)(['"])([^'"\n]+)\2/g,
    (m, head, q, spec) => `${head}${q}${resolve(spec)}${q}`,
  );
  out = out.replace(/\bimport\.meta\.url\b/g, JSON.stringify(canonUrl));
  out = out.replace(/(^|[^\w$.])import\s*\(/g, (m, pre) => `${pre}__emberImport(${JSON.stringify(canonUrl)}, `);
  return out;
}

function walk(dir, base = dir, out = []) {
  for (const name of fs.readdirSync(dir).sort()) {
    if (name.startsWith('.')) continue;
    const p = path.join(dir, name);
    if (fs.statSync(p).isDirectory()) walk(p, base, out);
    else out.push(path.relative(base, p).split(path.sep).join('/'));
  }
  return out;
}

const BOOTSTRAP = `(function(){
"use strict";
var BASE=${JSON.stringify(CANON)};
var S=window.__EMBER_SINGLE__={base:BASE,assets:{},modules:{}};
var meta=JSON.parse(document.getElementById("ember-assets").textContent);
S.assets=meta.assets;S.version=meta.version;
var map={};
var els=document.querySelectorAll('script[type="text/x-ember-module"]');
for(var i=0;i<els.length;i++){var p=els[i].getAttribute("data-path");
 map[BASE+p]=URL.createObjectURL(new Blob([els[i].textContent],{type:"text/javascript"}));S.modules[p]=1;}
S.urls=map;
var im=document.createElement("script");im.type="importmap";var me=document.currentScript;if(me&&me.nonce)im.nonce=me.nonce;im.textContent=JSON.stringify({imports:map});
document.currentScript.after(im);
window.__emberImport=function(base,spec){var u;try{u=new URL(spec,base).href}catch(e){u=spec}return import(u)};
var cache={};
function bytes(p){if(cache[p])return cache[p];var a=S.assets[p],b64=a.b64;
 if(b64===undefined){var el=document.getElementById(a.el);b64=el?el.textContent.replace(/\\s+/g,""):""}
 var bin=atob(b64),u=new Uint8Array(bin.length);for(var i=0;i<bin.length;i++)u[i]=bin.charCodeAt(i);
 return cache[p]=u}
var docBase=new URL(".",location.href).href;
function lookup(url){var u;try{u=new URL(url,location.href)}catch(e){return null}
 var h=u.href.replace(/[?#].*$/,""),p=null;
 if(h.indexOf(BASE)===0)p=h.slice(BASE.length);else if(h.indexOf(docBase)===0)p=h.slice(docBase.length);else return null;
 try{p=decodeURIComponent(p)}catch(e){}
 return{path:p,canon:h.indexOf(BASE)===0}}
var realFetch=window.fetch.bind(window);
window.fetch=function(input,init){
 var url=typeof input==="string"?input:(input&&input.url)||String(input);var hit=lookup(url);
 if(hit&&S.assets[hit.path]){var a=S.assets[hit.path];
  return Promise.resolve(new Response(new Blob([bytes(hit.path)],{type:a.type}),{status:200,headers:{"content-type":a.type}}))}
 if(hit&&hit.canon)return Promise.resolve(new Response("",{status:404,statusText:"Not in single-file build"}));
 return realFetch(input,init)};
})();`;

function dataUri(file, buf) {
  return `data:${mimeOf(file)};base64,${buf.toString('base64')}`;
}

/** Inline url(...) references and @imports of a stylesheet. */
function inlineCss(css, cssPath, siteDir, depth = 0) {
  const dir = path.posix.dirname(cssPath);
  css = css.replace(/@import\s+(?:url\()?\s*['"]?([^'")\s]+)['"]?\s*\)?\s*;/g, (m, ref) => {
    if (/^(data:|https?:|\/\/)/.test(ref) || depth > 8) return m;
    const p = path.posix.normalize(path.posix.join(dir, ref));
    const f = path.join(siteDir, p);
    return fs.existsSync(f) ? inlineCss(fs.readFileSync(f, 'utf8'), p, siteDir, depth + 1) : m;
  });
  return css.replace(/url\(\s*(['"]?)([^'")]+)\1\s*\)/g, (m, q, ref) => {
    if (/^(data:|https?:|\/\/|#)/.test(ref)) return m;
    const p = path.posix.normalize(path.posix.join(dir, ref.split(/[?#]/)[0]));
    const f = path.join(siteDir, p);
    return fs.existsSync(f) ? `url("${dataUri(p, fs.readFileSync(f))}")` : m;
  });
}

/**
 * @param {object} o
 * @param {string} o.siteDir       directory with the static site (index.html at its root)
 * @param {string} o.indexHtml     HTML text of the page to bundle (defaults to siteDir/index.html)
 * @param {string} o.singleWasm    path of ember.single.mjs (wasm embedded); replaces wasm/ember.mjs
 * @param {string|null} o.guides   path of guides.pack to inline, or null
 * @param {string[]} o.exclude     site-relative paths not to embed
 * @param {object} o.buildInfo     wasm/build.json contents for this variant
 * @returns {string} the HTML
 */
export function buildSingleFile({ siteDir, indexHtml, singleWasm, guides = null, exclude = [], buildInfo = {}, version = '' }) {
  let html = indexHtml ?? fs.readFileSync(path.join(siteDir, 'index.html'), 'utf8');
  const files = walk(siteDir).filter((f) => !exclude.includes(f) && f !== 'index.html');
  const modules = [];
  const assets = {};
  for (const f of files) {
    if (f === 'wasm/ember.mjs' || f === 'wasm/ember.wasm') continue;
    const full = path.join(siteDir, f);
    if (/\.m?js$/.test(f)) modules.push([f, fs.readFileSync(full, 'utf8')]);
    else if (f === 'wasm/build.json') continue;
    else if (f.startsWith('data/') && f.endsWith('.pack')) continue;
    else assets[f] = { type: mimeOf(f), b64: fs.readFileSync(full).toString('base64') };
  }
  modules.push(['wasm/ember.mjs', fs.readFileSync(singleWasm, 'utf8')]);
  assets['wasm/build.json'] = { type: 'application/json', b64: Buffer.from(JSON.stringify(buildInfo)).toString('base64') };
  let guidesEl = '';
  if (guides) {
    assets['data/guides.pack'] = { type: 'application/octet-stream', el: 'ember-guides' };
    guidesEl = `<script type="application/octet-stream" id="ember-guides">${fs.readFileSync(guides).toString('base64')}</script>\n`;
  }

  const moduleEls = modules
    .map(([p, src]) => `<script type="text/x-ember-module" data-path="${p}">${escapeScript(rewriteModule(src, CANON + p))}</script>`)
    .join('\n');
  const assetJson = JSON.stringify({ version, assets }).replace(/</g, '\\u003c');

  // --- rewrite the page -----------------------------------------------------------------------
  html = html.replace(/<link\b[^>]*\brel=["']?manifest["']?[^>]*>\s*/gi, '');
  html = html.replace(/<script\b[^>]*\bsrc=["']?pwa\.js["']?[^>]*>\s*<\/script>\s*/gi, '');
  // stylesheets
  html = html.replace(/<link\b([^>]*)>/gi, (m, attrs) => {
    const rel = (attrs.match(/\brel=["']?([^"'\s>]+)/i) || [])[1] || '';
    const href = (attrs.match(/\bhref=["']?([^"'\s>]+)/i) || [])[1];
    if (!href || /^(data:|https?:|\/\/)/.test(href)) return m;
    const p = path.posix.normalize(href.replace(/^\.\//, '').replace(/^\//, '').split(/[?#]/)[0]);
    const f = path.join(siteDir, p);
    if (!fs.existsSync(f)) return m;
    if (/stylesheet/i.test(rel)) return `<style>${inlineCss(fs.readFileSync(f, 'utf8'), p, siteDir).replace(/<\/style/gi, '<\\/style')}</style>`;
    if (/modulepreload|preload|prefetch/i.test(rel)) return '';
    return m.replace(href, dataUri(p, fs.readFileSync(f)));
  });
  // module / classic scripts with src
  html = html.replace(/<script\b([^>]*)\bsrc=["']?([^"'\s>]+)["']?([^>]*)>\s*<\/script>/gi, (m, a1, src, a2) => {
    if (/^(data:|https?:|\/\/)/.test(src)) return m;
    const p = path.posix.normalize(src.replace(/^\.\//, '').replace(/^\//, '').split(/[?#]/)[0]);
    const attrs = `${a1} ${a2}`;
    if (/type=["']?module/i.test(attrs)) return `<script type="module">import ${JSON.stringify(CANON + p)};</script>`;
    const f = path.join(siteDir, p);
    if (!fs.existsSync(f)) return m;
    return `<script>${escapeScript(fs.readFileSync(f, 'utf8'))}</script>`;
  });
  // inline module scripts
  html = html.replace(/<script\b([^>]*\btype=["']?module["']?[^>]*)>([\s\S]*?)<\/script>/gi, (m, attrs, body) => {
    if (/\bsrc=/.test(attrs) || !body.trim() || body.includes(CANON)) return m;
    return `<script${attrs}>${rewriteModule(body, CANON + 'index.html')}</script>`;
  });
  // images
  html = html.replace(/(<img\b[^>]*\bsrc=)(["'])([^"']+)\2/gi, (m, head, q, src) => {
    if (/^(data:|https?:|\/\/)/.test(src)) return m;
    const p = path.posix.normalize(src.replace(/^\.\//, ''));
    const f = path.join(siteDir, p);
    return fs.existsSync(f) ? `${head}${q}${dataUri(p, fs.readFileSync(f))}${q}` : m;
  });

  // Deterministic nonce (reproducible builds) for the one element created at run time: the import map.
  const nonce = crypto.createHash('sha256').update(assetJson).update(moduleEls).digest('base64').slice(0, 22).replace(/[+/]/g, '_');
  const head = `<script type="application/json" id="ember-assets">${assetJson}</script>\n${moduleEls}\n<script nonce="${nonce}">${escapeScript(BOOTSTRAP)}</script>\n`;
  if (!/<meta\s+charset/i.test(html)) html = html.replace(/<head[^>]*>/i, (m) => `${m}\n<meta charset="utf-8">`);
  // The bootstrap must run before any module script, under the page's CSP if it has one.
  const cspRe = /<meta\s+http-equiv=["']Content-Security-Policy["'][^>]*>/i;
  if (cspRe.test(html)) html = html.replace(cspRe, (m) => `${m}\n${head}`);
  else if (/<meta\s+charset[^>]*>/i.test(html)) html = html.replace(/<meta\s+charset[^>]*>/i, (m) => `${m}\n${head}`);
  else html = html.replace(/<head[^>]*>/i, (m) => `${m}\n${head}`);
  if (guidesEl) html = html.replace(/<\/body>/i, `${guidesEl}</body>`);
  return rewriteCsp(html, nonce);
}

const NON_JS_TYPES = /^(text\/x-ember-module|application\/json|application\/octet-stream|text\/plain)$/i;

/**
 * A strict CSP (script-src 'self'; style-src 'self') blocks everything a single file inlines. Allow
 * exactly what this file contains: sha256 hashes of each inline script and style, blob: for the
 * module/worklet blob URLs, and the nonce of the bootstrap (which gives it to the import map).
 */
export function rewriteCsp(html, nonce) {
  const re = /(<meta\s+http-equiv=["']Content-Security-Policy["']\s+content=)(["'])((?:(?!\2)[\s\S])*)\2/i;
  const m = html.match(re);
  if (!m) return html;
  const sha = (t) => `'sha256-${crypto.createHash('sha256').update(t, 'utf8').digest('base64')}'`;
  const scriptHashes = new Set();
  for (const s of html.matchAll(/<script\b([^>]*)>([\s\S]*?)<\/script>/gi)) {
    const type = (s[1].match(/\btype=["']?([^"'\s>]+)/i) || [])[1] || '';
    if (/\bsrc=/i.test(s[1]) || NON_JS_TYPES.test(type) || /\bnonce=/.test(s[1]) || !s[2].trim()) continue;
    scriptHashes.add(sha(s[2]));
  }
  const styleHashes = new Set([...html.matchAll(/<style\b[^>]*>([\s\S]*?)<\/style>/gi)].map((s) => sha(s[1])));
  const dirs = m[3].split(';').map((d) => d.trim()).filter(Boolean).map((d) => d.split(/\s+/));
  const get = (name) => {
    let d = dirs.find((x) => x[0] === name);
    if (!d) {
      const def = dirs.find((x) => x[0] === 'default-src');
      d = [name, ...(def ? def.slice(1) : ["'self'"])];
      dirs.push(d);
    }
    return d;
  };
  const add = (d, ...vals) => { for (const v of vals) if (!d.includes(v)) d.push(v); };
  add(get('script-src'), 'blob:', `'nonce-${nonce}'`, ...scriptHashes);
  if (styleHashes.size) add(get('style-src'), ...styleHashes);
  const content = dirs.map((d) => d.join(' ')).join('; ');
  return html.replace(re, (all, head, q) => `${head}${q}${content}${q}`);
}
