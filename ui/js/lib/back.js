// Decides what the Android back button does (window.__emberBridge.onBack, see app.js). Pure.
//   'sheet'   close the top dialog      'overlay' stop the Morse flash overlay
//   'up'      go back one screen        'home'    go to Chats
//   null      not handled: native may leave the app
export function backAction(state) {
  if (state.sheetOpen) return 'sheet';
  if (state.overlay) return 'overlay';
  if (state.locked || state.setup) return null;
  const path = String(state.hash || '').replace(/^#\/?/, '').split('?')[0].split('/').filter(Boolean);
  if (path.length > 1) return 'up';
  if (path.length === 1 && path[0] !== 'chats') return 'home';
  return null;
}

