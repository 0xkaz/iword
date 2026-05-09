# iWord

**High-speed keyword search with shared memory — one dictionary, many processes.**

Dictionary is loaded once into System V shared memory. All processes (PHP, Python, Go, Node.js) read from the same in-memory hash table with zero copying and zero network overhead.

[![CI](https://github.com/0xkaz/iword/actions/workflows/ci.yml/badge.svg)](https://github.com/0xkaz/iword/actions/workflows/ci.yml)

## Features

- **Zero-copy shared memory** — `shmget`/`shmat`; all processes share one dictionary instance
- **O(N) rolling hash scan** — single-pass text scan, finds all matching words
- **Multi-category** — each word carries a category key (0–14); spam, adult, hidden, custom
- **HTML-aware** — skips tags and decodes entities during scan
- **Multi-language** — PHP PECL extension, Python (ctypes), Go (CGO + iwordserver client), Node.js (N-API)
- **iword-server** — Unix socket + TCP server for language-agnostic access (no CGO required)
- **CLI tools** — `iwordctl` for dictionary load/seek/status, `iwordserver` for server mode

## Quick Start

```bash
# Build CLI tools and shared library
make tool   # → bin/iwordctl, bin/iworduse
make lib    # → bin/libiword.so  (for Python/Go/Node)

# Load a dictionary
bin/iwordctl load words.txt

# Search
bin/iwordctl seek apple
bin/iwordctl --json seek apple   # {"word":"apple","found":true,"key":9}

# Status / cleanup
bin/iwordctl status
bin/iwordctl stop
```

### Dictionary Format

```
apple                   # key 9 (default)
spam_word	2       # key 2 (spam)
adult_word	1       # key 1 (adult)
hidden_word	0       # key 0 (hidden/forbidden)
```

Tab-separated: `word<TAB>key`. Lines starting with `#` are ignored.

### Multiple Dictionaries

```bash
bin/iwordctl dict medical load medical_terms.txt
bin/iwordctl dict legal   load legal_terms.txt
bin/iwordctl dict medical seek cancer
bin/iwordctl dict medical stop
```

## Build

**Requirements:** GCC, Make. `phpize` only needed for PHP extension.

```bash
make tool   # CLI tools → bin/iwordctl, bin/iworduse, bin/iwordserver
make lib    # Shared library → bin/libiword.so (Python/Go/Node bindings)
make        # Both of the above (default)
make full   # tool + lib + PHP extension (requires phpize)
make pecl   # PHP extension only → bin/modules/iword.so
make clean
```

## Python Binding

```python
import sys; sys.path.insert(0, 'bindings/python')
from iword import load, seek, map as iword_map, filter_text
from iword import MODE_HTML, MODE_FORBID

load("words.txt")                                    # load dictionary into SHM
key = seek("spam_word")                              # returns key (0-14), or -1 if not found
matches = iword_map(text, MODE_HTML | MODE_FORBID)   # list of Match(position, length, key)
clean   = filter_text(text, MODE_HTML | MODE_FORBID) # replace matched words with '*'
```

See [`bindings/python/example_langchain.py`](bindings/python/example_langchain.py) for LangChain/LlamaIndex integration examples.  
See [`bindings/python/example_rag.py`](bindings/python/example_rag.py) for RAG pre-processing examples.

## Node.js Binding

```javascript
const iword = require('./bindings/node');

iword.load('words.txt');                                   // load dictionary into SHM
const key     = iword.seek('spam_word');                   // -1 if not found
const matches = iword.map(text, iword.MODE_HTML | iword.MODE_FORBID);
const clean   = iword.filterText(text, iword.MODE_HTML | iword.MODE_FORBID);
```

**Requirements:** `make lib && npm run build` in `bindings/node/` (builds N-API native addon).

## Go Binding

Two integration paths are available:

**iwordserver client (recommended)** — connects to a running `iwordserver` process via Unix socket or TCP. No CGO, no thread-safety concerns.

```go
import iword "github.com/0xkaz/iword/bindings/go"

c, err := iword.NewClient("unix", "/tmp/iword.sock")
defer c.Close()

key, _     := c.Seek("spam_word")                         // -1 if not found
matches, _ := c.Map(text, iword.ModeHTML|iword.ModeForbid)
clean, _   := c.FilterText(text, iword.ModeHTML|iword.ModeForbid)
```

**CGO direct call (performance-critical only)** — calls iword C library in-process. Requires `sync.Mutex` because `iword_seek`/`iword_map` are not thread-safe.

```go
import iword "github.com/0xkaz/iword/bindings/go"

iword.Load("words.txt")
key     := iword.Seek("spam_word")                        // -1 if not found
matches := iword.Map(text, iword.ModeHTML|iword.ModeForbid)
clean   := iword.FilterText(text, iword.ModeHTML|iword.ModeForbid)
```

See [`bindings/go/example_router.go`](bindings/go/example_router.go) for a routing server example.

## iword-server

A standalone server that exposes iword over Unix socket and TCP using a newline-delimited JSON protocol. Any language can connect without CGO or FFI.

```bash
# Start server (dictionary must be loaded first)
bin/iwordctl load words.txt
bin/iwordserver -u /tmp/iword.sock -p 7743

# Options
#   -u PATH   Unix socket path (default: /tmp/iword.sock)
#   -p PORT   TCP port (default: 7743; -p 0 to disable)
#   -d KEY    Dictionary key (for multiple dictionaries)
#   -n        Disable Unix socket
```

**Protocol** — send one JSON object per line, receive one JSON response per line:

```bash
# seek
echo '{"op":"seek","word":"spam_word"}' | nc -U /tmp/iword.sock
# → {"found":true,"key":2}

# map
echo '{"op":"map","text":"get free prize now","mode":3}' | nc -U /tmp/iword.sock
# → {"matches":[{"pos":4,"len":4,"key":2},{"pos":9,"len":5,"key":2}],"mask":4}

# other ops: ping, mask, status
```

See [`iword-server-spec.md`](iword-server-spec.md) for the full protocol specification.

## PHP Extension

```php
// php.ini: extension=iword.so
iword_set($text);
$map   = iword_map();         // all matches: [position => length]
$spam  = iword_get_spam();    // spam words only
$adult = iword_get_adult();   // adult words only
```

## How It Works

iWord builds a hash table from a word list and stores it in **System V shared memory** (`shmget`/`shmat`). The dictionary is loaded once by `iwordctl load` and stays resident until `iwordctl stop`. Any number of processes — PHP workers, Python services, Go binaries — read from the same memory segment without copying or network overhead.

Text scanning (`iword_map`) runs a rolling hash over every byte position, checking each candidate against the table in **O(N)** time, returning all matches with position, length, and category key.

## Memory Usage

~8 bytes per word.

| Dictionary size | Memory |
|---|---|
| 10K words | ~80 KB |
| 100K words | ~800 KB |
| 1M words | ~8 MB |

## Docker

```bash
docker compose run --rm dev bash
# Inside container:
make tool && make lib
bin/iwordctl load /tmp/words.txt
bin/iwordctl seek apple
bin/iwordctl stop
```

## Notes

- Shared memory persists until `iwordctl stop` or OS reboot. Always run `stop` after testing.
- System V SHM is not available on AWS Lambda, Cloud Run, or similar serverless platforms.
- For Kubernetes: use `emptyDir: { medium: Memory }` to share SHM between containers in a pod.
- For AWS ECS: `sharedMemorySize` in `linuxParameters` covers `/dev/shm` (POSIX), not System V SHM. A sidecar container running `iwordctl` is required.

## Credits

Originally developed by [imos](http://imoz.jp/document/iword/).  
Multi-language bindings and extensions by [0xkaz](https://github.com/0xkaz).
