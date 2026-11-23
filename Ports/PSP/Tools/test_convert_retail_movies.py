import struct
import tempfile
import unittest
from fractions import Fraction
from pathlib import Path

import convert_retail_movies as movies


def jpeg_section(marker, payload):
    return bytes((255, marker)) + struct.pack(">H", len(payload) + 2) + payload


def jpeg_fixture(width=movies.WIDTH, luma_sampling=0x22):
    return (
        b"\xff\xd8"
        + jpeg_section(0xDB, b"q")
        + jpeg_section(0xC4, b"h")
        + jpeg_section(
            0xC0,
            struct.pack(">BHH", 8, movies.HEIGHT, width)
            + bytes((3, 1, luma_sampling, 0, 2, 0x11, 0, 3, 0x11, 0)),
        )
        + jpeg_section(0xDA, b"test")
        + b"\xff\xd9"
    )


class RetailMovieTests(unittest.TestCase):
    def test_rejects_unsupported_frame_profiles(self):
        movies.validate_jpeg(jpeg_fixture())
        with self.assertRaisesRegex(ValueError, "dimensions"):
            movies.validate_jpeg(jpeg_fixture(width=481))
        with self.assertRaisesRegex(ValueError, "4:2:0"):
            movies.validate_jpeg(jpeg_fixture(luma_sampling=0x12))
        with self.assertRaisesRegex(ValueError, "markers"):
            movies.validate_jpeg(jpeg_fixture()[:-1])

    def test_rejects_packet_index_gaps(self):
        with self.assertRaisesRegex(ValueError, "discontinuous"):
            movies.index_packets({"packets": [{"pos": 1, "size": 2}]}, 3)

    def test_preserves_source_rate_and_only_caps_faster_movies(self):
        self.assertEqual(movies.psp_frame_rate("5000000/166833", 30), Fraction(5000000, 166833))
        self.assertEqual(movies.psp_frame_rate("60000/1001", 30), 30)
        self.assertEqual(movies.psp_frame_rate("24000/1001", 30), Fraction(24000, 1001))
        self.assertEqual(movies.psp_frame_rate("2997/100", 15), 15)
        with self.assertRaises(ValueError):
            movies.psp_frame_rate("0/0", 30)

    def test_packs_two_tracks_and_validates_the_complete_extent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            jpeg = root / "video.mjpeg"
            first, second = root / "one.pcm", root / "two.pcm"
            destination = root / "sample.pmvf"
            frame = jpeg_fixture()
            jpeg.write_bytes(frame * 3)
            first.write_bytes(b"\x01\x02\x03\x04")
            second.write_bytes(b"\x06\x07\x08\x09" * 6000)
            result = movies.pack_movie(
                jpeg, [(index * len(frame), len(frame)) for index in range(3)], [first, second], destination,
                Fraction(2997, 100),
            )
            movies.verify_movie(destination)
            samples = 1471 + 1471 + 1472
            self.assertEqual(result["frame_rate"], "2997/100")
            self.assertEqual(result["audio"][0]["padded_frames"], samples - 1)
            self.assertEqual(result["audio"][1]["trimmed_frames"], 6000 - samples)
            payload = destination.read_bytes()
            audio_offset = movies.HEADER.size + 4 + len(frame)
            self.assertEqual(payload[audio_offset : audio_offset + 8], b"\x01\x02\x03\x04\0\0\0\0")
            next_track = audio_offset + 1471 * 4
            self.assertEqual(payload[next_track : next_track + 4], b"\x06\x07\x08\x09")
            self.assertEqual(len(payload), movies.HEADER.size + 3 * (4 + len(frame)) + samples * 8)
            destination.write_bytes(payload[:-1])
            with self.assertRaisesRegex(ValueError, "truncated PMVF frame"):
                movies.verify_movie(destination)

    def test_verifies_legacy_15_fps_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "legacy.pmvf"
            frame = jpeg_fixture()
            pcm = bytes(2940 * 4)
            path.write_bytes(movies.HEADER_V1.pack(
                movies.MAGIC, 1, movies.HEADER_V1.size, movies.WIDTH, movies.HEIGHT, 15, 1,
                len(frame), 1, movies.SAMPLE_RATE, len(pcm),
            ) + movies.FRAME_SIZE.pack(len(frame)) + frame + pcm)
            movies.verify_movie(path)


if __name__ == "__main__":
    unittest.main()
