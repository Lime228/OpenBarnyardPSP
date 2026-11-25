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


OPENING_SHOT_COUNT = 16


def camera_shots(script: dict, limit: int | None = None) -> tuple[int, list]:
    camera_track, = script["camera_tracks"]
    master = script["master_timing"]
    synchronization = [("wait", 1), ("wait", 2), ("resume", None)]
    if [(item["type"], item.get("flag")) for item in master[:3]] != synchronization:
        raise ValueError("unexpected SS01 opening synchronization")

    intervals = [[]]
    for event in master[3:]:
        if event["type"] == "resume":
            intervals.append([])
        elif event["type"] == "wait":
            intervals[-1].append(event)
        else:
            raise ValueError("unexpected SS01 timing instruction")
    ui_by_shot = {cue["after_resume"]: cue["arguments"][0] for cue in script.get("master_cues", [])
                  if cue["type"] == "command" and cue["name"] == "UI"}

    shots = []
    commands = {}
    for event in camera_track["events"]:
        if event["type"] == "command":
            commands[event["name"]] = event["arguments"]
            continue
        index = len(shots)
        if index >= len(intervals) - 1:
            raise ValueError("unexpected SS01 opening synchronization")
        durations = [wait["duration"] for wait in intervals[index] if wait["flag"] == 0]
        wait_flags = [wait["flag"] for wait in intervals[index] if wait["flag"] != 0]
        ui = ui_by_shot.get(index + 1)
        if (ui and intervals[index]) or (not ui and len(durations) != 1):
            raise ValueError("unexpected SS01 opening synchronization")
        shot = {
            "clip": event["name"], "camera_bone": commands["bindCamera"][0],
            "visibility_row": commands["SetTerrain"][0],
            "horizontal_fov_degrees": float(commands["Set_FOV"][0]),
            "near_clip": float(commands["SetNearClip"][0]),
            "duration": None if ui else durations[0],
            "wait_flags": wait_flags, "ui": ui,
        }
        if (not 1.0 <= shot["horizontal_fov_degrees"] < 170.0 or not 0.0 < shot["near_clip"] < 280.0 or
                (ui is None and not 0.0 < shot["duration"] <= 60.0)):
            raise ValueError("opening camera parameters are outside the PSP range")
        shots.append(shot)
        if len(shots) == limit:
            break
    if limit is None and (len(intervals) != len(shots) + 1 or intervals[-1]):
        raise ValueError("the master and camera track counts differ")
    return camera_track["actor"], shots


def opening_shots(script: dict, limit: int = OPENING_SHOT_COUNT) -> tuple[int, list, dict]:
    actor, shots = camera_shots(script, limit)
    voice, = [entry for entry in script["voice_instructions"]
              if entry["actor"] == 15 and entry["type"] == "vostream" and entry["opens_track"]]
    if (len(shots) != limit or len({shot["visibility_row"] for shot in shots}) != 1 or
            any(shot["ui"] or (shot["wait_flags"] and (i + 1 != limit or shot["wait_flags"] != [3]))
                for i, shot in enumerate(shots))):
        raise ValueError("the opening preview needs all supported timed shots in the same visibility row")
    return actor, shots, voice


def split_at_character_setup(shots: list) -> tuple[int, list]:
    gate, = [index for index, shot in enumerate(shots) if shot["ui"] == "Player_select"]
    return gate, shots[gate + 1:]


