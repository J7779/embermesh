// Values and rules the UI takes from docs/RPC_API.md, kept in one place, plus small pure helpers
// that interpret core results (hops, conversation ids, link legal mode, audio meters, catalogues).
// No DOM. Tested in ui/tests/core_spec.test.js. The mock backend uses the same table so the UI's
// test double can never drift from what the forms allow.

// ---------------------------------------------------------------- settings (RPC_API §2 "settings")
/**
 * Known settings keys with the core's documented defaults and ranges. `zero` = 0 is allowed in
 * addition to min..max (beacon_interval_s: 0 = never announce).
 */
export const SETTINGS_SPEC = {
  region: { type: 'string', default: 'US' },
  relay_enabled: { type: 'bool', default: true },
  store_forward_days: { type: 'int', default: 3, min: 1, max: 30, unit: 'days' },
  beacon_interval_s: { type: 'int', default: 1800, min: 60, max: 86400, zero: true, unit: 's' },
  low_power: { type: 'bool', default: false },
  position_sharing: { type: 'enum', default: 'manual', values: ['off', 'manual', 'auto'] },
  default_ttl_hops: { type: 'int', default: 7, min: 1, max: 15 },
  units: { type: 'enum', default: 'metric', values: ['metric', 'imperial'] },
  theme: { type: 'enum', default: 'dark', values: ['dark', 'light', 'night'] },
  read_receipts: { type: 'bool', default: true },
};

export const SETTINGS_DEFAULTS = Object.fromEntries(Object.entries(SETTINGS_SPEC).map(([k, s]) => [k, s.default]));

/** Error text for one known setting value, or null when valid (unknown keys are always valid). */
export function settingError(key, value) {
  const s = SETTINGS_SPEC[key];
  if (!s) return null;
  switch (s.type) {
    case 'bool': return typeof value === 'boolean' ? null : 'Must be on or off';
    case 'string': return typeof value === 'string' && value ? null : 'Required';
    case 'enum': return s.values.includes(value) ? null : 'Choose one of ' + s.values.join(', ');
    case 'int': {
      if (typeof value !== 'number' || !Number.isInteger(value)) return 'Enter a whole number';
      if (s.zero && value === 0) return null;
      if (value < s.min || value > s.max) return (s.zero ? '0 (never) or ' : '') + s.min + '–' + s.max + (s.unit ? ' ' + s.unit : '');
      return null;
    }
    default: return null;
  }
}

/** {key: message} for every invalid known key in a settings patch (empty = valid). */
export function validateSettings(patch) {
  const out = {};
  for (const [k, v] of Object.entries(patch || {})) {
    const e = settingError(k, v);
    if (e) out[k] = e;
  }
  return out;
}

/** Field schema for the Settings → Mesh & relaying form, ranges/defaults from SETTINGS_SPEC. */
export function meshFields() {
  const S = SETTINGS_SPEC;
  return [
    { key: 'relay_enabled', label: 'Pass on messages for others', type: 'bool', default: S.relay_enabled.default, help: 'Keeps the mesh working. Turn off only to save battery in an emergency.' },
    { key: 'store_forward_days', label: 'Carry undelivered messages for', type: 'number', unit: 'days', min: S.store_forward_days.min, max: S.store_forward_days.max, step: 1, default: S.store_forward_days.default, help: 'Store-and-forward: your device carries messages for people out of range and hands them over when it meets them (' + S.store_forward_days.min + '–' + S.store_forward_days.max + ' days).' },
    { key: 'beacon_interval_s', label: 'Announce this node every', type: 'number', unit: 's', min: 0, max: S.beacon_interval_s.max, step: 60, default: S.beacon_interval_s.default, help: 'Longer saves airtime and battery; shorter helps others find you. ' + S.beacon_interval_s.min + '–' + S.beacon_interval_s.max + ' s, or 0 to never announce automatically.' },
    { key: 'read_receipts', label: 'Send read receipts', type: 'bool', default: S.read_receipts.default, help: 'Tell the sender of a direct message when you have read it.' },
    { key: 'default_ttl_hops', label: 'Default hop limit', type: 'number', min: S.default_ttl_hops.min, max: S.default_ttl_hops.max, step: 1, default: S.default_ttl_hops.default, advanced: true, help: 'How many times a message may be relayed (' + S.default_ttl_hops.min + '–' + S.default_ttl_hops.max + '; large meshes need more).' },
  ];
}

// ---------------------------------------------------------------- hops / ids
/** hops: 0 = heard directly, null/undefined = unknown, n = relayed n times. */
export function hopsLabel(hops, direct) {
  if (direct === true || hops === 0) return 'direct';
  if (!Number.isFinite(hops) || hops < 0) return 'path unknown';
  return hops + (hops === 1 ? ' hop' : ' hops');
}

