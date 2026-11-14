// Seed data for the mock backend: fake world, contacts, messages, links, catalogues.

import { CORE_LINK_TYPES } from './link_types.js';

export const HOME = { lat: 39.8283, lon: -98.5795 }; // geographic centre of the contiguous US (public landmark)
export const OWN_ID = 'a1b2c3d4e5f60718';

export const WORDS = [
  'amber', 'anchor', 'apple', 'arrow', 'badger', 'banjo', 'beacon', 'birch', 'bison', 'cabin',
  'canyon', 'cedar', 'cobalt', 'comet', 'coral', 'delta', 'desert', 'dolphin', 'eagle', 'ember',
  'falcon', 'fern', 'forest', 'galaxy', 'garnet', 'glacier', 'granite', 'harbor', 'hazel', 'heron',
  'island', 'jasper', 'juniper', 'kayak', 'kettle', 'lantern', 'lemon', 'lilac', 'lotus', 'maple',
  'meadow', 'meteor', 'nectar', 'nickel', 'oasis', 'olive', 'orbit', 'otter', 'pepper', 'pine',
  'planet', 'quartz', 'raven', 'river', 'saddle', 'salmon', 'summit', 'thunder', 'tiger', 'tundra',
  'velvet', 'violet', 'walnut', 'willow',
];

export function fingerprint(nodeId) {
  const out = [];
  for (let i = 0; i < 8; i++) { // 8 words since protocol v2 (first 6 = the old ones)
    const b = parseInt(nodeId.substr((i * 2) % 16, 2), 16) || 0;
    out.push(WORDS[(b ^ (i * 37) ^ (parseInt(nodeId.substr(15 - i, 1), 16) << 2)) % WORDS.length]);
  }
  return out;
}

/** Point `distKm` away from (lat, lon) along `bearingDeg`. */
export function destination(lat, lon, bearingDeg, distKm) {
  const R = 6371.0088, d = distKm / R, b = (bearingDeg * Math.PI) / 180;
  const p1 = (lat * Math.PI) / 180, l1 = (lon * Math.PI) / 180;
  const p2 = Math.asin(Math.sin(p1) * Math.cos(d) + Math.cos(p1) * Math.sin(d) * Math.cos(b));
  const l2 = l1 + Math.atan2(Math.sin(b) * Math.sin(d) * Math.cos(p1), Math.cos(d) - Math.sin(p1) * Math.sin(p2));
  return { lat: +((p2 * 180) / Math.PI).toFixed(6), lon: +((l2 * 180) / Math.PI).toFixed(6) };
}

// The fake mesh around us. `contact` = present in our contact list at seed time.
export const WORLD = [
  { node_id: 'b7e4c1d2a9f03e56', name: 'Maya Chen', callsign: 'QX0MCH', status: 'Clinic radio desk', bearing: 40, km: 1.2, hops: 0, link: 1, rssi: -71, snr: 9.5, seenAgoMin: 2, contact: true, verified: true, favorite: true },
  { node_id: 'c3d9a0e8f1b27c44', name: 'Dr. Okafor', callsign: '', status: 'Clinic open until dark', bearing: 185, km: 3.4, hops: 1, link: 3, rssi: -104, snr: -3.5, seenAgoMin: 9, contact: true, verified: true, favorite: false },
  { node_id: 'd41f8e2b6c7a9035', name: 'Ridge Camp Relay', callsign: 'QX0RDG', status: 'Solar relay, 2 m + LoRa', bearing: 95, km: 14.1, hops: 0, link: 2, snr: 12, seenAgoMin: 1, contact: true, verified: false, favorite: true },
  { node_id: 'e8a2b5c4d3f10967', name: 'Sam Ortiz', callsign: '', status: 'Shelter volunteer', bearing: 270, km: 0.6, hops: 0, link: 1, rssi: -58, snr: 11, seenAgoMin: 4, contact: true, verified: false, favorite: true },
  { node_id: 'f02c6d9e8b1a4573', name: 'Lena Novak', callsign: '', status: 'Hiking out via Cedar trail', bearing: 352, km: 7.8, hops: 2, link: 3, rssi: -112, snr: -7, seenAgoMin: 25, contact: true, verified: false, favorite: false },
  { node_id: '1d6e3f8a2c9b4071', name: 'Tom Becker', callsign: 'QX0TBK', status: '', bearing: 225, km: 9.5, hops: 3, link: 2, snr: 3, seenAgoMin: 60 * 46, contact: true, verified: false, favorite: false },
  { node_id: '7a3e91c0d55b28f4', name: 'Hilltop Node', callsign: '', status: 'Unattended repeater', bearing: 130, km: 5.2, hops: 1, link: 3, rssi: -98, snr: 1.5, seenAgoMin: 6, contact: false },
  { node_id: '0c5b7e2a9d4f6138', name: '', callsign: '', status: '', bearing: 300, km: 12.6, hops: null, link: 3, seenAgoMin: 80, contact: false, noPosition: true },
];

