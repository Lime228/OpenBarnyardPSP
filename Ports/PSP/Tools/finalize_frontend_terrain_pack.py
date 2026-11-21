#!/usr/bin/env python3

import argparse
import json
import pathlib
import struct


FRONTEND_TERRAIN_PACK_MAGIC = 0x50444650
FRONTEND_TERRAIN_PACK_VERSION_1 = 1
FRONTEND_TERRAIN_PACK_VERSION_2 = 2
FRONTEND_TERRAIN_PACK_ENTRY_SIZE_1 = 20
FRONTEND_TERRAIN_PACK_ENTRY_SIZE_2 = 24
MAX_PACK_MODELS = 48
MAX_PACK_TEXTURES = 48
ALPHA_MODES = {"mask": 1, "blend": 2, "additive": 3}
LIGHT_PROFILES = {
    "none": 0,
    "barn": 1,
    "cone": 2,
    "farmhouse": 3,
    "neighbour": 4,
    "lodge": 5,
    "beady": 6,
    "nightlight": 7,
}
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
    force_alpha_mode: str | None = None,
    light_profiles: dict[str, str] | None = None,
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
    if force_alpha_mode is not None and force_alpha_mode not in ALPHA_MODES:
        raise ValueError(f"invalid forced terrain pack alpha mode {force_alpha_mode!r}")
    forced_alpha_mode = ALPHA_MODES[force_alpha_mode] if force_alpha_mode is not None else None
    parsed_light_profiles: dict[str, int] = {}
    for material, profile in (light_profiles or {}).items():
        if not material:
            raise ValueError("terrain pack light-profile material is empty")
        if profile not in LIGHT_PROFILES or profile == "none":
            raise ValueError(f"invalid terrain pack light profile {profile!r}")
        material_key = material.casefold()
        if material_key in parsed_light_profiles:
            raise ValueError(f"duplicate terrain pack light-profile material {material!r}")
        parsed_light_profiles[material_key] = LIGHT_PROFILES[profile]

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
    matched_light_profiles: set[str] = set()
    for model_index, model in enumerate(models):
        if not isinstance(model, dict) or not isinstance(model.get("material"), str):
            raise ValueError(f"terrain pack model {model_index} is invalid")
        texture_index = model.get("texture_index")
        if not isinstance(texture_index, int) or not 0 <= texture_index < len(textures):
            raise ValueError(f"terrain pack model {model_index} texture index is invalid")
        material_key = model["material"].casefold()
        address_u, address_v = clamped_materials.get(material_key, (ADDRESS_WRAP, ADDRESS_WRAP))
        render_order = ORDER_START if material_key in start_materials else ORDER_END if material_key in end_materials else ORDER_DEFAULT
        alpha_mode = forced_alpha_mode if forced_alpha_mode is not None else parsed_alpha_modes[texture_index]
        light_profile = parsed_light_profiles.get(material_key, LIGHT_PROFILES["none"])
        if light_profile != LIGHT_PROFILES["none"]:
            matched_light_profiles.add(material_key)
        entries.append((render_order, model_index, texture_index, alpha_mode, address_u, address_v, light_profile))
    entries.sort(key=lambda entry: (entry[0], entry[1]))
    missing_light_profiles = parsed_light_profiles.keys() - matched_light_profiles
    if missing_light_profiles:
        raise ValueError(f"terrain pack light-profile materials were not found: {sorted(missing_light_profiles)!r}")

    version = FRONTEND_TERRAIN_PACK_VERSION_2 if parsed_light_profiles else FRONTEND_TERRAIN_PACK_VERSION_1
    entry_size = FRONTEND_TERRAIN_PACK_ENTRY_SIZE_2 if parsed_light_profiles else FRONTEND_TERRAIN_PACK_ENTRY_SIZE_1

    payload = bytearray(
        struct.pack(
            "<6I",
            FRONTEND_TERRAIN_PACK_MAGIC,
            version,
            len(models),
            len(textures),
            entry_size,
            0,
        )
    )
    for _, model_index, texture_index, alpha_mode, address_u, address_v, light_profile in entries:
        payload.extend(struct.pack("<5I", model_index, texture_index, alpha_mode, address_u, address_v))
        if version == FRONTEND_TERRAIN_PACK_VERSION_2:
            payload.extend(struct.pack("<I", light_profile))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(payload)
    return {
        "version": version,
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
    parser.add_argument("--force-alpha-mode", choices=sorted(ALPHA_MODES))
    parser.add_argument(
        "--light-profile",
        action="append",
        default=[],
        metavar="MATERIAL=PROFILE",
        help="assign an original terrain light schedule to a material",
    )
    args = parser.parse_args()
    modes = [mode.strip().lower() for mode in args.texture_alpha_modes.split(",") if mode.strip()]
    light_profiles: dict[str, str] = {}
    for assignment in args.light_profile:
        material, separator, profile = assignment.partition("=")
        if not separator or not material or not profile:
            parser.error("--light-profile must use MATERIAL=PROFILE")
        if material.casefold() in {name.casefold() for name in light_profiles}:
            parser.error(f"duplicate --light-profile material {material!r}")
        light_profiles[material] = profile.lower()
    print(
        json.dumps(
            finalize(
                args.manifest,
                args.properties,
                args.destination,
                modes,
                force_alpha_mode=args.force_alpha_mode,
                light_profiles=light_profiles,
            ),
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
