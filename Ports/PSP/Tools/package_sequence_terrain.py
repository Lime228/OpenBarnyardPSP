#!/usr/bin/env python3
"""Package the original sequence terrain and share byte-identical textures."""

import argparse
import hashlib
import json
import shutil
import struct
from pathlib import Path


PACK_HEADER = struct.Struct("<4s5I")
DRAW_ENTRY = struct.Struct("<4I")
INDEX_HEADER = struct.Struct("<4s3I")
MAX_MESHES = 512
MAX_TEXTURES = 256


def package(source: Path, output: Path, envmain_dawn: bool = False) -> dict:
    terrain = json.loads(source.read_text(encoding="utf-8-sig"))
    if terrain.get("version") != 1 or terrain["failed"] or terrain["converted"] != len(terrain["packs"]):
        raise ValueError("expected a complete sequence terrain conversion")
    if envmain_dawn and terrain["terrain"] != "EnvMain":
        raise ValueError("the dawn material visibility applies to EnvMain")

    meshes = []
    textures = {}
    entries = []
    packs = []
    hidden_light_cones = 0
    for pack in terrain["packs"]:
        manifest = source.parent / pack["manifest"]
        data = json.loads(manifest.read_text(encoding="utf-8"))
        config = manifest.with_suffix(".pcfg").read_bytes()
        magic, version, models, local_textures, entry_size, reserved = PACK_HEADER.unpack_from(config)
        if (magic, version, entry_size, reserved) != (b"PFDP", 1, 20, 0):
            raise ValueError(f"unsupported terrain material config: {manifest}")
        if (models != len(data["models"]) or local_textures != len(data["textures"]) or
                len(config) != PACK_HEADER.size + models * entry_size):
            raise ValueError(f"terrain material config does not match the manifest: {manifest}")

        texture_indices = []
        for texture in data["textures"]:
            path = manifest.parent / texture["texture"]
            payload = path.read_bytes()
            if payload[:4] != b"PTEX":
                raise ValueError(f"expected PTEX: {path}")
            key = hashlib.sha256(payload).digest()
            index, _, _ = textures.setdefault(key, (len(textures), path, len(payload)))
            texture_indices.append(index)

        seen = set()
        start = len(meshes)
        for offset in range(PACK_HEADER.size, len(config), entry_size):
            model, texture, alpha, address_u, address_v = struct.unpack_from("<5I", config, offset)
            if (model in seen or model >= models or texture >= local_textures or
                    alpha not in (1, 2, 3) or address_u > 1 or address_v > 1 or
                    texture != data["models"][model]["texture_index"]):
                raise ValueError(f"invalid terrain material entry: {manifest}")
            seen.add(model)
            # ATerrainInterface turns these night light cones off by 05:30.
            if envmain_dawn and data["models"][model]["material"].casefold() in {"lightcone_outer", "lightcone_inner"}:
                hidden_light_cones += 1
                continue
            path = manifest.parent / data["models"][model]["mesh"]
            if path.stat().st_size != data["models"][model]["bytes"]:
                raise ValueError(f"PMES size does not match the manifest: {path}")
            meshes.append(path)
            entries.append(DRAW_ENTRY.pack(texture_indices[texture], alpha, address_u, address_v))
        packs.append({"source": pack["source"], "section": pack["section"], "lod": pack["lod"],
                      "glow": pack["glow"], "first_mesh": start, "meshes": len(meshes) - start})

    if not 1 <= len(meshes) <= MAX_MESHES or not 1 <= len(textures) <= MAX_TEXTURES:
        raise ValueError("sequence terrain exceeds its bounded resource table")

    output.mkdir(parents=True, exist_ok=True)
    for index, mesh in enumerate(meshes):
        shutil.copyfile(mesh, output / f"mesh_{index}.pmes")
    for index, texture, _ in textures.values():
        shutil.copyfile(texture, output / f"texture_{index}.ptex")
    payload = INDEX_HEADER.pack(b"PTLD", 1, len(meshes), len(textures)) + b"".join(entries)
    temporary = output / "terrain.ptld.tmp"
    temporary.write_bytes(payload)
    temporary.replace(output / "terrain.ptld")

    report = {"version": 1, "terrain": terrain["terrain"], "visibility_row": terrain["visibility_row"],
              "mesh_count": len(meshes), "texture_count": len(textures), "packs": packs,
              "geometry_bytes": sum(mesh.stat().st_size for mesh in meshes),
              "texture_bytes": sum(size for _, _, size in textures.values())}
    if envmain_dawn:
        report["dawn_hidden_light_cones"] = hidden_light_cones
    (output / "terrain.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--envmain-dawn", action="store_true")
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        report = package(args.source, args.output, args.envmain_dawn)
    except (KeyError, OSError, ValueError, struct.error) as error:
        parser.exit(1, f"Sequence terrain packaging failed: {error}\n")
    print(f"Packaged {report['mesh_count']} meshes and {report['texture_count']} shared textures "
          f"({report['geometry_bytes'] + report['texture_bytes']} bytes)")


if __name__ == "__main__":
    main()
