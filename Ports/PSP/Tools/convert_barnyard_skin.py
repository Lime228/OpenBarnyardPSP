#!/usr/bin/env python3

import argparse
import json
import os
import pathlib
import shutil
import struct
import subprocess
import tempfile
import zlib

from convert_animation import convert as convert_animation
from convert_tmd_dump import convert as convert_tmd_dump, get_material_name
from convert_texture import (
    PTEX_FORMAT_RGBA8888,
    PTEX_FORMAT_RGBA8888_SWIZZLED,
    PTEX_MAGIC,
    PTEX_VERSION,
    convert as convert_texture,
    is_power_of_two,
)

TFLOAT_EPSILON = 1.1920929e-7
PSET_MAGIC = 0x54455350
PSET_VERSION = 1
PSET_FLAG_TEXTURE = 1
PSET_HEADER_SIZE = 48


def require_file(path: pathlib.Path, name: str) -> None:
    if not path.is_file():
        raise ValueError(f"{name} does not exist: {path}")


def publish_outputs(
    staged_outputs: list[tuple[pathlib.Path | None, pathlib.Path]],
    replace_file=os.replace,
) -> None:
    backup_root = next((staged.parent for staged, _ in staged_outputs if staged is not None), None)
    if backup_root is None:
        raise ValueError("preview publication requires at least one staged output")
    transactions: list[tuple[pathlib.Path, pathlib.Path | None, bool]] = []
    try:
        for index, (staged, destination) in enumerate(staged_outputs):
            backup = backup_root / f".{index}.{destination.name}.backup" if destination.exists() else None
            if backup is not None:
                replace_file(destination, backup)
            transactions.append((destination, backup, False))
            if staged is not None:
                replace_file(staged, destination)
                transactions[-1] = (destination, backup, True)
    except Exception:
        rollback_error: Exception | None = None
        for destination, backup, published in reversed(transactions):
            try:
                if published and destination.exists():
                    destination.unlink()
                if backup is not None and backup.exists():
                    replace_file(backup, destination)
            except Exception as error:
                rollback_error = error
        if rollback_error is not None:
            raise RuntimeError("PSP preview publish failed and rollback was incomplete") from rollback_error
        raise


def safe_name(value: str) -> str:
    result = "".join(character if character.isalnum() or character in "-_." else "_" for character in value)
    if not result or result in {".", ".."}:
        raise ValueError("sequence name does not produce a safe output filename")
    return result


def write_animation_skeleton(model_dump: pathlib.Path, destination: pathlib.Path) -> None:
    document = json.loads(model_dump.read_text(encoding="utf-8"))
    source_bones = document.get("skeleton") if isinstance(document, dict) else None
    if not isinstance(source_bones, list):
        raise ValueError("TMD dump does not contain a skeleton")

    bones = []
    names = []
    for index, source_bone in enumerate(source_bones):
        if not isinstance(source_bone, dict) or not isinstance(source_bone.get("name"), str):
            raise ValueError(f"TMD dump bone {index} is invalid")
        parent = source_bone.get("parent")
        if parent is None:
            parent_name = None
        elif isinstance(parent, int) and 0 <= parent < index:
            parent_name = names[parent]
        else:
            raise ValueError(f"TMD dump bone {index} has an invalid parent")
        name = source_bone["name"]
        names.append(name)
        bones.append(
            {
                "name": name,
                "parent": parent_name,
                "translation": source_bone.get("translation", [0.0, 0.0, 0.0]),
                "rotation": source_bone.get("rotation", [0.0, 0.0, 0.0, 1.0]),
            }
        )
    destination.write_text(json.dumps({"bones": bones}, indent=2) + "\n", encoding="utf-8")


def stage_texture(source: pathlib.Path, destination: pathlib.Path) -> None:
    if source.suffix.casefold() == ".ppm":
        convert_texture(source, destination)
        return
    if source.suffix.casefold() != ".ptex":
        raise ValueError("preview texture must be an ASCII P3 PPM or a converted PTEX")

    payload = source.read_bytes()
    if len(payload) < 24:
        raise ValueError("preview PTEX is smaller than its header")
    magic, version, width, height, buffer_width, texture_format = struct.unpack("<6I", payload[:24])
    if (
        magic != PTEX_MAGIC
        or version != PTEX_VERSION
        or not is_power_of_two(width)
        or not is_power_of_two(height)
        or not is_power_of_two(buffer_width)
        or width > buffer_width
        or width > 512
        or height > 512
        or texture_format not in {PTEX_FORMAT_RGBA8888, PTEX_FORMAT_RGBA8888_SWIZZLED}
        or len(payload) != 24 + buffer_width * height * 4
    ):
        raise ValueError("preview PTEX header or payload is invalid")
    destination.write_bytes(payload)


