#!/usr/bin/env python3

import json
import math
import pathlib
import struct
import tempfile

from convert_camera import PCAM_FLAG_LOOP, PCAM_FRAME_SIZE, PCAM_MAGIC, PCAM_VERSION, convert


def main() -> None:
    document = {
        "sequence": "camera_loop",
        "sample_rate": 30,
        "loop": True,
        "bones": [
            {"name": "camlocator", "parent": None},
            {"name": "camera", "parent": 0},
        ],
        "frames": [
            {
                "camlocator": {
                    "rotation": [0.0, 0.0, 0.0, 1.0],
                    "translation": [1.0, 2.0, 3.0],
                },
                "camera": {
                    "rotation": [0.7071067811865475, 0.0, 0.0, 0.7071067811865476],
                    "translation": [0.0, 0.0, 1.0],
                },
            }
        ],
    }
    with tempfile.TemporaryDirectory() as temporary_directory:
        root = pathlib.Path(temporary_directory)
        source = root / "camera.json"
        destination = root / "camera.pcam"
        source.write_text(json.dumps(document), encoding="utf-8")
        convert(source, destination)
        payload = destination.read_bytes()

        frame_data = document["frames"][0]
        frame_data["CH_Cam"] = frame_data.pop("camera")
        document["bones"][1]["name"] = "CH_Cam"
        half_roll = math.pi / 8.0
        c = math.cos(half_roll) / math.sqrt(2.0)
        s = math.sin(half_roll) / math.sqrt(2.0)
        frame_data["CH_Cam"]["rotation"] = [c, -s, s, c]
        source.write_text(json.dumps(document), encoding="utf-8")
        convert(source, destination, "CH_Cam", preserve_roll=True)
        rolled_frame = struct.unpack_from("<9f", destination.read_bytes(), 24)

    header = struct.unpack_from("<6I", payload)
    assert header == (PCAM_MAGIC, PCAM_VERSION, 1, 30, PCAM_FRAME_SIZE, PCAM_FLAG_LOOP)
    assert len(payload) == 24 + PCAM_FRAME_SIZE
    frame = struct.unpack_from("<9f", payload, 24)
    expected = (1.0, -4.0, 2.0, 0.0, 0.0, 1.0, 0.0, 1.0, 0.0)
    assert all(math.isclose(actual, wanted, abs_tol=0.000001) for actual, wanted in zip(frame, expected))
    up = 1.0 / math.sqrt(2.0)
    expected_rolled = (1.0, -4.0, 2.0, 0.0, 0.0, 1.0, up, up, 0.0)
    assert all(math.isclose(actual, wanted, abs_tol=0.000001) for actual, wanted in zip(rolled_frame, expected_rolled))


if __name__ == "__main__":
    main()
