#!/usr/bin/env python3

import argparse
import pathlib
import struct


PADP_MAGIC = 0x50444150
PADP_VERSION = 2
SAMPLE_RATE = 44100
CHANNELS = 2
BLOCK_SAMPLE_FRAMES = 1024
CHANNEL_DATA_BYTES = 512
BLOCK_BYTES = 1032
DEFAULT_SAMPLE_FRAMES = 3072
AMPLITUDE = 1200
HALF_PERIOD_FRAMES = 50

STEP_TABLE = (
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
    143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
    494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026,
    4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767,
)

INDEX_TABLE = (-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8)


def encode_channel(samples: list[int]) -> bytes:
    predictor = samples[0]
    step_index = 0
    codes: list[int] = []

    for sample in samples[1:]:
        step = STEP_TABLE[step_index]
        difference = sample - predictor
        code = 0
        if difference < 0:
            code = 8
            difference = -difference

        delta = step >> 3
        if difference >= step:
            code |= 4
            difference -= step
            delta += step
        if difference >= step >> 1:
            code |= 2
            difference -= step >> 1
            delta += step >> 1
        if difference >= step >> 2:
            code |= 1
            delta += step >> 2

        predictor += -delta if code & 8 else delta
        predictor = max(-32768, min(32767, predictor))
        step_index = max(0, min(88, step_index + INDEX_TABLE[code]))
        codes.append(code)

    output = bytearray(struct.pack("<hBB", samples[0], 0, 0))
    for index in range(0, len(codes), 2):
        low = codes[index]
        high = codes[index + 1] if index + 1 < len(codes) else 0
        output.append(low | (high << 4))
    output.extend(bytes(CHANNEL_DATA_BYTES - (len(output) - 4)))
    return bytes(output)


def write_padp(
    destination: pathlib.Path,
    channel_samples: list[list[int]],
    loop_start: int,
    loop_end: int,
) -> None:
    if len(channel_samples) != CHANNELS:
        raise ValueError(f"PADP requires exactly {CHANNELS} channels")
    sample_frames = len(channel_samples[0])
    if sample_frames <= BLOCK_SAMPLE_FRAMES:
        raise ValueError("ADPCM stream must be longer than one block")
    if any(len(samples) != sample_frames for samples in channel_samples):
        raise ValueError("PADP channels must contain the same frame count")
    if not 0 <= loop_start < loop_end <= sample_frames:
        raise ValueError("PADP loop range must be inside the stream")
    if loop_start % BLOCK_SAMPLE_FRAMES or loop_end % BLOCK_SAMPLE_FRAMES:
        raise ValueError("PADP loop points must be aligned to 1024 frames")

    block_count = (sample_frames + BLOCK_SAMPLE_FRAMES - 1) // BLOCK_SAMPLE_FRAMES
    output = bytearray(
        struct.pack(
            "<10I",
            PADP_MAGIC,
            PADP_VERSION,
            SAMPLE_RATE,
            CHANNELS,
            sample_frames,
            BLOCK_SAMPLE_FRAMES,
            BLOCK_BYTES,
            block_count,
            loop_start,
            loop_end,
        )
    )

    for block in range(block_count):
        start = block * BLOCK_SAMPLE_FRAMES
        for samples in channel_samples:
            block_samples = samples[start : start + BLOCK_SAMPLE_FRAMES]
            block_samples.extend([block_samples[-1]] * (BLOCK_SAMPLE_FRAMES - len(block_samples)))
            output.extend(encode_channel(block_samples))

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output)


def generate(destination: pathlib.Path, sample_frames: int, loop_start: int, loop_end: int) -> None:
    samples = [
        AMPLITUDE if (frame // HALF_PERIOD_FRAMES) % 2 == 0 else -AMPLITUDE
        for frame in range(sample_frames)
    ]
    write_padp(destination, [samples, samples], loop_start, loop_end)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate the PSP bootstrap IMA ADPCM stream")
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--sample-frames", type=int, default=DEFAULT_SAMPLE_FRAMES)
    parser.add_argument("--loop-start", type=int, default=1024)
    parser.add_argument("--loop-end", type=int)
    args = parser.parse_args()
    loop_end = args.loop_end if args.loop_end is not None else args.sample_frames
    generate(args.destination, args.sample_frames, args.loop_start, loop_end)


if __name__ == "__main__":
    main()
