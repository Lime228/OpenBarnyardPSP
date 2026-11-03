#!/usr/bin/env python3

import argparse
import json
import pathlib
import struct

from convert_skeleton import read_bones


PMES_MAGIC = 0x53454D50
PMES_VERSION = 1
PMES_FORMAT_COLOUR_XYZ = 0
PMES_FORMAT_TEXTURE_COLOUR_XYZ = 1
PMES_FORMAT_SKIN_TEXTURE_COLOUR_XYZ = 2
PMES_COLOUR_VERTEX_SIZE = 16
PMES_TEXTURE_VERTEX_SIZE = 24
PMES_SKIN_TEXTURE_VERTEX_SIZE = 32
MAX_ELEMENT_COUNT = 0xFFFF

COLOURS = (
    0xFF5AC8FA,
    0xFF65D26E,
    0xFFF0CF65,
    0xFFB06CE3,
    0xFFEB7D5B,
    0xFF63C7C5,
)


def parse_index(value: str, element_count: int, name: str) -> int:
    if not value:
        raise ValueError(f"face vertex is missing a {name} index")
    index = int(value)
    if index == 0:
        raise ValueError("OBJ indices are one-based")
    if index < 0:
        index = element_count + index
    else:
        index -= 1
    if index < 0 or index >= element_count:
        raise ValueError(f"face {name} index is out of range")
    return index


def parse_vertex_reference(token: str, position_count: int, texture_count: int) -> tuple[int, int | None]:
    fields = token.split("/")
    position = parse_index(fields[0], position_count, "position")
    texture = None
    if len(fields) > 1 and fields[1]:
        texture = parse_index(fields[1], texture_count, "texture")
    return position, texture


def read_obj(path: pathlib.Path) -> tuple[list[tuple[float, ...]], list[int], bool, list[int]]:
    positions: list[tuple[float, float, float]] = []
    textures: list[tuple[float, float]] = []
    faces: list[tuple[int, list[str]]] = []

    for line_number, source_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = source_line.split("#", 1)[0].strip()
        if not line:
            continue

        fields = line.split()
        if fields[0] == "v":
            if len(fields) < 4:
                raise ValueError(f"line {line_number}: vertex requires x, y, and z")
            positions.append((float(fields[1]), float(fields[2]), float(fields[3])))
        elif fields[0] == "vt":
            if len(fields) < 3:
                raise ValueError(f"line {line_number}: texture coordinate requires u and v")
            textures.append((float(fields[1]), 1.0 - float(fields[2])))
        elif fields[0] == "f":
            if len(fields) < 4:
                raise ValueError(f"line {line_number}: face requires at least three vertices")
            faces.append((line_number, fields[1:]))

    if not positions or not faces:
        raise ValueError("OBJ must contain vertices and at least one face")

    references = [
        [parse_vertex_reference(token, len(positions), len(textures)) for token in tokens]
        for _, tokens in faces
    ]
    has_texture = any(texture is not None for face in references for _, texture in face)
    if has_texture and any(texture is None for face in references for _, texture in face):
        raise ValueError("OBJ cannot mix textured and untextured face vertices")

    indices: list[int] = []
    vertex_positions: list[int] = []
    if has_texture:
        vertices: list[tuple[float, ...]] = []
        vertex_lookup: dict[tuple[int, int | None], int] = {}
        for face in references:
            converted_face: list[int] = []
            for reference in face:
                if reference not in vertex_lookup:
                    position_index, texture_index = reference
                    assert texture_index is not None
                    x, y, z = positions[position_index]
                    u, v = textures[texture_index]
                    vertex_lookup[reference] = len(vertices)
                    vertices.append((u, v, x, y, z))
                    vertex_positions.append(position_index)
                converted_face.append(vertex_lookup[reference])
            for offset in range(1, len(converted_face) - 1):
                indices.extend((converted_face[0], converted_face[offset], converted_face[offset + 1]))
    else:
        vertices = list(positions)
        vertex_positions = list(range(len(positions)))
        for face in references:
            converted_face = [position for position, _ in face]
            for offset in range(1, len(converted_face) - 1):
                indices.extend((converted_face[0], converted_face[offset], converted_face[offset + 1]))

    if len(vertices) > MAX_ELEMENT_COUNT or len(indices) > MAX_ELEMENT_COUNT:
        raise ValueError("PSP mesh exceeds the 16-bit vertex/index limit")
    return vertices, indices, has_texture, vertex_positions


