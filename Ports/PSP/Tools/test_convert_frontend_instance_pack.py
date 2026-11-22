#!/usr/bin/env python3

import json
import pathlib
import struct
import tempfile
import unittest

from convert_frontend_instance_pack import convert


def source_vertex(position: list[float], uv: list[float]) -> dict[str, object]:
    return {
        "position": position,
        "uv": uv,
        "bones": [0, 0, 0, 0],
        "weights": [255, 0, 0, 0],
    }


class FrontendInstancePackTests(unittest.TestCase):
    def write_fixture(self, root: pathlib.Path) -> tuple[pathlib.Path, pathlib.Path]:
        inventory = root / "inventory.json"
        inventory.write_text(
            json.dumps(
                {
                    "version": 1,
                    "resident_section_names": ["Barn"],
                    "instances": [
                        {
                            "index": 4,
                            "name": "fncw02_0",
                            "model_index": 9,
                            "model_name": "fncw02",
                            "model_path": "data/models/fncw02.trb",
                            "vis_section_name": "Barn",
                            "rotation": [0.0, 0.0, 0.0, 1.0],
                            "position": [10.0, 20.0, 30.0],
                            "scale": 2.0,
                        },
                        {
                            "index": 5,
                            "name": "fncw02_1",
                            "model_index": 9,
                            "model_name": "fncw02",
                            "model_path": "data/models/fncw02.trb",
                            "vis_section_name": "Hillbase",
                            "rotation": [0.0, 0.0, 0.0, 1.0],
                            "position": [100.0, 0.0, 0.0],
                            "scale": 1.0,
                        },
                    ],
                }
            ),
            encoding="utf-8",
        )
        model = root / "model.json"
        model.write_text(
            json.dumps(
                {
                    "version": 1,
                    "skeleton": [{"name": "root", "parent": None}],
                    "lods": [
                        {
                            "shader": "skin",
                            "meshes": [
                                {
                                    "material": "fencewhite",
                                    "texture": "World\\fencewhite.tga",
                                    "submeshes": [
                                        {
                                            "bone_palette": [0],
                                            "vertices": [
                                                source_vertex([0.0, 0.0, 0.0], [0.0, 0.0]),
                                                source_vertex([1.0, 0.0, 0.0], [1.0, 0.0]),
                                                source_vertex([0.0, 1.0, 0.0], [0.0, 1.0]),
                                            ],
                                            "triangle_strip": [0, 1, 2],
                                        }
                                    ],
                                }
                            ],
                        }
                    ],
                }
            ),
            encoding="utf-8",
        )
        return inventory, model

    def test_bakes_only_resident_instances_with_exact_transform(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = pathlib.Path(temporary_name)
            inventory, model = self.write_fixture(root)
            output = root / "output"
            manifest = convert(inventory, model, output, "fncw02", "frontend_instances_fncw02")
            self.assertEqual(manifest["source_instances"], 1)
            self.assertEqual(manifest["resident_sections"], ["Barn"])
            self.assertEqual(len(manifest["models"]), 1)
            self.assertEqual(len(manifest["textures"]), 1)
            payload = (output / "frontend_instances_fncw02_model_0.pmes").read_bytes()
            magic, version, vertices, indices, vertex_size, vertex_format = struct.unpack_from("<6I", payload)
            self.assertEqual((magic, version, vertices, indices, vertex_size, vertex_format), (0x53454D50, 1, 3, 3, 24, 1))
            positions = [struct.unpack_from("<3f", payload, 24 + index * 24 + 12) for index in range(3)]
            self.assertEqual(positions, [(10.0, 20.0, 30.0), (12.0, 20.0, 30.0), (10.0, 22.0, 30.0)])

    def test_rejects_model_absent_from_resident_sections(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = pathlib.Path(temporary_name)
            inventory, model = self.write_fixture(root)
            document = json.loads(inventory.read_text(encoding="utf-8"))
            document["resident_section_names"] = ["Meadowsouth"]
            inventory.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "no instances"):
                convert(inventory, model, root / "output", "fncw02", "frontend_instances_fncw02")


if __name__ == "__main__":
    unittest.main()
