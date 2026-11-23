#!/usr/bin/env python3
"""Inventory FSB3.1 banks with full sample headers for offline conversion."""

import argparse
import json
import struct
from pathlib import Path


BANK_HEADER = struct.Struct("<4s5I")
SAMPLE_HEADER = struct.Struct("<H30siIiiIi6xH16x")
FSB31 = 0x00030001
MODE_DUPLICATE = 0x00008000


def find_banks(install: Path) -> list[Path]:
    paths = (
        path.relative_to(install)
        for path in install.rglob("*")
        if path.suffix.lower() == ".fsb" and path.is_file()
    )
    return sorted(paths, key=lambda path: path.as_posix().casefold())


def inventory_bank(path: Path) -> dict:
    with path.open("rb") as bank:
        header = bank.read(BANK_HEADER.size)
        if len(header) != BANK_HEADER.size:
            raise ValueError("truncated bank header")
        magic, count, table_bytes, data_bytes, version, flags = BANK_HEADER.unpack(header)
        if magic != b"FSB3" or version != FSB31 or flags:
            raise ValueError("expected FSB3.1 with full headers and unpadded data")
        file_bytes = path.stat().st_size
        data_start = BANK_HEADER.size + table_bytes
        if data_start + data_bytes != file_bytes or count * SAMPLE_HEADER.size > table_bytes:
            raise ValueError("inconsistent table or data size")

        tracks = []
        data_offset = data_start
        for index in range(count):
            header_offset = bank.tell()
            if header_offset + SAMPLE_HEADER.size > data_start:
                raise ValueError(f"sample {index + 1} header exceeds the table")

            row = bank.read(SAMPLE_HEADER.size)
            size, name, frames, stream_bytes, loop_start, loop_end, mode, rate, channels = (
                SAMPLE_HEADER.unpack(row)
            )
            if size < SAMPLE_HEADER.size or header_offset + size > data_start:
                raise ValueError(f"invalid sample {index + 1} header size")
            bank.seek(header_offset + size)

            if mode & MODE_DUPLICATE:
                if not tracks:
                    raise ValueError("first sample cannot reference a previous payload")
                payload_offset = tracks[-1]["payload_offset"]
                if stream_bytes > tracks[-1]["payload_bytes"]:
                    raise ValueError(f"sample {index + 1} exceeds the referenced payload")
            else:
                payload_offset = data_offset
                data_offset += stream_bytes
                if data_offset > file_bytes:
                    raise ValueError(f"sample {index + 1} exceeds the bank payload")

            tracks.append(
                {
                    "index": index + 1,
                    "name": name.split(b"\0", 1)[0].decode("ascii", errors="replace"),
                    "frames": frames,
                    "sample_rate": rate,
                    "channels": channels,
                    "mode": mode,
                    "loop_start": loop_start,
                    "loop_end_inclusive": loop_end,
                    "payload_offset": payload_offset,
                    "payload_bytes": stream_bytes,
                }
            )

    return {"tracks": tracks, "data_bytes": data_bytes}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("install", type=Path)
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    banks = find_banks(args.install)
    if not banks:
        parser.error("no FSB input files found")

    entries = []
    for path in banks:
        entry = {"path": path.as_posix()}
        try:
            entry.update(status="inventoried", **inventory_bank(args.install / path))
        except (OSError, ValueError, struct.error) as error:
            entry.update(status="failed", error=str(error))
        entries.append(entry)

    failed = sum(entry["status"] == "failed" for entry in entries)
    report = {
        "version": 1,
        "bank_count": len(entries),
        "failed_bank_count": failed,
        "track_count": sum(len(entry.get("tracks", [])) for entry in entries),
        "banks": entries,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.report.with_suffix(args.report.suffix + ".tmp")
    temporary.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    temporary.replace(args.report)
    print(f"{len(entries) - failed}/{len(entries)} banks, {report['track_count']} tracks")
    return int(failed != 0)


if __name__ == "__main__":
    raise SystemExit(main())
