// Calculators and band plans for the mock backend (real formulas).

const C = 299.792458; // speed of light, m * MHz

export const TOOLS = [
  { id: 'dipole', title: 'Half-wave dipole', category: 'antenna', description: 'Cut lengths for a wire dipole, with a build diagram.',
    inputs: [
      { key: 'freq_mhz', label: 'Frequency', type: 'number', unit: 'MHz', default: 146.52, min: 1, max: 3000, step: 0.001, required: true },
      { key: 'velocity_factor', label: 'Wire velocity factor', type: 'number', default: 0.95, min: 0.5, max: 1, step: 0.01, help: 'Bare wire about 0.95; insulated wire 0.90-0.95.' },
      { key: 'feed', label: 'Feed', type: 'select', default: 'coax', options: [{ value: 'coax', label: 'Coax (with choke)' }, { value: 'ladder', label: 'Ladder line' }] },
    ] },
  { id: 'ground_plane', title: 'Quarter-wave ground plane', category: 'antenna', description: 'Vertical with drooping radials: cheap, omnidirectional VHF/UHF base antenna.',
    inputs: [
      { key: 'freq_mhz', label: 'Frequency', type: 'number', unit: 'MHz', default: 146.52, min: 1, max: 3000, step: 0.001, required: true },
      { key: 'radials', label: 'Number of radials', type: 'number', default: 4, min: 2, max: 8, step: 1 },
      { key: 'velocity_factor', label: 'Element velocity factor', type: 'number', default: 0.95, min: 0.5, max: 1, step: 0.01, advanced: true },
    ] },
  { id: 'link_budget', title: 'Link budget', category: 'rf', description: 'Will a link work? Free-space loss, received level and fade margin.',
    inputs: [
      { key: 'freq_mhz', label: 'Frequency', type: 'number', unit: 'MHz', default: 146.52, min: 0.1, max: 100000, step: 0.001, required: true },
      { key: 'distance_km', label: 'Distance', type: 'number', unit: 'km', default: 10, min: 0.01, max: 40000, step: 0.1, required: true },
      { key: 'tx_power_w', label: 'TX power', type: 'number', unit: 'W', default: 5, min: 0.001, max: 2000, step: 0.1 },
      { key: 'tx_gain_dbi', label: 'TX antenna gain', type: 'number', unit: 'dBi', default: 2.15, min: -20, max: 40, step: 0.1 },
      { key: 'tx_cable_loss_db', label: 'TX cable loss', type: 'number', unit: 'dB', default: 1, min: 0, max: 30, step: 0.1 },
      { key: 'tx_height_m', label: 'TX antenna height', type: 'number', unit: 'm', default: 10, min: 0, max: 10000, step: 1 },
      { key: 'rx_gain_dbi', label: 'RX antenna gain', type: 'number', unit: 'dBi', default: 0, min: -20, max: 40, step: 0.1 },
      { key: 'rx_cable_loss_db', label: 'RX cable loss', type: 'number', unit: 'dB', default: 0, min: 0, max: 30, step: 0.1 },
      { key: 'rx_height_m', label: 'RX antenna height', type: 'number', unit: 'm', default: 1.5, min: 0, max: 10000, step: 0.5 },
      { key: 'rx_sensitivity_dbm', label: 'RX sensitivity', type: 'number', unit: 'dBm', default: -120, min: -150, max: 0, step: 1, help: 'FM handheld about -120 dBm (12 dB SINAD); LoRa SF12 about -137 dBm.' },
    ] },
  { id: 'radio_horizon', title: 'Radio horizon', category: 'rf', description: 'Line-of-sight range over smooth earth for VHF/UHF.',
    inputs: [
      { key: 'tx_height_m', label: 'Antenna A height', type: 'number', unit: 'm', default: 10, min: 0, max: 10000, step: 1, required: true },
      { key: 'rx_height_m', label: 'Antenna B height', type: 'number', unit: 'm', default: 1.5, min: 0, max: 10000, step: 0.5, required: true },
    ] },
  { id: 'battery_runtime', title: 'Battery runtime', category: 'power', description: 'How long a battery runs a radio or node.',
    inputs: [
      { key: 'capacity_ah', label: 'Battery capacity', type: 'number', unit: 'Ah', default: 7, min: 0.01, max: 10000, step: 0.1, required: true },
      { key: 'rx_current_ma', label: 'Receive / idle current', type: 'number', unit: 'mA', default: 80, min: 0, max: 100000, step: 1 },
      { key: 'tx_current_ma', label: 'Transmit current', type: 'number', unit: 'mA', default: 1500, min: 0, max: 100000, step: 1 },
      { key: 'tx_duty_pct', label: 'Time transmitting', type: 'number', unit: '%', default: 10, min: 0, max: 100, step: 1 },
      { key: 'usable_pct', label: 'Usable capacity', type: 'number', unit: '%', default: 80, min: 1, max: 100, step: 1, help: 'Lead-acid ~50 %, LiFePO4 ~90 %.' },
    ] },
  { id: 'morse', title: 'Morse code', category: 'signal', description: 'Convert text to Morse with timing at a chosen speed.',
    inputs: [
      { key: 'text', label: 'Text', type: 'textarea', default: 'SOS', required: true },
      { key: 'wpm', label: 'Speed', type: 'number', unit: 'WPM', default: 12, min: 3, max: 40, step: 1 },
    ] },
  { id: 'bearing_distance', title: 'Bearing and distance', category: 'navigation', description: 'Great-circle distance and initial bearing between two coordinates.',
    inputs: [
      { key: 'lat1', label: 'From latitude', type: 'number', unit: '°', default: 39.8283, min: -90, max: 90, step: 0.000001, required: true },
      { key: 'lon1', label: 'From longitude', type: 'number', unit: '°', default: -98.5795, min: -180, max: 180, step: 0.000001, required: true },
      { key: 'lat2', label: 'To latitude', type: 'number', unit: '°', default: 40, min: -90, max: 90, step: 0.000001, required: true },
      { key: 'lon2', label: 'To longitude', type: 'number', unit: '°', default: -98, min: -180, max: 180, step: 0.000001, required: true },
    ] },
];

