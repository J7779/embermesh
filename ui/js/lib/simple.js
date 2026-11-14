// Simple mode behaviour that CSS alone cannot do (css/a11y.css does the rest):
//   * plain wording — exact-match relabelling of headings, nav labels, section titles and buttons
//     (GLOSSARY; the operator manual uses the same words, see ui/README.md); restored when Simple
//     mode is switched off;
//   * one "More options" toggle per screen that reveals the controls tucked away as secondary
//     ([data-secondary], plus the interim selectors in a11y.css) — nothing is removed;
//   * chat composer quick buttons: "Send my location" and "I'm OK";
//   * guides home: emergency shortcuts card next to search / Ask the library;
//   * a big Back bar on sub-screens that have no Back button of their own;
//   * the More screen also lists the destinations Simple mode takes out of the navigation.
// All of it is idempotent, scoped to #main / #nav, and only active while html[data-simple="true"].
import { getPrefs, onPrefsChange, SIMPLE_NAV } from './prefs.js';
import { announce, haptic } from './a11y.js';

/** Plain wording. Exact full-text matches only (so changed source strings simply stop matching). */
export const GLOSSARY = [
  // navigation and screen titles
  ['Chats', 'Messages'], ['SOS', 'Send for help'], ['Emergency', 'Send for help'], ['Links', 'Radios and links'],
  ['Links & radios', 'Radios and links'], ['Bulletins', 'Notice board'], ['Instructions', 'How to use'],
  ['Survival library', 'Guides'], ['People & mesh', 'People'], ['Tools', 'Radio tools'],
  // sections and controls
  ['Safety check-in', 'Tell people how you are'], ['SOS alerts received', 'Calls for help nearby'],
  ['Roll call (latest check-in per person)', 'Who has said how they are'], ['Smart search', 'Search by meaning'],
  ['Region & display', 'Region and screen'], ['Mesh & relaying', 'Network and passing on messages'],
  ['Sneakernet bundles', 'Carry messages by hand'], ['Fingerprint words', 'Safety words'],
  ['Low-power mode (no animation, fewer refreshes)', 'Battery saver (no animation, fewer updates)'],
  ['Share this device', 'Let others connect through this device'], ['Net plan (where to tune)', 'Where to tune the radio'],
  ['Mesh statistics', 'Network numbers'], ['Peers', 'Nearby devices'],
  ['Radar', 'Map of nearby devices'], ['Contacts', 'People I know'], ['Post', 'Write a notice'],
  ['Show my card', 'Show my contact code'], ['Back up identity', 'Save a backup of my identity'],
  ['Set passphrase', 'Set a password'], ['Change passphrase', 'Change password'], ['Panic wipe', 'Erase everything'],
  ['Duress passphrase', 'Emergency password'], ['I am safe — stop SOS', 'I am safe now — stop calling for help'],
  ['Send emergency', 'Yes, send as emergency'], ['Stop SOS', 'Yes, stop'],
  ['Sends a repeating distress beacon with your position to every device in range. Every node relays it first. Press and hold to activate.',
    'Press and hold the red button for 3 seconds. Every phone and radio in reach is asked for help, again and again, with your position.'],
  // confirmations (plain language)
  ['Send as emergency?', 'Send as an emergency?'],
  ['Emergency priority jumps every queue on every relay. Use it only for life-threatening situations. For a repeating distress beacon use the SOS screen.',
    'Emergency messages go before all others. Use this only when a life is in danger. To keep calling for help, use “Send for help”.'],
  ['Stop SOS?', 'Stop calling for help?'],
  ['Stop the beacon only when you are safe or help has arrived.', 'Only stop when you are safe or help is with you.'],
  ['Remove passphrase?', 'Remove the password?'],
  ['Storage will no longer be encrypted at rest.', 'Your messages on this device will no longer be locked with a password.'],
  ['Replace identity?', 'Replace who you are on this device?'],
  ['The current identity on this device will be replaced.', 'The name and keys on this device will be swapped for the backup.'],
];
const WORD = new Map(GLOSSARY);

const TEXT_TARGETS = 'h1, h2, h3, .section-title, .tabbar a > span, .btn, button, label.check > span, p, summary, .item .title';
const originals = new WeakMap(); // text node → original text
let app = null;
let busy = false;

function simpleOn() { return document.documentElement.dataset.simple === 'true'; }

/** Replace exact-match text nodes inside `root` (Simple on) or restore them (Simple off). */
export function applyWording(root, on = simpleOn()) {
  if (!root || !root.querySelectorAll) return 0;
  let n = 0;
  const els = root.matches && root.matches(TEXT_TARGETS) ? [root, ...root.querySelectorAll(TEXT_TARGETS)] : [...root.querySelectorAll(TEXT_TARGETS)];
  for (const el of els) {
    for (const node of el.childNodes) {
      if (node.nodeType !== 3) continue;
      if (on) {
        const t = node.nodeValue.trim();
        const w = WORD.get(t);
        if (w && !originals.has(node)) { originals.set(node, node.nodeValue); node.nodeValue = node.nodeValue.replace(t, w); n++; }
      } else if (originals.has(node)) { node.nodeValue = originals.get(node); originals.delete(node); n++; }
    }
  }
  return n;
}

