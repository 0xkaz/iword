# iword-server Protocol Specification

## Overview

`iwordserver` wraps the iword C library behind a Unix socket / TCP interface,
enabling any language to access iword without CGO or FFI.

## Phases

| Phase | Wire format | Status |
|-------|-------------|--------|
| 1 | Newline-delimited JSON | Implemented (`tool/iwordserver.c`) |
| 2 | Length-prefixed MessagePack | Planned (same message structure) |

---

## Transport

| Type | Default | Flag |
|------|---------|------|
| Unix socket | `/tmp/iword.sock` | `-u PATH` |
| TCP | port 7743 | `-p PORT` (`-p 0` to disable) |

Both transports use the same protocol. Unix socket is recommended for same-host deployments.

---

## Protocol — Phase 1: Newline-delimited JSON

### Framing

```
<JSON object>\n
```

- One request per line, terminated by `\n`
- One response per line, terminated by `\n`
- Strict 1:1 request/response (no streaming, no pipelining)
- Encoding: UTF-8

### Threading model

```
Client connections (I/O thread per connection)
       ↓  enqueue
   Worker queue (single worker thread)
       ↓  dequeue (serialized)
   iword C library (SHM — not thread-safe)
```

`iword_seek` and `iword_map` are NOT thread-safe. A single worker thread
processes all iword calls serially. I/O threads enqueue requests and block
until the worker signals completion.

---

## Operations

### ping

```json
// Request
{"op":"ping"}

// Response
{"pong":true}
```

### seek

```json
// Request
{"op":"seek","word":"free"}

// Response — found
{"found":true,"key":2}

// Response — not found
{"found":false,"key":null}
```

| Field | Type | Description |
|-------|------|-------------|
| `word` | string | Word to look up (required) |
| `found` | bool | Whether the word exists in the dictionary |
| `key` | int\|null | Category key (0–14); `null` when not found |

### map

```json
// Request
{"op":"map","text":"get free prize now","mode":3}

// Response
{"matches":[{"pos":4,"len":4,"key":2},{"pos":9,"len":5,"key":2}],"mask":4}
```

| Field | Type | Description |
|-------|------|-------------|
| `text` | string | Text to scan (required) |
| `mode` | int | Mode flags (default: `3` = HTML + FORBID) |
| `matches` | array | List of matches (empty array if none) |
| `matches[].pos` | int | Byte offset of match in `text` |
| `matches[].len` | int | Byte length of matched word |
| `matches[].key` | int | Category key of matched word |
| `mask` | int | Bitmask of all matched category keys |

#### Mode flags

| Value | Constant | Description |
|-------|----------|-------------|
| `0x1` | `MODE_HTML` | Skip HTML tag content |
| `0x2` | `MODE_FORBID` | Return forbidden-category words (key < 5) |
| `0x4` | `MODE_ENGLISH` | Split on English word boundaries |

Typical usage: `mode: 3` (HTML + FORBID).

### mask

```json
// Request
{"op":"mask"}

// Response
{"mask":518}
```

| Field | Type | Description |
|-------|------|-------------|
| `mask` | int | Bitmask of category keys present in the loaded dictionary |

Bit `i` is set if at least one word with `key=i` exists in the dictionary.

### status

```json
// Request
{"op":"status"}

// Response
{"loaded":true,"version":"0.7.1 2010.01.06"}
```

### Error responses

Any operation may return an error object:

```json
{"error":"missing word"}
{"error":"missing text"}
{"error":"unknown op: foo"}
{"error":"missing op"}
```

---

## Category key constants

| key | Constant | Meaning |
|-----|----------|---------|
| 0 | `KEY_HIDDEN` | Hidden category |
| 1 | `KEY_ADULT` | Adult content |
| 2 | `KEY_SPAM` | Spam / marketing |
| 3–4 | (reserved) | Forbidden (key < 5 = `IWORD_FORBID_NUM`) |
| 5–8 | (reserved) | — |
| 9 | `KEY_DEFAULT` | Standard dictionary word |
| 10–14 | (user-defined) | Application-specific categories |

Words with key < 5 are "forbidden" and only returned when `MODE_FORBID` is set.

---

## Startup options

```
iwordserver [options]

  -u PATH   Unix socket path          (default: /tmp/iword.sock)
  -p PORT   TCP port                  (default: 7743; 0 = disabled)
  -d KEY    Dictionary key string     (passed to iword_set_strkey)
  -n        Disable Unix socket
  -h        Show help
```

---

## Client examples

### bash / nc