export const MORSE = {
  A: '.-', B: '-...', C: '-.-.', D: '-..', E: '.', F: '..-.', G: '--.', H: '....', I: '..', J: '.---', K: '-.-', L: '.-..', M: '--',
  N: '-.', O: '---', P: '.--.', Q: '--.-', R: '.-.', S: '...', T: '-', U: '..-', V: '...-', W: '.--', X: '-..-', Y: '-.--', Z: '--..',
  0: '-----', 1: '.----', 2: '..---', 3: '...--', 4: '....-', 5: '.....', 6: '-....', 7: '--...', 8: '---..', 9: '----.',
  '.': '.-.-.-', ',': '--..--', '?': '..--..', '/': '-..-.', '-': '-....-', '=': '-...-', '+': '.-.-.', '@': '.--.-.', ':': '---...', "'": '.----.', '(': '-.--.', ')': '-.--.-', '!': '-.-.--', '"': '.-..-.',
};

const r = (v, d = 2) => Math.round(v * 10 ** d) / 10 ** d;
const COMPASS = ['N', 'NNE', 'NE', 'ENE', 'E', 'ESE', 'SE', 'SSE', 'S', 'SSW', 'SW', 'WSW', 'W', 'WNW', 'NW', 'NNW'];
export const compassPoint = deg => COMPASS[Math.round((((deg % 360) + 360) % 360) / 22.5) % 16];
const horizonKm = h => 4.12 * Math.sqrt(Math.max(0, h));