export const CANNED_REPLIES = [
  'Copy that.',
  'Roger, will relay to the others.',
  'Thanks. Signal is weak here, keeping it short.',
  'Understood. Check in again at 19:00?',
  'On my way, ETA about 40 minutes.',
  'We have spare water filters if anyone needs one.',
  'Heard you 5 by 5 via the ridge relay.',
  'Good to hear from you. All OK here.',
];

export const PUBLIC_CHATTER = [
  'Generator at the community hall runs 18:00 to 21:00 for charging phones.',
  'Bridge on Oak St is passable on foot only.',
  'Anyone have a spare 12 V car charger? Can trade batteries.',
  'Heard the ridge relay loud and clear from the east side.',
  'Reminder: boil water from the creek for at least one minute.',
  'Road 12 crews clearing debris, expect delays.',
  'Lost dog near the school: brown lab, answers to Biscuit.',
];

export const INTEROP_CHATTER = [
  { network: 'APRS', from: 'QX0XYZ-9', to: 'EMBER', text: 'Mobile on the highway eastbound, heading to the county depot with supplies.' },
  { network: 'Meshtastic', from: '!a4c2f19e', to: '^all', text: 'Testing from the north ridge, who hears me?' },
  { network: 'RTTY', from: 'QX0QRT', to: 'CQ', text: 'CQ CQ DE QX0QRT QX0QRT K' },
];

export const CHECKIN_STATUSES = ['ok', 'need_help', 'injured', 'moving', 'sheltering'];
export const BULLETIN_CATEGORIES = ['water', 'food', 'medical', 'shelter', 'hazard', 'trade', 'missing', 'transport', 'info'];

const MIN = 60000, HOUR = 60 * MIN, DAY = 24 * HOUR;

export function worldPeer(id) { return WORLD.find(p => p.node_id === id); }
export function peerPosition(p, now) {
  if (p.noPosition) return undefined;
  const pos = destination(HOME.lat, HOME.lon, p.bearing, p.km);
  return { ...pos, alt: 40 + Math.round(p.km * 7), accuracy: 15, ts: now - p.seenAgoMin * MIN };
}

