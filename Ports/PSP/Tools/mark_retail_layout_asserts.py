#!/usr/bin/env python3
"""Mark Win32-only sizeof checks so PSP builds keep logical assertions enabled."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


LAYOUT_ASSERT = re.compile(r"\bTSTATICASSERT(\(\s*sizeof\s*\()")


def iter_sources(paths: list[Path]):
    for path in paths:
        if path.is_dir():
            yield from (candidate for candidate in path.rglob("*") if candidate.suffix in {".h", ".hpp", ".cpp"})
        elif path.suffix in {".h", ".hpp", ".cpp"}:
            yield path


def mark_source(text: str) -> tuple[str, int]:
    return LAYOUT_ASSERT.subn(r"TSTATICASSERT_SIZE\1", text)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()

    found = 0
    for path in iter_sources(args.paths):
        source = path.read_text(encoding="utf-8")
        marked, count = mark_source(source)
        if count == 0:
            continue

        found += count
        print(f"{path}: {count}")
        if args.write:
            path.write_text(marked, encoding="utf-8", newline="")

    if found != 0 and not args.write:
        print(f"found {found} unmarked retail layout assertion(s)")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
