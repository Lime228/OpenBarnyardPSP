#!/usr/bin/env python3

import argparse
import pathlib
import struct


PMES_MAGIC = 0x53454D50
PMES_VERSION = 1
PMES_FORMAT_COLOUR_XYZ = 0
PMES_FORMAT_TEXTURE_COLOUR_XYZ = 1
PMES_COLOUR_VERTEX_SIZE = 16
PMES_TEXTURE_VERTEX_SIZE = 24
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


def read_obj(path: pathlib.Path) -> tuple[list[tuple[float, ...]], list[int], bool]:
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
                converted_face.append(vertex_lookup[reference])
            for offset in range(1, len(converted_face) - 1):
                indices.extend((converted_face[0], converted_face[offset], converted_face[offset + 1]))
    else:
        vertices = list(positions)
        for face in references:
            converted_face = [position for position, _ in face]
            for offset in range(1, len(converted_face) - 1):
                indices.extend((converted_face[0], converted_face[offset], converted_face[offset + 1]))

    if len(vertices) > MAX_ELEMENT_COUNT or len(indices) > MAX_ELEMENT_COUNT:
        raise ValueError("PSP mesh exceeds the 16-bit vertex/index limit")
    return vertices, indices, has_texture


def convert(source: pathlib.Path, destination: pathlib.Path) -> None:
    vertices, indices, has_texture = read_obj(source)
    vertex_size = PMES_TEXTURE_VERTEX_SIZE if has_texture else PMES_COLOUR_VERTEX_SIZE
    mesh_format = PMES_FORMAT_TEXTURE_COLOUR_XYZ if has_texture else PMES_FORMAT_COLOUR_XYZ
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
    args = parser.parse_args()
    convert(args.source, args.destination)


if __name__ == "__main__":
    main()
