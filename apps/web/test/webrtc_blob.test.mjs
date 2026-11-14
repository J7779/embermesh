import { test } from 'node:test';
import assert from 'node:assert/strict';
import { ui } from './helpers.mjs';

const { encodeBlob, decodeBlob, buildSdp, parseSdp, parseCandidate, BLOB_PREFIX, MAX_CANDIDATES } = await import(ui('endpoints/webrtc_blob.js'));

const FP = '6B:8B:F0:65:5F:78:E2:51:3B:AC:6F:F3:3F:46:1B:35:DC:B8:5F:64:1A:24:C2:43:F0:A1:58:D0:A1:2C:19:08';

// Shape of a Chrome data-channel offer after complete ICE gathering (mDNS-obfuscated host + srflx + tcp).
const CHROME_OFFER = [
  'v=0', 'o=- 4611731400430051336 2 IN IP4 127.0.0.1', 's=-', 't=0 0', 'a=group:BUNDLE 0',
  'a=extmap-allow-mixed', 'a=msid-semantic: WMS',
  'm=application 54321 UDP/DTLS/SCTP webrtc-datachannel', 'c=IN IP4 203.0.113.7',
  'a=candidate:1467250027 1 udp 2122260223 192.168.1.23 54321 typ host generation 0 network-id 1',
  'a=candidate:1467250027 1 udp 2122260223 192.168.1.23 54321 typ host generation 0 network-id 1',
  'a=candidate:3141592653 1 udp 2122194687 0d8e5c36-9b2f-4c4a-8f1e-1a2b3c4d5e6f.local 61000 typ host generation 0',
  'a=candidate:2718281828 1 udp 2122129151 fe80::1c2b:3aff:fe4d:5e6f 50000 typ host generation 0',
  'a=candidate:842163049 1 udp 1686052607 203.0.113.7 54321 typ srflx raddr 192.168.1.23 rport 54321 generation 0',
  'a=candidate:99 1 tcp 1518280447 192.168.1.23 9 typ host tcptype active generation 0',
  'a=candidate:98 1 tcp 1518280447 192.168.1.23 443 typ host tcptype passive generation 0',
  'a=candidate:97 2 udp 2122260222 192.168.1.23 54322 typ host generation 0',
  'a=ice-ufrag:Kq7d', 'a=ice-pwd:2bQh8bMEWJ5J5OejmF1X1lgD', 'a=ice-options:trickle',
  `a=fingerprint:sha-256 ${FP}`, 'a=setup:actpass', 'a=mid:0', 'a=sctp-port:5000', 'a=max-message-size:262144', '',
].join('\r\n');

test('offer round trip keeps everything a data channel needs', () => {
  const blob = encodeBlob(CHROME_OFFER, 'offer');
  assert.ok(blob.startsWith(BLOB_PREFIX));
  assert.ok(blob.length < 200, `blob should be QR-friendly, got ${blob.length} chars`);
  assert.match(blob.slice(BLOB_PREFIX.length), /^[A-Za-z0-9_-]+$/);
  const d = decodeBlob(blob);
  assert.equal(d.kind, 'offer');
  assert.equal(d.setup, 'actpass');
  assert.equal(d.ufrag, 'Kq7d');
  assert.equal(d.pwd, '2bQh8bMEWJ5J5OejmF1X1lgD');
  assert.deepEqual(d.fingerprint, { alg: 'sha-256', hex: FP });
  assert.equal(d.mid, '0');
  assert.equal(d.sctpPort, 5000);
  assert.equal(d.maxMessageSize, 262144);
  const addrs = d.candidates.map((c) => `${c.type}/${c.tcp ? 'tcp' : 'udp'}/${c.address}:${c.port}`);
  assert.deepEqual(addrs, [
    'host/udp/192.168.1.23:54321',
    'host/udp/0d8e5c36-9b2f-4c4a-8f1e-1a2b3c4d5e6f.local:61000',
    'host/udp/fe80::1c2b:3aff:fe4d:5e6f:50000',
    'srflx/udp/203.0.113.7:54321',
    'host/tcp/192.168.1.23:443',
  ], 'IPv4 host first; duplicates, active-tcp and component-2 dropped');
});

test('rebuilt SDP parses back to the same essentials', () => {
  const d = decodeBlob(encodeBlob(CHROME_OFFER, 'offer'));
  const sdp = buildSdp(d);
  assert.match(sdp, /^v=0\r\n/);
  assert.match(sdp, /m=application 9 UDP\/DTLS\/SCTP webrtc-datachannel/);
  assert.match(sdp, /a=end-of-candidates/);
  const p = parseSdp(sdp);
  assert.equal(p.ufrag, d.ufrag);
  assert.equal(p.pwd, d.pwd);
  assert.deepEqual(p.fingerprint, d.fingerprint);
  assert.equal(p.candidates.length, d.candidates.length);
  // priorities are ordered like the blob (host > srflx)
  const prios = sdp.split('\r\n').filter((l) => l.startsWith('a=candidate:')).map((l) => Number(l.split(' ')[3]));
  assert.ok(prios[0] > prios[3]);
  // and re-encoding is stable
  assert.equal(encodeBlob(sdp, 'offer'), encodeBlob(CHROME_OFFER, 'offer'));
});

