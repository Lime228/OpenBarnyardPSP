#!/usr/bin/env python3
"""Package the retail actors animated in the supported SS01 opening shots."""

import argparse
import json
import re
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

from convert_animation import convert as convert_animation
from convert_retail_model_animations import clip_sample_rate
from convert_skeleton import convert as convert_skeleton
from inventory_retail_models import run_export, run_tmdl
from inventory_retail_sequence import retail_file
from package_retail_sequence_opening import opening_shots


MAX_ACTORS = 5
MAX_RESIDENT_ACTORS = 4
MAX_MESHES_PER_ACTOR = 2
MAX_SKIN_MESHES = 5
MAX_TEXTURES = 5
MAX_ANIMATED_VERTICES = 8192


def read_actor_names(source: Path) -> list[str]:
    table = re.search(r"ASSeqActorData::ms_aActorNames\[ NUM_ACTORS \] = \{([^}]+)\};",
                      source.read_text(encoding="utf-8"))
    if table is None:
        raise ValueError("could not read the engine actor IDs")
    return re.findall(r'"([A-Za-z0-9_]+)"', table[1])


def pack_clips(tool: Path, install: Path, pack: Path, libraries: dict, model: str, clips: dict,
               prefix: str, output: Path, scratch: Path, bind_bones: list | None = None) -> list:
    source = retail_file(install, f"Data/Models/{model}.trb")
    info = run_export(tool, "-psp-animation-info", source, scratch / "info.json")
    symbol = libraries[info["library"].casefold()]
    skeleton = scratch / "skeleton.json"
    dump = scratch / "clip.json"
    hierarchy = None
    for shot, (name, duration) in clips.items():
        clip, = [entry for entry in info["sequences"] if entry["name"] == name]
        rate = clip_sample_rate(clip)
        if not rate or clip["mode"] != 1 or abs(clip["duration"] - duration) > 0.01:
            raise ValueError(f"{model}: invalid duration or playback mode for {name}")
        run_tmdl(tool, "-psp-animation", source, dump, "-keylib", str(pack), "-keylib-symbol", symbol,
                 "-sequence", name, "-sample-rate", str(rate))
        document = json.loads(dump.read_text(encoding="utf-8"))
        if document["sequence"] != name or document["sample_rate"] != rate or document["loop"]:
            raise ValueError(f"{model}: different source animation exported for {name}")
        current = [(bone["name"], bone["parent"]) for bone in document["bones"]]
        if hierarchy is None:
            hierarchy = current
            if bind_bones is None:
                # The attachment rig never skins vertices. Its reference pose is
                # the first retail sample, and every animated frame sets all bones.
                bind_bones = [{**bone, **document["frames"][0][bone["name"]]} for bone in document["bones"]]
        if current != hierarchy or [(bone["name"], bone["parent"]) for bone in bind_bones] != current:
            raise ValueError(f"{model}: clip bones differ from the source model skeleton")
        skeleton.write_text(json.dumps({"bones": [
            {**bone, "parent": None if bone["parent"] is None else current[bone["parent"]][0]}
            for bone in bind_bones
        ]}), encoding="utf-8")
        convert_animation(skeleton, dump, output / f"{prefix}-{shot}.panm")
    if hierarchy is None:
        raise ValueError(f"{model}: no opening animation")
    if prefix == "actor-root":
        convert_skeleton(skeleton, output / f"{prefix}.pskl")
    return hierarchy


