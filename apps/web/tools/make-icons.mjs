#!/usr/bin/env node
// Generates the PWA / favicon images from docs/assets/logo.svg into apps/web/pwa/icons/.
// Run by hand when the logo changes (needs Chrome; the normal build only copies the results):
//
//   node apps/web/tools/make-icons.mjs        (CHROME=/path/to/chrome to override)
//
// Output (all PNGs carry only IHDR/PLTE/tRNS/IDAT/IEND: no text, time, EXIF or colour-profile chunks):
//   icon-192.png, icon-512.png            purpose "any": the round logo on transparent
//   maskable-192.png, maskable-512.png    purpose "maskable": full-bleed #17110f, logo inside the 80 % safe zone
//   monochrome-512.png                    purpose "monochrome": white glyph (alpha only) on transparent
//   apple-touch-icon.png (180)            opaque #17110f square (iOS ignores transparency)
//   favicon.svg, favicon-32.png           browser tab icons (SVG, PNG fallback; no .ico: the publishing
//                                         check cannot inspect .ico files for metadata)
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { findChrome, launchChrome, openPage } from '../test/lib/cdp.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '../../..');
const LOGO = path.join(ROOT, 'docs/assets/logo.svg');
const OUT = path.join(ROOT, 'apps/web/pwa/icons');
export const BG = '#17110f';

/** Drops every ancillary PNG chunk except tRNS (no tEXt/iTXt/zTXt/tIME/eXIf/iCCP/sRGB/pHYs…). */
export function stripPng(buf) {
  const sig = buf.subarray(0, 8);
  const keep = new Set(['IHDR', 'PLTE', 'tRNS', 'IDAT', 'IEND']);
  const parts = [sig];
  for (let o = 8; o < buf.length;) {
    const len = buf.readUInt32BE(o);
    const type = buf.toString('latin1', o + 4, o + 8);
    const end = o + 12 + len;
    if (keep.has(type)) parts.push(buf.subarray(o, end));
    o = end;
  }
  return Buffer.concat(parts);
}

/** PNG chunk types present (tests / the publishing check). */
export function pngChunks(buf) {
  const out = [];
  for (let o = 8; o < buf.length;) { const len = buf.readUInt32BE(o); out.push(buf.toString('latin1', o + 4, o + 8)); o += 12 + len; }
  return out;
}

async function main() {
  const chrome = findChrome();
  if (!chrome) throw new Error('Chrome not found (set CHROME=...)');
  const svg = fs.readFileSync(LOGO, 'utf8');
  // the glyph without its dark disc, for the monochrome icon
  const glyph = svg.replace(/<circle cx="64" cy="64" r="62"[^>]*\/>/, '');
  if (glyph === svg) throw new Error('logo.svg changed: update the background-disc pattern in make-icons.mjs');
  const uri = (s) => `data:image/svg+xml;base64,${Buffer.from(s).toString('base64')}`;
  const b = await launchChrome(chrome);
  try {
    const P = await openPage(b.cdp, 'about:blank');
    await P.send('Emulation.setDefaultBackgroundColorOverride', { color: { r: 0, g: 0, b: 0, a: 0 } });
    const render = async (size, { bg = 'transparent', scale = 1, src = uri(svg), filter = '' } = {}) => {
      await P.send('Emulation.setDeviceMetricsOverride', { width: size, height: size, deviceScaleFactor: 1, mobile: false });
      const px = Math.round(size * scale), off = Math.round((size - px) / 2);
      await P.evaluate(`new Promise((done) => {
        document.documentElement.style.cssText = 'margin:0;background:transparent';
        document.body.style.cssText = 'margin:0;width:${size}px;height:${size}px;overflow:hidden;background:${bg}';
        document.body.innerHTML = '<img id="i" style="position:absolute;left:${off}px;top:${off}px;width:${px}px;height:${px}px;${filter}">';
        const i = document.getElementById('i'); i.onload = () => requestAnimationFrame(() => requestAnimationFrame(done)); i.src = ${JSON.stringify(src)};
      })`);
      const r = await P.send('Page.captureScreenshot', { format: 'png', clip: { x: 0, y: 0, width: size, height: size, scale: 1 }, fromSurface: true });
      return stripPng(Buffer.from(r.data, 'base64'));
    };
    fs.mkdirSync(OUT, { recursive: true });
    const put = (name, data) => { fs.writeFileSync(path.join(OUT, name), data); console.log(`  ${name} ${data.length} B`); };
    for (const s of [192, 512]) put(`icon-${s}.png`, await render(s));
    for (const s of [192, 512]) put(`maskable-${s}.png`, await render(s, { bg: BG, scale: 0.8 }));
    put('monochrome-512.png', await render(512, { src: uri(glyph), filter: 'filter:brightness(0) invert(1)' }));
    put('apple-touch-icon.png', await render(180, { bg: BG, scale: 0.9 }));
    put('favicon-32.png', await render(32));
    // the SVG favicon: the logo itself, minus the accessibility title (a tab already shows the name)
    put('favicon.svg', Buffer.from(svg.replace(/\s*<title>[^<]*<\/title>/, '').replace(/ role="img" aria-label="[^"]*"/, '')));
    await P.close();
  } finally {
    await b.close();
  }
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main().catch((e) => { console.error(e.message || e); process.exit(1); });
}
