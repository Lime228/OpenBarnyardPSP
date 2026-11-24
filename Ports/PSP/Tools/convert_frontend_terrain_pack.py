#!/usr/bin/env python3

import argparse
import json
import math
import os
import pathlib
import struct
import tempfile

from convert_mesh import (
    MAX_ELEMENT_COUNT,
    PMES_FORMAT_TEXTURE_COLOUR_XYZ,
    PMES_MAGIC,
    PMES_TEXTURE_VERTEX_SIZE,
    PMES_VERSION,
)
from convert_terrain_dump import MAX_TERRAIN_VERTEX_COUNT, pack_colour, vector
from convert_tmd_dump import triangulate_strip


MAX_SOURCE_MESHES = 256
MAX_PACK_MODELS = 48
MAX_PACK_TEXTURES = 48


def parse_mesh(mesh: object, source_index: int) -> dict[str, object]:
    if not isinstance(mesh, dict):
        raise ValueError(f"terrain mesh {source_index} must be an object")
    material = mesh.get("material")
    texture = mesh.get("texture")
    source_vertices = mesh.get("vertices")
    if not isinstance(material, str) or not material:
        raise ValueError(f"terrain mesh {source_index} requires a material")
    if not isinstance(texture, str) or not texture:
        raise ValueError(f"terrain mesh {source_index} requires a texture")
    if not isinstance(source_vertices, list) or not 3 <= len(source_vertices) <= MAX_TERRAIN_VERTEX_COUNT:
        raise ValueError(f"terrain mesh {source_index} has an invalid vertex count")

    vertices = []
    for vertex_index, source_vertex in enumerate(source_vertices):
        if not isinstance(source_vertex, dict):
            raise ValueError(f"terrain mesh {source_index} vertex {vertex_index} must be an object")
        vertices.append(
            (
                vector(source_vertex.get("uv"), 2, f"terrain mesh {source_index} vertex {vertex_index} uv"),
                vector(source_vertex.get("position"), 3, f"terrain mesh {source_index} vertex {vertex_index} position"),
                pack_colour(vector(source_vertex.get("colour"), 3, f"terrain mesh {source_index} vertex {vertex_index} colour")),
            )
        )
    indices = triangulate_strip(mesh.get("triangle_strip"), len(vertices))
    return {"material": material, "texture": texture, "vertices": vertices, "indices": indices, "source_index": source_index}


def write_model(model: dict[str, object], destination: pathlib.Path) -> dict[str, int]:
    vertices = model["vertices"]
    indices = model["indices"]
    payload = bytearray(
        struct.pack(
            "<6I",
            PMES_MAGIC,
            PMES_VERSION,
            len(vertices),
            len(indices),
            PMES_TEXTURE_VERTEX_SIZE,
            PMES_FORMAT_TEXTURE_COLOUR_XYZ,
        )
    )
    for uv, position, colour in vertices:
        payload.extend(struct.pack("<ffIfff", uv[0], uv[1], colour, *position))
    payload.extend(struct.pack(f"<{len(indices)}H", *indices))
    destination.write_bytes(payload)
    return {"vertices": len(vertices), "indices": len(indices), "bytes": len(payload)}


def convert(source: pathlib.Path, destination: pathlib.Path, prefix: str = "frontend_detail") -> dict[str, object]:
    if not prefix or not prefix.replace("_", "").isalnum():
        raise ValueError("terrain pack prefix must contain only letters, numbers and underscores")
    document = json.loads(source.read_text(encoding="utf-8"))
    meshes = document.get("meshes") if isinstance(document, dict) and document.get("version") == 2 else None
    if not isinstance(meshes, list) or not 1 <= len(meshes) <= MAX_SOURCE_MESHES:
        raise ValueError(f"terrain pack requires between 1 and {MAX_SOURCE_MESHES} source meshes")

    groups: list[dict[str, object]] = []
    groups_by_material: dict[tuple[str, str], list[dict[str, object]]] = {}
    textures: list[dict[str, str]] = []
    texture_indices: dict[str, int] = {}
    bounds_min = [math.inf, math.inf, math.inf]
    bounds_max = [-math.inf, -math.inf, -math.inf]

    for source_index, source_mesh in enumerate(meshes):
        mesh = parse_mesh(source_mesh, source_index)
        for _, position, _ in mesh["vertices"]:
            for axis in range(3):
                bounds_min[axis] = min(bounds_min[axis], position[axis])
                bounds_max[axis] = max(bounds_max[axis], position[axis])

        texture_key = mesh["texture"].casefold()
        if texture_key not in texture_indices:
            if len(textures) >= MAX_PACK_TEXTURES:
                raise ValueError(f"terrain pack exceeds {MAX_PACK_TEXTURES} unique textures")
            texture_indices[texture_key] = len(textures)
            textures.append({"texture": f"{prefix}_texture_{len(textures)}.ptex", "source_texture": mesh["texture"]})

        group_key = (mesh["material"].casefold(), texture_key)
        candidates = groups_by_material.setdefault(group_key, [])
        group = candidates[-1] if candidates else None
        if group is None or (
            len(group["vertices"]) + len(mesh["vertices"]) > MAX_TERRAIN_VERTEX_COUNT
            or len(group["indices"]) + len(mesh["indices"]) > MAX_ELEMENT_COUNT
        ):
            if len(groups) >= MAX_PACK_MODELS:
                raise ValueError(f"terrain pack exceeds {MAX_PACK_MODELS} bounded material groups")
            group = {
                "material": mesh["material"],
                "source_texture": mesh["texture"],
                "texture_index": texture_indices[texture_key],
                "vertices": [],
                "indices": [],
                "source_meshes": [],
            }
            groups.append(group)
            candidates.append(group)

        vertex_offset = len(group["vertices"])
        group["vertices"].extend(mesh["vertices"])
        group["indices"].extend(index + vertex_offset for index in mesh["indices"])
        group["source_meshes"].append(source_index)

    destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=f"{prefix}-", dir=destination) as temporary_name:
        temporary = pathlib.Path(temporary_name)
        manifest_models = []
        for model_index, group in enumerate(groups):
            model_name = f"{prefix}_model_{model_index}.pmes"
            summary = write_model(group, temporary / model_name)
            manifest_models.append(
                {
                    "mesh": model_name,
                    "material": group["material"],
                    "source_texture": group["source_texture"],
                    "texture_index": group["texture_index"],
                    "source_meshes": group["source_meshes"],
                    **summary,
                }
            )

        manifest = {
            "version": 1,
            "prefix": prefix,
            "source_meshes": len(meshes),
            "bounds_min": bounds_min,
            "bounds_max": bounds_max,
            "models": manifest_models,
            "textures": textures,
        }
        (temporary / f"{prefix}.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        for path in temporary.iterdir():
            os.replace(path, destination / path.name)

    for index in range(len(groups), MAX_PACK_MODELS):
        stale = destination / f"{prefix}_model_{index}.pmes"
        if stale.exists():
            stale.unlink()
    for index in range(len(textures), MAX_PACK_TEXTURES):
        stale = destination / f"{prefix}_texture_{index}.ptex"
        if stale.exists():
            stale.unlink()
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description="Merge a complete original terrain cell into bounded PSP material groups")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--prefix", default="frontend_detail")
    args = parser.parse_args()
    manifest = convert(args.source, args.destination, args.prefix)
    print(json.dumps({"source_meshes": manifest["source_meshes"], "models": len(manifest["models"]), "textures": len(manifest["textures"])}, sort_keys=True))


if __name__ == "__main__":
    main()
