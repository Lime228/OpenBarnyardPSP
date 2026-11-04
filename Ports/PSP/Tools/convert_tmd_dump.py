#!/usr/bin/env python3

import argparse
import json
import math
import pathlib
import struct


PMES_MAGIC = 0x53454D50
PMES_VERSION = 1
PMES_FORMAT_SKIN_TEXTURE_COLOUR_XYZ = 2
PMES_SKIN_TEXTURE_VERTEX_SIZE = 32
PSKL_MAGIC = 0x4C4B5350
PSKL_VERSION = 1
PSKL_BONE_SIZE = 64
MAX_BONE_COUNT = 64
MAX_ELEMENT_COUNT = 0xFFFF
MAX_ANIMATED_VERTEX_COUNT = 8192
MAX_NAME_LENGTH = 31


def vector(value: object, size: int, name: str) -> list[float]:
    if not isinstance(value, list) or len(value) != size:
        raise ValueError(f"{name} must contain {size} values")
    result = [float(component) for component in value]
    if not all(math.isfinite(component) for component in result):
        raise ValueError(f"{name} contains a non-finite value")
    return result


def build_skeleton(document: dict[str, object]) -> bytes:
    values = document.get("skeleton")
    if not isinstance(values, list) or not 1 <= len(values) <= MAX_BONE_COUNT:
        raise ValueError("skeleton must contain between 1 and 64 bones")

    names: dict[str, int] = {}
    bones: list[tuple[str, int, list[float], list[float]]] = []
    for index, value in enumerate(values):
        if not isinstance(value, dict) or not isinstance(value.get("name"), str):
            raise ValueError(f"bone {index} requires a name")
        name = value["name"]
        encoded_name = name.encode("ascii")
        if not encoded_name or len(encoded_name) > MAX_NAME_LENGTH or name.casefold() in names:
            raise ValueError(f"invalid or duplicate bone name: {name}")
        parent_value = value.get("parent")
        if index == 0:
            if parent_value is not None:
                raise ValueError("first bone must have a null parent")
            parent = -1
        elif not isinstance(parent_value, int) or parent_value < 0 or parent_value >= index:
            raise ValueError(f"bone {name} parent must be an earlier index")
        else:
            parent = parent_value
        rotation = vector(value.get("rotation", [0.0, 0.0, 0.0, 1.0]), 4, f"bone {name} rotation")
        length = math.sqrt(sum(component * component for component in rotation))
        if length <= 0.000001:
            raise ValueError(f"bone {name} rotation is zero")
        rotation = [component / length for component in rotation]
        translation = vector(value.get("translation", [0.0, 0.0, 0.0]), 3, f"bone {name} translation")
        names[name.casefold()] = index
        bones.append((name, parent, rotation, translation))

    payload = bytearray(struct.pack("<4I", PSKL_MAGIC, PSKL_VERSION, len(bones), PSKL_BONE_SIZE))
    for name, parent, rotation, translation in bones:
        encoded_name = name.encode("ascii")
        payload.extend(struct.pack("<hBB32s4f3f", parent, len(encoded_name), 0, encoded_name, *rotation, *translation))
    return bytes(payload)


