#!/usr/bin/env python3

import argparse
import pathlib
import struct

from convert_texture import (
    PTEX_FORMAT_RGBA8888,
    PTEX_FORMAT_RGBA8888_SWIZZLED,
    PTEX_MAGIC,
    PTEX_VERSION,
    is_power_of_two,
    swizzle_rgba8888,
)


def decode_565(value: int) -> tuple[int, int, int]:
    red = (value >> 11) & 0x1F
    green = (value >> 5) & 0x3F
    blue = value & 0x1F
    return (
        (red << 3) | (red >> 2),
        (green << 2) | (green >> 4),
        (blue << 3) | (blue >> 2),
    )


def colour_table(block: bytes, allow_transparency: bool) -> list[tuple[int, int, int, int]]:
    colour0, colour1, indices = struct.unpack("<HHI", block)
    rgb0 = decode_565(colour0)
    rgb1 = decode_565(colour1)
    colours = [(*rgb0, 255), (*rgb1, 255)]
    if colour0 > colour1 or not allow_transparency:
        colours.extend(
            (
                tuple((2 * rgb0[channel] + rgb1[channel]) // 3 for channel in range(3)) + (255,),
                tuple((rgb0[channel] + 2 * rgb1[channel]) // 3 for channel in range(3)) + (255,),
            )
        )
    else:
        colours.extend(
            (
                tuple((rgb0[channel] + rgb1[channel]) // 2 for channel in range(3)) + (255,),
                (0, 0, 0, 0),
            )
        )
    return [colours[(indices >> (pixel * 2)) & 3] for pixel in range(16)]


def decode_dxt_block(fourcc: bytes, block: bytes) -> list[tuple[int, int, int, int]]:
    if fourcc == b"DXT1":
        return colour_table(block, True)
    if fourcc == b"DXT3":
        colours = colour_table(block[8:], False)
        alpha_bits = int.from_bytes(block[:8], "little")
        return [(*colour[:3], ((alpha_bits >> (pixel * 4)) & 0xF) * 17) for pixel, colour in enumerate(colours)]
    if fourcc == b"DXT5":
        colours = colour_table(block[8:], False)
        alpha0, alpha1 = block[0], block[1]
        alphas = [alpha0, alpha1]
        if alpha0 > alpha1:
            alphas.extend(((7 - index) * alpha0 + (index - 1) * alpha1) // 7 for index in range(2, 8))
        else:
            alphas.extend(((5 - index) * alpha0 + (index - 1) * alpha1) // 5 for index in range(2, 6))
            alphas.extend((0, 255))
        alpha_bits = int.from_bytes(block[2:8], "little")
        return [(*colour[:3], alphas[(alpha_bits >> (pixel * 3)) & 7]) for pixel, colour in enumerate(colours)]
    raise ValueError(f"unsupported DDS compression {fourcc!r}")


def read_dds(path: pathlib.Path) -> tuple[int, int, bytes]:
    payload = path.read_bytes()
    if len(payload) < 128 or payload[:4] != b"DDS " or struct.unpack_from("<I", payload, 4)[0] != 124:
        raise ValueError("input is not a supported DDS file")
    height, width = struct.unpack_from("<II", payload, 12)
    pixel_format_size = struct.unpack_from("<I", payload, 76)[0]
    fourcc = payload[84:88]
    if pixel_format_size != 32 or fourcc not in {b"DXT1", b"DXT3", b"DXT5"}:
        raise ValueError(f"DDS must use DXT1, DXT3 or DXT5 compression, got {fourcc!r}")
    if not is_power_of_two(width) or not is_power_of_two(height) or width > 512 or height > 512:
        raise ValueError("PSP DDS dimensions must be powers of two no larger than 512x512")

    block_size = 8 if fourcc == b"DXT1" else 16
    blocks_x = (width + 3) // 4
    blocks_y = (height + 3) // 4
    required_size = 128 + blocks_x * blocks_y * block_size
    if len(payload) < required_size:
        raise ValueError("DDS top mip payload is truncated")

    pixels = bytearray(width * height * 4)
    source = 128
    for block_y in range(blocks_y):
        for block_x in range(blocks_x):
            decoded = decode_dxt_block(fourcc, payload[source : source + block_size])
            source += block_size
            for local_y in range(4):
                for local_x in range(4):
                    x = block_x * 4 + local_x
                    y = block_y * 4 + local_y
                    if x >= width or y >= height:
                        continue
                    destination = (y * width + x) * 4
                    pixels[destination : destination + 4] = bytes(decoded[local_y * 4 + local_x])
    return width, height, bytes(pixels)


def convert(source: pathlib.Path, destination: pathlib.Path, linear: bool = False) -> None:
    width, height, pixels = read_dds(source)
    texture_format = PTEX_FORMAT_RGBA8888
    if not linear:
        pixels = swizzle_rgba8888(pixels, width, height)
        texture_format = PTEX_FORMAT_RGBA8888_SWIZZLED
    header = struct.pack("<6I", PTEX_MAGIC, PTEX_VERSION, width, height, width, texture_format)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(header + pixels)


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert a bounded DXT DDS texture to OpenBarnyard PSP PTEX")
    parser.add_argument("--linear", action="store_true", help="store linear pixels instead of PSP-swizzled blocks")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()
    convert(args.source, args.destination, args.linear)


if __name__ == "__main__":
    main()
