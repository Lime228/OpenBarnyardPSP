#!/usr/bin/env python3

import argparse
import json
import pathlib

from convert_frontend_terrain_pack import MAX_SOURCE_MESHES


MAX_SOURCE_CELLS = 16


def merge(sources: list[pathlib.Path], destination: pathlib.Path) -> dict[str, object]:
    if not 1 <= len(sources) <= MAX_SOURCE_CELLS:
        raise ValueError(f"frontend terrain merge requires between 1 and {MAX_SOURCE_CELLS} cells")

    world = None
    cell = None
    meshes = []
    for source_index, source in enumerate(sources):
        document = json.loads(source.read_text(encoding="utf-8"))
        if not isinstance(document, dict) or document.get("version") != 2:
            raise ValueError(f"frontend terrain cell {source_index} has an unsupported contract version")
        source_world = document.get("world")
        source_cell = document.get("cell")
        source_meshes = document.get("meshes")
        if not isinstance(source_world, int) or source_world < 0:
            raise ValueError(f"frontend terrain cell {source_index} has an invalid world")
        if not isinstance(source_cell, int) or source_cell < 0:
            raise ValueError(f"frontend terrain cell {source_index} has an invalid cell")
        if not isinstance(source_meshes, list) or not source_meshes:
            raise ValueError(f"frontend terrain cell {source_index} requires meshes")
        if world is None:
            world = source_world
            cell = source_cell
        elif source_world != world or source_cell != cell:
            raise ValueError(f"frontend terrain cell {source_index} does not match world {world} cell {cell}")
        if len(meshes) + len(source_meshes) > MAX_SOURCE_MESHES:
            raise ValueError(f"frontend terrain merge exceeds {MAX_SOURCE_MESHES} source meshes")
        for mesh_index, mesh in enumerate(source_meshes):
            if not isinstance(mesh, dict):
                raise ValueError(f"frontend terrain cell {source_index} mesh {mesh_index} must be an object")
        meshes.extend(source_meshes)

    merged = {"version": 2, "world": world, "cell": cell, "meshes": meshes}
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(merged, indent=2) + "\n", encoding="utf-8")
    return merged


def main() -> None:
    parser = argparse.ArgumentParser(description="Merge complete original frontend terrain cells in source order")
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("sources", nargs="+", type=pathlib.Path)
    args = parser.parse_args()
    merged = merge(args.sources, args.output)
    print(json.dumps({"world": merged["world"], "cell": merged["cell"], "meshes": len(merged["meshes"])}, sort_keys=True))


if __name__ == "__main__":
    main()