/** Build the seeded mutable state. `empty` => after wipe: no contacts/messages. */
export function seedState(now, { empty = false } = {}) {
  const contacts = new Map();
  const messages = new Map();
  const files = new Map();
  const convMeta = new Map();
  const channels = new Map();
  channels.set('public', { id: 'public', name: 'Public', encrypted: false, is_default: true, muted: false, members_seen: 7 });
  if (!empty) {
    channels.set('5e1d9a70', { id: '5e1d9a70', name: 'Ridge Camp', encrypted: true, is_default: false, muted: false, members_seen: 4, passphrase: 'ridge-camp-2026' });
    for (const p of WORLD) {
      if (!p.contact) continue;
      contacts.set(p.node_id, {
        node_id: p.node_id, name: p.name, callsign: p.callsign, verified: !!p.verified, blocked: false,
        favorite: !!p.favorite, status: p.status, last_seen_ts: now - p.seenAgoMin * MIN, hops: p.hops,
        direct: p.hops === 0, via_link: p.link, ...(p.rssi !== undefined ? { rssi: p.rssi } : {}),
        ...(p.snr !== undefined ? { snr: p.snr } : {}), ...(peerPosition(p, now) ? { position: peerPosition(p, now) } : {}),
        fingerprint: fingerprint(p.node_id),
      });
    }
  }

  let n = 0;
  const id = () => (0x5eed0000 + ++n).toString(16).padStart(24, '0');
  const add = (conv, ago, from, text, extra = {}) => {
    const p = from === 'me' ? null : worldPeer(from);
    const ts = now - ago;
    const isCh = conv.startsWith('ch:');
    const ch = isCh ? channels.get(conv.slice(3)) : null;
    const m = {
      id: id(), conv, from: from === 'me' ? OWN_ID : from, from_name: from === 'me' ? '' : (p ? p.name : from),
      outgoing: from === 'me', ts, received_ts: from === 'me' ? ts : ts + 1500 + (p ? (p.hops || 0) * 900 : 0), text,
      kind: 'text', status: from === 'me' ? 'delivered' : 'received', priority: 'normal',
      encrypted: isCh ? !!(ch && ch.encrypted) : true, signed_ok: true,
      hops: from === 'me' ? 0 : (p ? p.hops : null), via_link: from === 'me' ? 0 : (p ? p.link : 1), ...extra,
    };
    if (from === 'me') m.via_link = isCh && conv === 'ch:public' ? 2 : 1;
    messages.set(m.id, m);
    return m;
  };

  if (empty) {
    convMeta.set('ch:public', { unread: 0, muted: false });
    return { contacts, messages, files, convMeta, channels, bulletins: new Map(), checkins: new Map(), alerts: new Map(), heardPlans: [], nextId: n };
  }

  // Public
  add('ch:public', 3 * HOUR, 'me', 'You joined the Public channel. Messages here are signed but not encrypted.', { kind: 'system', outgoing: false, from: '', from_name: 'EmberMesh', status: 'received', hops: 0 });
  add('ch:public', 2 * HOUR + 50 * MIN, 'd41f8e2b6c7a9035', 'Net check-in tonight 19:00 on 146.520 simplex. Relay is up on solar.');
  add('io:aprs', 2 * HOUR, 'QX0XYZ-9', 'Road 12 closed at mile 41, rockslide.', { kind: 'interop', from: '', from_name: 'QX0XYZ-9', encrypted: false, signed_ok: false, hops: 0, via_link: 2, interop: { network: 'aprs', from: 'QX0XYZ-9', to: 'QX0ALX' } });
  add('ch:public', 100 * MIN, 'b7e4c1d2a9f03e56', 'Water point at the school gym is running again, bring containers.');
  add('ch:public', 80 * MIN, 'c3d9a0e8f1b27c44', 'Check-in: OK. Clinic open until dark.', { kind: 'checkin' });
  const samPos = peerPosition(worldPeer('e8a2b5c4d3f10967'), now);
  add('ch:public', 60 * MIN, 'e8a2b5c4d3f10967', `Position ${samPos.lat.toFixed(5)}, ${samPos.lon.toFixed(5)}`, { kind: 'position', position: samPos });
  add('io:meshtastic', 40 * MIN, '!a4c2f19e', 'Anyone near the river bridge? Need a hand moving supplies.', { kind: 'interop', from: '', from_name: '!a4c2f19e', encrypted: false, signed_ok: false, hops: 0, via_link: 3, interop: { network: 'meshtastic', from: '!a4c2f19e', to: '' } });
  const lena = worldPeer('f02c6d9e8b1a4573');
  add('ch:public', 25 * MIN, lena.node_id, "SOS: twisted ankle near Cedar trailhead, can't walk out. Have water, need a stretcher.", { kind: 'sos', priority: 'emergency', position: peerPosition(lena, now) });
  add('ch:public', 20 * MIN, 'me', 'Copy that, Lena. Dr. Okafor is about 3 km from you, relaying.', { status: 'relayed', encrypted: false });
  convMeta.set('ch:public', { unread: 2, muted: false });
  convMeta.set('io:aprs', { unread: 1, muted: false });
  convMeta.set('io:meshtastic', { unread: 0, muted: false });

  // Ridge Camp (encrypted channel)
  add('ch:5e1d9a70', 5 * HOUR, 'd41f8e2b6c7a9035', 'Battery bank at 78%. Next generator run 06:00.');
  add('ch:5e1d9a70', 4 * HOUR, 'me', 'Can you bring 2 jerry cans tomorrow?', { status: 'relayed' });
  add('ch:5e1d9a70', 3.5 * HOUR, '1d6e3f8a2c9b4071', 'Will do. Also have spare coax and a few PL-259s.');
  const f = add('ch:5e1d9a70', 3 * HOUR, 'd41f8e2b6c7a9035', 'netplan-week46.txt', { kind: 'file', file: { name: 'netplan-week46.txt', mime: 'text/plain', size: 0, complete: true } });
  const fileText = 'Week 46 net plan\n\n19:00 daily  146.520 MHz FM voice  roll call\n19:30 daily  144.390 MHz AFSK1200 messages\nSun 12:00    7.110 MHz LSB  regional net\n';
  f.file.size = fileText.length;
  files.set(f.id, { name: 'netplan-week46.txt', mime: 'text/plain', data_b64: btoa(fileText) });
  convMeta.set('ch:5e1d9a70', { unread: 0, muted: false });

  // DMs
  const maya = 'dm:b7e4c1d2a9f03e56';
  add(maya, 6 * HOUR, 'b7e4c1d2a9f03e56', 'Are you on the 2 m net tonight?');
  add(maya, 6 * HOUR - 5 * MIN, 'me', 'Yes, with the Baofeng on the roof antenna.', { status: 'read' });
  add(maya, 6 * HOUR - 10 * MIN, 'b7e4c1d2a9f03e56', 'Great. Bring the spare battery for the clinic radio.');
  add(maya, 6 * HOUR - 12 * MIN, 'me', 'Will do.', { status: 'read' });
  add(maya, 30 * MIN, 'me', 'Is the bridge on Oak St passable?', { status: 'failed', priority: 'high' });
  add(maya, 10 * MIN, 'b7e4c1d2a9f03e56', "Haven't been there today, will ask around.");
  convMeta.set(maya, { unread: 1, muted: false });

  const ok = 'dm:c3d9a0e8f1b27c44';
  add(ok, DAY + 2 * HOUR, 'me', "How's the supply of oral rehydration salts?", { status: 'read' });
  add(ok, DAY + HOUR, 'c3d9a0e8f1b27c44', 'Low. If you find any, bring them. You can make it: 6 level teaspoons sugar + half a teaspoon salt per litre of clean water.');
  add(ok, 23 * HOUR, 'me', 'Thanks, noted.', { status: 'delivered' });
  convMeta.set(ok, { unread: 0, muted: false });

  const sam = 'dm:e8a2b5c4d3f10967';
  add(sam, 2 * HOUR, 'e8a2b5c4d3f10967', 'Back at the shelter by 8. Cots are set up in the gym.');
  convMeta.set(sam, { unread: 0, muted: false });

  // Bulletins
  const bulletins = new Map();
  const bl = (bid, cat, from, ago, expH, title, text, extra = {}) => {
    const p = from === 'me' ? null : worldPeer(from);
    bulletins.set(bid, { id: bid, from: p ? p.node_id : OWN_ID, from_name: p ? p.name : '', ts: now - ago, expires_ts: now + expH * HOUR, category: cat, title, text, mine: from === 'me', ...extra });
  };
  bl('b00100000000000000000001', 'water', 'b7e4c1d2a9f03e56', 3 * HOUR, 48, 'Drinking water at the school gym', 'Treated water, 10:00-16:00 daily. Bring your own containers, 20 L per household.', { position: destination(HOME.lat, HOME.lon, 30, 1.0) });
  bl('b00100000000000000000002', 'medical', 'c3d9a0e8f1b27c44', 5 * HOUR, 72, 'Field clinic at the fire station', 'Minor injuries, wound care, prescriptions advice. No surgery. Open until dark.', { position: destination(HOME.lat, HOME.lon, 185, 3.3) });
  bl('b00100000000000000000003', 'hazard', 'd41f8e2b6c7a9035', 7 * HOUR, 24, 'Rockslide on Road 12 at mile 41', 'Road fully blocked. Use the forest road via Ridge Camp. Unstable slope, keep away.');
  bl('b00100000000000000000004', 'trade', '1d6e3f8a2c9b4071', DAY, 96, 'Coax and connectors for trade', 'RG-58 (30 m), PL-259 connectors. Looking for AA batteries or canned food.');
  bl('b00100000000000000000005', 'missing', 'e8a2b5c4d3f10967', 9 * HOUR, 120, 'Missing: Elena R., 72', 'Last seen Tuesday near the library, grey coat, uses a cane. Contact Sam at the shelter.');
  bl('b00100000000000000000006', 'food', 'me', 2 * HOUR, 36, 'Surplus rice and beans', 'About 15 kg rice and 8 kg dried beans to share. Pick up at the north end of Elm St.');
  bl('b00100000000000000000007', 'info', 'd41f8e2b6c7a9035', 4 * HOUR, 168, 'Nightly radio net 19:00', 'Roll call on 146.520 MHz FM. Messages relayed to the mesh afterwards.');

  // Check-ins (latest per node)
  const checkins = new Map();
  const ci = (pid, status, note, ago) => {
    const p = worldPeer(pid);
    checkins.set(pid, { node_id: pid, name: p.name, status, note, ...(peerPosition(p, now) ? { position: peerPosition(p, now) } : {}), ts: now - ago });
  };
  ci('b7e4c1d2a9f03e56', 'ok', 'At the clinic', 40 * MIN);
  ci('c3d9a0e8f1b27c44', 'ok', 'Clinic open until dark', 80 * MIN);
  ci('f02c6d9e8b1a4573', 'injured', 'Twisted ankle, Cedar trailhead', 25 * MIN);
  ci('e8a2b5c4d3f10967', 'sheltering', 'At the gym shelter', 2 * HOUR);

  // Received SOS alerts
  const alerts = new Map();
  alerts.set(lena.node_id, { node_id: lena.node_id, name: lena.name, text: "Twisted ankle near Cedar trailhead, can't walk out. Have water, need a stretcher.", position: peerPosition(lena, now), first_ts: now - 25 * MIN, last_ts: now - 2 * MIN, hops: lena.hops, acked: false });
  // Unverified (stranger) alerts, as the core flags them: signed_ok false, kept in a separate table.
  for (const [nid, nm, txt, ago] of [['9f00aa11bb22cc33', 'HELP', 'Trapped, send everyone to the quarry!', 12], ['9f00aa11bb22cc34', 'Rescue', 'Fire at the school, evacuate', 9], ['9f00aa11bb22cc35', '', 'SOS', 7]]) {
    alerts.set(nid, { node_id: nid, name: nm, text: txt, first_ts: now - ago * MIN, last_ts: now - ago * MIN, hops: null, acked: false, signed_ok: false });
  }

  const heardPlans = [{
    from: 'd41f8e2b6c7a9035', from_name: 'Ridge Camp Relay', ts: now - 3 * HOUR,
    entries: [
      { label: 'Evening voice net', freq_hz: 146520000, mode: 'voice', tone: '', schedule: 'Daily 19:00', notes: 'Roll call, FM simplex' },
      { label: 'APRS / packet messages', freq_hz: 144390000, mode: 'afsk1200', schedule: 'Continuous', notes: 'Mesh gateway QX0RDG' },
      { label: 'LoRa mesh', freq_hz: 906875000, mode: 'lora', schedule: 'Continuous', notes: 'Meshtastic LongFast' },
    ],
  }];

  return { contacts, messages, files, convMeta, channels, bulletins, checkins, alerts, heardPlans, nextId: n };
}

