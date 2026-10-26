#!/usr/bin/env python3

import argparse
import pathlib
import struct


PTEX_MAGIC = 0x58455450
PTEX_VERSION = 1
PTEX_FORMAT_RGBA8888 = 0


def read_ppm(path: pathlib.Path) -> tuple[int, int, list[int]]:
    tokens: list[str] = []
    for line in path.read_text(encoding="ascii").splitlines():
        tokens.extend(line.split("#", 1)[0].split())

    if len(tokens) < 4 or tokens[0] != "P3":
        raise ValueError("only ASCII P3 PPM input is supported")

    width, height, maximum = map(int, tokens[1:4])
    values = [int(value) for value in tokens[4:]]
    if maximum != 255:
        raise ValueError("PPM maximum value must be 255")
    if len(values) != width * height * 3:
        raise ValueError("PPM pixel count does not match its dimensions")
    return width, height, values


def is_power_of_two(value: int) -> bool:
    return value > 0 and value & (value - 1) == 0


def convert(source: pathlib.Path, destination: pathlib.Path) -> None:
    width, height, values = read_ppm(source)
    if not is_power_of_two(width) or not is_power_of_two(height):
        raise ValueError("PSP probe texture dimensions must be powers of two")
    if width > 512 or height > 512:
        raise ValueError("PSP texture dimensions cannot exceed 512")

    header = struct.pack(
        "<6I",
        PTEX_MAGIC,
        PTEX_VERSION,
        width,
        height,
        width,
        PTEX_FORMAT_RGBA8888,
    )
    pixels = bytearray()
    for index in range(0, len(values), 3):
        pixels.extend((values[index], values[index + 1], values[index + 2], 255))

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(header + pixels)


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert an ASCII PPM texture to OpenBarnyard PSP PTEX")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()
    convert(args.source, args.destination)


if __name__ == "__main__":
    main()
