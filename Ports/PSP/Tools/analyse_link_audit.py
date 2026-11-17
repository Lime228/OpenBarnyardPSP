#!/usr/bin/env python3
"""Summarise the repeatable PSP original-game compile/link audit."""

from __future__ import annotations

import argparse
import collections
import pathlib
import re
import shutil
import subprocess


UNDEFINED_RE = re.compile(r"undefined reference to [`'](.+?)[`']")
ERROR_RE = re.compile(r"(?:^|\s)([^\s:]+\.(?:c|cc|cpp|cxx|h|hpp)):(\d+)(?::\d+)?: error: (.+)$")


def demangle(symbols: list[str]) -> list[str]:
    tool = shutil.which("c++filt")
    if tool is None or not symbols:
        return symbols

    result = subprocess.run(
        [tool],
        input="\n".join(symbols),
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        return symbols

    names = result.stdout.splitlines()
    return names if len(names) == len(symbols) else symbols


def analyse(text: str) -> tuple[collections.Counter[str], collections.Counter[str]]:
    undefined: collections.Counter[str] = collections.Counter()
    errors: collections.Counter[str] = collections.Counter()

    for line in text.splitlines():
        undefined_match = UNDEFINED_RE.search(line)
        if undefined_match:
            undefined[undefined_match.group(1)] += 1

        error_match = ERROR_RE.search(line)
        if error_match:
            location = f"{error_match.group(1)}:{error_match.group(2)}"
            errors[f"{location}: {error_match.group(3)}"] += 1

    return undefined, errors


def render_markdown(
    undefined: collections.Counter[str],
    errors: collections.Counter[str],
    limit: int,
) -> str:
    symbols = list(undefined)
    readable = dict(zip(symbols, demangle(symbols)))
    lines = [
        "# PSP original-game link audit",
        "",
        f"- Compile errors: {sum(errors.values())} ({len(errors)} unique)",
        f"- Undefined references: {sum(undefined.values())} ({len(undefined)} unique)",
        "",
    ]

    if errors:
        lines.extend(["## Compile errors", ""])
        for error, count in errors.most_common(limit):
            lines.append(f"- `{error}` ({count})")
        lines.append("")

    if undefined:
        lines.extend(["## Undefined references", ""])
        for symbol, count in undefined.most_common(limit):
            lines.append(f"- `{readable[symbol]}` ({count})")
        lines.append("")

    if not errors and not undefined:
        lines.extend(["No compiler or linker failures were found.", ""])

    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--output", type=pathlib.Path)
    parser.add_argument("--limit", type=int, default=200)
    args = parser.parse_args()

    text = args.log.read_text(encoding="utf-8", errors="replace")
    undefined, errors = analyse(text)
    report = render_markdown(undefined, errors, max(1, args.limit))

    if args.output is None:
        print(report, end="")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8", newline="\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
