// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { APP_LICENCE, THIRD_PARTY, licenceFamily, licenceSummary } from '../js/lib/about.js';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');

test('guide licence strings map to families', () => {
  assert.equal(licenceFamily('CC BY-SA 4.0'), 'CC BY-SA 4.0');
  assert.equal(licenceFamily('CC BY-SA 3.0'), 'CC BY-SA 3.0');
  assert.equal(licenceFamily('CC BY 4.0'), 'CC BY 4.0');
  assert.equal(licenceFamily('CC0-1.0'), 'CC0 1.0 (public domain dedication)');
  assert.equal(licenceFamily('Public Domain (US Government work, 17 U.S.C. §105; published 1918, United States)'), 'Public domain (US Government works)');
  assert.equal(licenceFamily('Public Domain (US Government)'), 'Public domain (US Government works)');
  assert.equal(licenceFamily('Public Domain (published 1914, United States)'), 'Public domain (old books and other works)');
  assert.equal(licenceFamily('Public Domain'), 'Public domain (old books and other works)');
  assert.equal(licenceFamily(''), 'Unknown');
  assert.equal(licenceFamily('OGL-UK-3.0'), 'OGL-UK-3.0');
});

test('licence summary groups and sorts, ignoring junk', () => {
  const s = licenceSummary([
    { name: 'Public Domain', count: 3 }, { name: 'CC BY-SA 4.0', count: 5 },
    { name: 'Public Domain (published 1920, United States)', count: 4 }, null, { count: 9 }, { name: 'CC0-1.0', count: 'x' },
  ]);
  assert.deepEqual(s, [
    { family: 'Public domain (old books and other works)', count: 7 },
    { family: 'CC BY-SA 4.0', count: 5 },
    { family: 'CC0 1.0 (public domain dedication)', count: 0 },
  ]);
  assert.deepEqual(licenceSummary(undefined), []);
});

test('About licences match LICENSE and THIRD_PARTY_LICENSES.md', () => {
  const lic = path.join(ROOT, 'LICENSE');
  const tp = path.join(ROOT, 'THIRD_PARTY_LICENSES.md');
  if (!fs.existsSync(lic) || !fs.existsSync(tp)) return; // ui/ copied on its own (app bundles)
  const licence = fs.readFileSync(lic, 'utf8');
  assert.match(licence, /^MIT License/);
  assert.ok(licence.includes(APP_LICENCE.copyright), 'LICENSE copyright line');
  const doc = fs.readFileSync(tp, 'utf8');
  for (const c of THIRD_PARTY) {
    assert.ok(doc.includes(c.name), c.name + ' listed in THIRD_PARTY_LICENSES.md');
    assert.ok(doc.includes(c.version), c.name + ' version ' + c.version + ' in THIRD_PARTY_LICENSES.md');
  }
});
