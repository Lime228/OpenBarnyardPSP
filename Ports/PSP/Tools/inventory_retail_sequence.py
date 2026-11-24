#!/usr/bin/env python3
"""Resolve a scripted sequence's asset declarations from the installed game."""

import argparse
import json
import struct
from pathlib import Path, PurePosixPath

from inventory_retail_models import run_export


TSO_HEADER = struct.Struct("<6I")
TSO_SCRIPT = struct.Struct("<IiIII")
TSO_INSTRUCTION = struct.Struct("<iIi")
TSO_VOICE = struct.Struct("<Iii")
TSO_OP_WAIT = 3
TSO_OP_ANIMATION = 4
TSO_OP_VOICE = 5
TSO_OP_RESUME = 11
TSO_OP_COMMAND = 12
TSO_OP_END = 13
# ASSeq::ms_aInstructionSizes, up to the END sentinel.
TSO_INSTRUCTION_SIZES = (0x1C, 0x14, 0x1C, 0x14, 0x1C, 0x18, 0x18, 0x14, 0x10, 0x10, 0x0C, 0x0C, 0x1C)
TSO_SOUND_TYPES = ("vostream", "cue")
CAMERA_COMMANDS = {"bindCamera", "Set_FOV", "anim_FOV", "SetTerrain", "SetNearClip"}


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
    string_fields = set()
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
            if offset == strings_reloc:
                string_fields.add(field)

    name_offset, _, count, actors_offset, tracks_offset = TSO_SCRIPT.unpack_from(data)
    if count > len(data) // 4 or any(pointers + offset + count * 4 > len(data) for offset in (actors_offset, tracks_offset)):
        raise ValueError("invalid TSO track tables")

    def read_string(offset: int) -> str:
        start = TSO_HEADER.size + strings + offset
        end = blob.find(b"\0", start)
        if end < start:
            raise ValueError("unterminated TSO string")
        return blob[start:end].decode("ascii")

    def read_string_field(field: int) -> str | None:
        if field in string_fields:
            offset, = struct.unpack_from("<I", data, field)
            return read_string(offset)
        return None

    name = read_string(name_offset)
    if not name:
        raise ValueError("invalid TSO script name")
    actors = [actor for actor, in struct.iter_unpack("<i", data[pointers + actors_offset:pointers + actors_offset + count * 4])]
    starts = struct.unpack_from(f"<{count}I", data, pointers + tracks_offset)
    code_end = min(strings_reloc, strings, pointers_reloc, pointers)
    ordered_starts = sorted(set(starts))
    limits = dict(zip(ordered_starts, ordered_starts[1:] + [code_end]))
    voices = []
    cameras = []
    master = []
    for track, (actor, start) in enumerate(zip(actors, starts)):
        limit = limits[start]
        if start % 4 or not TSO_SCRIPT.size <= start < limit <= code_end:
            raise ValueError(f"invalid TSO instruction range for track {track}")
        cursor = start
        camera_events = []
        while cursor + TSO_INSTRUCTION.size <= limit:
            opcode, _, _ = TSO_INSTRUCTION.unpack_from(data, cursor)
            if opcode == TSO_OP_END:
                break
            if not 0 <= opcode < TSO_OP_END or cursor + TSO_INSTRUCTION_SIZES[opcode] > limit:
                raise ValueError(f"invalid TSO instruction for track {track}")
            if opcode == TSO_OP_VOICE:
                sound_name, index, kind = TSO_VOICE.unpack_from(data, cursor + TSO_INSTRUCTION.size)
                if not 0 <= kind < len(TSO_SOUND_TYPES):
                    raise ValueError(f"invalid TSO sound type for track {track}")
                voices.append({"track": track, "actor": actor, "type": TSO_SOUND_TYPES[kind],
                               "index": index, "name": read_string(sound_name), "opens_track": cursor == start})
            elif opcode == TSO_OP_ANIMATION:
                # ASSeqActorInterface::SequenceEvent has the clip name at +0x10.
                clip = read_string_field(cursor + 0x10)
                if clip:
                    camera_events.append({"type": "animation", "name": clip})
            elif opcode == TSO_OP_COMMAND:
                command = read_string_field(cursor + 0x0C)
                if command in CAMERA_COMMANDS:
                    arguments = [read_string_field(cursor + field) for field in (0x10, 0x14, 0x18)]
                    camera_events.append({"type": "command", "name": command, "arguments": arguments})
            elif actor == 0 and opcode == TSO_OP_WAIT:
                flag, duration = struct.unpack_from("<if", data, cursor + TSO_INSTRUCTION.size)
                master.append({"type": "wait", "flag": flag, "duration": duration})
            elif actor == 0 and opcode == TSO_OP_RESUME:
                master.append({"type": "resume"})
            cursor += TSO_INSTRUCTION_SIZES[opcode]
        else:
            raise ValueError(f"missing TSO track end for track {track}")
        if any(event["name"] == "bindCamera" for event in camera_events):
            cameras.append({"track": track, "actor": actor, "events": camera_events})
    return {"name": name, "tracks": count, "track_actors": actors, "voice_instructions": voices,
            "camera_tracks": cameras, "master_timing": master}


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
            "packed_world_database": entries.get(f"{model}_Database".casefold()),
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
