#!/usr/bin/env python3

import argparse
import pathlib
import struct


PBNK_MAGIC = 0x4B4E4250
PBNK_VERSION = 2
MAX_WAVES = 16
NAME_SIZE = 48
PATH_SIZE = 64


def encode_field(value: str, size: int, label: str) -> bytes:
    encoded = value.encode("ascii")
    if not encoded or len(encoded) >= size:
        raise ValueError(f"{label} must contain 1-{size - 1} ASCII bytes")
    return encoded + bytes(size - len(encoded))


def generate(destination: pathlib.Path, waves: list[str], streams: list[str]) -> None:
    specifications = [(0, value) for value in waves] + [(1, value) for value in streams]
    if not 1 <= len(specifications) <= MAX_WAVES:
        raise ValueError(f"wave bank must contain 1-{MAX_WAVES} entries")

    names: set[str] = set()
    entries = bytearray()
    for entry_type, specification in specifications:
        name, separator, path = specification.partition("=")
        if not separator:
            raise ValueError(f"invalid wave specification: {specification}")
        folded_name = name.casefold()
        if folded_name in names:
            raise ValueError(f"duplicate wave name: {name}")
        names.add(folded_name)
        entries.extend(struct.pack("<I", entry_type))
        entries.extend(encode_field(name, NAME_SIZE, "wave name"))
        entries.extend(encode_field(path, PATH_SIZE, "wave path"))

    output = bytearray(struct.pack("<3I", PBNK_MAGIC, PBNK_VERSION, len(specifications)))
    output.extend(entries)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a bounded PSP wave bank manifest")
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--wave", action="append", default=[], dest="waves")
    parser.add_argument("--stream", action="append", default=[], dest="streams")
    args = parser.parse_args()
    generate(args.destination, args.waves, args.streams)


if __name__ == "__main__":
    main()
