#!/usr/bin/env python3

import argparse
import pathlib
import struct


PTEX_MAGIC = 0x58455450
PTEX_VERSION = 1
PTEX_FORMAT_RGBA8888 = 0
PTEX_FORMAT_RGBA8888_SWIZZLED = 1
SWIZZLE_BLOCK_WIDTH_BYTES = 16
SWIZZLE_BLOCK_HEIGHT = 8


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


def swizzle_rgba8888(pixels: bytes, buffer_width: int, height: int) -> bytes:
    row_bytes = buffer_width * 4
    if row_bytes % SWIZZLE_BLOCK_WIDTH_BYTES != 0 or height % SWIZZLE_BLOCK_HEIGHT != 0:
        raise ValueError("swizzled PSP textures require 16-byte rows and a height divisible by 8")

    output = bytearray(len(pixels))
    block_count_x = row_bytes // SWIZZLE_BLOCK_WIDTH_BYTES
    block_count_y = height // SWIZZLE_BLOCK_HEIGHT
    destination = 0
    for block_y in range(block_count_y):
        for block_x in range(block_count_x):
            for row in range(SWIZZLE_BLOCK_HEIGHT):
                source = (block_y * SWIZZLE_BLOCK_HEIGHT + row) * row_bytes
                source += block_x * SWIZZLE_BLOCK_WIDTH_BYTES
                output[destination : destination + SWIZZLE_BLOCK_WIDTH_BYTES] = (
                    pixels[source : source + SWIZZLE_BLOCK_WIDTH_BYTES]
                )
                destination += SWIZZLE_BLOCK_WIDTH_BYTES
    return bytes(output)


def convert(source: pathlib.Path, destination: pathlib.Path, linear: bool = False) -> None:
    width, height, values = read_ppm(source)
    if not is_power_of_two(width) or not is_power_of_two(height):
        raise ValueError("PSP probe texture dimensions must be powers of two")
    if width > 512 or height > 512:
        raise ValueError("PSP texture dimensions cannot exceed 512")

    pixels = bytearray()
    for index in range(0, len(values), 3):
        pixels.extend((values[index], values[index + 1], values[index + 2], 255))

    texture_format = PTEX_FORMAT_RGBA8888
    if not linear:
        pixels = bytearray(swizzle_rgba8888(pixels, width, height))
        texture_format = PTEX_FORMAT_RGBA8888_SWIZZLED

    header = struct.pack(
        "<6I",
        PTEX_MAGIC,
        PTEX_VERSION,
        width,
        height,
        width,
        texture_format,
    )

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(header + pixels)


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert an ASCII PPM texture to OpenBarnyard PSP PTEX")
    parser.add_argument("--linear", action="store_true", help="store linear pixels instead of PSP-swizzled blocks")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()
    convert(args.source, args.destination, args.linear)


if __name__ == "__main__":
    main()
