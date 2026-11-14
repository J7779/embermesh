// Lock screen: storage is encrypted at rest and locked. security.unlock with the passphrase.
import { h } from '../lib/dom.js';
import { icon } from '../lib/icons.js';
import { errorText } from '../lib/ui.js';

export default async function render(app, _params, root) {
  const input = h('input', { type: 'password', autocomplete: 'current-password', 'aria-label': 'Passphrase', placeholder: 'Passphrase', autofocus: true });
  const err = h('div', { class: 'field-error', role: 'alert' });
  const btn = h('button', { class: 'btn primary block', type: 'submit' }, icon('unlock'), 'Unlock');
  const form = h('form', {
    class: 'stack',
    onsubmit: async e => {
      e.preventDefault();
      err.textContent = '';
      btn.disabled = true;
      try {
        const r = await app.api.call('security.unlock', { passphrase: input.value });
        input.value = '';
        if (r && r.unlocked === false) { err.textContent = 'Wrong passphrase.'; return; }
        app.store.set({ locked: false });
        await app.afterUnlock();
        app.nav(location.hash || '#/chats');
      } catch (ex) {
        err.textContent = ex.code === 'denied' ? 'Wrong passphrase.' : errorText(ex);
        input.select();
      } finally { btn.disabled = false; }
    },
  }, input, err, btn);
  root.appendChild(h('div', { class: 'lock-screen' }, h('div', { class: 'card stack' },
    h('div', { class: 'center' }, icon('lock', { size: 48 })),
    h('h1', { class: 'center' }, 'EmberMesh is locked'),
    h('p', { class: 'muted center' }, 'Messages and keys are encrypted on this device. Enter your storage passphrase to continue. After a restart, nothing is sent, received or relayed until you unlock.'),
    form)));
  setTimeout(() => input.focus(), 50);
}
