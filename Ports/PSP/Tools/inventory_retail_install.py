#!/usr/bin/env python3
"""Build a deterministic, path-only inventory of a retail Barnyard install."""

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path


FORMAT_BY_EXTENSION = {
    ".bik": "movie",
    ".fsb": "audio-bank",
    ".mp3": "audio-stream",
    ".tkl": "animation-library",
    ".trb": "resource-container",
    ".trz": "compressed-resource-container",
    ".tso": "script-object",
    ".ttl": "texture-library",
}


def classify_format(path: Path) -> str:
    return FORMAT_BY_EXTENSION.get(path.suffix.lower(), "support-file")


def classify_area(relative_path: Path) -> str:
    parts = relative_path.parts
    if len(parts) >= 2 and parts[0].lower() == "data":
        return f"Data/{parts[1]}"
    if parts:
        return parts[0]
    return "."


def _summaries(values: dict[str, dict[str, object]]) -> list[dict[str, object]]:
    result = []
    for name in sorted(values, key=str.casefold):
        value = values[name]
        result.append(
            {
                "name": name,
                "file_count": value["file_count"],
                "bytes": value["bytes"],
                "extensions": sorted(value["extensions"]),
            }
        )
    return result


def build_inventory(install_root: Path) -> dict[str, object]:
    install_root = install_root.resolve()
    if not install_root.is_dir():
        raise ValueError(f"retail install directory does not exist: {install_root}")

    files: list[dict[str, object]] = []
    formats: dict[str, dict[str, object]] = defaultdict(
        lambda: {"file_count": 0, "bytes": 0, "extensions": set()}
    )
    areas: dict[str, dict[str, object]] = defaultdict(
        lambda: {"file_count": 0, "bytes": 0, "extensions": set()}
    )

    source_files = sorted(
        (path for path in install_root.rglob("*") if path.is_file()),
        key=lambda path: path.relative_to(install_root).as_posix().casefold(),
    )
    for source_path in source_files:
        relative_path = source_path.relative_to(install_root)
        relative_name = relative_path.as_posix()
        extension = source_path.suffix.lower()
        format_name = classify_format(source_path)
        area_name = classify_area(relative_path)
        byte_count = source_path.stat().st_size

        files.append(
            {
                "path": relative_name,
                "extension": extension,
                "format": format_name,
                "area": area_name,
                "bytes": byte_count,
            }
        )
        for summary, name in ((formats, format_name), (areas, area_name)):
            summary[name]["file_count"] += 1
            summary[name]["bytes"] += byte_count
            summary[name]["extensions"].add(extension)

    return {
        "version": 1,
        "install_root": install_root.name,
        "file_count": len(files),
        "bytes": sum(item["bytes"] for item in files),
        "formats": _summaries(formats),
        "areas": _summaries(areas),
        "files": files,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Inventory every file in an original Barnyard installation"
    )
    parser.add_argument("install_root", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    inventory = build_inventory(args.install_root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary_path.write_text(
        json.dumps(inventory, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    temporary_path.replace(args.output)

    print(
        json.dumps(
            {
                "files": inventory["file_count"],
                "bytes": inventory["bytes"],
                "formats": {
                    item["name"]: item["file_count"] for item in inventory["formats"]
                },
                "output": str(args.output.resolve()),
            },
            separators=(",", ":"),
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
