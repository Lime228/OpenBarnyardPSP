import unittest
from pathlib import Path
from unittest.mock import patch

from convert_retail_sequence_models import index_textures


class SequenceTextureTests(unittest.TestCase):
    def test_latest_declared_library_has_priority(self):
        libraries = [{"symbol": "base_TTL"}, {"symbol": None}, {"symbol": "scene_TTL"}]
        with patch("convert_retail_sequence_models.run_export") as export:
            export.side_effect = [
                {"textures": [{"name": "Char\\Otis.tga"}, {"name": "Char\\Eyes.tga"}]},
                {"textures": [{"name": "char/otis.tga"}]},
            ]
            bindings = index_textures(Path("TMDL"), Path("scene.trb"), libraries, Path("output"))

        self.assertEqual(bindings["char/otis.tga"], ("scene_TTL", "char/otis.tga"))
        self.assertEqual(bindings["char/eyes.tga"], ("base_TTL", "Char\\Eyes.tga"))
        self.assertEqual(export.call_count, 2)
