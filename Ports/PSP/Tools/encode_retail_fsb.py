#!/usr/bin/env python3
"""Encode a full decoded FSB inventory to the existing PSP audio formats."""

import argparse
import json
import struct
import sys
import wave
from contextlib import ExitStack
from pathlib import Path

from convert_audio import MAX_RESIDENT_SAMPLE_FRAMES
from generate_bootstrap_adpcm import BLOCK_BYTES, BLOCK_SAMPLE_FRAMES, PADP_MAGIC, PADP_VERSION, SAMPLE_RATE
from generate_bootstrap_audio import PPCM_MAGIC, PPCM_VERSION

try:
    import audioop
except ImportError:
    audioop = None


PCM_WIDTH = 2
MAX_STREAM_FRAMES = SAMPLE_RATE * 300
SEGMENT_FRAMES = MAX_STREAM_FRAMES // BLOCK_SAMPLE_FRAMES * BLOCK_SAMPLE_FRAMES
NIBBLE_SWAP = bytes((value >> 4) | ((value & 15) << 4) for value in range(256))


def scaled_frames(frames: int, source_rate: int) -> int:
    return (frames * SAMPLE_RATE + source_rate // 2) // source_rate


def resampled_blocks(wav: wave.Wave_read, target_frames: int):
    frame_size = wav.getnchannels() * PCM_WIDTH
    bytes_left = target_frames * frame_size
    block_bytes = BLOCK_SAMPLE_FRAMES * frame_size
    pending = bytearray()
    state = None
    last_frame = b""
    source_frames = 0
    while chunk := wav.readframes(32768):
        if len(chunk) % frame_size:
            raise ValueError("truncated PCM frame")
        source_frames += len(chunk) // frame_size
        last_frame = chunk[-frame_size:]
        if wav.getframerate() != SAMPLE_RATE:
            chunk, state = audioop.ratecv(
                chunk, PCM_WIDTH, wav.getnchannels(), wav.getframerate(), SAMPLE_RATE, state
            )
        chunk = chunk[:bytes_left]
        bytes_left -= len(chunk)
        pending.extend(chunk)
        end = len(pending) // block_bytes * block_bytes
        for offset in range(0, end, block_bytes):
            yield bytes(pending[offset : offset + block_bytes])
        del pending[:end]

    if source_frames != wav.getnframes() or not last_frame:
        raise ValueError("truncated or empty source WAV")
    pending.extend(last_frame * (bytes_left // frame_size))
    for offset in range(0, len(pending), block_bytes):
        yield bytes(pending[offset : offset + block_bytes])


def encode_channel_block(samples: bytes) -> bytes:
    predictor = struct.unpack_from("<h", samples)[0]
    last_sample = samples[-PCM_WIDTH:]
    # audioop puts the first code in the high nibble. PADP puts it in the low nibble.
    padded = samples[PCM_WIDTH:] + last_sample * (BLOCK_SAMPLE_FRAMES + 1 - len(samples) // PCM_WIDTH)
    codes, _ = audioop.lin2adpcm(padded, PCM_WIDTH, (predictor, 0))
    codes = codes.translate(NIBBLE_SWAP)
    return struct.pack("<hBB", predictor, 0, 0) + codes[:-1] + bytes([codes[-1] & 15])


def channel_pairs(channels: int) -> list[tuple[int, int]]:
    return {1: [(0, 0)], 2: [(0, 1)], 4: [(0, 1), (2, 3)]}[channels]


def write_audio_block(files: list, data: bytes, channels: int, resident: bool) -> None:
    if resident:
        if channels == 1:
            files[0].write(audioop.tostereo(data, PCM_WIDTH, 1, 1))
        elif channels == 2:
            files[0].write(data)
        else:
            samples = memoryview(data).cast("i")
            for file, side in zip(files, (0, 1)):
                file.write(samples[side::2].tobytes())
        return

    interleaved = memoryview(data).cast("h")
    encoded = [encode_channel_block(interleaved[channel::channels].tobytes())
               for channel in range(channels)]
    for file, (left, right) in zip(files, channel_pairs(channels)):
        file.write(encoded[left] + encoded[right])


def loop_bounds(start: int, length: int, source_start: int, source_end: int) -> tuple[int, int]:
    maximum = length // BLOCK_SAMPLE_FRAMES * BLOCK_SAMPLE_FRAMES
    exact_end = start + length
    if source_end <= start or source_start >= exact_end:
        return 0, maximum
    first = max(0, source_start - start)
    last = min(length, source_end - start)
    aligned_start = ((first + BLOCK_SAMPLE_FRAMES // 2) // BLOCK_SAMPLE_FRAMES) * BLOCK_SAMPLE_FRAMES
    aligned_end = ((last + BLOCK_SAMPLE_FRAMES // 2) // BLOCK_SAMPLE_FRAMES) * BLOCK_SAMPLE_FRAMES
    aligned_start = min(maximum - BLOCK_SAMPLE_FRAMES, aligned_start)
    aligned_end = min(maximum, max(aligned_start + BLOCK_SAMPLE_FRAMES, aligned_end))
    return aligned_start, aligned_end


def encode_track(source: Path, destination: Path, track: dict) -> dict:
    with wave.open(str(source), "rb") as wav:
        rate, channels, frames = wav.getframerate(), wav.getnchannels(), wav.getnframes()
        if (rate, channels, frames) != (track["sample_rate"], track["channels"], track["frames"]):
            raise ValueError("WAV disagrees with FSB inventory")
        if wav.getsampwidth() != PCM_WIDTH or wav.getcomptype() != "NONE":
            raise ValueError("expected PCM16 WAV")
        if rate <= 0 or frames <= 0 or channels not in (1, 2, 4):
            raise ValueError("invalid frame count, rate or channel layout")

        target_frames = max(1, scaled_frames(frames, rate))
        source_start = track["loop_start"]
        source_end = track["loop_end_inclusive"]
        if 0 <= source_start < frames and source_start <= source_end:
            exact_loop_start = scaled_frames(source_start, rate)
            exact_loop_end = min(target_frames, scaled_frames(min(frames, source_end + 1), rate))
        else:
            exact_loop_start, exact_loop_end = 0, target_frames

        blocks = iter(resampled_blocks(wav, target_frames))
        outputs = []
        pending = []
        pairs = channel_pairs(channels)
        destination.mkdir(parents=True, exist_ok=True)
        for part, start in enumerate(range(0, target_frames, SEGMENT_FRAMES)):
            length = min(SEGMENT_FRAMES, target_frames - start)
            resident = length <= MAX_RESIDENT_SAMPLE_FRAMES
            kind = "ppcm" if resident else "padp"
            block_count = (length + BLOCK_SAMPLE_FRAMES - 1) // BLOCK_SAMPLE_FRAMES
            if resident:
                header = struct.pack("<5I", PPCM_MAGIC, PPCM_VERSION, SAMPLE_RATE, 2, length)
                loop_start = loop_end = None
                file_bytes = len(header) + length * 4
            else:
                loop_start, loop_end = loop_bounds(start, length, exact_loop_start, exact_loop_end)
                header = struct.pack(
                    "<10I", PADP_MAGIC, PADP_VERSION, SAMPLE_RATE, 2, length,
                    BLOCK_SAMPLE_FRAMES, BLOCK_BYTES, block_count, loop_start, loop_end,
                )
                file_bytes = len(header) + block_count * BLOCK_BYTES

            with ExitStack() as stack:
                files = []
                for pair, channels_used in enumerate(pairs):
                    name = f"{track['index']:06}-{part:02}-{pair}.{kind}"
                    output = destination / name
                    temporary = output.with_suffix(f".{kind}.tmp")
                    pending.append((temporary, output))
                    file = stack.enter_context(temporary.open("wb"))
                    file.write(header)
                    files.append(file)
                    outputs.append({
                        "path": name, "channels": channels_used, "part": part,
                        "start_frame": start, "frames": length, "bytes": file_bytes,
                        "loop_start_frame": loop_start, "loop_end_frame": loop_end,
                    })
                for frame in range(0, length, BLOCK_SAMPLE_FRAMES):
                    data = next(blocks, b"")
                    expected_bytes = min(BLOCK_SAMPLE_FRAMES, length - frame) * channels * PCM_WIDTH
                    if len(data) != expected_bytes:
                        raise ValueError("incorrect resampled block size")
                    write_audio_block(files, data, channels, resident)
                if any(file.tell() != file_bytes for file in files):
                    raise ValueError("incorrect PSP audio output size")

        if next(blocks, None) is not None:
            raise ValueError("unexpected trailing resampled frames")
        for temporary, output in pending:
            temporary.replace(output)

    return {
        "output_frames": target_frames, "output_sample_rate": SAMPLE_RATE,
        "source_loop_start": source_start, "source_loop_end_inclusive": source_end,
        "source_mode": track["mode"],
        "resampled_loop_start": exact_loop_start, "resampled_loop_end": exact_loop_end,
        "requires_chaining": target_frames > SEGMENT_FRAMES, "outputs": outputs,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inventory", type=Path)
    parser.add_argument("decoded", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if audioop is None or sys.byteorder != "little":
        parser.error("requires little-endian Python 3.12 or Python 3.13+ with audioop-lts")

    inventory = json.loads(args.inventory.read_text(encoding="utf-8"))
    decoded = json.loads((args.decoded / "fsb.decode.json").read_text(encoding="utf-8"))
    if (inventory.get("version") != 1 or decoded.get("version") != 1 or
        inventory["failed_bank_count"] or decoded["failed_bank_count"] or
        decoded["decoded_track_count"] != inventory["track_count"]):
        parser.error("requires a complete, successful FSB inventory and decode report")
    banks = []
    for source_bank, decoded_bank in zip(inventory["banks"], decoded["banks"], strict=True):
        path = Path(source_bank["path"])
        if path.is_absolute() or ".." in path.parts or path.as_posix() != decoded_bank["path"]:
            parser.error(f"invalid or mismatched bank path: {path}")
        tracks = []
        for track, decoded_track in zip(source_bank["tracks"], decoded_bank["tracks"], strict=True):
            index = track["index"]
            filename = f"{index:06}.wav"
            if (decoded_track["index"], decoded_track["output"], decoded_track["status"]) != (index, filename, "decoded"):
                parser.error(f"invalid or mismatched decoded track: {path} #{index}")
            result = {"index": index, "name": track["name"], "status": "failed",
                      "source_channels": track["channels"], "source_rate": track["sample_rate"],
                      "source_frames": track["frames"]}
            try:
                result.update(encode_track(args.decoded / path / filename, args.output / path, track))
                result["status"] = "encoded"
            except (OSError, ValueError, EOFError, wave.Error) as error:
                result["error"] = str(error)
            tracks.append(result)
        banks.append({"path": path.as_posix(), "tracks": tracks})

    encoded = [track for bank in banks for track in bank["tracks"] if track["status"] == "encoded"]
    report = {
        "version": 1, "bank_count": len(banks), "source_track_count": inventory["track_count"],
        "encoded_track_count": len(encoded), "file_count": sum(len(track["outputs"]) for track in encoded),
        "bytes": sum(item["bytes"] for track in encoded for item in track["outputs"]), "banks": banks,
    }
    args.output.mkdir(parents=True, exist_ok=True)
    temporary_report = args.output / "fsb.encode.json.tmp"
    temporary_report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    temporary_report.replace(args.output / "fsb.encode.json")
    print(f"{len(encoded)}/{inventory['track_count']} tracks, {report['file_count']} PSP audio files")
    return int(len(encoded) != inventory["track_count"])


if __name__ == "__main__":
    raise SystemExit(main())