def reduce_weights(
    palette: list[int], local_bones: object, source_weights: object, bone_count: int
) -> tuple[int, int, int, int]:
    if not isinstance(local_bones, list) or not isinstance(source_weights, list) or len(local_bones) != 4 or len(source_weights) != 4:
        raise ValueError("each vertex requires four bone slots and four weights")
    combined: dict[int, int] = {}
    for local_bone, weight in zip(local_bones, source_weights):
        if not isinstance(local_bone, int) or not isinstance(weight, int) or weight < 0 or weight > 255:
            raise ValueError("bone slots and weights must be byte values")
        if weight == 0:
            continue
        if local_bone < 0 or local_bone >= len(palette):
            raise ValueError("vertex bone slot is outside the submesh palette")
        bone = palette[local_bone]
        if bone < 0 or bone >= bone_count:
            raise ValueError("palette bone is outside the skeleton")
        combined[bone] = combined.get(bone, 0) + weight
    if not combined:
        raise ValueError("vertex has no non-zero bone weight")

    strongest = sorted(combined.items(), key=lambda item: (-item[1], item[0]))[:2]
    if len(strongest) == 1:
        return strongest[0][0], 0, 255, 0
    total = strongest[0][1] + strongest[1][1]
    weight0 = max(0, min(255, (strongest[0][1] * 255 + total // 2) // total))
    return strongest[0][0], strongest[1][0], weight0, 255 - weight0


def triangulate_strip(indices: object, vertex_count: int) -> list[int]:
    if not isinstance(indices, list) or len(indices) < 3:
        raise ValueError("triangle strip requires at least three indices")
    parsed = []
    for index in indices:
        if not isinstance(index, int) or index < 0 or index >= vertex_count:
            raise ValueError("triangle strip index is out of range")
        parsed.append(index)

    triangles: list[int] = []
    for offset in range(len(parsed) - 2):
        triangle = (
            (parsed[offset + 1], parsed[offset], parsed[offset + 2])
            if offset % 2
            else (parsed[offset], parsed[offset + 1], parsed[offset + 2])
        )
        if len(set(triangle)) == 3:
            triangles.extend(triangle)
    if not triangles:
        raise ValueError("triangle strip contains only degenerate triangles")
    return triangles


def get_material_name(document: dict[str, object], requested_material: str | None = None) -> str:
    lods = document.get("lods")
    if not isinstance(lods, list) or not lods or not isinstance(lods[0], dict):
        raise ValueError("dump requires LOD0")
    meshes = lods[0].get("meshes")
    if not isinstance(meshes, list) or not meshes:
        raise ValueError("LOD0 requires meshes")

    material_names: dict[str, str] = {}
    for mesh_index, mesh in enumerate(meshes):
        if not isinstance(mesh, dict):
            raise ValueError(f"mesh {mesh_index} must be an object")
        material = mesh.get("material")
        if not isinstance(material, str) or not material:
            raise ValueError(f"mesh {mesh_index} requires a material name")
        try:
            material.encode("ascii")
        except UnicodeEncodeError as error:
            raise ValueError(f"mesh {mesh_index} material name must be ASCII") from error
        material_names.setdefault(material.casefold(), material)
    if not material_names:
        raise ValueError("LOD0 requires meshes")
    if requested_material is not None:
        selected = material_names.get(requested_material.casefold())
        if selected is None:
            available = ", ".join(sorted(material_names.values(), key=str.casefold))
            raise ValueError(f"material {requested_material!r} was not found; available: {available}")
        return selected
    if len(material_names) != 1:
        available = ", ".join(sorted(material_names.values(), key=str.casefold))
        raise ValueError(f"PSP PMES preview supports one material; found: {available}")
    return next(iter(material_names.values()))


def build_mesh(
    document: dict[str, object],
    bone_count: int,
    requested_material: str | None = None,
) -> bytes:
    lods = document.get("lods")
    if not isinstance(lods, list) or not lods or not isinstance(lods[0], dict):
        raise ValueError("dump requires LOD0")
    lod = lods[0]
    if lod.get("shader") != "skin" or not isinstance(lod.get("meshes"), list):
        raise ValueError("LOD0 must use the skin shader and contain meshes")
    material_name = get_material_name(document, requested_material)

    vertices: list[tuple[list[float], list[float], int, tuple[int, int, int, int]]] = []
    indices: list[int] = []
    for mesh_index, mesh in enumerate(lod["meshes"]):
        if not isinstance(mesh, dict) or not isinstance(mesh.get("submeshes"), list):
            raise ValueError(f"mesh {mesh_index} requires submeshes")
        if mesh["material"].casefold() != material_name.casefold():
            continue
        for submesh_index, submesh in enumerate(mesh["submeshes"]):
            if not isinstance(submesh, dict) or not isinstance(submesh.get("bone_palette"), list) or not isinstance(submesh.get("vertices"), list):
                raise ValueError(f"mesh {mesh_index} submesh {submesh_index} is incomplete")
            palette = submesh["bone_palette"]
            if not palette or not all(isinstance(bone, int) for bone in palette):
                raise ValueError("bone palette must contain integer skeleton indices")
            source_vertices = submesh["vertices"]
            base_vertex = len(vertices)
            for vertex_index, vertex in enumerate(source_vertices):
                if len(vertices) >= MAX_ANIMATED_VERTEX_COUNT:
                    raise ValueError(
                        f"converted TMD LOD exceeds the PSP {MAX_ANIMATED_VERTEX_COUNT}-vertex skinning budget"
                    )
                if not isinstance(vertex, dict):
                    raise ValueError(f"vertex {vertex_index} must be an object")
                position = vector(vertex.get("position"), 3, f"vertex {vertex_index} position")
                uv = vector(vertex.get("uv"), 2, f"vertex {vertex_index} uv")
                colour = vertex.get("colour", 0xFFFFFFFF)
                if not isinstance(colour, int) or colour < 0 or colour > 0xFFFFFFFF:
                    raise ValueError("vertex colour must be an unsigned 32-bit integer")
                skin = reduce_weights(palette, vertex.get("bones"), vertex.get("weights"), bone_count)
                vertices.append((position, uv, colour, skin))
            local_indices = triangulate_strip(submesh.get("triangle_strip"), len(source_vertices))
            indices.extend(base_vertex + index for index in local_indices)

    if not vertices or len(indices) > MAX_ELEMENT_COUNT:
        raise ValueError("converted TMD LOD exceeds PSP 16-bit element limits")
    payload = bytearray(
        struct.pack(
            "<6I",
            PMES_MAGIC,
            PMES_VERSION,
            len(vertices),
            len(indices),
            PMES_SKIN_TEXTURE_VERTEX_SIZE,
            PMES_FORMAT_SKIN_TEXTURE_COLOUR_XYZ,
        )
    )
    for position, uv, colour, skin in vertices:
        bone0, bone1, weight0, weight1 = skin
        payload.extend(struct.pack("<ffIfff4BI", uv[0], uv[1], colour, *position, bone0, bone1, weight0, weight1, 0))
    payload.extend(struct.pack(f"<{len(indices)}H", *indices))
    return bytes(payload)


def convert(
    source: pathlib.Path,
    mesh_destination: pathlib.Path,
    skeleton_destination: pathlib.Path,
    requested_material: str | None = None,
) -> None:
    document = json.loads(source.read_text(encoding="utf-8"))
    if not isinstance(document, dict) or document.get("version") != 1:
        raise ValueError("unsupported TMD dump contract version")
    skeleton = build_skeleton(document)
    bone_count = struct.unpack_from("<I", skeleton, 8)[0]
    mesh = build_mesh(document, bone_count, requested_material)
    mesh_destination.parent.mkdir(parents=True, exist_ok=True)
    skeleton_destination.parent.mkdir(parents=True, exist_ok=True)
    mesh_destination.write_bytes(mesh)
    skeleton_destination.write_bytes(skeleton)


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert a host TMDL JSON dump to bounded PSP assets")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("mesh", type=pathlib.Path)
    parser.add_argument("skeleton", type=pathlib.Path)
    parser.add_argument("--material", help="select one material from a multi-material LOD0")
    args = parser.parse_args()
    convert(args.source, args.mesh, args.skeleton, args.material)


if __name__ == "__main__":
    main()
