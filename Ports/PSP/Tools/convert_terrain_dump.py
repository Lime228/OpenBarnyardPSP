#!/usr/bin/env python3

import argparse
import json
import math
import pathlib
import struct

from convert_mesh import (
    MAX_ELEMENT_COUNT,
    PMES_FORMAT_TEXTURE_COLOUR_XYZ,
    PMES_MAGIC,
    PMES_TEXTURE_VERTEX_SIZE,
    PMES_VERSION,
)
from convert_tmd_dump import triangulate_strip


MAX_TERRAIN_VERTEX_COUNT = 8192


def vector(value: object, size: int, name: str) -> list[float]:
    if not isinstance(value, list) or len(value) != size:
        raise ValueError(f"{name} requires {size} components")
    result = [float(component) for component in value]
    if not all(math.isfinite(component) for component in result):
        raise ValueError(f"{name} contains a non-finite component")
    return result


def pack_colour(colour: list[float]) -> int:
    channels = [max(0, min(255, round(component * 255.0))) for component in colour]
    return 0xFF000000 | channels[2] << 16 | channels[1] << 8 | channels[0]


def convert_document(document: object, destination: pathlib.Path) -> dict[str, object]:
    if not isinstance(document, dict) or document.get("version") != 1:
        raise ValueError("unsupported terrain dump contract version")
    material = document.get("material")
    texture = document.get("texture", "")
    vertices = document.get("vertices")
    if not isinstance(material, str) or not material or not isinstance(vertices, list):
        raise ValueError("terrain dump requires a material and vertices")
    if not isinstance(texture, str):
        raise ValueError("terrain dump texture must be a string")
    if not 3 <= len(vertices) <= MAX_TERRAIN_VERTEX_COUNT:
        raise ValueError(f"terrain selection must contain between 3 and {MAX_TERRAIN_VERTEX_COUNT} vertices")

    parsed_vertices = []
    for index, vertex in enumerate(vertices):
        if not isinstance(vertex, dict):
            raise ValueError(f"vertex {index} must be an object")
        parsed_vertices.append(
            (
                vector(vertex.get("uv"), 2, f"vertex {index} uv"),
                vector(vertex.get("position"), 3, f"vertex {index} position"),
                pack_colour(vector(vertex.get("colour"), 3, f"vertex {index} colour")),
            )
        )
    indices = triangulate_strip(document.get("triangle_strip"), len(parsed_vertices))
    if len(indices) > MAX_ELEMENT_COUNT:
        raise ValueError("terrain selection exceeds the PSP 16-bit index limit")

    payload = bytearray(
        struct.pack(
            "<6I",
            PMES_MAGIC,
            PMES_VERSION,
            len(parsed_vertices),
            len(indices),
            PMES_TEXTURE_VERTEX_SIZE,
            PMES_FORMAT_TEXTURE_COLOUR_XYZ,
        )
    )
    for uv, position, colour in parsed_vertices:
        payload.extend(struct.pack("<ffIfff", uv[0], uv[1], colour, *position))
    payload.extend(struct.pack(f"<{len(indices)}H", *indices))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(payload)
    return {
        "material": material,
        "texture": texture,
        "vertices": len(parsed_vertices),
        "indices": len(indices),
        "bytes": len(payload),
    }


def convert(source: pathlib.Path, destination: pathlib.Path) -> dict[str, object]:
    return convert_document(json.loads(source.read_text(encoding="utf-8")), destination)


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert one bounded retail terrain selection to PSP PMES")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()
    print(json.dumps(convert(args.source, args.destination), sort_keys=True))


if __name__ == "__main__":
    main()
