import test from 'node:test';
import assert from 'node:assert/strict';
import * as f from '../js/lib/format.js';

test('timeAgo', () => {
  const now = Date.UTC(2026, 10, 13, 12, 0, 0);
  assert.equal(f.timeAgo(now - 10e3, now), 'just now');
  assert.equal(f.timeAgo(now - 5 * 60e3, now), '5 min ago');
  assert.equal(f.timeAgo(now - 3 * 3600e3, now), '3 h ago');
  assert.equal(f.timeAgo(now - 2 * 86400e3, now), '2 d ago');
  assert.equal(f.timeAgo(now + 2 * 3600e3, now), 'in 2 h');
  assert.match(f.timeAgo(now - 30 * 86400e3, now), /^\d{4}-\d{2}-\d{2}$/);
  assert.equal(f.timeAgo(0, now), 'never');
  assert.equal(f.timeAgo(undefined, now), 'never');
});

test('bytes, durations, frequencies, bitrates', () => {
  assert.equal(f.formatBytes(512), '512 B');
  assert.equal(f.formatBytes(1536), '1.5 KB');
  assert.equal(f.formatBytes(5 * 1024 * 1024), '5.0 MB');
  assert.equal(f.formatBytes(-1), '?');
  assert.equal(f.formatDuration(250), '250 ms');
  assert.equal(f.formatDuration(2500), '2.5 s');
  assert.equal(f.formatDuration(125000), '2 min 5 s');
  assert.equal(f.formatDuration(3 * 3600e3 + 60e3), '3 h 1 min');
  assert.equal(f.formatFreq(146520000), '146.52 MHz');
  assert.equal(f.formatFreq(446006250), '446.00625 MHz');
  assert.equal(f.formatFreq(12500), '12.5 kHz');
  assert.equal(f.formatFreq(2.4e9), '2.4 GHz');
  assert.equal(f.formatBitrate(1200), '1200 bit/s');
  assert.equal(f.formatBitrate(10e6), '10 Mbit/s');
});

test('geo: distance, bearing, compass, maidenhead', () => {
  const a = { lat: 39.8283, lon: -98.5795 }, b = { lat: 39.8283, lon: -98.4 };
  const d = f.distance(a, b);
  assert.ok(Math.abs(d - 15330) < 100, String(d));
  assert.ok(Math.abs(f.bearing(a, b) - 90) < 0.2);
  assert.ok(Math.abs(f.bearing(a, { lat: 40.5, lon: -98.5795 })) < 1e-6);
  assert.equal(f.compass(0), 'N'); assert.equal(f.compass(359), 'N'); assert.equal(f.compass(225), 'SW'); assert.equal(f.compass(NaN), '?');
  assert.ok(Number.isNaN(f.distance(a, null)));
  assert.ok(Number.isNaN(f.distance(a, { lat: 91, lon: 0 })));
  assert.equal(f.maidenhead({ lat: 39.8283, lon: -98.5795 }), 'EM09rt');
  assert.equal(f.formatDistance(640), '640 m');
  assert.equal(f.formatDistance(1234), '1.2 km');
  assert.equal(f.formatDistance(1609.344 * 3, 'imperial'), '3.0 mi');
  assert.equal(f.formatDistance(100, 'imperial'), '328 ft');
  assert.equal(f.formatPos({ lat: -33.5, lon: 151.25 }), '33.50000° S, 151.25000° E');
});

test('message sizing and names', () => {
  assert.equal(f.utf8Length('abc'), 3);
  assert.equal(f.utf8Length('é'), 2);
  assert.equal(f.utf8Length('€'), 3);
  assert.equal(f.utf8Length('😀'), 4);
  assert.equal(f.estimateWireBytes('hi', { encrypted: false }), 2 + 88);
  assert.equal(f.estimateWireBytes('hi', { encrypted: true }), 2 + 128);
  assert.equal(Math.round(f.estimateAirtimeMs(120, 1200)), 1000);
  assert.ok(Number.isNaN(f.estimateAirtimeMs(10, 0)));
  assert.equal(f.initials('Maya Chen'), 'MC');
  assert.equal(f.initials(''), '?');
  assert.equal(f.displayName({ name: 'Maya', callsign: 'QX0MCH' }), 'Maya (QX0MCH)');
  assert.equal(f.displayName({ node_id: 'abcdef0123456789' }), 'Node abcdef01');
  assert.equal(f.hueFor('x'), f.hueFor('x'));
});
