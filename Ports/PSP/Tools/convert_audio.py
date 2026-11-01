#!/usr/bin/env python3

import argparse
import pathlib
import struct
import wave

from generate_bootstrap_adpcm import CHANNELS, SAMPLE_RATE, write_padp
from generate_bootstrap_audio import PPCM_MAGIC, PPCM_VERSION


MAX_RESIDENT_SAMPLE_FRAMES = 1024


def write_ppcm(
    destination: pathlib.Path,
    interleaved: tuple[int, ...],
    sample_frames: int,
) -> None:
    if not 1 <= sample_frames <= MAX_RESIDENT_SAMPLE_FRAMES:
        raise ValueError(
            f"resident PCM must contain 1-{MAX_RESIDENT_SAMPLE_FRAMES} sample frames"
        )

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
    output.extend(struct.pack(f"<{len(interleaved)}h", *interleaved))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output)


def convert(
    source: pathlib.Path,
    destination: pathlib.Path,
    loop_start: int,
    loop_end: int | None,
    output_format: str,
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

    if output_format == "ppcm":
        if loop_start != 0 or loop_end is not None:
            raise ValueError("resident PCM does not support stream loop points")
        write_ppcm(destination, interleaved, sample_frames)
        return
    if output_format != "padp":
        raise ValueError(f"unsupported output format: {output_format}")

    channel_samples = [
        list(interleaved[channel::CHANNELS])
        for channel in range(CHANNELS)
    ]
    resolved_loop_end = loop_end if loop_end is not None else sample_frames
    write_padp(destination, channel_samples, loop_start, resolved_loop_end)


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert PCM16 stereo WAV to PSP audio")
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--loop-start", type=int, default=0)
    parser.add_argument("--loop-end", type=int)
    parser.add_argument("--format", choices=("padp", "ppcm"), default="padp")
    args = parser.parse_args()
    convert(args.source, args.destination, args.loop_start, args.loop_end, args.format)


if __name__ == "__main__":
    main()