// ---------- links ----------
// Link types are the real core's catalogue; endpoint kinds imitate a host that has all of them
// (web + Android + desktop), with compatible link types derived from the core's endpoint_kinds.
export const LINK_TYPES = CORE_LINK_TYPES;

const BAUD = [1200, 4800, 9600, 19200, 38400, 57600, 115200].map(b => ({ value: b, label: `${b}` }));
const typesFor = kind => LINK_TYPES.filter(t => t.endpoint_kinds.includes(kind)).map(t => t.type);
const ek = (kind, title, description, config, extra = {}) => {
  const link_types = typesFor(kind);
  return { kind, title, description, available: true, scannable: false, config, link_types, default_link_type: extra.default_link_type || link_types[0], ...extra };
};

export const ENDPOINT_KINDS = [
  ek('websocket', 'Hub (WebSocket)', 'Join a hub on this Wi-Fi network (a computer running the EmberMesh hub).',
    [{ key: 'url', label: 'Hub address', type: 'text', default: 'ws://192.168.4.1:8437/ws', required: true, help: 'Shown on the hub screen, e.g. ws://192.168.4.1:8437/ws' }]),
  ek('ws_server', 'Accept WebSocket peers', 'Let other devices connect to this one.',
    [{ key: 'port', label: 'Port', type: 'number', default: 8437, min: 1, max: 65535, step: 1 }],
    { available: false, unavailable_reason: 'Browsers cannot accept incoming connections. Use the desktop app.' }),
  ek('tcp', 'TCP connection', 'Connect to a TCP port, e.g. Direwolf KISS (8001), a Meshtastic node (4403) or rtl_tcp (1234).', [
    { key: 'host', label: 'Host', type: 'text', default: '127.0.0.1', required: true },
    { key: 'port', label: 'Port', type: 'number', default: 8001, min: 1, max: 65535, step: 1, required: true },
  ], { default_link_type: 'kiss' }),
  ek('udp', 'Local network (UDP multicast)', 'Find every EmberMesh device on the same Wi-Fi or hotspot automatically.', [
    { key: 'group', label: 'Multicast group', type: 'text', default: '239.83.77.69', advanced: true },
    { key: 'port', label: 'Port', type: 'number', default: 47474, min: 1, max: 65535, step: 1, advanced: true },
  ]),
  ek('serial', 'USB / serial cable', 'TNCs, LoRa modules and radios with a USB or serial port.', [
    { key: 'port', label: 'Port', type: 'text', default: '/dev/ttyUSB0', required: true },
    { key: 'baud', label: 'Baud rate', type: 'select', default: 9600, options: BAUD },
    { key: 'rts', label: 'Assert RTS', type: 'bool', default: false, advanced: true },
    { key: 'dtr', label: 'Assert DTR', type: 'bool', default: true, advanced: true },
  ], { scannable: true, default_link_type: 'kiss' }),
  ek('ble', 'Bluetooth LE device', 'Meshtastic radios, BLE TNCs (Mobilinkd TNC3/4) and RNodes.', [
    { key: 'device_id', label: 'Device', type: 'text', required: true, help: 'Pick from the scan list.' },
    { key: 'service_uuid', label: 'Service UUID', type: 'text', default: '6e400001-b5a3-f393-e0a9-e50e24dcca9e', advanced: true },
  ], { scannable: true, default_link_type: 'meshtastic' }),
  ek('ble_peer', 'Nearby phone (Bluetooth)', 'Phone to phone over Bluetooth LE, about 10-50 m, no other hardware.',
    [{ key: 'device_id', label: 'Phone', type: 'text', required: true }], { scannable: true }),
  ek('bt_spp', 'Bluetooth serial (classic)', 'Classic Bluetooth SPP TNCs and radios (e.g. Kenwood TH-D74, Mobilinkd TNC2).',
    [{ key: 'address', label: 'Device address', type: 'text', required: true }],
    { available: false, unavailable_reason: 'Classic Bluetooth is not available in browsers. Use the Android or desktop app.', default_link_type: 'kiss' }),
  ek('audio', 'Sound card / headset jack', 'Connect a radio by audio cable, or hold the phone to the radio speaker.', [
    { key: 'input_device', label: 'Input (from radio)', type: 'select', default: 'default', options: [{ value: 'default', label: 'Default microphone' }, { value: 'usb', label: 'USB Audio Device' }] },
    { key: 'output_device', label: 'Output (to radio)', type: 'select', default: 'default', options: [{ value: 'default', label: 'Default speaker' }, { value: 'usb', label: 'USB Audio Device' }] },
    { key: 'ptt_link_id', label: 'PTT via serial link', type: 'number', default: 0, min: 0, advanced: true, help: 'Key the transmitter through the RTS/DTR lines of a serial connection; none = VOX.' },
    { key: 'rx_gain', label: 'Input gain', type: 'number', unit: 'dB', default: 0, min: -20, max: 30, step: 1, advanced: true },
  ]),
  ek('rtl_tcp', 'SDR receiver (rtl_tcp)', 'A cheap RTL-SDR dongle served by rtl_tcp: listen without transmitting.', [
    { key: 'host', label: 'Host', type: 'text', default: '127.0.0.1', required: true },
    { key: 'port', label: 'Port', type: 'number', default: 1234, min: 1, max: 65535, step: 1 },
  ]),
  ek('webrtc', 'Browser to browser (WebRTC)', 'Pair two devices on the same network by exchanging codes (QR or copy-paste).', []),
  ek('loopback', 'Loopback (testing)', 'Talks to itself. Useful for testing drivers.', []),
];

