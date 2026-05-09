"""Smoke tests for iword.server (iwordserver client).

Skip automatically if IWORD_SERVER_SOCK is not set.

Run after loading a dictionary and starting iwordserver:
    bin/iwordctl load /tmp/dict.txt
    bin/iwordserver -u /tmp/iword.sock -p 0 &
    IWORD_SERVER_SOCK=/tmp/iword.sock python3 bindings/python/test_server.py
    kill %1 && bin/iwordctl stop
"""
import os
import sys
import threading
sys.path.insert(0, os.path.join(os.path.dirname(__file__)))

from iword.server import Client, Match, MODE_HTML, MODE_FORBID

SOCK = os.environ.get("IWORD_SERVER_SOCK", "")
if not SOCK:
    print("SKIP: IWORD_SERVER_SOCK not set")
    sys.exit(0)

passed = 0
failed = 0

def assert_ok(condition, label):
    global passed, failed
    if condition:
        print(f"  ok  {label}")
        passed += 1
    else:
        print(f"  FAIL {label}")
        failed += 1

with Client.unix(SOCK) as c:

    # --- ping ---
    print("ping:")
    c.ping()
    assert_ok(True, "ping: no exception")

    # --- status ---
    print("\nstatus:")
    st = c.status()
    assert_ok(st.get("loaded") is True, "status: loaded=true")
    assert_ok("version" in st,          "status: version present")

    # --- seek ---
    print("\nseek:")
    assert_ok(c.seek("apple") == 9,  'seek("apple") == 9')
    assert_ok(c.seek("spam") == 2,   'seek("spam") == 2')
    assert_ok(c.seek("adult_word") == 1, 'seek("adult_word") == 1')
    assert_ok(c.seek("notaword_xyz") == -1, 'seek("notaword_xyz") == -1')
    assert_ok(c.seek("") == -1,      'seek("") == -1')

    # --- map ---
    print("\nmap:")
    matches = c.map("I got spam in my apple", MODE_HTML | MODE_FORBID)
    keys = [m.key for m in matches]
    assert_ok(len(matches) > 0,                         "map: finds matches")
    assert_ok(9 in keys,                                "map: apple (key=9) found")
    assert_ok(2 in keys,                                "map: spam (key=2) found")
    assert_ok(all(m.position >= 0 for m in matches),    "map: positions non-negative")
    assert_ok(all(m.length > 0 for m in matches),       "map: lengths positive")
    empty = c.map("", MODE_HTML | MODE_FORBID)
    assert_ok(empty == [],                              "map: empty text returns []")

    # --- mask ---
    print("\nmask:")
    m = c.mask()
    assert_ok(isinstance(m, int),  "mask: returns int")
    assert_ok(m > 0,               "mask: non-zero (dictionary loaded)")

    # --- filter_text ---
    print("\nfilter_text:")
    result = c.filter_text("buy spam now", MODE_HTML | MODE_FORBID)
    assert_ok("spam" not in result, "filter_text: spam masked")
    assert_ok("*" in result,        "filter_text: replaced with *")
    clean = c.filter_text("hello world", MODE_HTML | MODE_FORBID)
    assert_ok(clean == "hello world", "filter_text: clean text unchanged")

    # --- extract_by_key ---
    print("\nextract_by_key:")
    spam_m = c.extract_by_key("get spam and apple", 2, MODE_HTML | MODE_FORBID)
    assert_ok(all(m.key == 2 for m in spam_m), "extract_by_key: only key=2 returned")
    assert_ok(len(spam_m) > 0,                 "extract_by_key: found spam matches")

    # --- concurrent ---
    print("\nconcurrent:")
    errors = []
    def worker():
        try:
            with Client.unix(SOCK) as cc:
                for _ in range(10):
                    r = cc.seek("spam")
                    if r != 2:
                        errors.append(f"expected 2, got {r}")
        except Exception as e:
            errors.append(str(e))

    threads = [threading.Thread(target=worker) for _ in range(5)]
    for t in threads: t.start()
    for t in threads: t.join()
    assert_ok(len(errors) == 0, f"concurrent: 5 clients × 10 seeks, no errors ({errors})")

print(f"\n{passed} passed, {failed} failed")
if failed > 0:
    sys.exit(1)