def package_continuation(camera_manifest: Path, output: Path) -> dict:
    camera = json.loads(camera_manifest.read_text(encoding="utf-8"))
    if camera["sequence"] != "SS01_start":
        raise ValueError("expected the SS01_start camera sequence")
    gate, shots = split_at_character_setup(camera["shots"])
    voice, = [entry for entry in camera["voice_instructions"]
              if entry["actor"] == 15 and entry["type"] == "vostream" and not entry["opens_track"]]
    if (not 2 <= len(shots) <= OPENING_SHOT_COUNT or
            any(shot["ui"] or shot["loop"] or not shot["duration"] or
                shot["visibility_row"] not in {"Barn", "BeadyIntDay"} for shot in shots)):
        raise ValueError("unsupported SS01 timeline after Player_select")
    mask = sum(1 << i for i, shot in enumerate(shots) if shot["visibility_row"] == "BeadyIntDay")
    if not mask or mask & 1:
        raise ValueError("SS01 must start in Barn and visit BeadyIntDay after Player_select")

    output.mkdir(parents=True, exist_ok=True)
    payload = bytearray(struct.pack("<4sIIi", b"PSCO", 3, len(shots), voice["index"]))
    files = {}
    for index, shot in enumerate(shots):
        source = f"opening-{gate + 1 + index}.pcam"
        if shot["file"] != source:
            raise ValueError(f"unexpected SS01 camera file: {shot['file']}")
        filename = f"opening-{index}.pcam" if index else "opening.pcam"
        data = (camera_manifest.parent / source).read_bytes()
        (output / filename).write_bytes(data)
        files[filename] = len(data)
        payload.extend(struct.pack("<3f", shot["horizontal_fov_degrees"], shot["near_clip"], shot["duration"]))
    payload.extend(struct.pack("<I", mask))
    (output / "opening.psco").write_bytes(payload)
    files["opening.psco"] = len(payload)
    report = {"version": 3, "sequence": camera["sequence"], "voice": voice, "shots": shots, "files": files}
    (output / "continuation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def export_cameras(tool: Path, install: Path, sequence: dict, shots: list, output: Path) -> None:
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
        loop = shot["ui"] is not None
        if not rate or clip["mode"] != int(not loop) or (
                not loop and abs(clip["duration"] - shot["duration"]) > 0.01):
            raise ValueError(f"unexpected camera duration or loop mode: {clip_name}")
        stem = "opening" if index == 0 else f"opening-{index}"
        dump = output / f"{stem}-dump.json"
        run_tmdl(tool, "-psp-animation", model, dump, "-keylib", str(pack), "-keylib-symbol", key_library,
                 "-sequence", clip_name, "-sample-rate", str(rate))
        camera = json.loads(dump.read_text(encoding="utf-8"))
        if camera["sequence"] != clip_name or camera["sample_rate"] != rate or camera["loop"] != loop:
            raise ValueError("unexpected opening camera sample export")
        filename = f"{stem}.pcam"
        convert(dump, output / filename, shot["camera_bone"], preserve_roll=True)
        shot.update(file=filename, sample_rate=rate, frames=len(camera["frames"]), loop=loop)


def package(tool: Path, install: Path, manifest: Path, output: Path, all_shots: bool = False) -> dict:
    sequence = json.loads(manifest.read_text(encoding="utf-8"))
    if sequence["sequence"] != "SS01_start":
        raise ValueError("expected the SS01_start inventory")
    script = sequence["script"]
    if all_shots:
        actor, shots = camera_shots(script)
        export_cameras(tool, install, sequence, shots, output)
        report = {"version": 1, "sequence": sequence["sequence"], "camera_actor": actor, "shots": shots,
                  "master_cues": script["master_cues"], "voice_instructions": script["voice_instructions"]}
        (output / "camera_sequence.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        return report

    actor, shots, voice = opening_shots(script)
    export_cameras(tool, install, sequence, shots, output)
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
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--all-shots", action="store_true", help="export every retail camera without staging a runtime timeline")
    mode.add_argument("--after-character-setup", type=Path, metavar="CAMERA_MANIFEST",
                      help="stage the second narration timeline from an existing complete camera export")
    parser.add_argument("--tmdl", type=Path)
    parser.add_argument("install", type=Path, nargs="?")
    parser.add_argument("manifest", type=Path, nargs="?")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        if args.after_character_setup is not None:
            if args.install is not None:
                parser.error("the continuation only takes the camera manifest option and output directory")
            report = package_continuation(args.after_character_setup, args.output)
        else:
            if args.tmdl is None or args.install is None or args.manifest is None:
                parser.error("camera export needs --tmdl, the install directory and the sequence manifest")
            tool = args.tmdl.resolve(strict=True)
            (tool.parent / "Logs").mkdir(exist_ok=True)
            report = package(tool, args.install.resolve(strict=True), args.manifest, args.output.resolve(), args.all_shots)
    except (IndexError, KeyError, OSError, TypeError, ValueError, struct.error, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"Sequence opening packaging failed: {error}\n")
    if args.all_shots or args.after_character_setup is not None:
        print(f"Exported {len(report['shots'])} original SS01 cameras")
    else:
        print(f"Packaged {len(report['shots'])} SS01 opening shots using the {report['visibility_row']} row")


if __name__ == "__main__":
    main()
