'use strict';
/**
 * iWord Node.js binding via ffi-napi.
 *
 * Requires:
 *   npm install ffi-napi ref-napi
 *   make lib  (builds bin/libiword.so)
 *
 * The iWord shared memory dictionary must be loaded before calling seek/map:
 *   iwordctl load words.txt
 * or programmatically via load().
 */

const ffi = require('ffi-napi');
const ref = require('ref-napi');
const path = require('path');
const fs = require('fs');

// Locate libiword.so relative to this file
const SO_CANDIDATES = [
  path.resolve(__dirname, '../../bin/libiword.so'),
  path.resolve(__dirname, '../../bin/libiword.dylib'),
  '/usr/local/lib/libiword.so',
];

function findLib() {
  for (const p of SO_CANDIDATES) {
    if (fs.existsSync(p)) return p;
  }
  throw new Error(
    'iword shared library not found. Run "make lib" to build bin/libiword.so.'
  );
}

const lib = ffi.Library(findLib(), {
  iword_seek:      ['int',  ['string']],
  iword_map:       ['pointer', ['string', 'int', 'int']],
  iword_load:      ['int',  ['string']],
  iword_unload:    ['int',  []],
  iword_mask:      ['int',  []],
  iword_set_limit: ['void', ['int']],
  iword_set_strkey:['void', ['string', 'size_t']],
});

// Mode flags (mirrors include/iword.h)
const MODE_HTML    = 0x1;
const MODE_FORBID  = 0x2;
const MODE_ENGLISH = 0x4;

// Category key constants
const KEY_HIDDEN  = 0;
const KEY_ADULT   = 1;
const KEY_SPAM    = 2;
const KEY_DEFAULT = 9;

/**
 * Load a dictionary file into shared memory.
 * @param {string} filename
 * @returns {number} 0 on success
 */
function load(filename) {
  return lib.iword_load(filename);
}

/**
 * Release the shared memory dictionary.
 * @returns {number} 0 on success
 */
function unload() {
  return lib.iword_unload();
}

/**
 * Search for a single word.
 * @param {string} word
 * @returns {number} category key (0-14), or -1 if not found
 */
function seek(word) {
  return lib.iword_seek(word);
}

/**
 * Extract all matching words from text.
 * @param {string} text
 * @param {number} [mode=MODE_HTML|MODE_FORBID]
 * @returns {Array<{position: number, length: number, key: number}>}
 */
function map(text, mode = MODE_HTML | MODE_FORBID) {
  const buf = Buffer.from(text, 'utf8');
  const ptr = lib.iword_map(buf.toString('binary'), buf.length, mode);
  if (ptr.isNull()) return [];

  const results = [];
  let i = 0;
  // Each entry is a 64-bit (8-byte) long long
  // layout: [position(32bit) | key(8bit) | length(8bit) | padding(16bit)]
  // packed as: entry >> 16 = position, (entry >> 8) & 0xFF = key, entry & 0xFF = length
  while (true) {
    const lo = ptr.readInt32LE(i * 8);
    const hi = ptr.readInt32LE(i * 8 + 4);
    if (lo === 0 && hi === 0) break;

    // Reconstruct 64-bit value from two 32-bit reads
    // hi:lo — note JS bitwise ops are 32-bit, use hi/lo arithmetic
    const length   =  lo & 0xFF;
    const key      = (lo >>> 8) & 0xFF;
    const posLo    = (lo >>> 16) & 0xFFFF;
    const posHi    = hi & 0xFFFF;
    const position = (posHi * 0x10000) + posLo;

    results.push({ position, length, key });
    i++;
  }

  // Free the C-allocated array
  const libc = ffi.Library(null, { free: ['void', ['pointer']] });
  libc.free(ptr);

  return results;
}

/**
 * Return bitmask of category keys present in the loaded dictionary.
 * @returns {number}
 */
function mask() {
  return lib.iword_mask();
}

/**
 * Limit the maximum number of matches returned by map().
 * @param {number} num
 */
function setLimit(num) {
  lib.iword_set_limit(num);
}

/**
 * Set the dictionary key (for using multiple dictionaries).
 * @param {string} key
 */
function setDictKey(key) {
  lib.iword_set_strkey(key, Buffer.byteLength(key));
}

/**
 * Replace all matched words in text with '*' characters.
 * @param {string} text
 * @param {number} [mode=MODE_HTML|MODE_FORBID]
 * @returns {string}
 */
function filterText(text, mode = MODE_HTML | MODE_FORBID) {
  const buf = Buffer.from(text, 'utf8');
  const matches = map(text, mode);
  if (matches.length === 0) return text;

  for (const m of matches) {
    buf.fill(0x2A, m.position, m.position + m.length); // 0x2A = '*'
  }
  return buf.toString('utf8');
}

/**
 * Extract only matches with a specific category key.
 * @param {string} text
 * @param {number} key
 * @param {number} [mode=MODE_HTML]
 * @returns {Array<{position: number, length: number, key: number}>}
 */
function extractByKey(text, key, mode = MODE_HTML) {
  return map(text, mode).filter(m => m.key === key);
}

module.exports = {
  load, unload, seek, map, mask, setLimit, setDictKey, filterText, extractByKey,
  MODE_HTML, MODE_FORBID, MODE_ENGLISH,
  KEY_HIDDEN, KEY_ADULT, KEY_SPAM, KEY_DEFAULT,
};
