import { test } from 'node:test';
import assert from 'node:assert/strict';
import { rewriteModule, escapeScript, CANON } from '../tools/singlefile.mjs';

test('rewriteModule: static, re-export, dynamic and import.meta.url', () => {
  const src = [
    "import a from './a.js';",
    "import { b, c as d } from '../lib/b.js';",
    "import * as ns from \"./ns.js\";",
    "import './side.js';",
    "export { x } from './x.js';",
    "export * from './all.js';",
    "import ext from 'node:fs';",
    "const m = await import(`./backends/${k}.js`);",
    "const u = new URL('../../wasm/ember.mjs', import.meta.url);",
    "obj.import('x'); __emberImport('y');",
  ].join('\n');
  const out = rewriteModule(src, `${CANON}js/backends/wasm.js`);
  assert.match(out, /import a from 'https:\/\/embermesh\.invalid\/js\/backends\/a\.js'/);
  assert.match(out, /from 'https:\/\/embermesh\.invalid\/js\/lib\/b\.js'/);
  assert.match(out, /from "https:\/\/embermesh\.invalid\/js\/backends\/ns\.js"/);
  assert.match(out, /import 'https:\/\/embermesh\.invalid\/js\/backends\/side\.js'/);
  assert.match(out, /export \{ x \} from 'https:\/\/embermesh\.invalid\/js\/backends\/x\.js'/);
  assert.match(out, /export \* from 'https:\/\/embermesh\.invalid\/js\/backends\/all\.js'/);
  assert.match(out, /import ext from 'node:fs'/, 'bare specifiers untouched');
  assert.match(out, /__emberImport\("https:\/\/embermesh\.invalid\/js\/backends\/wasm\.js", `\.\/backends\/\$\{k\}\.js`\)/);
  assert.match(out, /new URL\('\.\.\/\.\.\/wasm\/ember\.mjs', "https:\/\/embermesh\.invalid\/js\/backends\/wasm\.js"\)/);
  assert.match(out, /obj\.import\('x'\)/, 'method named import untouched');
  assert.match(out, /__emberImport\('y'\)/, 'already-rewritten call untouched');
});

test('escapeScript removes HTML-significant sequences only', () => {
  assert.equal(escapeScript("a='</script>'; b='<!--'; c='<div>'"), "a='\\x3C/script>'; b='\\x3C!--'; c='<div>'");
  assert.equal(escapeScript('x</SCRIPT'), 'x\\x3C/SCRIPT');
  // the escaped text is still the same JS string value
  // eslint-disable-next-line no-eval
  assert.equal(eval(escapeScript("'</script><!--'")), '</script><!--');
});
