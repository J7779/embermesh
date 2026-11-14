import test from 'node:test';
import assert from 'node:assert/strict';
import { parseMarkdown, toHtml, classifyHref, outline, stripFrontMatter, LIMITS } from '../js/lib/markdown.js';

const html = s => toHtml(parseMarkdown(s));

test('headings, paragraphs and emphasis', () => {
  assert.equal(html('# Title'), '<h1 id="md-title">Title</h1>');
  assert.equal(html('Title\n====='), '<h1 id="md-title">Title</h1>');
  assert.equal(html('Sub\n---'), '<h2 id="md-sub">Sub</h2>');
  assert.equal(html('a *b* _c_ **d** __e__ ~~f~~ ***g***'), '<p>a <em>b</em> <em>c</em> <strong>d</strong> <strong>e</strong> <del>f</del> <strong><em>g</em></strong></p>');
  assert.equal(html('snake_case_name and 2*3*4'), '<p>snake_case_name and 2<em>3</em>4</p>');
  assert.equal(html('not * emphasis * here'), '<p>not * emphasis * here</p>');
  assert.equal(html('line one  \nline two'), '<p>line one<br>line two</p>');
  assert.equal(html('\\*literal\\*'), '<p>*literal*</p>');
});

test('duplicate heading ids are unique and outline matches', () => {
  const ast = parseMarkdown('# A\n\n## A\n\n### B');
  assert.match(toHtml(ast), /id="md-a".*id="md-a-1"/s);
  assert.deepEqual(outline(ast).map(o => o.id), ['md-a', 'md-a-1', 'md-b']);
});

test('lists: nested, ordered start, loose, task items', () => {
  assert.equal(html('- a\n- b\n  - c\n- d'), '<ul><li>a</li><li>b<ul><li>c</li></ul></li><li>d</li></ul>');
  assert.equal(html('3. x\n4. y'), '<ol start="3"><li>x</li><li>y</li></ol>');
  assert.equal(html('- a\n\n- b'), '<ul><li><p>a</p></li><li><p>b</p></li></ul>');
  assert.match(html('- [x] done\n- [ ] todo'), /md-task done.*☑.*md-task.*☐/s);
});

test('code: fenced, indented, inline; content escaped', () => {
  assert.equal(html('```js\nlet a = "<b>";\n```'), '<pre class="md-code lang-js"><code>let a = &quot;&lt;b&gt;&quot;;</code></pre>');
  assert.equal(html('    x < y'), '<pre class="md-code"><code>x &lt; y</code></pre>');
  assert.equal(html('use `a <b>` here'), '<p>use <code>a &lt;b&gt;</code> here</p>');
  assert.equal(html('```\nunclosed'), '<pre class="md-code"><code>unclosed</code></pre>');
  assert.equal(html('```"><script>\nx\n```'), '<pre class="md-code lang-script"><code>x</code></pre>'); // info string sanitised
});

test('blockquotes and tables', () => {
  assert.equal(html('> a\n> > b'), '<blockquote><p>a</p><blockquote><p>b</p></blockquote></blockquote>');
  const t = html('| L | C | R |\n|:--|:-:|--:|\n| 1 | 2 | 3 |\n| only |');
  assert.match(t, /<th class="al-left">L<\/th><th class="al-center">C<\/th><th class="al-right">R<\/th>/);
  assert.match(t, /<td class="al-left">only<\/td><td class="al-center"><\/td><td class="al-right"><\/td>/);
  assert.match(html('a | b\n--|--\nx \\| y | z'), /<td>x \| y<\/td>/);
});

test('links are classified and never get an href', () => {
  const out = html('[ok](https://example.org) [rel](../water/boil.md) [anchor](#steps) <https://a.b> see http://x.y/z.');
  assert.doesNotMatch(out, /\shref=/);
  assert.match(out, /data-href="https:\/\/example.org" data-kind="external"/);
  assert.match(out, /data-href="..\/water\/boil.md" data-kind="internal"/);
  assert.match(out, /data-href="#steps" data-kind="anchor"/);
  assert.match(out, /data-href="http:\/\/x.y\/z"/); // trailing dot excluded
  assert.equal(classifyHref('guide:water/boil'), 'internal');
  assert.equal(classifyHref('mailto:a@b'), 'external');
});

