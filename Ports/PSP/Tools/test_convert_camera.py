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
                    "rotation": [0.0, 0.0, 0.0, 1.0],
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

    header = struct.unpack_from("<6I", payload)
    assert header == (PCAM_MAGIC, PCAM_VERSION, 1, 30, PCAM_FRAME_SIZE, PCAM_FLAG_LOOP)
    assert len(payload) == 24 + PCAM_FRAME_SIZE
    frame = struct.unpack_from("<9f", payload, 24)
    expected = (1.0, -4.0, 2.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0)
    assert all(math.isclose(actual, wanted, abs_tol=0.000001) for actual, wanted in zip(frame, expected))


if __name__ == "__main__":
    main()
