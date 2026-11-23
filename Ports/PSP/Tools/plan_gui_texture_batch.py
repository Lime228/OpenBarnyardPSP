#!/usr/bin/env python3
"""Resolve every retail GUI texture section through a complete TTL inventory."""

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path, PureWindowsPath


def normalize_texture_name(value: str) -> str:
    return value.replace("/", "\\").casefold()


def relative_path(value: str) -> str:
    path = PureWindowsPath(value)
    if not path.parts or path.anchor or any(
        part == ".." or ":" in part or part.rstrip(" .") != part or path_part_is_reserved(part)
        for part in path.parts
    ):
        raise ValueError(f"invalid relative asset path: {value!r}")
    return path.as_posix()


def path_part_is_reserved(part: str) -> bool:
    return PureWindowsPath(part).is_reserved()


def build_plan(sections: dict[str, object], libraries: dict[str, object]) -> dict[str, object]:
    section_items = list(sections.get("sections", []))
    if sections.get("section_count") != len(section_items):
        raise ValueError("GUI section inventory count is inconsistent")

    owners: dict[str, dict[tuple[str, str], str]] = defaultdict(dict)
    for library in libraries.get("libraries", []):
        if library.get("status") != "inventoried":
            continue
        library_path = relative_path(str(library["path"]))
        symbol = str(library["symbol"])
        for texture in library.get("textures", []):
            texture_name = str(texture["name"])
            owners[normalize_texture_name(texture_name)].setdefault(
                (library_path, symbol), texture_name
            )

    sections_by_texture: dict[str, list[dict[str, object]]] = defaultdict(list)
    original_names: dict[str, str] = {}
    for section in section_items:
        texture_name = str(section["texture"])
        key = normalize_texture_name(texture_name)
        original_names.setdefault(key, texture_name)
        sections_by_texture[key].append(
            {"index": int(section["index"]), "name": str(section["name"])}
        )

    items: list[dict[str, object]] = []
    resolved = 0
    for key in sorted(sections_by_texture):
        texture_name = original_names[key]
        base = {
            "texture": texture_name,
            "section_count": len(sections_by_texture[key]),
            "sections": sorted(sections_by_texture[key], key=lambda item: item["index"]),
        }
        variants = owners.get(key, {})
        if not variants:
            items.append({**base, "status": "unresolved"})
            continue
        resolved += 1
        for (library, symbol), source_name in sorted(variants.items()):
            output = f"{library}/{relative_path(symbol)}/{relative_path(texture_name)}.ptex"
            items.append(
                {
                    **base,
                    "source_library": library,
                    "source_symbol": symbol,
                    "source_texture": source_name,
                    "output": output,
                    "status": "resolved",
                }
            )

    return {
        "version": 2,
        "section_count": len(section_items),
        "texture_count": len(sections_by_texture),
        "resolved_texture_count": resolved,
        "unresolved_texture_count": len(sections_by_texture) - resolved,
        "source_variant_count": sum(item["status"] == "resolved" for item in items),
        "items": items,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Plan a complete retail GUI texture conversion batch"
    )
    parser.add_argument("sections", type=Path)
    parser.add_argument("libraries", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    plan = build_plan(
        json.loads(args.sections.read_text(encoding="utf-8-sig")),
        json.loads(args.libraries.read_text(encoding="utf-8-sig")),
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary_path.write_text(
        json.dumps(plan, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    temporary_path.replace(args.output)
    print(json.dumps({key: plan[key] for key in plan if key.endswith("count")}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