/** Conversation id → kind ('dm' | 'channel' | 'interop' | null). */
export function convKind(id) {
  const s = String(id || '');
  if (/^dm:[0-9a-f]{16}$/.test(s)) return 'dm';
  if (s.startsWith('ch:') && s.length > 3) return 'channel';
  if (s.startsWith('io:') && s.length > 3) return 'interop';
  return null;
}

const NETWORK_TITLES = { aprs: 'APRS', meshtastic: 'Meshtastic', rtty: 'RTTY', psk31: 'PSK31', cw: 'CW (Morse)', dtmf: 'DTMF', lora: 'LoRa', ax25: 'AX.25 packet' };
/** "aprs" → "APRS"; unknown networks are upper-cased. */
export function networkTitle(net) {
  const n = String(net || '').toLowerCase();
  return NETWORK_TITLES[n] || (n ? n.toUpperCase() : 'Radio');
}

/** A link id is a positive integer; 0 / null means "none" (outgoing messages carry via_link 0). */
export const validLinkId = id => Number.isInteger(id) && id > 0;

// ---------------------------------------------------------------- legal (amateur) mode
// Drivers mark amateur operation with one of these config fields (links.types): a "licence" select
// whose value "amateur" means ham rules, or a bool "amateur" / "licensed". The resulting
// Link.caps.encryption_allowed / requires_station_id are what the core enforces; before a link
// exists (wizard) the UI predicts them from these fields.
const LEGAL_FIELDS = [
  { key: 'licence', amateur: v => v === 'amateur' },
  { key: 'amateur', amateur: v => v === true },
  { key: 'licensed', amateur: v => v === true },
];

/** true / false when the driver config says whether this is amateur operation; null if it has no such field. */
export function amateurFromConfig(fields, config) {
  const keys = new Set((fields || []).map(f => f && f.key));
  for (const lf of LEGAL_FIELDS) if (keys.has(lf.key)) return lf.amateur((config || {})[lf.key]);
  return null;
}

/** Legal state of an existing link from its caps: {noEncryption, stationId}. */
export function linkLegal(link) {
  const c = (link && link.caps) || {};
  return { noEncryption: c.encryption_allowed === false, stationId: c.requires_station_id === true };
}

/** A callsign usable on the air (not empty and not the drivers' N0CALL placeholder). */
export function usableCallsign(cs) {
  const s = String(cs || '').trim().toUpperCase();
  return !!s && !/^N0CALL(-\d+)?$/.test(s) && /^[A-Z0-9]{3,7}(-\d{1,2})?$/.test(s);
}

// ---------------------------------------------------------------- audio meter (RPC_API "Audio link status fields")
const num = v => (typeof v === 'number' && Number.isFinite(v) ? v : null);

/**
 * Normalises Link.status of an audio / rtl_tcp link into meter values. Uses the documented flat
 * fields; falls back to the older nested modem_status for builds that predate them.
 * Returns null when the status carries no meter information at all.
 */
export function audioMeter(status) {
  if (!status || typeof status !== 'object') return null;
  const ms = status.modem_status && typeof status.modem_status === 'object' ? status.modem_status : {};
  const pick = k => (status[k] !== undefined ? status[k] : ms[k]);
  const level = num(pick('rx_level_dbfs'));
  const hint = typeof pick('level_hint') === 'string' ? pick('level_hint') : '';
  if (level === null && !hint && num(pick('frames_ok')) === null) return null;
  const clipPct = num(pick('clip_percent'));
  return {
    level: level === null ? -120 : Math.max(-120, Math.min(0, level)),
    peak: (() => { const p = num(pick('rx_peak_dbfs')); return p === null ? null : Math.max(-120, Math.min(0, p)); })(),
    clipping: pick('clipping') === true || hint === 'clipping',
    clipPct,
    snr: num(pick('snr_db')),
    dcd: pick('dcd') === true,
    tx: pick('tx_active') === true || status.transmitting === true,
    ok: num(pick('frames_ok')),
    bad: num(pick('frames_bad')),
    ageMs: num(pick('rx_audio_age_ms')),
    error: typeof status.last_error === 'string' ? status.last_error : '',
    hint,
  };
}

export const LEVEL_HINTS = {
  no_audio: ['bad', 'No audio arriving — check the cable, the input device and the microphone permission.'],
  too_quiet: ['warn', 'Too quiet — turn the radio volume up (or raise the input gain).'],
  ok: ['ok', 'Level OK.'],
  too_loud: ['warn', 'Too loud — turn the radio volume down a little.'],
  clipping: ['bad', 'Clipping — turn the radio volume down until the clip lamp stays off.'],
};

/** Meter bar position 0..100 for a dBFS value (−60 dBFS … 0 dBFS). */
export const meterPct = db => Math.max(0, Math.min(100, ((Number.isFinite(db) ? db : -120) + 60) / 60 * 100));

/** Link types whose status carries the audio meter fields. */
export const METER_TYPES = new Set(['audio', 'rtl_tcp']);

// ---------------------------------------------------------------- catalogues
/**
 * Link types compatible with an endpoint kind. Prefers the core's links.types endpoint_kinds,
 * then the host's own list (host.info endpoint_kinds[].link_types). Never hard-coded.
 */