def package(tool: Path, install: Path, sequence: dict, models_path: Path,
            actor_names: list[str], output: Path) -> dict:
    script = sequence["script"]
    _, shots, _ = opening_shots(script)
    attachment, = script["attachment_tracks"]
    root, = [entry["arguments"][0] for entry in attachment["events"] if entry["name"] == "parentNode_Config"]
    sockets = {entry["arguments"][0].casefold(): entry["arguments"][1]
               for entry in attachment["events"] if entry["name"] == "attachToParentNode"}
    tracks = {actor_names[entry["actor"]].casefold(): {
        clip["after_pause"] - 1: (clip["clip"], shots[clip["after_pause"] - 1]["duration"])
        for clip in entry["clips"] if 1 <= clip["after_pause"] <= len(shots)
    } for entry in script["animation_tracks"] if 0 <= entry["actor"] < len(actor_names)}
    active = {name: clips for name, clips in tracks.items() if name in sockets and clips}
    if sequence["sequence"] != "SS01_start" or not 0 < len(active) <= MAX_ACTORS:
        raise ValueError("the opening exceeds the PSP actor limit")
    if len(tracks[root.casefold()]) != len(shots):
        raise ValueError("every shot must animate the opening attachment rig")

    manifest = json.loads((models_path / "sequence_models.json").read_text(encoding="utf-8"))
    if manifest["sequence"] != sequence["sequence"]:
        raise ValueError("actor models and script come from different sequences")
    models = {entry["model"].casefold(): entry for entry in manifest["models"]}
    for shot in range(len(shots)):
        names = [name for name, clips in active.items() if shot in clips]
        meshes = [mesh for name in names for mesh in models[name]["meshes"]]
        if len(names) > MAX_RESIDENT_ACTORS or len(meshes) > MAX_SKIN_MESHES:
            raise ValueError(f"shot {shot + 1} exceeds the resident PSP actor or skin mesh limit")
        if sum(mesh["vertices"] for mesh in meshes) > MAX_ANIMATED_VERTICES:
            raise ValueError(f"shot {shot + 1} exceeds the PSP animated vertex limit")
    pack = retail_file(install, sequence["sources"]["asset_pack"]["path"])
    libraries = {entry["name"].casefold(): entry["symbol"] for entry in sequence["key_libraries"] if entry["symbol"]}
    output.mkdir(parents=True, exist_ok=True)
    actors = []
    textures = {}
    payload = bytearray(struct.pack("<4sIII", b"PSAC", 1, len(active), len(shots)))
    with tempfile.TemporaryDirectory(dir=output) as temporary:
        work = Path(temporary)
        runtime = work / "Actors"
        runtime.mkdir()
        rig = pack_clips(tool, install, pack, libraries, root, tracks[root.casefold()], "actor-root", runtime, work)
        rig_bones = {bone.casefold(): index for index, (bone, _) in enumerate(rig)}
        for index, (name, clips) in enumerate(active.items()):
            model = models[name]
            if (model["kind"] != "skin" or model["status"] != "converted" or
                    not 0 < len(model["meshes"]) <= MAX_MESHES_PER_ACTOR):
                raise ValueError(f"{name}: expected one or two converted skin meshes")
            directory = models_path / model["directory"]
            prefix = f"actor-{index}"
            bind_bones = json.loads((directory / "model.json").read_text(encoding="utf-8"))["skeleton"]
            pack_clips(tool, install, pack, libraries, name, clips, prefix, runtime, work, bind_bones)
            shutil.copyfile(directory / model["skeleton"], runtime / f"{prefix}.pskl")
            indices = []
            for mesh_index, mesh in enumerate(model["meshes"]):
                texture = mesh["texture_path"]
                if texture not in textures:
                    if len(textures) == MAX_TEXTURES:
                        raise ValueError("the opening exceeds the resident PSP actor texture limit")
                    textures[texture] = len(textures)
                    shutil.copyfile(models_path / texture, runtime / f"texture-{textures[texture]}.ptex")
                indices.append(textures[texture])
                shutil.copyfile(directory / mesh["path"], runtime / f"{prefix}-{mesh_index}.pmes")
            payload.extend(struct.pack("<5I", rig_bones[sockets[name].casefold()], sum(1 << shot for shot in clips),
                                       len(indices), *indices, *([0] * (MAX_MESHES_PER_ACTOR - len(indices)))))
            actors.append({"model": name, "socket": sockets[name], "clips": clips, "meshes": len(indices)})
        (runtime / "actors.psac").write_bytes(payload)
        files = {path.name: path.stat().st_size for path in sorted(runtime.iterdir())}
        destination = output / "Actors"
        destination.mkdir(exist_ok=True)
        for name in files:
            (runtime / name).replace(destination / name)
        report = {"version": 1, "sequence": sequence["sequence"], "root": root, "actors": actors, "files": files}
        pending = work / "opening_actors.json"
        pending.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        pending.replace(output / "opening_actors.json")
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", required=True, type=Path)
    parser.add_argument("install", type=Path)
    parser.add_argument("sequence", type=Path)
    parser.add_argument("models", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        tool = args.tmdl.resolve(strict=True)
        (tool.parent / "Logs").mkdir(exist_ok=True)
        engine = Path(__file__).resolve().parents[3] / "OpenBarnyard/Source/Units/ASSeqActorData.cpp"
        sequence = json.loads(args.sequence.read_text(encoding="utf-8"))
        report = package(tool, args.install.resolve(strict=True), sequence, args.models, read_actor_names(engine),
                         args.output.resolve())
    except (IndexError, KeyError, OSError, TypeError, ValueError, struct.error, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"Opening actor packaging failed: {error}\n")
    print(f"Packaged {len(report['actors'])} SS01 opening actors ({sum(report['files'].values())} bytes)")


if __name__ == "__main__":
    main()
