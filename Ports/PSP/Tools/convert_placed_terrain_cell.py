#!/usr/bin/env python3

import argparse
import copy
import json
import math
import os
import pathlib
import tempfile

from convert_camera import multiply, quaternion_matrix, read_vector, transform_basis
from convert_terrain_cell import MAX_TERRAIN_MESHES
from convert_terrain_dump import convert_document


GLOBAL_LOCATOR_TRANSFORM = [
    [1.0, 0.0, 0.0, 0.0],
    [0.0, 0.0, 1.0, 0.0],
    [0.0, -1.0, 0.0, 0.0],
    [0.0, 0.0, 0.0, 1.0],
]


def validate_prefix(value: str) -> str:
    if not value or any(not (character.isascii() and (character.isalnum() or character == "_")) for character in value):
        raise ValueError("prefix must contain only ASCII letters, digits, and underscores")
    return value


def load_locator(source: pathlib.Path, expected_name: str) -> tuple[list[list[float]], dict[str, object]]:
    document = json.loads(source.read_text(encoding="utf-8"))
    if not isinstance(document, dict) or document.get("version") != 1:
        raise ValueError("unsupported locator contract version")
    name = document.get("name")
    if not isinstance(name, str) or name.casefold() != expected_name.casefold():
        raise ValueError(f"locator must be named {expected_name}")
    rotation = read_vector(document.get("rotation"), 4, "locator rotation")
    position = read_vector(document.get("position"), 3, "locator position")
    scale = document.get("scale")
    if not isinstance(scale, (int, float)) or not math.isfinite(float(scale)) or float(scale) < 0.0:
        raise ValueError("locator scale must be finite and non-negative")

    # ATerrainLocatorList applies this fixed axis conversion before the local
    # locator matrix. ABarnSign uses the resulting matrix directly and does
    # not apply ATerrainLocator::GetScale(). Bake that exact retail transform
    # into the PSP vertices so the runtime needs no duplicate locator parser.
    locator_transform = quaternion_matrix(rotation, position)
    return multiply(GLOBAL_LOCATOR_TRANSFORM, locator_transform), document


def transform_position(matrix: list[list[float]], value: object, name: str) -> list[float]:
    position = read_vector(value, 3, name)
    transformed = transform_basis(matrix, [*position, 1.0])
    if not all(math.isfinite(component) for component in transformed[:3]):
        raise ValueError(f"{name} produced a non-finite position")
    return transformed[:3]


def convert(
    source: pathlib.Path,
    locator_source: pathlib.Path,
    destination: pathlib.Path,
    prefix: str,
    locator_name: str,
) -> dict[str, object]:
    prefix = validate_prefix(prefix)
    document = json.loads(source.read_text(encoding="utf-8"))
    if not isinstance(document, dict) or document.get("version") != 2:
        raise ValueError("unsupported terrain cell contract version")
    meshes = document.get("meshes")
    if not isinstance(meshes, list) or not 1 <= len(meshes) <= MAX_TERRAIN_MESHES:
        raise ValueError(f"placed terrain cell requires between 1 and {MAX_TERRAIN_MESHES} meshes")

    locator_transform, locator = load_locator(locator_source, locator_name)
    bounds_min = [math.inf, math.inf, math.inf]
    bounds_max = [-math.inf, -math.inf, -math.inf]
    manifest_meshes = []
    destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=f"{prefix}-", dir=destination) as temporary_name:
        temporary = pathlib.Path(temporary_name)
        for mesh_index, source_mesh in enumerate(meshes):
            if not isinstance(source_mesh, dict):
                raise ValueError(f"placed mesh {mesh_index} must be an object")
            mesh = copy.deepcopy(source_mesh)
            vertices = mesh.get("vertices")
            if not isinstance(vertices, list):
                raise ValueError(f"placed mesh {mesh_index} requires vertices")
            for vertex_index, vertex in enumerate(vertices):
                if not isinstance(vertex, dict):
                    raise ValueError(f"placed mesh {mesh_index} vertex {vertex_index} must be an object")
                position = transform_position(
                    locator_transform,
                    vertex.get("position"),
                    f"placed mesh {mesh_index} vertex {vertex_index}",
                )
                vertex["position"] = position
                for axis in range(3):
                    bounds_min[axis] = min(bounds_min[axis], position[axis])
                    bounds_max[axis] = max(bounds_max[axis], position[axis])

            mesh_name = f"{prefix}_{mesh_index}.pmes"
            texture_name = f"{prefix}_{mesh_index}.ptex"
            summary = convert_document({"version": 1, **mesh}, temporary / mesh_name)
            manifest_meshes.append(
                {
                    "mesh": mesh_name,
                    "texture": texture_name,
                    "material": summary["material"],
                    "source_texture": summary["texture"],
                    "vertices": summary["vertices"],
                    "indices": summary["indices"],
                    "bytes": summary["bytes"],
                }
            )

        manifest = {
            "version": 1,
            "locator": locator["name"],
            "locator_position": locator["position"],
            "locator_rotation": locator["rotation"],
            "bounds_min": bounds_min,
            "bounds_max": bounds_max,
            "meshes": manifest_meshes,
        }
        manifest_path = temporary / f"{prefix}.json"
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        for path in temporary.iterdir():
            os.replace(path, destination / path.name)

    for index in range(len(meshes), MAX_TERRAIN_MESHES):
        stale = destination / f"{prefix}_{index}.pmes"
        if stale.exists():
            stale.unlink()
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description="Bake a retail terrain-model locator into bounded PSP meshes")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("locator", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--prefix", default="frontend_barnsign")
    parser.add_argument("--locator-name", default="loc_barnsign")
    args = parser.parse_args()
    print(json.dumps(convert(args.source, args.locator, args.destination, args.prefix, args.locator_name), sort_keys=True))


if __name__ == "__main__":
    main()
