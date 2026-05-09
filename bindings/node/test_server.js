'use strict';
/**
 * Smoke tests for iword/server (iwordserver client).
 *
 * Auto-skipped when IWORD_SERVER_SOCK is not set.
 *
 * Run after loading a dictionary and starting iwordserver:
 *   bin/iwordctl load /tmp/dict.txt
 *   bin/iwordserver -u /tmp/iword.sock -p 0 &
 *   IWORD_SERVER_SOCK=/tmp/iword.sock node bindings/node/test_server.js
 */

const { Client, MODE_HTML, MODE_FORBID } = require('./iwordserver');

const SOCK = process.env.IWORD_SERVER_SOCK;
if (!SOCK) {
  console.log('SKIP: IWORD_SERVER_SOCK not set');
  process.exit(0);
}

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

async function run() {
  const c = await Client.unix(SOCK);

  // --- ping ---
  console.log('ping:');
  await c.ping();
  assert(true, 'ping: no exception');

  // --- status ---
  console.log('\nstatus:');
  const st = await c.status();
  assert(st.loaded === true,      'status: loaded=true');
  assert('version' in st,         'status: version present');

  // --- seek ---
  console.log('\nseek:');
  assert(await c.seek('apple')        === 9,  'seek("apple") === 9');
  assert(await c.seek('spam')         === 2,  'seek("spam") === 2');
  assert(await c.seek('adult_word')   === 1,  'seek("adult_word") === 1');
  assert(await c.seek('notaword_xyz') === -1, 'seek("notaword_xyz") === -1');
  assert(await c.seek('')             === -1, 'seek("") === -1');

  // --- map ---
  console.log('\nmap:');
  const matches = await c.map('I got spam in my apple', MODE_HTML | MODE_FORBID);
  const keys = matches.map(m => m.key);
  assert(matches.length > 0,                              'map: finds matches');
  assert(keys.includes(9),                                'map: apple (key=9) found');
  assert(keys.includes(2),                                'map: spam (key=2) found');
  assert(matches.every(m => m.position >= 0),             'map: positions non-negative');
  assert(matches.every(m => m.length > 0),                'map: lengths positive');
  const empty = await c.map('', MODE_HTML | MODE_FORBID);
  assert(empty.length === 0,                              'map: empty text returns []');

  // --- mask ---
  console.log('\nmask:');
  const m = await c.mask();
  assert(typeof m === 'number', 'mask: returns number');
  assert(m > 0,                 'mask: non-zero (dictionary loaded)');

  // --- filterText ---
  console.log('\nfilterText:');
  const dirty = await c.filterText('buy spam now', MODE_HTML | MODE_FORBID);
  assert(!dirty.includes('spam'), 'filterText: spam masked');
  assert(dirty.includes('*'),     'filterText: replaced with *');
  const clean = await c.filterText('hello world', MODE_HTML | MODE_FORBID);
  assert(clean === 'hello world', 'filterText: clean text unchanged');

  // --- extractByKey ---
  console.log('\nextractByKey:');
  const spamM = await c.extractByKey('get spam and apple', 2, MODE_HTML | MODE_FORBID);
  assert(spamM.every(x => x.key === 2), 'extractByKey: only key=2 returned');
  assert(spamM.length > 0,              'extractByKey: found spam matches');

  // --- concurrent ---
  console.log('\nconcurrent:');
  const clients = await Promise.all(Array.from({ length: 5 }, () => Client.unix(SOCK)));
  const errors = [];
  await Promise.all(clients.map(async (cc) => {
    for (let i = 0; i < 10; i++) {
      const r = await cc.seek('spam');
      if (r !== 2) errors.push(`expected 2, got ${r}`);
    }
    await cc.close();
  }));
  assert(errors.length === 0, `concurrent: 5 clients × 10 seeks, no errors (${JSON.stringify(errors)})`);

  await c.close();
}

run().then(() => {
  console.log(`\n${passed} passed, ${failed} failed`);
  if (failed > 0) process.exit(1);
}).catch(err => {
  console.error('Error:', err);
  process.exit(1);
});
