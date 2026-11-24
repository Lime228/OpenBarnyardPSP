#!/usr/bin/env python3

import argparse
import json
import math
import pathlib
import struct


PCAM_MAGIC = 0x4D414350
PCAM_VERSION = 1
PCAM_FRAME_SIZE = 36
PCAM_FLAG_LOOP = 1
MAX_BONE_COUNT = 64
MAX_FRAME_COUNT = 64
MAX_SAMPLE_RATE = 60


def read_vector(value: object, size: int, name: str) -> list[float]:
    if not isinstance(value, list) or len(value) != size:
        raise ValueError(f"{name} must contain {size} values")
    result = [float(component) for component in value]
    if not all(math.isfinite(component) for component in result):
        raise ValueError(f"{name} must be finite")
    return result


def normalize(value: list[float], name: str) -> list[float]:
    length = math.sqrt(sum(component * component for component in value))
    if length <= 0.000001:
        raise ValueError(f"{name} is zero")
    return [component / length for component in value]


def quaternion_matrix(rotation: list[float], translation: list[float]) -> list[list[float]]:
    x, y, z, w = normalize(rotation, "rotation")
    return [
        [1.0 - 2.0 * z * z - 2.0 * y * y, 2.0 * y * x + 2.0 * z * w, 2.0 * z * x - 2.0 * y * w, 0.0],
        [2.0 * y * x - 2.0 * z * w, 1.0 - 2.0 * z * z - 2.0 * x * x, 2.0 * z * y + 2.0 * x * w, 0.0],
        [2.0 * z * x + 2.0 * y * w, 2.0 * z * y - 2.0 * x * w, 1.0 - 2.0 * y * y - 2.0 * x * x, 0.0],
        [translation[0], translation[1], translation[2], 1.0],
    ]


def transform_basis(matrix: list[list[float]], value: list[float]) -> list[float]:
    return [
        sum(matrix[basis][component] * value[basis] for basis in range(4))
        for component in range(4)
    ]


def multiply(left: list[list[float]], right: list[list[float]]) -> list[list[float]]:
    return [transform_basis(left, basis) for basis in right]


def convert(source: pathlib.Path, destination: pathlib.Path, camera_bone: str = "camera", preserve_roll: bool = False) -> None:
    document = json.loads(source.read_text(encoding="utf-8"))
    if not isinstance(document, dict):
        raise ValueError("camera animation must be an object")

    sample_rate = document.get("sample_rate")
    if not isinstance(sample_rate, int) or sample_rate < 1 or sample_rate > MAX_SAMPLE_RATE:
        raise ValueError(f"sample_rate must be between 1 and {MAX_SAMPLE_RATE}")
    loop = document.get("loop", False)
    if not isinstance(loop, bool):
        raise ValueError("loop must be a boolean")

    bones = document.get("bones")
    if not isinstance(bones, list) or not 1 <= len(bones) <= MAX_BONE_COUNT:
        raise ValueError(f"bones must contain between 1 and {MAX_BONE_COUNT} entries")
    bone_names: list[str] = []
    parents: list[int | None] = []
    for index, bone in enumerate(bones):
        if not isinstance(bone, dict):
            raise ValueError(f"bone {index} must be an object")
        name = bone.get("name")
        parent = bone.get("parent")
        if not isinstance(name, str) or not name or name in bone_names:
            raise ValueError(f"bone {index} requires a unique name")
        if parent is not None and (not isinstance(parent, int) or parent < 0 or parent >= index):
            raise ValueError(f"bone {index} has an invalid parent")
        bone_names.append(name)
        parents.append(parent)
    if camera_bone not in bone_names:
        raise ValueError(f"camera animation requires the {camera_bone!r} bone")
    camera_index = bone_names.index(camera_bone)

    source_frames = document.get("frames")
    if not isinstance(source_frames, list) or not 1 <= len(source_frames) <= MAX_FRAME_COUNT:
        raise ValueError(f"frames must contain between 1 and {MAX_FRAME_COUNT} entries")

    # ABoneAttachCameraHelper applies +pi/2 around X to the model before the
    # hierarchical camera-bone transform.
    model_transform = [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, -1.0, 0.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]
    frames: list[tuple[list[float], list[float], list[float]]] = []
    expected_names = set(bone_names)
    for frame_index, source_frame in enumerate(source_frames):
        if not isinstance(source_frame, dict) or set(source_frame) != expected_names:
            raise ValueError(f"frame {frame_index} must define every bone exactly once")
        world_transforms: list[list[list[float]]] = []
        for bone_index, bone_name in enumerate(bone_names):
            pose = source_frame[bone_name]
            if not isinstance(pose, dict):
                raise ValueError(f"frame {frame_index} bone {bone_name} must be an object")
            rotation = read_vector(pose.get("rotation"), 4, f"frame {frame_index} {bone_name} rotation")
            translation = read_vector(pose.get("translation"), 3, f"frame {frame_index} {bone_name} translation")
            local_transform = quaternion_matrix(rotation, translation)
            parent = parents[bone_index]
            world_transforms.append(
                local_transform if parent is None else multiply(world_transforms[parent], local_transform)
            )

        camera_transform = multiply(model_transform, world_transforms[camera_index])
        position = camera_transform[3][:3]
        direction = normalize(
            [-component for component in camera_transform[2][:3]],
            f"frame {frame_index} camera direction",
        )
        if preserve_roll:
            # ASSeqCameraHelper applies a local half-turn around X, then
            # copies the resulting bone matrix directly to ACamera.
            up_basis = camera_transform[1][:3]
        else:
            # ACamera::LookAtDirection rebuilds up instead of copying roll.
            retail_world_up = [0.0, -1.0, 0.0]
            up_dot = sum(component * direction[index] for index, component in enumerate(retail_world_up))
            up_basis = [retail_world_up[index] - direction[index] * up_dot for index in range(3)]
        up = normalize([-component for component in up_basis], f"frame {frame_index} camera up")
        frames.append((position, direction, up))

    flags = PCAM_FLAG_LOOP if loop else 0
    payload = bytearray(
        struct.pack(
            "<6I",
            PCAM_MAGIC,
            PCAM_VERSION,
            len(frames),
            sample_rate,
            PCAM_FRAME_SIZE,
            flags,
        )
    )
    for position, direction, up in frames:
        payload.extend(struct.pack("<9f", *position, *direction, *up))

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(payload)


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert a sampled Barnyard camera to bounded PSP PCAM")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()
    convert(args.source, args.destination)


if __name__ == "__main__":
    main()
