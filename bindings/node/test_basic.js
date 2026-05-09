'use strict';
/**
 * Basic smoke tests for iWord Node.js binding.
 * Requires: bin/iwordctl load <dict> to be run first.
 *
 * Usage:
 *   bin/iwordctl load /tmp/dict.txt
 *   node bindings/node/test_basic.js
 */

const iword = require('./index');

let passed = 0;
let failed = 0;

function assert(condition, label) {
  if (condition) {
    console.log(`  ok  ${label}`);
    passed++;
  } else {
    console.error(`  FAIL ${label}`);
    failed++;
  }
}

// --- seek ---
console.log('seek:');
assert(iword.seek('free')         === iword.KEY_SPAM,    'seek("free") === KEY_SPAM (2)');
assert(iword.seek('spam')         === iword.KEY_SPAM,    'seek("spam") === KEY_SPAM (2)');
assert(iword.seek('adult_word')   === iword.KEY_ADULT,   'seek("adult_word") === KEY_ADULT (1)');
assert(iword.seek('apple')        === iword.KEY_DEFAULT, 'seek("apple") === KEY_DEFAULT (9)');
assert(iword.seek('notaword_xyz') === -1,                'seek("notaword_xyz") === -1');

// --- map ---
console.log('\nmap:');
const matches = iword.map('Get your free prize now!', iword.MODE_HTML | iword.MODE_FORBID);
assert(matches.length > 0,                                   'map: finds matches in spam text');
assert(matches.every(m => typeof m.position === 'number'),   'map: matches have position');
assert(matches.every(m => typeof m.length   === 'number'),   'map: matches have length');
assert(matches.every(m => typeof m.key      === 'number'),   'map: matches have key');
assert(matches.every(m => m.position >= 0),                  'map: positions are non-negative');
assert(matches.every(m => m.length > 0),                     'map: lengths are positive');

const emptyMatches = iword.map('', iword.MODE_HTML | iword.MODE_FORBID);
assert(emptyMatches.length === 0,                            'map: empty text returns []');

// --- filterText ---
console.log('\nfilterText:');
const dirty = iword.filterText('This is free spam', iword.MODE_HTML | iword.MODE_FORBID);
assert(dirty !== 'This is free spam',  'filterText: modifies spam text');
assert(dirty.includes('*'),            'filterText: replaces with *');
const clean = iword.filterText('hello world', iword.MODE_HTML | iword.MODE_FORBID);
assert(clean === 'hello world',        'filterText: clean text unchanged');

// --- extractByKey ---
console.log('\nextractByKey:');
const spamOnly = iword.extractByKey('Get free prize apple', iword.KEY_SPAM, iword.MODE_HTML | iword.MODE_FORBID);
assert(spamOnly.every(m => m.key === iword.KEY_SPAM), 'extractByKey: only KEY_SPAM returned');
assert(spamOnly.length > 0,                           'extractByKey: found spam matches');

const adultOnly = iword.extractByKey('adult_word hello', iword.KEY_ADULT, iword.MODE_HTML);
assert(adultOnly.every(m => m.key === iword.KEY_ADULT), 'extractByKey: only KEY_ADULT returned');

console.log(`\n${passed} passed, ${failed} failed`);
if (failed > 0) process.exit(1);
