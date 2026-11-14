// SPDX-License-Identifier: MIT
// Licence information for Settings → About (pure data + helpers; tested in ui/tests/about.test.js).
// Keep THIRD_PARTY in step with THIRD_PARTY_LICENSES.md at the repository root.

export const APP_LICENCE = { name: 'MIT License', copyright: 'Copyright (c) 2026 Joe' };

/** Third-party components bundled with EmberMesh: {name, version, licence, use}. */
export const THIRD_PARTY = [
  { name: 'Monocypher', version: '4.0.2', licence: 'BSD-2-Clause or CC0-1.0', use: 'cryptography, in every app' },
  { name: 'miniaudio', version: '0.11.21', licence: 'Public domain (Unlicense) or MIT-0', use: 'sound cards, desktop app only' },
  { name: 'ZXing core', version: '3.5.3', licence: 'Apache-2.0', use: 'QR scanning, Android app only' },
  { name: 'potion-base-4M', version: '9b3cff412d30', licence: 'MIT', use: 'smart-search model by Minish Lab, 4-bit, inside the guide pack' },
  { name: 'QR Code generator library', version: 'Project Nayuki', licence: 'MIT', use: 'QR code encoder in the core (adapted parts)' },
  { name: 'doctest', version: '2.4.11', licence: 'MIT', use: 'unit tests only, not part of any app' },
  { name: 'Gradle wrapper', version: '9.8.2', licence: 'Apache-2.0', use: 'Android build tool only, not part of the app' },
];

/** Licence family for one guide licence string (the front matter's free-form `license`). */
export function licenceFamily(name) {
  const s = String(name || '').trim();
  if (/^CC0\b|CC0[- ]1\.0/i.test(s)) return 'CC0 1.0 (public domain dedication)';
  if (/BY-SA\s*4/i.test(s)) return 'CC BY-SA 4.0';
  if (/BY-SA\s*3/i.test(s)) return 'CC BY-SA 3.0';
  if (/BY-SA/i.test(s)) return 'CC BY-SA';
  if (/^CC\s*BY\s*4/i.test(s)) return 'CC BY 4.0';
  if (/^CC\s*BY\b/i.test(s)) return 'CC BY';
  if (/public\s*domain/i.test(s)) return /government/i.test(s) ? 'Public domain (US Government works)' : 'Public domain (old books and other works)';
  return s || 'Unknown';
}

/** guides.info `licences` [{name, count}] → [{family, count}] grouped, largest first. */
export function licenceSummary(licences) {
  const m = new Map();
  for (const l of Array.isArray(licences) ? licences : []) {
    if (!l || !l.name) continue;
    const f = licenceFamily(l.name);
    m.set(f, (m.get(f) || 0) + (Number(l.count) || 0));
  }
  return [...m].map(([family, count]) => ({ family, count })).sort((a, b) => b.count - a.count || a.family.localeCompare(b.family));
}
