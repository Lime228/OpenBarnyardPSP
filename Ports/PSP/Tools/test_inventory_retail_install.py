import tempfile
import unittest
from pathlib import Path

from inventory_retail_install import build_inventory, classify_area, classify_format


class RetailInstallInventoryTests(unittest.TestCase):
    def test_classifies_conversion_formats_and_areas(self):
        self.assertEqual(classify_format(Path("actor.TRB")), "resource-container")
        self.assertEqual(classify_format(Path("voice.FSB")), "audio-bank")
        self.assertEqual(classify_format(Path("readme.htm")), "support-file")
        self.assertEqual(
            classify_area(Path("Data") / "Terrain" / "frontend" / "Barn_L0Mod0.trb"),
            "Data/Terrain",
        )
        self.assertEqual(classify_area(Path("Manual") / "index.htm"), "Manual")

    def test_inventory_is_relative_complete_and_deterministic(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory) / "Install"
            terrain = root / "Data" / "Terrain" / "frontend"
            sound = root / "Data" / "Sound"
            terrain.mkdir(parents=True)
            sound.mkdir(parents=True)
            (terrain / "z.ttl").write_bytes(b"texture")
            (terrain / "A.trb").write_bytes(b"model")
            (sound / "ui.fsb").write_bytes(b"audio")

            inventory = build_inventory(root)

            self.assertEqual(inventory["file_count"], 3)
            self.assertEqual(inventory["bytes"], 17)
            self.assertEqual(
                [item["path"] for item in inventory["files"]],
                [
                    "Data/Sound/ui.fsb",
                    "Data/Terrain/frontend/A.trb",
                    "Data/Terrain/frontend/z.ttl",
                ],
            )
            self.assertNotIn(str(root), str(inventory))
            self.assertEqual(
                {item["name"]: item["file_count"] for item in inventory["formats"]},
                {"audio-bank": 1, "resource-container": 1, "texture-library": 1},
            )


if __name__ == "__main__":
    unittest.main()
