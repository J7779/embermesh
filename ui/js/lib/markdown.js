// Safe Markdown renderer for guide files and other hostile text. No DOM needed to parse.
//
//   const ast = parseMarkdown(src);           // block/inline AST (plain objects)
//   toHtml(ast)                               // escaped HTML string (tests, previews)
//   toDom(ast, document)                      // DocumentFragment built with createElement
//
// Supported: ATX + setext headings, paragraphs, *em* _em_ **strong** __strong__ ~~del~~,
// `code`, fenced/indented code, ordered/unordered nested lists, blockquotes, GFM tables with
// alignment, horizontal rules, hard breaks, [links](url), <autolinks>, bare http(s) URLs.
// Raw HTML is shown as text. Images render as "[image: alt]". Links never get an href: they
// carry data-href + data-kind ("internal" | "external" | "anchor"), the app decides what a click
// does. Script URLs (javascript:, vbscript:, data:, file:…) are rendered as inert text.
// Input size, nesting depth and table width are capped so hostile files cannot hang the reader.

export const LIMITS = { maxInput: 2_000_000, maxDepth: 8, maxCols: 40, maxInline: 100_000 };

// ---------------------------------------------------------------- block parsing

const RE = {
  fence: /^ {0,3}(`{3,}|~{3,})/,
  atx: /^ {0,3}(#{1,6})(?=[ \t]|$)/,
  hr: /^ {0,3}([-*_])(?:[ \t]*\1){2,}[ \t]*$/,
  quote: /^ {0,3}> ?(.*)$/,
  list: /^( {0,12})([-*+]|\d{1,9}[.)])(?:[ \t]+(.*)|$)/,
  setext: /^ {0,3}(=+|-+)[ \t]*$/,
  tableDelim: /^ {0,3}\|?[ \t]*:?-+:?[ \t]*(?:\|[ \t]*:?-+:?[ \t]*)*\|?[ \t]*$/,
  indentCode: /^(?: {4}|\t)(.*)$/,
  blank: /^[ \t]*$/,
  pipeRow: /^ {0,3}\|.*\|[ \t]*$/,
};

/** Fence opener → {fence, lang} or null. Manual parsing avoids regex backtracking on long lines. */
function fenceOpen(line) {
  const m = RE.fence.exec(line);
  if (!m) return null;
  const info = line.slice(m[0].length);
  if (m[1][0] === '`' && info.includes('`')) return null;
  const lang = info.trim().split(/[ \t]/)[0] || '';
  return { fence: m[1], lang };
}

/** ATX heading → {level, text} or null. */
function atxHeading(line) {
  const m = RE.atx.exec(line);
  if (!m) return null;
  let t = line.slice(m[0].length).trim();
  if (t.endsWith('#')) {
    let k = t.length;
    while (k > 0 && t[k - 1] === '#') k--;
    if (k === 0) t = '';
    else if (t[k - 1] === ' ' || t[k - 1] === '\t') t = t.slice(0, k).trim();
  }
  return { level: m[1].length, text: t };
}

export function stripFrontMatter(src) {
  const m = /^---[ \t]*\r?\n[\s\S]*?\r?\n---[ \t]*(?:\r?\n|$)/.exec(src);
  return m ? src.slice(m[0].length) : src;
}

export function parseMarkdown(src) {
  let s = typeof src === 'string' ? src : String(src ?? '');
  if (s.length > LIMITS.maxInput) s = s.slice(0, LIMITS.maxInput) + '\n\n… (truncated)';
  s = s.replace(/\r\n?/g, '\n').replace(/\u0000/g, '\uFFFD');
  s = stripFrontMatter(s);
  return { type: 'root', children: parseBlocks(s.split('\n'), 0) };
}

function isBlockStart(line, next) {
  return !!fenceOpen(line) || RE.atx.test(line) || RE.hr.test(line) || RE.quote.test(line) ||
    RE.list.test(line) || (line.includes('|') && next !== undefined && RE.tableDelim.test(next) && next.includes('-')) ||
    (RE.pipeRow.test(line) && next !== undefined && RE.pipeRow.test(next));
}

function parseBlocks(lines, depth) {
  const out = [];
  let i = 0;
  const n = lines.length;
  while (i < n) {
    const line = lines[i];
    if (RE.blank.test(line)) { i++; continue; }

    let m;
    // fenced code
    const fo = fenceOpen(line);
    if (fo) {
      const fence = fo.fence;
      const lang = fo.lang.toLowerCase().replace(/[^a-z0-9+#-]/g, '').slice(0, 20);
      const body = [];
      i++;
      while (i < n && !(new RegExp('^ {0,3}' + (fence[0] === '`' ? '`' : '~') + '{' + fence.length + ',}[ \\t]*$').test(lines[i]))) body.push(lines[i++]);
      i++; // closing fence (or EOF)
      out.push({ type: 'code', lang, text: body.join('\n') });
      continue;
    }
    const ah = atxHeading(line);
    if (ah) {
      out.push({ type: 'heading', level: ah.level, children: parseInline(ah.text, 0) });
      i++; continue;
    }
    if (RE.hr.test(line)) { out.push({ type: 'hr' }); i++; continue; }

    if (RE.quote.test(line)) {
      const body = [];
      while (i < n && (m = RE.quote.exec(lines[i]))) { body.push(m[1]); i++; }
      if (depth >= LIMITS.maxDepth) out.push({ type: 'paragraph', children: parseInline(body.join('\n'), 0) });
      else out.push({ type: 'blockquote', children: parseBlocks(body, depth + 1) });
      continue;
    }

    if ((m = RE.list.exec(line))) {
      const res = parseList(lines, i, depth);
      out.push(res.node);
      i = res.next;
      continue;
    }

    // table
    if (line.includes('|') && i + 1 < n && RE.tableDelim.test(lines[i + 1]) && lines[i + 1].includes('-')) {
      const header = splitRow(line);
      const aligns = splitRow(lines[i + 1]).map(c => {
        const l = c.startsWith(':'), r = c.endsWith(':');
        return l && r ? 'center' : r ? 'right' : l ? 'left' : '';
      });
      const cols = Math.min(LIMITS.maxCols, header.length);
      i += 2;
      const rows = [];
      while (i < n && !RE.blank.test(lines[i]) && lines[i].includes('|')) { rows.push(splitRow(lines[i])); i++; }
      const cell = t => parseInline(t, 0);
      out.push({
        type: 'table',
        aligns: aligns.slice(0, cols),
        header: header.slice(0, cols).map(cell),
        rows: rows.map(r => { const c = r.slice(0, cols); while (c.length < cols) c.push(''); return c.map(cell); }),
      });
      continue;
    }

    // pipe table without a delimiter row (common in converted documents): two or more
    // consecutive lines that start and end with "|" become a table with no header row
    if (RE.pipeRow.test(line) && i + 1 < n && RE.pipeRow.test(lines[i + 1])) {
      const rows = [];
      while (i < n && RE.pipeRow.test(lines[i])) { rows.push(splitRow(lines[i])); i++; }
      const cols = Math.min(LIMITS.maxCols, Math.max(...rows.map(r => r.length)));
      const cell = t => parseInline(t, 0);
      out.push({ type: 'table', aligns: [], header: null, rows: rows.map(r => { const c = r.slice(0, cols); while (c.length < cols) c.push(''); return c.map(cell); }) });
      continue;
    }

    // indented code
    if (RE.indentCode.test(line)) {
      const body = [];
      while (i < n && (RE.indentCode.test(lines[i]) || (RE.blank.test(lines[i]) && i + 1 < n && RE.indentCode.test(lines[i + 1])))) {
        const mm = RE.indentCode.exec(lines[i]);
        body.push(mm ? mm[1] : '');
        i++;
      }
      out.push({ type: 'code', lang: '', text: body.join('\n') });
      continue;
    }

    // paragraph (+ setext heading)
    const para = [line];
    i++;
    let setext = 0;
    while (i < n && !RE.blank.test(lines[i])) {
      const sm = RE.setext.exec(lines[i]);
      if (sm) { setext = sm[1][0] === '=' ? 1 : 2; i++; break; }
      if (isBlockStart(lines[i], lines[i + 1])) break;
      para.push(lines[i]);
      i++;
    }
    const text = para.map(l => l.replace(/^[ \t]+/, '')).join('\n');
    if (setext) out.push({ type: 'heading', level: setext, children: parseInline(text, 0) });
    else out.push({ type: 'paragraph', children: parseInline(text, 0) });
  }
  return out;
}

function parseList(lines, start, depth) {
  const first = RE.list.exec(lines[start]);
  const indent = first[1].length;
  const ordered = /\d/.test(first[2]);
  const startNum = ordered ? parseInt(first[2], 10) : 1;
  const items = [];
  let i = start;
  let loose = false;
  const n = lines.length;
  while (i < n) {
    const m = RE.list.exec(lines[i]);
    if (!m || m[1].length !== indent || /\d/.test(m[2]) !== ordered) break;
    const contentIndent = indent + m[2].length + 1;
    const body = [m[3] || ''];
    i++;
    let sawBlank = false;
    while (i < n) {
      const l = lines[i];
      if (RE.blank.test(l)) {
        // blank line: continue only if next non-blank line is indented into this item
        let j = i + 1;
        while (j < n && RE.blank.test(lines[j])) j++;
        if (j < n && leadingSpaces(lines[j]) >= contentIndent) { body.push(''); sawBlank = true; i++; continue; }
        break;
      }
      const lm = RE.list.exec(l);
      if (lm && lm[1].length <= indent) break;
      const lead = leadingSpaces(l);
      if (lead >= contentIndent || lm) { body.push(l.slice(Math.min(lead, contentIndent))); i++; continue; }
      if (isBlockStart(l, lines[i + 1])) break;
      body.push(l.trim()); // lazy continuation
      i++;
    }
    if (sawBlank) loose = true;
    // task list item
    let checked = null;
    const tm = /^\[([ xX])\][ \t]+/.exec(body[0]);
    if (tm) { checked = tm[1] !== ' '; body[0] = body[0].slice(tm[0].length); }
    const children = depth >= LIMITS.maxDepth
      ? [{ type: 'paragraph', children: parseInline(body.join('\n'), 0) }]
      : parseBlocks(body, depth + 1);
    items.push({ type: 'item', checked, children });
    // a blank line between items makes the list loose
    if (i < n && RE.blank.test(lines[i])) {
      let j = i;
      while (j < n && RE.blank.test(lines[j])) j++;
      const nm = j < n && RE.list.exec(lines[j]);
      if (nm && nm[1].length === indent && /\d/.test(nm[2]) === ordered) { loose = true; i = j; continue; }
      break;
    }
  }
  return { node: { type: 'list', ordered, start: startNum, loose, items }, next: i };
}

function leadingSpaces(l) {
  let k = 0;
  for (const c of l) { if (c === ' ') k++; else if (c === '\t') k += 4; else break; }
  return k;
}

function splitRow(line) {
  let s = line.trim();
  if (s.startsWith('|')) s = s.slice(1);
  if (s.endsWith('|') && !s.endsWith('\\|')) s = s.slice(0, -1);
  const cells = [];
  let cur = '';
  for (let k = 0; k < s.length; k++) {
    const c = s[k];
    if (c === '\\' && s[k + 1] === '|') { cur += '|'; k++; continue; }
    if (c === '|') { cells.push(cur.trim()); cur = ''; continue; }
    cur += c;
  }
  cells.push(cur.trim());
  return cells.slice(0, LIMITS.maxCols * 2);
}

// ---------------------------------------------------------------- inline parsing

const ESCAPABLE = '\\`*_{}[]()#+-.!|~<>"\'';

export function parseInline(src, depth) {
  let s = String(src);
  if (s.length > LIMITS.maxInline) s = s.slice(0, LIMITS.maxInline) + '…';
  const out = [];
  let buf = '';
  const noCloser = new Map(); // memo: delimiter -> {from, at} of the next valid closer
  const flush = () => { if (buf) { pushText(out, buf); buf = ''; } };
  let i = 0;
  while (i < s.length) {
    const c = s[i];
    // escapes and hard breaks
    if (c === '\\') {
      if (s[i + 1] === '\n') { flush(); out.push({ type: 'br' }); i += 2; continue; }
      if (s[i + 1] && ESCAPABLE.includes(s[i + 1])) { buf += s[i + 1]; i += 2; continue; }
    }
    if (c === '\n') {
      let sp = 0;
      while (sp < buf.length && buf.charCodeAt(buf.length - 1 - sp) === 32) sp++;
      if (sp >= 2) { buf = buf.slice(0, buf.length - sp); flush(); out.push({ type: 'br' }); }
      else buf += ' ';
      i++; continue;
    }
    // code span
    if (c === '`') {
      let run = 1;
      while (s[i + run] === '`') run++;
      const fence = '`'.repeat(run);
      const end = s.indexOf(fence, i + run);
      if (end > 0) {
        flush();
        let code = s.slice(i + run, end).replace(/\n/g, ' ');
        if (/^ .* $/.test(code)) code = code.slice(1, -1);
        out.push({ type: 'code', text: code });
        i = end + run; continue;
      }
      buf += fence; i += run; continue;
    }
    // image → placeholder text
    if (c === '!' && s[i + 1] === '[') {
      const l = parseLinkAt(s, i + 1);
      if (l) { flush(); out.push({ type: 'image', alt: l.label, src: l.href }); i = l.end; continue; }
    }
    if (c === '[') {
      const l = parseLinkAt(s, i);
      if (l) {
        flush();
        out.push(makeLink(l.href, depth < LIMITS.maxDepth ? parseInline(l.label, depth + 1) : [{ type: 'text', text: l.label }], l.title));
        i = l.end; continue;
      }
    }
    // autolink <scheme:...>
    if (c === '<') {
      const m = /^<([a-zA-Z][a-zA-Z0-9+.-]{1,31}:[^\s<>]*)>/.exec(s.slice(i, i + 2048));
      if (m) { flush(); out.push(makeLink(m[1], [{ type: 'text', text: m[1] }])); i += m[0].length; continue; }
    }
    // bare URL
    if ((c === 'h' || c === 'H') && /^https?:\/\//i.test(s.slice(i, i + 8)) && !/[A-Za-z0-9]$/.test(buf)) {
      const m = /^https?:\/\/[^\s<>"]+/i.exec(s.slice(i, i + 2048));
      // "http://" with nothing after it (seen in converted guides) is plain text, not a link
      const url = m ? m[0].replace(/[.,;:!?)\]'"]+$/, '') : '';
      if (!/^https?:\/\/[^/\s]/i.test(url)) { buf += s.slice(i, i + (m ? m[0].length : 7)); i += m ? m[0].length : 7; continue; }
      flush();
      out.push(makeLink(url, [{ type: 'text', text: url }]));
      i += url.length; continue;
    }
    // emphasis
    if (c === '*' || c === '_' || c === '~') {
      const r = parseEmphasis(s, i, depth, buf, noCloser);
      if (r) { flush(); out.push(r.node); i = r.end; continue; }
      // no match: consume the whole delimiter run as text (keeps hostile runs linear)
      let run = 1;
      while (s[i + run] === c) run++;
      buf += s.slice(i, i + run);
      i += run;
      continue;
    }
    buf += c;
    i++;
  }
  flush();
  return out;
}

function pushText(out, text) {
  const last = out[out.length - 1];
  if (last && last.type === 'text') last.text += text;
  else out.push({ type: 'text', text });
}

function parseEmphasis(s, i, depth, before, memo) {
  const c = s[i];
  let run = 1;
  while (s[i + run] === c) run++;
  if (c === '~' && run !== 2) return null;
  const size = c === '~' ? 2 : Math.min(run, 2);
  // opener must be followed by non-space; '_' must not be intraword
  const after = s[i + size];
  if (!after || /\s/.test(after)) return null;
  if (c === '_' && /[A-Za-z0-9]$/.test(before)) return null;
  if (depth >= LIMITS.maxDepth) return null;
  const k = findCloser(s, c, size, i + size + 1, memo);
  if (k < 0) return null;
  const type = c === '~' ? 'del' : size === 2 ? 'strong' : 'em';
  return { node: { type, children: parseInline(s.slice(i + size, k), depth + 1) }, end: k + size };
}

/**
 * First valid closing delimiter at or after `from`. Closer validity does not depend on the opener,
 * so results are memoised per delimiter: later openers reuse the answer (amortised linear time).
 */
function findCloser(s, c, size, from, memo) {
  const delim = c.repeat(size);
  const m = memo && memo.get(delim);
  if (m && from >= m.from && (m.at < 0 || from <= m.at)) return m.at;
  let p = from, at = -1;
  for (;;) {
    let k = s.indexOf(delim, p);
    if (k < 0) break;
    let r = 0;
    while (s[k + r] === c) r++;
    const kk = r > size && c !== '~' ? k + r - size : k; // "***" closes with its last `size` chars
    const prev = s[kk - 1];
    const tooLong = size === 1 && r % 2 === 0; // a "**" run belongs to strong, it cannot close "*"
    const intraword = c === '_' && /[A-Za-z0-9]/.test(s[kk + size] || '');
    if (prev && !/\s/.test(prev) && !tooLong && !intraword) { at = kk; break; }
    p = k + r;
  }
  if (memo) memo.set(delim, { from, at });
  return at;
}

/** Parse "[label](href "title")" starting at s[i] === '['. */
function parseLinkAt(s, i) {
  let depth = 0, k = i;
  for (; k < s.length && k - i < 2000; k++) {
    const ch = s[k];
    if (ch === '\\') { k++; continue; }
    if (ch === '[') depth++;
    else if (ch === ']') { depth--; if (depth === 0) break; }
  }
  if (s[k] !== ']' || s[k + 1] !== '(') return null;
  const label = s.slice(i + 1, k);
  let p = k + 2, pd = 1;
  let q = p;
  for (; q < s.length && q - p < 2048; q++) {
    if (s[q] === '\\') { q++; continue; }
    if (s[q] === '(') pd++;
    else if (s[q] === ')') { pd--; if (pd === 0) break; }
    else if (s[q] === '\n') return null;
  }
  if (s[q] !== ')') return null;
  let inside = s.slice(p, q).trim();
  let title = '';
  const tm = /^(\S+)\s+["'(](.*)["')]$/.exec(inside);
  if (tm) { inside = tm[1]; title = tm[2]; }
  if (inside.startsWith('<') && inside.endsWith('>')) inside = inside.slice(1, -1);
  return { label, href: inside, title, end: q + 1 };
}

/** Classify a link target. Exported for tests. */
export function classifyHref(href) {
  const raw = String(href ?? '').trim();
  // Remove control chars / whitespace that browsers ignore inside schemes ("java\tscript:").
  const squashed = raw.replace(/[\u0000-\u0020\u007f-\u009f]+/g, '').toLowerCase();
  if (!squashed) return 'blocked';
  if (squashed.startsWith('#')) return 'anchor';
  const sm = /^([a-z][a-z0-9+.-]*):/.exec(squashed);
  if (sm) {
    const scheme = sm[1];
    if (['http', 'https', 'mailto', 'tel', 'geo', 'ftp'].includes(scheme)) return 'external';
    if (scheme === 'guide' || scheme === 'ember') return 'internal';
    return 'blocked';
  }
  if (/^&#|%[0-9a-f]{2}/i.test(squashed) && /script/i.test(decodeSafe(squashed))) return 'blocked';
  return 'internal';
}

function decodeSafe(s) { try { return decodeURIComponent(s); } catch { return s; } }

function makeLink(href, children, title) {
  const kind = classifyHref(href);
  if (kind === 'blocked') return { type: 'blocked', href: String(href).slice(0, 300), children };
  return { type: 'link', href: String(href).slice(0, 2048), kind, title: title || '', children };
}

// ---------------------------------------------------------------- rendering

export function slugify(text) {
  return String(text).toLowerCase().normalize('NFKD').replace(/[^\w\s-]/g, '').trim().replace(/[\s_]+/g, '-').replace(/[^a-z0-9-]/g, '').slice(0, 64);
}

function inlineText(nodes) {
  let s = '';
  for (const n of nodes || []) s += n.text != null ? n.text : n.alt != null ? n.alt : inlineText(n.children);
  return s;
}

/** Generic renderer: builder = {el(tag, attrs, children) → node, text(str) → node}. */
export function render(ast, b, state) {
  const slugs = (state && state.slugs) || new Map();
  const blocks = list => list.map(block);
  function block(n) {
    switch (n.type) {
      case 'heading': {
        const base = slugify(inlineText(n.children)) || 'section';
        const c = slugs.get(base) || 0;
        slugs.set(base, c + 1);
        return b.el('h' + Math.min(6, Math.max(1, n.level)), { id: 'md-' + base + (c ? '-' + c : '') }, inlines(n.children));
      }
      case 'paragraph': return b.el('p', null, inlines(n.children));
      case 'code': return b.el('pre', { class: 'md-code' + (n.lang ? ' lang-' + n.lang : '') }, [b.el('code', null, [b.text(n.text)])]);
      case 'hr': return b.el('hr', null, []);
      case 'blockquote': return b.el('blockquote', null, blocks(n.children));
      case 'list': {
        const attrs = n.ordered && n.start !== 1 ? { start: String(n.start) } : null;
        return b.el(n.ordered ? 'ol' : 'ul', attrs, n.items.map(it => {
          const kids = (!n.loose && it.children.length && it.children[0].type === 'paragraph')
            ? [...inlines(it.children[0].children), ...blocks(it.children.slice(1))]
            : blocks(it.children);
          if (it.checked !== null) kids.unshift(b.el('span', { class: 'md-task' + (it.checked ? ' done' : ''), 'aria-label': it.checked ? 'done' : 'not done' }, [b.text(it.checked ? '☑ ' : '☐ ')]));
          return b.el('li', null, kids);
        }));
      }
      case 'table': {
        const al = k => (n.aligns[k] ? { class: 'al-' + n.aligns[k] } : null);
        return b.el('div', { class: 'md-table-wrap' }, [b.el('table', null, [
          ...(n.header ? [b.el('thead', null, [b.el('tr', null, n.header.map((c, k) => b.el('th', al(k), inlines(c))))])] : []),
          b.el('tbody', null, n.rows.map(r => b.el('tr', null, r.map((c, k) => b.el('td', al(k), inlines(c)))))),
        ])]);
      }
      default: return b.text('');
    }
  }
  function inlines(list) { return (list || []).map(inline); }
  function inline(n) {
    switch (n.type) {
      case 'text': return b.text(n.text);
      case 'br': return b.el('br', null, []);
      case 'code': return b.el('code', null, [b.text(n.text)]);
      case 'em': return b.el('em', null, inlines(n.children));
      case 'strong': return b.el('strong', null, inlines(n.children));
      case 'del': return b.el('del', null, inlines(n.children));
      case 'image': return b.el('span', { class: 'md-image' }, [b.text('[image' + (n.alt ? ': ' + n.alt : '') + ']')]);
      case 'blocked': return b.el('span', { class: 'md-blocked', title: 'Link removed (unsafe address)' }, [...inlines(n.children), b.text(' [link removed]')]);
      case 'link': {
        const kids = inlines(n.children);
        const label = inlineText(n.children);
        const attrs = { class: 'md-link md-' + n.kind, role: 'link', tabindex: '0', 'data-href': n.href, 'data-kind': n.kind };
        if (n.title) attrs.title = n.title;
        const out = [b.el('a', attrs, kids)];
        if (n.kind === 'external' && label.trim() !== n.href) out.push(b.el('span', { class: 'md-url' }, [b.text(' <' + n.href + '>')]));
        return out.length === 1 ? out[0] : b.el('span', null, out);
      }
      default: return b.text('');
    }
  }
  return blocks(ast.children || []);
}

export function escapeHtml(s) {
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}

const htmlBuilder = {
  el(tag, attrs, children) {
    let a = '';
    if (attrs) for (const k in attrs) a += ' ' + k + '="' + escapeHtml(attrs[k]) + '"';
    if (tag === 'br' || tag === 'hr') return '<' + tag + a + '>';
    return '<' + tag + a + '>' + children.join('') + '</' + tag + '>';
  },
  text(s) { return escapeHtml(s); },
};

export function toHtml(ast) {
  return render(typeof ast === 'string' ? parseMarkdown(ast) : ast, htmlBuilder).join('\n');
}

export function toDom(ast, doc = document) {
  const b = {
    el(tag, attrs, children) {
      const e = doc.createElement(tag);
      if (attrs) for (const k in attrs) e.setAttribute(k, attrs[k]);
      for (const c of children) e.appendChild(c);
      return e;
    },
    text(s) { return doc.createTextNode(s); },
  };
  const frag = doc.createDocumentFragment();
  for (const n of render(typeof ast === 'string' ? parseMarkdown(ast) : ast, b)) frag.appendChild(n);
  return frag;
}

/**
 * Incremental DOM rendering for long documents (slow phones): splits the top-level blocks into
 * chunks of `size` and returns thunks that each build one DocumentFragment. Heading ids are the
 * same as toDom's because all chunks share one slug table. Render chunk 0 at once, the rest when idle.
 */
export function toDomChunks(ast, size = 60, doc = document) {
  const b = {
    el(tag, attrs, children) {
      const e = doc.createElement(tag);
      if (attrs) for (const k in attrs) e.setAttribute(k, attrs[k]);
      for (const c of children) e.appendChild(c);
      return e;
    },
    text(s) { return doc.createTextNode(s); },
  };
  const state = { slugs: new Map() };
  const kids = (typeof ast === 'string' ? parseMarkdown(ast) : ast).children || [];
  const out = [];
  const n = Math.max(1, size | 0);
  for (let i = 0; i < kids.length || i === 0; i += n) {
    const part = kids.slice(i, i + n);
    out.push(() => {
      const frag = doc.createDocumentFragment();
      for (const node of render({ children: part }, b, state)) frag.appendChild(node);
      return frag;
    });
    if (!kids.length) break;
  }
  return out;
}

/** Plain-text headings outline for a table of contents. */
export function outline(ast) {
  const out = [];
  const slugs = new Map();
  for (const n of ast.children || []) {
    if (n.type !== 'heading') continue;
    const t = inlineText(n.children);
    const base = slugify(t) || 'section';
    const c = slugs.get(base) || 0;
    slugs.set(base, c + 1);
    out.push({ level: n.level, text: t, id: 'md-' + base + (c ? '-' + c : '') });
  }
  return out;
}
