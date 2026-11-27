#!/usr/bin/env python3
"""Inventory the original story population and quest-tier spawn records."""

import argparse
import json
import struct
import subprocess
from pathlib import Path

from inventory_retail_models import run_export
from inventory_retail_sequence import retail_file
from read_retail_trb import read_sections


POPULATION = "Data/Units/AnimalSpawn.trb"
STRING_TABLES = ("type", "name", "quest", "animation_set", "animation", "network",
                 "area", "goal", "locator", "behaviour", "interaction")
SPAWN_TABLES = ("animation_set", "animation", "type", "name", "network",
                "area", "goal", "locator", "behaviour", "interaction")
SPAWN_DATA = "4B8hH"  # AAnimalPopulationManager::SpawnData (0x16 bytes)
ANIMAL_RECORD = struct.Struct("<Hh" + SPAWN_DATA)
QUEST_RECORD = struct.Struct("<2B" + SPAWN_DATA)


def read_population(source: Path, symbols: dict) -> dict:
    sections, follow = read_sections(source)
    addresses = {symbol["name"]: (symbol["stream"], symbol["offset"]) for symbol in symbols["symbols"]}

    def entries(address: tuple[int, int], entry_size: int) -> tuple[int, range]:
        section, offset = address
        count, = struct.unpack_from("<i", sections[section], offset + 4)
        entry_section, start = follow(section, offset)
        end = start + count * entry_size
        if count < 0 or end > len(sections[entry_section]):
            raise ValueError("invalid population table length")
        return entry_section, range(start, end, entry_size)

    section, offset = addresses["StringMappingTables"]
    string_tables = {}
    for i, name in enumerate(STRING_TABLES):
        entry_section, pointers = entries((section, offset + i * 8), 4)
        strings = []
        for pointer in pointers:
            text_section, start = follow(entry_section, pointer)
            text, terminator, _ = bytes(sections[text_section][start:start + 256]).partition(b"\0")
            if not terminator:
                raise ValueError(f"unterminated string in {name} table")
            strings.append(text.decode("ascii"))
        string_tables[name] = strings

    def decode_string(name: str, index: int) -> str:
        if not 0 <= index < len(string_tables[name]):
            raise ValueError(f"invalid {name} string index {index}")
        return string_tables[name][index]

    def spawn_data(record: list[int]) -> dict:
        flags, count, *fields, marker_flags = record
        return {
            "flags": flags, "count": count, "marker_flags": marker_flags,
            **{name: decode_string(name, index) for name, index in zip(SPAWN_TABLES, fields, strict=True)},
        }

    section, offsets = entries(addresses["AnimalSpawnSet"], ANIMAL_RECORD.size)
    animals = []
    for i, offset in enumerate(offsets):
        state, tier_spawn, *data = ANIMAL_RECORD.unpack_from(sections[section], offset)
        animals.append({"index": i, "state": state, "tier_spawn_index": tier_spawn, **spawn_data(data)})

    section, offsets = entries(addresses["QuestTierSpawnSet"], QUEST_RECORD.size)
    quest_spawns = []
    for i, offset in enumerate(offsets):
        if i == 0:  # Retail reserves the first entry for animals without a tier override.
            continue
        tier, quest, *data = QUEST_RECORD.unpack_from(sections[section], offset)
        quest_spawns.append({
            "index": i, "absolute_tier": tier, "quest": decode_string("quest", quest), **spawn_data(data),
        })

    return {"version": 1, "source": POPULATION, "animals": animals, "quest_tier_spawns": quest_spawns}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", type=Path, required=True)
    parser.add_argument("install", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        tool = args.tmdl.resolve(strict=True)
        (tool.parent / "Logs").mkdir(exist_ok=True)
        source = retail_file(args.install.resolve(strict=True), POPULATION)
        output = args.output.resolve()
        symbols = run_export(tool, "-psp-symbols", source, output / "animal_spawn.symbols.json")
        report = read_population(source, symbols)
        (output / "sim_population.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    except (IndexError, KeyError, OSError, ValueError, struct.error, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"Sim population inventory failed: {error}\n")
    print(f"Read {len(report['animals'])} animal spawns and {len(report['quest_tier_spawns'])} quest-tier overrides")


if __name__ == "__main__":
    main()
