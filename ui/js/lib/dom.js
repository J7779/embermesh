// Tiny DOM builder. All text goes through text nodes / setAttribute — never innerHTML.
//
//   h('button', {class: 'btn', onclick: fn, 'aria-label': 'Send'}, 'Send')
//
// props: class/className, style (object), dataset (object), on<event> (function),
// value/checked/disabled/hidden/selected (properties), everything else → setAttribute.
// children: strings/numbers (text nodes), Nodes, arrays (flattened), null/false (skipped).

const PROP_KEYS = new Set(['value', 'checked', 'disabled', 'hidden', 'selected', 'readOnly', 'multiple', 'indeterminate', 'open']);
const URL_ATTRS = new Set(['href', 'src', 'action', 'formaction', 'xlink:href', 'srcdoc']);

export function h(tag, props, ...children) {
  const el = document.createElement(tag);
  if (props) applyProps(el, props);
  append(el, children);
  return el;
}

export function applyProps(el, props) {
  for (const k in props) {
    const v = props[k];
    if (v == null || v === false && !PROP_KEYS.has(k)) continue;
    if (k === 'class' || k === 'className') el.className = Array.isArray(v) ? v.filter(Boolean).join(' ') : v;
    else if (k === 'style' && typeof v === 'object') Object.assign(el.style, v);
    else if (k === 'dataset') Object.assign(el.dataset, v);
    else if (k.startsWith('on') && typeof v === 'function') el.addEventListener(k.slice(2).toLowerCase(), v);
    else if (PROP_KEYS.has(k)) el[k] = v;
    else if (k === 'ref' && typeof v === 'function') v(el);
    else {
      const s = String(v);
      // Defence in depth: never allow script-ish URLs even from trusted code paths.
      if (URL_ATTRS.has(k) && !isSafeUrl(s)) continue;
      el.setAttribute(k, v === true ? '' : s);
    }
  }
}

export function isSafeUrl(u) {
  const s = String(u).trim().toLowerCase().replace(/[\u0000-\u001f\s]+/g, '');
  if (s.startsWith('javascript:') || s.startsWith('vbscript:') || s.startsWith('data:text/html')) return false;
  return true;
}

/** Append children (strings, Nodes, nested arrays); null/false/true are skipped (unlike Element.append). */
export function append(el, ...children) {
  for (const c of children) {
    if (c == null || c === false || c === true) continue;
    if (Array.isArray(c)) append(el, ...c);
    else if (c instanceof Node) el.appendChild(c);
    else el.appendChild(document.createTextNode(String(c)));
  }
  return el;
}

export function clear(el) {
  while (el.firstChild) el.removeChild(el.firstChild);
  return el;
}

export function replace(el, ...children) {
  clear(el);
  return append(el, ...children);
}

const SVGNS = 'http://www.w3.org/2000/svg';
export function svg(tag, attrs, ...children) {
  const el = document.createElementNS(SVGNS, tag);
  if (attrs) for (const k in attrs) if (attrs[k] != null) el.setAttribute(k, String(attrs[k]));
  for (const c of children.flat()) {
    if (c == null || c === false) continue;
    el.appendChild(c instanceof Node ? c : document.createTextNode(String(c)));
  }
  return el;
}

/** Wrap text, highlighting any of `terms` (case-insensitive) with <mark>. Safe: text nodes only. */
export function highlight(text, terms) {
  const s = String(text ?? '');
  const ts = (terms || []).map(t => String(t).trim()).filter(t => t.length > 0).slice(0, 12);
  if (!ts.length) return document.createTextNode(s);
  const re = new RegExp('(' + ts.map(escapeRe).join('|') + ')', 'gi');
  const frag = document.createDocumentFragment();
  let last = 0;
  s.replace(re, (m, _g, off) => {
    if (off > last) frag.appendChild(document.createTextNode(s.slice(last, off)));
    const mk = document.createElement('mark');
    mk.textContent = m;
    frag.appendChild(mk);
    last = off + m.length;
    return m;
  });
  if (last < s.length) frag.appendChild(document.createTextNode(s.slice(last)));
  return frag;
}

export function escapeRe(s) { return s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'); }

export function debounce(fn, ms) {
  let t = 0;
  const d = (...a) => { clearTimeout(t); t = setTimeout(() => fn(...a), ms); };
  d.cancel = () => clearTimeout(t);
  return d;
}

let uid = 0;
export const nextId = (p = 'id') => `${p}-${++uid}`;
