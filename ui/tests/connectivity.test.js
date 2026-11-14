// Nearby devices and the internet relay (docs/RPC_API.md §11, docs/CONNECTIVITY.md): the UI helpers
// (lib/connectivity.js), the mock backend's connectivity.* in every state, and the web host's relay
// plumbing (endpoints/router.js syncRelays + endpoints/websocket.js subprotocols / wss rule).
import test from 'node:test';
import assert from 'node:assert/strict';
import {
  relayUrlProblem, relayStateText, connectivitySummary, linkRemovedText, autoKind, reachText, topicsPausedText, linkNameFor,
  capabilities, internetNoticeOf, nearbyNoticeOf, nearbyPromptText, relayHost, isLanHost, isRelaySession, THIRD_PARTY_LINE,
} from '../js/lib/connectivity.js';
import { checkRelays, DEFAULT_RELAYS, NEARBY_NOTICE, internetNotice, SESSION_BASE } from '../js/mock/connectivity.js';
import MockBackend from '../js/backends/mock.js';
import { HostRouter } from '../js/endpoints/router.js';
import websocket, { wsUrlProblem } from '../js/endpoints/websocket.js';

const sleep = ms => new Promise(r => setTimeout(r, ms));
const CONTRACT_NOTICE = 'Send messages over the internet when no other link reaches your contacts. They travel through a public relay server, which cannot read them but can see your IP address, when you are online and which other users you exchange messages with.';

// ------------------------------------------------------------------ helpers
test('notices come from the core, never from the UI', () => {
  const c = { nearby: { notice: ' Nearby text from the core. ' }, internet: { notice: 'Internet text from the core.' } };
  assert.equal(nearbyNoticeOf(c), 'Nearby text from the core.');
  assert.equal(internetNoticeOf(c), 'Internet text from the core.');
  assert.equal(internetNoticeOf({ internet: {} }), '', 'nothing is made up when the core sends none');
  assert.equal(nearbyNoticeOf(null), '');
  // the mock "core": the nearby notice says what nearby devices can see; the internet one names relays
  assert.match(NEARBY_NOTICE, /can see your name and your EmberMesh identity/);
  assert.match(internetNotice(DEFAULT_RELAYS), /broker\.hivemq\.com:8884/);
  assert.doesNotMatch(internetNotice(DEFAULT_RELAYS), /emqx|mosquitto/, 'only enabled relays are named');
  assert.match(THIRD_PARTY_LINE, /free services .* no guarantee/);
});

test('relay URL rules: wss://; ws:// only when allowed AND on the local network; no duplicates; ≤ 8', () => {
  assert.equal(relayUrlProblem('wss://relay.example.org/mqtt'), null);
  assert.match(relayUrlProblem(''), /Enter/);
  assert.match(relayUrlProblem('relay.example.org'), /not a web address/);
  assert.match(relayUrlProblem('https://relay.example.org'), /starts with wss/);
  assert.match(relayUrlProblem('ws://192.168.1.20:8437/mqtt'), /Use wss/);
  assert.equal(relayUrlProblem('ws://192.168.1.20:8437/mqtt', true), null);
  assert.equal(relayUrlProblem('ws://hub.local/mqtt', true), null);
  assert.match(relayUrlProblem('ws://relay.example.org/mqtt', true), /on your own network/);
  assert.match(relayUrlProblem('wss://a.example/mqtt', false, [{ url: 'wss://a.example/mqtt' }]), /already/);
  assert.match(relayUrlProblem('wss://b.example/mqtt', false, new Array(8).fill(0).map((_, i) => ({ url: 'wss://x' + i + '.example' }))), /At most 8/);
  for (const hst of ['10.0.0.2', '172.16.5.1', '172.31.0.1', '192.168.0.1', '169.254.1.2', 'fe80::1', 'fd12:3456::1', 'hub.local']) assert.ok(isLanHost(hst), hst);
  // just outside the private range, built so no public address literal appears in the source
  const outside172 = [172, 16 + 16, 0, 1].join('.');
  for (const hst of ['203.0.113.5', outside172, 'example.org', '2001:db8::1', 'local']) assert.ok(!isLanHost(hst), hst);
  assert.equal(relayHost('wss://broker.emqx.io:8084/mqtt'), 'broker.emqx.io:8084');
});