export const SCAN_RESULTS = {
  ble: [
    { id: 'ble:00:00:5E:00:53:10', name: 'T-Beam 5310', detail: 'Meshtastic', rssi: -58, config: { device_id: '00:00:5E:00:53:10' } },
    { id: 'ble:00:00:5E:00:53:5E', name: 'TNC4 Mobilinkd', detail: 'KISS TNC (Nordic UART)', rssi: -73, config: { device_id: '00:00:5E:00:53:5E' } },
    { id: 'ble:00:00:5E:00:53:1F', name: 'RNode 531F', detail: 'RNode firmware 1.74', rssi: -81, config: { device_id: '00:00:5E:00:53:1F' } },
  ],
  ble_peer: [
    { id: 'peer:5a1c', name: "Jordan's Pixel", detail: 'EmberMesh 0.1', rssi: -66, config: { device_id: '5a1c' } },
    { id: 'peer:9e02', name: 'iPhone (Priya)', detail: 'EmberMesh 0.1', rssi: -79, config: { device_id: '9e02' } },
  ],
  serial: [
    { id: 'serial:/dev/ttyUSB0', name: '/dev/ttyUSB0', detail: 'CP2102 USB-UART (likely TNC or LoRa module)', config: { port: '/dev/ttyUSB0', baud: 9600 } },
    { id: 'serial:/dev/ttyACM0', name: '/dev/ttyACM0', detail: 'Espressif ESP32-S3 (Meshtastic)', config: { port: '/dev/ttyACM0', baud: 115200 } },
  ],
};

