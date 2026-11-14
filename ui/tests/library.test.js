// Smart search / Ask / Related (docs/RPC_API.md §7): pure helpers and the mock's contract.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { semanticState, strongPassages, strongRelated, resultSnippet, licenceLines, EMERGENCY_QUESTIONS } from '../js/lib/library.js';
import { vectorize, cosine, passages } from '../js/mock/semantic.js';
import MockBackend, { _MOCK_METHODS } from '../js/backends/mock.js';

test('semantic state degrades to keyword-only', () => {
  assert.deepEqual(semanticState(null), { available: false, info: null });
  assert.equal(semanticState({ semantic: { available: false } }).available, false);
  assert.equal(semanticState({ semantic: { available: true, dims: 64 } }).available, true);
  assert.equal(semanticState({}).available, false);
});

test('weak Ask results are not shown as answers', () => {
  assert.deepEqual(strongPassages([]), []);
  assert.deepEqual(strongPassages([{ id: 'a', text: 'x', score: 0.1 }]), []);
  const ps = strongPassages([{ id: 'a', text: 'x', score: 0.8 }, { id: 'b', text: 'y', score: 0.3 }, { id: 'c', text: '', score: 0.9 }]);
  assert.deepEqual(ps.map(p => p.id), ['a']);
  assert.deepEqual(strongPassages([{ id: 'a', text: 'x', score: 40 }, { id: 'b', text: 'y', score: 25 }]).map(p => p.id), ['a', 'b'], 'unknown scale: relative cut');
  // the core's verdict wins when present; otherwise the calibrated 0.30 cut-off
  assert.deepEqual(strongPassages([{ id: 'a', text: 'x', score: 0.9 }], true), []);
  assert.deepEqual(strongPassages([{ id: 'a', text: 'x', score: 0.25 }, { id: 'b', text: 'y', score: 0.1 }], false).map(p => p.id), ['a']);
  assert.deepEqual(strongPassages([{ id: 'a', text: 'x', score: 0.25 }]), [], 'below 0.30 without a weak flag');
  assert.deepEqual(strongPassages([{ id: 'a', text: 'x', score: 0.35 }]).map(p => p.id), ['a']);
  assert.deepEqual(strongRelated({ weak: true, results: [{ id: 'a', score: 0.9 }] }), []);
  assert.deepEqual(strongRelated({ weak: false, results: [{ id: 'a', score: 0.2 }] }).map(x => x.id), ['a']);
});

test('result snippet prefers the passage and flags meaning-only matches', () => {
  assert.deepEqual(resultSnippet({ snippet: 's', match: 'words' }), { heading: '', text: 's', byMeaning: false });
  assert.deepEqual(resultSnippet({ snippet: 's', match: 'meaning', passage: { text: 'p', heading: 'H' } }), { heading: 'H', text: 'p', byMeaning: true });
  assert.deepEqual(resultSnippet({ snippet: 's', passage: { text: '  ' } }).text, 's');
  assert.deepEqual(licenceLines({ licences: [{ name: 'A', count: 1 }, { name: 'B', count: 5 }, {}] }).map(l => l.name), ['B', 'A']);
  assert.ok(EMERGENCY_QUESTIONS.length >= 4);
});

test('mock meaning model: concepts, typos, passages', () => {
  assert.ok(cosine(vectorize('hemorrhage'), vectorize('stop the bleeding')) > 0.3);
  assert.ok(cosine(vectorize('hemorrhage'), vectorize('compass and map')) < 0.1);
  const ps = passages({ title: 'T', markdown: '# T\n\nintro\n\n## A\n\ntext a\n\n## B\n\n' + 'x '.repeat(400) });
  assert.deepEqual(ps.map(p => p.heading), ['T', 'A', 'B']);
  assert.ok(ps[2].text.length <= 600);
});

const fresh = (params = {}) => new MockBackend({ persist: false, speed: 0.001, autoplay: false, latency: 0, params });

test('mock implements §7 per contract; exact terms keep keyword ranking', async () => {
  const spec = readFileSync(fileURLToPath(new URL('../../docs/RPC_API.md', import.meta.url)), 'utf8');
  const sec = spec.slice(spec.indexOf('## 7.'), spec.indexOf('## 8.') > 0 ? spec.indexOf('## 8.') : undefined);
  const methods = [...sec.matchAll(/^\| `([a-z_.]+)` \|/gm)].map(m => m[1]);
  assert.deepEqual(methods.sort(), ['guides.ask', 'guides.info', 'guides.related', 'guides.search']);
  for (const m of methods) assert.ok(m in _MOCK_METHODS, m);
  const b = fresh();
  const meaning = (await b.call('guides.search', { query: 'hemorrhage' })).results;
  assert.equal(meaning[0].id, 'medical/bleeding-control');
  assert.equal(meaning[0].match, 'meaning');
  assert.ok(meaning[0].passage.text.length <= 600);
  const exact = (await b.call('guides.search', { query: 'tourniquet' })).results;
  assert.equal(exact[0].id, 'medical/bleeding-control');
  assert.notEqual(exact[0].match, 'meaning');
  const kw = (await b.call('guides.search', { query: 'hemorrhage', semantic: false })).results;
  assert.deepEqual(kw, [], 'semantic:false is keyword-only');
  assert.ok(!('match' in ((await b.call('guides.search', { query: 'tourniquet', semantic: false })).results[0])));
  const ask = await b.call('guides.ask', { question: 'person not breathing' });
  assert.equal(ask.passages[0].id, 'medical/cpr-basics');
  for (const k of ['id', 'title', 'category', 'text', 'score']) assert.ok(k in ask.passages[0], k);
  assert.deepEqual((await b.call('guides.ask', { question: 'repair a car gearbox' })).passages, []);
  await assert.rejects(b.call('guides.ask', {}), e => e.code === 'bad_params');
  const rel = await b.call('guides.related', { id: 'radio/handheld-basics-1' });
  assert.ok(rel.results.length && rel.results.every(r => r.id !== 'radio/handheld-basics-1' && 'score' in r && 'title' in r));
  await assert.rejects(b.call('guides.related', { id: 'nope/x' }), e => e.code === 'not_found');
  const info = await b.call('guides.info');
  assert.equal(info.semantic.available, true);
  assert.ok(info.guides > 5 && info.licences.length);
  b.close();
  const off = fresh({ nosemantic: '1' });
  assert.equal((await off.call('guides.info')).semantic.available, false);
  assert.deepEqual((await off.call('guides.related', { id: 'radio/handheld-basics-1' })).results, []);
  assert.deepEqual((await off.call('guides.search', { query: 'hemorrhage' })).results, []);
  off.close();
});
