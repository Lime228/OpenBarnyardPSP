#!/usr/bin/env python3
"""Resolve the original new-game spawn and initial terrain from the retail install."""

import argparse
import json
import math
import struct
import subprocess
from collections import Counter
from pathlib import Path

from inventory_retail_models import run_export
from inventory_retail_sequence import retail_file
from read_retail_trb import read_sections


NETWORK = "Data/Networks/park2.trb"
TERRAIN = "Data/Terrain/EnvMain/EnvMain.trb"
START_GOAL = "park2_goals_playerstart"  # AGameLoader::LoadMainGameImpl
GOAL_SIZE = 0x40  # Retail 32-bit AInteriorGoal


def read_goal(source: Path, symbols: dict, goal_name: str) -> dict:
    sections, follow = read_sections(source)
    network, = [symbol for symbol in symbols["symbols"] if symbol["name"] == "INetwork"]
    section, offset = network["stream"], network["offset"]
    area_count, goal_count = struct.unpack_from("<2i", sections[section], offset)
    goal_section, start = follow(section, offset + 0x14)
    goals = sections[goal_section]
    if not 0 < goal_count <= (len(goals) - start) // GOAL_SIZE:
        raise ValueError("invalid network goal table")

    matches = []
    for index in range(goal_count):
        cursor = start + index * GOAL_SIZE
        name_section, name_offset = follow(goal_section, cursor + 0x34)
        # The name is a relocated C string in the original network.
        encoded_name, terminator, _ = bytes(sections[name_section][name_offset:name_offset + 256]).partition(b"\0")
        if not terminator:
            raise ValueError("unterminated network goal name")
        name = encoded_name.decode("ascii")
        if name.casefold() != goal_name.casefold():
            continue
        forward = struct.unpack_from("<3f", goals, cursor)
        x, z = struct.unpack_from("<2f", goals, cursor + 0x10)
        y, _, export_id, area = struct.unpack_from("<f3i", goals, cursor + 0x24)
        if not all(math.isfinite(value) for value in (*forward, x, y, z)) or not -1 <= area < area_count:
            raise ValueError(f"invalid position or area for {goal_name}")
        matches.append({"name": name, "stored_position": [x, y, z], "facing": forward,
                        "area_index": area, "export_id": export_id})
    if len(matches) != 1:
        raise ValueError(f"expected one original network goal named {goal_name}")
    return matches[0]


def inventory(tool: Path, install: Path, output: Path) -> dict:
    network = retail_file(install, NETWORK)
    terrain = retail_file(install, TERRAIN)
    symbols = run_export(tool, "-psp-symbols", network, output / "park2.symbols.json")
    goal = read_goal(network, symbols, START_GOAL)
    vis = run_export(tool, "-psp-terrain-vis", terrain, output / "envmain.vis.json", version=2)
    sections = vis["sections"]
    first = sections[0]  # Terrain_SetMain uses VIS index 0 when there is no saved index.
    if len(first["visibility"]) != len(sections):
        raise ValueError("incomplete first EnvMain visibility row")
    counts = Counter(first["visibility"])
    report = {"version": 1, "network": NETWORK, "start_goal": goal, "terrain": TERRAIN,
              "initial_visibility_row": first["name"], "visibility_counts": dict(counts), "sections": len(sections)}
    if "collision" in first:
        collision = []
        for section, lod in zip(sections, first["visibility"], strict=True):
            filename = section["collision"]
            if filename:
                asset = retail_file(install, filename)
                collision.append({"section": section["name"], "path": asset.relative_to(install).as_posix(),
                                  "bytes": asset.stat().st_size, "initial_lod": lod})
        # Terrain_SetMain enables streamed collision. ATerrainSection queues
        # each collider with the high LOD, including the initial VIS row.
        report["collision"] = collision
        report["initial_collision_bytes"] = sum(entry["bytes"] for entry in collision if entry["initial_lod"] == "high")
    (output / "sim_start.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", type=Path, required=True)
    parser.add_argument("install", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        tool = args.tmdl.resolve(strict=True)
        (tool.parent / "Logs").mkdir(exist_ok=True)
        result = inventory(tool, args.install.resolve(strict=True), args.output.resolve())
    except (IndexError, KeyError, OSError, ValueError, struct.error, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"Sim start inventory failed: {error}\n")
    print(f"Resolved {result['start_goal']['name']} and the first {result['initial_visibility_row']} VIS row")
    if "collision" in result:
        count = sum(entry["initial_lod"] == "high" for entry in result["collision"])
        print(f"EnvMain has {len(result['collision'])} collision TRBs, including {count} at the initial high LOD")


if __name__ == "__main__":
    main()