/** Driver config with every default filled in (what links.add echoes on a real core). */
export function linkConfig(type, config = {}) {
  const t = LINK_TYPES.find(x => x.type === type);
  const out = {};
  for (const f of (t && t.config) || []) if (f.default !== undefined) out[f.key] = f.default;
  return { ...out, ...(config || {}) };
}

const MODEM_BPS = { afsk1200: 1200, afsk300: 300, psk2400: 2400, psk4800: 4800, robust31: 31, robust62: 62, robust125: 125, robust250: 250, dtmf: 0, cw: 0, psk31: 0, rtty: 0 };
const TEXT_ONLY = new Set(['dtmf', 'cw', 'psk31', 'rtty']);

/** Caps as the core derives them: amateur operation (licence/amateur/licensed) forbids encryption. */
export function linkCaps(type, config = {}) {
  const ham = config.licence === 'amateur' || config.amateur === true || config.licensed === true;
  const legal = { encryption_allowed: !ham, requires_station_id: ham };
  const base = { mtu: 1400, bitrate_bps: 10000000, broadcast: true, half_duplex: false, reliable: false, ...legal, duty_cycle_permille: 1000, cost: 1 };
  const duty = Number(config.duty_cycle_permille) || 1000;
  switch (type) {
    case 'raw': return { ...base, mtu: Number(config.mtu) || (config.preset === 'loopback' ? 4096 : 1400), bitrate_bps: Number(config.bitrate_bps) || 100000000, reliable: config.preset === 'websocket' || config.preset === 'loopback', cost: Number(config.cost) || 1 };
    case 'stream': return { ...base, mtu: Number(config.mtu) || 2048, bitrate_bps: Number(config.bitrate_bps) || 115200, broadcast: false, duty_cycle_permille: duty, cost: 2 };
    case 'kiss': return { ...base, mtu: Number(config.mtu) || 256, bitrate_bps: Number(config.bitrate_bps) || 1200, half_duplex: true, duty_cycle_permille: duty, cost: 40 };
    case 'audio': case 'rtl_tcp': {
      const m = config.modem || 'afsk1200';
      return { ...base, mtu: TEXT_ONLY.has(m) ? 0 : 256, bitrate_bps: MODEM_BPS[m] ?? 1200, half_duplex: true, duty_cycle_permille: duty, cost: 40 };
    }
    case 'meshtastic': return { ...base, mtu: 230, bitrate_bps: 1070, half_duplex: true, cost: 20 };
    case 'rnode': return { ...base, mtu: 254, bitrate_bps: 1760, half_duplex: true, duty_cycle_permille: Number(config.duty_cycle_permille) || (String(config.region).startsWith('eu868') ? 10 : 1000), cost: 20 };
    case 'lora_at': return { ...base, mtu: 240, bitrate_bps: 980, half_duplex: true, duty_cycle_permille: Number(config.duty_cycle_permille) || (String(config.region).startsWith('eu868') ? 10 : 1000), cost: 20 };
    default: return base;
  }
}

