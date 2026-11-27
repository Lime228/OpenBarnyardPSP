#!/usr/bin/env python3
"""Bake the original resident story animal models and base animations for PSP."""

import argparse
import json
import struct
import subprocess
import tempfile
from pathlib import Path

from convert_dds import convert as convert_texture
from convert_frontend_instance_pack import load_source_meshes
from convert_frontend_terrain_pack import write_model
from convert_retail_model_animations import bake_clip, clip_sample_rate
from convert_retail_model_geometry import bake_geometry
from inventory_retail_models import run_export, run_tmdl
from inventory_retail_sequence import main_properties, retail_file
from inventory_retail_sim_population import POPULATION, read_population


def bake_marker(tool: Path, install: Path, actor: str, output: Path, scratch: Path) -> list[str]:
    model = retail_file(install, "Data/Models/Exclamation.trb")
    dump = run_export(tool, "-psp-dump", model, scratch / "marker-model.json")
    if len(dump["skeleton"]) != 1:
        raise ValueError("expected the original rigid quest marker")
    mesh, = load_source_meshes(dump)
    vertices = []
    indices = []
    for submesh in mesh["submeshes"]:
        if len(vertices) + len(submesh["vertices"]) > 65535:
            raise ValueError("original quest marker exceeded the mesh index limit")
        indices.extend(len(vertices) + index for index in submesh["indices"])
        # AAnimalPopulationManager rotates the authored marker by +90 degrees about X.
        vertices.extend((uv, (x, -z, y), colour) for uv, (x, y, z), colour in submesh["vertices"])
    write_model({"vertices": vertices, "indices": indices}, output / "exclamation.pmes")
    library = retail_file(install, "Data/Matlibs/MiscObj.ttl")
    dds = scratch / "marker.dds"
    run_tmdl(tool, "-psp-texture", library, dds, "-texture", mesh["texture"])
    convert_texture(dds, output / "exclamation.ptex")

    units = retail_file(install, "Data/Units/AllUnitTypes.trz")
    types = dict(main_properties(run_export(tool, "-psp-properties", units, scratch / "unit-types.json")))
    properties = dict(types[actor]["properties"])
    height = dict(properties["aunitcollisioncomponentdata"]["properties"])["Height"]
    if not 0 < height < 8.0:
        raise ValueError("invalid original NPC collision height")
    (output / "exclamation.psmk").write_bytes(struct.pack("<4sIf", b"PSMK", 1, height))
    return ["exclamation.pmes", "exclamation.ptex", "exclamation.psmk"]


def bake_animal(tool: Path, install: Path, spawn: dict, output: Path) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    actor = spawn["type"]
    with tempfile.TemporaryDirectory(prefix="animal-", dir=output) as directory:
        scratch = Path(directory)
        descriptor = retail_file(install, f"Data/ModelTypes/{actor}.trb")
        props = dict(main_properties(run_export(tool, "-psp-properties", descriptor, scratch / "model-type.json")))
        declaration = props["AnimatableObjectType"]["properties"]
        model = retail_file(install, f"Data/Models/{dict(declaration)['Model']}.trb")
        info = run_export(tool, "-psp-animation-info", model, scratch / "animation-info.json")
        library_id = info["library"]
        keys = retail_file(install, f"Data/Models/{library_id}.tkl")
        textures = retail_file(install, f"Data/Matlibs/{library_id}.ttl")
        dump = run_export(tool, "-psp-dump", model, scratch / "model.json")
        geometry = bake_geometry(dump, output)
        for mesh in geometry["meshes"]:
            target = (output / mesh["path"]).with_suffix(".ptex")
            dds = scratch / "mesh.dds"
            run_tmdl(tool, "-psp-texture", textures, dds, "-texture", mesh["texture"])
            convert_texture(dds, target, max_width=128)
            mesh["texture_file"] = target.name

        clips = {}
        for index, clip in enumerate(info["sequences"]):
            rate = clip_sample_rate(clip)
            if rate:
                target = output / f"clip-{index:03}.panm"
                clips[clip["name"].casefold()] = bake_clip(tool, model, keys, clip, rate, target, scratch,
                                                          dump["skeleton"])

        animation_sets = [entry["properties"] for key, entry in declaration if key == "NamedAnimationSet"]
        default, = [entry for entry in animation_sets if dict(entry)["Name"] == "default"]
        names = {dict(entry["properties"])["Name"]: dict(entry["properties"])["ExportedName"]
                 for key, entry in default if key == "NamedAnimation"}
        idle = clips[names["idle_lp"].casefold()]["path"]
        names_on_disk = [geometry["skeleton"], *[clip["path"] for clip in clips.values()]]
        for mesh in geometry["meshes"]:
            names_on_disk.extend((mesh["path"], mesh["texture_file"]))
        marker_files = bake_marker(tool, install, actor, output, scratch)
        names_on_disk.extend(marker_files)
        return {
            "spawn": spawn, "model": model.relative_to(install).as_posix(),
            "idle": idle, "animations": names, "clips": clips, "marker_files": marker_files, **geometry,
            "files": {name: (output / name).stat().st_size for name in names_on_disk},
        }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", type=Path, required=True)
    parser.add_argument("install", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        tool = args.tmdl.resolve(strict=True)
        (tool.parent / "Logs").mkdir(exist_ok=True)
        install = args.install.resolve(strict=True)
        output = args.output.resolve()
        output.mkdir(parents=True, exist_ok=True)
        source = retail_file(install, POPULATION)
        with tempfile.TemporaryDirectory(prefix="spawns-", dir=output) as directory:
            symbols = run_export(tool, "-psp-symbols", source, Path(directory) / "symbols.json")
            records = read_population(source, symbols)["quest_tier_spawns"]
        spawn, = [record for record in records if record["absolute_tier"] == 1 and record["quest"] == "Miles Welcome"]
        actor = bake_animal(tool, install, spawn, output / spawn["name"])
        manifest = output / "sim_animals.json"
        pending = manifest.with_suffix(".tmp")
        pending.write_text(json.dumps({"version": 1, "source": POPULATION, "animals": [actor]}, indent=2) + "\n",
                           encoding="utf-8")
        pending.replace(manifest)
    except (IndexError, KeyError, OSError, ValueError, struct.error, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"Story animal package failed: {error}\n")
    print(f"Baked {len(actor['meshes'])} original meshes and {len(actor['clips'])} base clips for {spawn['name']}")


if __name__ == "__main__":
    main()
