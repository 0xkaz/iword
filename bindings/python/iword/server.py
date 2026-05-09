"""
iword.server — iwordserver client (no CGO / no shared library required).

Connects to a running iwordserver over Unix socket or TCP and exposes
the same API surface as the ctypes-based functions in iword.__init__.

Quick start:
    bin/iwordctl load words.txt
    bin/iwordserver -u /tmp/iword.sock -p 0

    from iword.server import Client
    with Client.unix("/tmp/iword.sock") as c:
        key = c.seek("spam_word")          # 2, or -1 if not found
        matches = c.map("get free prize")  # list of Match
        clean = c.filter_text("get free prize")

All methods are thread-safe — iwordserver serializes iword calls internally.
"""

import json
import socket
import threading
from dataclasses import dataclass
from typing import List, Optional


@dataclass
class Match:
    position: int
    length: int
    key: int


# Mode flags (mirror iword.__init__)
MODE_HTML    = 0x1
MODE_FORBID  = 0x2
MODE_ENGLISH = 0x4


class IwordServerError(Exception):
    pass


class Client:
    """Persistent connection to iwordserver.

    Use as a context manager or call close() explicitly.
    """

    def __init__(self, sock: socket.socket):
        self._sock = sock
        self._file = sock.makefile("r", encoding="utf-8")
        self._lock = threading.Lock()

    # ------------------------------------------------------------------
    # Factory helpers
    # ------------------------------------------------------------------

    @classmethod
    def unix(cls, path: str, timeout: Optional[float] = None) -> "Client":
        """Connect via Unix socket."""
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        if timeout is not None:
            s.settimeout(timeout)
        s.connect(path)
        return cls(s)

    @classmethod
    def tcp(cls, host: str, port: int, timeout: Optional[float] = None) -> "Client":
        """Connect via TCP."""
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if timeout is not None:
            s.settimeout(timeout)
        s.connect((host, port))
        return cls(s)

    # ------------------------------------------------------------------
    # Context manager
    # ------------------------------------------------------------------

    def __enter__(self) -> "Client":
        return self

    def __exit__(self, *_) -> None:
        self.close()

    def close(self) -> None:
        try:
            self._sock.close()
        except OSError:
            pass

    # ------------------------------------------------------------------
    # Protocol
    # ------------------------------------------------------------------

    def _call(self, req: dict) -> dict:
        line = json.dumps(req) + "\n"
        with self._lock:
            self._sock.sendall(line.encode("utf-8"))
            resp_line = self._file.readline()
        if not resp_line:
            raise IwordServerError("connection closed by server")
        resp = json.loads(resp_line)
        if "error" in resp:
            raise IwordServerError(resp["error"])
        return resp

    # ------------------------------------------------------------------
    # API
    # ------------------------------------------------------------------

    def ping(self) -> None:
        """Verify the connection is alive."""
        resp = self._call({"op": "ping"})
        if not resp.get("pong"):
            raise IwordServerError("unexpected ping response")

    def seek(self, word: str) -> int:
        """Return the category key (0–14) for word, or -1 if not found."""
        resp = self._call({"op": "seek", "word": word})
        if not resp.get("found"):
            return -1
        return resp["key"]

    def map(self, text: str, mode: int = MODE_HTML | MODE_FORBID) -> List[Match]:
        """Extract all matching words from text. Returns list of Match."""
        resp = self._call({"op": "map", "text": text, "mode": mode})
        return [
            Match(position=m["pos"], length=m["len"], key=m["key"])
            for m in resp.get("matches", [])
        ]

    def mask(self) -> int:
        """Return bitmask of category keys present in the loaded dictionary."""
        resp = self._call({"op": "mask"})
        return resp["mask"]

    def status(self) -> dict:
        """Return server status dict (loaded, version)."""
        return self._call({"op": "status"})

    def filter_text(self, text: str, mode: int = MODE_HTML | MODE_FORBID) -> str:
        """Replace all matched words in text with '*' characters."""
        matches = self.map(text, mode)
        if not matches:
            return text
        buf = bytearray(text.encode("utf-8"))
        for m in matches:
            for i in range(m.length):
                buf[m.position + i] = ord("*")
        return buf.decode("utf-8", errors="replace")

    def extract_by_key(self, text: str, key: int, mode: int = MODE_HTML) -> List[Match]:
        """Extract only matches with a specific category key."""
        return [m for m in self.map(text, mode) if m.key == key]
