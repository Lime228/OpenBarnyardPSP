#!/usr/bin/env python3

import json
import math
import pathlib
import struct
import tempfile
import unittest

from convert_world import (
    MAX_INSTANCE_COUNT,
    PWLD_HEADER_SIZE,
    PWLD_INSTANCE_SIZE,
    PWLD_MAGIC,
    PWLD_VERSION,
    convert,
)


WORLD = {
    "version": 1,
    "instances": [
        {
            "position": [-1.0, 0.5, -4.0],
            "rotation": [0.0, 0.25, 0.0],
            "scale": 0.5,
            "model": 0,
        },
        {
            "position": [2.0, 0.0, -8.0],
            "rotation": [0.0, -0.5, 0.0],
            "scale": 1.25,
            "model": 0,
        },
    ],
}


class WorldConverterTests(unittest.TestCase):
    def convert_document(self, document: object) -> tuple[dict[str, object], bytes]:
        with tempfile.TemporaryDirectory() as temporary_name:
            temporary = pathlib.Path(temporary_name)
            source = temporary / "world.json"
            destination = temporary / "world.pwld"
            source.write_text(json.dumps(document), encoding="utf-8")
            summary = convert(source, destination)
            return summary, destination.read_bytes()

    def test_writes_fixed_world_layout(self) -> None:
        summary, payload = self.convert_document(WORLD)
        self.assertEqual(summary, {"instances": 2, "bytes": 80, "model_slots": 1})
        self.assertEqual(len(payload), PWLD_HEADER_SIZE + 2 * PWLD_INSTANCE_SIZE)
        self.assertEqual(
            struct.unpack_from("<4I", payload),
            (PWLD_MAGIC, PWLD_VERSION, 2, PWLD_INSTANCE_SIZE),
        )
        first = struct.unpack_from("<7fI", payload, PWLD_HEADER_SIZE)
        self.assertEqual(first[:3], (-1.0, 0.5, -4.0))
        self.assertEqual(first[3:6], (0.0, 0.25, 0.0))
        self.assertEqual(first[6:], (0.5, 0))

    def test_rejects_unbounded_instance_count(self) -> None:
        document = {
            "version": 1,
            "instances": [WORLD["instances"][0]] * (MAX_INSTANCE_COUNT + 1),
        }
        with self.assertRaisesRegex(ValueError, "between 1 and 16"):
            self.convert_document(document)

    def test_rejects_non_finite_transform(self) -> None:
        document = json.loads(json.dumps(WORLD))
        document["instances"][0]["rotation"][1] = math.inf
        with self.assertRaisesRegex(ValueError, "non-finite"):
            self.convert_document(document)

    def test_rejects_position_scale_and_model_limits(self) -> None:
        for field, value, message in (
            ("position", [1001.0, 0.0, 0.0], "position exceeds"),
            ("scale", 0.0, "scale exceeds"),
            ("model", 1, "model slot zero"),
        ):
            document = json.loads(json.dumps(WORLD))
            document["instances"][0][field] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, message):
                self.convert_document(document)


if __name__ == "__main__":
    unittest.main()
