#!/usr/bin/env python3
"""Generate the original Barnyard pooled-string definitions on non-Windows hosts."""

from __future__ import annotations

import argparse
import pathlib
import re


IDENTIFIER_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def read_strings(path: pathlib.Path) -> list[str]:
    strings: list[str] = []
    seen: set[str] = set()

    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        value = raw_line.strip()
        if not value or value.startswith("#"):
            continue
        if not IDENTIFIER_RE.fullmatch(value):
            raise ValueError(f"{path}:{line_number}: invalid pooled-string identifier {value!r}")
        if value in seen:
            raise ValueError(f"{path}:{line_number}: duplicate pooled-string identifier {value!r}")

        seen.add(value)
        strings.append(value)

    if not strings:
        raise ValueError(f"{path}: no pooled strings found")
    return strings


def generate(strings: list[str]) -> str:
    lines = [
        "//-----------------------------------------------------------------------------",
        "// Contains information about predefined pooled strings.",
        "// Note: Automatically generated source file.",
        "//-----------------------------------------------------------------------------",
        '#include "pch.h"',
        "#include <Toshi/TPString8.h>",
        "",
        "TOSHI_NAMESPACE_USING",
        "",
    ]
    lines.extend(f"TPString8 g_str_{value};" for value in strings)
    lines.extend(
        [
            "",
            "static TPString8Initialiser::StringMap s_aPooledStrings[] =",
            "{",
        ]
    )
    lines.extend(f'\t{{ &g_str_{value}, "{value}" }},' for value in strings)
    lines.extend(
        [
            "};",
            "",
            "static TPString8Initialiser s_oPooledStringsInitialiser(",
            "\ts_aPooledStrings,",
            "\tTARRAYSIZE( s_aPooledStrings ),",
            "\tTTRUE",
            ");",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()

    content = generate(read_strings(args.input))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary.write_text(content, encoding="utf-8", newline="\n")
    temporary.replace(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
