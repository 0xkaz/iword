'use strict';
/**
 * iWord Node.js binding via N-API native addon.
 *
 * Build: npm run build  (requires `make lib` first)
 * Use:   const iword = require('./bindings/node');
 */

const path = require('path');
const addon = require(path.resolve(__dirname, 'build/Release/iword_napi.node'));

// Mode flags (mirrors include/iword.h)
const MODE_HTML    = 0x1;
const MODE_FORBID  = 0x2;
const MODE_ENGLISH = 0x4;

// Category key constants
const KEY_HIDDEN  = 0;
const KEY_ADULT   = 1;
const KEY_SPAM    = 2;
const KEY_DEFAULT = 9;

function load(filename)  { return addon.load(filename); }
function unload()        { return addon.unload(); }
function seek(word)      { return addon.seek(word); }
function mask()          { return addon.mask(); }
function setLimit(n)     { return addon.setLimit(n); }
function setDictKey(key) { return addon.setDictKey(key); }

function map(text, mode) {
  if (mode === undefined) mode = MODE_HTML | MODE_FORBID;
  return addon.map(text, mode);
}

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
