#!/usr/bin/env python3

import argparse
import json
import pathlib
import struct


FRONTEND_TERRAIN_CONFIG_MAGIC = 0x52544650
FRONTEND_TERRAIN_CONFIG_VERSION = 2
FRONTEND_TERRAIN_CONFIG_ENTRY_SIZE = 4
MAX_TERRAIN_MESHES = 16
ALPHA_MODES = {"mask": 1, "blend": 2, "additive": 3}


def finalize(manifest_path: pathlib.Path, destination: pathlib.Path, alpha_modes: list[str]) -> dict[str, object]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    meshes = manifest.get("meshes") if isinstance(manifest, dict) else None
    if not isinstance(meshes, list) or not 1 <= len(meshes) <= MAX_TERRAIN_MESHES:
        raise ValueError(f"terrain manifest requires between 1 and {MAX_TERRAIN_MESHES} meshes")
    if len(alpha_modes) != len(meshes):
        raise ValueError("terrain alpha-mode count does not match the mesh manifest")
    parsed_modes = []
    for index, mode in enumerate(alpha_modes):
        if mode not in ALPHA_MODES:
            raise ValueError(f"terrain mesh {index} has invalid alpha mode {mode!r}")
        parsed_modes.append(ALPHA_MODES[mode])

    payload = bytearray(
        struct.pack(
            "<4I",
            FRONTEND_TERRAIN_CONFIG_MAGIC,
            FRONTEND_TERRAIN_CONFIG_VERSION,
            len(meshes),
            FRONTEND_TERRAIN_CONFIG_ENTRY_SIZE,
        )
    )
    payload.extend(struct.pack(f"<{len(parsed_modes)}I", *parsed_modes))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(payload)
    return {"version": FRONTEND_TERRAIN_CONFIG_VERSION, "meshes": len(meshes), "alpha_modes": alpha_modes, "bytes": len(payload)}


def main() -> None:
    parser = argparse.ArgumentParser(description="Finalize a bounded PSP terrain config with retail-derived material modes")
    parser.add_argument("manifest", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--alpha-modes", required=True)
    args = parser.parse_args()
    modes = [mode.strip().lower() for mode in args.alpha_modes.split(",") if mode.strip()]
    print(json.dumps(finalize(args.manifest, args.destination, modes), sort_keys=True))


if __name__ == "__main__":
    main()
