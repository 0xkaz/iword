# iWord

**Microsecond keyword search for AI Agents and content filtering.**  
Dictionary loaded once into shared memory — all processes share it with zero copying.

[![CI](https://github.com/0xkaz/iword/actions/workflows/ci.yml/badge.svg)](https://github.com/0xkaz/iword/actions/workflows/ci.yml)

## Why iWord?

| | iWord | Redis | Regex | Aho-Corasick |
|---|---|---|---|---|
| Latency | **~1 µs** | ~1 ms | varies | ~10 µs |
| Network hop | none | yes | none | none |
| Cross-process | **SHM zero-copy** | via network | per-process | per-process |
| Multi-category | **yes (0-14)** | no | no | no |
| Update | reload | runtime | runtime | rebuild |

- **Zero-copy shared memory** — all processes/threads read the same in-memory hash table via `shmget`/`shmat`
- **O(N) rolling hash scan** — scans input text in a single pass, finding all matching words
- **Multi-category** — each word carries a category key (spam=2, adult=1, hidden=0, custom 0-14)
- **HTML-aware** — skips tags, decodes entities during scan
- **Bindings** — PHP PECL extension, Python (ctypes), Go (cgo)
- **AI Agent ready** — zero-latency guardrail and router before LLM calls

## AI Agent Use Cases

### Guardrail (block before LLM)
```python
from bindings.python.iword import map as iword_map, MODE_FORBID, MODE_HTML

def is_safe(text: str) -> bool:
    matches = iword_map(text, MODE_HTML | MODE_FORBID)
    return all(m.key not in (1, 2) for m in matches)  # block adult/spam

if not is_safe(user_input):
    return "Request blocked by content policy."
response = llm.invoke(user_input)
```

### Zero-latency Router (no embedding needed)
```python
from bindings.python.iword import map as iword_map, MODE_HTML

ROUTES = {3: "medical_agent", 4: "legal_agent", 5: "finance_agent"}

def route(text: str) -> str:
    matches = iword_map(text, MODE_HTML)
    if matches:
        key = max(set(m.key for m in matches), key=lambda k: sum(1 for m in matches if m.key == k))
        return ROUTES.get(key, "general_agent")
    return "general_agent"
```

### LangChain integration
```python
from langchain.tools import tool
from bindings.python.iword import seek

@tool
def check_word(word: str) -> str:
    """Check if a word is in the content filter dictionary."""
    key = seek(word)
    return f"category={key}" if key >= 0 else "not found"
```

See [`bindings/python/example_langchain.py`](bindings/python/example_langchain.py) for full LangChain + LlamaIndex examples.

## Quick Start

```bash
# Build C library and CLI tools
make tool

# Load a dictionary
bin/iwordctl load words.txt

# Search
bin/iwordctl seek apple
bin/iwordctl --json seek apple   # {"word":"apple","found":true,"key":9}

# Status
bin/iwordctl status

# Release shared memory
bin/iwordctl stop
```

### Dictionary Format

```
apple           # key 9 (default word)
spam_word	2   # key 2 (spam)
adult_word	1   # key 1 (adult)
hidden_word	0   # key 0 (hidden)
```

### Multiple Dictionaries

```bash
bin/iwordctl dict medical load medical_terms.txt
bin/iwordctl dict legal   load legal_terms.txt
bin/iwordctl dict medical seek cancer
```

## Build

**Requirements:** GCC, Make, phpize (for PHP extension)

```bash
make        # build everything
make tool   # CLI tools only → bin/iwordctl, bin/iworduse
make pecl   # PHP extension → bin/modules/iword.so
make clean
```

## Python Binding

```python
from bindings.python.iword import load, seek, map as iword_map, filter_text
from bindings.python.iword import MODE_HTML, MODE_FORBID

load("words.txt")                              # load dictionary
key = seek("spam_word")                        # -1 if not found
matches = iword_map(text, MODE_HTML | MODE_FORBID)
clean = filter_text(text, MODE_HTML | MODE_FORBID)  # replace matches with *
```

See [`bindings/python/example_rag.py`](bindings/python/example_rag.py) for RAG pre-processing examples.

## Go Binding

```go
import iword "github.com/0xkaz/iword/bindings/go"

iword.Load("words.txt")
key := iword.Seek("spam_word")         // -1 if not found
matches := iword.Map(text, iword.ModeHTML | iword.ModeForbid)
clean := iword.FilterText(text, iword.ModeHTML)
```

See [`bindings/go/example_router.go`](bindings/go/example_router.go) for an HTTP routing server.

## PHP Extension

```php
// php.ini: extension=iword.so
iword_set($text);              // scan text
$map   = iword_map();          // all matches: [position => length]
$spam  = iword_get_spam();     // spam words only
$adult = iword_get_adult();    // adult words only
```

## Docker

```bash
docker compose run --rm dev bash
# Inside container:
bin/iwordctl load /tmp/words.txt
bin/iwordctl seek apple
```

## How It Works

iWord builds a hash table from the word list and stores it in **System V shared memory** (`shmget`/`shmat`). The dictionary is loaded once by `iwordctl load` and stays resident until `iwordctl stop`. Any number of processes — PHP workers, Python services, Go binaries — read from the same memory segment without copying or network overhead.

Text scanning (`iword_map`) runs a rolling hash over every byte position and checks each candidate against the table in **O(N)** time, returning all matches with position, length, and category key.

## Memory Usage

~8 bytes per word. Typical sizes:

| Dictionary | Size |
|---|---|
| 100K words | ~1 MB |
| 1M words | ~10 MB |
| 10M words | ~100 MB |

## Notes

- Shared memory is released on OS reboot; reload after restart.
- For Kubernetes: use `emptyDir: { medium: Memory }` to share between containers in a pod.
- For AWS ECS: set `sharedMemorySize` in the task definition's `linuxParameters`.

## License

Originally developed by [@freaks / imos](http://imoz.jp/document/iword/).  
Maintained at [atfreaks/iword](https://github.com/atfreaks/iword).  
AI Agent extensions and multi-language bindings by [0xkaz](https://github.com/0xkaz).
