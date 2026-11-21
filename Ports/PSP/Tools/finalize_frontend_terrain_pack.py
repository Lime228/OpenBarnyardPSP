#!/usr/bin/env python3

import argparse
import json
import pathlib
import struct


FRONTEND_TERRAIN_PACK_MAGIC = 0x50444650
FRONTEND_TERRAIN_PACK_VERSION = 1
FRONTEND_TERRAIN_PACK_ENTRY_SIZE = 20
MAX_PACK_MODELS = 48
MAX_PACK_TEXTURES = 48
ALPHA_MODES = {"mask": 1, "blend": 2, "additive": 3}
ADDRESS_WRAP = 0
ADDRESS_CLAMP = 1
ORDER_START = 0
ORDER_DEFAULT = 1
ORDER_END = 2


def read_name_set(document: dict[str, object], name: str) -> set[str]:
    values = document.get(name)
    if not isinstance(values, list) or not all(isinstance(value, str) for value in values):
        raise ValueError(f"material properties {name} list is invalid")
    return {value.casefold() for value in values}


def finalize(
    manifest_path: pathlib.Path,
    properties_path: pathlib.Path,
    destination: pathlib.Path,
    texture_alpha_modes: list[str],
) -> dict[str, object]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    properties = json.loads(properties_path.read_text(encoding="utf-8"))
    if not isinstance(manifest, dict) or manifest.get("version") != 1:
        raise ValueError("unsupported front-end terrain pack manifest")
    if not isinstance(properties, dict) or properties.get("version") != 1:
        raise ValueError("unsupported material properties contract")
    models = manifest.get("models")
    textures = manifest.get("textures")
    if not isinstance(models, list) or not 1 <= len(models) <= MAX_PACK_MODELS:
        raise ValueError("front-end terrain pack model count is invalid")
    if not isinstance(textures, list) or not 1 <= len(textures) <= MAX_PACK_TEXTURES:
        raise ValueError("front-end terrain pack texture count is invalid")
    if len(texture_alpha_modes) != len(textures):
        raise ValueError("texture alpha-mode count does not match the pack manifest")
    parsed_alpha_modes = []
    for texture_index, mode in enumerate(texture_alpha_modes):
        if mode not in ALPHA_MODES:
            raise ValueError(f"terrain pack texture {texture_index} has invalid alpha mode {mode!r}")
        parsed_alpha_modes.append(ALPHA_MODES[mode])

    clamped = properties.get("clamped")
    if not isinstance(clamped, list):
        raise ValueError("material properties clamped list is invalid")
    clamped_materials: dict[str, tuple[int, int]] = {}
    for index, item in enumerate(clamped):
        if not isinstance(item, dict) or not isinstance(item.get("material"), str):
            raise ValueError(f"clamped material {index} is invalid")
        address_u = item.get("address_u")
        address_v = item.get("address_v")
        if not isinstance(address_u, int) or not isinstance(address_v, int):
            raise ValueError(f"clamped material {index} address modes are invalid")
        clamped_materials[item["material"].casefold()] = (
            ADDRESS_CLAMP if address_u > 0 else ADDRESS_WRAP,
            ADDRESS_CLAMP if address_v > 0 else ADDRESS_WRAP,
        )
    start_materials = read_name_set(properties, "world_start_render")
    end_materials = read_name_set(properties, "world_end_render")

    entries = []
    for model_index, model in enumerate(models):
        if not isinstance(model, dict) or not isinstance(model.get("material"), str):
            raise ValueError(f"terrain pack model {model_index} is invalid")
        texture_index = model.get("texture_index")
        if not isinstance(texture_index, int) or not 0 <= texture_index < len(textures):
            raise ValueError(f"terrain pack model {model_index} texture index is invalid")
        material_key = model["material"].casefold()
        address_u, address_v = clamped_materials.get(material_key, (ADDRESS_WRAP, ADDRESS_WRAP))
        render_order = ORDER_START if material_key in start_materials else ORDER_END if material_key in end_materials else ORDER_DEFAULT
        entries.append((render_order, model_index, texture_index, parsed_alpha_modes[texture_index], address_u, address_v))
    entries.sort(key=lambda entry: (entry[0], entry[1]))

    payload = bytearray(
        struct.pack(
            "<6I",
            FRONTEND_TERRAIN_PACK_MAGIC,
            FRONTEND_TERRAIN_PACK_VERSION,
            len(models),
            len(textures),
            FRONTEND_TERRAIN_PACK_ENTRY_SIZE,
            0,
        )
    )
    for _, model_index, texture_index, alpha_mode, address_u, address_v in entries:
        payload.extend(struct.pack("<5I", model_index, texture_index, alpha_mode, address_u, address_v))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(payload)
    return {
        "version": FRONTEND_TERRAIN_PACK_VERSION,
        "models": len(models),
        "textures": len(textures),
        "entries": len(entries),
        "bytes": len(payload),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Finalize an original front-end terrain pack with retail material properties")
    parser.add_argument("manifest", type=pathlib.Path)
    parser.add_argument("properties", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--texture-alpha-modes", required=True)
    args = parser.parse_args()
    modes = [mode.strip().lower() for mode in args.texture_alpha_modes.split(",") if mode.strip()]
    print(json.dumps(finalize(args.manifest, args.properties, args.destination, modes), sort_keys=True))


if __name__ == "__main__":
    main()