// ------------------------------------------------------------------ More options toggle
function moreToggle(main) {
  const page = main.querySelector('.page');
  const existing = main.querySelector('.simple-more');
  if (main.querySelector('.compose')) { if (existing) existing.remove(); return; } // the composer has its own
  if (existing && existing.parentNode === page && page.lastElementChild === existing) return;
  if (!page || !(main.querySelector('[data-secondary]') || hiddenBySimple(main))) { if (existing) existing.remove(); return; }
  const wrap = existing || document.createElement('div');
  wrap.className = 'simple-more';
  if (!existing) {
    const b = document.createElement('button');
    b.type = 'button';
    b.className = 'btn';
    b.setAttribute('aria-expanded', String(main.dataset.showSecondary === 'true'));
    const label = () => { b.textContent = main.dataset.showSecondary === 'true' ? 'Fewer options' : 'More options'; b.setAttribute('aria-expanded', String(main.dataset.showSecondary === 'true')); };
    b.addEventListener('click', () => {
      main.dataset.showSecondary = main.dataset.showSecondary === 'true' ? 'false' : 'true';
      label();
      announce(main.dataset.showSecondary === 'true' ? 'More options shown' : 'Extra options hidden');
    });
    label();
    wrap.appendChild(b);
  }
  page.appendChild(wrap);
}

/** True when an interim a11y.css rule currently hides something on this screen. */
function hiddenBySimple(main) {
  if (main.dataset.showSecondary === 'true') return true;
  const page = main.querySelector('.page');
  if (!page) return false;
  for (const el of page.querySelectorAll('section, .section-title, .smart-switch, select, .lib-overview, .compose .row, button, details, div')) {
    if (el.closest('.simple-more')) continue;
    if (el.offsetParent === null && getComputedStyle(el).display === 'none' && !el.hidden && !el.closest('[hidden]')) return true;
  }
  return false;
}

// ------------------------------------------------------------------ chat quick buttons
function convFromHash() {
  const m = /^#\/chats\/([^/?]+)/.exec(location.hash || '');
  if (!m) return null;
  try { return decodeURIComponent(m[1]); } catch { return null; }
}

function quickButtons(main) {
  const compose = main.querySelector('.compose:not(.interop)');
  if (!compose || compose.querySelector('.simple-quick') || !app) return;
  const conv = convFromHash();
  if (!conv) return;
  const row = document.createElement('div');
  row.className = 'simple-quick';
  const mk = (text, run) => {
    const b = document.createElement('button');
    b.type = 'button'; b.className = 'btn'; b.textContent = text;
    b.addEventListener('click', async () => {
      b.disabled = true;
      try { await run(); haptic('success'); } catch (e) { announce('Not sent: ' + ((e && e.message) || 'error'), { assertive: true }); haptic('error'); toastErr(e); } finally { b.disabled = false; }
    });
    return b;
  };
  if (app.supports('position.share')) row.appendChild(mk('Send my location', async () => { await app.api.call('position.share', { conv }); announce('Your location was sent.'); }));
  row.appendChild(mk('I’m OK', async () => { await app.api.call('messages.send', { conv, text: 'I’m OK' }); announce('Sent: I’m OK.'); }));
  const more = document.createElement('button');
  more.type = 'button'; more.className = 'btn';
  const label = () => { const open = main.dataset.showSecondary === 'true'; more.textContent = open ? 'Fewer options' : 'More options'; more.setAttribute('aria-expanded', String(open)); };
  more.addEventListener('click', () => { main.dataset.showSecondary = main.dataset.showSecondary === 'true' ? 'false' : 'true'; label(); });
  label();
  row.appendChild(more);
  compose.appendChild(row);
}

async function toastErr(e) {
  try { const { toastError } = await import('./ui.js'); toastError(e, 'Not sent'); } catch { /* ignore */ }
}

// ------------------------------------------------------------------ guides: emergency shortcuts
const EMERGENCY = [
  ['Bleeding won’t stop', 'bleeding won’t stop'], ['Not breathing', 'person not breathing'],
  ['Burn', 'how to treat a burn'], ['Unsafe water', 'how to make unsafe water safe to drink'],
];
function guidesCard(main) {
  if (document.documentElement.dataset.route !== 'guides' || document.documentElement.dataset.subroute) return;
  const page = main.querySelector('.page');
  if (!page || page.querySelector('.simple-sos-card') || !page.querySelector('input[type="search"]')) return;
  const card = document.createElement('section');
  card.className = 'card stack simple-sos-card';
  card.setAttribute('aria-label', 'In an emergency');
  const hd = document.createElement('h2'); hd.textContent = 'In an emergency';
  const row = document.createElement('div'); row.className = 'row';
  for (const [label, q] of EMERGENCY) {
    const b = document.createElement('button');
    b.type = 'button'; b.className = 'btn'; b.textContent = label;
    b.addEventListener('click', () => {
      const ask = page.querySelector('.ask-box input[type="search"]');
      const askBtn = ask && ask.closest('.ask-box').querySelector('.btn.primary');
      if (ask && askBtn) { ask.value = q; askBtn.click(); ask.closest('.ask-box').scrollIntoView({ block: 'start' }); return; }
      const s = page.querySelector('input[type="search"]');
      s.value = q; s.dispatchEvent(new Event('input', { bubbles: true }));
    });
    row.appendChild(b);
  }
  const help = document.createElement('a');
  help.className = 'btn danger'; help.href = '#/sos'; help.textContent = 'Send for help';
  row.appendChild(help);
  card.appendChild(hd); card.appendChild(row);
  const after = page.querySelector(':scope > .row.nowrap') || page.firstElementChild;
  after.after(card);
}

