"""
iWord Python binding via ctypes.

Requires iword.so built and loaded into shared memory via iwordctl.
"""
import ctypes
import os
import sys
from dataclasses import dataclass
from typing import Optional

# Locate iword.so relative to this file
_SO_CANDIDATES = [
    os.path.join(os.path.dirname(__file__), "../../bin/modules/iword.so"),
    os.path.join(os.path.dirname(__file__), "../../bin/modules/iword.dylib"),
    "/usr/local/lib/iword.so",
]

def _load_lib():
    for path in _SO_CANDIDATES:
        path = os.path.abspath(path)
        if os.path.exists(path):
            return ctypes.CDLL(path)
    raise OSError(
        "iword shared library not found. Run 'make pecl' to build, "
        "or set the library path manually."
    )

_lib = _load_lib()

# Function signatures
_lib.iword_seek.restype  = ctypes.c_int
_lib.iword_seek.argtypes = [ctypes.c_char_p]

_lib.iword_map.restype   = ctypes.POINTER(ctypes.c_longlong)
_lib.iword_map.argtypes  = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]

_lib.iword_load.restype  = ctypes.c_int
_lib.iword_load.argtypes = [ctypes.c_char_p]

_lib.iword_unload.restype  = ctypes.c_int
_lib.iword_unload.argtypes = []

_lib.iword_mask.restype  = ctypes.c_int
_lib.iword_mask.argtypes = []

_lib.iword_set_limit.restype  = None
_lib.iword_set_limit.argtypes = [ctypes.c_int]

_lib.iword_set_strkey.restype  = None
_lib.iword_set_strkey.argtypes = [ctypes.c_char_p, ctypes.c_size_t]

# Mode flags (mirrors include/iword.h)
MODE_HTML    = 0x1
MODE_FORBID  = 0x2
MODE_ENGLISH = 0x4

# Category key constants
KEY_HIDDEN = 0
KEY_ADULT  = 1
KEY_SPAM   = 2
KEY_DEFAULT = 9


@dataclass
class Match:
    position: int  # byte offset in source text
    length: int    # byte length of matched word
    key: int       # category key (0-14)


def load(filename: str) -> int:
    """Load a dictionary file into shared memory. Returns 0 on success."""
    return _lib.iword_load(filename.encode())


def unload() -> int:
    """Release the shared memory dictionary. Returns 0 on success."""
    return _lib.iword_unload()


def seek(word: str) -> int:
    """
    Search for a single word. Returns its category key (0-14), or -1 if not found.
    Shared memory must be loaded via iwordctl or load() first.
    """
    return _lib.iword_seek(word.encode())


def map(text: str, mode: int = MODE_HTML | MODE_FORBID) -> list[Match]:
    """
    Extract all matching words from text.
    Returns a list of Match(position, length, key).

    mode flags:
      MODE_HTML    - skip HTML tags
      MODE_FORBID  - include forbidden (low-key) words
      MODE_ENGLISH - require word boundaries (for English)
    """
    encoded = text.encode("utf-8")
    raw = _lib.iword_map(encoded, len(encoded), mode)
    if not raw:
        return []

    results = []
    i = 0
    while raw[i]:
        entry = raw[i]
        position = (entry >> 16) & 0xFFFFFFFF
        key      = (entry >> 8)  & 0xFF
        length   =  entry        & 0xFF
        results.append(Match(position=position, length=length, key=key))
        i += 1

    # Free the C-allocated array
    ctypes.cdll.LoadLibrary(None).free(raw)
    return results


def mask() -> int:
    """Return bitmask of category keys present in the loaded dictionary."""
    return _lib.iword_mask()


def set_limit(num: int) -> None:
    """Limit the maximum number of matches returned by map()."""
    _lib.iword_set_limit(num)


def set_dict_key(key: str) -> None:
    """Set the dictionary key (for using multiple dictionaries)."""
    encoded = key.encode()
    _lib.iword_set_strkey(encoded, len(encoded))


def filter_text(text: str, mode: int = MODE_HTML | MODE_FORBID) -> str:
    """
    Remove all matched words from text, replacing with '*' characters.
    Useful as a pre-processing step for RAG pipelines.
    """
    encoded = text.encode("utf-8")
    matches = map(text, mode)
    if not matches:
        return text

    buf = bytearray(encoded)
    for m in matches:
        for j in range(m.length):
            buf[m.position + j] = ord("*")
    return buf.decode("utf-8", errors="replace")


def extract_by_key(text: str, key: int, mode: int = MODE_HTML) -> list[Match]:
    """Extract only matches with a specific category key."""
    return [m for m in map(text, mode) if m.key == key]
