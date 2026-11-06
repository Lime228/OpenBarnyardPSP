#!/usr/bin/env python3

import argparse
import json
import math
import os
import pathlib
import tempfile

from convert_terrain_dump import convert_document


MAX_TERRAIN_MESHES = 4


def convert(source: pathlib.Path, destination: pathlib.Path) -> dict[str, object]:
    document = json.loads(source.read_text(encoding="utf-8"))
    if not isinstance(document, dict) or document.get("version") != 2:
        raise ValueError("unsupported terrain cell contract version")
    meshes = document.get("meshes")
    if not isinstance(meshes, list) or not 1 <= len(meshes) <= MAX_TERRAIN_MESHES:
        raise ValueError(f"terrain cell requires between 1 and {MAX_TERRAIN_MESHES} meshes")

    bounds_min = [math.inf, math.inf, math.inf]
    bounds_max = [-math.inf, -math.inf, -math.inf]
    manifest_meshes = []
    destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="terrain-cell-", dir=destination) as temporary_name:
        temporary = pathlib.Path(temporary_name)
        for index, mesh in enumerate(meshes):
            if not isinstance(mesh, dict):
                raise ValueError(f"terrain mesh {index} must be an object")
            vertices = mesh.get("vertices")
            if not isinstance(vertices, list):
                raise ValueError(f"terrain mesh {index} requires vertices")
            for vertex_index, vertex in enumerate(vertices):
                if not isinstance(vertex, dict):
                    raise ValueError(f"terrain mesh {index} vertex {vertex_index} must be an object")
                position = vertex.get("position")
                if not isinstance(position, list) or len(position) != 3:
                    raise ValueError(f"terrain mesh {index} vertex {vertex_index} requires a position")
                parsed = [float(component) for component in position]
                if not all(math.isfinite(component) for component in parsed):
                    raise ValueError(f"terrain mesh {index} vertex {vertex_index} contains a non-finite position")
                for axis in range(3):
                    bounds_min[axis] = min(bounds_min[axis], parsed[axis])
                    bounds_max[axis] = max(bounds_max[axis], parsed[axis])

            mesh_document = {"version": 1, **mesh}
            mesh_name = f"barnyard_terrain_{index}.pmes"
            summary = convert_document(mesh_document, temporary / mesh_name)
            manifest_meshes.append(
                {
                    "mesh": mesh_name,
                    "texture": f"barnyard_terrain_{index}.ptex",
                    "material": summary["material"],
                    "source_texture": summary["texture"],
                    "vertices": summary["vertices"],
                    "indices": summary["indices"],
                    "bytes": summary["bytes"],
                }
            )

        largest_extent = max(bounds_max[axis] - bounds_min[axis] for axis in range(3))
        if not math.isfinite(largest_extent) or largest_extent <= 1.0e-6:
            raise ValueError("terrain cell has degenerate combined bounds")
        manifest = {
            "version": 1,
            "world": int(document.get("world", 0)),
            "cell": int(document.get("cell", 0)),
            "bounds_min": bounds_min,
            "bounds_max": bounds_max,
            "center": [(bounds_min[axis] + bounds_max[axis]) * 0.5 for axis in range(3)],
            "scale": 2.0 / largest_extent,
            "meshes": manifest_meshes,
        }
        manifest_path = temporary / "barnyard_terrain.json"
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        for path in temporary.iterdir():
            os.replace(path, destination / path.name)

    for index in range(len(meshes), MAX_TERRAIN_MESHES):
        stale = destination / f"barnyard_terrain_{index}.pmes"
        if stale.exists():
            stale.unlink()
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert one bounded retail terrain cell to PSP meshes")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()
    print(json.dumps(convert(args.source, args.destination), sort_keys=True))


if __name__ == "__main__":
    main()
