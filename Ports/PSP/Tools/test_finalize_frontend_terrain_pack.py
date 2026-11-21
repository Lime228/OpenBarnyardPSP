#!/usr/bin/env python3

import json
import pathlib
import struct
import tempfile
import unittest

from finalize_frontend_terrain_pack import finalize


class FinalizeFrontendTerrainPackTests(unittest.TestCase):
    def test_preserves_retail_addressing_and_render_order(self) -> None:
        manifest = {
            "version": 1,
            "models": [
                {"material": "normal", "texture_index": 0},
                {"material": "end", "texture_index": 1},
                {"material": "start", "texture_index": 0},
            ],
            "textures": [{}, {}],
        }
        properties = {
            "version": 1,
            "clamped": [{"material": "normal", "address_u": 1, "address_v": 0}],
            "world_start_render": ["start"],
            "world_end_render": ["end"],
        }
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            manifest_path = temporary / "manifest.json"
            properties_path = temporary / "properties.json"
            output = temporary / "pack.pcfg"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            properties_path.write_text(json.dumps(properties), encoding="utf-8")
            finalize(manifest_path, properties_path, output, ["mask", "blend"])
            values = struct.unpack("<21I", output.read_bytes())
            self.assertEqual((0x50444650, 1, 3, 2, 20, 0), values[:6])
            self.assertEqual((2, 0, 1, 0, 0), values[6:11])
            self.assertEqual((0, 0, 1, 1, 0), values[11:16])
            self.assertEqual((1, 1, 2, 0, 0), values[16:21])


if __name__ == "__main__":
    unittest.main()
