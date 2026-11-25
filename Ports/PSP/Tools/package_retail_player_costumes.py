#!/usr/bin/env python3
"""Bake the original 3D costume textures for both SS01 player models."""

import argparse
import json
import struct
import subprocess
import tempfile
from pathlib import Path

from convert_dds import convert as convert_texture
from convert_retail_sequence_models import index_textures, texture_key
from inventory_retail_models import run_tmdl
from inventory_retail_sequence import main_properties, retail_file


def package(tool: Path, install: Path, inventory: Path, output: Path, max_width: int) -> dict:
    sequence = json.loads(inventory.read_text(encoding="utf-8"))
    if sequence["sequence"] != "SS01_start":
        raise ValueError("expected the SS01_start inventory")
    types = json.loads(inventory.with_name("model-types.json").read_text(encoding="utf-8"))
    declarations = dict(main_properties(types))
    source = retail_file(install, sequence["sources"]["asset_pack"]["path"])
    output.mkdir(parents=True, exist_ok=True)
    models = []
    with tempfile.TemporaryDirectory(dir=output) as temporary:
        work = Path(temporary)
        textures = index_textures(tool, source, sequence["material_libraries"], work)
        files = []
        for name in ("ss_player_quad", "ss_playerf_quad"):
            body = dict(declarations[name]["properties"])["AnimatableObjectType"]
            skins = dict(dict(body["properties"])["Skins"]["properties"])["Tex"]
            if len(skins) != 15:
                raise ValueError(f"{name}: expected the original seven skin textures and lighting profiles")
            costumes = []
            scratch = work / "skin.dds"
            for index, (texture, lighting) in enumerate(zip(skins[1:8], skins[8:])):
                symbol, source_name = textures[texture_key(texture)]
                filename = f"{name}-{index}.ptex"
                run_tmdl(tool, "-psp-texture", source, scratch, "-texture-symbol", symbol, "-texture", source_name)
                details = convert_texture(scratch, work / filename, max_width=max_width)
                costumes.append({"index": index, "texture": source_name, "symbol": symbol,
                                 "lighting": lighting, "file": filename, "bytes": details["bytes"]})
                files.append(filename)
            models.append({"model": name, "material": skins[0], "costumes": costumes})
        report = {"version": 1, "sequence": sequence["sequence"], "models": models}
        manifest = work / "player_costumes.json"
        manifest.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        for name in files:
            (work / name).replace(output / name)
        manifest.replace(output / manifest.name)
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", required=True, type=Path)
    parser.add_argument("--max-width", type=int, default=128)
    parser.add_argument("install", type=Path)
    parser.add_argument("inventory", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        tool = args.tmdl.resolve(strict=True)
        (tool.parent / "Logs").mkdir(exist_ok=True)
        report = package(tool, args.install.resolve(strict=True), args.inventory, args.output.resolve(), args.max_width)
    except (IndexError, KeyError, OSError, TypeError, ValueError, struct.error, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"Player costume packaging failed: {error}\n")
    print(f"Prepared {sum(len(model['costumes']) for model in report['models'])} original SS01 3D costume textures")


if __name__ == "__main__":
    main()
