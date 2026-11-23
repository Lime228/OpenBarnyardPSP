#!/usr/bin/env python3
"""Inventory installed model and animation containers with the PSP TMDL host tool."""

import argparse
import json
import subprocess
from collections import Counter
from pathlib import Path


def model_kind(symbols: list[str]) -> str:
    names = set(symbols)
    if {"FileHeader", "Header", "Skeleton", "Materials"} <= names:
        if any(name.startswith("LOD0_Mesh_") for name in names):
            return "tmd-geometry"
        return "tmd-metadata"
    if "Database" in names:
        return "world-geometry"
    if "keylib" in names:
        return "key-library"
    return "other"


def run_export(tool: Path, mode: str, source: Path, destination: Path) -> dict:
    destination.parent.mkdir(parents=True, exist_ok=True)
    pending = destination.with_suffix(".json.tmp")
    process = subprocess.run(
        [str(tool), mode, str(source), "-o", str(pending)],
        cwd=tool.parent,
        capture_output=True,
        timeout=45,
        check=False,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    if process.returncode:
        diagnostic = (process.stdout + process.stderr)[-600:].decode(errors="replace").strip()
        raise ValueError(f"TMDL exited with {process.returncode}: {diagnostic}")
    document = json.loads(pending.read_text(encoding="utf-8"))
    if document.get("version") != 1:
        raise ValueError("unsupported TMDL host format")
    pending.replace(destination)
    return document


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", type=Path, required=True)
    parser.add_argument("--dump-skins", action="store_true", help="attempt the existing exporter for every TMD geometry")
    parser.add_argument("install", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    install = args.install.resolve(strict=True)
    root = install / "Data/Models"
    output = args.output.resolve()
    if output.is_relative_to(install):
        parser.error("output must be outside the original installation")
    tool = args.tmdl.resolve(strict=True)
    paths = sorted(
        (path for path in root.rglob("*") if path.is_file() and path.suffix.casefold() in (".trb", ".tkl")),
        key=lambda path: path.relative_to(root).as_posix().casefold(),
    )
    if not paths:
        parser.error("no model or key-library containers found")
    paths_by_stem = {path.relative_to(root).as_posix().casefold(): path for path in paths if path.suffix.lower() == ".tkl"}
    records = []
    for path in paths:
        name = path.relative_to(root)
        result = {"path": name.as_posix(), "bytes": path.stat().st_size, "kind": "unreadable"}
        try:
            document = run_export(tool, "-psp-symbols", path, output / "Symbols" / name.with_suffix(name.suffix + ".json"))
            result["symbols"] = [symbol["name"] for symbol in document["symbols"]]
            result["kind"] = model_kind(result["symbols"])
        except (KeyError, OSError, ValueError, subprocess.TimeoutExpired) as error:
            result["error"] = str(error)
        if path.suffix.casefold() == ".trb":
            keylib = paths_by_stem.get(name.with_suffix(".tkl").as_posix().casefold())
            if keylib:
                result["same_stem_keylib"] = keylib.relative_to(root).as_posix()
        if args.dump_skins and result["kind"] == "tmd-geometry":
            try:
                target = Path("SkinDumps") / name.with_suffix(".json")
                document = run_export(tool, "-psp-dump", path, output / target)
                result["skin_dump"] = {
                    "status": "exported", "path": target.as_posix(),
                    "bones": len(document["skeleton"]), "lod0_meshes": len(document["lods"][0]["meshes"]),
                }
            except (KeyError, OSError, ValueError, subprocess.TimeoutExpired) as error:
                result["skin_dump"] = {"status": "rejected", "error": str(error)}
        records.append(result)
    kinds = Counter(record["kind"] for record in records)
    dumps = Counter(record["skin_dump"]["status"] for record in records if "skin_dump" in record)
    report = {
        "version": 1, "source_count": len(records), "kind_counts": dict(sorted(kinds.items())),
        "skin_dump_counts": dict(sorted(dumps.items())), "records": records,
    }
    pending = output / "models.inventory.json.tmp"
    pending.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    pending.replace(output / "models.inventory.json")
    print(f"{len(records) - kinds['unreadable']}/{len(records)} containers inventoried")
    if args.dump_skins:
        print(f"{dumps['exported']} skin dumps exported, {dumps['rejected']} rejected by the current exporter")
    return int(kinds["unreadable"] != 0)


if __name__ == "__main__":
    raise SystemExit(main())
