#!/usr/bin/env python3

import argparse
import json
import math
import pathlib
import struct


PWLD_MAGIC = 0x444C5750
PWLD_VERSION = 3
PWLD_HEADER_SIZE = 32
PWLD_INSTANCE_SIZE = 32
PWLD_RESOURCE_SIZE = 48
PWLD_CELL_SIZE = 24
MAX_INSTANCE_COUNT = 16
MAX_MODEL_RESOURCES = 4
MAX_CELL_COUNT = 8
MAX_RESOURCE_NAME_LENGTH = 23
MAX_POSITION = 1000.0
MIN_SCALE = 0.001
MAX_SCALE = 100.0
DEFAULT_CELL_SIZE = 16.0
MIN_CELL_SIZE = 1.0
MAX_CELL_SIZE = 256.0


def vector(value: object, size: int, name: str) -> list[float]:
    if not isinstance(value, list) or len(value) != size:
        raise ValueError(f"{name} must contain {size} values")
    result = [float(component) for component in value]
    if not all(math.isfinite(component) for component in result):
        raise ValueError(f"{name} contains a non-finite value")
    return result


def is_resource_name_character(character: int) -> bool:
    return (
        character in {45, 46, 95}
        or 48 <= character <= 57
        or 65 <= character <= 90
        or 97 <= character <= 122
    )


def resource_name(value: object, name: str) -> bytes:
    if not isinstance(value, str) or not value:
        raise ValueError(f"{name} must be a non-empty string")
    try:
        encoded = value.encode("ascii")
    except UnicodeEncodeError as error:
        raise ValueError(f"{name} must be ASCII") from error
    if len(encoded) > MAX_RESOURCE_NAME_LENGTH or not all(
        is_resource_name_character(character) for character in encoded
    ):
        raise ValueError(f"{name} is not a valid PSP resource name")
    return encoded


def convert(source: pathlib.Path, destination: pathlib.Path) -> dict[str, object]:
    document = json.loads(source.read_text(encoding="utf-8"))
    if not isinstance(document, dict) or document.get("version") != PWLD_VERSION:
        raise ValueError("unsupported PWLD source version")
    instances = document.get("instances")
    if not isinstance(instances, list) or not 1 <= len(instances) <= MAX_INSTANCE_COUNT:
        raise ValueError("world must contain between 1 and 16 instances")
    resources = document.get("resources")
    if not isinstance(resources, list) or not 1 <= len(resources) <= MAX_MODEL_RESOURCES:
        raise ValueError("world must contain between 1 and 4 resources")

    parsed_resources = []
    for index, resource in enumerate(resources):
        if not isinstance(resource, dict):
            raise ValueError(f"resource {index} must be an object")
        parsed_resources.append(
            (
                resource_name(resource.get("model"), f"resource {index} model"),
                resource_name(resource.get("texture"), f"resource {index} texture"),
            )
        )

    cell_size = float(document.get("cell_size", DEFAULT_CELL_SIZE))
    if not math.isfinite(cell_size) or not MIN_CELL_SIZE <= cell_size <= MAX_CELL_SIZE:
        raise ValueError("cell size exceeds the PSP world limit")

    parsed_instances = []
    for index, instance in enumerate(instances):
        if not isinstance(instance, dict):
            raise ValueError(f"instance {index} must be an object")
        position = vector(instance.get("position"), 3, f"instance {index} position")
        rotation = vector(instance.get("rotation", [0.0, 0.0, 0.0]), 3, f"instance {index} rotation")
        if any(abs(component) > MAX_POSITION for component in position):
            raise ValueError(f"instance {index} position exceeds the PSP world limit")
        scale = float(instance.get("scale", 1.0))
        if not math.isfinite(scale) or scale < MIN_SCALE or scale > MAX_SCALE:
            raise ValueError(f"instance {index} scale exceeds the PSP world limit")
        model = instance.get("model", 0)
        if not isinstance(model, int) or isinstance(model, bool) or not 0 <= model < len(resources):
            raise ValueError(f"instance {index} model slot exceeds the PSP world limit")
        parsed_instances.append((position, rotation, scale, model))

    cell_groups: dict[tuple[int, int], list[tuple[list[float], list[float], float, int]]] = {}
    for instance in parsed_instances:
        position = instance[0]
        half_cell = cell_size * 0.5
        key = (
            math.floor((position[0] + half_cell) / cell_size),
            math.floor((position[2] + half_cell) / cell_size),
        )
        cell_groups.setdefault(key, []).append(instance)
    if len(cell_groups) > MAX_CELL_COUNT:
        raise ValueError("world exceeds the PSP cell limit")

    ordered_instances = []
    cells = []
    for key in sorted(cell_groups):
        group = cell_groups[key]
        first_instance = len(ordered_instances)
        ordered_instances.extend(group)
        center = [
            (min(instance[0][axis] for instance in group) + max(instance[0][axis] for instance in group)) * 0.5
            for axis in range(3)
        ]
        radius = max(
            math.sqrt(sum((instance[0][axis] - center[axis]) ** 2 for axis in range(3)))
            + instance[2] * 1.75
            for instance in group
        ) + 0.001
        cells.append((center, radius, first_instance, len(group)))

    payload = bytearray(
        struct.pack(
            "<8I",
            PWLD_MAGIC,
            PWLD_VERSION,
            len(ordered_instances),
            PWLD_INSTANCE_SIZE,
            len(resources),
            PWLD_RESOURCE_SIZE,
            len(cells),
            PWLD_CELL_SIZE,
        )
    )
    for position, rotation, scale, model in ordered_instances:
        payload.extend(struct.pack("<7fI", *position, *rotation, scale, model))
    for model, texture in parsed_resources:
        payload.extend(struct.pack("<24s24s", model, texture))
    for center, radius, first_instance, instance_count in cells:
        payload.extend(struct.pack("<4f2I", *center, radius, first_instance, instance_count))

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(payload)
    return {
        "instances": len(instances),
        "bytes": len(payload),
        "model_slots": len({instance.get("model", 0) for instance in instances}),
        "resources": len(resources),
        "cells": len(cells),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert a bounded JSON world layout to OpenBarnyard PSP PWLD")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()
    print(json.dumps(convert(args.source, args.destination), sort_keys=True))


if __name__ == "__main__":
    main()
