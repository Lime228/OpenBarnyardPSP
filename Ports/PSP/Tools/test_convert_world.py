#!/usr/bin/env python3

import json
import math
import pathlib
import struct
import tempfile
import unittest

from convert_world import (
    MAX_INSTANCE_COUNT,
    MAX_CELL_COUNT,
    MAX_MODEL_RESOURCES,
    PWLD_CELL_SIZE,
    PWLD_HEADER_SIZE,
    PWLD_INSTANCE_SIZE,
    PWLD_MAGIC,
    PWLD_RESOURCE_SIZE,
    PWLD_VERSION,
    convert,
)


WORLD = {
    "version": 3,
    "cell_size": 16.0,
    "resources": [
        {"model": "probe.model", "texture": "probe.texture"},
    ],
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
        self.assertEqual(
            summary,
            {"instances": 2, "bytes": 168, "model_slots": 1, "resources": 1, "cells": 1},
        )
        self.assertEqual(
            len(payload),
            PWLD_HEADER_SIZE + 2 * PWLD_INSTANCE_SIZE + PWLD_RESOURCE_SIZE + PWLD_CELL_SIZE,
        )
        self.assertEqual(
            struct.unpack_from("<8I", payload),
            (PWLD_MAGIC, PWLD_VERSION, 2, PWLD_INSTANCE_SIZE, 1, PWLD_RESOURCE_SIZE, 1, PWLD_CELL_SIZE),
        )
        first = struct.unpack_from("<7fI", payload, PWLD_HEADER_SIZE)
        self.assertEqual(first[:3], (-1.0, 0.5, -4.0))
        self.assertEqual(first[3:6], (0.0, 0.25, 0.0))
        self.assertEqual(first[6:], (0.5, 0))
        resource = struct.unpack_from(
            "<24s24s",
            payload,
            PWLD_HEADER_SIZE + 2 * PWLD_INSTANCE_SIZE,
        )
        self.assertEqual(resource[0].rstrip(b"\0"), b"probe.model")
        self.assertEqual(resource[1].rstrip(b"\0"), b"probe.texture")
        cell = struct.unpack_from(
            "<4f2I",
            payload,
            PWLD_HEADER_SIZE + 2 * PWLD_INSTANCE_SIZE + PWLD_RESOURCE_SIZE,
        )
        self.assertEqual(cell[4:], (0, 2))
        self.assertGreater(cell[3], 0.0)

    def test_rejects_unbounded_instance_count(self) -> None:
        document = {
            "version": PWLD_VERSION,
            "resources": WORLD["resources"],
            "instances": [WORLD["instances"][0]] * (MAX_INSTANCE_COUNT + 1),
        }
        with self.assertRaisesRegex(ValueError, "between 1 and 16"):
            self.convert_document(document)

    def test_accepts_bounded_model_slots(self) -> None:
        document = json.loads(json.dumps(WORLD))
        document["resources"] = [
            {"model": f"model.{index}", "texture": f"texture.{index}"}
            for index in range(MAX_MODEL_RESOURCES)
        ]
        document["instances"][1]["model"] = MAX_MODEL_RESOURCES - 1
        summary, payload = self.convert_document(document)
        self.assertEqual(summary["model_slots"], 2)
        self.assertEqual(summary["resources"], MAX_MODEL_RESOURCES)
        second = struct.unpack_from("<7fI", payload, PWLD_HEADER_SIZE + PWLD_INSTANCE_SIZE)
        self.assertEqual(second[7], MAX_MODEL_RESOURCES - 1)

    def test_rejects_non_finite_transform(self) -> None:
        document = json.loads(json.dumps(WORLD))
        document["instances"][0]["rotation"][1] = math.inf
        with self.assertRaisesRegex(ValueError, "non-finite"):
            self.convert_document(document)

    def test_rejects_position_scale_and_model_limits(self) -> None:
        for field, value, message in (
            ("position", [1001.0, 0.0, 0.0], "position exceeds"),
            ("scale", 0.0, "scale exceeds"),
            ("model", MAX_MODEL_RESOURCES, "model slot exceeds"),
        ):
            document = json.loads(json.dumps(WORLD))
            document["instances"][0][field] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, message):
                self.convert_document(document)

    def test_rejects_invalid_resource_names(self) -> None:
        for field, value, message in (
            ("model", "invalid/name", "valid PSP resource name"),
            ("texture", "текстура", "must be ASCII"),
            ("model", "x" * 24, "valid PSP resource name"),
        ):
            document = json.loads(json.dumps(WORLD))
            document["resources"][0][field] = value
            with self.subTest(field=field, value=value), self.assertRaisesRegex(ValueError, message):
                self.convert_document(document)

    def test_partitions_and_rejects_unbounded_cells(self) -> None:
        document = json.loads(json.dumps(WORLD))
        document["cell_size"] = 1.0
        document["instances"] = [
            {
                "position": [float(index * 2), 0.0, -4.0],
                "rotation": [0.0, 0.0, 0.0],
                "scale": 0.5,
                "model": 0,
            }
            for index in range(MAX_CELL_COUNT + 1)
        ]
        with self.assertRaisesRegex(ValueError, "cell limit"):
            self.convert_document(document)


if __name__ == "__main__":
    unittest.main()
