"""Basic smoke tests for the iWord Python binding.

Run after loading a dictionary:
    bin/iwordctl load /tmp/dict.txt
    python3 bindings/python/test_basic.py
    bin/iwordctl stop
"""
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__)))

from iword import seek, map as iword_map, filter_text, extract_by_key
from iword import MODE_HTML, MODE_FORBID, KEY_SPAM, KEY_ADULT, KEY_DEFAULT

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

# --- seek ---
print("seek:")
assert_ok(seek("apple") == KEY_DEFAULT,  'seek("apple") == KEY_DEFAULT (9)')
assert_ok(seek("spam") == KEY_SPAM,      'seek("spam") == KEY_SPAM (2)')
assert_ok(seek("adult_word") == KEY_ADULT, 'seek("adult_word") == KEY_ADULT (1)')
assert_ok(seek("notaword_xyz") == -1,    'seek("notaword_xyz") == -1')

# --- map ---
print("\nmap:")
matches = iword_map("I got spam in my apple", MODE_HTML | MODE_FORBID)
keys = [m.key for m in matches]
assert_ok(len(matches) > 0,              "map: finds matches in text")
assert_ok(KEY_DEFAULT in keys,           "map: apple (key=9) found")
assert_ok(KEY_SPAM in keys,              "map: spam (key=2) found")
assert_ok(all(m.position >= 0 for m in matches), "map: positions are non-negative")
assert_ok(all(m.length > 0 for m in matches),    "map: lengths are positive")

empty = iword_map("", MODE_HTML | MODE_FORBID)
assert_ok(empty == [],                   "map: empty text returns []")

# --- filter_text ---
print("\nfilter_text:")
result = filter_text("buy spam now", MODE_HTML | MODE_FORBID)
assert_ok("spam" not in result,          "filter_text: spam is masked")
assert_ok("*" in result,                 "filter_text: replaced with *")
clean = filter_text("hello world", MODE_HTML | MODE_FORBID)
assert_ok(clean == "hello world",        "filter_text: clean text unchanged")

# --- extract_by_key ---
print("\nextract_by_key:")
spam_matches = extract_by_key("get spam and apple", KEY_SPAM, MODE_HTML | MODE_FORBID)
assert_ok(all(m.key == KEY_SPAM for m in spam_matches), "extract_by_key: only KEY_SPAM returned")
assert_ok(len(spam_matches) > 0,         "extract_by_key: found spam matches")

print(f"\n{passed} passed, {failed} failed")
if failed > 0:
    sys.exit(1)
