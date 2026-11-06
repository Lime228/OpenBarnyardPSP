#!/usr/bin/env python3

import struct
from pathlib import Path


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def validate_png(path: Path, width: int, height: int, maximum_size: int) -> None:
    data = path.read_bytes()
    if len(data) > maximum_size:
        raise ValueError(f"{path} exceeds the PSP package budget ({len(data)} > {maximum_size})")
    if data[:8] != PNG_SIGNATURE or data[12:16] != b"IHDR":
        raise ValueError(f"{path} is not a PNG with an IHDR first chunk")

    actual_width, actual_height, bit_depth, colour_type = struct.unpack(">IIBB", data[16:26])
    if (actual_width, actual_height) != (width, height):
        raise ValueError(
            f"{path} has dimensions {actual_width}x{actual_height}, expected {width}x{height}"
        )
    if bit_depth != 8 or colour_type not in (2, 6):
        raise ValueError(
            f"{path} must be an 8-bit RGB/RGBA PNG, got depth={bit_depth} type={colour_type}"
        )


def main() -> None:
    assets = Path(__file__).resolve().parents[1] / "Assets"
    validate_png(assets / "ICON0.PNG", 144, 80, 128 * 1024)
    validate_png(assets / "PIC1.PNG", 480, 272, 512 * 1024)
    print("PSP package artwork OK")


if __name__ == "__main__":
    main()
