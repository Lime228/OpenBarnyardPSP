#!/usr/bin/env python3

import argparse
import json
import math
import pathlib
import struct

from convert_skeleton import MAX_BONE_COUNT, read_bones, read_vector


PANM_MAGIC = 0x4D4E4150
PANM_VERSION = 1
PANM_SAMPLE_SIZE = 16
MAX_FRAME_COUNT = 64
MAX_SAMPLE_RATE = 60
QUANTIZED_MAX = 32767


def normalize_quaternion(value: object, name: str) -> list[float]:
    rotation = read_vector(value, 4, name)
    length = math.sqrt(sum(component * component for component in rotation))
    if length <= 0.000001:
        raise ValueError(f"{name} is zero")
    return [component / length for component in rotation]


def read_animation(
    skeleton_path: pathlib.Path, animation_path: pathlib.Path
) -> tuple[list[str], int, bool, list[list[tuple[list[float], list[float]]]]]:
    bones = read_bones(skeleton_path)
    bone_names = [bone[0] for bone in bones]
    if len(bone_names) > MAX_BONE_COUNT:
        raise ValueError(f"animation exceeds the {MAX_BONE_COUNT}-bone limit")

    document = json.loads(animation_path.read_text(encoding="utf-8"))
    if not isinstance(document, dict):
        raise ValueError("animation must be an object")
    sample_rate = document.get("sample_rate")
    if not isinstance(sample_rate, int) or sample_rate < 1 or sample_rate > MAX_SAMPLE_RATE:
        raise ValueError(f"sample_rate must be between 1 and {MAX_SAMPLE_RATE}")
    loop = document.get("loop", False)
    if not isinstance(loop, bool):
        raise ValueError("loop must be a boolean")

    source_frames = document.get("frames")
    if not isinstance(source_frames, list) or not source_frames:
        raise ValueError("animation requires a non-empty frames array")
    if len(source_frames) > MAX_FRAME_COUNT:
        raise ValueError(f"animation exceeds the {MAX_FRAME_COUNT}-frame limit")

    frames: list[list[tuple[list[float], list[float]]]] = []
    expected_names = set(bone_names)
    for frame_index, source_frame in enumerate(source_frames):
        if not isinstance(source_frame, dict) or set(source_frame) != expected_names:
            raise ValueError(f"frame {frame_index} must define every skeleton bone exactly once")
        frame: list[tuple[list[float], list[float]]] = []
        for bone_name in bone_names:
            value = source_frame[bone_name]
            if not isinstance(value, dict):
                raise ValueError(f"frame {frame_index} bone {bone_name} must be an object")
            rotation = normalize_quaternion(
                value.get("rotation", [0.0, 0.0, 0.0, 1.0]),
                f"frame {frame_index} bone {bone_name} rotation",
            )
            translation = read_vector(
                value.get("translation", [0.0, 0.0, 0.0]),
                3,
                f"frame {frame_index} bone {bone_name} translation",
            )
            frame.append((rotation, translation))
        frames.append(frame)
    return bone_names, sample_rate, loop, frames


def quantize(value: float) -> int:
    return max(-QUANTIZED_MAX, min(QUANTIZED_MAX, round(value * QUANTIZED_MAX)))


def convert(skeleton_path: pathlib.Path, animation_path: pathlib.Path, destination: pathlib.Path) -> None:
    bone_names, sample_rate, loop, frames = read_animation(skeleton_path, animation_path)
    translation_scale = max(
        1.0,
        max(abs(component) for frame in frames for _, translation in frame for component in translation),
    )
    flags = 1 if loop else 0
    payload = bytearray(
        struct.pack(
            "<6IfI",
            PANM_MAGIC,
            PANM_VERSION,
            len(bone_names),
            len(frames),
            sample_rate,
            PANM_SAMPLE_SIZE,
            translation_scale,
            flags,
        )
    )
    for frame in frames:
        for rotation, translation in frame:
            quantized_rotation = [quantize(component) for component in rotation]
            quantized_translation = [quantize(component / translation_scale) for component in translation]
            payload.extend(struct.pack("<4h3hh", *quantized_rotation, *quantized_translation, 0))

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(payload)


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert sampled JSON animation to OpenBarnyard PSP PANM")
    parser.add_argument("skeleton", type=pathlib.Path)
    parser.add_argument("animation", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()
    convert(args.skeleton, args.animation, args.destination)


if __name__ == "__main__":
    main()