test('states in plain words: relays (incl. "off"), summary, every removal reason, paused topics', () => {
  assert.equal(relayStateText({ enabled: true, state: 'up' }), 'Connected');
  assert.equal(relayStateText({ enabled: true, state: 'connecting' }), 'Connecting…');
  assert.equal(relayStateText({ enabled: true, state: 'error', error: 'timeout' }), 'Not connected: timeout');
  assert.equal(relayStateText({ enabled: false, state: 'off' }), 'Not used');
  assert.equal(relayStateText({ enabled: true, state: 'off' }), 'Off');
  assert.equal(connectivitySummary({ ready: true, nearby: { enabled: true, links: 2 }, internet: { enabled: false } }), '2 nearby devices connected · internet relay off');
  assert.equal(connectivitySummary({ ready: true, nearby: { enabled: false }, internet: { enabled: true, relays: [{ enabled: true, state: 'up' }] } }), 'Nearby devices off · internet relay connected (1)');
  assert.match(connectivitySummary({ ready: false, setup_done: false, nearby: {}, internet: {} }), /Not started yet/);
  const reasons = { duplicate: /another connection already reaches/, nearby_off: /Nearby devices was turned off/, hello_failed: /not an EmberMesh device/, evicted: /make room/, relay_off: /internet relay was turned off/, relay_removed: /switched off or removed/, locked: /locked/, wipe: /erased/ };
  for (const [r, re] of Object.entries(reasons)) assert.match(linkRemovedText(r, 'Nearby: Maya'), re, r);
  assert.equal(linkRemovedText('something-new'), 'A connection was closed.');
  assert.equal(topicsPausedText({ topics_paused: 0 }), '');
  assert.match(topicsPausedText({ topics_paused: 2 }), /Paused for 2 contacts or channels.*resumes by itself/);
  assert.equal(autoKind({ auto: 'relay' }).label, 'Automatic · internet relay');
  assert.equal(autoKind({ auto: 'nearby' }).label, 'Automatic · nearby device');
});

test('relay sessions read "internet relay"; Contact.reach in words, never delivery', () => {
  assert.ok(isRelaySession(0x40000000) && isRelaySession(0x40000123) && !isRelaySession(3));
  const links = [{ id: 3, name: 'T-Beam LoRa', type: 'meshtastic' }, { id: 7, name: 'Nearby: Maya', type: 'stream' }];
  assert.equal(linkNameFor(0x40000001, links), 'internet relay');
  assert.equal(linkNameFor(3, links), 'T-Beam LoRa');
  assert.equal(reachText({ path: 'nearby', link_id: 7 }, links).text, 'Nearby now (same Wi-Fi).');
  assert.equal(reachText({ path: 'radio', link_id: 3 }, links).text, 'In radio range now (T-Beam LoRa).');
  assert.equal(reachText({ path: 'network', link_id: 1 }, [{ id: 1, name: 'Home Wi-Fi LAN' }]).text, 'Connected now over Home Wi-Fi LAN.');
  assert.equal(reachText({ path: 'relay', link_id: 0x40000001 }).text, 'Reachable through the internet relay.');
  const none = reachText({ path: 'none', heard_s: 7200 });
  assert.equal(none.state, 'none');
  assert.match(none.text, /Not reachable right now\. Last heard 2 hours ago\. Messages wait on this device and are sent when a connection to them exists\./);
  assert.match(reachText(undefined).text, /^Not reachable right now\./);
  for (const p of ['nearby', 'network', 'radio', 'relay', 'none']) assert.doesNotMatch(reachText({ path: p }).text, /delivered/i);
});

test('capabilities follow the host features, the core and ready', () => {
  const conn = { ready: true, nearby: {}, internet: {} };
  assert.deepEqual(capabilities({ features: ['nearby', 'relay'], websocket_tls: true }, conn), { core: true, nearby: true, relay: true, ready: true });
  assert.equal(capabilities({ features: ['nearby', 'relay'], websocket_tls: false }, conn).relay, false, 'no TLS: no relay');
  assert.deepEqual(capabilities({ features: ['nearby', 'relay'] }, null), { core: false, nearby: false, relay: false, ready: false }, 'older core');
  assert.equal(capabilities({ features: [] }, { ready: false }).ready, false);
  assert.match(nearbyPromptText('ios'), /local network\.$/);
  assert.match(nearbyPromptText('android'), /local network/);
  for (const p of ['ios', 'android', 'web', 'desktop']) assert.doesNotMatch(nearbyPromptText(p), /Bluetooth/);
  assert.match(nearbyPromptText('desktop'), /No permission/);
});

// ------------------------------------------------------------------ mock backend (revision 2)
// init() is what a page does first: it starts the node (and with it the automatic links)
const mock = (q = '') => { const b = new MockBackend({ persist: false, speed: 0.01, autoplay: false, latency: 0, params: new URLSearchParams(q) }); b.ready = b.init(); return b; };

