// Motion system rules (css/app.css "Motion" section, js/lib/motion.js):
//  - every animation/transition is gated on html[data-motion="full"|"reduced"] (off, low power
//    and a missing attribute animate nothing), and the global kill switches exist;
//  - reduced motion only fades (its keyframes touch opacity alone);
//  - keyframes animate transform/opacity only; transitions never touch layout properties;
//  - CSS that would break Chrome 80-era WebViews is not used;
//  - pure helpers (level resolution, route direction, signal level) behave.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { resolveLevel, routeDirection } from '../js/lib/motion.js';
import { signalLevel } from '../js/lib/ui.js';

const UI = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const CSS = fs.readFileSync(path.join(UI, 'css/app.css'), 'utf8');

/** Minimal CSS parser: leaf rules {selector, decls, at: [enclosing at-rule preludes]} and keyframes. */
export function parseCss(src) {
  const css = src.replace(/\/\*[\s\S]*?\*\//g, '');
  const rules = [], keyframes = new Map();
  let i = 0;
  function block(at) {
    while (i < css.length) {
      const open = css.indexOf('{', i), close = css.indexOf('}', i);
      if (close >= 0 && (open < 0 || close < open)) { i = close + 1; return; }
      if (open < 0) { i = css.length; return; }
      const prelude = css.slice(i, open).trim();
      i = open + 1;
      if (/^@keyframes\s/.test(prelude)) {
        const name = prelude.split(/\s+/)[1];
        let depth = 1, j = i;
        while (depth && j < css.length) { if (css[j] === '{') depth++; else if (css[j] === '}') depth--; j++; }
        keyframes.set(name, css.slice(i, j - 1));
        i = j;
      } else if (prelude.startsWith('@')) block([...at, prelude]);
      else {
        const end = css.indexOf('}', i);
        rules.push({ selector: prelude, decls: css.slice(i, end), at });
        i = end + 1;
      }
    }
  }
  block([]);
  return { rules, keyframes };
}

const decls = body => body.split(';').map(d => d.trim()).filter(Boolean).map(d => {
  const k = d.indexOf(':');
  return [d.slice(0, k).trim().toLowerCase(), d.slice(k + 1).trim()];
});
const { rules, keyframes } = parseCss(CSS);
const GATED = /^(?::root|html)?\[data-motion="(full|reduced)"\]/;
const LAYOUT = /^(width|height|top|left|right|bottom|margin|padding|inset|font-size|line-height|letter-spacing|border-width|max-|min-|flex|grid|gap)/;

test('every animation and transition in app.css is gated on data-motion', () => {
  let animated = 0;
  for (const r of rules) {
    for (const [k, v] of decls(r.decls)) {
      if (!/^(animation|transition)(-name|-duration)?$/.test(k)) continue;
      if (/^none\b/.test(v)) continue; // kill switches and resets
      if (k.endsWith('-duration')) continue;
      animated++;
      for (const sel of r.selector.split(',').map(s => s.trim())) {
        assert.match(sel, GATED, `ungated ${k} on "${sel}" — prefix it with [data-motion="full"] (or "reduced")`);
      }
    }
  }
  assert.ok(animated > 30, 'expected the motion section to define animations, found ' + animated);
});

test('off, low power and hidden page switch motion off; reduced stops scrolling animation', () => {
  const kill = rules.filter(r => /animation:\s*none\s*!important/.test(r.decls) && /transition:\s*none\s*!important/.test(r.decls));
  const sels = kill.map(r => r.selector).join(',');
  for (const s of ['[data-motion="off"] *', '[data-motion="off"] *::before', '[data-lowpower="1"] *', '[data-lowpower="1"] *::after']) assert.ok(sels.includes(s), s);
  assert.ok(rules.some(r => r.selector.includes('[data-hidden="1"] *') && /animation-play-state:\s*paused\s*!important/.test(r.decls)), 'hidden pause');
  assert.ok(rules.some(r => r.selector.includes('::view-transition') && /pointer-events:\s*none/.test(r.decls)), 'view transitions must let taps through');
});

test('keyframes animate only transform and opacity', () => {
  assert.ok(keyframes.size >= 15);
  for (const [name, body] of keyframes) {
    for (const m of body.matchAll(/\{([^}]*)\}/g)) {
      for (const [k] of decls(m[1])) assert.ok(['transform', 'opacity'].includes(k), `@keyframes ${name} animates ${k}`);
    }
  }
});

