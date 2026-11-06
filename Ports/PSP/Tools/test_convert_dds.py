#!/usr/bin/env python3

import pathlib
import struct
import tempfile
import unittest

from convert_dds import convert, decode_dxt_block


def dds_header(width: int, height: int, fourcc: bytes, data_size: int) -> bytes:
    payload = bytearray(128)
    payload[:4] = b"DDS "
    struct.pack_into("<I", payload, 4, 124)
    struct.pack_into("<II", payload, 12, height, width)
    struct.pack_into("<I", payload, 20, data_size)
    struct.pack_into("<I", payload, 76, 32)
    struct.pack_into("<I", payload, 80, 4)
    payload[84:88] = fourcc
    return bytes(payload)


class DDSConversionTests(unittest.TestCase):
    def test_decodes_dxt3_colour_and_alpha(self) -> None:
        alpha = sum((pixel & 0xF) << (pixel * 4) for pixel in range(16)).to_bytes(8, "little")
        colour = struct.pack("<HHI", 0xF800, 0x07E0, 0)
        pixels = decode_dxt_block(b"DXT3", alpha + colour)
        self.assertEqual(pixels[0], (255, 0, 0, 0))
        self.assertEqual(pixels[15], (255, 0, 0, 255))

    def test_converts_top_mip_to_linear_ptex(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            source = temporary / "source.dds"
            destination = temporary / "output.ptex"
            block = b"\xff" * 8 + struct.pack("<HHI", 0xF800, 0x07E0, 0)
            source.write_bytes(dds_header(4, 4, b"DXT3", len(block)) + block)
            convert(source, destination, linear=True)
            payload = destination.read_bytes()
            self.assertEqual(struct.unpack_from("<6I", payload), (0x58455450, 1, 4, 4, 4, 0))
            self.assertEqual(payload[24:28], bytes((255, 0, 0, 255)))

    def test_converts_a1r5g5b5_to_rgba(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            source = temporary / "source.dds"
            destination = temporary / "output.ptex"
            header = bytearray(dds_header(2, 1, b"\0\0\0\0", 4))
            struct.pack_into("<I", header, 80, 0x41)
            struct.pack_into("<5I", header, 88, 16, 0x7C00, 0x03E0, 0x001F, 0x8000)
            source.write_bytes(header + struct.pack("<HH", 0xFC00, 0x03E0))
            convert(source, destination, linear=True)
            payload = destination.read_bytes()
            self.assertEqual(payload[24:28], bytes((255, 0, 0, 255)))
            self.assertEqual(payload[28:32], bytes((0, 255, 0, 0)))

    def test_rejects_oversized_texture(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            source = pathlib.Path(temporary_name) / "large.dds"
            source.write_bytes(dds_header(1024, 4, b"DXT1", 0))
            with self.assertRaisesRegex(ValueError, "no larger than 512"):
                convert(source, source.with_suffix(".ptex"))


if __name__ == "__main__":
    unittest.main()
