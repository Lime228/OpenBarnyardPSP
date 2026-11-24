#!/usr/bin/env python3
"""Bake a retail sequence's declared models and material textures."""

import argparse
import json
import re
import struct
import tempfile
from collections import Counter
from pathlib import Path

from convert_dds import convert as convert_texture
from convert_frontend_terrain_pack import convert as bake_world
from convert_retail_model_geometry import bake_geometry
from inventory_retail_models import run_export, run_tmdl
from inventory_retail_sequence import retail_file


def texture_key(name: str) -> str:
    return name.replace("\\", "/").casefold()


def index_textures(tool: Path, pack: Path, libraries: list, output: Path) -> dict:
    textures = {}
    indexed = output / "Libraries"
    for number, library in enumerate(libraries):
        symbol = library["symbol"]
        if not symbol:
            continue
        document = run_export(tool, "-psp-textures", pack, indexed / f"{number:03}.json", "-texture-symbol", symbol)
        for texture in document["textures"]:
            name = texture["name"]
            # The original material manager searches the most recently loaded library first.
            textures[texture_key(name)] = (symbol, name)
    return textures


def bake_packed_model(tool: Path, pack: Path, destination: Path, prefix: str, world_model: bool) -> dict:
    source = destination / "model.json"
    if not world_model:
        dump = run_export(tool, "-psp-dump", pack, source, "-model-name", prefix)
        return {"kind": "skin", **bake_geometry(dump, destination)}

    run_export(tool, "-psp-terrain-cell", pack, source, "-model-name", prefix, version=2)
    baked = bake_world(source, destination, "world")
    meshes = [
        {"path": mesh["mesh"], "material": mesh["material"], "texture": mesh["source_texture"],
         "vertices": mesh["vertices"], "indices": mesh["indices"], "bytes": mesh["bytes"]}
        for mesh in baked["models"]
    ]
    return {"kind": "world", "meshes": meshes, "bytes": sum(mesh["bytes"] for mesh in meshes)}


def convert_models(tool: Path, install: Path, manifest: dict, output: Path, max_width: int) -> dict:
    pack = retail_file(install, manifest["sources"]["asset_pack"]["path"])
    available_textures = index_textures(tool, pack, manifest["material_libraries"], output)
    textures = {}
    models = {}
    texture_path = output / "Textures"
    texture_path.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="dds-", dir=output) as scratch_name:
        scratch_dds = Path(scratch_name) / "texture.dds"
        for declaration in manifest["model_types"]:
            model_name = declaration["model"]
            key = model_name.casefold()
            if key in models:
                models[key]["types"].append(declaration["type"])
                continue
            record = {"model": model_name, "types": [declaration["type"]]}
            models[key] = record
            world_symbol = declaration.get("packed_world_database")
            header = world_symbol or declaration["packed_file_header"]
            if not header:
                record["status"] = "no-packed-geometry"
                continue
            prefix = header.removesuffix("_Database" if world_symbol else "_FileHeader")
            if prefix == header or not re.fullmatch(r"[A-Za-z0-9_-]+", prefix):
                raise ValueError(f"invalid packed model symbol: {header}")
            directory = f"Models/{prefix}"
            destination = output / directory
            try:
                baked = bake_packed_model(tool, pack, destination, prefix, bool(world_symbol))
                record.update(directory=directory, **baked)
                missing = set()
                for mesh in record["meshes"]:
                    texture = mesh["texture"]
                    texture_id = texture_key(texture)
                    location = available_textures.get(texture_id)
                    if location is None:
                        missing.add(texture)
                        continue
                    if texture_id not in textures:
                        symbol, name = location
                        relative = f"Textures/{len(textures):03}.ptex"
                        run_tmdl(tool, "-psp-texture", pack, scratch_dds, "-texture-symbol", symbol, "-texture", name)
                        details = convert_texture(scratch_dds, output / relative, max_width=max_width)
                        textures[texture_id] = {"name": name, "symbol": symbol, "path": relative, **details}
                    mesh["texture_path"] = textures[texture_id]["path"]
                if missing:
                    record.update(status="missing-textures", missing_textures=sorted(missing, key=str.casefold))
                else:
                    record["status"] = "converted"
            except (KeyError, OSError, ValueError, struct.error) as error:
                record.update(status="rejected", error=str(error))

    statuses = Counter(record["status"] for record in models.values())
    report = {
        "version": 1,
        "sequence": manifest["sequence"],
        "asset_pack": manifest["sources"]["asset_pack"]["path"],
        "status_counts": dict(sorted(statuses.items())),
        "geometry_bytes": sum(record.get("bytes", 0) for record in models.values()),
        "texture_bytes": sum(texture["bytes"] for texture in textures.values()),
        "models": list(models.values()),
        "textures": list(textures.values()),
    }
    pending = output / "sequence_models.json.tmp"
    pending.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    pending.replace(output / "sequence_models.json")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", type=Path, required=True)
    parser.add_argument("--max-width", type=int, choices=(32, 64, 128, 256, 512), default=128)
    parser.add_argument("install", type=Path)
    parser.add_argument("inventory", type=Path, help="sequence.json from inventory_retail_sequence.py")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    install = args.install.resolve(strict=True)
    output = args.output.resolve()
    if output.is_relative_to(install):
        parser.error("output must be outside the retail installation")
    manifest = json.loads(args.inventory.read_text(encoding="utf-8"))
    if manifest.get("version") != 1 or "asset_pack" not in manifest.get("sources", {}):
        parser.error("expected a retail sequence inventory with its own asset pack")
    tool = args.tmdl.resolve(strict=True)
    (tool.parent / "Logs").mkdir(exist_ok=True)
    try:
        report = convert_models(tool, install, manifest, output, args.max_width)
    except (KeyError, OSError, ValueError) as error:
        parser.exit(1, f"Sequence model conversion failed: {error}\n")
    print(json.dumps(report["status_counts"], sort_keys=True))
    print(f"{len(report['textures'])} textures, {report['geometry_bytes'] + report['texture_bytes']} bytes")
    return int(any(status != "converted" for status in report["status_counts"]))


if __name__ == "__main__":
    raise SystemExit(main())
