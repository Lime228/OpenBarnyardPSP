#!/usr/bin/env python3

import json
import pathlib
import struct
import tempfile
import unittest

from convert_frontend_terrain_pack import convert


def mesh(material: str, texture: str, offset: float) -> dict[str, object]:
    return {
        "material": material,
        "texture": texture,
        "vertices": [
            {"position": [offset, 0, 0], "uv": [0, 0], "colour": [1, 1, 1]},
            {"position": [offset + 1, 0, 0], "uv": [1, 0], "colour": [1, 1, 1]},
            {"position": [offset, 1, 0], "uv": [0, 1], "colour": [1, 1, 1]},
        ],
        "triangle_strip": [0, 1, 2],
    }


class ConvertFrontendTerrainPackTests(unittest.TestCase):
    def test_merges_exact_material_groups_and_shares_textures(self) -> None:
        document = {
            "version": 2,
            "meshes": [
                mesh("fence", "World\\fence.tga", 0),
                mesh("fence", "World\\fence.tga", 2),
                mesh("fence_variant", "World\\fence.tga", 4),
            ],
        }
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            source = temporary / "source.json"
            output = temporary / "output"
            source.write_text(json.dumps(document), encoding="utf-8")
            manifest = convert(source, output)
            self.assertEqual(3, manifest["source_meshes"])
            self.assertEqual(2, len(manifest["models"]))
            self.assertEqual(1, len(manifest["textures"]))
            self.assertEqual([0, 1], manifest["models"][0]["source_meshes"])
            self.assertEqual(6, manifest["models"][0]["vertices"])
            self.assertEqual(6, manifest["models"][0]["indices"])
            payload = (output / "frontend_detail_model_0.pmes").read_bytes()
            self.assertEqual((0, 1, 2, 3, 4, 5), struct.unpack_from("<6H", payload, 24 + 6 * 24))


if __name__ == "__main__":
    unittest.main()
