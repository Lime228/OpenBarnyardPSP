#!/usr/bin/env python3

import json
import pathlib
import tempfile
import unittest

from merge_frontend_terrain_cells import MAX_SOURCE_CELLS, merge


def cell(materials: list[str], world: int = 0, cell_index: int = 0) -> dict[str, object]:
    return {
        "version": 2,
        "world": world,
        "cell": cell_index,
        "meshes": [{"material": material} for material in materials],
    }


class FrontendTerrainCellMergeTests(unittest.TestCase):
    def write_cell(self, directory: pathlib.Path, name: str, document: object) -> pathlib.Path:
        path = directory / name
        path.write_text(json.dumps(document), encoding="utf-8")
        return path

    def test_merges_complete_cells_in_source_order(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            first = self.write_cell(temporary, "first.json", cell(["a", "b"]))
            second = self.write_cell(temporary, "second.json", cell(["c"]))
            output = temporary / "merged.json"
            merged = merge([first, second], output)
            self.assertEqual((merged["world"], merged["cell"]), (0, 0))
            self.assertEqual([mesh["material"] for mesh in merged["meshes"]], ["a", "b", "c"])
            self.assertEqual(json.loads(output.read_text(encoding="utf-8")), merged)

    def test_rejects_mismatched_world_or_cell(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            first = self.write_cell(temporary, "first.json", cell(["a"]))
            second = self.write_cell(temporary, "second.json", cell(["b"], cell_index=1))
            with self.assertRaisesRegex(ValueError, "does not match world 0 cell 0"):
                merge([first, second], temporary / "merged.json")

    def test_rejects_unsupported_or_empty_cells(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            unsupported = self.write_cell(temporary, "unsupported.json", {"version": 1})
            empty = self.write_cell(temporary, "empty.json", cell([]))
            with self.assertRaisesRegex(ValueError, "unsupported contract version"):
                merge([unsupported], temporary / "unsupported-merged.json")
            with self.assertRaisesRegex(ValueError, "requires meshes"):
                merge([empty], temporary / "empty-merged.json")

    def test_rejects_source_count_and_mesh_budget_overflow(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            source = self.write_cell(temporary, "source.json", cell(["a"]))
            with self.assertRaisesRegex(ValueError, f"between 1 and {MAX_SOURCE_CELLS}"):
                merge([source] * (MAX_SOURCE_CELLS + 1), temporary / "too-many-cells.json")
            full = self.write_cell(temporary, "full.json", cell(["a"] * 128))
            extra = self.write_cell(temporary, "extra.json", cell(["b"]))
            with self.assertRaisesRegex(ValueError, "exceeds 128 source meshes"):
                merge([full, extra], temporary / "too-many-meshes.json")


if __name__ == "__main__":
    unittest.main()