test('mock connectivity.get: defaults per revision 2', async () => {
  const b = mock();
  await b.ready;
  const c = await b.call('connectivity.get');
  assert.equal(c.ready, true);
  assert.equal(c.setup_done, true);
  assert.deepEqual([c.nearby.enabled, c.nearby.active], [true, true]);
  assert.match(c.nearby.token, /^[0-9a-f]{8}$/);
  assert.equal(c.nearby.notice, NEARBY_NOTICE);
  assert.deepEqual([c.internet.enabled, c.internet.active], [false, false]);
  assert.equal(c.internet.consent_ts, undefined);
  assert.ok(c.internet.notice.length > 40);
  assert.equal('bridge_all' in c.internet, false, 'no bridge-all option');
  assert.deepEqual(c.internet.relays.map(r => [r.url, r.enabled, r.state]), [
    ['wss://broker.hivemq.com:8884/mqtt', true, 'off'], ['wss://broker.emqx.io:8084/mqtt', false, 'off'], ['wss://test.mosquitto.org:8081/mqtt', false, 'off']]);
  assert.match(c.internet.relays[1].note, /Off by default: anyone .* can watch topic names/);
  await new Promise(r => setTimeout(r, 60));
  assert.equal((await b.call('connectivity.get')).nearby.links, 2);
  const links = (await b.call('links.list')).links;
  assert.equal(links.filter(l => l.auto === 'relay').length, 0, 'nothing contacts a relay before consent');
  const nb = links.filter(l => l.auto === 'nearby');
  assert.equal(nb.length, 2);
  assert.ok(nb.every(l => l.config.preset === 'nearby' && l.persist === false && /@192\.0\.2\./.test(l.auto_key)));
  assert.ok(nb.some(l => /^Nearby: /.test(l.name)), 'named after the device once its identity is proven');
  await assert.rejects(b.call('settings.set', { settings: { 'connectivity.internet': true } }), { code: 'bad_params' });
  await assert.rejects(b.call('connectivity.set', { relay_bridge_all: true }), { code: 'bad_params' });
  const t = await b.call('connectivity.network_changed');
  assert.match(t.token, /^[0-9a-f]{8}$/);
  b.close();
});

test('mock gating: nothing starts before setup_done; setup applies the choices; lock closes everything', async () => {
  const b = mock('upgrade=1');
  await b.ready;
  await new Promise(r => setTimeout(r, 40));
  let c = await b.call('connectivity.get');
  assert.deepEqual([c.ready, c.setup_done, c.nearby.active], [false, false, false]);
  assert.equal((await b.call('links.list')).links.filter(l => l.auto).length, 0, 'no discovery before the disclosure');
  c = await b.call('connectivity.set', { setup_done: true, nearby: true, internet: true });
  assert.deepEqual([c.ready, c.nearby.active, c.internet.active], [true, true, true]);
  await assert.rejects(b.call('connectivity.set', { setup_done: false }), { code: 'bad_params' }, 'cannot be unset');
  await new Promise(r => setTimeout(r, 60));
  assert.ok((await b.call('links.list')).links.some(l => l.auto === 'relay'));
  const removed = [];
  b.on('link.removed', d => removed.push(d.reason));
  await b.call('security.set_passphrase', { new: 'correct horse battery staple' });
  await b.call('security.lock');
  assert.ok(removed.length >= 3 && removed.every(r => r === 'locked'), JSON.stringify(removed));
  assert.equal(b.links.size === 0 || [...b.links.values()].every(l => !l.auto), true, 'no automatic link while locked');
  b.close();
});

