#!/usr/bin/env python3

import argparse
import json
import math
import os
import pathlib
import tempfile

from convert_camera import quaternion_matrix, read_vector, transform_basis
from convert_frontend_terrain_pack import (
    MAX_PACK_MODELS,
    MAX_PACK_TEXTURES,
    write_model,
)
from convert_mesh import MAX_ELEMENT_COUNT
from convert_terrain_dump import MAX_TERRAIN_VERTEX_COUNT, vector
from convert_tmd_dump import triangulate_strip


def validate_prefix(value: str) -> str:
    if not value or any(not (character.isascii() and (character.isalnum() or character == "_")) for character in value):
        raise ValueError("instance pack prefix must contain only ASCII letters, digits and underscores")
    return value


def load_source_meshes(document: object) -> list[dict[str, object]]:
    if not isinstance(document, dict) or document.get("version") != 1:
        raise ValueError("unsupported TMD model dump contract version")
    lods = document.get("lods")
    if not isinstance(lods, list) or not lods or not isinstance(lods[0], dict):
        raise ValueError("TMD model dump requires LOD0")
    meshes = lods[0].get("meshes")
    if lods[0].get("shader") != "skin" or not isinstance(meshes, list) or not meshes:
        raise ValueError("TMD model LOD0 must contain skin meshes")

    parsed_meshes = []
    for mesh_index, mesh in enumerate(meshes):
        if not isinstance(mesh, dict):
            raise ValueError(f"TMD model mesh {mesh_index} must be an object")
        material = mesh.get("material")
        texture = mesh.get("texture")
        submeshes = mesh.get("submeshes")
        if not isinstance(material, str) or not material:
            raise ValueError(f"TMD model mesh {mesh_index} requires a material")
        if not isinstance(texture, str) or not texture:
            raise ValueError(f"TMD model mesh {mesh_index} requires a resolved texture")
        if not isinstance(submeshes, list) or not submeshes:
            raise ValueError(f"TMD model mesh {mesh_index} requires submeshes")
        parsed_submeshes = []
        for submesh_index, submesh in enumerate(submeshes):
            if not isinstance(submesh, dict) or not isinstance(submesh.get("vertices"), list):
                raise ValueError(f"TMD model mesh {mesh_index} submesh {submesh_index} is invalid")
            source_vertices = submesh["vertices"]
            if not 3 <= len(source_vertices) <= MAX_TERRAIN_VERTEX_COUNT:
                raise ValueError(f"TMD model mesh {mesh_index} submesh {submesh_index} has an invalid vertex count")
            vertices = []
            for vertex_index, vertex in enumerate(source_vertices):
                if not isinstance(vertex, dict):
                    raise ValueError(f"TMD model mesh {mesh_index} vertex {vertex_index} must be an object")
                vertices.append(
                    (
                        vector(vertex.get("uv"), 2, f"TMD model mesh {mesh_index} vertex {vertex_index} uv"),
                        vector(
                            vertex.get("position"),
                            3,
                            f"TMD model mesh {mesh_index} vertex {vertex_index} position",
                        ),
                        0xFFFFFFFF,
                    )
                )
            indices = triangulate_strip(submesh.get("triangle_strip"), len(vertices))
            parsed_submeshes.append({"vertices": vertices, "indices": indices})
        parsed_meshes.append(
            {
                "source_index": mesh_index,
                "material": material,
                "texture": texture,
                "submeshes": parsed_submeshes,
            }
        )
    return parsed_meshes


def transform_position(matrix: list[list[float]], scale: float, position: list[float]) -> list[float]:
    transformed = transform_basis(matrix, [component * scale for component in position] + [1.0])
    if not all(math.isfinite(component) for component in transformed[:3]):
        raise ValueError("instance transform produced a non-finite position")
    return transformed[:3]


