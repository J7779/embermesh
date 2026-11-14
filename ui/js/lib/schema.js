// Field-schema value handling (docs/RPC_API.md §1 "Field schema"). Pure, no DOM.
// Catalogues come from the core / host and are treated as untrusted: unknown types fall back to
// "text", bad entries are dropped, lengths are capped.

export const FIELD_TYPES = ['number', 'text', 'select', 'bool', 'textarea', 'password'];
const MAX_FIELDS = 200;

export function normalizeFields(fields) {
  if (!Array.isArray(fields)) return [];
  const out = [];
  const seen = new Set();
  for (const f of fields.slice(0, MAX_FIELDS)) {
    if (!f || typeof f !== 'object' || typeof f.key !== 'string' || !f.key || seen.has(f.key)) continue;
    seen.add(f.key);
    const type = FIELD_TYPES.includes(f.type) ? f.type : 'text';
    const n = {
      key: f.key.slice(0, 100),
      label: typeof f.label === 'string' && f.label ? f.label.slice(0, 200) : f.key,
      type,
      unit: typeof f.unit === 'string' ? f.unit.slice(0, 20) : '',
      help: typeof f.help === 'string' ? f.help.slice(0, 500) : '',
      required: f.required === true,
      advanced: f.advanced === true,
    };
    for (const k of ['min', 'max', 'step']) if (Number.isFinite(f[k])) n[k] = f[k];
    if (type === 'select') {
      n.options = (Array.isArray(f.options) ? f.options : []).slice(0, 500)
        .map(o => (o && typeof o === 'object') ? { value: o.value, label: o.label != null ? String(o.label) : String(o.value) } : { value: o, label: String(o) })
        .filter(o => o.value != null);
    }
    if ('default' in f) n.default = f.default;
    out.push(n);
  }
  return out;
}

/** Default value for one field (typed). */
export function fieldDefault(f) {
  if (f.default !== undefined && f.default !== null) return coerce(f, f.default);
  switch (f.type) {
    case 'bool': return false;
    case 'number': return null;
    case 'select': return f.options && f.options.length ? f.options[0].value : '';
    default: return '';
  }
}

export function defaults(fields) {
  const v = {};
  for (const f of normalizeFields(fields)) v[f.key] = fieldDefault(f);
  return v;
}

/**
 * Convert a raw value (string from an <input>, or a JSON value) to the field's JSON type.
 * Numbers → JSON number (null when empty/invalid), bools → JSON bool, everything else → string.
 * Select values keep the option's own type when it matches an option.
 */
export function coerce(f, raw) {
  switch (f.type) {
    case 'number': {
      if (raw === null || raw === undefined) return null;
      if (typeof raw === 'number') return Number.isFinite(raw) ? raw : null;
      const s = String(raw).trim().replace(',', '.');
      if (s === '') return null;
      const n = Number(s);
      return Number.isFinite(n) ? n : null;
    }
    case 'bool':
      if (typeof raw === 'boolean') return raw;
      if (typeof raw === 'string') return raw === 'true' || raw === '1' || raw === 'on' || raw === 'yes';
      return !!raw;
    case 'select': {
      const opt = (f.options || []).find(o => String(o.value) === String(raw));
      if (opt) return typeof opt.value === 'number' || typeof opt.value === 'boolean' ? opt.value : String(opt.value);
      return raw == null ? '' : String(raw);
    }
    default:
      return raw == null ? '' : String(raw);
  }
}

/** Returns {key: message} for invalid values; empty object = valid. */
export function validate(fields, values) {
  const errors = {};
  for (const f of normalizeFields(fields)) {
    const v = values ? values[f.key] : undefined;
    const empty = v === null || v === undefined || v === '';
    if (f.required && empty) { errors[f.key] = 'Required'; continue; }
    if (empty) continue;
    if (f.type === 'number') {
      if (typeof v !== 'number' || !Number.isFinite(v)) { errors[f.key] = 'Enter a number'; continue; }
      if (f.min !== undefined && v < f.min) errors[f.key] = 'Minimum ' + f.min + (f.unit ? ' ' + f.unit : '');
      else if (f.max !== undefined && v > f.max) errors[f.key] = 'Maximum ' + f.max + (f.unit ? ' ' + f.unit : '');
    } else if (f.type === 'select' && f.options && f.options.length) {
      if (!f.options.some(o => String(o.value) === String(v))) errors[f.key] = 'Choose an option';
    }
  }
  return errors;
}

/**
 * Build the object sent back to the backend: every field present, typed; empty optional
 * numbers are omitted so the core applies its own default.
 */
export function collect(fields, values) {
  const out = {};
  for (const f of normalizeFields(fields)) {
    let v = values ? values[f.key] : undefined;
    v = v === undefined ? fieldDefault(f) : coerce(f, v);
    if (f.type === 'number' && v === null) continue;
    out[f.key] = v;
  }
  return out;
}

/** Merge saved values over defaults, coercing each. Unknown keys are dropped. */
export function withDefaults(fields, saved) {
  const out = defaults(fields);
  if (saved && typeof saved === 'object') {
    for (const f of normalizeFields(fields)) if (f.key in saved) out[f.key] = coerce(f, saved[f.key]);
  }
  return out;
}
