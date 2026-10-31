#!/usr/bin/env python3

import argparse
import pathlib
import struct


PPCM_MAGIC = 0x4D435050
PPCM_VERSION = 1
SAMPLE_RATE = 44100
CHANNELS = 2
SAMPLE_FRAMES = 1024
AMPLITUDE = 1200
HALF_PERIOD_FRAMES = 50


def generate(destination: pathlib.Path) -> None:
    samples: list[int] = []
    for frame in range(SAMPLE_FRAMES):
        sample = AMPLITUDE if (frame // HALF_PERIOD_FRAMES) % 2 == 0 else -AMPLITUDE
        samples.extend((sample, sample))

    output = bytearray(
        struct.pack(
            "<5I",
            PPCM_MAGIC,
            PPCM_VERSION,
            SAMPLE_RATE,
            CHANNELS,
            SAMPLE_FRAMES,
        )
    )
    output.extend(struct.pack(f"<{len(samples)}h", *samples))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate the PSP bootstrap PCM wave")
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()
    generate(args.destination)


if __name__ == "__main__":
    main()