function bad(msg) { const e = new Error(msg); e.code = 'bad_params'; return e; }
function num(inputs, key, def, { min = -Infinity, max = Infinity } = {}) {
  const raw = inputs[key];
  const v = raw === undefined || raw === null || raw === '' ? def : Number(raw);
  if (!Number.isFinite(v)) throw bad(`${key} must be a number`);
  if (v < min || v > max) throw bad(`${key} must be between ${min} and ${max}`);
  return v;
}

export function runTool(id, inputs = {}) {
  switch (id) {
    case 'dipole': {
      const f = num(inputs, 'freq_mhz', 146.52, { min: 0.1, max: 30000 });
      const vf = num(inputs, 'velocity_factor', 0.95, { min: 0.5, max: 1 });
      const wl = C / f, total = (wl / 2) * vf, leg = total / 2;
      const ft = leg * 3.28084;
      const text = [
        '        insulator         centre insulator        insulator',
        '  o----[ ]=================[+]=================[ ]----o  rope',
        `            |<---- ${leg.toFixed(3)} m ---->|<---- ${leg.toFixed(3)} m ---->|`,
        '                              |',
        '                              | coax (choke: 6-8 turns',
        '                              |  near the feed point)',
        '                              v to radio',
        '',
        `  Total ${total.toFixed(3)} m. Cut each leg ~5 % long and trim for lowest SWR.`,
      ].join('\n');
      return {
        outputs: [
          { key: 'leg_m', label: 'Each leg', value: r(leg, 3), unit: 'm' },
          { key: 'leg_ft', label: 'Each leg', value: r(ft, 2), unit: 'ft' },
          { key: 'total_m', label: 'Total length', value: r(total, 3), unit: 'm' },
          { key: 'wavelength_m', label: 'Wavelength', value: r(wl, 3), unit: 'm' },
        ],
        text,
        notes: ['Mount at least half a wavelength above ground if you can.', inputs.feed === 'ladder' ? 'Ladder line needs a tuner at the radio.' : 'Use a current choke (coiled coax or ferrites) at the feed point.'],
      };
    }
    case 'ground_plane': {
      const f = num(inputs, 'freq_mhz', 146.52, { min: 0.1, max: 30000 });
      const n = Math.round(num(inputs, 'radials', 4, { min: 2, max: 8 }));
      const vf = num(inputs, 'velocity_factor', 0.95, { min: 0.5, max: 1 });
      const rad = (C / f / 4) * vf, radial = rad * 1.05;
      const text = [
        '            |',
        `            |  radiator ${(rad * 100).toFixed(1)} cm`,
        '            |',
        '           [=]  SO-239 / coax connector',
        '          / | \\',
        `         /  |  \\   ${n} radials, ${(radial * 100).toFixed(1)} cm each`,
        '        /   |   \\  bent down 45° (about 50 ohm)',
        '            |',
        '          coax to radio',
      ].join('\n');
      return {
        outputs: [
          { key: 'radiator_cm', label: 'Radiator', value: r(rad * 100, 1), unit: 'cm' },
          { key: 'radial_cm', label: 'Each radial', value: r(radial * 100, 1), unit: 'cm' },
          { key: 'radials', label: 'Radials', value: n },
        ],
        text,
        notes: ['Radials horizontal give about 36 ohm; 45° down is close to 50 ohm.', 'Trim the radiator a few mm at a time for lowest SWR.'],
      };
    }
    case 'link_budget': {
      const f = num(inputs, 'freq_mhz', 146.52, { min: 0.01 });
      const d = num(inputs, 'distance_km', 10, { min: 0.001 });
      const p = num(inputs, 'tx_power_w', 5, { min: 0.000001 });
      const tg = num(inputs, 'tx_gain_dbi', 2.15), tl = num(inputs, 'tx_cable_loss_db', 1);
      const th = num(inputs, 'tx_height_m', 10, { min: 0 });
      const rg = num(inputs, 'rx_gain_dbi', 0), rl = num(inputs, 'rx_cable_loss_db', 0);
      const rh = num(inputs, 'rx_height_m', 1.5, { min: 0 });
      const sens = num(inputs, 'rx_sensitivity_dbm', -120);
      const fspl = 20 * Math.log10(d) + 20 * Math.log10(f) + 32.44;
      const ptx = 10 * Math.log10(p * 1000);
      const eirp = ptx + tg - tl;
      const prx = eirp - fspl + rg - rl;
      const margin = prx - sens;
      const hor = horizonKm(th) + horizonKm(rh);
      const notes = [];
      if (d > hor) notes.push(`Distance exceeds the radio horizon (${r(hor, 1)} km): VHF/UHF needs a relay or higher antennas.`);
      if (margin < 0) notes.push('Negative margin: the link will not work in free space, let alone with obstacles.');
      else if (margin < 10) notes.push('Margin under 10 dB: expect dropouts. Add height, gain or power.');
      else if (margin < 20) notes.push('Margin 10-20 dB: probably workable with clear line of sight.');
      else notes.push('Margin above 20 dB: comfortable, survives some foliage and terrain.');
      notes.push('Free-space model only; trees, buildings and terrain add 10-40 dB of loss.');
      const pad = (s, w) => String(s).padStart(w);
      const text = [
        ` TX ${pad(r(ptx, 1), 6)} dBm  (${p} W)`,
        `  + ${pad(r(tg, 1), 6)} dBi  antenna`,
        `  - ${pad(r(tl, 1), 6)} dB   cable`,
        ` = ${pad(r(eirp, 1), 6)} dBm  EIRP`,
        `  - ${pad(r(fspl, 1), 6)} dB   path (${d} km @ ${f} MHz)`,
        `  + ${pad(r(rg, 1), 6)} dBi  antenna`,
        `  - ${pad(r(rl, 1), 6)} dB   cable`,
        ` = ${pad(r(prx, 1), 6)} dBm  at receiver`,
        `  vs ${pad(sens, 5)} dBm  sensitivity  ->  margin ${r(margin, 1)} dB`,
        '',
        `  [TX]${th}m )))${'-'.repeat(18)}((( ${rh}m[RX]`,
      ].join('\n');
      return {
        outputs: [
          { key: 'fspl_db', label: 'Free-space path loss', value: r(fspl, 1), unit: 'dB' },
          { key: 'eirp_dbm', label: 'EIRP', value: r(eirp, 1), unit: 'dBm' },
          { key: 'rx_dbm', label: 'Received level', value: r(prx, 1), unit: 'dBm' },
          { key: 'margin_db', label: 'Fade margin', value: r(margin, 1), unit: 'dB' },
          { key: 'horizon_km', label: 'Radio horizon', value: r(hor, 1), unit: 'km' },
        ],
        text,
        notes,
      };
    }
    case 'radio_horizon': {
      const a = num(inputs, 'tx_height_m', 10, { min: 0 }), b = num(inputs, 'rx_height_m', 1.5, { min: 0 });
      const ha = horizonKm(a), hb = horizonKm(b);
      return {
        outputs: [
          { key: 'a_km', label: 'Horizon of A', value: r(ha, 1), unit: 'km' },
          { key: 'b_km', label: 'Horizon of B', value: r(hb, 1), unit: 'km' },
          { key: 'total_km', label: 'Max line-of-sight range', value: r(ha + hb, 1), unit: 'km' },
        ],
        notes: ['Uses 4/3 earth radius (d = 4.12 √h). Hills and buildings shorten it; a hilltop extends it a lot.'],
      };
    }
    case 'battery_runtime': {
      const cap = num(inputs, 'capacity_ah', 7, { min: 0.0001 });
      const rx = num(inputs, 'rx_current_ma', 80, { min: 0 }), tx = num(inputs, 'tx_current_ma', 1500, { min: 0 });
      const duty = num(inputs, 'tx_duty_pct', 10, { min: 0, max: 100 }) / 100;
      const usable = num(inputs, 'usable_pct', 80, { min: 1, max: 100 }) / 100;
      const avg = rx * (1 - duty) + tx * duty;
      const hours = avg > 0 ? (cap * 1000 * usable) / avg : Infinity;
      return {
        outputs: [
          { key: 'avg_ma', label: 'Average current', value: r(avg, 1), unit: 'mA' },
          { key: 'hours', label: 'Runtime', value: Number.isFinite(hours) ? r(hours, 1) : 'unlimited', unit: 'h' },
          { key: 'days', label: 'Runtime', value: Number.isFinite(hours) ? r(hours / 24, 2) : 'unlimited', unit: 'days' },
        ],
        notes: ['Cold weather can halve battery capacity.', 'Lower TX power and shorter messages extend runtime the most.'],
      };
    }
    case 'morse': {
      const t = String(inputs.text ?? '').toUpperCase().slice(0, 2000);
      if (!t.trim()) throw bad('text is required');
      const wpm = num(inputs, 'wpm', 12, { min: 1, max: 60 });
      const unit = 1200 / wpm;
      let units = 0;
      const words = t.trim().split(/\s+/).map(w => [...w].map(ch => MORSE[ch]).filter(Boolean));
      words.forEach((w, wi) => {
        w.forEach((code, ci) => {
          for (let i = 0; i < code.length; i++) units += (code[i] === '.' ? 1 : 3) + (i < code.length - 1 ? 1 : 0);
          if (ci < w.length - 1) units += 3;
        });
        if (wi < words.length - 1) units += 7;
      });
      return {
        outputs: [
          { key: 'duration_s', label: 'Sending time', value: r((units * unit) / 1000, 1), unit: 's' },
          { key: 'unit_ms', label: 'Dot length', value: r(unit, 0), unit: 'ms' },
        ],
        text: words.map(w => w.join(' ')).join('  /  '),
        notes: ['Dash = 3 dots; gap between letters = 3 dots; between words = 7 dots.'],
      };
    }
    case 'bearing_distance': {
      const la1 = num(inputs, 'lat1', 0, { min: -90, max: 90 }), lo1 = num(inputs, 'lon1', 0, { min: -180, max: 180 });
      const la2 = num(inputs, 'lat2', 0, { min: -90, max: 90 }), lo2 = num(inputs, 'lon2', 0, { min: -180, max: 180 });
      const { km, bearing } = distanceBearing(la1, lo1, la2, lo2);
      return {
        outputs: [
          { key: 'distance_km', label: 'Distance', value: r(km, 2), unit: 'km' },
          { key: 'distance_mi', label: 'Distance', value: r(km * 0.621371, 2), unit: 'mi' },
          { key: 'bearing_deg', label: 'Initial bearing (true)', value: r(bearing, 1), unit: '°' },
          { key: 'compass', label: 'Compass point', value: compassPoint(bearing) },
        ],
        notes: ['Bearing is relative to true north; correct for magnetic declination before using a compass.'],
      };
    }
    default: {
      const e = new Error(`Unknown tool ${id}`); e.code = 'not_found'; throw e;
    }
  }
}

