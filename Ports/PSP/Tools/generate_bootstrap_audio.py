#!/usr/bin/env python3

import argparse
import pathlib
import struct
import wave


PPCM_MAGIC = 0x4D435050
PPCM_VERSION = 1
SAMPLE_RATE = 44100
CHANNELS = 2
DEFAULT_SAMPLE_FRAMES = 1024
AMPLITUDE = 1200
HALF_PERIOD_FRAMES = 50


def generate_samples(sample_frames: int, amplitude: int, half_period_frames: int) -> list[int]:
    samples: list[int] = []
    for frame in range(sample_frames):
        sample = amplitude if (frame // half_period_frames) % 2 == 0 else -amplitude
        samples.extend((sample, sample))
    return samples


def generate(
    destination: pathlib.Path,
    sample_frames: int,
    amplitude: int = AMPLITUDE,
    half_period_frames: int = HALF_PERIOD_FRAMES,
    output_format: str = "ppcm",
) -> None:
    if sample_frames <= 0:
        raise ValueError("sample frame count must be positive")
    if not 1 <= amplitude <= 32767:
        raise ValueError("amplitude must be in the range 1-32767")
    if half_period_frames <= 0:
        raise ValueError("half-period frame count must be positive")

    samples = generate_samples(sample_frames, amplitude, half_period_frames)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if output_format == "wav":
        with wave.open(str(destination), "wb") as wav_file:
            wav_file.setnchannels(CHANNELS)
            wav_file.setsampwidth(2)
            wav_file.setframerate(SAMPLE_RATE)
            wav_file.writeframes(struct.pack(f"<{len(samples)}h", *samples))
        return
    if output_format != "ppcm":
        raise ValueError(f"unsupported output format: {output_format}")

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
    destination.write_bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate the PSP bootstrap PCM wave")
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--sample-frames", type=int, default=DEFAULT_SAMPLE_FRAMES)
    parser.add_argument("--amplitude", type=int, default=AMPLITUDE)
    parser.add_argument("--half-period-frames", type=int, default=HALF_PERIOD_FRAMES)
    parser.add_argument("--format", choices=("ppcm", "wav"), default="ppcm")
    args = parser.parse_args()
    generate(
        args.destination,
        args.sample_frames,
        args.amplitude,
        args.half_period_frames,
        args.format,
    )


if __name__ == "__main__":
    main()
