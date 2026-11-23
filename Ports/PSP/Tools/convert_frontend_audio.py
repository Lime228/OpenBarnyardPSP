#!/usr/bin/env python3
"""Package the original frontend UI samples and streamed music for PSP."""

import argparse
import json
import shutil
import struct
import wave
from pathlib import Path

from encode_retail_fsb import PCM_WIDTH, audioop, resampled_blocks, scaled_frames
from generate_bootstrap_adpcm import BLOCK_BYTES, BLOCK_SAMPLE_FRAMES, PADP_MAGIC, PADP_VERSION, SAMPLE_RATE
from generate_bootstrap_audio import PPCM_MAGIC, PPCM_VERSION
from generate_wave_bank import generate


CUE_NAMES = (
    "UI_NEGATIVE", "UI_MENUCLICK", "UI_MENUBACK", "UI_MENUOK",
    "UI_FE_MOVE", "UI_FE_SELECT", "UI_VO_CLICK",
)
WAVE_NAMES = {"negative", "click", "back", "harp02", "harp03", "harp_strum"}
MAX_WAVE_FRAMES = 57344
MAX_BANK_BYTES = 400 * 1024
PADP_HEADER = struct.Struct("<10I")


def read_wave(path: Path) -> bytes:
    with wave.open(str(path), "rb") as wav:
        if wav.getnchannels() != 1 or wav.getsampwidth() != PCM_WIDTH:
            raise ValueError(f"expected a mono PCM16 UI sample: {path}")
        frames = scaled_frames(wav.getnframes(), wav.getframerate())
        if not 1 <= frames <= MAX_WAVE_FRAMES:
            raise ValueError(f"UI sample exceeds its resident limit: {path}")
        header = struct.pack("<5I", PPCM_MAGIC, PPCM_VERSION, SAMPLE_RATE, 1, frames)
        return header + b"".join(resampled_blocks(wav, frames))


def copy_music(root: Path, destination: Path, cue: dict) -> None:
    samples = cue["samples"]
    if (cue["status"] != "mapped" or cue["category"] != "Music" or
            dict(cue["settings"]).get("trackloop") != [1] or len(samples) != 1):
        raise ValueError("unexpected original MUSIC_START_SCREEN cue")
    sample = samples[0]
    if (sample["status"] != "mapped" or sample["source_channels"] != 2 or
            sample["requires_chaining"] or len(sample["files"]) != 1):
        raise ValueError("MUSIC_START_SCREEN needs one stereo stream")

    source = root / "Encoded" / sample["files"][0]
    with source.open("rb") as stream:
        header = stream.read(PADP_HEADER.size)
    if len(header) != PADP_HEADER.size:
        raise ValueError(f"truncated PADP header: {source}")
    magic, version, rate, channels, frames, block_frames, block_bytes, blocks, loop_start, loop_end = (
        PADP_HEADER.unpack(header)
    )
    if (
        (magic, version, rate, channels) != (PADP_MAGIC, PADP_VERSION, SAMPLE_RATE, 2)
        or (block_frames, block_bytes) != (BLOCK_SAMPLE_FRAMES, BLOCK_BYTES)
        or blocks != (frames + block_frames - 1) // block_frames
        or not 0 <= loop_start < loop_end <= frames <= SAMPLE_RATE * 300
        or loop_start % block_frames or loop_end % block_frames
        or source.stat().st_size != PADP_HEADER.size + blocks * block_bytes
    ):
        raise ValueError(f"invalid PADP loop: {source}")

    shutil.copyfile(source, destination / "start_screen.padp")
    print(f"Copied original MUSIC_START_SCREEN ({frames * 1000 // rate} ms, {source.stat().st_size} bytes)")


def convert(root: Path, destination: Path) -> None:
    if audioop is None:
        raise ValueError("the FSB resampler requires Python 3.12 or audioop-lts")
    report = json.loads((root / "Cues" / "sound_cues.json").read_text(encoding="utf-8"))
    cues = {cue["name"]: cue for bank in report["sound_banks"] for cue in bank["cues"]}
    sources: dict[str, Path] = {}
    for name in CUE_NAMES:
        cue = cues.get(name)
        if cue is None or cue["status"] != "mapped":
            raise ValueError(f"original menu cue is missing: {name}")
        for sample in cue["samples"]:
            wave_name = Path(sample["track_name"]).stem.lower()
            if sample["wave_bank"] != "ui" or sample["status"] != "mapped":
                raise ValueError(f"unexpected source bank for {name}")
            source = root / "Decoded" / sample["fsb"] / f'{sample["track_index"]:06}.wav'
            if sources.setdefault(wave_name, source) != source:
                raise ValueError(f"ambiguous original UI sample: {wave_name}")

    if sources.keys() != WAVE_NAMES:
        raise ValueError(f"unexpected original menu wave list: {sorted(sources)}")

    waves = {name: read_wave(source) for name, source in sources.items()}
    total_bytes = sum(len(data) for data in waves.values())
    if total_bytes > MAX_BANK_BYTES:
        raise ValueError(f"menu bank exceeds {MAX_BANK_BYTES} bytes")

    destination.mkdir(parents=True, exist_ok=True)
    for name, data in waves.items():
        (destination / f"{name}.ppcm").write_bytes(data)
    generate(
        destination / "ui.pbnk",
        [f"{name}=local:Data/Sound/Menu/{name}.ppcm" for name in sorted(waves)],
        [],
    )
    print(f"Converted {len(waves)} original UI samples ({total_bytes} bytes)")
    copy_music(root, destination, cues["MUSIC_START_SCREEN"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("audio_banks", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    convert(args.audio_banks, args.destination)
