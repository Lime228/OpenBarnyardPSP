import struct
import tempfile
import unittest
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

    def test_packs_two_tracks_and_validates_the_complete_extent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            jpeg = root / "video.mjpeg"
            first, second = root / "one.pcm", root / "two.pcm"
            destination = root / "sample.pmvf"
            frame = jpeg_fixture()
            jpeg.write_bytes(frame * 2)
            first.write_bytes(b"\x01\x02\x03\x04")
            second.write_bytes(b"\x06\x07\x08\x09" * 6000)
            result = movies.pack_movie(
                jpeg, [(0, len(frame)), (len(frame), len(frame))], [first, second], destination,
            )
            movies.verify_movie(destination)
            self.assertEqual(result["audio"][0]["padded_frames"], 2 * 2940 - 1)
            self.assertEqual(result["audio"][1]["trimmed_frames"], 6000 - 2 * 2940)
            payload = destination.read_bytes()
            audio_offset = movies.HEADER.size + 4 + len(frame)
            self.assertEqual(payload[audio_offset : audio_offset + 8], b"\x01\x02\x03\x04\0\0\0\0")
            next_track = audio_offset + movies.PCM_PER_FRAME
            self.assertEqual(payload[next_track : next_track + 4], b"\x06\x07\x08\x09")
            destination.write_bytes(payload[:-1])
            with self.assertRaisesRegex(ValueError, "truncated PMVF frame"):
                movies.verify_movie(destination)


if __name__ == "__main__":
    unittest.main()