test('every animation used exists; reduced motion only fades', () => {
  const names = v => v.split(',').map(p => p.trim().split(/\s+/).find(t => keyframes.has(t) || /^em-|^spin$/.test(t))).filter(Boolean);
  const fadeOnly = n => [...keyframes.get(n).matchAll(/\{([^}]*)\}/g)].every(m => decls(m[1]).every(([k]) => k === 'opacity'));
  for (const r of rules) {
    for (const [k, v] of decls(r.decls)) {
      if (!/^animation(-name)?$/.test(k) || /^none/.test(v)) continue;
      for (const n of names(v)) {
        assert.ok(keyframes.has(n), `${r.selector}: unknown keyframes ${n}`);
        if (/\[data-motion="reduced"\]/.test(r.selector)) assert.ok(fadeOnly(n), `reduced motion uses ${n} (not a fade) on ${r.selector}`);
      }
    }
  }
});

test('transitions never animate layout properties', () => {
  for (const r of rules) {
    for (const [k, v] of decls(r.decls)) {
      if (k === 'transition-property') for (const p of v.split(',')) assert.doesNotMatch(p.trim(), LAYOUT, r.selector);
      if (k !== 'transition' || /^none/.test(v)) continue;
      for (const part of v.split(/,(?![^(]*\))/)) assert.doesNotMatch(part.trim(), LAYOUT, `${r.selector}: transition ${part}`);
    }
  }
});

test('no CSS that breaks Chrome 80-era WebViews', () => {
  const src = CSS.replace(/\/\*[\s\S]*?\*\//g, '');
  assert.doesNotMatch(src, /:(is|where|has)\(/, ':is()/:where()/:has() need Chrome 88+');
  assert.doesNotMatch(src, /(^|[;{\s])inset\s*:/m, 'inset needs Chrome 87; use top/right/bottom/left');
  for (const r of rules) if (/aspect-ratio\s*:/.test(r.decls)) assert.ok(r.at.some(a => /@supports/.test(a)), 'aspect-ratio outside @supports: ' + r.selector);
  assert.doesNotMatch(src, /color-mix\(/);
  // font sizes follow --text-scale: rem/em/var only (SVG text in the radar is in viewBox units)
  for (const r of rules) for (const [k, v] of decls(r.decls)) if (k === 'font-size' && /\d+px/.test(v)) assert.match(r.selector, /\.radar/, `px font-size on ${r.selector}`);
});

test('night theme stays red: no other hues in its tokens', () => {
  const night = rules.find(r => r.selector.trim() === '[data-theme="night"]');
  for (const [k, v] of decls(night.decls)) {
    for (const m of v.matchAll(/#([0-9a-f]{6})\b/gi)) {
      const [r, g, b] = [0, 2, 4].map(o => parseInt(m[1].slice(o, o + 2), 16));
      assert.ok(Math.max(g, b) <= 70 || r >= 2.2 * Math.max(g, b), `${k}: ${m[0]} is not red/black`);
    }
    for (const m of v.matchAll(/rgb\((\d+) (\d+) (\d+)/g)) assert.ok(Math.max(m[2], m[3]) <= 70 || m[1] >= 2.2 * Math.max(m[2], m[3]), `${k}: ${m[0]}`);
  }
});

test('motion level resolution', () => {
  assert.equal(resolveLevel({ attr: 'full' }), 'full');
  assert.equal(resolveLevel({ attr: 'reduced' }), 'reduced');
  assert.equal(resolveLevel({ attr: 'full', lowPower: true }), 'off');
  assert.equal(resolveLevel({ attr: 'full', hidden: true }), 'off');
  assert.equal(resolveLevel({}), 'full');
  assert.equal(resolveLevel({ reducedMedia: true }), 'reduced');
  assert.equal(resolveLevel({ webdriver: true }), 'off');
  assert.equal(resolveLevel({ query: '?mock=1&motion=off' }), 'off');
  assert.equal(resolveLevel({ attr: 'bogus', reducedMedia: true }), 'reduced');
});

test('route direction: back, forward, tab', () => {
  assert.equal(routeDirection(null, '#/chats', []), 'none');
  assert.equal(routeDirection('#/chats', '#/chats/dm%3Aab', ['#/chats']), 'fwd');
  assert.equal(routeDirection('#/chats/dm%3Aab', '#/chats', ['#/x', '#/y']), 'back');
  assert.equal(routeDirection('#/guides/read/a', '#/sos', ['#/guides', '#/guides/read/a']), 'tab');
  assert.equal(routeDirection('#/sos', '#/guides/read/a', ['#/guides/read/a', '#/sos']), 'back');
  assert.equal(routeDirection('#/tools?x=1', '#/tools/run/dipole', []), 'fwd');
});

test('signal level from SNR or RSSI', () => {
  assert.equal(signalLevel(8, null), 4);
  assert.equal(signalLevel(-3, -120), 2);
  assert.equal(signalLevel(undefined, -65), 4);
  assert.equal(signalLevel(undefined, -118), 0);
  assert.equal(signalLevel(undefined, undefined), null);
});
