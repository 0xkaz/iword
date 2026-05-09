'use strict';
/**
 * iWord Node.js binding — auto-selects N-API addon or ffi-napi fallback.
 *
 * Priority:
 *   1. N-API native addon (build/Release/iword_napi.node) — Node 16+, no extra deps
 *   2. ffi-napi wrapper (iword.js) — requires `npm install`, Node 16-20 only
 *
 * Build N-API addon:  npm run build  (requires make lib first)
 * Install ffi-napi:   npm install
 */

const path = require('path');
const fs   = require('fs');

const NAPI_ADDON = path.resolve(__dirname, 'build/Release/iword_napi.node');

let _native;

function loadNative() {
  if (_native) return _native;

  // Try N-API addon first
  if (fs.existsSync(NAPI_ADDON)) {
    _native = require(NAPI_ADDON);
    return _native;
  }

  // Fall back to ffi-napi wrapper
  try {
    _native = require('./iword.js');
    return _native;
  } catch (e) {
    throw new Error(
      'iWord: no binding available.\n' +
      '  Option 1 (recommended): make lib && npm run build\n' +
      '  Option 2 (Node 16-20):  make lib && npm install\n' +
      `  (N-API addon path: ${NAPI_ADDON})\n` +
      `  (Original error: ${e.message})`
    );
  }
}

// Mode flags and category key constants (same values regardless of backend)
const MODE_HTML    = 0x1;
const MODE_FORBID  = 0x2;
const MODE_ENGLISH = 0x4;
const KEY_HIDDEN   = 0;
const KEY_ADULT    = 1;
const KEY_SPAM     = 2;
const KEY_DEFAULT  = 9;

function load(filename)           { return loadNative().load(filename); }
function unload()                 { return loadNative().unload(); }
function seek(word)               { return loadNative().seek(word); }
function map(text, mode)          { return loadNative().map(text, mode !== undefined ? mode : MODE_HTML | MODE_FORBID); }
function mask()                   { return loadNative().mask(); }
function setLimit(n)              { return loadNative().setLimit(n); }
function setDictKey(key)          { return loadNative().setDictKey(key); }

function filterText(text, mode) {
  if (mode === undefined) mode = MODE_HTML | MODE_FORBID;
  const buf = Buffer.from(text, 'utf8');
  const matches = map(text, mode);
  if (matches.length === 0) return text;
  for (const m of matches) {
    buf.fill(0x2A, m.position, m.position + m.length); // '*'
  }
  return buf.toString('utf8');
}

function extractByKey(text, key, mode) {
  if (mode === undefined) mode = MODE_HTML;
  return map(text, mode).filter(m => m.key === key);
}

module.exports = {
  load, unload, seek, map, mask, setLimit, setDictKey, filterText, extractByKey,
  MODE_HTML, MODE_FORBID, MODE_ENGLISH,
  KEY_HIDDEN, KEY_ADULT, KEY_SPAM, KEY_DEFAULT,
};
