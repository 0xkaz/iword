# iWord Sample Dictionaries

Sample dictionaries for quick-start testing. These are **not production-grade** — they are minimal curated lists for demonstration.

## Files

| File | Key | Entries | Description |
|------|-----|---------|-------------|
| `spam_en.txt` | 2 | ~50 | Common English spam keywords |
| `profanity_en.txt` | 1 | ~30 | Harmful/abusive words (minimal) |

## Format

```
word<TAB>key
```

- `key=1` — adult/harmful (`IWORD_KEY_ADULT`)
- `key=2` — spam (`IWORD_KEY_SPAM`)
- `key=9` — default/general (`IWORD_KEY_DEFAULT`)
- Lines starting with `#` are comments (stripped by iwordctl)

## Usage

```bash
# Load and search
bin/iwordctl load dict/spam_en.txt
bin/iwordctl seek "free"
bin/iwordctl stop

# Grep files
python3 tool/iword-grep.py --dict dict/spam_en.txt --key 2 *.txt

# Python
import sys; sys.path.insert(0, 'bindings/python')
import iword
iword.load('dict/spam_en.txt')
print(iword.seek('free'))   # → 2
```

## For Production Use

These sample dictionaries are intentionally small. For production:

- **English spam**: [SpamAssassin rules](https://spamassassin.apache.org/), [UCI Spambase](https://archive.ics.uci.edu/dataset/94/spambase) (CC BY 4.0)
- **English profanity**: [CMU Offensive Word List](https://www.cs.cmu.edu/~biglou/resources/) (free), [profanity-list](https://github.com/dsojevic/profanity-list)
- **Japanese**: Collect from public NG word lists; use MeCab/fugashi for tokenization

## License

Sample dictionaries in this directory are released under CC0 (public domain).
