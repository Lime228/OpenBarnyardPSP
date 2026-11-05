#!/usr/bin/env python3

import argparse
import json
import math
import pathlib
import struct


PWLD_MAGIC = 0x444C5750
PWLD_VERSION = 1
PWLD_HEADER_SIZE = 16
PWLD_INSTANCE_SIZE = 32
MAX_INSTANCE_COUNT = 16
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


def convert(source: pathlib.Path, destination: pathlib.Path) -> dict[str, object]:
    document = json.loads(source.read_text(encoding="utf-8"))
    if not isinstance(document, dict) or document.get("version") != PWLD_VERSION:
        raise ValueError("unsupported PWLD source version")
    instances = document.get("instances")
    if not isinstance(instances, list) or not 1 <= len(instances) <= MAX_INSTANCE_COUNT:
        raise ValueError("world must contain between 1 and 16 instances")

    payload = bytearray(
        struct.pack(
            "<4I",
            PWLD_MAGIC,
            PWLD_VERSION,
            len(instances),
            PWLD_INSTANCE_SIZE,
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
        if not isinstance(model, int) or model != 0:
            raise ValueError(f"instance {index} requires PSP model slot zero")
        payload.extend(struct.pack("<7fI", *position, *rotation, scale, model))

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(payload)
    return {
        "instances": len(instances),
        "bytes": len(payload),
        "model_slots": 1,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert a bounded JSON world layout to OpenBarnyard PSP PWLD")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()
    print(json.dumps(convert(args.source, args.destination), sort_keys=True))


if __name__ == "__main__":
    main()