test('mock consent: relay links connect, relays named in the notice; off → "relay_off"; disabling one → "relay_removed"', async () => {
  const b = mock();
  await b.ready;
  const removed = [];
  b.on('link.removed', d => removed.push(d.reason));
  const t0 = Date.now() - 1000;
  const c = await b.call('connectivity.set', { internet: true });
  assert.ok(c.internet.consent_ts >= t0);
  await new Promise(r => setTimeout(r, 60));
  const on = await b.call('connectivity.get');
  assert.deepEqual(on.internet.relays.map(r => r.state), ['up', 'off', 'off']);
  const rl = (await b.call('links.list')).links.filter(l => l.auto === 'relay');
  assert.deepEqual(rl.map(l => l.auto_key), ['wss://broker.hivemq.com:8884/mqtt']);
  assert.equal(rl[0].caps.mtu, 0, 'the relay link itself carries no mesh traffic');
  assert.deepEqual(rl[0].endpoint.config.protocols, ['mqtt']);
  assert.equal(rl[0].status.mqtt_state, 'up');
  // the far contact: heard through a relay session, Contact.reach says relay
  const tom = (await b.call('contacts.list')).contacts.find(x => x.node_id === '1d6e3f8a2c9b4071');
  assert.equal(tom.reach.path, 'relay');
  assert.ok(tom.reach.link_id >= SESSION_BASE);
  const maya = (await b.call('contacts.list')).contacts.find(x => x.node_id === 'b7e4c1d2a9f03e56');
  assert.equal(maya.reach.path, 'nearby');
  await b.call('connectivity.set', { relays: DEFAULT_RELAYS.map(r => ({ ...r, enabled: false })) });
  assert.deepEqual(removed, ['relay_removed']);
  await b.call('connectivity.set', { relays: DEFAULT_RELAYS, internet: false });
  assert.equal((await b.call('links.list')).links.filter(l => l.auto === 'relay').length, 0);
  await b.call('connectivity.set', { internet: true });
  await new Promise(r => setTimeout(r, 30));
  await b.call('connectivity.set', { internet: false });
  assert.equal(removed.at(-1), 'relay_off');
  await b.call('connectivity.set', { nearby: false });
  assert.equal(removed.at(-1), 'nearby_off');
  b.close();
});

test('mock: relay failure, paused topics, relay list validation, passwords never returned', async () => {
  let b = mock('internet=1&relay=fail');
  await b.ready;
  await new Promise(r => setTimeout(r, 60));
  let c = await b.call('connectivity.get');
  assert.equal(c.internet.relays[0].state, 'error');
  assert.match(c.internet.relays[0].error, /did not answer/);
  b.close();
  b = mock('internet=1&paused=1');
  await b.ready;
  await new Promise(r => setTimeout(r, 60));
  const rl = (await b.call('links.list')).links.find(l => l.auto === 'relay');
  assert.match(topicsPausedText(rl.status), /Paused for 2/);
  await assert.rejects(b.call('connectivity.set', { relays: [{ url: 'ws://192.168.1.20:8437/mqtt' }] }), { code: 'bad_params' });
  await assert.rejects(b.call('connectivity.set', { relays: [{ url: 'ws://relay.example.org/mqtt', allow_plaintext: true }] }), { code: 'bad_params' }, 'plaintext only on the LAN');
  await assert.rejects(b.call('connectivity.set', { relays: new Array(9).fill(0).map((_, i) => ({ url: 'wss://r' + i + '.example/mqtt' })) }), { code: 'bad_params' });
  c = await b.call('connectivity.set', { relays: [...DEFAULT_RELAYS, { url: 'ws://192.168.1.20:8437/mqtt', allow_plaintext: true, username: 'u', password: 'secret' }] });
  const mine = c.internet.relays.at(-1);
  assert.deepEqual([mine.allow_plaintext, mine.username, mine.password, mine.note], [true, 'u', undefined, undefined]);
  b.close();
  assert.deepEqual(checkRelays([{ url: ' wss://x.example/mqtt ', enabled: false }]), [{ url: 'wss://x.example/mqtt', enabled: false }]);
});

test('mock channels: relay only when allowed (random key or strong passphrase); invites carry r=1', async () => {
  const b = mock();
  await b.ready;
  const { channel } = await b.call('channels.create', { name: 'Far camp', random_key: true });
  assert.equal(channel.relay_allowed, true);
  assert.equal(channel.relay, false);
  assert.equal((await b.call('channels.update', { id: channel.id, relay: true })).channel.relay, true);
  assert.match((await b.call('channels.invite', { id: channel.id })).invite, /[?&]r=1(&|$)/);
  const weak = (await b.call('channels.create', { name: 'Weak', passphrase: 'abc' })).channel;
  assert.equal(weak.relay_allowed, false);
  await assert.rejects(b.call('channels.update', { id: weak.id, relay: true }), { code: 'denied' });
  const pub = (await b.call('channels.list')).channels.find(x => x.is_default);
  assert.equal(pub.relay_allowed, false, 'open channels have no relay topic');
  b.close();
});

test('mock: an older core (no connectivity.*) and a host without nearby/relay', async () => {
  let b = mock('unsupported=connectivity.get');
  await assert.rejects(b.call('connectivity.get'), { code: 'not_implemented' });
  await b.ready;
  await new Promise(r => setTimeout(r, 40));
  assert.equal((await b.call('links.list')).links.filter(l => l.auto).length, 0, 'no automatic links');
  b.close();
  b = mock('unsupported=feature:nearby,feature:relay');
  const hi = await b.call('host.info');
  assert.ok(!hi.features.includes('nearby') && !hi.features.includes('relay'));
  const caps = capabilities(hi, await b.call('connectivity.get'));
  assert.deepEqual([caps.core, caps.nearby, caps.relay], [true, false, false]);
  b.close();
});

