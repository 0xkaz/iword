# iWord

Fast word search library using shared memory (System V IPC).

- **O(N)** search over input text
- Dictionary loaded once into shared memory; all processes share it
- Supports up to ~100 million words
- HTML-aware scanning mode
- PHP PECL extension included

## Build

**Requirements:** GCC, Make, phpize (for the PHP extension)

```bash
# Build everything
make

# PHP PECL extension only → bin/modules/iword.so
make pecl

# CLI tools only → bin/iwordctl, bin/iworduse
make tool

# Clean
make clean
```

## CLI Usage (iwordctl)

```bash
# Load a dictionary file into shared memory
bin/iwordctl load words.txt

# Load multiple files with category flags
bin/iwordctl load words.txt -word spam.txt -spam adult.txt -adult

# Search for a word
bin/iwordctl seek <word>

# Show status and memory usage
bin/iwordctl status

# Release shared memory
bin/iwordctl stop

# Show version
bin/iwordctl version
```

### Dictionary Format

One word per line. An optional tab-separated flag sets the category:

```
apple           # default (key 9)
spam_word	2   # spam (key 2)
adult_word	1   # adult (key 1)
```

### Multiple Dictionaries

Use the `dict` subcommand to manage separate dictionaries by key:

```bash
bin/iwordctl dict mykey load words.txt
bin/iwordctl dict mykey seek apple
bin/iwordctl dict mykey stop
```

## PHP Extension

```php
// Load the extension
// extension=iword.so in php.ini

// Set the text to scan (HTML mode + forbid mode by default)
iword_set($text);

// Get all matched words: array of [position => length]
$map = iword_map();

// Get spam words only
$spam = iword_get_spam();

// Get adult words only
$adult = iword_get_adult();

// Check if a category exists in the dictionary
$exists = iword_exists(2); // true if spam words are loaded
```

## How It Works

iWord builds a hash table from the word list and stores it in System V shared memory (`shmget`/`shmat`). The dictionary is loaded once by `iwordctl load` and stays resident until `iwordctl stop` or OS reboot. Any number of processes can then read from the same shared segment without reloading.

Text scanning (`iword_map`) runs a rolling hash over every position in the input string and checks each candidate against the in-memory hash table in O(N) time.

## Notes

- Shared memory is released on OS reboot; reload the dictionary after restart.
- Memory usage: approximately 8 bytes per word. Wikipedia Japanese (~1M words) uses ~10 MB.
- For more information (Japanese): http://imoz.jp/document/iword/
