#!/usr/bin/env python3

import json
import pathlib
import struct
import tempfile
import unittest

from convert_terrain_dump import convert


class TerrainDumpConversionTests(unittest.TestCase):
    def test_converts_textured_strip_and_vertex_colours(self) -> None:
        document = {
            "version": 1,
            "material": "ground",
            "vertices": [
                {"position": [0, 0, 0], "uv": [0, 0], "colour": [1, 0, 0]},
                {"position": [1, 0, 0], "uv": [1, 0], "colour": [0, 1, 0]},
                {"position": [0, 1, 0], "uv": [0, 1], "colour": [0, 0, 1]},
                {"position": [1, 1, 0], "uv": [1, 1], "colour": [1, 1, 1]},
            ],
            "triangle_strip": [0, 1, 2, 3],
        }
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            source = temporary / "terrain.json"
            destination = temporary / "terrain.pmes"
            source.write_text(json.dumps(document), encoding="utf-8")
            summary = convert(source, destination)
            payload = destination.read_bytes()
            self.assertEqual(summary, {"material": "ground", "vertices": 4, "indices": 6, "bytes": 132})
            self.assertEqual(struct.unpack_from("<6I", payload), (0x53454D50, 1, 4, 6, 24, 1))
            self.assertEqual(struct.unpack_from("<I", payload, 32)[0], 0xFF0000FF)

    def test_rejects_non_finite_positions(self) -> None:
        document = {
            "version": 1,
            "material": "ground",
            "vertices": [
                {"position": [0, 0, 0], "uv": [0, 0], "colour": [1, 1, 1]},
                {"position": [1, 0, 0], "uv": [1, 0], "colour": [1, 1, 1]},
                {"position": [float("nan"), 1, 0], "uv": [0, 1], "colour": [1, 1, 1]},
            ],
            "triangle_strip": [0, 1, 2],
        }
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            source = temporary / "terrain.json"
            source.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "non-finite"):
                convert(source, temporary / "terrain.pmes")


if __name__ == "__main__":
    unittest.main()
