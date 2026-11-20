#!/usr/bin/env python3

import json
import pathlib
import struct
import tempfile
import unittest

from convert_placed_terrain_cell import convert


class ConvertPlacedTerrainCellTests(unittest.TestCase):
    def test_bakes_retail_locator_axis_transform(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            cell = temporary / "cell.json"
            locator = temporary / "locator.json"
            output = temporary / "output"
            cell.write_text(
                json.dumps(
                    {
                        "version": 2,
                        "world": 0,
                        "cell": 0,
                        "meshes": [
                            {
                                "material": "sign",
                                "texture": "World\\sign.tga",
                                "sphere": [0.0, 0.0, 0.0, 1.0],
                                "vertices": [
                                    {"position": [4.0, 5.0, 6.0], "uv": [0.0, 0.0], "colour": [1.0, 1.0, 1.0]},
                                    {"position": [5.0, 5.0, 6.0], "uv": [1.0, 0.0], "colour": [1.0, 1.0, 1.0]},
                                    {"position": [4.0, 6.0, 6.0], "uv": [0.0, 1.0], "colour": [1.0, 1.0, 1.0]},
                                ],
                                "triangle_strip": [0, 1, 2],
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            locator.write_text(
                json.dumps(
                    {
                        "version": 1,
                        "name": "loc_barnsign",
                        "rotation": [0.0, 0.0, 0.0, 1.0],
                        "position": [1.0, 2.0, 3.0],
                        "scale": 0.0,
                        "flags1": 0,
                        "flags2": 0,
                    }
                ),
                encoding="utf-8",
            )

            summary = convert(cell, locator, output, "frontend_barnsign", "loc_barnsign")

            self.assertEqual(summary["locator"], "loc_barnsign")
            self.assertEqual(summary["bounds_min"], [5.0, -9.0, 7.0])
            self.assertEqual(summary["bounds_max"], [6.0, -9.0, 8.0])
            payload = (output / "frontend_barnsign_0.pmes").read_bytes()
            vertex = struct.unpack_from("<ffIfff", payload, 24)
            self.assertEqual(vertex[3:], (5.0, -9.0, 7.0))

    def test_rejects_wrong_locator_name(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            cell = temporary / "cell.json"
            locator = temporary / "locator.json"
            cell.write_text(json.dumps({"version": 2, "meshes": [{}]}), encoding="utf-8")
            locator.write_text(
                json.dumps(
                    {
                        "version": 1,
                        "name": "wrong",
                        "rotation": [0.0, 0.0, 0.0, 1.0],
                        "position": [0.0, 0.0, 0.0],
                        "scale": 1.0,
                    }
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "locator must be named"):
                convert(cell, locator, temporary / "output", "frontend_barnsign", "loc_barnsign")


if __name__ == "__main__":
    unittest.main()
