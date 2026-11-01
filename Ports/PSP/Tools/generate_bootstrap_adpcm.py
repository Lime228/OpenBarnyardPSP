#!/usr/bin/env python3

import argparse
import pathlib
import struct


PADP_MAGIC = 0x50444150
PADP_VERSION = 1
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


def generate(destination: pathlib.Path, sample_frames: int) -> None:
    if sample_frames <= BLOCK_SAMPLE_FRAMES:
        raise ValueError("ADPCM stream must be longer than one block")

    samples = [
        AMPLITUDE if (frame // HALF_PERIOD_FRAMES) % 2 == 0 else -AMPLITUDE
        for frame in range(sample_frames)
    ]
    block_count = (sample_frames + BLOCK_SAMPLE_FRAMES - 1) // BLOCK_SAMPLE_FRAMES
    output = bytearray(
        struct.pack(
            "<8I",
            PADP_MAGIC,
            PADP_VERSION,
            SAMPLE_RATE,
            CHANNELS,
            sample_frames,
            BLOCK_SAMPLE_FRAMES,
            BLOCK_BYTES,
            block_count,
        )
    )

    for block in range(block_count):
        start = block * BLOCK_SAMPLE_FRAMES
        block_samples = samples[start : start + BLOCK_SAMPLE_FRAMES]
        block_samples.extend([block_samples[-1]] * (BLOCK_SAMPLE_FRAMES - len(block_samples)))
        for _channel in range(CHANNELS):
            output.extend(encode_channel(block_samples))

    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate the PSP bootstrap IMA ADPCM stream")
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--sample-frames", type=int, default=DEFAULT_SAMPLE_FRAMES)
    args = parser.parse_args()
    generate(args.destination, args.sample_frames)


if __name__ == "__main__":
    main()
