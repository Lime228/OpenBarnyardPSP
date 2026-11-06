#!/usr/bin/env python3

import json
import pathlib
import tempfile
import unittest

from merge_terrain_dumps import merge


MESH = {
    "version": 1,
    "material": "barn",
    "texture": "World\\barn.tga",
    "vertices": [
        {"position": [0, 0, 0], "uv": [0, 0], "colour": [1, 1, 1]},
        {"position": [1, 0, 0], "uv": [1, 0], "colour": [1, 1, 1]},
        {"position": [0, 1, 0], "uv": [0, 1], "colour": [1, 1, 1]},
    ],
    "triangle_strip": [0, 1, 2],
}


class TerrainDumpMergeTests(unittest.TestCase):
    def test_merges_explicit_meshes_in_order(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            sources = []
            for index in range(2):
                source = temporary / f"mesh_{index}.json"
                source.write_text(json.dumps({**MESH, "material": f"barn_{index}"}), encoding="utf-8")
                sources.append(source)
            output = temporary / "cell.json"
            merged = merge(sources, output, 2, 3)
            self.assertEqual((merged["world"], merged["cell"]), (2, 3))
            self.assertEqual([mesh["material"] for mesh in merged["meshes"]], ["barn_0", "barn_1"])
            self.assertNotIn("version", merged["meshes"][0])
            self.assertEqual(json.loads(output.read_text(encoding="utf-8")), merged)

    def test_rejects_more_than_runtime_mesh_budget(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            source = pathlib.Path(temporary_name) / "mesh.json"
            source.write_text(json.dumps(MESH), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "between 1 and 4"):
                merge([source] * 5, source.with_name("cell.json"), 0, 0)


if __name__ == "__main__":
    unittest.main()
