#!/usr/bin/env python3

import argparse
import json
import pathlib

from convert_terrain_cell import MAX_TERRAIN_MESHES


def merge(sources: list[pathlib.Path], destination: pathlib.Path, world: int, cell: int) -> dict[str, object]:
    if not 1 <= len(sources) <= MAX_TERRAIN_MESHES:
        raise ValueError(f"terrain merge requires between 1 and {MAX_TERRAIN_MESHES} meshes")
    meshes = []
    for index, source in enumerate(sources):
        document = json.loads(source.read_text(encoding="utf-8"))
        if not isinstance(document, dict) or document.get("version") != 1:
            raise ValueError(f"terrain mesh {index} has an unsupported contract version")
        for required in ("material", "vertices", "triangle_strip"):
            if required not in document:
                raise ValueError(f"terrain mesh {index} is missing {required}")
        meshes.append({key: value for key, value in document.items() if key != "version"})
    merged = {"version": 2, "world": world, "cell": cell, "meshes": meshes}
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(merged, indent=2) + "\n", encoding="utf-8")
    return merged


def main() -> None:
    parser = argparse.ArgumentParser(description="Merge explicit retail terrain selections into one bounded PSP cell")
    parser.add_argument("--world", type=int, default=0)
    parser.add_argument("--cell", type=int, default=0)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("sources", nargs="+", type=pathlib.Path)
    args = parser.parse_args()
    if args.world < 0 or args.cell < 0:
        raise ValueError("world and cell must be non-negative")
    merged = merge(args.sources, args.output, args.world, args.cell)
    print(json.dumps({"world": args.world, "cell": args.cell, "meshes": len(merged["meshes"])}, sort_keys=True))


if __name__ == "__main__":
    main()
