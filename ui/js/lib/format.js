// Pure formatting / geo / sizing helpers. No DOM. Tested in ui/tests/format.test.js.

export function timeAgo(ts, now = Date.now()) {
  if (!Number.isFinite(ts) || ts <= 0) return 'never';
  const s = Math.round((now - ts) / 1000);
  if (Math.abs(s) < 45) return 'just now';
  const a = Math.abs(s);
  if (a >= 14 * 86400) return shortDate(ts);
  const m = Math.round(a / 60), hr = Math.round(a / 3600), d = Math.round(a / 86400);
  const span = m < 60 ? m + ' min' : hr < 24 ? hr + ' h' : d + ' d';
  return s < 0 ? 'in ' + span : span + ' ago';
}

const pad = n => String(n).padStart(2, '0');

export function clockTime(ts) {
  const d = new Date(ts);
  return pad(d.getHours()) + ':' + pad(d.getMinutes());
}

export function shortDate(ts) {
  const d = new Date(ts);
  return d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate());
}

export function dateTime(ts) {
  if (!Number.isFinite(ts)) return '';
  return shortDate(ts) + ' ' + clockTime(ts);
}

/** Compact message timestamp: time today, else date + time. */
export function msgTime(ts, now = Date.now()) {
  if (!Number.isFinite(ts)) return '';
  const a = new Date(ts), b = new Date(now);
  if (a.toDateString() === b.toDateString()) return clockTime(ts);
  return shortDate(ts).slice(5) + ' ' + clockTime(ts);
}

export function formatBytes(n) {
  if (!Number.isFinite(n) || n < 0) return '?';
  if (n < 1024) return n + ' B';
  if (n < 1024 * 1024) return (n / 1024).toFixed(n < 10240 ? 1 : 0) + ' KB';
  if (n < 1024 ** 3) return (n / 1024 / 1024).toFixed(1) + ' MB';
  return (n / 1024 ** 3).toFixed(2) + ' GB';
}

export function formatDuration(ms) {
  if (!Number.isFinite(ms) || ms < 0) return '?';
  const s = ms / 1000;
  if (s < 1) return Math.round(ms) + ' ms';
  if (s < 60) return (s < 10 ? s.toFixed(1) : Math.round(s)) + ' s';
  const m = Math.floor(s / 60);
  if (m < 60) return m + ' min ' + Math.round(s % 60) + ' s';
  const hr = Math.floor(m / 60);
  if (hr < 48) return hr + ' h ' + (m % 60) + ' min';
  return Math.floor(hr / 24) + ' d ' + (hr % 24) + ' h';
}

export function formatFreq(hz) {
  if (!Number.isFinite(hz)) return '?';
  if (hz >= 1e9) return trimNum(hz / 1e9, 6) + ' GHz';
  if (hz >= 1e6) return trimNum(hz / 1e6, 6) + ' MHz';
  if (hz >= 1e3) return trimNum(hz / 1e3, 3) + ' kHz';
  return hz + ' Hz';
}

export function trimNum(v, maxDp = 3) {
  if (!Number.isFinite(v)) return String(v);
  return String(Number(v.toFixed(maxDp)));
}

export function formatBitrate(bps) {
  if (!Number.isFinite(bps) || bps <= 0) return '?';
  if (bps >= 1e6) return trimNum(bps / 1e6, 1) + ' Mbit/s';
  if (bps >= 1e4) return trimNum(bps / 1e3, 0) + ' kbit/s';
  return trimNum(bps, 1) + ' bit/s';
}

// ---- geo ----
const R = 6371008.8; // metres
const rad = d => d * Math.PI / 180;
const deg = r => r * 180 / Math.PI;

export function validPos(p) {
  return !!p && Number.isFinite(p.lat) && Number.isFinite(p.lon) && Math.abs(p.lat) <= 90 && Math.abs(p.lon) <= 180;
}

/** Great-circle distance in metres. */
export function distance(a, b) {
  if (!validPos(a) || !validPos(b)) return NaN;
  const dLat = rad(b.lat - a.lat), dLon = rad(b.lon - a.lon);
  const s = Math.sin(dLat / 2) ** 2 + Math.cos(rad(a.lat)) * Math.cos(rad(b.lat)) * Math.sin(dLon / 2) ** 2;
  return 2 * R * Math.asin(Math.min(1, Math.sqrt(s)));
}

