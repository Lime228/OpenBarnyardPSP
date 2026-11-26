#!/usr/bin/env python3
"""Bake the original static instances for an EnvMain visibility row."""

import argparse
import json
import shutil
import struct
import subprocess
import sys
from pathlib import Path

from convert_dds import convert as convert_texture
from convert_frontend_instance_pack import convert as convert_instance
from convert_retail_sequence_models import texture_key
from finalize_frontend_terrain_pack import finalize
from inventory_retail_models import run_export, run_tmdl
from inventory_retail_sequence import retail_file
from package_sequence_terrain import package as package_terrain


class InstanceSources:
    def __init__(self, tool: Path, install: Path, workspace: Path, max_width: int):
        self.tool = tool
        self.install = install
        self.max_width = max_width
        self.cache = workspace / "InstanceSources"
        self.cache.mkdir(parents=True, exist_ok=True)
        self.properties = self.cache / "material-properties.json"
        material_source = retail_file(install, "Data/MiscTextures/MaterialProperties.trb")
        run_export(tool, "-psp-material-properties", material_source, self.properties)
        self.textures = {}
        # Prefer the EnvMain common library for textures also present in the startup pack.
        for path, options in (("Data/Assets/lib_startup.trb", ("-texture-symbol", "instances_TTL")),
                              ("Data/Terrain/EnvMain/envmain_Common.ttl", ())):
            library = retail_file(install, path)
            atlas = run_export(tool, "-psp-textures", library, self.cache / f"{library.stem}-textures.json", *options)
            for texture in atlas["textures"]:
                self.textures[texture_key(texture["name"])] = (library, options, texture["name"])
        self.model_dumps = {}
        self.converted_textures = {}

    def model_dump(self, source: str) -> Path:
        if source not in self.model_dumps:
            dump = self.cache / f"model-{len(self.model_dumps):03}.json"
            run_export(self.tool, "-psp-dump", retail_file(self.install, source), dump)
            self.model_dumps[source] = dump
        return self.model_dumps[source]

    def texture(self, name: str) -> tuple[Path, str]:
        key = texture_key(name)
        if key not in self.converted_textures:
            library, options, source = self.textures[key]
            output = self.cache / f"texture-{len(self.converted_textures):03}.ptex"
            dds = self.cache / "texture.dds"
            run_tmdl(self.tool, "-psp-texture", library, dds, "-texture", source, *options)
            details = convert_texture(dds, output, max_width=self.max_width, resample_to_power_of_two=True)
            self.converted_textures[key] = (output, details["alpha_mode"])
        return self.converted_textures[key]


def package(workspace: Path, sources: InstanceSources, row: str) -> dict:
    if not row.isascii() or not row.replace("_", "").isalnum():
        raise ValueError(f"invalid EnvMain visibility row name: {row}")
    inventory = workspace / ("envmain-instances.json" if row == "Barn" else f"envmain-{row}-instances.json")
    instances = json.loads(inventory.read_text(encoding="utf-8"))
    sections = set(instances["resident_section_names"])
    models = {entry["model_name"]: entry["model_path"] for entry in instances["instances"]
              if entry["vis_section_name"] in sections}
    if instances["main_game_model_count"] <= 0 or len(models) != instances["resident_used_model_count"]:
        raise ValueError("expected the original EnvMain instance inventory")

    intermediate = workspace / ("Instances" if row == "Barn" else f"Instances_{row}")
    terrain_source = workspace / f"Terrain_{row}/EnvMain_terrain.json"
    terrain = json.loads(terrain_source.read_text(encoding="utf-8-sig"))
    if terrain["terrain"] != "EnvMain" or terrain["visibility_row"] != row:
        raise ValueError(f"expected the original EnvMain {row} terrain plan")
    for entry in terrain["packs"]:
        entry["manifest"] = (terrain_source.parent / entry["manifest"]).resolve().relative_to(workspace).as_posix()

    original_packs = len(terrain["packs"])
    total_instances = 0
    for name, source in sorted(models.items()):
        output = intermediate / name
        prefix = f"envmain_instances_{name.casefold()}"
        manifest = convert_instance(inventory, sources.model_dump(source), output, name, prefix)
        modes = []
        for texture in manifest["textures"]:
            source_texture, alpha = sources.texture(texture["source_texture"])
            shutil.copyfile(source_texture, output / texture["texture"])
            modes.append(alpha)

        filename = output / f"{prefix}.json"
        finalize(filename, sources.properties, filename.with_suffix(".pcfg"), modes)
        terrain["packs"].append({"source": source, "section": row, "lod": "instance", "glow": False,
                                 "manifest": filename.relative_to(workspace).as_posix()})
        total_instances += manifest["source_instances"]

    if total_instances != instances["resident_instance_count"]:
        raise ValueError("converted instance count differs from the original visible locators")
    terrain["converted"] = len(terrain["packs"])
    combined = workspace / f"envmain-{row.casefold()}-with-instances.json"
    combined.write_text(json.dumps(terrain, indent=2) + "\n", encoding="utf-8")
    destination = workspace / f"TerrainPack_{row}WithInstances"
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
    parser.add_argument("--visibility-row", action="append")
    parser.add_argument("install", type=Path)
    parser.add_argument("workspace", type=Path)
    args = parser.parse_args()
    try:
        tool = args.tmdl.resolve(strict=True)
        (tool.parent / "Logs").mkdir(exist_ok=True)
        workspace = args.workspace.resolve(strict=True)
        sources = InstanceSources(tool, args.install.resolve(strict=True), workspace, args.max_width)
    except (KeyError, OSError, ValueError, struct.error, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"EnvMain instance packaging failed: {error}\n")
    failed = False
    for row in args.visibility_row or ["Barn"]:
        try:
            report = package(workspace, sources, row)
        except (KeyError, OSError, ValueError, struct.error, subprocess.TimeoutExpired) as error:
            print(f"EnvMain {row} instance packaging failed: {error}", file=sys.stderr)
            failed = True
            continue
        print(f"{row}: Packed {report['instance_count']} instances into {report['instance_draw_groups']} draw groups, "
              f"{report['mesh_count']} total meshes and {report['texture_count']} shared textures.")
    if failed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