/** Driver status like the core's: common counters, plus the documented meter fields for audio / rtl_tcp. */
export function linkStatus(type, state, extra = {}) {
  const st = { type, state, tx_frames: 0, tx_bytes: 0, rx_frames: 0, rx_bytes: 0, rx_errors: 0, tx_refused: 0, tx_evicted: 0, interop_rx: 0, interop_tx: 0, airtime_ms: 0, queue: 0, queue_bytes: 0 };
  if (type === 'audio' || type === 'rtl_tcp') {
    Object.assign(st, { rx_level_dbfs: -120, rx_peak_dbfs: -120, clipping: false, clip_percent: 0, snr_db: null, dcd: false, tx_active: false, frames_ok: 0, frames_bad: 0, rx_audio_age_ms: null, last_error: '', level_hint: 'no_audio' });
  }
  return { ...st, ...extra };
}

export function seedLinks(now) {
  const stats = (tx, rx, air, lastAgo) => ({ tx_frames: tx, rx_frames: rx, tx_bytes: tx * 180, rx_bytes: rx * 170, rx_errors: Math.round(rx * 0.03), airtime_ms: air, last_rx_ts: now - lastAgo, queue: 0 });
  const mk = (id, type, name, state, endpoint, config, st, status) => {
    const cfg = linkConfig(type, config);
    return { id, type, name, state, endpoint, config: cfg, overrides: { ...(config || {}) }, caps: linkCaps(type, cfg), stats: st, status: linkStatus(type, state, status), persist: true };
  };
  return [
    mk(1, 'raw', 'Home Wi-Fi LAN', 'up', { kind: 'udp', config: { group: '239.83.77.69', port: 47474 } }, { preset: 'udp_lan' }, stats(412, 655, 0, 4000), { preset: 'udp_lan' }),
    mk(2, 'audio', 'Baofeng 2 m (audio)', 'up', { kind: 'audio', config: { input_device: 'usb', output_device: 'usb', ptt_link_id: 0 } },
      { preset: 'handheld_vox', modem: 'afsk1200', callsign: 'N0CALL', licence: 'amateur' }, stats(58, 141, 96000, 60000),
      { rx_level_dbfs: -24.5, rx_peak_dbfs: -12, snr_db: 14.2, frames_ok: 141, frames_bad: 6, rx_audio_age_ms: 20, level_hint: 'ok', modem_status: { modem: 'afsk1200', framing: 'ax25' } }),
    mk(3, 'meshtastic', 'T-Beam LoRa', 'connecting', { kind: 'ble', config: { device_id: '00:00:5E:00:53:10' } },
      { region: 'US' }, stats(97, 230, 41000, 180000), { device: 'T-Beam 5310', firmware: '2.5.6', preset: 'LONG_FAST', battery_pct: 82, channel_util_pct: 6.5 }),
  ];
}