export function distanceBearing(la1, lo1, la2, lo2) {
  const rad = Math.PI / 180;
  const p1 = la1 * rad, p2 = la2 * rad, dp = (la2 - la1) * rad, dl = (lo2 - lo1) * rad;
  const a = Math.sin(dp / 2) ** 2 + Math.cos(p1) * Math.cos(p2) * Math.sin(dl / 2) ** 2;
  const km = 2 * 6371.0088 * Math.asin(Math.min(1, Math.sqrt(a)));
  const y = Math.sin(dl) * Math.cos(p2);
  const x = Math.cos(p1) * Math.sin(p2) - Math.sin(p1) * Math.cos(p2) * Math.cos(dl);
  const bearing = ((Math.atan2(y, x) / rad) + 360) % 360;
  return { km, bearing };
}

// ---------- band plans ----------

const MHz = v => Math.round(v * 1e6);
const seq = (n, start, step, name, bw, from = 1) => Array.from({ length: n }, (_, i) => ({ name: `${name} ${i + from}`, freq_hz: MHz(start + step * i), bw_hz: bw }));
const CB40 = [26.965, 26.975, 26.985, 27.005, 27.015, 27.025, 27.035, 27.055, 27.065, 27.075, 27.085, 27.105, 27.115, 27.125, 27.135, 27.155, 27.165, 27.175, 27.185, 27.205, 27.215, 27.225, 27.255, 27.235, 27.245, 27.265, 27.275, 27.285, 27.295, 27.305, 27.315, 27.325, 27.335, 27.345, 27.355, 27.365, 27.375, 27.385, 27.395, 27.405]
  .map((f, i) => ({ name: `CB ${i + 1}`, freq_hz: MHz(f), bw_hz: 10000, ...(i === 8 ? { notes: 'Emergency channel' } : i === 18 ? { notes: 'Road / truckers' } : {}) }));
