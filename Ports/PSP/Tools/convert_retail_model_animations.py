#!/usr/bin/env python3
"""Resolve installed model key libraries and bake every supported retail clip to PANM."""

import argparse
import hashlib
import json
import math
import struct
import subprocess
import tempfile
from collections import Counter, defaultdict
from pathlib import Path

from convert_animation import MAX_FRAME_COUNT, convert
from convert_skeleton import MAX_BONE_COUNT
from inventory_retail_models import run_export


KEY_COMPONENTS = ("translations", "quaternions", "scales")
EXPORT_ERRORS = (KeyError, OSError, ValueError, struct.error, subprocess.TimeoutExpired)


def key_ranges_fit(model: dict, library: dict) -> bool:
    for component in KEY_COMPONENTS:
        base, count = model[component]
        if base < 0 or count < 0 or base + count > library[component]:
            return False
    return True


def select_library(model: dict, candidates: list[dict]) -> tuple[str, list[dict]]:
    matches = [library for library in candidates if key_ranges_fit(model, library)]
    if not matches:
        return ("incompatible-library" if candidates else "missing-library"), []
    local = Path(model["source"]).with_suffix(".tkl").as_posix().casefold()
    for library in matches:
        if library["source"].casefold() == local:
            return "resolved", [library]
    if len({library["sha256"] for library in matches}) != 1:
        return "ambiguous-library", matches
    return "resolved", matches


def clip_sample_rate(clip: dict) -> int:
    duration = clip["duration"]
    if clip["overlay"] or clip["mode"] not in (0, 1) or not math.isfinite(duration) or duration <= 0:
        return 0
    intervals = MAX_FRAME_COUNT - int(clip["mode"] == 1)
    return min(30, math.floor(intervals / duration))


