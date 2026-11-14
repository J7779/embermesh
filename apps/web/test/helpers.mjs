// Shared paths for the web tests. Tests that need build output skip (with a reason) when it is
// missing; run apps/web/build.sh first.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

export const WEB_DIR = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
export const ROOT = path.resolve(WEB_DIR, '../..');
export const WASM_DIR = process.env.EMBER_WASM_DIR || path.join(ROOT, 'build/web/wasm');
export const DIST_DIR = process.env.EMBER_DIST_DIR || path.join(ROOT, 'dist/web');
export const UI_JS = path.join(ROOT, 'ui/js');

export const wasmModuleUrl = (name = 'ember.mjs') => pathToFileURL(path.join(WASM_DIR, name)).href;
export const haveWasm = () => fs.existsSync(path.join(WASM_DIR, 'ember.mjs')) && fs.existsSync(path.join(WASM_DIR, 'ember.single.mjs'));
export const haveDist = () => fs.existsSync(path.join(DIST_DIR, 'index.html')) && fs.existsSync(path.join(DIST_DIR, 'embermesh.html'));
export const ui = (rel) => pathToFileURL(path.join(UI_JS, rel)).href;

export const NO_WASM = 'WASM build missing: run apps/web/build.sh';
export const NO_DIST = 'dist/web missing: run apps/web/build.sh';

export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
