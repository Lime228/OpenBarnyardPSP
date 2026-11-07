#!/usr/bin/env python3

import argparse
import json
import pathlib
import re


INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]', re.MULTILINE)


def resolve_include(source: pathlib.Path, include: str, roots: list[pathlib.Path]) -> pathlib.Path | None:
    candidates = [source.parent / include]
    candidates.extend(root / include for root in roots)
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    return None


def audit(entries: list[pathlib.Path], roots: list[pathlib.Path]) -> dict[str, object]:
    resolved_roots = [root.resolve() for root in roots]
    pending = [entry.resolve() for entry in entries]
    visited: set[pathlib.Path] = set()
    edges: dict[str, list[str]] = {}
    missing_project: set[str] = set()
    external: set[str] = set()

    while pending:
        source = pending.pop()
        if source in visited:
            continue
        if not source.is_file():
            raise ValueError(f"entry source does not exist: {source}")
        visited.add(source)
        dependencies = []
        text = source.read_text(encoding="utf-8-sig", errors="strict")
        for delimiter, include in INCLUDE_PATTERN.findall(text):
            resolved = resolve_include(source, include.replace("\\", "/"), resolved_roots)
            if resolved is None:
                (missing_project if delimiter == '"' else external).add(include)
                continue
            dependencies.append(resolved.as_posix())
            if resolved not in visited:
                pending.append(resolved)
        edges[source.as_posix()] = sorted(set(dependencies))

    return {
        "version": 1,
        "entries": sorted(entry.resolve().as_posix() for entry in entries),
        "files": sorted(source.as_posix() for source in visited),
        "edges": dict(sorted(edges.items())),
        "missing_project_includes": sorted(missing_project),
        "external_includes": sorted(external),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Audit the transitive local include graph for a PSP source slice")
    parser.add_argument("--root", action="append", required=True, type=pathlib.Path, dest="roots")
    parser.add_argument("--output", type=pathlib.Path)
    parser.add_argument("entries", nargs="+", type=pathlib.Path)
    args = parser.parse_args()
    result = audit(args.entries, args.roots)
    payload = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload, encoding="utf-8")
    else:
        print(payload, end="")


if __name__ == "__main__":
    main()
