#!/usr/bin/env python3

import argparse
import pathlib
import struct


PPCM_MAGIC = 0x4D435050
PPCM_VERSION = 1
SAMPLE_RATE = 44100
CHANNELS = 2
DEFAULT_SAMPLE_FRAMES = 1024
AMPLITUDE = 1200
HALF_PERIOD_FRAMES = 50


def generate(
    destination: pathlib.Path,
    sample_frames: int,
    amplitude: int = AMPLITUDE,
    half_period_frames: int = HALF_PERIOD_FRAMES,
) -> None:
    if sample_frames <= 0:
        raise ValueError("sample frame count must be positive")
    if not 1 <= amplitude <= 32767:
        raise ValueError("amplitude must be in the range 1-32767")
    if half_period_frames <= 0:
        raise ValueError("half-period frame count must be positive")

    samples: list[int] = []
    for frame in range(sample_frames):
        sample = amplitude if (frame // half_period_frames) % 2 == 0 else -amplitude
        samples.extend((sample, sample))

    output = bytearray(
        struct.pack(
            "<5I",
            PPCM_MAGIC,
            PPCM_VERSION,
            SAMPLE_RATE,
            CHANNELS,
            sample_frames,
        )
    )
    output.extend(struct.pack(f"<{len(samples)}h", *samples))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate the PSP bootstrap PCM wave")
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--sample-frames", type=int, default=DEFAULT_SAMPLE_FRAMES)
    parser.add_argument("--amplitude", type=int, default=AMPLITUDE)
    parser.add_argument("--half-period-frames", type=int, default=HALF_PERIOD_FRAMES)
    args = parser.parse_args()
    generate(args.destination, args.sample_frames, args.amplitude, args.half_period_frames)


if __name__ == "__main__":
    main()