def write_preview_set(
    destination: pathlib.Path,
    mesh: pathlib.Path,
    skeleton: pathlib.Path,
    animation: pathlib.Path,
    texture: pathlib.Path | None,
) -> None:
    assets = (mesh, skeleton, animation, texture)
    values = []
    for asset in assets:
        if asset is None:
            values.extend((0, 0))
            continue
        payload = asset.read_bytes()
        values.extend((len(payload), zlib.crc32(payload) & 0xFFFFFFFF))
    flags = PSET_FLAG_TEXTURE if texture is not None else 0
    destination.write_bytes(
        struct.pack(
            "<12I",
            PSET_MAGIC,
            PSET_VERSION,
            flags,
            PSET_HEADER_SIZE,
            *values,
        )
    )


def read_summary(
    mesh: pathlib.Path,
    skeleton: pathlib.Path,
    animation: pathlib.Path | None,
    texture: pathlib.Path | None,
) -> dict[str, object]:
    mesh_payload = mesh.read_bytes()
    mesh_header = struct.unpack("<6I", mesh_payload[:24])
    skeleton_header = struct.unpack("<4I", skeleton.read_bytes()[:16])
    vertex_count = mesh_header[2]
    vertex_size = mesh_header[4]
    mesh_format = mesh_header[5]
    position_offset = 4 if mesh_format == 0 else 12
    positions = [
        struct.unpack_from("<3f", mesh_payload, 24 + index * vertex_size + position_offset)
        for index in range(vertex_count)
    ]
    bounds_min = [min(position[axis] for position in positions) for axis in range(3)]
    bounds_max = [max(position[axis] for position in positions) for axis in range(3)]
    preview_center = [
        bounds_min[axis] + (bounds_max[axis] - bounds_min[axis]) * 0.5
        for axis in range(3)
    ]
    largest_size = max(bounds_max[axis] - bounds_min[axis] for axis in range(3))
    if largest_size <= TFLOAT_EPSILON:
        raise ValueError("preview model bounds are degenerate")
    summary = {
        "vertices": vertex_count,
        "indices": mesh_header[3],
        "bones": skeleton_header[2],
        "mesh_bytes": mesh.stat().st_size,
        "skeleton_bytes": skeleton.stat().st_size,
        "bounds_min": bounds_min,
        "bounds_max": bounds_max,
        "preview_center": preview_center,
        "preview_scale": 2.0 / largest_size,
    }
    if animation is not None:
        animation_header = struct.unpack("<6IfI", animation.read_bytes()[:32])
        summary.update(
            {
                "frames": animation_header[3],
                "sample_rate": animation_header[4],
                "animation_bytes": animation.stat().st_size,
            }
        )
    if texture is not None:
        texture_header = struct.unpack("<6I", texture.read_bytes()[:24])
        summary.update(
            {
                "texture_width": texture_header[2],
                "texture_height": texture_header[3],
                "texture_bytes": texture.stat().st_size,
            }
        )
    return summary


