#!/usr/bin/env python3

import argparse
import pathlib
import struct


PLOC_MAGIC = 0x434F4C50
PLOC_VERSION = 1
STRINGS = (
    "OPENBARNYARD",
    "START PROBE",
    "PORT STATUS",
    "PSP PORT STATUS",
    "CORE READY",
    "RENDER READY",
    "CIRCLE - BACK",
)


def generate(destination: pathlib.Path) -> None:
    offsets = [0]
    code_units: list[int] = []
    for text in STRINGS:
        encoded = text.encode("utf-16le") + b"\x00\x00"
        code_units.extend(struct.unpack(f"<{len(encoded) // 2}H", encoded))
        offsets.append(len(code_units))

    output = bytearray(struct.pack("<4I", PLOC_MAGIC, PLOC_VERSION, len(STRINGS), len(code_units)))
    output.extend(struct.pack(f"<{len(offsets)}I", *offsets))
    output.extend(struct.pack(f"<{len(code_units)}H", *code_units))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate the PSP bootstrap locale table")
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()
    generate(args.destination)


if __name__ == "__main__":
    main()
