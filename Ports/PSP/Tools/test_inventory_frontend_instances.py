#!/usr/bin/env python3

import json
import pathlib
import tempfile
import unittest

from inventory_frontend_instances import inventory


MODEL_DEFINITIONS = """
inline static ModelDefinition ms_aModelDefinitions[] = {
    { 0, "fncb01", "data/models/fncb01.trb" },
    { 0, "rock01", "data/models/rock01.trb", },
    { 0, "unused", "data/models/unused.trb" },
};
"""

GAME_LOADER = """
static TUINT s_aFrontEndInstanceModels[] = {
    0x0, 0x1
};
"""


def locator(index: int, name: str) -> dict[str, object]:
    return {
        "index": index,
        "name": name,
        "rotation": [0.0, 0.0, 0.0, 1.0],
        "position": [float(index), 0.0, 0.0],
        "scale": 1.0,
        "scale_index": 5,
        "yaw_index": 0,
        "raw_flags1": 5,
        "flags2": 0,
    }


class FrontendInstanceInventoryTests(unittest.TestCase):
    def create_fixture(self, root: pathlib.Path) -> tuple[pathlib.Path, pathlib.Path, pathlib.Path, pathlib.Path]:
        definitions = root / "AInstanceManagerModelList.inl"
        definitions.write_text(MODEL_DEFINITIONS, encoding="utf-8")
        loader = root / "AGameLoader.cpp"
        loader.write_text(GAME_LOADER, encoding="utf-8")
        install = root / "Install"
        for name in ("fncb01", "rock01", "unused"):
            path = install / "data" / "models" / f"{name}.trb"
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(name.encode("ascii"))
        dump = root / "locators.json"
        dump.write_text(
            json.dumps(
                {
                    "version": 1,
                    "locator_count": 4,
                    "locators": [
                        locator(0, "fncb01_001"),
                        locator(1, "rock01"),
                        locator(2, "unused_02"),
                        locator(3, "loc_camera"),
                    ],
                    "vis_section_count": 2,
                    "vis_sections": [
                        {"index": 0, "name": "Barn", "first_locator": 0, "locator_count": 2},
                        {"index": 1, "name": "undefined", "first_locator": 2, "locator_count": 2},
                    ],
                }
            ),
            encoding="utf-8",
        )
        return dump, definitions, loader, install

    def test_matches_original_prefix_rule_and_frontend_model_list(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = pathlib.Path(temporary_name)
            dump, definitions, loader, install = self.create_fixture(root)
            result = inventory(dump, definitions, loader, install, root / "inventory.json", ["Barn"])
            self.assertEqual(result["frontend_model_count"], 2)
            self.assertEqual(result["used_model_count"], 2)
            self.assertEqual(result["instance_count"], 2)
            self.assertEqual(result["skipped_unloaded_locator_count"], 1)
            self.assertEqual(result["unmatched_locator_count"], 1)
            self.assertEqual(result["resident_used_model_count"], 2)
            self.assertEqual(result["resident_instance_count"], 2)
            self.assertEqual([entry["model_name"] for entry in result["instances"]], ["fncb01", "rock01"])
            self.assertEqual(result["sections"][0]["instance_count"], 2)

    def test_rejects_overlapping_vis_ranges(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = pathlib.Path(temporary_name)
            dump, definitions, loader, install = self.create_fixture(root)
            document = json.loads(dump.read_text(encoding="utf-8"))
            document["vis_sections"][1]["first_locator"] = 1
            dump.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "overlaps locator 1"):
                inventory(dump, definitions, loader, install, root / "inventory.json")

    def test_main_game_uses_visible_sections_and_keeps_regrowth_separate(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = pathlib.Path(temporary_name)
            dump, definitions, loader, install = self.create_fixture(root)
            loader.write_text("static TUINT s_aInstanceModels0[] = {0x0, 0x1, 0x2};", encoding="utf-8")
            (root / "AInstanceManager.cpp").write_text('static const TCHAR* s_apszRegrowthNames[] = {"rock01"};',
                                                      encoding="utf-8")
            vis = root / "vis.json"
            vis.write_text(json.dumps({"sections": [
                {"name": "barn", "visibility": ["high", "low"]}, {"name": "NoLocators"},
            ]}), encoding="utf-8")
            result = inventory(dump, definitions, loader, install, root / "inventory.json", main_game_vis=vis)
            self.assertEqual(result["main_game_model_count"], 3)
            self.assertEqual(result["resident_section_names"], ["Barn", "undefined"])
            self.assertEqual(result["skipped_regrowth_locator_count"], 1)
            self.assertEqual([entry["model_name"] for entry in result["instances"]], ["fncb01", "unused"])

    def test_rejects_missing_retail_model(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = pathlib.Path(temporary_name)
            dump, definitions, loader, install = self.create_fixture(root)
            (install / "data" / "models" / "rock01.trb").unlink()
            with self.assertRaisesRegex(ValueError, "rock01"):
                inventory(dump, definitions, loader, install, root / "inventory.json")

    def test_rejects_unknown_resident_section(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = pathlib.Path(temporary_name)
            dump, definitions, loader, install = self.create_fixture(root)
            with self.assertRaisesRegex(ValueError, "resident VIS sections are missing"):
                inventory(dump, definitions, loader, install, root / "inventory.json", ["Missing"])


if __name__ == "__main__":
    unittest.main()
