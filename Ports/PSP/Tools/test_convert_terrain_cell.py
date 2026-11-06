#!/usr/bin/env python3

import json
import pathlib
import tempfile
import unittest

from convert_terrain_cell import convert


def mesh(material: str, offset: float) -> dict[str, object]:
    return {
        "material": material,
        "texture": f"World\\{material}.tga",
        "sphere": [offset, 0, 0, 1],
        "vertices": [
            {"position": [offset, 0, 0], "uv": [0, 0], "colour": [1, 1, 1]},
            {"position": [offset + 1, 0, 0], "uv": [1, 0], "colour": [1, 1, 1]},
            {"position": [offset, 1, 0], "uv": [0, 1], "colour": [1, 1, 1]},
        ],
        "triangle_strip": [0, 1, 2],
    }


class TerrainCellConversionTests(unittest.TestCase):
    def test_converts_two_meshes_with_shared_bounds(self) -> None:
        document = {"version": 2, "world": 0, "cell": 3, "meshes": [mesh("inner", 0), mesh("outer", 2)]}
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            source = temporary / "cell.json"
            output = temporary / "output"
            source.write_text(json.dumps(document), encoding="utf-8")
            manifest = convert(source, output)
            self.assertEqual(manifest["center"], [1.5, 0.5, 0.0])
            self.assertAlmostEqual(manifest["scale"], 2.0 / 3.0)
            self.assertEqual(len(manifest["meshes"]), 2)
            self.assertEqual((output / "barnyard_terrain_0.pmes").stat().st_size, 102)
            self.assertEqual((output / "barnyard_terrain_1.pmes").stat().st_size, 102)
            self.assertEqual(json.loads((output / "barnyard_terrain.json").read_text())["cell"], 3)

    def test_rejects_more_than_four_meshes(self) -> None:
        document = {"version": 2, "meshes": [mesh(str(index), float(index)) for index in range(5)]}
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            source = temporary / "cell.json"
            source.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "between 1 and 4"):
                convert(source, temporary / "output")


if __name__ == "__main__":
    unittest.main()