const frsFreqs = [...seq(7, 462.5625, 0.025, 'FRS', 12500), ...seq(7, 467.5625, 0.025, 'FRS', 12500, 8), ...seq(8, 462.55, 0.025, 'FRS', 12500, 15)];
const ch = (name, f, bw, notes) => ({ name, freq_hz: MHz(f), bw_hz: bw, ...(notes ? { notes } : {}) });

const SERVICES = [
  { id: 'frs', name: 'FRS', regions: ['US'], license: 'Licence-free', max_power_w: 2, notes: 'Family Radio Service. Channels 8-14 limited to 0.5 W. Bubble-pack radios.', channels: frsFreqs.map((c, i) => (i === 0 ? { ...c, notes: 'Common calling channel' } : c)) },
  { id: 'gmrs', name: 'GMRS', regions: ['US'], license: 'FCC GMRS licence (no exam, covers family)', max_power_w: 50, notes: 'Shares FRS frequencies; repeaters on 462.550-462.725 output / +5 MHz input.', channels: [...frsFreqs.slice(14).map(c => ({ ...c, name: c.name.replace('FRS', 'GMRS') })), ch('GMRS 20 (emergency)', 462.675, 20000, 'Traditional emergency/travel channel, tone 141.3 Hz')] },
  { id: 'murs', name: 'MURS', regions: ['US'], license: 'Licence-free', max_power_w: 2, notes: 'Multi-Use Radio Service, VHF, good in woods.', channels: [ch('MURS 1', 151.82, 11250), ch('MURS 2', 151.88, 11250), ch('MURS 3', 151.94, 11250), ch('MURS 4', 154.57, 20000), ch('MURS 5', 154.6, 20000)] },
  { id: 'cb', name: 'CB radio (27 MHz)', regions: ['US', 'EU', 'UK', 'AU'], license: 'Licence-free', max_power_w: 4, notes: 'AM/FM (US: AM 4 W, SSB 12 W PEP). Skip propagation can carry very far during the day.', channels: CB40 },
  { id: 'ham2m_us', name: 'Amateur 2 m (144-148 MHz)', regions: ['US'], license: 'Amateur licence required. Encryption prohibited; identify every 10 min.', max_power_w: 1500, notes: 'In a genuine emergency anyone may transmit to save life.', channels: [ch('National simplex calling', 146.52, 16000, 'FM voice'), ch('APRS', 144.39, 16000, 'AFSK 1200 packet'), ch('Simplex', 146.55, 16000), ch('Simplex', 146.58, 16000), ch('SSB calling', 144.2, 2800), ch('Packet', 145.01, 16000)] },
  { id: 'ham70_us', name: 'Amateur 70 cm (420-450 MHz)', regions: ['US'], license: 'Amateur licence required. Encryption prohibited.', max_power_w: 1500, notes: '', channels: [ch('National simplex calling', 446.0, 16000, 'FM voice'), ch('SSB calling', 432.1, 2800)] },
  { id: 'ism915', name: 'ISM 915 MHz (LoRa)', regions: ['US', 'AU'], license: 'Licence-free (US Part 15 / AU LIPD)', max_power_w: 1, notes: 'Meshtastic, RNode and LoRa modules. Encryption allowed.', channels: [ch('Meshtastic US LongFast (default)', 906.875, 250000), ch('LoRa 915.0', 915.0, 125000), ch('Meshtastic ANZ LongFast', 919.875, 250000)] },
  { id: 'pmr446', name: 'PMR446', regions: ['EU', 'UK'], license: 'Licence-free', max_power_w: 0.5, notes: 'Fixed antenna handhelds only.', channels: seq(16, 446.00625, 0.0125, 'PMR', 12500).map((c, i) => (i === 7 ? { ...c, notes: 'Common calling / emergency (ch 8, tone 0)' } : c)) },
  { id: 'lpd433', name: 'LPD433', regions: ['EU'], license: 'Licence-free', max_power_w: 0.01, notes: '10 mW, very short range.', channels: seq(69, 433.075, 0.025, 'LPD', 25000) },
  { id: 'cb_uk', name: 'UK CB 27/81', regions: ['UK'], license: 'Licence-free', max_power_w: 4, notes: 'UK-only FM channels alongside the EU 40.', channels: seq(40, 27.60125, 0.01, 'UK', 10000) },
  { id: 'ham2m_eu', name: 'Amateur 2 m (144-146 MHz)', regions: ['EU', 'UK'], license: 'Amateur licence required. Encryption prohibited.', max_power_w: 400, notes: 'IARU Region 1 plan.', channels: [ch('FM calling', 145.5, 12500, 'FM voice'), ch('APRS', 144.8, 12500, 'AFSK 1200'), ch('SSB calling', 144.3, 2800)] },
  { id: 'ham70_eu', name: 'Amateur 70 cm (430-440 MHz)', regions: ['EU', 'UK'], license: 'Amateur licence required. Encryption prohibited.', max_power_w: 400, notes: '', channels: [ch('FM calling', 433.5, 12500), ch('SSB calling', 432.2, 2800)] },
  { id: 'srd868', name: 'SRD 868 MHz (LoRa)', regions: ['EU', 'UK'], license: 'Licence-free', max_power_w: 0.025, notes: 'Duty cycle 1 % in most sub-bands; 869.40-869.65 MHz allows 500 mW at 10 %.', channels: [ch('Meshtastic EU LongFast', 869.525, 250000, '10 % duty, 500 mW'), ch('LoRaWAN 868.1', 868.1, 125000, '1 % duty'), ch('LoRaWAN 868.3', 868.3, 125000), ch('LoRaWAN 868.5', 868.5, 125000)] },
  { id: 'uhfcb_au', name: 'UHF CB (477 MHz)', regions: ['AU'], license: 'Licence-free (class licence)', max_power_w: 5, notes: 'Ch 5/35 emergency (duplex), ch 11 calling, ch 22/23 data only (no voice). Ch 1-8 / 41-48 repeater outputs.', channels: seq(80, 476.425, 0.0125, 'UHF', 12500).map((c, i) => (i === 4 ? { ...c, notes: 'Emergency' } : i === 10 ? { ...c, notes: 'Calling' } : i === 21 || i === 22 ? { ...c, notes: 'Data only, no voice' } : c)) },
  { id: 'ham2m_au', name: 'Amateur 2 m (144-148 MHz)', regions: ['AU'], license: 'Amateur licence required. Encryption prohibited.', max_power_w: 400, notes: 'WIA band plan.', channels: [ch('FM simplex calling', 146.5, 16000), ch('APRS', 145.175, 16000, 'AFSK 1200')] },
];

export function bandsFor(region, service) {
  return SERVICES
    .filter(s => !region || s.regions.includes(String(region).toUpperCase()))
    .filter(s => !service || s.id === service)
    .map(({ regions, ...s }) => ({ ...s, region: region ? String(region).toUpperCase() : regions.join(',') }));
}