const XSS = [
  '<script>alert(1)</script>',
  '<img src=x onerror=alert(1)>',
  '<svg/onload=alert(1)>',
  '<iframe src="javascript:alert(1)"></iframe>',
  '[x](javascript:alert(1))',
  '[x](JaVaScRiPt:alert(1))',
  '[x](java\tscript:alert(1))',
  '[x]( javascript:alert(1) )',
  '[x](vbscript:msgbox(1))',
  '[x](data:text/html;base64,PHNjcmlwdD4=)',
  '[x](file:///etc/passwd)',
  '<javascript:alert(1)>',
  '![a](javascript:alert(1))',
  '![a" onerror="alert(1)](x.png)',
  '[a" onmouseover="alert(1)](https://x)',
  '[x](https://x "t\\" onclick=\\"alert(1)")',
  '`<script>`',
  '| <b onclick=x> |\n|---|\n| <script> |',
  '> <style>*{}</style>',
  '# <img src=x onerror=alert(1)>',
  '```html\n<script>alert(1)</script>\n```',
  '<a href="javascript:alert(1)">x</a>',
  '&lt;script&gt; &#106;avascript',
];

test('XSS attempts render inert', () => {
  for (const x of XSS) {
    const full = html(x);
    const out = full.replace(/="[^"]*"/g, '=""'); // attribute values are escaped; inspect tag structure only
    assert.doesNotMatch(out, /<(script|img|svg|iframe|style|object|embed|link|form|input|base|meta)\b/i, x + ' → ' + out);
    assert.doesNotMatch(out, /<[^>]+\son\w+\s*=/i, x + ' → ' + out);
    assert.doesNotMatch(out, /<[^>]*\s(href|src|style)=/i, x + ' → ' + out);
    assert.doesNotMatch(full, /data-href="\s*(javascript|vbscript|data|file):/i, x);
    assert.doesNotMatch(full, /data-href="\s*j\s*a\s*v\s*a/i, x + ' → ' + full);
    assert.doesNotMatch(full, /"[^"]*<script/i, x);
  }
});

test('blocked links keep their text but drop the target', () => {
  const out = html('[click me](javascript:alert(1))');
  assert.match(out, /md-blocked/);
  assert.match(out, /click me/);
});

test('front matter stripped; hostile sizes and nesting bounded', () => {
  assert.equal(stripFrontMatter('---\ntitle: x\n---\n# Hi'), '# Hi');
  const deep = '>'.repeat(5000) + ' x';
  const t0 = Date.now();
  assert.ok(html(deep).length > 0);
  const nested = Array.from({ length: 200 }, (_, i) => ' '.repeat(i * 2) + '- item').join('\n');
  assert.ok(html(nested).length > 0);
  assert.ok(html('*'.repeat(20000) + 'a').length > 0);
  assert.ok(html('['.repeat(20000)).length > 0);
  assert.ok(html('`'.repeat(5000)).length > 0);
  assert.ok(html('| a '.repeat(500) + '|\n' + '|---'.repeat(500) + '|').split(/<th[ >]/).length - 1 <= LIMITS.maxCols);
  assert.ok(Date.now() - t0 < 5000, 'pathological inputs must render quickly');
});

test('non-string and empty input', () => {
  assert.equal(html(''), '');
  assert.equal(toHtml(parseMarkdown(null)), '');
  assert.equal(toHtml(parseMarkdown(42)), '<p>42</p>');
});

test('no super-linear regex backtracking on long hostile lines', () => {
  const S = ' '.repeat(30000);
  const cases = ['# a' + S + 'b', '#' + S + '#' + S + 'x', '- '.repeat(20000) + 'x', 'a\n' + '='.repeat(30000) + ' x',
    'a|b\n|' + ' -'.repeat(20000) + ' x', '```' + S + 'x`', '-' + S + 'x', '[' + S + '](' + S, '<' + 'a'.repeat(30000),
    'http://' + 'a'.repeat(100000), '>' + S + 'x', '    ' + S + 'x', '*' + S + '*', '`' + S, 'x' + S + '\ny',
    '_a '.repeat(20000), '*a '.repeat(20000), Array.from({ length: 400 }, (_, i) => ' '.repeat(i * 2) + '- item').join('\n')];
  for (const c of cases) {
    const t0 = Date.now();
    html(c);
    assert.ok(Date.now() - t0 < 300, 'slow input: ' + JSON.stringify(c.slice(0, 20)) + ' took ' + (Date.now() - t0) + ' ms');
  }
});
