#!/usr/bin/env python3
"""Bake the gameplay player models, costume textures and base animation clips."""

import argparse
import json
import re
import tempfile
from pathlib import Path

from convert_dds import convert as convert_texture
from convert_retail_model_animations import EXPORT_ERRORS, bake_clip, clip_sample_rate
from convert_retail_model_geometry import bake_geometry
from convert_retail_sequence_models import texture_key
from inventory_retail_models import run_export, run_tmdl
from inventory_retail_sequence import main_properties, retail_file


def read_costume_names(source: Path) -> list[str]:
    table = re.search(r"s_aCostumeNames\[\] = \{([^}]+)\};", source.read_text(encoding="utf-8"))
    if table is None:
        raise ValueError("could not read the original profile costume library names")
    names = re.findall(r'"([a-z0-9_]+)"', table[1])
    if len(names) != 7:
        raise ValueError("expected seven original gameplay costume libraries")
    return names


def bake_costumes(tool: Path, install: Path, gender: str, names: list[str], skins: list,
                  meshes: list, output: Path, scratch: Path, max_width: int) -> tuple[int, list]:
    if len(skins) != 1 + 2 * len(names):
        raise ValueError("unexpected gameplay Skins.Tex layout")
    costume_mesh, = [i for i, mesh in enumerate(meshes)
                     if ("ss_" + mesh["material"]).casefold() == skins[0].casefold()]
    if texture_key(meshes[costume_mesh]["texture"]) != texture_key(skins[1]):
        raise ValueError("the original costume slot does not match the model texture")

    costumes = []
    dds = scratch / "texture.dds"
    for i, (name, texture, lighting) in enumerate(zip(names, skins[1:8], skins[8:], strict=True)):
        relative = f"Data/Matlibs/p{gender[0]}{name}.ttl"
        library = retail_file(install, relative)
        atlas = run_export(tool, "-psp-textures", library, scratch / "costume-textures.json")
        by_name = {texture_key(entry["name"]): entry["name"] for entry in atlas["textures"]}
        filename = f"costume-{i}.ptex"
        run_tmdl(tool, "-psp-texture", library, dds, "-texture", by_name[texture_key(texture)])
        details = convert_texture(dds, output / filename, max_width=max_width)
        costumes.append({"index": i, "file": filename, "bytes": details["bytes"], "texture": texture,
                         "lighting": lighting, "library": library.relative_to(install).as_posix()})
        if i == 0:
            for j, mesh in enumerate(meshes):
                if j == costume_mesh:
                    continue
                name = by_name[texture_key(mesh["texture"])]
                mesh["texture_file"] = f"mesh-{j:03}.ptex"
                run_tmdl(tool, "-psp-texture", library, dds, "-texture", name)
                convert_texture(dds, output / mesh["texture_file"], max_width=max_width)
    return costume_mesh, costumes


def bake_player(tool: Path, install: Path, gender: str, properties: dict, names: list[str],
                output: Path, scratch: Path, max_width: int) -> dict:
    actor, = [value for value in properties["AnimObjTypes"] if not value.endswith("_bike")]
    descriptor = retail_file(install, f"Data/ModelTypes/{actor}.trb")
    body = dict(main_properties(run_export(tool, "-psp-properties", descriptor, scratch / "model-type.json")))
    declaration = body["AnimatableObjectType"]["properties"]
    model = retail_file(install, f"Data/Models/{dict(declaration)['Model']}.trb")
    info = run_export(tool, "-psp-animation-info", model, scratch / "animation-info.json")
    library_id, = properties["keylib"]
    if info["library"].casefold() != library_id.casefold():
        raise ValueError(f"{actor}: the model uses a different original key library")
    library = retail_file(install, f"Data/Models/{library_id}.tkl")
    dump = run_export(tool, "-psp-dump", model, scratch / "model.json")
    geometry = bake_geometry(dump, output)

    skins_model = retail_file(install, f"Data/ModelTypes/{actor}_skins.trb")
    skin_props = dict(main_properties(run_export(tool, "-psp-properties", skins_model, scratch / "skins.json")))
    skins = dict(skin_props["Skins"]["properties"])["Tex"]
    mesh, costumes = bake_costumes(tool, install, gender, names, skins, geometry["meshes"], output, scratch, max_width)

    animations = {}
    for i, clip in enumerate(info["sequences"]):
        rate = clip_sample_rate(clip)
        record = {key: clip[key] for key in ("name", "duration", "mode", "overlay")}
        if rate:
            record.update(bake_clip(tool, model, library, clip, rate, output / f"clip-{i:03}.panm", scratch, dump["skeleton"]))
        key = clip["name"].casefold()
        if key in animations:
            raise ValueError(f"{actor}: duplicate original animation name {clip['name']}")
        animations[key] = record

    named_sets = []
    for key, value in declaration:
        if key != "NamedAnimationSet":
            continue
        named = [dict(item["properties"]) for name, item in value["properties"] if name == "NamedAnimation"]
        named_sets.append({"name": dict(value["properties"])["Name"], "animations": named})
    default_animations = {item["Name"]: item["ExportedName"] for item in named_sets[0]["animations"]}
    for name in ("idle_lp", "walk_lp", "run_lp"):
        if "path" not in animations[default_animations[name].casefold()]:
            raise ValueError(f"{actor}: could not bake the declared gameplay clip {name}")

    return {"gender": gender, "actor": actor, "model": model.relative_to(install).as_posix(),
            "key_library": library.relative_to(install).as_posix(), "costume_mesh": mesh, "costumes": costumes,
            "animation_sets": named_sets, "clips": list(animations.values()), **geometry}


def package(tool: Path, install: Path, output: Path, max_width: int) -> dict:
    source = Path(__file__).resolve().parents[3] / "OpenBarnyard/Source/Assets/AAssetLoader.cpp"
    names = read_costume_names(source)
    if output.is_relative_to(install):
        raise ValueError("use an output directory outside the original installation")
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output) as temporary:
        scratch = Path(temporary)
        descriptor = retail_file(install, "Data/lib_maingame.trb")
        library = dict(main_properties(run_export(tool, "-psp-properties", descriptor, scratch / "library.json")))
        profiles = dict(library["playercharacter"]["properties"])
        players = []
        for gender in ("male", "female"):
            player = scratch / gender
            player.mkdir()
            record = bake_player(tool, install, gender, dict(profiles[gender]["properties"]), names, player, scratch, max_width)
            record["files"] = {path.name: path.stat().st_size for path in sorted(player.iterdir())}
            players.append(record)
        report = {"version": 1, "library": "lib_maingame", "players": players}
        pending = scratch / "sim_players.json"
        pending.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        for record in players:
            destination = output / record["gender"]
            destination.mkdir(exist_ok=True)
            for name in record["files"]:
                (scratch / record["gender"] / name).replace(destination / name)
        pending.replace(output / pending.name)
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", required=True, type=Path)
    parser.add_argument("--max-width", type=int, default=128)
    parser.add_argument("install", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        tool = args.tmdl.resolve(strict=True)
        (tool.parent / "Logs").mkdir(exist_ok=True)
        report = package(tool, args.install.resolve(strict=True), args.output.resolve(), args.max_width)
    except (*EXPORT_ERRORS, IndexError, TypeError) as error:
        parser.exit(1, f"Gameplay player packaging failed: {error}\n")
    for player in report["players"]:
        clips = sum("path" in item for item in player["clips"])
        print(f"{player['actor']}: {len(player['meshes'])} meshes, {len(player['costumes'])} costumes, {clips} base animations")


if __name__ == "__main__":
    main()
