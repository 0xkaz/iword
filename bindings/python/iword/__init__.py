"""
iWord Python binding via ctypes.

Requires libiword.so built via `make lib` and a dictionary loaded via iwordctl.

Quick start:
    bin/iwordctl load words.txt
    from iword import seek, map as iword_map, filter_text
    print(seek("spam"))   # 2
"""
import ctypes
import os
from dataclasses import dataclass

# Locate libiword.so relative to this package, or in standard locations
_SO_CANDIDATES = [
    # installed alongside the package (e.g. pip install -e .)
    os.path.join(os.path.dirname(__file__), "../../../bin/libiword.so"),
    os.path.join(os.path.dirname(__file__), "../../../bin/libiword.dylib"),
    # system-wide install
    "/usr/local/lib/libiword.so",
    "/usr/local/lib/libiword.dylib",
]

def _load_lib():
    for path in _SO_CANDIDATES:
        path = os.path.abspath(path)
        if os.path.exists(path):
            return ctypes.CDLL(path)
    raise OSError(
        "iword shared library not found. Run 'make lib' to build bin/libiword.so."
    )

_lib = _load_lib()

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

# Mode flags
MODE_HTML    = 0x1
MODE_FORBID  = 0x2
MODE_ENGLISH = 0x4

# Category key constants
KEY_HIDDEN  = 0
KEY_ADULT   = 1
KEY_SPAM    = 2
KEY_DEFAULT = 9


@dataclass
class Match:
    position: int
    length: int
    key: int


def load(filename: str) -> int:
    """Load a dictionary file into shared memory. Returns 0 on success."""
    return _lib.iword_load(filename.encode())


def unload() -> int:
    """Release the shared memory dictionary. Returns 0 on success."""
    return _lib.iword_unload()


def seek(word: str) -> int:
    """Return the category key (0-14) for word, or -1 if not found."""
    return _lib.iword_seek(word.encode())


def map(text: str, mode: int = MODE_HTML | MODE_FORBID):
    """Extract all matching words from text. Returns list of Match objects."""
    encoded = text.encode("utf-8")
    raw = _lib.iword_map(encoded, len(encoded), mode)
    if not raw:
        return []
    results = []
    i = 0
    while raw[i]:
        entry = raw[i]
        results.append(Match(
            position=(entry >> 16) & 0xFFFFFFFF,
            key=(entry >> 8) & 0xFF,
            length=entry & 0xFF,
        ))
        i += 1
    ctypes.cdll.LoadLibrary(None).free(raw)
    return results


def mask() -> int:
    """Return bitmask of category keys present in the loaded dictionary."""
    return _lib.iword_mask()


def set_limit(num: int) -> None:
    """Limit the maximum number of matches returned by map()."""
    _lib.iword_set_limit(num)


def set_dict_key(key: str) -> None:
    """Select which shared memory dictionary to use."""
    encoded = key.encode()
    _lib.iword_set_strkey(encoded, len(encoded))


def filter_text(text: str, mode: int = MODE_HTML | MODE_FORBID) -> str:
    """Replace all matched words in text with '*' characters."""
    encoded = text.encode("utf-8")
    matches = map(text, mode)
    if not matches:
        return text
    buf = bytearray(encoded)
    for m in matches:
        for j in range(m.length):
            buf[m.position + j] = ord("*")
    return buf.decode("utf-8", errors="replace")


def extract_by_key(text: str, key: int, mode: int = MODE_HTML):
    """Extract only matches with a specific category key."""
    return [m for m in map(text, mode) if m.key == key]


__all__ = [
    "load", "unload", "seek", "map", "mask", "set_limit", "set_dict_key",
    "filter_text", "extract_by_key",
    "Match",
    "MODE_HTML", "MODE_FORBID", "MODE_ENGLISH",
    "KEY_HIDDEN", "KEY_ADULT", "KEY_SPAM", "KEY_DEFAULT",
]
