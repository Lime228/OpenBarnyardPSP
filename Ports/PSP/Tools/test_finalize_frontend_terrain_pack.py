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

    def test_forces_original_lod_glow_mode_for_every_model(self) -> None:
        manifest = {
            "version": 1,
            "models": [
                {"material": "lights_barn", "texture_index": 0},
                {"material": "lightcone_outer", "texture_index": 0},
                {"material": "lightcone_inner", "texture_index": 0},
            ],
            "textures": [{}],
        }
        properties = {
            "version": 1,
            "clamped": [],
            "world_start_render": [],
            "world_end_render": [],
        }
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            manifest_path = temporary / "manifest.json"
            properties_path = temporary / "properties.json"
            output = temporary / "pack.pcfg"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            properties_path.write_text(json.dumps(properties), encoding="utf-8")
            finalize(
                manifest_path,
                properties_path,
                output,
                ["mask"],
                force_alpha_mode="additive",
                light_profiles={
                    "lights_barn": "barn",
                    "lightcone_outer": "cone",
                    "lightcone_inner": "cone",
                },
            )
            values = struct.unpack("<24I", output.read_bytes())
            self.assertEqual((0x50444650, 2, 3, 1, 24, 0), values[:6])
            self.assertEqual((0, 0, 3, 0, 0, 1), values[6:12])
            self.assertEqual((1, 0, 3, 0, 0, 2), values[12:18])
            self.assertEqual((2, 0, 3, 0, 0, 2), values[18:24])

    def test_rejects_unknown_original_light_profile(self) -> None:
        manifest = {
            "version": 1,
            "models": [{"material": "lights_barn", "texture_index": 0}],
            "textures": [{}],
        }
        properties = {"version": 1, "clamped": [], "world_start_render": [], "world_end_render": []}
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            manifest_path = temporary / "manifest.json"
            properties_path = temporary / "properties.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            properties_path.write_text(json.dumps(properties), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "invalid terrain pack light profile"):
                finalize(
                    manifest_path,
                    properties_path,
                    temporary / "pack.pcfg",
                    ["mask"],
                    light_profiles={"lights_barn": "invented"},
                )

    def test_preserves_barnnorth_light_profiles(self) -> None:
        manifest = {
            "version": 1,
            "models": [
                {"material": "lights_farmhouse", "texture_index": 0},
                {"material": "nightlight", "texture_index": 0},
            ],
            "textures": [{}],
        }
        properties = {
            "version": 1,
            "clamped": [],
            "world_start_render": [],
            "world_end_render": [],
        }
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            manifest_path = temporary / "manifest.json"
            properties_path = temporary / "properties.json"
            output = temporary / "pack.pcfg"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            properties_path.write_text(json.dumps(properties), encoding="utf-8")
            finalize(
                manifest_path,
                properties_path,
                output,
                ["mask"],
                force_alpha_mode="additive",
                light_profiles={
                    "lights_farmhouse": "farmhouse",
                    "nightlight": "nightlight",
                },
            )
            values = struct.unpack("<18I", output.read_bytes())
            self.assertEqual((0x50444650, 2, 2, 1, 24, 0), values[:6])
            self.assertEqual((0, 0, 3, 0, 0, 3), values[6:12])
            self.assertEqual((1, 0, 3, 0, 0, 7), values[12:18])


if __name__ == "__main__":
    unittest.main()
