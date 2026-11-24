#!/usr/bin/env python3
"""Resolve a scripted sequence's asset declarations from the installed game."""

import argparse
import json
import struct
from pathlib import Path, PurePosixPath

from inventory_retail_models import run_export


TSO_HEADER = struct.Struct("<6I")
TSO_SCRIPT = struct.Struct("<IiIII")


def retail_file(install: Path, name: str) -> Path:
    relative = PurePosixPath(name.replace("\\", "/"))
    if relative.is_absolute() or ".." in relative.parts or ":" in name:
        raise ValueError(f"invalid retail file reference: {name}")
    path = install
    for part in relative.parts:
        matches = [entry for entry in path.iterdir() if entry.name.casefold() == part.casefold()]
        if len(matches) != 1:
            raise ValueError(f"expected one installed match for {name}: {part}")
        path = matches[0]
    if not path.is_file() or not path.resolve().is_relative_to(install):
        raise ValueError(f"expected an installed file: {name}")
    return path


def main_properties(document: dict) -> list:
    roots = [entry["root"]["properties"] for entry in document["symbols"] if entry["symbol"] == "Main"]
    if len(roots) != 1:
        raise ValueError("expected one Main property symbol")
    return roots[0]


def inspect_script(path: Path) -> dict:
    blob = path.read_bytes()
    if len(blob) < TSO_HEADER.size + TSO_SCRIPT.size:
        raise ValueError("truncated TSO header")
    data = memoryview(blob)[TSO_HEADER.size:]
    strings_reloc, strings_size, strings, pointers_reloc, pointers_size, pointers = TSO_HEADER.unpack_from(blob)
    for offset, size, base in ((strings_reloc, strings_size, strings), (pointers_reloc, pointers_size, pointers)):
        if offset % 4 or size % 4 or offset + size > len(data) or base > len(data):
            raise ValueError("invalid TSO relocation range")
        for cursor in range(offset, offset + size, 4):
            field, = struct.unpack_from("<I", data, cursor)
            if field % 4 or field + 4 > len(data):
                raise ValueError("invalid TSO relocation field")
            target, = struct.unpack_from("<I", data, field)
            if base + target >= len(data):
                raise ValueError("TSO relocation target is outside the file")

    name_offset, _, count, actors_offset, tracks_offset = TSO_SCRIPT.unpack_from(data)
    name_start = TSO_HEADER.size + strings + name_offset
    name_end = blob.find(b"\0", name_start)
    if name_end <= name_start:
        raise ValueError("invalid TSO script name")
    if count > len(data) // 4 or any(pointers + offset + count * 4 > len(data) for offset in (actors_offset, tracks_offset)):
        raise ValueError("invalid TSO track tables")

    actors = [actor for actor, in struct.iter_unpack("<i", data[pointers + actors_offset:pointers + actors_offset + count * 4])]
    return {"name": blob[name_start:name_end].decode("ascii"), "tracks": count, "track_actors": actors}


def describe_resources(properties: list, type_properties: list, symbols: dict) -> dict:
    descriptor = dict(properties)
    entries = {entry["name"].casefold(): entry["name"] for entry in symbols["symbols"]}

    def libraries(property_name: str, suffix: str) -> list:
        return [
            {"name": name, "symbol": entries.get(f"{name}{suffix}".casefold())}
            for name in descriptor.get(property_name, [])
        ]

    models = []
    for name, entry in type_properties:
        body = dict(entry["properties"])
        for kind in ("AnimatableObjectTypeBunch", "AnimatableObjectType"):
            if kind in body:
                attributes = dict(body[kind]["properties"])
                break
        else:
            kind, attributes = "same-name-model", {}
        model = attributes.get("Model", name)
        models.append({
            "type": name, "kind": kind, "model": model,
            "packed_header": entries.get(f"{model}_Header".casefold()),
            "packed_file_header": entries.get(f"{model}_FileHeader".casefold()),
        })

    return {
        "key_libraries": libraries("keylib", "_keylib"),
        "material_libraries": libraries("matlib", "_TTL"),
        "model_types": models,
    }


def write_inventory(tool: Path, install: Path, sequence: str, output: Path) -> dict:
    descriptor_path = retail_file(install, f"Data/ScriptedSequence/{sequence}.trb")
    script_path = retail_file(install, f"Data/ScriptedSequence/{sequence}.tso")
    type_path = retail_file(install, f"Data/ModelTypes/Libs/ScriptedSequence/{sequence}.trb")
    descriptor = run_export(tool, "-psp-properties", descriptor_path, output / "descriptor.json")
    properties = main_properties(descriptor)
    model_types = run_export(tool, "-psp-properties", type_path, output / "model-types.json")
    sources = {"descriptor": descriptor_path, "script": script_path, "model_types": type_path}
    pack = {"symbols": []}
    if pack_name := dict(properties).get("modellib"):
        pack_path = retail_file(install, pack_name)
        pack = run_export(tool, "-psp-symbols", pack_path, output / "pack-symbols.json")
        sources["asset_pack"] = pack_path
    report = {
        "version": 1,
        "sequence": sequence,
        "sources": {
            name: {"path": path.relative_to(install).as_posix(), "bytes": path.stat().st_size}
            for name, path in sources.items()
        },
        "script": inspect_script(script_path),
        "properties": properties,
        **describe_resources(properties, main_properties(model_types), pack),
    }
    pending = output / "sequence.json.tmp"
    pending.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    pending.replace(output / "sequence.json")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", type=Path, required=True)
    parser.add_argument("install", type=Path)
    parser.add_argument("sequence", help="original sequence file stem, for example SS01_start")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    install = args.install.resolve(strict=True)
    output = args.output.resolve()
    if not args.sequence or not all(c.isascii() and (c.isalnum() or c == "_") for c in args.sequence):
        parser.error("sequence must be a retail file stem")
    if output.is_relative_to(install):
        parser.error("output must be outside the retail installation")
    tool = args.tmdl.resolve(strict=True)
    (tool.parent / "Logs").mkdir(exist_ok=True)
    try:
        report = write_inventory(tool, install, args.sequence, output)
    except (KeyError, OSError, ValueError) as error:
        parser.exit(1, f"Sequence inventory failed: {error}\n")
    print(f"{report['sequence']}: {report['script']['tracks']} tracks, "
          f"{len(report['model_types'])} model types, {len(report['key_libraries'])} key libraries, "
          f"{len(report['material_libraries'])} material libraries")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
