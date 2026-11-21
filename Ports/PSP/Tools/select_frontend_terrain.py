#!/usr/bin/env python3

import argparse
import json
import math
import pathlib
import struct


PCAM_MAGIC = 0x4D414350
PCAM_VERSION = 1
PCAM_HEADER = struct.Struct("<6I")
PCAM_FRAME = struct.Struct("<9f")
DEFAULT_FOV_DEGREES = 60.0
DEFAULT_ASPECT = 480.0 / 272.0
DEFAULT_NEAR = 1.0
DEFAULT_FAR = 280.0
MAX_SELECTED_MESHES = 16


def dot(left: tuple[float, float, float], right: tuple[float, float, float]) -> float:
    return sum(left[index] * right[index] for index in range(3))


def cross(left: tuple[float, float, float], right: tuple[float, float, float]) -> tuple[float, float, float]:
    return (
        left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0],
    )


def normalise(vector: tuple[float, float, float], label: str) -> tuple[float, float, float]:
    length_squared = dot(vector, vector)
    if not math.isfinite(length_squared) or length_squared <= 1.0e-12:
        raise ValueError(f"{label} has zero or non-finite length")
    inverse_length = 1.0 / math.sqrt(length_squared)
    return tuple(component * inverse_length for component in vector)


def read_camera(path: pathlib.Path) -> list[dict[str, tuple[float, float, float]]]:
    payload = path.read_bytes()
    if len(payload) < PCAM_HEADER.size:
        raise ValueError("PCAM header is truncated")
    magic, version, frame_count, sample_rate, frame_size, flags = PCAM_HEADER.unpack_from(payload)
    if magic != PCAM_MAGIC or version != PCAM_VERSION:
        raise ValueError("unsupported PCAM contract")
    if not 1 <= frame_count <= 64 or not 1 <= sample_rate <= 60:
        raise ValueError("PCAM frame count or sample rate is out of bounds")
    if frame_size != PCAM_FRAME.size or flags & ~1:
        raise ValueError("PCAM frame layout or flags are invalid")
    if len(payload) != PCAM_HEADER.size + frame_count * PCAM_FRAME.size:
        raise ValueError("PCAM size does not match its declared frame count")

    frames = []
    for frame_index in range(frame_count):
        values = PCAM_FRAME.unpack_from(payload, PCAM_HEADER.size + frame_index * PCAM_FRAME.size)
        if not all(math.isfinite(value) for value in values):
            raise ValueError(f"PCAM frame {frame_index} contains a non-finite value")
        position = tuple(values[0:3])
        direction = normalise(tuple(values[3:6]), f"PCAM frame {frame_index} direction")
        source_up = normalise(tuple(values[6:9]), f"PCAM frame {frame_index} up")
        projection = dot(direction, source_up)
        up = normalise(
            tuple(source_up[axis] - direction[axis] * projection for axis in range(3)),
            f"PCAM frame {frame_index} projected up",
        )
        right = normalise(cross(direction, up), f"PCAM frame {frame_index} right")
        frames.append({"position": position, "direction": direction, "up": up, "right": right})
    return frames


def read_inventory(path: pathlib.Path) -> list[dict[str, object]]:
    document = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(document, dict) or document.get("version") != 1:
        raise ValueError("unsupported terrain inventory contract")
    meshes = document.get("meshes")
    if not isinstance(meshes, list) or not 1 <= len(meshes) <= 1024:
        raise ValueError("terrain inventory requires between 1 and 1024 meshes")
    if document.get("mesh_count") != len(meshes):
        raise ValueError("terrain inventory mesh count does not match the mesh list")

    for expected_index, mesh in enumerate(meshes):
        if not isinstance(mesh, dict) or mesh.get("index") != expected_index:
            raise ValueError(f"terrain inventory mesh {expected_index} is missing or out of order")
        if not isinstance(mesh.get("material"), str) or not isinstance(mesh.get("texture"), str):
            raise ValueError(f"terrain inventory mesh {expected_index} names are invalid")
        vertices = mesh.get("vertices")
        indices = mesh.get("indices")
        sphere = mesh.get("sphere")
        if not isinstance(vertices, int) or not 1 <= vertices <= 0xFFFF:
            raise ValueError(f"terrain inventory mesh {expected_index} vertex count is invalid")
        if not isinstance(indices, int) or not 3 <= indices <= 0xFFFF:
            raise ValueError(f"terrain inventory mesh {expected_index} index count is invalid")
        if not isinstance(sphere, list) or len(sphere) != 4:
            raise ValueError(f"terrain inventory mesh {expected_index} sphere is invalid")
        parsed_sphere = [float(component) for component in sphere]
        if not all(math.isfinite(component) for component in parsed_sphere) or parsed_sphere[3] < 0.0:
            raise ValueError(f"terrain inventory mesh {expected_index} sphere is non-finite")
        mesh["sphere"] = parsed_sphere
    return meshes


