import struct
import tempfile
import unittest
from pathlib import Path

from inventory_retail_sequence import describe_resources, inspect_script, retail_file


class RetailSequenceTests(unittest.TestCase):
    def test_inspects_the_original_tso_tables(self):
        header = struct.pack("<6I", 20, 4, 24, 32, 8, 40)
        script = struct.pack("<IiIII", 0, 0, 1, 0, 4)
        body = struct.pack("<I8sIIiI", 0, b"Intro", 12, 16, 7, 0)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "Intro.tso"
            path.write_bytes(header + script + body)
            self.assertEqual(inspect_script(path), {"name": "Intro", "tracks": 1, "track_actors": [7]})

            path.write_bytes(header + script + body[:-1])
            with self.assertRaisesRegex(ValueError, "track tables"):
                inspect_script(path)

    def test_uses_model_types_and_the_original_symbol_case(self):
        properties = [["keylib", ["shared"]], ["matlib", ["walls", "other"]]]
        model_types = [
            ["chief", {"properties": [["AnimatableObjectType", {"properties": [["Model", "Leader"]]}]]}],
            ["crowd", {"properties": []}],
        ]
        symbols = {"symbols": [{"name": name} for name in ("Shared_keylib", "Walls_TTL", "Leader_Header", "Leader_FileHeader", "Crowd_Header")]}
        result = describe_resources(properties, model_types, symbols)
        self.assertEqual(result["key_libraries"], [{"name": "shared", "symbol": "Shared_keylib"}])
        self.assertEqual(result["material_libraries"], [{"name": "walls", "symbol": "Walls_TTL"}, {"name": "other", "symbol": None}])
        self.assertEqual([model["model"] for model in result["model_types"]], ["Leader", "crowd"])
        self.assertEqual([model["packed_header"] for model in result["model_types"]], ["Leader_Header", "Crowd_Header"])

    def test_resolves_retail_path_separators_and_case(self):
        with tempfile.TemporaryDirectory() as directory:
            install = Path(directory)
            path = install / "Data" / "Script.TRB"
            path.parent.mkdir()
            path.touch()
            self.assertEqual(retail_file(install, "data\\script.trb"), path)
            with self.assertRaisesRegex(ValueError, "invalid retail file reference"):
                retail_file(install, "Data/../../Script.TRB")
