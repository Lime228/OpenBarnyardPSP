#!/usr/bin/env python3
"""Export the original EnvMain character shadow size and texture for PSP."""

import argparse
import struct
import subprocess
import tempfile
from pathlib import Path

from convert_dds import convert as convert_texture
from inventory_retail_models import run_export, run_tmdl
from inventory_retail_sequence import retail_file
from read_retail_trb import read_sections


def read_shadow(source: Path, symbols: dict) -> tuple[float, str]:
    sections, follow = read_sections(source)
    addresses = {symbol["name"]: (symbol["stream"], symbol["offset"]) for symbol in symbols["symbols"]}

    def values(address: tuple[int, int], layout: str, offset: int = 0) -> tuple:
        section, start = address
        return struct.unpack_from(layout, sections[section], start + offset)

    def pointer(address: tuple[int, int], offset: int = 0) -> tuple[int, int]:
        return follow(address[0], address[1] + offset)

    def string(address: tuple[int, int], offset: int = 0) -> str:
        section, start = pointer(address, offset)
        text, terminator, _ = bytes(sections[section][start:start + 128]).partition(b"\0")
        if not terminator:
            raise ValueError("unterminated particle name")
        return text.decode("ascii")

    effects = addresses["EffectManager"]
    count, = values(effects, "<i")
    section, start = pointer(effects, 4)
    if not 0 < count <= (len(sections[section]) - start) // 4:
        raise ValueError("invalid particle effect count")
    effect, = [item for i in range(count) if string(item := pointer((section, start + 4 * i))) == "shadow"]
    if values(effect, "<h", 4) != (1,):
        raise ValueError("expected the original single-emitter shadow")
    emitter = pointer(effect, 8)
    settings = pointer(emitter, 4)
    if values(settings, "<i", 0x3C) != (0,) or values(pointer(settings, 0x68), "<B", 0x4C) != (1,):
        raise ValueError("expected the original fixed-corner shadow sprite")

    size_init = pointer(settings, 0x44 + 4 * 4)
    size_data = pointer(size_init, 4)
    if values(size_data, "<iI", 4) != (13, 16):
        raise ValueError("expected the original fixed shadow size")
    size, = values(size_data, "<f", 12)
    if not 0 < size <= 8.0:
        raise ValueError("invalid original shadow size")

    materials = addresses["MaterialManager"]
    count, = values(materials, "<i")
    section, start = pointer(materials, 4)
    if not 0 < count <= (len(sections[section]) - start) // 28:
        raise ValueError("invalid particle material count")
    name = string(settings, 4)
    material, = [item for i in range(count) if string(item := (section, start + 28 * i)) == name]
    if values(material, "<i", 0x18) != (3,) or values(pointer(material, 0x14), "<4f") != (0, 0, 1, 1):
        raise ValueError("unexpected original shadow blend or UVs")
    return size, string(material, 4)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", type=Path, required=True)
    parser.add_argument("install", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        tool = args.tmdl.resolve(strict=True)
        (tool.parent / "Logs").mkdir(exist_ok=True)
        install = args.install.resolve(strict=True)
        output = args.output.resolve()
        output.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="shadow-", dir=output) as directory:
            scratch = Path(directory)
            source = retail_file(install, "Data/Particles/pbarnyard.trb")
            size, texture = read_shadow(source, run_export(tool, "-psp-symbols", source, scratch / "symbols.json"))
            library = retail_file(install, "Data/Particles/pbarnyard.ttl")
            dds = scratch / "shadow.dds"
            run_tmdl(tool, "-psp-texture", library, dds, "-texture", texture)
            convert_texture(dds, output / "shadow.ptex")
            config = scratch / "shadow.pshs"
            config.write_bytes(struct.pack("<4sIf", b"PSHS", 1, size))
            config.replace(output / config.name)
    except (IndexError, KeyError, OSError, ValueError, struct.error, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"Story shadow package failed: {error}\n")
    print(f"Exported the original {size:.3f} m shadow sprite and {texture}")


if __name__ == "__main__":
    main()