```bash
# Unix socket
echo '{"op":"seek","word":"free"}' | nc -U /tmp/iword.sock
# → {"found":true,"key":2}

# TCP
echo '{"op":"map","text":"get free prize","mode":3}' | nc localhost 7743
```

### Python

```python
import socket, json

def iword_seek(word, sock_path="/tmp/iword.sock"):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(sock_path)
    sock.sendall((json.dumps({"op": "seek", "word": word}) + "\n").encode())
    resp = json.loads(sock.makefile().readline())
    sock.close()
    return resp

result = iword_seek("free")
# → {'found': True, 'key': 2}
```

### Go (without CGO)

```go
conn, _ := net.Dial("unix", "/tmp/iword.sock")
defer conn.Close()

req, _ := json.Marshal(map[string]string{"op": "seek", "word": "free"})
fmt.Fprintf(conn, "%s\n", req)

var resp map[string]interface{}
json.NewDecoder(conn).Decode(&resp)
fmt.Println(resp) // map[found:true key:2]
```

### Node.js

```javascript
const net = require('net');

function seek(word) {
  return new Promise((resolve) => {
    const sock = net.createConnection('/tmp/iword.sock', () => {
      sock.write(JSON.stringify({ op: 'seek', word }) + '\n');
    });
    sock.once('data', (buf) => {
      resolve(JSON.parse(buf.toString()));
      sock.destroy();
    });
  });
}

seek('free').then(console.log); // { found: true, key: 2 }
```

---

## Phase 2: MessagePack migration

Phase 2 replaces JSON with MessagePack using the same message structure.
The only client-side change is swapping the encode/decode library.

**Framing change**: MessagePack is binary with variable length, so a
4-byte big-endian length prefix is prepended:

```
[uint32 big-endian: message length][MessagePack bytes]
```

Example seek request (binary, schematic):

```
00 00 00 12  fixmap{2}  "op" "seek"  "word" "free"
```

---

## Performance

| Path | Latency | Recommended use |
|------|---------|-----------------|
| iwordserver via Unix socket | ~100 µs | **Default** (same host) |
| iwordserver via TCP | ~500 µs–1 ms | Cross-host |
| CGO (direct call) | ~1 µs | Performance-critical only |

**iwordserver is the default integration path.** CGO is the fastest option
within the same process, but it requires CGO-capable runtimes, explicit
`sync.Mutex` serialization (iword is not thread-safe), and tighter deployment
coupling. iwordserver provides process isolation, works from any language, and
eliminates the thread-safety burden. CGO remains available as an opt-in for
latency-critical use cases that iwordserver cannot satisfy.

---

## Connection management recommendations

- **Short-lived clients** (nc, curl-style): open connection, send one request, close.
- **Long-lived clients** (igate, daemons): maintain a persistent connection and reuse it.
  The server supports multiple concurrent connections; the worker serializes iword calls internally.
- **Connection pool** (high-throughput): maintain N persistent connections per worker process.
  The server queues requests; throughput is bounded by iword processing speed (~µs/request).

---

## Dictionary key and concurrent connection consistency

### Current constraints (Phase 1)

`iword_set_strkey` (dictionary key switching) writes to a process-global variable.
iwordserver serializes all calls through a single worker thread, so **there is no
race condition**, but the dictionary key is fixed at startup via `-d KEY`.
**Per-connection dictionary switching is not supported.**

```
Client A ─┐
Client B ─┼─→ Worker (serial) → iword (one dictionary)
Client C ─┘
```

### Multiple dictionaries

Run a separate iwordserver instance per dictionary (recommended):

```bash
iwordserver -u /tmp/iword-spam.sock   -d spam-dict
iwordserver -u /tmp/iword-adult.sock  -d adult-dict
```

Clients connect to the appropriate socket based on the dictionary they need.

### Future extension options (not yet implemented)

| Approach | Protocol change | Notes |
|----------|----------------|-------|
| Per-request dict key | `{"op":"seek","word":"free","dict":"spam-dict"}` | Worker calls `iword_set_strkey` each time. Safe because of serial processing. |
| Per-connection dict key | `{"op":"use","dict":"spam-dict"}` on connect | Adds session state. Connection pool users must ensure connections are not shared across dict contexts. |
| Hot dictionary reload | `{"op":"reload","file":"/path/to/dict.txt"}` | Re-invokes `iword_load` at runtime to refresh the dictionary without restarting the server. |
| Built-in watchdog | (server-side) | iwordserver detects its own crash / stuck state and self-restarts. Currently handled externally via `iwordd`. Future option: embed a watchdog thread inside iwordserver itself to eliminate the external supervisor dependency. |
