import random
import struct
import tempfile
import unittest
import wave
from pathlib import Path
from unittest.mock import patch

import encode_retail_fsb as encoder
from generate_bootstrap_adpcm import encode_channel


@unittest.skipIf(encoder.audioop is None, "audioop is unavailable")
class RetailAudioEncodingTests(unittest.TestCase):
    def test_native_adpcm_matches_reference_for_full_and_partial_blocks(self):
        random_samples = random.Random(1103)
        samples = [random_samples.randint(-32768, 32767) for _ in range(1024)]
        for length in (1, 517, 1024):
            original = samples[:length]
            reference = original + [original[-1]] * (1024 - length)
            packed = struct.pack(f"<{length}h", *original)
            self.assertEqual(encoder.encode_channel_block(packed), encode_channel(reference))

    def test_multichannel_audio_survives_resampling_and_segmentation(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source.wav"
            destination = Path(directory) / "encoded"
            with wave.open(str(source), "wb") as wav:
                wav.setparams((4, 2, 22050, 1100, "NONE", ""))
                wav.writeframes(struct.pack("<4h", 400, 800, -1200, -1600) * 1100)
            track = {
                "index": 1, "channels": 4, "sample_rate": 22050, "frames": 1100,
                "loop_start": 3, "loop_end_inclusive": 1050, "mode": 1,
            }
            with patch.object(encoder, "SEGMENT_FRAMES", 2048):
                report = encoder.encode_track(source, destination, track)
            self.assertTrue(report["requires_chaining"])
            self.assertEqual(report["output_frames"], 2200)
            self.assertEqual(report["resampled_loop_start"], 6)
            self.assertEqual(report["resampled_loop_end"], 2102)
            self.assertEqual(len(report["outputs"]), 4)
            for index, item in enumerate(report["outputs"]):
                data = (destination / item["path"]).read_bytes()
                self.assertEqual(len(data), item["bytes"])
                if index < 2:
                    self.assertEqual(struct.unpack("<10I", data[:40]), (
                        encoder.PADP_MAGIC, 2, 44100, 2, 2048, 1024, 1032, 2, 0, 2048,
                    ))
                    offsets = (40, 40 + 516)
                else:
                    self.assertEqual(struct.unpack("<5I", data[:20]), (
                        encoder.PPCM_MAGIC, 1, 44100, 2, 152,
                    ))
                    offsets = (20, 22)
                expected = (400, 800) if index % 2 == 0 else (-1200, -1600)
                self.assertEqual(tuple(struct.unpack_from("<h", data, i)[0] for i in offsets), expected)


if __name__ == "__main__":
    unittest.main()
