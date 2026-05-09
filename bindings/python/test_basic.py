"""Basic smoke tests for the iWord Python binding.

Run after loading a dictionary:
    bin/iwordctl load /tmp/dict.txt
    python3 bindings/python/test_basic.py
    bin/iwordctl stop
"""
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__)))

from iword import seek, map as iword_map, filter_text, MODE_HTML, MODE_FORBID

def test_seek():
    assert seek("apple") == 9,     "seek apple should return key 9"
    assert seek("spam") == 2,      "seek spam should return key 2"
    assert seek("notaword") == -1, "seek unknown word should return -1"
    print("seek: OK")

def test_map():
    matches = iword_map("I got spam in my apple", MODE_HTML | MODE_FORBID)
    keys = [m.key for m in matches]
    assert 9 in keys, "map: apple (key=9) not found"
    assert 2 in keys, "map: spam (key=2) not found"
    print("map: OK")

def test_filter_text():
    result = filter_text("buy spam now", MODE_HTML | MODE_FORBID)
    assert "spam" not in result, "filter_text: spam should be masked"
    print("filter_text: OK")

if __name__ == "__main__":
    test_seek()
    test_map()
    test_filter_text()
    print("All Python binding tests passed.")