def run_pipeline(
    tmdl: pathlib.Path,
    model: pathlib.Path,
    output_directory: pathlib.Path,
    output_name: str,
    key_library: pathlib.Path | None = None,
    sequence: str | None = None,
    sample_rate: int = 30,
    keep_intermediate: bool = False,
    runtime_preview: bool = False,
    preview_texture: pathlib.Path | None = None,
    preview_material: str | None = None,
) -> dict[str, object]:
    require_file(tmdl, "TMDL executable")
    require_file(model, "model")
    if (key_library is None) != (sequence is None):
        raise ValueError("key library and sequence must be supplied together")
    if key_library is not None:
        require_file(key_library, "key library")
    if sample_rate < 1 or sample_rate > 60:
        raise ValueError("sample rate must be between 1 and 60")
    if runtime_preview and sequence is None:
        raise ValueError("runtime preview requires a key library and sequence")
    if preview_texture is not None:
        require_file(preview_texture, "preview texture")

    tmdl = tmdl.resolve()
    model = model.resolve()
    output_directory = output_directory.resolve()
    key_library = key_library.resolve() if key_library is not None else None
    preview_texture = preview_texture.resolve() if preview_texture is not None else None
    (tmdl.parent / "Logs").mkdir(exist_ok=True)

    output_name = safe_name(output_name)

    output_directory.mkdir(parents=True, exist_ok=True)
    asset_name = "barnyard_preview" if runtime_preview else output_name
    mesh_destination = output_directory / f"{asset_name}.pmes"
    skeleton_destination = output_directory / f"{asset_name}.pskl"
    animation_destination = (
        output_directory / (f"{asset_name}.panm" if runtime_preview else f"{asset_name}_{safe_name(sequence)}.panm")
        if sequence is not None
        else None
    )
    texture_destination = (
        output_directory / f"{asset_name}.ptex"
        if preview_texture is not None or runtime_preview
        else None
    )
    preview_set_destination = output_directory / f"{asset_name}.pset" if runtime_preview else None

    with tempfile.TemporaryDirectory(prefix=".openbarnyard_psp_", dir=output_directory) as temporary_name:
        temporary = pathlib.Path(temporary_name)
        model_dump = temporary / "model.json"
        mesh = temporary / mesh_destination.name
        skeleton = temporary / skeleton_destination.name
        animation = temporary / animation_destination.name if animation_destination is not None else None
        texture = (
            temporary / texture_destination.name
            if preview_texture is not None and texture_destination is not None
            else None
        )
        preview_set = temporary / preview_set_destination.name if preview_set_destination is not None else None
        subprocess.run(
            [str(tmdl), "-psp-dump", str(model), "-o", str(model_dump)],
            check=True,
            cwd=tmdl.parent,
        )
        convert_tmd_dump(model_dump, mesh, skeleton, preview_material)
        model_document = json.loads(model_dump.read_text(encoding="utf-8"))
        if not isinstance(model_document, dict):
            raise ValueError("TMD dump root must be an object")
        material_name = get_material_name(model_document, preview_material)

        animation_dump = None
        animation_skeleton = None
        if key_library is not None and sequence is not None and animation is not None:
            animation_dump = temporary / "animation.json"
            animation_skeleton = temporary / "skeleton.json"
            subprocess.run(
                [
                    str(tmdl),
                    "-psp-animation",
                    str(model),
                    "-keylib",
                    str(key_library),
                    "-sequence",
                    sequence,
                    "-sample-rate",
                    str(sample_rate),
                    "-o",
                    str(animation_dump),
                ],
                check=True,
                cwd=tmdl.parent,
            )
            write_animation_skeleton(model_dump, animation_skeleton)
            convert_animation(animation_skeleton, animation_dump, animation)

        if preview_texture is not None and texture is not None:
            stage_texture(preview_texture, texture)
        if preview_set is not None and animation is not None:
            write_preview_set(preview_set, mesh, skeleton, animation, texture)

        if keep_intermediate:
            intermediate = output_directory / f"{output_name}_intermediate"
            intermediate.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(model_dump, intermediate / "model.json")
            if animation_dump is not None and animation_skeleton is not None:
                shutil.copyfile(animation_dump, intermediate / "animation.json")
                shutil.copyfile(animation_skeleton, intermediate / "skeleton.json")

        summary = read_summary(mesh, skeleton, animation, texture)
        summary["material"] = material_name
        summary["texture_source"] = "barnyard_preview.ptex" if texture is not None else "probe fallback"
        if preview_set is not None:
            summary["preview_set_bytes"] = preview_set.stat().st_size
        staged_outputs = [
            (mesh, mesh_destination),
            (skeleton, skeleton_destination),
        ]
        if animation is not None and animation_destination is not None:
            staged_outputs.append((animation, animation_destination))
        if texture_destination is not None:
            staged_outputs.append((texture, texture_destination))
        if preview_set is not None and preview_set_destination is not None:
            staged_outputs.append((preview_set, preview_set_destination))
        publish_outputs(staged_outputs)

    return summary


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert one Barnyard skin model and optional sequence to PSP assets")
    parser.add_argument("--tmdl", type=pathlib.Path, required=True, help="path to the host TMDL executable")
    parser.add_argument("--model", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--name", help="output basename; defaults to the model filename")
    parser.add_argument("--keylib", type=pathlib.Path)
    parser.add_argument("--sequence")
    parser.add_argument("--material", help="select one material from a multi-material model")
    parser.add_argument("--texture", type=pathlib.Path, help="ASCII P3 PPM or preconverted PTEX")
    parser.add_argument("--sample-rate", type=int, default=30)
    parser.add_argument("--keep-intermediate", action="store_true")
    parser.add_argument(
        "--runtime-preview",
        action="store_true",
        help="write barnyard_preview.pmes/.pskl/.panm for optional EBOOT loading",
    )
    args = parser.parse_args()

    summary = run_pipeline(
        args.tmdl,
        args.model,
        args.output,
        args.name or args.model.stem,
        args.keylib,
        args.sequence,
        args.sample_rate,
        args.keep_intermediate,
        args.runtime_preview,
        args.texture,
        args.material,
    )
    print(json.dumps(summary, sort_keys=True))


if __name__ == "__main__":
    main()
