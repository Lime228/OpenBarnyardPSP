#!/usr/bin/env python3

import pathlib
import tempfile
import unittest

from audit_source_dependencies import audit


class SourceDependencyAuditTests(unittest.TestCase):
    def test_collects_transitive_project_includes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = pathlib.Path(temporary_name)
            (root / "entry.cpp").write_text('#include "local.h"\n#include <Engine/shared.h>\n', encoding="utf-8")
            (root / "local.h").write_text('#include "leaf.h"\n', encoding="utf-8")
            (root / "leaf.h").write_text("#pragma once\n", encoding="utf-8")
            (root / "Engine").mkdir()
            (root / "Engine/shared.h").write_text("#pragma once\n", encoding="utf-8")
            result = audit([root / "entry.cpp"], [root])
            self.assertEqual(len(result["files"]), 4)
            self.assertEqual(result["missing_project_includes"], [])
            self.assertEqual(result["external_includes"], [])

    def test_separates_missing_quoted_and_external_angle_includes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = pathlib.Path(temporary_name)
            entry = root / "entry.cpp"
            entry.write_text('#include "missing.h"\n#include <vector>\n', encoding="utf-8")
            result = audit([entry], [root])
            self.assertEqual(result["missing_project_includes"], ["missing.h"])
            self.assertEqual(result["external_includes"], ["vector"])


if __name__ == "__main__":
    unittest.main()