def rotate_frontend_sphere(sphere: list[float]) -> tuple[tuple[float, float, float], float]:
    # ATerrainInterface::CreateModelInstance applies +pi/2 around X to world models.
    return (sphere[0], -sphere[2], sphere[1]), sphere[3]


def sphere_visible(
    center: tuple[float, float, float],
    radius: float,
    frame: dict[str, tuple[float, float, float]],
    tangent_vertical: float,
    tangent_horizontal: float,
    near: float,
    far: float,
) -> tuple[bool, float, float]:
    relative = tuple(center[axis] - frame["position"][axis] for axis in range(3))
    depth = dot(relative, frame["direction"])
    horizontal = dot(relative, frame["right"])
    vertical = dot(relative, frame["up"])
    visible = (
        depth - near >= -radius
        and far - depth >= -radius
        and abs(horizontal) <= depth * tangent_horizontal + radius * math.sqrt(1.0 + tangent_horizontal**2)
        and abs(vertical) <= depth * tangent_vertical + radius * math.sqrt(1.0 + tangent_vertical**2)
    )
    projected_radius = radius / max(depth, near) if visible else 0.0
    return visible, depth, projected_radius


def plan_residency(
    meshes: list[dict[str, object]],
    frames: list[dict[str, tuple[float, float, float]]],
    limit: int = MAX_SELECTED_MESHES,
    fov_degrees: float = DEFAULT_FOV_DEGREES,
    aspect: float = DEFAULT_ASPECT,
    near: float = DEFAULT_NEAR,
    far: float = DEFAULT_FAR,
) -> dict[str, object]:
    if not 1 <= limit <= MAX_SELECTED_MESHES:
        raise ValueError(f"residency seed limit must be between 1 and {MAX_SELECTED_MESHES}")
    if not 1.0 <= fov_degrees < 179.0 or not math.isfinite(aspect) or aspect <= 0.0:
        raise ValueError("frustum FOV or aspect is invalid")
    if not math.isfinite(near) or not math.isfinite(far) or near <= 0.0 or far <= near:
        raise ValueError("frustum near/far range is invalid")

    tangent_vertical = math.tan(math.radians(fov_degrees) * 0.5)
    tangent_horizontal = tangent_vertical * aspect
    ranked = []
    for mesh in meshes:
        center, radius = rotate_frontend_sphere(mesh["sphere"])
        visible_frames = 0
        closest_depth = math.inf
        maximum_projected_radius = 0.0
        for frame in frames:
            visible, depth, projected_radius = sphere_visible(
                center,
                radius,
                frame,
                tangent_vertical,
                tangent_horizontal,
                near,
                far,
            )
            if visible:
                visible_frames += 1
                closest_depth = min(closest_depth, depth)
                maximum_projected_radius = max(maximum_projected_radius, projected_radius)
        if visible_frames == 0:
            continue
        vertices = int(mesh["vertices"])
        indices = int(mesh["indices"])
        ranked.append(
            {
                "index": mesh["index"],
                "material": mesh["material"],
                "texture": mesh["texture"],
                "vertices": vertices,
                "indices": indices,
                "estimated_geometry_bytes": 24 + vertices * 24 + indices * 2,
                "visible_frames": visible_frames,
                "closest_depth": closest_depth,
                "maximum_projected_radius": maximum_projected_radius,
                "world_sphere": [*center, radius],
            }
        )

    ranked.sort(
        key=lambda mesh: (
            -int(mesh["visible_frames"]),
            -float(mesh["maximum_projected_radius"]),
            int(mesh["estimated_geometry_bytes"]),
            int(mesh["index"]),
        )
    )
    priority = ranked[:limit]
    return {
        "version": 1,
        "camera_frames": len(frames),
        "frustum": {
            "fov_degrees": fov_degrees,
            "aspect": aspect,
            "near": near,
            "far": far,
        },
        "inventory_meshes": len(meshes),
        "visible_meshes": len(ranked),
        "residency_seed_limit": limit,
        "priority_indices": sorted(int(mesh["index"]) for mesh in priority),
        "priority_geometry_bytes": sum(int(mesh["estimated_geometry_bytes"]) for mesh in priority),
        "ranked_visible_meshes": ranked,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Rank original front-end terrain for bounded PSP residency without changing its layout")
    parser.add_argument("inventory", type=pathlib.Path)
    parser.add_argument("camera", type=pathlib.Path)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--limit", type=int, default=MAX_SELECTED_MESHES)
    parser.add_argument("--fov", type=float, default=DEFAULT_FOV_DEGREES)
    args = parser.parse_args()
    result = plan_residency(read_inventory(args.inventory), read_camera(args.camera), args.limit, args.fov)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: result[key] for key in ("inventory_meshes", "visible_meshes", "priority_indices", "priority_geometry_bytes")}, sort_keys=True))


if __name__ == "__main__":
    main()
