#!/usr/bin/env python3

import json
import pathlib
import struct
import tempfile
import unittest

import select_frontend_terrain as selector


def write_camera(path: pathlib.Path) -> None:
    frames = [
        (0.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 1.0, 0.0),
        (1.0, 0.0, 0.0, 0.0, 0.0, -1.0, 0.0, 1.0, 0.0),
    ]
    payload = bytearray(selector.PCAM_HEADER.pack(selector.PCAM_MAGIC, 1, len(frames), 30, selector.PCAM_FRAME.size, 1))
    for frame in frames:
        payload.extend(selector.PCAM_FRAME.pack(*frame))
    path.write_bytes(payload)


def inventory_document() -> dict[str, object]:
    meshes = [
        {"index": 0, "material": "center", "texture": "center.tga", "vertices": 10, "indices": 12, "sphere": [0.0, -10.0, 0.0, 1.0]},
        {"index": 1, "material": "right", "texture": "right.tga", "vertices": 8, "indices": 9, "sphere": [4.0, -10.0, 0.0, 0.5]},
        {"index": 2, "material": "behind", "texture": "behind.tga", "vertices": 6, "indices": 6, "sphere": [0.0, 10.0, 0.0, 0.5]},
        {"index": 3, "material": "offscreen", "texture": "offscreen.tga", "vertices": 7, "indices": 9, "sphere": [20.0, -10.0, 0.0, 0.1]},
    ]
    return {"version": 1, "world": 0, "cell": 0, "mesh_count": len(meshes), "meshes": meshes}


class SelectFrontendTerrainTests(unittest.TestCase):
    def test_reads_camera_and_selects_visible_rotated_spheres(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            camera_path = temporary / "camera.pcam"
            inventory_path = temporary / "inventory.json"
            write_camera(camera_path)
            inventory_path.write_text(json.dumps(inventory_document()), encoding="utf-8")

            frames = selector.read_camera(camera_path)
            meshes = selector.read_inventory(inventory_path)
            result = selector.plan_residency(meshes, frames, limit=2)

            self.assertEqual(2, result["camera_frames"])
            self.assertEqual(2, result["visible_meshes"])
            self.assertEqual([0, 1], result["priority_indices"])
            self.assertEqual(2, result["ranked_visible_meshes"][0]["visible_frames"])
            self.assertEqual([0.0, 0.0, -10.0, 1.0], result["ranked_visible_meshes"][0]["world_sphere"])

    def test_rejects_inconsistent_inventory_count(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            inventory_path = pathlib.Path(temporary_name) / "inventory.json"
            document = inventory_document()
            document["mesh_count"] = 99
            inventory_path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "mesh count"):
                selector.read_inventory(inventory_path)

    def test_rejects_truncated_camera(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            camera_path = pathlib.Path(temporary_name) / "camera.pcam"
            camera_path.write_bytes(struct.pack("<I", selector.PCAM_MAGIC))
            with self.assertRaisesRegex(ValueError, "truncated"):
                selector.read_camera(camera_path)

    def test_enforces_psp_selection_limit(self) -> None:
        with self.assertRaisesRegex(ValueError, "between 1 and 16"):
            selector.plan_residency([], [], limit=17)


if __name__ == "__main__":
    unittest.main()