test('answer with Firefox-style credentials and sha-512 fingerprint', () => {
  const fp512 = Array.from({ length: 64 }, (_, i) => (i * 37 % 256).toString(16).toUpperCase().padStart(2, '0')).join(':');
  const sdp = `v=0\r\na=ice-ufrag:3f1c9a2b\r\na=ice-pwd:0123456789abcdef0123456789abcdef\r\na=fingerprint:sha-512 ${fp512}\r\na=setup:active\r\na=mid:data\r\na=candidate:0 1 UDP 2122252543 10.0.0.5 40000 typ host\r\n`;
  const d = decodeBlob(encodeBlob(sdp, 'answer'));
  assert.equal(d.kind, 'answer');
  assert.equal(d.setup, 'active');
  assert.equal(d.mid, 'data');
  assert.equal(d.fingerprint.alg, 'sha-512');
  assert.equal(d.fingerprint.hex, fp512);
  assert.equal(d.maxMessageSize, 0);
  assert.deepEqual(d.candidates, [{ type: 'host', address: '10.0.0.5', port: 40000, tcp: false }]);
});

test('no candidates and many candidates', () => {
  const base = `a=ice-ufrag:abcd\r\na=ice-pwd:abcdefghijklmnopqrstuvwx\r\na=fingerprint:sha-256 ${FP}\r\n`;
  assert.equal(decodeBlob(encodeBlob(base, 'offer')).candidates.length, 0);
  let many = base;
  for (let i = 0; i < 20; i++) many += `a=candidate:${i} 1 udp 1 10.0.${i}.1 ${1000 + i} typ host\r\n`;
  assert.equal(decodeBlob(encodeBlob(many, 'offer')).candidates.length, MAX_CANDIDATES);
});

test('IPv6 forms survive the round trip', () => {
  for (const [addr, want] of [['::1', '::1'], ['2001:db8::8a2e:370:7334', '2001:db8::8a2e:370:7334'],
    ['2001:db8:0:0:1:0:0:1', '2001:db8::1:0:0:1'], ['::ffff:192.168.1.1', '::ffff:c0a8:101']]) {
    const sdp = `a=ice-ufrag:abcd\r\na=ice-pwd:abcdefghijklmnopqrstuvwx\r\na=fingerprint:sha-256 ${FP}\r\na=candidate:1 1 udp 1 ${addr} 9999 typ host\r\n`;
    assert.equal(decodeBlob(encodeBlob(sdp, 'offer')).candidates[0].address, want, addr);
  }
});

test('hostile / damaged input is rejected with bad_params, never crashes', () => {
  const blob = encodeBlob(CHROME_OFFER, 'offer');
  const body = blob.slice(BLOB_PREFIX.length);
  const cases = ['', 'ember:rtc:', 'hello world', '!!!!', body.slice(0, 10), body.slice(0, -1), body + 'AA',
    `${body.slice(0, 20)}${body[20] === 'A' ? 'B' : 'A'}${body.slice(21)}`];
  for (const c of cases) assert.throws(() => decodeBlob(c), (e) => e.code === 'bad_params', JSON.stringify(c));
  // every single-character corruption is caught by the CRC or the structure checks
  const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_';
  let caught = 0, total = 0;
  for (let i = 0; i < body.length; i++) {
    const ch = alphabet[(alphabet.indexOf(body[i]) + 1) % 64];
    const bad = body.slice(0, i) + ch + body.slice(i + 1);
    total++;
    try { decodeBlob(bad); } catch (e) { assert.equal(e.code, 'bad_params'); caught++; }
  }
  assert.ok(caught >= total - 1, `${caught}/${total} corruptions detected`); // last char may only touch pad bits
  // whitespace / line wrapping from copy-paste is fine, prefix is case-insensitive
  const wrapped = `EMBER:RTC:${body.replace(/(.{20})/g, '$1\n  ')}`;
  assert.equal(decodeBlob(wrapped).ufrag, 'Kq7d');
  // fuzz: random strings never throw anything but BlobError
  for (let i = 0; i < 300; i++) {
    const s = Array.from({ length: 1 + (i % 90) }, () => alphabet[Math.floor(Math.random() * 64)]).join('');
    try { decodeBlob(s); } catch (e) { assert.equal(e.code, 'bad_params'); }
  }
});

test('encode refuses SDP without credentials or fingerprint', () => {
  assert.throws(() => encodeBlob('v=0\r\n', 'offer'), { code: 'bad_params' });
  assert.throws(() => encodeBlob('a=ice-ufrag:x\r\na=ice-pwd:y\r\n', 'offer'), { code: 'bad_params' });
  assert.throws(() => encodeBlob(`a=ice-ufrag:x\r\na=ice-pwd:y\r\na=fingerprint:md5 AA:BB\r\n`, 'offer'), { code: 'bad_params' });
});

test('candidate parser', () => {
  assert.deepEqual(parseCandidate('candidate:1 1 udp 5 1.2.3.4 99 typ srflx raddr 0.0.0.0 rport 0'), { address: '1.2.3.4', port: 99, type: 'srflx', tcp: false });
  assert.equal(parseCandidate('candidate:1 2 udp 5 1.2.3.4 99 typ host'), null);
  assert.equal(parseCandidate('candidate:1 1 udp 5 1.2.3.4 0 typ host'), null);
  assert.equal(parseCandidate('candidate:1 1 sctp 5 1.2.3.4 9 typ host'), null);
  assert.equal(parseCandidate('garbage'), null);
});
