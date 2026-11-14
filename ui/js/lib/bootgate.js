// The start-up animation (js/boot) ends early when a toast or an overlay appears — so an incoming SOS
// or a real failure is never hidden behind it. Everything routine (offers, notices, "saved" toasts)
// therefore waits here until the animation has handed over (`ember:bootdone`), or runs at once when
// there is no animation (it was skipped, already finished, or this page has none).
//
//   afterBoot(fn)   run fn after the hand-over (now if it already happened)
//   bootDone()      true once it happened
//   isUrgent(opts)  toasts that may interrupt it: SOS and real failures

const SAFETY_MS = 8000; // the animation's own cap is far below this; never wait for ever
let done = false;
let waiters = [];

function finish() {
  if (done) return;
  done = true;
  const w = waiters; waiters = [];
  for (const fn of w) { try { fn(); } catch (e) { console.error(e); } }
}

export function bootDone() {
  if (done) return true;
  if (typeof document === 'undefined') return true;
  if (!document.getElementById('ember-boot')) { finish(); return true; } // none, or already removed
  return false;
}

export function afterBoot(fn) {
  if (bootDone()) { fn(); return; }
  waiters.push(fn);
}

/** Toasts that may interrupt the animation: an emergency, or a real failure. Pure. */
export const isUrgent = (opts = {}) => opts.kind === 'sos' || opts.kind === 'bad' || opts.urgent === true;

if (typeof document !== 'undefined' && document.addEventListener) {
  document.addEventListener('ember:bootdone', finish);
  setTimeout(finish, SAFETY_MS);
}

/** Test hook: back to "animation running". */
export function _resetBootGate() { done = false; waiters = []; }