export function compatibleTypes(kind, types, hostKind) {
  const fromCore = (types || []).filter(t => Array.isArray(t.endpoint_kinds) && t.endpoint_kinds.includes(kind.kind || kind)).map(t => t.type);
  const fromHost = hostKind && Array.isArray(hostKind.link_types) ? hostKind.link_types : [];
  if (fromCore.length && fromHost.length) {
    const both = fromCore.filter(t => fromHost.includes(t));
    return both.length ? both : fromCore;
  }
  return fromCore.length ? fromCore : fromHost;
}

/**
 * Net-plan modes: voice first, then every modem the core's sound-card drivers offer (links.types
 * `modem` options of audio / rtl_tcp), then other radio systems.
 */
export function netplanModes(types) {
  const out = [{ value: 'voice', label: 'Voice (FM/SSB)' }];
  const seen = new Set(['voice']);
  for (const t of types || []) {
    const f = (t.config || []).find(x => x && x.key === 'modem' && Array.isArray(x.options));
    if (!f) continue;
    for (const o of f.options) {
      if (!o || typeof o.value !== 'string' || seen.has(o.value)) continue;
      seen.add(o.value);
      out.push({ value: o.value, label: String(o.label || o.value).split(' - ')[0] });
    }
  }
  for (const [v, l] of [['cw', 'Morse code (CW)'], ['lora', 'LoRa'], ['meshtastic', 'Meshtastic'], ['other', 'Other']]) if (!seen.has(v)) { seen.add(v); out.push({ value: v, label: l }); }
  return out;
}

/** Endpoint-config fields that refer to another link (e.g. Android audio `ptt_link_id`). */
export const isLinkRefField = f => !!f && typeof f.key === 'string' && /(^|_)link_id$/.test(f.key);

/**
 * Options for a link-reference field: "none" plus every other link, serial-like links first.
 * Returns [{value, label}] (value 0 = none).
 */
export function linkRefOptions(links, selfId) {
  const serialish = l => /serial|spp|usb|tcp/.test(String(l.endpoint && l.endpoint.kind));
  const ls = (links || []).filter(l => l && validLinkId(l.id) && l.id !== selfId)
    .sort((a, b) => (serialish(b) - serialish(a)) || (a.id - b.id));
  return [{ value: 0, label: 'None (use VOX)' }, ...ls.map(l => ({ value: l.id, label: (l.name || l.type) + ' — ' + ((l.endpoint && l.endpoint.kind) || 'link') }))];
}

// ---------------------------------------------------------------- operator manual
export const MANUAL_BOOK = 'embermesh/manual';
const MANUAL_BY_TYPE = { audio: 'handheld-audio-modem', kiss: 'kiss-tnc-packet', meshtastic: 'lora', rnode: 'lora', lora_at: 'lora', rtl_tcp: 'sdr-listening' };
const MANUAL_BY_KIND = { websocket: 'hub-setup', ws_server: 'hub-setup', ble_peer: 'phone-to-phone', webrtc: 'phone-to-phone', multipeer: 'phone-to-phone', wifi_direct: 'phone-to-phone', udp: 'phone-to-phone', loopback: 'troubleshooting', audio: 'handheld-audio-modem', rtl_tcp: 'sdr-listening' };
/** Manual chapter guide id for a link type / endpoint kind (falls back to the quick start). */
export function manualFor(type, kind) {
  const slug = MANUAL_BY_TYPE[type] || MANUAL_BY_KIND[kind] || 'quick-start';
  return 'embermesh/' + slug;
}

// ---------------------------------------------------------------- host actions (RPC_API §3)
/**
 * Validates host.info.actions: [{id, title, description?, method, params?, confirm?, checked?}] →
 * safe list. Only host.* methods are allowed. `checked` (bool) makes the action a switch: the UI
 * calls `method` with {...params, enabled: <new state>}.
 */
export function hostActions(info) {
  const list = info && Array.isArray(info.actions) ? info.actions : [];
  const out = [];
  const seen = new Set();
  for (const a of list.slice(0, 32)) {
    if (!a || typeof a !== 'object') continue;
    const id = typeof a.id === 'string' ? a.id.slice(0, 64) : '';
    const method = typeof a.method === 'string' ? a.method : '';
    if (!id || seen.has(id) || !/^host\.[a-z0-9_.]+$/i.test(method) || typeof a.title !== 'string' || !a.title) continue;
    seen.add(id);
    out.push({
      id, method, title: a.title.slice(0, 80),
      description: typeof a.description === 'string' ? a.description.slice(0, 300) : '',
      params: a.params && typeof a.params === 'object' && !Array.isArray(a.params) ? a.params : {},
      confirm: typeof a.confirm === 'string' ? a.confirm.slice(0, 300) : '',
      ...(typeof a.checked === 'boolean' ? { checked: a.checked } : {}),
    });
  }
  return out;
}
