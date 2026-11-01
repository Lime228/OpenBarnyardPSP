#!/usr/bin/env python3

import argparse
import pathlib
import struct
import wave

from generate_bootstrap_adpcm import CHANNELS, SAMPLE_RATE, write_padp


def convert(
    source: pathlib.Path,
    destination: pathlib.Path,
    loop_start: int,
    loop_end: int | None,
) -> None:
    with wave.open(str(source), "rb") as wav_file:
        if wav_file.getnchannels() != CHANNELS:
            raise ValueError(f"source WAV must contain exactly {CHANNELS} channels")
        if wav_file.getsampwidth() != 2:
            raise ValueError("source WAV must use signed 16-bit PCM samples")
        if wav_file.getframerate() != SAMPLE_RATE:
            raise ValueError(f"source WAV must use a {SAMPLE_RATE} Hz sample rate")
        if wav_file.getcomptype() != "NONE":
            raise ValueError("source WAV must be uncompressed PCM")

        sample_frames = wav_file.getnframes()
        interleaved = struct.unpack(
            f"<{sample_frames * CHANNELS}h",
            wav_file.readframes(sample_frames),
        )

    channel_samples = [
        list(interleaved[channel::CHANNELS])
        for channel in range(CHANNELS)
    ]
    resolved_loop_end = loop_end if loop_end is not None else sample_frames
    write_padp(destination, channel_samples, loop_start, resolved_loop_end)


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert PCM16 stereo WAV to PSP PADP")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--loop-start", type=int, default=0)
    parser.add_argument("--loop-end", type=int)
    args = parser.parse_args()
    convert(args.source, args.destination, args.loop_start, args.loop_end)


if __name__ == "__main__":
    main()
