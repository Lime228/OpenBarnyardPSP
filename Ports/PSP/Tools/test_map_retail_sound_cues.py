import tempfile
import unittest
from pathlib import Path

from map_retail_sound_cues import (
    index_wave_banks,
    map_multiplex_streams,
    map_sound_banks,
    verify_linked_files,
)


def properties(*pairs):
    return {"properties": list(pairs)}


def document(root, symbol="Main"):
    return {"symbols": [{"symbol": symbol, "root": root}]}


class RetailSoundCueMappingTests(unittest.TestCase):
    def setUp(self):
        self.fsb = "Data/Sound/FSB/Sample.fsb"
        self.tracks = {
            self.fsb.casefold(): {
                "path": self.fsb,
                "tracks": {
                    1: {
                        "index": 1, "name": "first", "source_channels": 2,
                        "requires_chaining": False, "outputs": [{"path": "one.padp", "bytes": 5}],
                    },
                    2: {
                        "index": 2, "name": "second", "source_channels": 2,
                        "requires_chaining": False, "outputs": [{"path": "two.padp", "bytes": 6}],
                    },
                },
            },
        }
        self.wave_banks = index_wave_banks(document(properties(("Bank", properties(
            ("name", "sample_bank"), ("path", "Sample"), ("type", "File"),
            ("Wave", properties()), ("Wave", properties()),
        ))), "bank_sample_Main"))

    def test_preserves_repeated_sound_rows_and_maps_wave_indices(self):
        first = properties(
            ("id", 10), ("name", "example"), ("banks", ["Sample_Bank", "sample_bank"]),
            ("waves", [1, 0]), ("tracks", [2]),
        )
        second = properties(
            ("id", 11), ("name", "second"), ("banks", ["sample_bank"]), ("waves", [2]),
        )
        doc = document(properties(("Sounds", properties(
            ("name", "example"), ("numsounds", 2), ("Sound", first), ("Sound", second),
        ))))
        [bank] = map_sound_banks(Path("soundbank_example.trb"), doc, self.wave_banks, self.tracks)
        self.assertEqual([cue["id"] for cue in bank["cues"]], [10, 11])
        first, second = bank["cues"]
        self.assertEqual([sample["track_index"] for sample in first["samples"]], [2, 1])
        self.assertEqual(first["settings"], [["tracks", [2]]])
        self.assertEqual(second["samples"][0]["status"], "invalid-wave-index")

    def test_resolves_multiplex_channels_and_checks_cached_file_sizes(self):
        doc = document(properties(("MultiplexStream", properties(
            ("Name", "ambient"), ("FSB", self.fsb.replace("/", "\\")),
            ("ChannelCategories", ["music", "effects"]), ("Loop", True),
        ))))
        [stream] = map_multiplex_streams(doc, self.tracks)
        self.assertEqual(stream["status"], "mapped")
        self.assertEqual(stream["settings"], [["Loop", True]])
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            bank = root / self.fsb
            bank.mkdir(parents=True)
            (bank / "one.padp").write_bytes(b"12345")
            (bank / "two.padp").write_bytes(b"short")
            verified = verify_linked_files(root, self.tracks, stream["tracks"])
            self.assertEqual(verified["verified"], 1)
            self.assertEqual(verified["failures"][0]["path"], f"{self.fsb}/two.padp")


if __name__ == "__main__":
    unittest.main()
