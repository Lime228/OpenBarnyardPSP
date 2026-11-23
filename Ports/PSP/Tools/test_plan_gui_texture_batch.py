import unittest

from plan_gui_texture_batch import build_plan


class GUITextureBatchPlanTests(unittest.TestCase):
    def test_resolves_complete_case_insensitive_section_set(self):
        sections = {
            "section_count": 3,
            "sections": [
                {"index": 0, "name": "left_arrow", "texture": "HUD\\Arrows.tga"},
                {"index": 1, "name": "right_arrow", "texture": "HUD\\Arrows.tga"},
                {"index": 2, "name": "star", "texture": "HUD\\Stars.tga"},
            ],
        }
        libraries = {
            "libraries": [
                {
                    "path": "Data\\Terrain\\room\\room.ttl",
                    "symbol": "TTL",
                    "status": "inventoried",
                    "textures": [{"name": "hud\\arrows.tga"}],
                },
                {
                    "path": "Data\\Matlibs\\gui_HUD.ttl",
                    "symbol": "TTL",
                    "status": "inventoried",
                    "textures": [
                        {"name": "HUD\\ARROWS.TGA"},
                        {"name": "HUD\\Stars.tga"},
                    ],
                },
            ]
        }

        plan = build_plan(sections, libraries)

        self.assertEqual(plan["section_count"], 3)
        self.assertEqual(plan["texture_count"], 2)
        self.assertEqual(plan["resolved_texture_count"], 2)
        self.assertEqual(plan["unresolved_texture_count"], 0)
        arrows = [item for item in plan["items"] if item["texture"] == "HUD\\Arrows.tga"]
        self.assertEqual(len(arrows), 2)
        self.assertTrue(all(item["section_count"] == 2 for item in arrows))
        self.assertEqual(arrows[0]["source_texture"], "HUD\\ARROWS.TGA")

    def test_preserves_embedded_owner_and_full_texture_path(self):
        sections = {
            "section_count": 3,
            "sections": [
                {"index": 0, "name": "one", "texture": "A\\shared.tga"},
                {"index": 1, "name": "two", "texture": "B\\shared.tga"},
                {"index": 2, "name": "missing", "texture": "C\\missing.tga"},
            ],
        }
        libraries = {
            "libraries": [
                {
                    "path": "Data\\Assets\\lib_startup.trb",
                    "symbol": "gui_hud_TTL",
                    "status": "inventoried",
                    "textures": [{"name": "A\\shared.tga"}, {"name": "B\\shared.tga"}],
                }
            ]
        }

        plan = build_plan(sections, libraries)

        self.assertEqual(plan["resolved_texture_count"], 2)
        self.assertEqual(plan["unresolved_texture_count"], 1)
        self.assertEqual(
            [item["output"] for item in plan["items"] if item["status"] == "resolved"],
            [
                "Data/Assets/lib_startup.trb/gui_hud_TTL/A/shared.tga.ptex",
                "Data/Assets/lib_startup.trb/gui_hud_TTL/B/shared.tga.ptex",
            ],
        )

    def test_rejects_incomplete_section_inventory(self):
        with self.assertRaisesRegex(ValueError, "count is inconsistent"):
            build_plan({"section_count": 2, "sections": []}, {"libraries": []})


if __name__ == "__main__":
    unittest.main()