/** Initial bearing from a to b, degrees 0..360 (0 = north). */
export function bearing(a, b) {
  if (!validPos(a) || !validPos(b)) return NaN;
  const y = Math.sin(rad(b.lon - a.lon)) * Math.cos(rad(b.lat));
  const x = Math.cos(rad(a.lat)) * Math.sin(rad(b.lat)) - Math.sin(rad(a.lat)) * Math.cos(rad(b.lat)) * Math.cos(rad(b.lon - a.lon));
  return (deg(Math.atan2(y, x)) + 360) % 360;
}

const POINTS = ['N', 'NNE', 'NE', 'ENE', 'E', 'ESE', 'SE', 'SSE', 'S', 'SSW', 'SW', 'WSW', 'W', 'WNW', 'NW', 'NNW'];
export function compass(b) {
  if (!Number.isFinite(b)) return '?';
  return POINTS[Math.round(((b % 360) + 360) % 360 / 22.5) % 16];
}

export function formatDistance(m, units = 'metric') {
  if (!Number.isFinite(m)) return '?';
  if (units === 'imperial') {
    const ft = m * 3.28084;
    if (ft < 999.5) return Math.round(ft) + ' ft';
    const mi = m / 1609.344;
    return (mi < 10 ? mi.toFixed(1) : Math.round(mi)) + ' mi';
  }
  if (m < 999.5) return Math.round(m) + ' m';
  const km = m / 1000;
  return (km < 10 ? km.toFixed(1) : Math.round(km)) + ' km';
}

export function formatPos(p) {
  if (!validPos(p)) return 'unknown';
  const ns = p.lat >= 0 ? 'N' : 'S', ew = p.lon >= 0 ? 'E' : 'W';
  return Math.abs(p.lat).toFixed(5) + '° ' + ns + ', ' + Math.abs(p.lon).toFixed(5) + '° ' + ew;
}

/** Maidenhead locator (6 chars). */
export function maidenhead(p) {
  if (!validPos(p)) return '';
  let lon = p.lon + 180, lat = p.lat + 90;
  const A = 'ABCDEFGHIJKLMNOPQRSTUVWX';
  const f1 = A[Math.min(17, Math.floor(lon / 20))] + A[Math.min(17, Math.floor(lat / 10))];
  lon %= 20; lat %= 10;
  const f2 = String(Math.floor(lon / 2)) + String(Math.floor(lat));
  lon %= 2; lat %= 1;
  const f3 = A[Math.floor(lon * 12)].toLowerCase() + A[Math.floor(lat * 24)].toLowerCase();
  return f1 + f2 + f3;
}

// ---- message sizing ----
export function utf8Length(s) {
  let n = 0;
  for (const ch of String(s)) {
    const c = ch.codePointAt(0);
    n += c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
  }
  return n;
}

/**
 * Rough over-the-air size estimate for a chat message. The core compresses and frames; this is a
 * hint only: header ~24 B + signature 64 B, plus 40 B of AEAD nonce/tag when encrypted.
 */
export function estimateWireBytes(text, { encrypted = true } = {}) {
  const body = utf8Length(text);
  return body + 24 + 64 + (encrypted ? 40 : 0);
}

/** Airtime in ms for `bytes` at `bitrate_bps` (with ~25 % framing/FEC overhead). */
export function estimateAirtimeMs(bytes, bitrateBps) {
  if (!Number.isFinite(bitrateBps) || bitrateBps <= 0) return NaN;
  return (bytes * 8 * 1.25 / bitrateBps) * 1000;
}

export function shortId(id) { return String(id || '').slice(0, 8); }

export function initials(name) {
  const s = String(name || '').trim();
  if (!s) return '?';
  const parts = s.split(/\s+/).filter(Boolean);
  const chars = parts.length > 1 ? [parts[0], parts[parts.length - 1]].map(p => [...p][0]) : [...parts[0]].slice(0, 2);
  return chars.join('').toUpperCase();
}

/** Deterministic hue for an id (avatars). */
export function hueFor(id) {
  let h = 0;
  for (const ch of String(id || '')) h = (h * 31 + ch.charCodeAt(0)) >>> 0;
  return h % 360;
}

export function displayName(o) {
  if (!o) return 'Unknown';
  const n = (o.name || o.from_name || '').trim();
  if (n && o.callsign) return n + ' (' + o.callsign + ')';
  if (n) return n;
  if (o.callsign) return o.callsign;
  return 'Node ' + shortId(o.node_id || o.from);
}

export function clamp(v, lo, hi) { return Math.min(hi, Math.max(lo, v)); }