def convert(
    inventory_path: pathlib.Path,
    model_dump_path: pathlib.Path,
    destination: pathlib.Path,
    model_name: str,
    prefix: str,
) -> dict[str, object]:
    prefix = validate_prefix(prefix)
    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    if not isinstance(inventory, dict) or inventory.get("version") != 1:
        raise ValueError("unsupported frontend instance inventory contract version")
    resident_sections = inventory.get("resident_section_names")
    instances = inventory.get("instances")
    if not isinstance(resident_sections, list) or not all(isinstance(name, str) for name in resident_sections):
        raise ValueError("frontend instance inventory requires resident section names")
    if not isinstance(instances, list):
        raise ValueError("frontend instance inventory requires instances")
    selected_instances = [
        instance
        for instance in instances
        if isinstance(instance, dict)
        and str(instance.get("model_name", "")).casefold() == model_name.casefold()
        and instance.get("vis_section_name") in resident_sections
    ]
    if not selected_instances:
        raise ValueError(f"resident frontend has no instances of model {model_name!r}")
    model_indices = {instance.get("model_index") for instance in selected_instances}
    model_paths = {instance.get("model_path") for instance in selected_instances}
    if len(model_indices) != 1 or len(model_paths) != 1:
        raise ValueError("selected frontend instances disagree on model identity")

    source_meshes = load_source_meshes(json.loads(model_dump_path.read_text(encoding="utf-8")))
    groups: list[dict[str, object]] = []
    groups_by_binding: dict[tuple[str, str], list[dict[str, object]]] = {}
    textures = []
    texture_indices: dict[str, int] = {}
    bounds_min = [math.inf, math.inf, math.inf]
    bounds_max = [-math.inf, -math.inf, -math.inf]

    for instance in selected_instances:
        instance_index = instance.get("index")
        if type(instance_index) is not int or instance_index < 0:
            raise ValueError("frontend instance requires a non-negative source index")
        rotation = read_vector(instance.get("rotation"), 4, f"frontend instance {instance_index} rotation")
        position = read_vector(instance.get("position"), 3, f"frontend instance {instance_index} position")
        scale_value = instance.get("scale")
        if not isinstance(scale_value, (int, float)) or not math.isfinite(float(scale_value)) or float(scale_value) <= 0.0:
            raise ValueError(f"frontend instance {instance_index} requires a positive finite scale")
        scale = float(scale_value)
        matrix = quaternion_matrix(rotation, position)

        for source_mesh in source_meshes:
            texture_key = str(source_mesh["texture"]).casefold()
            if texture_key not in texture_indices:
                if len(textures) >= MAX_PACK_TEXTURES:
                    raise ValueError(f"frontend instance pack exceeds {MAX_PACK_TEXTURES} unique textures")
                texture_indices[texture_key] = len(textures)
                textures.append(
                    {
                        "texture": f"{prefix}_texture_{len(textures)}.ptex",
                        "source_texture": source_mesh["texture"],
                    }
                )

            binding = (str(source_mesh["material"]).casefold(), texture_key)
            candidates = groups_by_binding.setdefault(binding, [])
            for submesh in source_mesh["submeshes"]:
                transformed_vertices = []
                for uv, source_position, colour in submesh["vertices"]:
                    transformed_position = transform_position(matrix, scale, source_position)
                    for axis in range(3):
                        bounds_min[axis] = min(bounds_min[axis], transformed_position[axis])
                        bounds_max[axis] = max(bounds_max[axis], transformed_position[axis])
                    transformed_vertices.append((uv, transformed_position, colour))

                group = candidates[-1] if candidates else None
                if group is None or (
                    len(group["vertices"]) + len(transformed_vertices) > MAX_TERRAIN_VERTEX_COUNT
                    or len(group["indices"]) + len(submesh["indices"]) > MAX_ELEMENT_COUNT
                ):
                    if len(groups) >= MAX_PACK_MODELS:
                        raise ValueError(f"frontend instance pack exceeds {MAX_PACK_MODELS} bounded draw groups")
                    group = {
                        "material": source_mesh["material"],
                        "source_texture": source_mesh["texture"],
                        "texture_index": texture_indices[texture_key],
                        "vertices": [],
                        "indices": [],
                        "source_meshes": [],
                        "source_instances": [],
                    }
                    groups.append(group)
                    candidates.append(group)

                vertex_offset = len(group["vertices"])
                group["vertices"].extend(transformed_vertices)
                group["indices"].extend(index + vertex_offset for index in submesh["indices"])
                if source_mesh["source_index"] not in group["source_meshes"]:
                    group["source_meshes"].append(source_mesh["source_index"])
                if instance_index not in group["source_instances"]:
                    group["source_instances"].append(instance_index)

    destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=f"{prefix}-", dir=destination) as temporary_name:
        temporary = pathlib.Path(temporary_name)
        manifest_models = []
        for model_index, group in enumerate(groups):
            model_file_name = f"{prefix}_model_{model_index}.pmes"
            summary = write_model(group, temporary / model_file_name)
            manifest_models.append(
                {
                    "mesh": model_file_name,
                    "material": group["material"],
                    "source_texture": group["source_texture"],
                    "texture_index": group["texture_index"],
                    "source_meshes": group["source_meshes"],
                    "source_instances": group["source_instances"],
                    **summary,
                }
            )

        manifest = {
            "version": 1,
            "prefix": prefix,
            "model_index": next(iter(model_indices)),
            "model_name": model_name,
            "model_path": next(iter(model_paths)),
            "source_meshes": len(source_meshes),
            "source_instances": len(selected_instances),
            "resident_sections": sorted({str(instance["vis_section_name"]) for instance in selected_instances}),
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
    parser = argparse.ArgumentParser(description="Bake original frontend instances into bounded PSP draw groups")
    parser.add_argument("inventory", type=pathlib.Path)
    parser.add_argument("model_dump", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--model-name", required=True)
    parser.add_argument("--prefix", required=True)
    args = parser.parse_args()
    manifest = convert(
        args.inventory,
        args.model_dump,
        args.destination,
        args.model_name,
        args.prefix,
    )
    print(
        json.dumps(
            {
                "source_instances": manifest["source_instances"],
                "source_meshes": manifest["source_meshes"],
                "models": len(manifest["models"]),
                "textures": len(manifest["textures"]),
            },
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