def bake_clip(tool: Path, model: Path, library: Path, clip: dict, rate: int, destination: Path, scratch: Path) -> dict:
    dump = scratch / "clip.json"
    dump.unlink(missing_ok=True)
    process = subprocess.run(
        [str(tool), "-psp-animation", str(model), "-keylib", str(library), "-sequence", clip["name"],
         "-sample-rate", str(rate), "-o", str(dump)],
        cwd=tool.parent, capture_output=True, timeout=60, check=False,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    if process.returncode:
        diagnostic = (process.stdout + process.stderr)[-600:].decode(errors="replace").strip()
        raise ValueError(f"TMDL exited with {process.returncode}: {diagnostic}")
    document = json.loads(dump.read_text(encoding="utf-8"))
    if document["sequence"] != clip["name"] or document["loop"] != (clip["mode"] == 0) or document["sample_rate"] != rate:
        raise ValueError("TMDL selected a different source sequence")
    source_bones = document["bones"]
    bones = []
    for index, bone in enumerate(source_bones):
        parent = bone["parent"]
        if parent is not None and not 0 <= parent < index:
            raise ValueError("skeleton parent must precede its child")
        bones.append({"name": bone["name"], "parent": None if parent is None else source_bones[parent]["name"]})
    skeleton = scratch / "skeleton.json"
    skeleton.write_text(json.dumps({"bones": bones}), encoding="utf-8")
    pending = scratch / "clip.panm"
    convert(skeleton, dump, pending)
    size = pending.stat().st_size
    pending.replace(destination)
    frames = len(document["frames"])
    return {"path": destination.name, "frames": frames, "sample_rate": rate, "bytes": size,
            "runtime_duration": (frames - int(clip["mode"] == 1)) / rate}


def read_animation_info(tool: Path, install: Path, source: str, metadata: Path) -> dict:
    path = (install / source).resolve(strict=True)
    if not path.is_relative_to(install):
        raise ValueError(f"invalid animation source: {source}")
    target = metadata / Path(source).with_suffix(path.suffix + ".json")
    return {"source": source, **run_export(tool, "-psp-animation-info", path, target)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", type=Path, required=True)
    parser.add_argument("install", type=Path)
    parser.add_argument("inventory", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    tool = args.tmdl.resolve(strict=True)
    install = args.install.resolve(strict=True)
    output = args.output.resolve()
    inventory = json.loads(args.inventory.read_text(encoding="utf-8"))
    if output.is_relative_to(install) or inventory.get("version") != 1:
        parser.error("use a model inventory and an output directory outside the installation")

    metadata_dir = output / "Metadata"
    libraries = []
    by_id = defaultdict(list)
    paths = (path for path in install.rglob("*") if path.suffix.casefold() == ".tkl" and path.is_file())
    for path in sorted(paths, key=lambda path: path.as_posix().casefold()):
        source = path.relative_to(install).as_posix()
        try:
            entry = read_animation_info(tool, install, source, metadata_dir)
            with path.open("rb") as stream:
                entry["sha256"] = hashlib.file_digest(stream, "sha256").hexdigest()
            by_id[entry["library"].casefold()].append(entry)
        except EXPORT_ERRORS as error:
            entry = {"source": source, "error": str(error)}
        libraries.append(entry)

    models = []
    clip_counts = Counter()
    for record in inventory["records"]:
        if not {"Skeleton", "SkeletonHeader"} <= set(record.get("symbols", [])):
            continue
        source = (Path("Data/Models") / record["path"]).as_posix()
        try:
            model = read_animation_info(tool, install, source, metadata_dir)
        except EXPORT_ERRORS as error:
            models.append({"source": source, "status": "unreadable", "error": str(error)})
            continue

        clips = model["sequences"]
        status, matches = select_library(model, by_id[model["library"].casefold()])
        model["status"] = status if clips else "no-clips"
        model["key_libraries"] = [library["source"] for library in matches]
        models.append(model)
        if not clips:
            continue
        if status != "resolved" or not 0 < model["bones"] <= MAX_BONE_COUNT:
            if status == "resolved":
                model["status"] = "bone-limit"
            for clip in clips:
                clip["status"] = model["status"]
            clip_counts[model["status"]] += len(clips)
            continue

        destination = output / Path(source).with_suffix("")
        destination.mkdir(parents=True, exist_ok=True)
        duplicate_names = Counter(clip["name"].casefold() for clip in clips)
        with tempfile.TemporaryDirectory(prefix="clip-", dir=destination) as directory:
            scratch = Path(directory)
            for index, clip in enumerate(clips):
                rate = clip_sample_rate(clip)
                if not rate:
                    clip["status"] = "overlay" if clip["overlay"] else "unsupported-duration-or-mode"
                elif not clip["name"] or duplicate_names[clip["name"].casefold()] != 1:
                    clip["status"] = "missing-or-duplicate-name"
                else:
                    try:
                        clip.update(bake_clip(tool, install / source, install / matches[0]["source"], clip, rate,
                                              destination / f"clip-{index:03}.panm", scratch))
                        clip["status"] = "converted"
                    except EXPORT_ERRORS as error:
                        clip.update(status="rejected", error=str(error))
                clip_counts[clip["status"]] += 1
        model["directory"] = destination.relative_to(output).as_posix()

    model_counts = Counter(model["status"] for model in models)
    report = {"version": 1, "model_counts": dict(sorted(model_counts.items())),
              "clip_counts": dict(sorted(clip_counts.items())), "libraries": libraries, "models": models}
    output.mkdir(parents=True, exist_ok=True)
    pending = output / "model_animations.json.tmp"
    pending.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    pending.replace(output / "model_animations.json")
    print(f"{len(libraries)} libraries, {len(models)} models, {sum(clip_counts.values())} clips")
    print(dict(sorted(clip_counts.items())))
    return int(any("error" in library for library in libraries) or model_counts["unreadable"] or clip_counts["rejected"])


if __name__ == "__main__":
    raise SystemExit(main())
