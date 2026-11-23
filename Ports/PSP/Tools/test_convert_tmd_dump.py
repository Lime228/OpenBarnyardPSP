#!/usr/bin/env python3

import struct
import tempfile
import unittest
from pathlib import Path

from convert_retail_model_geometry import bake_geometry
from convert_tmd_dump import MAX_ANIMATED_VERTEX_COUNT, build_mesh, build_skeleton, reduce_weights, triangulate_strip


class TMDPSPBridgeTests(unittest.TestCase):
    def test_preserves_multiple_roots_and_rejects_cycles(self) -> None:
        skeleton = [
            {"name": "root_a", "parent": None},
            {"name": "root_b", "parent": None},
            {"name": "child_b", "parent": 1},
        ]
        buffer = build_skeleton({"skeleton": skeleton})
        self.assertEqual([struct.unpack_from("<h", buffer, 16 + index * 64)[0] for index in range(3)], [-1, -1, 1])
        skeleton[1]["parent"] = 2
        with self.assertRaisesRegex(ValueError, "earlier index"):
            build_skeleton({"skeleton": skeleton})

    def test_reduces_four_weights_to_two_strongest_global_bones(self) -> None:
        self.assertEqual(
            reduce_weights([0, 1, 2], [0, 1, 2, 0], [5, 20, 220, 10], 3),
            (2, 1, 234, 21),
        )

    def test_combines_duplicate_bones_before_reduction(self) -> None:
        self.assertEqual(
            reduce_weights([2, 1], [0, 0, 1, 1], [100, 50, 60, 40], 3),
            (2, 1, 153, 102),
        )

    def test_converts_strip_winding_and_skips_degenerates(self) -> None:
        self.assertEqual(triangulate_strip([0, 1, 2, 3], 4), [0, 1, 2, 2, 1, 3])
        self.assertEqual(triangulate_strip([0, 1, 1, 2, 3], 4), [1, 2, 3])

    def test_requires_explicit_material_for_multi_material_preview(self) -> None:
        vertex = {
            "position": [0.0, 0.0, 0.0],
            "uv": [0.0, 0.0],
            "bones": [0, 0, 0, 0],
            "weights": [255, 0, 0, 0],
        }
        submesh = {
            "bone_palette": [0],
            "vertices": [vertex, vertex, vertex],
            "triangle_strip": [0, 1, 2],
        }
        document = {
            "lods": [
                {
                    "shader": "skin",
                    "meshes": [
                        {"material": "body", "submeshes": [submesh]},
                        {"material": "eyes", "submeshes": [submesh]},
                    ],
                }
            ]
        }
        with self.assertRaisesRegex(ValueError, "supports one material.*body.*eyes"):
            build_mesh(document, 1)
        selected_mesh = build_mesh(document, 1, "EYES")
        self.assertEqual(struct.unpack_from("<II", selected_mesh, 8), (3, 3))
        with self.assertRaisesRegex(ValueError, "missing.*available: body, eyes"):
            build_mesh(document, 1, "missing")

        document["version"] = 1
        document["skeleton"] = [{"name": "root", "parent": None}]
        for mesh in document["lods"][0]["meshes"]:
            mesh["texture"] = f"{mesh['material']}.dds"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = bake_geometry(document, root)
            self.assertEqual(result["bones"], 1)
            self.assertEqual([mesh["texture"] for mesh in result["meshes"]], ["body.dds", "eyes.dds"])
            self.assertEqual(len(list(root.glob("*.pskl"))), 1)
            self.assertEqual(len(list(root.glob("*.pmes"))), 2)

    def test_rejects_mesh_above_runtime_skinning_budget(self) -> None:
        vertex = {
            "position": [0.0, 0.0, 0.0],
            "uv": [0.0, 0.0],
            "bones": [0, 0, 0, 0],
            "weights": [255, 0, 0, 0],
        }
        document = {
            "lods": [
                {
                    "shader": "skin",
                    "meshes": [
                        {
                            "material": "probe",
                            "submeshes": [
                                {
                                    "bone_palette": [0],
                                    "vertices": [vertex] * (MAX_ANIMATED_VERTEX_COUNT + 1),
                                    "triangle_strip": [0, 1, 2],
                                }
                            ]
                        }
                    ],
                }
            ]
        }
        with self.assertRaisesRegex(ValueError, "8192-vertex skinning budget"):
            build_mesh(document, 1)


if __name__ == "__main__":
    unittest.main()
