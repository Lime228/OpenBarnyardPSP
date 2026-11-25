#!/usr/bin/env python3
"""Bake and add the original initial EnvMain static instances to the terrain pack."""

import argparse
import json
import shutil
import struct
import subprocess
from pathlib import Path

from convert_dds import convert as convert_texture
from convert_frontend_instance_pack import convert as convert_instance
from convert_retail_sequence_models import texture_key
from finalize_frontend_terrain_pack import finalize
from inventory_retail_models import run_export, run_tmdl
from inventory_retail_sequence import retail_file
from package_sequence_terrain import package as package_terrain


def package(tool: Path, install: Path, workspace: Path, max_width: int) -> dict:
    inventory = workspace / "envmain-instances.json"
    instances = json.loads(inventory.read_text(encoding="utf-8"))
    sections = set(instances["resident_section_names"])
    models = {entry["model_name"]: entry["model_path"] for entry in instances["instances"]
              if entry["vis_section_name"] in sections}
    if instances["main_game_model_count"] <= 0 or len(models) != instances["resident_used_model_count"]:
        raise ValueError("expected the original initial EnvMain instance inventory")

    intermediate = workspace / "Instances"
    material_source = retail_file(install, "Data/MiscTextures/MaterialProperties.trb")
    properties = intermediate / "material-properties.json"
    run_export(tool, "-psp-material-properties", material_source, properties)
    textures = {}
    # Prefer the EnvMain common library for textures also present in the startup pack.
    for path, options in (("Data/Assets/lib_startup.trb", ("-texture-symbol", "instances_TTL")),
                          ("Data/Terrain/EnvMain/envmain_Common.ttl", ())):
        library = retail_file(install, path)
        atlas = run_export(tool, "-psp-textures", library, intermediate / f"{library.stem}-textures.json", *options)
        for texture in atlas["textures"]:
            textures[texture_key(texture["name"])] = (library, options, texture["name"])

    cache = intermediate / "Textures"
    cache.mkdir(exist_ok=True)
    converted_textures = {}
    terrain_source = workspace / "Terrain_Barn/EnvMain_terrain.json"
    terrain = json.loads(terrain_source.read_text(encoding="utf-8-sig"))
    if terrain["terrain"] != "EnvMain" or terrain["visibility_row"] != "Barn":
        raise ValueError("expected the original EnvMain Barn terrain plan")
    for entry in terrain["packs"]:
        entry["manifest"] = (terrain_source.parent / entry["manifest"]).relative_to(workspace).as_posix()

    original_packs = len(terrain["packs"])
    total_instances = 0
    for name, source in sorted(models.items()):
        output = intermediate / name
        prefix = f"envmain_instances_{name.casefold()}"
        dump = output / "model.json"
        run_export(tool, "-psp-dump", retail_file(install, source), dump)
        manifest = convert_instance(inventory, dump, output, name, prefix)
        modes = []
        for texture in manifest["textures"]:
            key = texture_key(texture["source_texture"])
            if key not in converted_textures:
                library, options, source_name = textures[key]
                output_texture = cache / f"{len(converted_textures):03}.ptex"
                dds = cache / "texture.dds"
                run_tmdl(tool, "-psp-texture", library, dds, "-texture", source_name, *options)
                details = convert_texture(dds, output_texture, max_width=max_width)
                converted_textures[key] = (output_texture, details["alpha_mode"])
            source_texture, alpha = converted_textures[key]
            shutil.copyfile(source_texture, output / texture["texture"])
            modes.append(alpha)

        filename = output / f"{prefix}.json"
        finalize(filename, properties, filename.with_suffix(".pcfg"), modes)
        terrain["packs"].append({"source": source, "section": "Barn", "lod": "instance", "glow": False,
                                 "manifest": filename.relative_to(workspace).as_posix()})
        total_instances += manifest["source_instances"]

    if total_instances != instances["resident_instance_count"]:
        raise ValueError("converted instance count differs from the original visible locators")
    terrain["converted"] = len(terrain["packs"])
    combined = workspace / "envmain-barn-with-instances.json"
    combined.write_text(json.dumps(terrain, indent=2) + "\n", encoding="utf-8")
    destination = workspace / "TerrainPack_BarnWithInstances"
    report = package_terrain(combined, destination, envmain_dawn=True)
    report["instance_count"] = total_instances
    report["instance_model_count"] = len(models)
    report["instance_draw_groups"] = sum(entry["meshes"] for entry in report["packs"][original_packs:])
    (destination / "terrain.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", required=True, type=Path)
    parser.add_argument("--max-width", type=int, default=64)
    parser.add_argument("install", type=Path)
    parser.add_argument("workspace", type=Path)
    args = parser.parse_args()
    try:
        tool = args.tmdl.resolve(strict=True)
        (tool.parent / "Logs").mkdir(exist_ok=True)
        report = package(tool, args.install.resolve(strict=True), args.workspace.resolve(strict=True), args.max_width)
    except (KeyError, OSError, ValueError, struct.error, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"EnvMain instance packaging failed: {error}\n")
    print(f"Packed {report['instance_count']} instances into {report['instance_draw_groups']} draw groups, "
          f"{report['mesh_count']} total meshes and {report['texture_count']} shared textures.")


if __name__ == "__main__":
    main()
