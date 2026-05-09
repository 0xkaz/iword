'use strict';
/**
 * Basic smoke tests for iWord Node.js binding.
 * Requires: bin/iwordctl load <dict> to be run first,
 * OR pass --dict <file> argument.
 *
 * Usage:
 *   bin/iwordctl load dict/spam_en.txt
 *   node bindings/node/test_basic.js
 */

const iword = require('./index');

let passed = 0;
let failed = 0;

function assert(condition, label) {
  if (condition) {
    console.log(`  ✓ ${label}`);
    passed++;
  } else {
    console.error(`  ✗ ${label}`);
    failed++;
  }
}

// Dictionary must be pre-loaded via iwordctl
console.log('iWord Node.js binding — basic tests');
console.log('(Dictionary must be loaded via: bin/iwordctl load dict/spam_en.txt)\n');

// seek
console.log('seek:');
assert(iword.seek('free') === iword.KEY_SPAM, 'seek("free") === KEY_SPAM (2)');
assert(iword.seek('spam') === iword.KEY_SPAM, 'seek("spam") === KEY_SPAM (2)');
assert(iword.seek('notaword_xyz') === -1,     'seek("notaword_xyz") === -1');

// map
console.log('\nmap:');
const matches = iword.map('Get your free prize now!', iword.MODE_HTML | iword.MODE_FORBID);
assert(matches.length > 0, 'map finds matches in spam text');
assert(matches.every(m => typeof m.position === 'number'), 'matches have position');
assert(matches.every(m => typeof m.length === 'number'),   'matches have length');
assert(matches.every(m => typeof m.key === 'number'),      'matches have key');

// filterText
console.log('\nfilterText:');
const clean = iword.filterText('This is free spam', iword.MODE_HTML | iword.MODE_FORBID);
assert(clean !== 'This is free spam', 'filterText modifies spam text');
assert(clean.includes('*'), 'filterText replaces with *');

// extractByKey
console.log('\nextractByKey:');
const spamOnly = iword.extractByKey('Get free prize', iword.KEY_SPAM, iword.MODE_HTML);
assert(spamOnly.every(m => m.key === iword.KEY_SPAM), 'extractByKey returns only KEY_SPAM');

console.log(`\n${passed} passed, ${failed} failed`);
if (failed > 0) process.exit(1);
