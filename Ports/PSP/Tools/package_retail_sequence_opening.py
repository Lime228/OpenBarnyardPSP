#!/usr/bin/env python3
"""Prepare the supported opening shots from the original scripted sequence."""

import argparse
import json
import struct
import subprocess
from pathlib import Path

from convert_camera import convert
from convert_retail_model_animations import clip_sample_rate
from inventory_retail_models import run_export, run_tmdl
from inventory_retail_sequence import retail_file
from package_retail_opening_sky import package as package_sky


OPENING_SHOT_COUNT = 3


def opening_shots(script: dict) -> tuple[int, list, dict]:
    camera_track, = script["camera_tracks"]
    voice, = [entry for entry in script["voice_instructions"]
              if entry["actor"] == 15 and entry["type"] == "vostream" and entry["opens_track"]]
    master = script["master_timing"]
    synchronization = [("wait", 1), ("wait", 2), ("resume", None)]
    synchronization += [("wait", 0), ("resume", None)] * OPENING_SHOT_COUNT
    if [(item["type"], item.get("flag")) for item in master[:len(synchronization)]] != synchronization:
        raise ValueError("unexpected SS01 opening synchronization")

    shots = []
    commands = {}
    for event in camera_track["events"]:
        if event["type"] == "command":
            commands[event["name"]] = event["arguments"]
            continue
        shot = {
            "clip": event["name"], "camera_bone": commands["bindCamera"][0],
            "visibility_row": commands["SetTerrain"][0],
            "horizontal_fov_degrees": float(commands["Set_FOV"][0]),
            "near_clip": float(commands["SetNearClip"][0]),
            "duration": master[3 + 2 * len(shots)]["duration"],
        }
        if (not 1.0 <= shot["horizontal_fov_degrees"] < 170.0 or not 0.0 < shot["near_clip"] < 280.0 or
                not 0.0 < shot["duration"] <= 60.0):
            raise ValueError("opening camera parameters are outside the PSP range")
        shots.append(shot)
        if len(shots) == OPENING_SHOT_COUNT:
            break
    if len(shots) != OPENING_SHOT_COUNT or len({shot["visibility_row"] for shot in shots}) != 1:
        raise ValueError("the opening preview needs all supported shots in the same visibility row")
    return camera_track["actor"], shots, voice


def package(tool: Path, install: Path, manifest: Path, output: Path) -> dict:
    sequence = json.loads(manifest.read_text(encoding="utf-8"))
    if sequence["sequence"] != "SS01_start":
        raise ValueError("expected the SS01_start inventory")
    actor, shots, voice = opening_shots(sequence["script"])

    # The source camera model comes from the SS01 entry of AScriptedSequenceActivity.
    model = retail_file(install, "Data/Models/SS01_cameraB.trb")
    info = run_export(tool, "-psp-animation-info", model, output / "camera-info.json")
    key_library, = [entry["symbol"] for entry in sequence["key_libraries"]
                    if entry["name"].casefold() == info["library"].casefold() and entry["symbol"]]
    pack = retail_file(install, sequence["sources"]["asset_pack"]["path"])
    for index, shot in enumerate(shots):
        clip_name = shot["clip"]
        clip, = [entry for entry in info["sequences"] if entry["name"] == clip_name]
        rate = clip_sample_rate(clip)
        if not rate or abs(clip["duration"] - shot["duration"]) > 0.01:
            raise ValueError(f"unexpected opening camera duration: {clip_name}")
        stem = "opening" if index == 0 else f"opening-{index}"
        dump = output / f"{stem}-dump.json"
        run_tmdl(tool, "-psp-animation", model, dump, "-keylib", str(pack), "-keylib-symbol", key_library,
                 "-sequence", clip_name, "-sample-rate", str(rate))
        camera = json.loads(dump.read_text(encoding="utf-8"))
        if camera["sequence"] != clip_name or camera["sample_rate"] != rate or camera["loop"]:
            raise ValueError("unexpected opening camera sample export")
        convert(dump, output / f"{stem}.pcam", shot["camera_bone"], preserve_roll=True)
        shot.update(sample_rate=rate, frames=len(camera["frames"]))
    sky = package_sky(tool, install, output)

    report = {"version": 2, "sequence": sequence["sequence"], "camera_actor": actor,
              "visibility_row": shots[0]["visibility_row"], "shots": shots, "voice": voice, "sky": sky}
    (output / "opening.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    payload = struct.pack("<4sIIi", b"PSCO", 2, len(shots), voice["index"])
    for shot in shots:
        payload += struct.pack("<3f", shot["horizontal_fov_degrees"], shot["near_clip"], shot["duration"])
    (output / "opening.psco").write_bytes(payload)
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", required=True, type=Path)
    parser.add_argument("install", type=Path)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        tool = args.tmdl.resolve(strict=True)
        (tool.parent / "Logs").mkdir(exist_ok=True)
        report = package(tool, args.install.resolve(strict=True), args.manifest, args.output.resolve())
    except (IndexError, KeyError, OSError, TypeError, ValueError, struct.error, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"Sequence opening packaging failed: {error}\n")
    print(f"Packaged {len(report['shots'])} SS01 opening shots using the {report['visibility_row']} row")


if __name__ == "__main__":
    main()
