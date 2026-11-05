#!/usr/bin/env python3

import argparse
import json
import math
import pathlib
import struct


PWLD_MAGIC = 0x444C5750
PWLD_VERSION = 2
PWLD_HEADER_SIZE = 24
PWLD_INSTANCE_SIZE = 32
PWLD_RESOURCE_SIZE = 48
MAX_INSTANCE_COUNT = 16
MAX_MODEL_RESOURCES = 4
MAX_RESOURCE_NAME_LENGTH = 23
MAX_POSITION = 1000.0
MIN_SCALE = 0.001
MAX_SCALE = 100.0


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

    payload = bytearray(
        struct.pack(
            "<6I",
            PWLD_MAGIC,
            PWLD_VERSION,
            len(instances),
            PWLD_INSTANCE_SIZE,
            len(resources),
            PWLD_RESOURCE_SIZE,
        )
    )
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
        payload.extend(struct.pack("<7fI", *position, *rotation, scale, model))
    for model, texture in parsed_resources:
        payload.extend(struct.pack("<24s24s", model, texture))

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(payload)
    return {
        "instances": len(instances),
        "bytes": len(payload),
        "model_slots": len({instance.get("model", 0) for instance in instances}),
        "resources": len(resources),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert a bounded JSON world layout to OpenBarnyard PSP PWLD")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()
    print(json.dumps(convert(args.source, args.destination), sort_keys=True))


if __name__ == "__main__":
    main()
