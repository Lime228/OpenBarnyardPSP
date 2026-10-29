#!/usr/bin/env python3

import argparse
import pathlib
import struct


PMES_MAGIC = 0x53454D50
PMES_VERSION = 1
PMES_FORMAT_COLOUR_XYZ = 0
PMES_VERTEX_SIZE = 16
MAX_ELEMENT_COUNT = 0xFFFF

COLOURS = (
    0xFF5AC8FA,
    0xFF65D26E,
    0xFFF0CF65,
    0xFFB06CE3,
    0xFFEB7D5B,
    0xFF63C7C5,
)


def parse_vertex_reference(token: str, vertex_count: int) -> int:
    position = token.split("/", 1)[0]
    if not position:
        raise ValueError("face vertex is missing a position index")

    index = int(position)
    if index == 0:
        raise ValueError("OBJ indices are one-based")
    if index < 0:
        index = vertex_count + index
    else:
        index -= 1
    if index < 0 or index >= vertex_count:
        raise ValueError("face position index is out of range")
    return index


def read_obj(path: pathlib.Path) -> tuple[list[tuple[float, float, float]], list[int]]:
    vertices: list[tuple[float, float, float]] = []
    indices: list[int] = []

    for line_number, source_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = source_line.split("#", 1)[0].strip()
        if not line:
            continue

        fields = line.split()
        if fields[0] == "v":
            if len(fields) < 4:
                raise ValueError(f"line {line_number}: vertex requires x, y, and z")
            vertices.append((float(fields[1]), float(fields[2]), float(fields[3])))
        elif fields[0] == "f":
            if len(fields) < 4:
                raise ValueError(f"line {line_number}: face requires at least three vertices")
            face = [parse_vertex_reference(token, len(vertices)) for token in fields[1:]]
            for offset in range(1, len(face) - 1):
                indices.extend((face[0], face[offset], face[offset + 1]))

    if not vertices or not indices:
        raise ValueError("OBJ must contain vertices and at least one face")
    if len(vertices) > MAX_ELEMENT_COUNT or len(indices) > MAX_ELEMENT_COUNT:
        raise ValueError("PSP mesh exceeds the 16-bit vertex/index limit")
    return vertices, indices


def convert(source: pathlib.Path, destination: pathlib.Path) -> None:
    vertices, indices = read_obj(source)
    header = struct.pack(
        "<6I",
        PMES_MAGIC,
        PMES_VERSION,
        len(vertices),
        len(indices),
        PMES_VERTEX_SIZE,
        PMES_FORMAT_COLOUR_XYZ,
    )

    payload = bytearray(header)
    for index, (x, y, z) in enumerate(vertices):
        payload.extend(struct.pack("<Ifff", COLOURS[index % len(COLOURS)], x, y, z))
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
