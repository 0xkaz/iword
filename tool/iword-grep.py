#!/usr/bin/env python3
"""
iword-grep: keyword-category-aware file search using iWord dictionary.

Usage:
    iword-grep.py [options] <target> [<target> ...]

Options:
    --key <k>       Filter by category key (0-14). May be repeated.
    --any           Show all matches regardless of key (default)
    --json          Output JSON lines
    --dict <file>   Load this dictionary file before searching
    --recursive, -r Search directories recursively
    --files-with-matches, -l  Print only filenames that have matches
    -n              Print line numbers (default: on)
    -h, --help      Show this help

Category keys:
    0  hidden/forbidden    1  adult    2  spam
    9  default (general)   others: custom

Examples:
    # Find spam words in all .txt files
    iword-grep.py --key 2 *.txt

    # Load a dictionary and grep recursively
    iword-grep.py --dict dict/spam_en.txt --key 2 -r ./logs

    # JSON output for pipeline
    iword-grep.py --json --key 1 --key 2 document.txt
"""

import sys
import os
import json
import argparse
import glob

# Locate iword.py relative to this script (../bindings/python/)
_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_BINDINGS_DIR = os.path.join(_SCRIPT_DIR, "..", "bindings", "python")
if _BINDINGS_DIR not in sys.path:
    sys.path.insert(0, _BINDINGS_DIR)

try:
    import iword
except OSError as e:
    print(f"Error: {e}", file=sys.stderr)
    print("Run 'make lib' to build bin/libiword.so first.", file=sys.stderr)
    sys.exit(1)


def parse_args():
    p = argparse.ArgumentParser(
        description="iWord keyword-category-aware grep",
        add_help=False,
    )
    p.add_argument("targets", nargs="*", metavar="target")
    p.add_argument("--key", action="append", type=int, dest="keys", metavar="K")
    p.add_argument("--any", action="store_true")
    p.add_argument("--json", action="store_true")
    p.add_argument("--dict", metavar="FILE")
    p.add_argument("--recursive", "-r", action="store_true")
    p.add_argument("--files-with-matches", "-l", action="store_true")
    p.add_argument("-n", action="store_true", default=True)
    p.add_argument("-h", "--help", action="store_true")
    return p.parse_args()


def collect_files(targets, recursive):
    files = []
    for t in targets:
        if os.path.isfile(t):
            files.append(t)
        elif os.path.isdir(t):
            if recursive:
                for root, _, fnames in os.walk(t):
                    for fn in fnames:
                        files.append(os.path.join(root, fn))
            else:
                print(f"iword-grep: {t}: Is a directory (use -r)", file=sys.stderr)
        else:
            # glob expansion
            expanded = glob.glob(t)
            if expanded:
                files.extend(expanded)
            else:
                print(f"iword-grep: {t}: No such file", file=sys.stderr)
    return files


def grep_file(filepath, keys, json_out, files_only, show_line_nums):
    try:
        with open(filepath, "r", encoding="utf-8", errors="replace") as f:
            lines = f.readlines()
    except OSError as e:
        print(f"iword-grep: {filepath}: {e}", file=sys.stderr)
        return False

    found_any = False
    for lineno, line in enumerate(lines, 1):
        stripped = line.rstrip("\n")
        matches = iword.map(stripped, mode=iword.MODE_HTML | iword.MODE_FORBID)
        if keys:
            matches = [m for m in matches if m.key in keys]
        if not matches:
            continue

        found_any = True
        if files_only:
            return True

        if json_out:
            for m in matches:
                word = stripped[m.position:m.position + m.length]
                print(json.dumps({
                    "file": filepath,
                    "line": lineno,
                    "pos": m.position,
                    "length": m.length,
                    "key": m.key,
                    "word": word,
                    "text": stripped,
                }, ensure_ascii=False))
        else:
            words = [stripped[m.position:m.position + m.length] for m in matches]
            key_summary = ",".join(str(m.key) for m in matches)
            prefix = f"{filepath}:{lineno}" if show_line_nums else filepath
            print(f"{prefix}: [{key_summary}] {stripped}  # matched: {words}")

    return found_any


def main():
    args = parse_args()

    if args.help or not args.targets:
        print(__doc__)
        sys.exit(0)

    # Load dictionary if specified
    if args.dict:
        ret = iword.load(args.dict)
        if ret != 0:
            print(f"Error: failed to load dictionary: {args.dict}", file=sys.stderr)
            sys.exit(1)

    files = collect_files(args.targets, args.recursive)
    if not files:
        sys.exit(1)

    keys = set(args.keys) if args.keys else None
    any_match = False

    for filepath in files:
        hit = grep_file(
            filepath,
            keys=keys,
            json_out=args.json,
            files_only=args.files_with_matches,
            show_line_nums=args.n,
        )
        if hit:
            any_match = True
            if args.files_with_matches:
                print(filepath)

    sys.exit(0 if any_match else 1)


if __name__ == "__main__":
    main()
