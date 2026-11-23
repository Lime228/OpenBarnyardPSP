#!/usr/bin/env python3
"""Bake all exported retail skin dumps to grouped PMES and shared PSKL."""

import argparse
import json
import struct
import tempfile
from collections import Counter
from pathlib import Path

from convert_barnyard_skin import publish_outputs
from convert_tmd_dump import build_mesh, build_skeleton


def bake_geometry(document: dict, destination: Path) -> dict:
    if document.get("version") != 1:
        raise ValueError("unsupported TMDL model dump")
    skeleton = build_skeleton(document)
    bone_count = len(document["skeleton"])
    materials = {}
    for mesh in document["lods"][0]["meshes"]:
        name, texture = mesh["material"], mesh["texture"]
        if not isinstance(name, str) or not name or not isinstance(texture, str) or not texture:
            raise ValueError("missing retail material or texture binding")
        previous = materials.setdefault(name.casefold(), (name, texture))
        if previous[1].casefold() != texture.casefold():
            raise ValueError(f"material has multiple texture bindings: {name}")
    if not materials:
        raise ValueError("LOD0 has no materials")

    meshes = []
    destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="mesh-", dir=destination) as directory:
        scratch = Path(directory)
        skeleton_path = scratch / "skeleton.pskl"
        skeleton_path.write_bytes(skeleton)
        outputs = [(skeleton_path, destination / skeleton_path.name)]
        for index, (material, texture) in enumerate(materials.values()):
            data = build_mesh(document, bone_count, material)
            filename = f"mesh-{index:03}.pmes"
            stage = scratch / filename
            stage.write_bytes(data)
            outputs.append((stage, destination / filename))
            vertices, indices = struct.unpack_from("<2I", data, 8)
            meshes.append({
                "path": filename, "material": material, "texture": texture,
                "vertices": vertices, "indices": indices, "bytes": len(data),
            })
        publish_outputs(outputs)
    return {"skeleton": "skeleton.pskl", "bones": bone_count, "meshes": meshes,
            "bytes": len(skeleton) + sum(mesh["bytes"] for mesh in meshes)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inventory", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    inventory_path = args.inventory.resolve(strict=True)
    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    if inventory.get("version") != 1 or not inventory.get("skin_dump_counts"):
        parser.error("expected a model inventory produced with --dump-skins")
    input_root = inventory_path.parent
    output = args.output.resolve()
    results = []
    for record in inventory["records"]:
        if record["kind"] == "key-library":
            continue
        result = {"source": record["path"], "kind": record["kind"], "status": "not-exported"}
        dump = record.get("skin_dump", {})
        if dump.get("status") == "exported":
            name = Path(record["path"]).with_suffix("")
            source = (input_root / dump["path"]).resolve()
            if name.is_absolute() or ".." in name.parts or not source.is_relative_to(input_root):
                parser.error(f"invalid inventory path: {name}")
            try:
                document = json.loads(source.read_text(encoding="utf-8"))
                result.update(bake_geometry(document, output / name))
                result.update(status="converted", directory=name.as_posix())
            except (KeyError, OSError, ValueError, struct.error) as error:
                result.update(status="rejected", error=str(error))
        results.append(result)
    statuses = Counter(record["status"] for record in results)
    converted = [record for record in results if record["status"] == "converted"]
    report = {
        "version": 1, "status_counts": dict(sorted(statuses.items())),
        "mesh_count": sum(len(record["meshes"]) for record in converted),
        "bytes": sum(record["bytes"] for record in converted), "models": results,
    }
    output.mkdir(parents=True, exist_ok=True)
    pending = output / "model_geometry.json.tmp"
    pending.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    pending.replace(output / "model_geometry.json")
    print(f"{statuses['converted']} models baked, {statuses['rejected']} rejected, {statuses['not-exported']} without a skin dump")
    return int(statuses["rejected"] != 0)


if __name__ == "__main__":
    raise SystemExit(main())
