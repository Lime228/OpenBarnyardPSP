#!/usr/bin/env python3

import json
import pathlib
import struct
import tempfile
import unittest

from finalize_terrain_config import finalize


class FinalizeTerrainConfigTests(unittest.TestCase):
    def test_writes_exact_per_mesh_alpha_modes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            manifest = temporary / "terrain.json"
            destination = temporary / "terrain.pcfg"
            manifest.write_text(json.dumps({"meshes": [{}, {}, {}]}), encoding="utf-8")
            summary = finalize(manifest, destination, ["mask", "blend", "additive"])
            self.assertEqual(
                (0x52544650, 2, 3, 4, 1, 2, 3),
                struct.unpack("<7I", destination.read_bytes()),
            )
            self.assertEqual(28, summary["bytes"])

    def test_rejects_mode_count_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            manifest = temporary / "terrain.json"
            manifest.write_text(json.dumps({"meshes": [{}, {}]}), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "does not match"):
                finalize(manifest, temporary / "terrain.pcfg", ["mask"])


if __name__ == "__main__":
    unittest.main()
