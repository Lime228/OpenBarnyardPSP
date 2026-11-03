#!/usr/bin/env python3

import argparse
import json
import math
import pathlib
import struct


PSKL_MAGIC = 0x4C4B5350
PSKL_VERSION = 1
PSKL_BONE_SIZE = 64
MAX_BONE_COUNT = 64
MAX_NAME_LENGTH = 31


def read_vector(value: object, size: int, name: str) -> list[float]:
    if not isinstance(value, list) or len(value) != size:
        raise ValueError(f"{name} must contain exactly {size} numbers")
    result = [float(component) for component in value]
    if not all(math.isfinite(component) for component in result):
        raise ValueError(f"{name} contains a non-finite value")
    return result


def read_bones(source: pathlib.Path) -> list[tuple[str, int, list[float], list[float]]]:
    document = json.loads(source.read_text(encoding="utf-8"))
    values = document.get("bones") if isinstance(document, dict) else None
    if not isinstance(values, list) or not values:
        raise ValueError("skeleton must contain a non-empty bones array")
    if len(values) > MAX_BONE_COUNT:
        raise ValueError(f"PSP skeleton exceeds the {MAX_BONE_COUNT}-bone limit")

    bones: list[tuple[str, int, list[float], list[float]]] = []
    indices: dict[str, int] = {}
    for index, value in enumerate(values):
        if not isinstance(value, dict):
            raise ValueError(f"bone {index} must be an object")

        name = value.get("name")
        if not isinstance(name, str) or not name:
            raise ValueError(f"bone {index} requires a non-empty name")
        try:
            encoded_name = name.encode("ascii")
        except UnicodeEncodeError as error:
            raise ValueError(f"bone {index} name must be ASCII") from error
        if len(encoded_name) > MAX_NAME_LENGTH:
            raise ValueError(f"bone {index} name exceeds {MAX_NAME_LENGTH} bytes")

        folded_name = name.casefold()
        if folded_name in indices:
            raise ValueError(f"duplicate bone name: {name}")

        parent_value = value.get("parent")
        if index == 0:
            if parent_value is not None:
                raise ValueError("the first bone must be the root with a null parent")
            parent = -1
        else:
            if not isinstance(parent_value, str) or parent_value.casefold() not in indices:
                raise ValueError(f"bone {name} parent must name an earlier bone")
            parent = indices[parent_value.casefold()]

        translation = read_vector(value.get("translation", [0.0, 0.0, 0.0]), 3, f"bone {name} translation")
        rotation = read_vector(value.get("rotation", [0.0, 0.0, 0.0, 1.0]), 4, f"bone {name} rotation")
        length = math.sqrt(sum(component * component for component in rotation))
        if length <= 0.000001:
            raise ValueError(f"bone {name} rotation quaternion is zero")
        rotation = [component / length for component in rotation]

        indices[folded_name] = index
        bones.append((name, parent, rotation, translation))

    return bones


def convert(source: pathlib.Path, destination: pathlib.Path) -> None:
    bones = read_bones(source)
    payload = bytearray(struct.pack("<4I", PSKL_MAGIC, PSKL_VERSION, len(bones), PSKL_BONE_SIZE))
    for name, parent, rotation, translation in bones:
        encoded_name = name.encode("ascii")
        payload.extend(struct.pack("<hBB32s4f3f", parent, len(encoded_name), 0, encoded_name, *rotation, *translation))

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(payload)


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert a bounded JSON skeleton to OpenBarnyard PSP PSKL")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()
    convert(args.source, args.destination)


if __name__ == "__main__":
    main()