// ------------------------------------------------------------------ Back bar for sub-screens
function backBar(main) {
  const depth = String(location.hash || '').replace(/^#\/?/, '').split('?')[0].split('/').filter(Boolean).length;
  const route = document.documentElement.dataset.route;
  const existing = main.querySelector('.simple-backbar');
  const hasBack = main.querySelector('a[aria-label^="Back"], .narrow-back, button[aria-label^="Back"]');
  if (depth < 2 || hasBack || route === 'chats') { if (existing && (hasBack || depth < 2)) existing.remove(); return; }
  if (existing) return;
  const bar = document.createElement('div');
  bar.className = 'simple-backbar';
  const a = document.createElement('a');
  a.className = 'btn icon-only simple-back';
  a.href = '#/' + route;
  a.setAttribute('aria-label', 'Back');
  a.addEventListener('click', e => { if (history.length > 1) { e.preventDefault(); history.back(); } });
  bar.appendChild(a);
  main.insertBefore(bar, main.firstChild);
}

// ------------------------------------------------------------------ More screen additions
const NAV_EXTRA = [['people', 'People', 'People you know, nearby devices, map'], ['help', 'How to use', 'Instructions and the operator manual'], ['guides', 'Guides', 'Survival library']];
function moreScreen(main) {
  if (document.documentElement.dataset.route !== 'more') return;
  const list = main.querySelector('.list');
  if (!list || list.querySelector('[data-simple-extra]')) return;
  for (const [id, title, sub] of NAV_EXTRA) {
    if (SIMPLE_NAV.includes(id) && id !== 'people') continue;
    if (list.querySelector('a[href="#/' + id + '"]')) continue;
    const li = document.createElement('li');
    li.dataset.simpleExtra = '';
    const a = document.createElement('a');
    a.className = 'item'; a.href = '#/' + id;
    const g = document.createElement('div'); g.className = 'grow';
    const t = document.createElement('div'); t.className = 'title'; t.textContent = title;
    const s = document.createElement('div'); s.className = 'sub'; s.textContent = sub;
    g.appendChild(t); g.appendChild(s); a.appendChild(g); li.appendChild(a);
    list.insertBefore(li, list.firstChild);
  }
}

// ------------------------------------------------------------------ driver
function pass() {
  if (busy) return;
  busy = true;
  try {
    const main = document.getElementById('main');
    const nav = document.getElementById('nav');
    const on = simpleOn();
    applyWording(main, on);
    applyWording(nav, on);
    for (const s of document.querySelectorAll('.overlay')) applyWording(s, on);
    if (!on || !main) return;
    quickButtons(main);
    guidesCard(main);
    backBar(main);
    moreScreen(main);
    moreToggle(main);
  } catch (e) { console.error('simple mode', e); } finally { busy = false; }
}

let scheduled = 0;
function schedule() {
  if (scheduled) return;
  scheduled = requestAnimationFrame(() => { scheduled = 0; pass(); });
}

let installed = false;
/** Start the Simple-mode helpers (idempotent). */
export function initSimple(a) {
  app = a || app;
  if (installed || typeof document === 'undefined') return;
  installed = true;
  const obs = new MutationObserver(muts => { if (!busy && muts.some(m => m.addedNodes.length || m.type === 'characterData')) schedule(); });
  const main = document.getElementById('main');
  if (main) obs.observe(main, { childList: true, subtree: true });
  const nav = document.getElementById('nav');
  if (nav) obs.observe(nav, { childList: true, subtree: true });
  obs.observe(document.body, { childList: true });
  window.addEventListener('hashchange', () => { const m = document.getElementById('main'); if (m) delete m.dataset.showSecondary; schedule(); });
  let was = getPrefs().simple_mode;
  onPrefsChange(p => {
    if (p.simple_mode === was) return;
    was = p.simple_mode;
    const m = document.getElementById('main');
    if (m) { delete m.dataset.showSecondary; if (!was) for (const x of m.querySelectorAll('.simple-more, .simple-quick, .simple-sos-card, .simple-backbar')) x.remove(); }
    pass();
  });
  schedule();
}
