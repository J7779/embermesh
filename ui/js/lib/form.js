// Render a schema-driven form (DOM). Value logic lives in schema.js.
import { h, nextId } from './dom.js';
import { normalizeFields, withDefaults, coerce, validate, collect } from './schema.js';
import { isLinkRefField } from './core_spec.js';

/**
 * Fields that reference another link (key ends in `link_id`, e.g. Android's audio `ptt_link_id`)
 * become a picker of existing links instead of asking for a raw id. Pure; exported for tests.
 */
export function withLinkPickers(rawFields, linkOptions, initial) {
  if (!Array.isArray(rawFields) || !Array.isArray(linkOptions)) return rawFields;
  return rawFields.map(f => {
    if (!isLinkRefField(f) || (f.type && f.type !== 'number' && f.type !== 'text')) return f;
    const cur = initial && Number(initial[f.key]);
    const options = linkOptions.slice();
    if (cur > 0 && !options.some(o => o.value === cur)) options.push({ value: cur, label: 'Link ' + cur + ' (no longer exists)' });
    return { ...f, type: 'select', options, default: 0 };
  });
}

/**
 * renderForm(fields, values?, {onChange, showAdvanced, linkOptions, check}) → {el, values(), validate(), set(values)}
 * values() returns the typed object to send to the backend (schema.collect).
 * check(values) → {key: message} adds rules the schema cannot express.
 */
export function renderForm(rawFields, initial, opts = {}) {
  const fields = normalizeFields(withLinkPickers(rawFields, opts.linkOptions, initial));
  const state = withDefaults(fields, initial);
  const inputs = new Map();
  const errEls = new Map();
  const basic = h('div', { class: 'form-fields' });
  const adv = h('div', { class: 'form-fields' });

  for (const f of fields) {
    const id = nextId('f');
    let input;
    const onInput = () => {
      state[f.key] = coerce(f, readInput(f, input));
      errEls.get(f.key).textContent = '';
      opts.onChange && opts.onChange(collect(fields, state), f.key);
    };
    if (f.type === 'bool') {
      input = h('input', { type: 'checkbox', id, checked: !!state[f.key], onchange: onInput });
    } else if (f.type === 'select') {
      input = h('select', { id, onchange: onInput },
        f.options.map(o => h('option', { value: String(o.value), selected: String(o.value) === String(state[f.key]) }, o.label)));
    } else if (f.type === 'textarea') {
      input = h('textarea', { id, rows: 3, oninput: onInput, value: state[f.key] ?? '' });
    } else {
      const isNum = f.type === 'number';
      input = h('input', {
        id, type: f.type === 'password' ? 'password' : 'text',
        inputmode: isNum ? (f.min !== undefined && f.min >= 0 && Number.isInteger(f.step ?? 1) ? 'numeric' : 'decimal') : null,
        autocomplete: f.type === 'password' ? 'new-password' : 'off', autocapitalize: 'off', spellcheck: 'false',
        value: state[f.key] == null ? '' : String(state[f.key]), oninput: onInput,
      });
      if (isNum) input.setAttribute('enterkeyhint', 'next');
    }
    if (f.required) input.setAttribute('aria-required', 'true');
    inputs.set(f.key, input);
    const err = h('div', { class: 'field-error', role: 'alert' });
    errEls.set(f.key, err);
    const helpId = f.help ? id + '-help' : null;
    if (helpId) input.setAttribute('aria-describedby', helpId);
    const label = h('label', { for: id, class: 'field-label' }, f.label, f.required ? h('span', { class: 'req', 'aria-hidden': 'true' }, ' *') : null);
    const row = f.type === 'bool'
      ? h('div', { class: 'field field-bool' }, h('label', { class: 'check' }, input, h('span', null, f.label)), f.help ? h('div', { class: 'field-help', id: helpId }, f.help) : null, err)
      : h('div', { class: 'field' }, label,
          h('div', { class: 'field-input' }, input, f.unit ? h('span', { class: 'unit' }, f.unit) : null),
          f.help ? h('div', { class: 'field-help', id: helpId }, f.help) : null, err);
    (f.advanced ? adv : basic).appendChild(row);
  }

  const el = h('div', { class: 'schema-form' }, basic);
  if (adv.childNodes.length) {
    const det = h('details', { class: 'advanced', open: !!opts.showAdvanced }, h('summary', null, 'Advanced settings'), adv);
    el.appendChild(det);
  }

  return {
    el,
    fields,
    values: () => collect(fields, state),
    validate() {
      const errors = validate(fields, collect(fields, state));
      if (opts.check) for (const [k, m] of Object.entries(opts.check(collect(fields, state)) || {})) if (!errors[k] && errEls.has(k)) errors[k] = m;
      // Required number fields that are empty are omitted by collect; check raw state too.
      for (const f of fields) if (f.required && (state[f.key] === null || state[f.key] === '')) errors[f.key] = 'Required';
      for (const [k, e] of errEls) e.textContent = errors[k] || '';
      const first = Object.keys(errors)[0];
      if (first) {
        const inp = inputs.get(first);
        const det = inp.closest('details');
        if (det) det.open = true;
        inp.focus();
      }
      return errors;
    },
    set(values) {
      for (const f of fields) {
        if (!(f.key in values)) continue;
        state[f.key] = coerce(f, values[f.key]);
        writeInput(f, inputs.get(f.key), state[f.key]);
      }
    },
  };
}

function readInput(f, el) {
  if (f.type === 'bool') return el.checked;
  return el.value;
}

function writeInput(f, el, v) {
  if (f.type === 'bool') el.checked = !!v;
  else el.value = v == null ? '' : String(v);
}
