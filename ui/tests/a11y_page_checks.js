// In-page accessibility checks, injected by a11y_audit.mjs (classic script, defines
// window.__a11yChecks). Each check returns findings {rule, sev, sel, msg}. No dependencies.
(function () {
  'use strict';
  const SEV = { critical: 4, serious: 3, moderate: 2, minor: 1 };

  function sel(el) {
    if (!el || el.nodeType !== 1) return '';
    const parts = [];
    let e = el;
    for (let i = 0; e && e.nodeType === 1 && i < 5; i++, e = e.parentElement) {
      let s = e.tagName.toLowerCase();
      if (e.id && !/^(sheet|id|a11y)-/.test(e.id)) { parts.unshift(s + '#' + e.id); break; }
      const cls = [...e.classList].filter(c => !/^(st-|cat-|status-)/.test(c)).slice(0, 2);
      if (cls.length) s += '.' + cls.join('.');
      const lab = e.getAttribute('aria-label');
      if (lab && i === 0) s += '[aria-label="' + lab.slice(0, 30) + '"]';
      parts.unshift(s);
      if (e.id === 'main' || e.tagName === 'BODY') break;
    }
    return parts.join(' > ');
  }
  const text = el => String(el.textContent || '').replace(/\s+/g, ' ').trim();
  const shown = el => {
    if (!el || !el.isConnected) return false;
    if (el.checkVisibility && !el.checkVisibility({ checkOpacity: false, checkVisibilityCSS: true })) return false;
    const r = el.getBoundingClientRect();
    if (r.width < 1 || r.height < 1) return false;
    if (el.closest('.sr-only, .a11y-live, [aria-hidden="true"]') && !el.closest('.skip-link')) return false;
    return true;
  };
  const scope = () => [document.getElementById('app'), ...document.querySelectorAll('.overlay')].filter(Boolean);
  const all = q => scope().flatMap(r => [...r.querySelectorAll(q)]);

  // ------------------------------------------------------------------ colour
  function parse(c) {
    let m = /rgba?\(\s*([\d.]+)[,\s]+([\d.]+)[,\s]+([\d.]+)(?:\s*[,/]\s*([\d.]+%?))?\s*\)/.exec(c || '');
    if (m) { let a = m[4] === undefined ? 1 : m[4].endsWith('%') ? parseFloat(m[4]) / 100 : +m[4]; return [+m[1], +m[2], +m[3], a]; }
    m = /color\(srgb\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)(?:\s*\/\s*([\d.]+))?\)/.exec(c || '');
    if (m) return [m[1] * 255, m[2] * 255, m[3] * 255, m[4] === undefined ? 1 : +m[4]];
    return [0, 0, 0, 0];
  }
  const over = (top, bot) => { const a = top[3] + bot[3] * (1 - top[3]); if (!a) return [0, 0, 0, 0]; return [0, 1, 2].map(i => (top[i] * top[3] + bot[i] * bot[3] * (1 - top[3])) / a).concat(a); };
  const lum = c => { const f = v => { v /= 255; return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4); }; return 0.2126 * f(c[0]) + 0.7152 * f(c[1]) + 0.0722 * f(c[2]); };
  const ratio = (a, b) => { const x = lum(a), y = lum(b); return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05); };
  function background(el) {
    const layers = [];
    let opacity = 1;
    for (let e = el; e && e.nodeType === 1; e = e.parentElement) {
      const cs = getComputedStyle(e);
      opacity *= parseFloat(cs.opacity) || (cs.opacity === '0' ? 0 : 1);
      if (cs.backgroundImage && cs.backgroundImage !== 'none' && !/gradient/.test(cs.backgroundImage)) return { unknown: true };
      const bg = parse(cs.backgroundColor);
      if (bg[3] > 0) { layers.push(bg); if (bg[3] >= 1) break; }
    }
    let c = [255, 255, 255, 1];
    for (let i = layers.length - 1; i >= 0; i--) c = over(layers[i], c);
    return { color: c, opacity };
  }
  function contrastOf(el) {
    const cs = getComputedStyle(el);
    const bg = background(el);
    if (bg.unknown) return null;
    let fg = parse(cs.color);
    fg = over(fg, bg.color);
    if (bg.opacity < 1) fg = [0, 1, 2].map(i => fg[i] * bg.opacity + bg.color[i] * (1 - bg.opacity)).concat(1);
    const size = parseFloat(cs.fontSize);
    const bold = (parseInt(cs.fontWeight, 10) || 400) >= 700;
    const large = size >= 24 || (size >= 18.66 && bold);
    return { ratio: ratio(fg, bg.color), large, fg, bg: bg.color, size };
  }
  const hex = c => '#' + c.slice(0, 3).map(v => Math.round(v).toString(16).padStart(2, '0')).join('');

  function checkContrast(tag) {
    const out = [];
    const seen = new Set();
    for (const root of scope()) {
      const w = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, { acceptNode: n => n.nodeValue.trim() ? NodeFilter.FILTER_ACCEPT : NodeFilter.FILTER_REJECT });
      let n;
      while ((n = w.nextNode())) {
        const el = n.parentElement;
        if (!el || seen.has(el) || !shown(el) || el.closest('svg, .qr, :disabled, [aria-disabled="true"], .preview, video, .morse-cam')) continue;
        seen.add(el);
        const r = contrastOf(el);
        if (!r) continue;
        const need = r.large ? 3 : 4.5;
        if (r.ratio + 0.005 < need) {
          out.push({ rule: 'contrast', sev: r.ratio < 3 ? 'serious' : 'serious', sel: sel(el), msg: tag + ': ' + r.ratio.toFixed(2) + ':1 (needs ' + need + ') ' + hex(r.fg) + ' on ' + hex(r.bg) + ' “' + text(el).slice(0, 40) + '”', ratio: r.ratio });
        }
      }
    }
    return out;
  }

  // ------------------------------------------------------------------ names, labels, images
  function checkNamesImages() {
    const out = [];
    for (const svg of all('svg')) {
      if (!shown(svg) || svg.getAttribute('aria-hidden') === 'true' || svg.closest('[aria-hidden="true"]')) continue;
      const named = svg.getAttribute('aria-label') || svg.getAttribute('aria-labelledby') || svg.querySelector(':scope > title');
      if (!named) out.push({ rule: 'image-alt', sev: 'serious', sel: sel(svg), msg: 'SVG exposed to assistive tech without a text alternative (add role="img" + aria-label, or aria-hidden="true" if decorative)' });
      else if (svg.getAttribute('role') !== 'img' && !svg.closest('button, a')) out.push({ rule: 'image-alt', sev: 'minor', sel: sel(svg), msg: 'Labelled SVG without role="img"' });
    }
    for (const img of all('img, canvas')) {
      if (!shown(img)) continue;
      if (img.tagName === 'IMG' && !img.hasAttribute('alt')) out.push({ rule: 'image-alt', sev: 'serious', sel: sel(img), msg: 'img without alt' });
      if (img.tagName === 'CANVAS' && !img.getAttribute('aria-label') && !img.getAttribute('role')) out.push({ rule: 'image-alt', sev: 'moderate', sel: sel(img), msg: 'canvas without a text alternative' });
    }
    for (const el of all('input:not([type="hidden"]), select, textarea')) {
      if (!shown(el) && !(el.type === 'checkbox' || el.type === 'radio')) continue;
      const lbl = el.labels && el.labels.length ? [...el.labels].map(text).join(' ') : '';
      const aria = el.getAttribute('aria-label') || (el.getAttribute('aria-labelledby') || '').split(/\s+/).map(id => document.getElementById(id)).filter(Boolean).map(text).join(' ');
      if (!lbl && !aria) {
        if (el.placeholder) out.push({ rule: 'label', sev: 'moderate', sel: sel(el), msg: 'Field labelled only by its placeholder “' + el.placeholder.slice(0, 40) + '” (disappears when typing)' });
        else if (!el.title) out.push({ rule: 'label', sev: 'serious', sel: sel(el), msg: 'Form field without a label' });
      }
    }
    // tablist misuse: role=tab without tabpanel / arrow-key handling (ui.js segmented)
    for (const tl of all('[role="tablist"]')) {
      if (!shown(tl)) continue;
      const tabs = tl.querySelectorAll('[role="tab"]');
      const controlled = [...tabs].some(t => t.getAttribute('aria-controls'));
      if (!controlled) out.push({ rule: 'aria', sev: 'moderate', sel: sel(tl), msg: 'role="tablist" used for a choice control (no tabpanels, all tabs in the tab order): use radio buttons or aria-pressed buttons' });
    }
    // clickable things that are not focusable
    for (const el of all('div, span, li, figure, p, svg')) {
      if (!shown(el)) continue;
      const cs = getComputedStyle(el);
      if (cs.cursor !== 'pointer' || el.closest('a, button, label, summary, [tabindex]')) continue;
      if (el.parentElement && getComputedStyle(el.parentElement).cursor === 'pointer' && !el.parentElement.closest('a,button,label,summary')) continue;
      if (el.tabIndex < 0) out.push({ rule: 'keyboard', sev: 'serious', sel: sel(el), msg: 'Clickable (cursor:pointer) but not focusable or announced as a control' });
    }
    return out;
  }

  // ------------------------------------------------------------------ structure
  function checkStructure() {
    const out = [];
    const main = document.getElementById('main');
    if (!main || main.tagName !== 'MAIN') out.push({ rule: 'landmark', sev: 'moderate', sel: '#main', msg: 'No <main> landmark' });
    const nav = document.getElementById('nav');
    if (nav && !nav.hidden && nav.tagName !== 'NAV') out.push({ rule: 'landmark', sev: 'moderate', sel: '#nav', msg: 'Navigation is not a <nav> landmark' });
    const strip = document.getElementById('strip');
    if (strip && !strip.hidden && shown(strip) && !strip.closest('header, [role="banner"], [role="region"], [role="status"]')) out.push({ rule: 'landmark', sev: 'minor', sel: '#strip', msg: 'Status strip is outside any landmark (wrap in <header> or role="region" aria-label="Status")' });
    const hs = [...(main || document).querySelectorAll('h1, h2, h3, h4, h5, h6')].filter(shown);
    const h1 = hs.filter(x => x.tagName === 'H1');
    if (!h1.length) out.push({ rule: 'heading', sev: 'moderate', sel: '#main', msg: 'Screen has no h1' });
    if (h1.length > 1) out.push({ rule: 'heading', sev: 'minor', sel: sel(h1[1]), msg: h1.length + ' h1 headings on one screen' });
    let prev = 0;
    for (const x of hs) {
      const lv = +x.tagName[1];
      if (prev && lv > prev + 1) out.push({ rule: 'heading', sev: 'minor', sel: sel(x), msg: 'Heading level jumps from h' + prev + ' to h' + lv });
      prev = lv;
    }
    // section titles styled like headings but not headings
    for (const st of (main || document).querySelectorAll('.section-title')) if (shown(st) && !/^H\d$/.test(st.tagName) && st.getAttribute('role') !== 'heading') { out.push({ rule: 'heading', sev: 'moderate', sel: sel(st), msg: 'Section title “' + text(st).slice(0, 40) + '” looks like a heading but is a plain div (use h2/h3)' }); }
    const lives = [...document.querySelectorAll('[aria-live], [role="status"], [role="alert"], [role="log"]')];
    if (!lives.length) out.push({ rule: 'live', sev: 'moderate', sel: 'body', msg: 'No live region on the page' });
    // live regions nested in live regions double-announce
    for (const l of lives) if (l.parentElement && l.parentElement.closest('[aria-live]')) out.push({ rule: 'live', sev: 'minor', sel: sel(l), msg: 'Live region nested inside another live region' });
    for (const el of all('[tabindex]')) if (+el.getAttribute('tabindex') > 0) out.push({ rule: 'focus-order', sev: 'serious', sel: sel(el), msg: 'Positive tabindex changes the focus order' });
    return out;
  }

  // ------------------------------------------------------------------ targets, overflow, clipping, overlap
  const INTERACTIVE = 'a[href], button, input:not([type="hidden"]), select, textarea, summary, [role="button"], [role="tab"], [tabindex]:not([tabindex="-1"])';
  function targetOf(el) {
    if ((el.type === 'checkbox' || el.type === 'radio') && el.labels && el.labels[0]) return el.labels[0];
    return el;
  }
  function inlineLink(el) {
    if (el.tagName !== 'A' || el.classList.contains('btn') || el.classList.contains('item')) return false;
    const p = el.parentElement;
    if (!p || !/^inline/.test(getComputedStyle(el).display) || /flex|grid/.test(getComputedStyle(p).display)) return false;
    return text(p).length > text(el).length + 3;
  }
  /** Rect clipped to the scroll containers it sits in (#main scrolls; content under the nav is not on screen). */
  function visibleRect(el) {
    let r = el.getBoundingClientRect();
    let L = r.left, T = r.top, R = r.right, B = r.bottom;
    for (let e = el.parentElement; e && e !== document.body; e = e.parentElement) {
      const cs = getComputedStyle(e);
      if (/(auto|scroll|hidden|clip)/.test(cs.overflowY + cs.overflowX)) {
        const q = e.getBoundingClientRect();
        L = Math.max(L, q.left); T = Math.max(T, q.top); R = Math.min(R, q.right); B = Math.min(B, q.bottom);
      }
    }
    return { left: L, top: T, right: R, bottom: B, width: Math.max(0, R - L), height: Math.max(0, B - T) };
  }
  function checkTargets(min) {
    const out = [];
    const done = new Set();
    for (const el of all(INTERACTIVE)) {
      const t = targetOf(el);
      if (done.has(t) || !shown(t) || inlineLink(el) || el.closest('.skip-link') || el.disabled) continue;
      done.add(t);
      const r = t.getBoundingClientRect();
      const w = Math.round(r.width), hh = Math.round(r.height);
      if (w < min || hh < min) {
        out.push({ rule: 'target-size', sev: w < 24 || hh < 24 ? 'serious' : 'moderate', sel: sel(t), msg: w + '×' + hh + ' px (needs ' + min + '×' + min + ') “' + (t.getAttribute('aria-label') || text(t)).slice(0, 30) + '”' });
      }
    }
    return out;
  }
  function checkOverflow(tag) {
    const out = [];
    const de = document.documentElement;
    if (de.scrollWidth > de.clientWidth + 1) out.push({ rule: 'reflow', sev: 'serious', sel: 'html', msg: tag + ': page scrolls sideways (' + de.scrollWidth + ' > ' + de.clientWidth + ')' });
    const main = document.getElementById('main');
    if (main && main.scrollWidth > main.clientWidth + 1) {
      // find the widest culprits that are not intentional scrollers
      const vw = main.getBoundingClientRect().right;
      const bad = [...main.querySelectorAll('*')].filter(e => shown(e) && e.getBoundingClientRect().right > vw + 1 && !e.closest('pre, .table-wrap, .md-table-wrap, .chips, .copybox, .messages')).slice(0, 3);
      out.push({ rule: 'reflow', sev: 'serious', sel: bad.map(sel).join(' | ') || '#main', msg: tag + ': content wider than the screen (' + main.scrollWidth + ' > ' + main.clientWidth + ')' });
    }
    return out;
  }
  function checkClipped(tag) {
    const out = [];
    for (const root of scope()) {
      for (const el of root.querySelectorAll('*')) {
        if (!el.firstChild || !shown(el) || el.closest('pre, .copybox, .table-wrap, .md-table-wrap, .chips, .messages, textarea, select, svg, .morse-transcript, .meter-bar, .sr-only') || ['SELECT', 'TEXTAREA', 'INPUT'].includes(el.tagName)) continue;
        const own = [...el.childNodes].some(n => n.nodeType === 3 && n.nodeValue.trim());
        if (!own) continue;
        const cs = getComputedStyle(el);
        const clipX = (cs.overflowX !== 'visible' || cs.textOverflow === 'ellipsis') && el.scrollWidth > el.clientWidth + 1 && cs.overflowX !== 'auto' && cs.overflowX !== 'scroll';
        const clamp = cs.webkitLineClamp && cs.webkitLineClamp !== 'none';
        const clipY = (clamp || (cs.overflowY === 'hidden' || cs.overflowY === 'clip')) && el.scrollHeight > el.clientHeight + 2;
        if (clipX || clipY) out.push({ rule: 'clipped-text', sev: clamp ? 'minor' : 'serious', sel: sel(el), msg: tag + ': text cut off' + (clamp ? ' (line clamp)' : '') + ' “' + text(el).slice(0, 40) + '”' });
      }
    }
    return out;
  }
  function checkOverlap(tag) {
    const out = [];
    const els = all(INTERACTIVE).map(targetOf).filter((e, i, a) => a.indexOf(e) === i && shown(e) && !e.closest('.skip-link'));
    const rs = els.map(visibleRect);
    for (let i = 0; i < els.length; i++) for (let j = i + 1; j < els.length; j++) {
      if (els[i].contains(els[j]) || els[j].contains(els[i])) continue;
      const a = rs[i], b = rs[j];
      if (!a.width || !a.height || !b.width || !b.height) continue;
      const w = Math.min(a.right, b.right) - Math.max(a.left, b.left);
      const hh = Math.min(a.bottom, b.bottom) - Math.max(a.top, b.top);
      if (w > 4 && hh > 4) {
        // something fixed on top (toast) is not an overlap of the layout
        if (getComputedStyle(els[i]).position === 'fixed' || getComputedStyle(els[j]).position === 'fixed' || els[i].closest('.toasts') || els[j].closest('.toasts')) continue;
        out.push({ rule: 'overlap', sev: 'serious', sel: sel(els[i]) + ' ⟂ ' + sel(els[j]), msg: tag + ': controls overlap by ' + Math.round(w) + '×' + Math.round(hh) + ' px' });
      }
    }
    return out.slice(0, 10);
  }

  // ------------------------------------------------------------------ colour as the only signal
  function checkColourOnly() {
    const out = [];
    for (const d of all('.dot')) if (shown(d) && !text(d.parentElement || d).replace(text(d), '').trim()) out.push({ rule: 'colour-only', sev: 'moderate', sel: sel(d), msg: 'Status dot with no text next to it' });
    // delivered vs read ticks: same icon, colour differs
    const ticks = all('.msg [class^="status-"]').filter(shown);
    const kinds = new Set(ticks.map(t => t.className));
    if (kinds.has('status-delivered') && kinds.has('status-read')) out.push({ rule: 'colour-only', sev: 'moderate', sel: '.msg .status-read', msg: 'Read and delivered ticks differ only by colour (add a shape or a visible word)' });
    for (const a of all('a:not(.btn):not(.item):not(.s-item)')) {
      if (!shown(a) || !inlineLink(a)) continue;
      const cs = getComputedStyle(a);
      if (!/underline/.test(cs.textDecorationLine)) { out.push({ rule: 'colour-only', sev: 'moderate', sel: sel(a), msg: 'Link inside text without underline (colour is the only cue)' }); break; }
    }
    for (const s of all('.strip .s-item.ok, .strip .s-item.bad')) if (shown(s) && !s.getAttribute('aria-label') && !/(up|down|none)/i.test(text(s))) { out.push({ rule: 'colour-only', sev: 'minor', sel: sel(s), msg: 'Links up/down shown by green/red only: “' + text(s) + '”' }); break; }
    return out;
  }

  // ------------------------------------------------------------------ motion
  function checkMotion() {
    const out = [];
    const anims = document.getAnimations ? document.getAnimations().filter(a => a.playState === 'running') : [];
    for (const a of anims.slice(0, 5)) out.push({ rule: 'motion', sev: 'serious', sel: sel(a.effect && a.effect.target), msg: 'Animation running while data-motion="' + document.documentElement.dataset.motion + '": ' + (a.animationName || a.transitionProperty || a.constructor.name) });
    return out;
  }

  window.__a11yChecks = {
    SEV, contrastOf, ratio, parse, sel,
    /** Contrast of the theme tokens (text on backgrounds) for the current theme / contrast. */
    tokens() {
      const cs = getComputedStyle(document.documentElement);
      const v = k => { const d = document.createElement('i'); d.style.color = cs.getPropertyValue(k).trim(); document.body.appendChild(d); const c = parse(getComputedStyle(d).color); d.remove(); return c; };
      const pairs = [['--text', '--bg'], ['--text', '--surface'], ['--muted', '--bg'], ['--muted', '--surface'], ['--muted', '--surface2'], ['--accent', '--bg'], ['--accent-text', '--accent'], ['--ok', '--bg'], ['--warn', '--bg'], ['--bad', '--bg'], ['--bad', '--surface'], ['--info', '--bg'], ['--sos-text', '--sos'], ['--text', '--mine'], ['--mark-text', '--mark'], ['--line', '--bg'], ['--focus', '--bg']];
      return pairs.map(([f, b]) => ({ pair: f + ' on ' + b, ratio: Math.round(ratio(v(f), v(b)) * 100) / 100 }));
    },
    structure: checkStructure, names: checkNamesImages, contrast: checkContrast, targets: checkTargets,
    overflow: checkOverflow, clipped: checkClipped, overlap: checkOverlap, colourOnly: checkColourOnly, motion: checkMotion,
    focusInfo() {
      const el = document.activeElement;
      if (!el || el === document.body) return { sel: 'body', visible: false, body: true };
      if (!el.__a11yId) el.__a11yId = ++window.__a11yFocusSeq || (window.__a11yFocusSeq = 1);
      const cs = getComputedStyle(el);
      const ring = (cs.outlineStyle !== 'none' && parseFloat(cs.outlineWidth) >= 1) || (cs.boxShadow && cs.boxShadow !== 'none');
      const r = el.getBoundingClientRect();
      return { id: el.__a11yId, sel: sel(el), visible: ring, inNav: !!el.closest('#nav'), inMain: !!el.closest('#main'), inOverlay: !!el.closest('.overlay'), offscreen: r.bottom < 0 || r.top > innerHeight + 2000, w: r.width, hh: r.height, tag: el.tagName, skip: el.classList.contains('skip-link') };
    },
  };
})();
