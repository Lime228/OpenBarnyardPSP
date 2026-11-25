"""Read uncompressed retail TRB sections and their relocated pointers."""

import struct
from collections.abc import Callable
from pathlib import Path


def read_sections(source: Path) -> tuple[list[memoryview], Callable[[int, int], tuple[int, int]]]:
    data = source.read_bytes()
    magic, size, form = struct.unpack_from("<4sI4s", data)
    if (magic, size, form) != (b"TSFL", len(data) - 8, b"TRBF"):
        raise ValueError("expected an uncompressed little-endian retail TRB")
    chunks = {}
    cursor = 12
    while cursor < len(data):
        name, length = struct.unpack_from("<4sI", data, cursor)
        end = cursor + 8 + length
        if end > len(data) or name in chunks:
            raise ValueError("invalid retail TRB TRB chunk")
        chunks[name] = memoryview(data)[cursor + 8:end]
        cursor = (end + 3) & ~3

    header, payload, reloc = (chunks[name] for name in (b"HDRX", b"SECT", b"RELC"))
    count, = struct.unpack_from("<I", header, 4)
    if len(header) != 8 + 16 * count:
        raise ValueError("invalid retail TRB section headers")
    sections = []
    start = 0
    for i in range(count):
        length, = struct.unpack_from("<I", header, 12 + 16 * i)
        sections.append(payload[start:start + length])
        start += length
    if start != len(payload):
        raise ValueError("invalid retail TRB section lengths")
    relocation_count, = struct.unpack_from("<I", reloc)
    if len(reloc) != 4 + relocation_count * 8:
        raise ValueError("invalid retail TRB relocation count")
    relocations = {(section, offset): target for section, target, offset in struct.iter_unpack("<HHI", reloc[4:])}
    if len(relocations) != relocation_count:
        raise ValueError("duplicate retail TRB relocation")

    def follow(section: int, offset: int) -> tuple[int, int]:
        if (section, offset) not in relocations:
            raise ValueError("missing retail TRB pointer relocation")
        target = relocations[section, offset]
        target_offset, = struct.unpack_from("<I", sections[section], offset)
        if target >= len(sections) or target_offset >= len(sections[target]):
            raise ValueError("invalid retail TRB pointer target")
        return target, target_offset

    return sections, follow
