#!/usr/bin/env python3
"""Prepare the first shot from the original scripted sequence camera track."""

import argparse
import json
import struct
import subprocess
from pathlib import Path

from convert_camera import convert
from convert_retail_model_animations import clip_sample_rate
from inventory_retail_models import run_export, run_tmdl
from inventory_retail_sequence import retail_file


def package(tool: Path, install: Path, manifest: Path, output: Path) -> dict:
    sequence = json.loads(manifest.read_text(encoding="utf-8"))
    if sequence["sequence"] != "SS01_start":
        raise ValueError("expected the SS01_start inventory")
    camera_track, = sequence["script"]["camera_tracks"]
    commands = {}
    for event in camera_track["events"]:
        if event["type"] == "animation":
            clip_name = event["name"]
            break
        commands[event["name"]] = event["arguments"]
    else:
        raise ValueError("opening camera has no animation")

    camera_bone = commands["bindCamera"][0]
    terrain = commands["SetTerrain"][0]
    fov = float(commands["Set_FOV"][0])
    near = float(commands["SetNearClip"][0])
    if not 1.0 <= fov < 170.0 or not 0.0 < near < 280.0:
        raise ValueError("opening camera projection is outside the PSP range")

    # The source camera model comes from the SS01 entry of AScriptedSequenceActivity.
    model = retail_file(install, "Data/Models/SS01_cameraB.trb")
    info = run_export(tool, "-psp-animation-info", model, output / "camera-info.json")
    clip, = [entry for entry in info["sequences"] if entry["name"] == clip_name]
    rate = clip_sample_rate(clip)
    if not rate:
        raise ValueError(f"opening camera clip cannot fit the PSP sample limit: {clip_name}")
    key_library, = [entry["symbol"] for entry in sequence["key_libraries"]
                    if entry["name"].casefold() == info["library"].casefold() and entry["symbol"]]
    pack = retail_file(install, sequence["sources"]["asset_pack"]["path"])
    dump = output / "camera-dump.json"
    run_tmdl(tool, "-psp-animation", model, dump, "-keylib", str(pack), "-keylib-symbol", key_library,
             "-sequence", clip_name, "-sample-rate", str(rate))
    camera = json.loads(dump.read_text(encoding="utf-8"))
    if camera["sequence"] != clip_name or camera["sample_rate"] != rate or camera["loop"]:
        raise ValueError("unexpected opening camera sample export")
    convert(dump, output / "opening.pcam", camera_bone, preserve_roll=True)

    report = {"version": 1, "sequence": sequence["sequence"], "camera_actor": camera_track["actor"],
              "camera_bone": camera_bone, "clip": clip_name, "visibility_row": terrain,
              "horizontal_fov_degrees": fov, "near_clip": near, "sample_rate": rate, "frames": len(camera["frames"])}
    (output / "opening.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    (output / "opening.psco").write_bytes(struct.pack("<4sIff", b"PSCO", 1, fov, near))
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
    except (KeyError, OSError, TypeError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"Sequence opening packaging failed: {error}\n")
    print(f"Packaged {report['clip']} using the {report['visibility_row']} row ({report['frames']} camera samples)")


if __name__ == "__main__":
    main()