// ------------------------------------------------------------------ web host: relay links
function fakeHost(conn) {
  const links = new Map();
  let next = 10;
  const calls = [];
  const core = {
    async call(m, p) {
      calls.push([m, p]);
      if (m === 'links.list') return { links: [...links.values()] };
      if (m === 'links.add') {
        if (p.type === 'relay' && !conn.internet.enabled) throw Object.assign(new Error('no consent'), { code: 'denied' });
        const l = { id: next++, ...p, state: 'down' };
        links.set(l.id, l);
        return { link: l };
      }
      if (m === 'links.remove') { links.delete(p.id); return {}; }
      if (m === 'connectivity.get') return conn;
      return {};
    },
    linkRx() {}, kick() {},
  };
  const sockets = [];
  class FakeWS { constructor(url, protocols) { this.url = url; this.protocols = protocols; this.closed = false; sockets.push(this); } close() { this.closed = true; } send() {} }
  const env = { WebSocket: FakeWS, location: { protocol: 'https:', host: 'app.example' } };
  const r = new HostRouter({ core, env, endpoints: [websocket], log: { warn() {}, error() {}, log() {} } });
  return { r, links, calls, sockets };
}

test('web host: no relay link before consent; one per enabled relay after, with Sec-WebSocket-Protocol mqtt', async () => {
  const conn = { ready: true, nearby: { enabled: true, active: true }, internet: { enabled: false, active: false, relays: DEFAULT_RELAYS.map(r => ({ ...r, enabled: true })) } };
  const { r, links, calls, sockets } = fakeHost(conn);
  assert.ok(r.features().includes('relay'));
  assert.ok(!r.features().includes('nearby'), 'a web page cannot discover nearby devices');
  await r.syncRelays(conn);
  assert.equal(calls.filter(([m]) => m === 'links.add').length, 0);
  assert.equal(sockets.length, 0, 'nothing connects before consent');
  conn.internet.enabled = true;
  await r.syncRelays(conn);
  assert.equal(sockets.length, 0, 'enabled but not active (setup not done / locked): still nothing');
  conn.internet.active = true;
  await r.syncRelays(conn);
  const added = calls.filter(([m]) => m === 'links.add').map(([, p]) => p);
  assert.deepEqual(added.map(p => p.auto_key), DEFAULT_RELAYS.map(r => r.url));
  for (const p of added) {
    assert.equal(p.type, 'relay');
    assert.equal(p.auto, 'relay');
    assert.equal(p.persist, false);
    assert.deepEqual(p.endpoint, { kind: 'websocket', config: { url: p.auto_key, protocols: ['mqtt'] } });
  }
  assert.deepEqual(sockets.map(s => [s.url, s.protocols]), added.map(p => [p.auto_key, ['mqtt']]));
  // again: nothing duplicated
  await r.syncRelays(conn);
  assert.equal(calls.filter(([m]) => m === 'links.add').length, DEFAULT_RELAYS.length);
  // a relay switched off → its link removed and socket closed
  conn.internet.relays[1].enabled = false;
  await r.syncRelays(conn);
  assert.deepEqual([...links.values()].map(l => l.auto_key), [DEFAULT_RELAYS[0].url, DEFAULT_RELAYS[2].url]);
  assert.ok(sockets[1].closed);
  // the core removes the rest after consent is withdrawn: link.removed closes the endpoint
  const id = [...links.keys()][0];
  r.onCoreEvent('link.removed', { id, reason: 'consent' });
  assert.ok(sockets[0].closed);
  assert.ok(!r.links.has(id));
});

test('websocket endpoint: a relay needs wss://; ws:// only with allow_plaintext AND a LAN host', () => {
  const env = { location: { protocol: 'http:', host: 'x' } };
  assert.equal(wsUrlProblem('wss://broker.emqx.io:8084/mqtt', env, { relay: true }), null);
  assert.match(wsUrlProblem('ws://192.168.1.20:8437/mqtt', env, { relay: true }), /must use wss/);
  assert.equal(wsUrlProblem('ws://192.168.1.20:8437/mqtt', env, { relay: true, allowPlaintext: true }), null);
  assert.match(wsUrlProblem('ws://relay.example.org/mqtt', env, { relay: true, allowPlaintext: true }), /must use wss/);
  assert.equal(wsUrlProblem('ws://192.168.1.20:8437/ws', env), null, 'hub links are unchanged');
});
