#!/usr/bin/env python3
"""Bake the original normal-weather dome and cloud layer."""

import argparse
import json
import math
import struct
from pathlib import Path

from convert_dds import convert as convert_texture
from convert_frontend_terrain_pack import write_model
from convert_mesh import PMES_COLOUR_VERTEX_SIZE, PMES_FORMAT_COLOUR_XYZ, PMES_MAGIC, PMES_VERSION
from inventory_retail_models import run_export, run_tmdl
from inventory_retail_sequence import retail_file
from read_retail_trb import read_sections


def read_time_fx(source: Path, symbols: dict, phase: int = 1) -> dict:
    sections, follow = read_sections(source)

    weather, = [symbol for symbol in symbols["symbols"] if symbol["name"] == "weathersettings"]
    # The first weather is normal. SS01 uses morning (1); LoadMainGameImpl uses dawn (0).
    weather_section, weather_offset = follow(weather["stream"], weather["offset"])
    fx_section, fx_offset = follow(weather_section, weather_offset + 4 * phase)
    fx = sections[fx_section][fx_offset:fx_offset + 0x178]
    if len(fx) != 0x178:
        raise ValueError("truncated AGameTimeFX record")
    name_section, name_offset = follow(fx_section, fx_offset + 0x114)
    name = bytes(sections[name_section][name_offset:]).split(b"\0", 1)[0].decode("ascii")
    colour_matrix = list(struct.iter_unpack("<4f", fx[0x90:0xD0]))
    light_matrix = list(struct.iter_unpack("<4f", fx[0xD0:0x110]))
    index, = struct.unpack_from("<i", fx, 0x124)
    rotation_speed, = struct.unpack_from("<f", fx, 0x144)
    if index not in (0, 1) or not 0.0 <= rotation_speed <= 10.0:
        raise ValueError("invalid original sky texture index or rotation speed")
    if any(not 0.0 <= channel <= 255.0 for row in colour_matrix + light_matrix for channel in row[:3]):
        raise ValueError("invalid original sky colour")
    return {"name": name, "colour_matrix": colour_matrix, "light_matrix": light_matrix,
            "texture_index": index, "rotation_speed": rotation_speed}


def bake_dome(matrix: list) -> dict:
    # Match ASkyDome::GenerateDomeVertices and the original Windows dimensions.
    sides, rings = 14, 6
    radius, height = 180.0, 75.0
    vertices, indices = [], []
    stride = sides + 1
    for vertical_sign in (1, -1):
        base = len(vertices)
        for ring in range(rings):
            ratio = ring / (rings - 1)
            ring_radius = math.cos(ratio * math.pi / 2.0) * radius
            for side in range(stride):
                angle = math.tau * (side % sides) / sides
                horizontal = 2.0 * min(side, sides - side) / sides
                position = (math.sin(angle) * ring_radius, math.cos(angle) * ring_radius, vertical_sign * ratio * height)
                colour = 0xff000000
                for channel in range(3):
                    low = matrix[2][channel] + horizontal * (matrix[3][channel] - matrix[2][channel])
                    high = matrix[0][channel] + horizontal * (matrix[1][channel] - matrix[0][channel])
                    colour |= int(low + ratio * (high - low)) << (8 * channel)
                uv = (side / sides - 0.00390625, min(0.994140625, max(0.005859375, 1.2 - 1.5 * ratio)))
                vertices.append((uv, position, colour))
                if ring < rings - 1 and side < sides:
                    a = base + ring * stride + side
                    indices.extend((a, a + stride, a + 1, a + 1, a + stride, a + stride + 1))
    return {"vertices": vertices, "indices": indices}


def package(tool: Path, install: Path, output: Path, phase: int = 1) -> dict:
    source = retail_file(install, "Data/GameTimeFXSettings.trb")
    symbols = run_export(tool, "-psp-symbols", source, output / "timefx-symbols.json")
    fx = read_time_fx(source, symbols, phase)
    sky_library = retail_file(install, "Data/Sky/sky.ttl")
    texture_name = ("sky_sunny_16.tga", "sky_night_16.tga")[fx["texture_index"]]
    dds = output / "sky-clouds.dds"
    run_tmdl(tool, "-psp-texture", sky_library, dds, "-texture", texture_name)
    texture = convert_texture(dds, output / "sky-clouds.ptex", max_width=256)

    base = bake_dome(fx["colour_matrix"])
    vertices, indices = base["vertices"], base["indices"]
    payload = bytearray(struct.pack("<6I", PMES_MAGIC, PMES_VERSION, len(vertices), len(indices),
                                    PMES_COLOUR_VERTEX_SIZE, PMES_FORMAT_COLOUR_XYZ))
    for _, position, colour in vertices:
        payload.extend(struct.pack("<I3f", colour, *position))
    payload.extend(struct.pack(f"<{len(indices)}H", *indices))
    (output / "sky-base.pmes").write_bytes(payload)
    clouds = write_model(bake_dome(fx["light_matrix"]), output / "sky-clouds.pmes")
    (output / "sky.psky").write_bytes(struct.pack("<4sIf", b"PSKY", 1, fx["rotation_speed"]))
    report = {"time_fx": fx["name"], "texture": texture_name, "colour_matrix": fx["colour_matrix"],
              "rotation_speed": fx["rotation_speed"], "texture_width": texture["width"],
              "texture_height": texture["height"], "bytes": len(payload) + clouds["bytes"] + texture["bytes"]}
    (output / "sky.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", type=Path, required=True)
    parser.add_argument("--phase", choices=("dawn", "morning"), required=True)
    parser.add_argument("install", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = package(args.tmdl.resolve(strict=True), args.install.resolve(strict=True), args.output.resolve(),
                     phase=("dawn", "morning").index(args.phase))
    print(f"Packaged the original {result['time_fx']} sky")
