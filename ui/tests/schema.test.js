import test from 'node:test';
import assert from 'node:assert/strict';
import { normalizeFields, defaults, coerce, validate, collect, withDefaults } from '../js/lib/schema.js';

const FIELDS = [
  { key: 'baud', label: 'Baud', type: 'number', default: 9600, min: 300, max: 115200, required: true },
  { key: 'modem', label: 'Modem', type: 'select', default: 'afsk1200', options: [{ value: 'afsk1200', label: 'AFSK 1200' }, { value: 'afsk300', label: 'AFSK 300' }] },
  { key: 'sf', label: 'SF', type: 'select', options: [{ value: 7 }, { value: 9 }] },
  { key: 'ham', label: 'Ham', type: 'bool', default: true },
  { key: 'call', label: 'Callsign', type: 'text', required: true },
  { key: 'psk', label: 'Key', type: 'password' },
  { key: 'notes', type: 'textarea' },
  { key: 'delay', type: 'number' },
];

test('normalizeFields drops junk and defaults unknown types to text', () => {
  const n = normalizeFields([null, 5, { label: 'no key' }, { key: 'a', type: 'weird' }, { key: 'a', type: 'number' }, { key: 'b', type: 'select', options: ['x', { value: 'y', label: 'Y' }, null] }]);
  assert.deepEqual(n.map(f => [f.key, f.type]), [['a', 'text'], ['b', 'select']]);
  assert.deepEqual(n[1].options, [{ value: 'x', label: 'x' }, { value: 'y', label: 'Y' }]);
  assert.deepEqual(normalizeFields('nope'), []);
});

test('defaults are typed', () => {
  assert.deepEqual(defaults(FIELDS), { baud: 9600, modem: 'afsk1200', sf: 7, ham: true, call: '', psk: '', notes: '', delay: null });
});

test('coerce converts input strings to JSON types', () => {
  const [baud, modem, sf, ham, call] = normalizeFields(FIELDS);
  assert.equal(coerce(baud, '1200'), 1200);
  assert.equal(coerce(baud, ' 12,5 '), 12.5);
  assert.equal(coerce(baud, ''), null);
  assert.equal(coerce(baud, 'abc'), null);
  assert.equal(coerce(baud, Infinity), null);
  assert.equal(coerce(modem, 'afsk300'), 'afsk300');
  assert.equal(coerce(sf, '9'), 9); // option value type preserved
  assert.equal(coerce(ham, 'false'), false);
  assert.equal(coerce(ham, 'on'), true);
  assert.equal(coerce(call, 42), '42');
  assert.equal(coerce(call, null), '');
});

test('validate: required, min/max, options', () => {
  assert.deepEqual(validate(FIELDS, { baud: 9600, call: 'QX1ABC', modem: 'afsk1200' }), {});
  const e = validate(FIELDS, { baud: 100, call: '', modem: 'nope' });
  assert.equal(e.baud, 'Minimum 300');
  assert.equal(e.call, 'Required');
  assert.equal(e.modem, 'Choose an option');
  assert.equal(validate(FIELDS, { baud: 1e9, call: 'x' }).baud, 'Maximum 115200');
  assert.equal(validate(FIELDS, { baud: null, call: 'x' }).baud, 'Required');
});

test('collect sends every field typed, omitting empty optional numbers', () => {
  const v = collect(FIELDS, { baud: '1200', ham: 'false', call: 'QX1ABC', sf: '9' });
  assert.deepEqual(v, { baud: 1200, modem: 'afsk1200', sf: 9, ham: false, call: 'QX1ABC', psk: '', notes: '' });
  assert.ok(!('delay' in v));
});

test('withDefaults merges saved values and drops unknown keys', () => {
  const v = withDefaults(FIELDS, { baud: '4800', evil: '<script>', ham: false });
  assert.equal(v.baud, 4800);
  assert.equal(v.ham, false);
  assert.ok(!('evil' in v));
});