def read_skin(
    skeleton_path: pathlib.Path,
    weights_path: pathlib.Path,
    vertex_positions: list[int],
) -> list[tuple[int, int, int, int]]:
    bone_names = [bone[0] for bone in read_bones(skeleton_path)]
    bone_indices = {name.casefold(): index for index, name in enumerate(bone_names)}
    document = json.loads(weights_path.read_text(encoding="utf-8"))
    if not isinstance(document, dict) or not isinstance(document.get("weights"), dict):
        raise ValueError("skin file requires a weights object")
    source_weights = document["weights"]
    default = source_weights.get("default")

    result: list[tuple[int, int, int, int]] = []
    for position in vertex_positions:
        influences = source_weights.get(str(position + 1), default)
        if not isinstance(influences, list) or not 1 <= len(influences) <= 2:
            raise ValueError(f"OBJ position {position + 1} requires one or two influences")
        parsed: list[tuple[int, float]] = []
        for influence in influences:
            if not isinstance(influence, list) or len(influence) != 2 or not isinstance(influence[0], str):
                raise ValueError(f"invalid influence for OBJ position {position + 1}")
            bone = bone_indices.get(influence[0].casefold())
            weight = float(influence[1])
            if bone is None or weight <= 0.0:
                raise ValueError(f"invalid bone or weight for OBJ position {position + 1}")
            parsed.append((bone, weight))
        total = sum(weight for _, weight in parsed)
        if total <= 0.0:
            raise ValueError(f"zero total weight for OBJ position {position + 1}")
        first_weight = round(parsed[0][1] * 255.0 / total) if len(parsed) == 2 else 255
        first_weight = max(0, min(255, first_weight))
        second_bone = parsed[1][0] if len(parsed) == 2 else 0
        result.append((parsed[0][0], second_bone, first_weight, 255 - first_weight))
    return result


def convert(
    source: pathlib.Path,
    destination: pathlib.Path,
    skeleton_path: pathlib.Path | None = None,
    weights_path: pathlib.Path | None = None,
) -> None:
    vertices, indices, has_texture, vertex_positions = read_obj(source)
    has_skin = skeleton_path is not None or weights_path is not None
    if has_skin and (skeleton_path is None or weights_path is None or not has_texture):
        raise ValueError("skinned conversion requires a textured OBJ, skeleton, and weights")
    skin = read_skin(skeleton_path, weights_path, vertex_positions) if has_skin else []
    vertex_size = PMES_SKIN_TEXTURE_VERTEX_SIZE if has_skin else PMES_TEXTURE_VERTEX_SIZE if has_texture else PMES_COLOUR_VERTEX_SIZE
    mesh_format = PMES_FORMAT_SKIN_TEXTURE_COLOUR_XYZ if has_skin else PMES_FORMAT_TEXTURE_COLOUR_XYZ if has_texture else PMES_FORMAT_COLOUR_XYZ
    header = struct.pack(
        "<6I",
        PMES_MAGIC,
        PMES_VERSION,
        len(vertices),
        len(indices),
        vertex_size,
        mesh_format,
    )

    payload = bytearray(header)
    for index, vertex in enumerate(vertices):
        colour = COLOURS[index % len(COLOURS)]
        if has_texture:
            u, v, x, y, z = vertex
            if has_skin:
                bone0, bone1, weight0, weight1 = skin[index]
                payload.extend(struct.pack("<ffIfff4BI", u, v, colour, x, y, z, bone0, bone1, weight0, weight1, 0))
            else:
                payload.extend(struct.pack("<ffIfff", u, v, colour, x, y, z))
        else:
            x, y, z = vertex
            payload.extend(struct.pack("<Ifff", colour, x, y, z))
    payload.extend(struct.pack(f"<{len(indices)}H", *indices))

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(payload)


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert a static OBJ mesh to OpenBarnyard PSP PMES")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--skeleton", type=pathlib.Path)
    parser.add_argument("--weights", type=pathlib.Path)
    args = parser.parse_args()
    convert(args.source, args.destination, args.skeleton, args.weights)


if __name__ == "__main__":
    main()
