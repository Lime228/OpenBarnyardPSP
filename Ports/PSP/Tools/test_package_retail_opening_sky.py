import struct
import tempfile
import unittest
from pathlib import Path

from package_retail_opening_sky import bake_dome, read_time_fx


class OpeningSkyTests(unittest.TestCase):
    def test_follows_weather_relocations_across_trb_sections(self):
        first = bytearray(32)
        first[16:24] = b"morning\0"
        second = bytearray(8 + 0x178)
        struct.pack_into("<I", second, 4, 8)
        struct.pack_into("<I", second, 8 + 0x114, 16)
        struct.pack_into("<i", second, 8 + 0x124, 1)
        struct.pack_into("<f", second, 8 + 0x144, 0.05)
        struct.pack_into("<4f", second, 8 + 0x90, 90, 137, 230, 0)

        def chunk(tag: bytes, payload: bytes) -> bytes:
            return struct.pack("<4sI", tag, len(payload)) + payload

        header = struct.pack("<HHI8I", 1, 1, 2, 0, len(first), 0, 0, 0, len(second), 0, 0)
        relocations = struct.pack("<IHHIHHIHHI", 3, 0, 1, 0, 1, 1, 4, 1, 0, 8 + 0x114)
        form = b"TRBF" + chunk(b"HDRX", header) + chunk(b"SECT", first + second) + chunk(b"RELC", relocations)
        symbols = {"symbols": [{"name": "weathersettings", "stream": 0, "offset": 0}]}
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "GameTimeFXSettings.trb"
            source.write_bytes(struct.pack("<4sI", b"TSFL", len(form)) + form)
            profile = read_time_fx(source, symbols)
        self.assertEqual(profile["name"], "morning")
        self.assertEqual(profile["texture_index"], 1)
        self.assertEqual(profile["colour_matrix"][0][:3], (90, 137, 230))
        self.assertAlmostEqual(profile["rotation_speed"], 0.05)

    def test_dome_keeps_original_horizon_and_zenith_on_both_halves(self):
        zenith, horizon = (90, 137, 230, 0), (190, 236, 251, 0)
        dome = bake_dome([zenith, zenith, horizon, horizon])
        vertices = dome["vertices"]
        self.assertEqual(len(vertices), 180)
        self.assertEqual(len(dome["indices"]), 840)
        self.assertEqual(vertices[0], ((-0.00390625, 0.994140625), (0, 180, 0), 0xfffbecbe))
        self.assertEqual(vertices[14][0][0], 0.99609375)
        self.assertEqual(vertices[75][1][2], 75)
        self.assertEqual(vertices[165][1][2], -75)
        self.assertEqual(vertices[75][2], 0xffe6895a)
        self.assertEqual(vertices[165][2], vertices[75][2])


if __name__ == "__main__":
    unittest.main()
