# iWord - Claude Code Guide

## Project Structure

```
include/iword.c   Core library (SHM dictionary, hash search)
include/iword.h   Core API definitions
pecl/             PHP PECL extension
tool/             CLI tools (iwordctl, iworduse)
.github/          CI configuration
_*.md             Design documents (gitignored, not tracked)
```

## Build

```bash
make pecl    # Build PHP PECL extension → bin/modules/iword.so
make tool    # Build CLI tools → bin/iwordctl, bin/iworduse
make         # Both
make clean   # Remove build artifacts
```

## Testing (manual smoke test)

```bash
# Load a dictionary
bin/iwordctl load /path/to/words.txt

# Search for a word
bin/iwordctl seek <word>

# Check status
bin/iwordctl status

# Release SHM (always clean up after testing)
bin/iwordctl stop
```

## Shared Memory (SHM)

- iWord uses System V IPC (shmget/shmat)
- When using multiple dictionaries, use `iwordctl dict <key> load ...` to separate keys
- Always run `iwordctl stop` or `iwordctl dict <key> stop` after testing
- In CI, use unique keys per job to avoid collisions between parallel jobs

## Coding Conventions

- C99 compliant; target zero warnings with `gcc -Wall -Wextra`
- All functions use the `iword_` prefix
- Comments may be in Japanese (consistent with existing code)
- Use `static inline` instead of bare `inline`

## Branch Policy

- `master` must always build successfully (CI required)
- Branch prefixes: `ci/`, `fix/`, `feature/`
- PHP 8.x compatibility is tracked in `fix/php8-compat`

## Notes for Claude

- Use the `Explore` agent for file searches
- Use the `Plan` agent to assess impact before large changes
- Always include SHM cleanup (`iwordctl stop`) in any test steps
- Verify PHP extension changes with `make pecl` before proposing
- Record design decisions and issue tracking in `_plan.md`
- Do not run `git commit` or `git push`; leave that to the user
