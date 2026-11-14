// Mock radio database for rf.radios (docs/RPC_API.md §6). Mock data: the core ships the real,
// reviewed list. Menu numbers below follow the common UV-5R firmware but are marked unverified.
const g = (label, value, how) => ({ label, value, how });
export const RADIOS = [
  {
    id: 'baofeng-uv5r', maker: 'Baofeng', model: 'UV-5R (and UV-5RA, UV-5R+, BF-F8HP)', kind: 'handheld', bands: ['2m', '70cm'],
    connector: 'K1 2-pin (3.5 mm + 2.5 mm)', vox: true, data_port: false, builtin_tnc: false, bluetooth: false, power_w: [1, 4], verified: false,
    settings: [
      g('Squelch', '3', 'MENU 0 (SQL) → 3 → MENU → EXIT'),
      g('VOX', '1–2 for a cable; off when you key PTT yourself', 'MENU 4 (VOX)'),
      g('Wide/narrow', 'Wide (25 kHz) for packet', 'MENU 5 (WN)'),
      g('Dual watch', 'Off', 'MENU 7 (TDR) → OFF'),
      g('Key beep', 'Off', 'MENU 8 (BEEP) → OFF'),
      g('Transmit timeout', '60 s or more', 'MENU 9 (TOT)'),
      g('Roger beep', 'Off', 'MENU ROGER → OFF'),
    ],
    cable_notes: 'Use a K1 audio cable with an isolation transformer or an APRS-K1 cable; the radio keys by VOX unless the interface drives PTT.',
    notes: 'Keep the volume around 1/3; set the final level with the app meter.',
    recommended: { preset: 'handheld_vox', link: { type: 'audio', config: { preset: 'handheld_vox', modem: 'afsk1200' } } },
  },
  {
    id: 'baofeng-bf888s', maker: 'Baofeng', model: 'BF-888S', kind: 'handheld', bands: ['70cm'], connector: 'K1 2-pin (3.5 mm + 2.5 mm)',
    vox: true, data_port: false, builtin_tnc: false, bluetooth: false, power_w: [2], verified: false,
    settings: [g('VOX', 'On (needs programming software)', 'Programming software: VOX on, level 1')],
    cable_notes: 'Same K1 cable as the UV-5R.', notes: 'UHF only, fixed channels set with programming software.',
    recommended: { preset: 'handheld_vox', link: { type: 'audio', config: { preset: 'handheld_vox' } } },
  },
  {
    id: 'kenwood-thd74', maker: 'Kenwood', model: 'TH-D74', kind: 'handheld', bands: ['2m', '1.25m', '70cm'], connector: 'Micro-USB / Bluetooth SPP',
    vox: true, data_port: true, builtin_tnc: true, bluetooth: true, power_w: [0.05, 0.5, 5], verified: false,
    settings: [g('KISS mode', 'On', 'MENU → Configuration → Interface → KISS'), g('Bluetooth', 'On', 'MENU 930')],
    cable_notes: 'No audio cable needed: use the built-in TNC over Bluetooth or USB.', notes: '',
    recommended: { preset: 'tnc', link: { type: 'kiss', config: {} } },
  },
  {
    id: 'generic-handheld', maker: 'Generic', model: 'Any handheld radio', kind: 'handheld', bands: [], connector: 'Speaker/mic jack',
    vox: true, data_port: false, builtin_tnc: false, bluetooth: false, power_w: [], verified: false,
    settings: [g('Squelch', 'Low but closed on an empty channel', 'Radio menu'), g('Dual watch / scan', 'Off', 'Radio menu'), g('Roger / key beeps', 'Off', 'Radio menu'), g('VOX', 'On (low sensitivity) if you use a cable without PTT', 'Radio menu')],
    cable_notes: 'Any cable from the device audio to the radio mic/speaker, or no cable (hold the device to the radio).', notes: '',
    recommended: { preset: 'handheld_vox', link: { type: 'audio', config: { preset: 'handheld_vox' } } },
  },
  {
    id: 'generic-mobile', maker: 'Generic', model: 'Mobile or base radio with a data port', kind: 'mobile', bands: [], connector: '6-pin mini-DIN data port',
    vox: false, data_port: true, builtin_tnc: false, bluetooth: false, power_w: [], verified: false,
    settings: [g('Data port speed', '1200 bps', 'Radio menu: DATA SPEED')], cable_notes: 'Use a sound-card interface (Digirig, SignaLink) on the data port.', notes: '',
    recommended: { preset: 'sound_card', link: { type: 'audio', config: { preset: 'sound_card' } } },
  },
];
