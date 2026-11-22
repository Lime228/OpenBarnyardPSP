#!/usr/bin/env python3
import argparse
import json
import os
import shutil
import struct
import tempfile
from pathlib import Path


MAGIC = 0x50444650
VERSION = 1
HEADER = struct.Struct("<6I")
ENTRY = struct.Struct("<5I")
MAX_MODELS = 48
MAX_TEXTURES = 48


def read_config(path, expected_models, expected_textures):
    data = path.read_bytes()
    if len(data) < HEADER.size:
        raise ValueError(f"truncated config: {path}")
    magic, version, models, textures, entry_size, reserved = HEADER.unpack_from(data)
    if (
        magic != MAGIC
        or version != VERSION
        or models != expected_models
        or textures != expected_textures
        or entry_size != ENTRY.size
        or reserved != 0
        or len(data) != HEADER.size + models * ENTRY.size
    ):
        raise ValueError(f"invalid config: {path}")
    return [ENTRY.unpack_from(data, HEADER.size + index * ENTRY.size) for index in range(models)]


def combine(source_manifest_paths, output_directory, prefix):
    output_directory = Path(output_directory)
    sources = []
    models = []
    textures = []
    texture_lookup = {}
    texture_payloads = []
    model_payloads = []
    entries = []

    for source_manifest_path in map(Path, source_manifest_paths):
        manifest = json.loads(source_manifest_path.read_text(encoding="utf-8"))
        if manifest.get("version") != 1 or not manifest.get("models") or not manifest.get("textures"):
            raise ValueError(f"invalid source manifest: {source_manifest_path}")
        source_directory = source_manifest_path.parent
        source_prefix = manifest.get("prefix")
        source_entries = read_config(
            source_directory / f"{source_prefix}.pcfg",
            len(manifest["models"]),
            len(manifest["textures"]),
        )
        sources.append(
            {
                "model_index": manifest["model_index"],
                "model_name": manifest["model_name"],
                "model_path": manifest["model_path"],
                "source_meshes": manifest["source_meshes"],
                "source_instances": manifest["source_instances"],
            }
        )

        source_texture_indices = {}
        for source_texture_index, texture in enumerate(manifest["textures"]):
            source_name = texture["source_texture"]
            key = source_name.casefold()
            payload = (source_directory / texture["texture"]).read_bytes()
            if key in texture_lookup:
                texture_index = texture_lookup[key]
                if texture_payloads[texture_index] != payload:
                    raise ValueError(f"shared texture payload mismatch: {source_name}")
            else:
                texture_index = len(textures)
                texture_lookup[key] = texture_index
                texture_payloads.append(payload)
                textures.append(
                    {
                        "texture": f"{prefix}_texture_{texture_index}.ptex",
                        "source_texture": source_name,
                    }
                )
            source_texture_indices[source_texture_index] = texture_index

        for source_model_index, model in enumerate(manifest["models"]):
            model_index, texture_index, alpha_mode, address_u, address_v = source_entries[source_model_index]
            if model_index != source_model_index or texture_index not in source_texture_indices:
                raise ValueError(f"invalid source binding: {source_manifest_path}")
            combined_model_index = len(models)
            combined_texture_index = source_texture_indices[texture_index]
            model_payloads.append((source_directory / model["mesh"]).read_bytes())
            models.append(
                {
                    "mesh": f"{prefix}_model_{combined_model_index}.pmes",
                    "source_model_index": manifest["model_index"],
                    "source_model_name": manifest["model_name"],
                    "source_mesh": model["mesh"],
                    "material": model["material"],
                    "source_texture": model["source_texture"],
                    "texture_index": combined_texture_index,
                    "vertices": model["vertices"],
                    "indices": model["indices"],
                    "bytes": model["bytes"],
                }
            )
            entries.append(
                (combined_model_index, combined_texture_index, alpha_mode, address_u, address_v)
            )

    if not models or len(models) > MAX_MODELS or not textures or len(textures) > MAX_TEXTURES:
        raise ValueError("combined frontend instance pack exceeds PSP bounds")

    output_directory.mkdir(parents=True, exist_ok=True)
    temporary_directory = Path(tempfile.mkdtemp(prefix=f".{prefix}.", dir=output_directory))
    try:
        for index, payload in enumerate(model_payloads):
            (temporary_directory / f"{prefix}_model_{index}.pmes").write_bytes(payload)
        for index, payload in enumerate(texture_payloads):
            (temporary_directory / f"{prefix}_texture_{index}.ptex").write_bytes(payload)
        config = bytearray(HEADER.pack(MAGIC, VERSION, len(models), len(textures), ENTRY.size, 0))
        for entry in entries:
            config.extend(ENTRY.pack(*entry))
        (temporary_directory / f"{prefix}.pcfg").write_bytes(config)
        result = {
            "version": 1,
            "prefix": prefix,
            "source_model_count": len(sources),
            "source_instances": sum(source["source_instances"] for source in sources),
            "sources": sources,
            "models": models,
            "textures": textures,
        }
        (temporary_directory / f"{prefix}.json").write_text(
            json.dumps(result, indent=2) + "\n", encoding="utf-8"
        )

        for stale_path in output_directory.glob(f"{prefix}*"):
            if stale_path.is_file():
                stale_path.unlink()
        for generated_path in temporary_directory.iterdir():
            os.replace(generated_path, output_directory / generated_path.name)
        return result
    finally:
        shutil.rmtree(temporary_directory, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output_directory")
    parser.add_argument("source_manifests", nargs="+")
    parser.add_argument("--prefix", required=True)
    args = parser.parse_args()
    result = combine(args.source_manifests, args.output_directory, args.prefix)
    print(
        json.dumps(
            {
                "models": len(result["models"]),
                "source_instances": result["source_instances"],
                "source_models": result["source_model_count"],
                "textures": len(result["textures"]),
            },
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
