# iword — High-speed keyword search with shared memory

Python binding for [iWord](https://github.com/atfreaks/iword): zero-copy keyword search via System V shared memory.

## Requirements

1. Build the shared library:
   ```bash
   git clone https://github.com/atfreaks/iword
   cd iword && make lib   # → bin/libiword.so
   ```

2. Load a dictionary:
   ```bash
   bin/iwordctl load dict/spam_en.txt
   ```

## Install

```bash
pip install iword
```

## Usage

```python
from iword import seek, map as iword_map, filter_text, extract_by_key
from iword import MODE_HTML, MODE_FORBID, KEY_SPAM

# Search for a single word
key = seek("spam")           # returns 2 (KEY_SPAM), or -1 if not found

# Scan text for all matches
matches = iword_map("Get your free prize now!", MODE_HTML | MODE_FORBID)
# [Match(position=8, length=4, key=2), ...]

# Replace matches with '*'
clean = filter_text("buy spam now", MODE_HTML | MODE_FORBID)
# "buy **** now"

# Extract matches by category
spam_only = extract_by_key(text, KEY_SPAM, MODE_HTML | MODE_FORBID)
```

## LangChain Integration

```python
from langchain.tools import BaseTool
from iword import map as iword_map, MODE_HTML, MODE_FORBID, KEY_SPAM

class IWordSpamFilter(BaseTool):
    name = "iword_spam_filter"
    description = "Detect spam keywords in text using shared memory dictionary."

    def _run(self, text: str) -> dict:
        matches = iword_map(text, MODE_HTML | MODE_FORBID)
        spam = [m for m in matches if m.key == KEY_SPAM]
        return {"is_spam": len(spam) > 0, "matches": len(spam)}
```

## Notes

- Requires `bin/libiword.so` (built via `make lib`) at runtime
- Dictionary must be loaded via `iwordctl load` before calling `seek`/`map`
- System V SHM is not available on AWS Lambda, Cloud Run, or other serverless platforms
