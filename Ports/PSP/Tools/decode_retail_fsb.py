#!/usr/bin/env python3
"""Decode every inventoried retail FSB track to native-rate PCM WAV."""

import argparse
import json
import subprocess
import tempfile
import wave
from pathlib import Path

from inventory_retail_fsb import find_banks


def decode_bank(decoder: Path, source: Path, destination: Path, tracks: list[dict]) -> list[dict]:
    if not tracks:
        return []
    destination.mkdir(parents=True, exist_ok=True)
    results = []
    with tempfile.TemporaryDirectory(prefix="decode-", dir=destination) as directory:
        temp = Path(directory)
        with (temp / "decoder.log").open("w+b") as log:
            run = subprocess.run(
                [str(decoder), "-i", "-s", "1", "-S", str(len(tracks)), "-o", "?06s.wav", str(source)],
                cwd=temp,
                stdout=log,
                stderr=subprocess.STDOUT,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
                check=False,
            )
            log.seek(max(0, log.tell() - 4096))
            last_lines = log.read().decode("utf-8", errors="replace").splitlines()[-4:]

        for track in tracks:
            filename = f"{track['index']:06}.wav"
            result = {"index": track["index"], "name": track["name"]}
            try:
                decoded = temp / filename
                with wave.open(str(decoded), "rb") as wav:
                    sample_rate = wav.getframerate()
                    channels = wav.getnchannels()
                    frames = wav.getnframes()
                    if wav.getsampwidth() != 2:
                        raise ValueError("expected 16-bit PCM output")
                    if (sample_rate, channels) != (track["sample_rate"], track["channels"]):
                        raise ValueError(f"unexpected WAV format: {sample_rate} Hz, {channels} channels")
                    if frames <= 0 or frames != track["frames"]:
                        raise ValueError(f"decoded {frames} frames, expected {track['frames']}")
                    wav.setpos(frames - 1)
                    if len(wav.readframes(1)) != channels * 2:
                        raise ValueError("decoded WAV is truncated")
                decoded.replace(destination / filename)
                result.update(
                    status="decoded",
                    output=filename,
                    sample_rate=sample_rate,
                    channels=channels,
                    frames=frames,
                    source_frames=track["frames"],
                )
            except (OSError, EOFError, ValueError, wave.Error) as error:
                result.update(status="failed", error=str(error))
            results.append(result)
        if run.returncode != 0 and all(track["status"] == "decoded" for track in results):
            raise ValueError(f"decoder exited with {run.returncode}: {' '.join(last_lines)}")
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decoder", required=True, type=Path)
    parser.add_argument("install", type=Path)
    parser.add_argument("inventory", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    decoder = args.decoder.resolve(strict=True)
    install = args.install.resolve(strict=True)
    inventory = json.loads(args.inventory.read_text(encoding="utf-8"))
    if inventory.get("version") != 1 or not inventory.get("banks"):
        parser.error("expected a complete FSB inventory from inventory_retail_fsb.py")
    if [bank["path"] for bank in inventory["banks"]] != [path.as_posix() for path in find_banks(install)]:
        parser.error("FSB inventory does not match the installation, regenerate it before decoding")

    args.output.mkdir(parents=True, exist_ok=True)
    banks = []
    for bank in inventory["banks"]:
        relative_path = Path(bank["path"])
        source = (install / relative_path).resolve()
        if relative_path.is_absolute() or ".." in relative_path.parts or not source.is_relative_to(install):
            parser.error(f"invalid bank path in inventory: {relative_path}")
        result = {"path": relative_path.as_posix(), "status": "failed", "tracks": []}
        try:
            if bank["status"] != "inventoried":
                raise ValueError(bank["error"])
            output = args.output / relative_path
            result["tracks"] = decode_bank(decoder, source, output, bank["tracks"])
            if all(track["status"] == "decoded" for track in result["tracks"]):
                result["status"] = "decoded"
        except (OSError, ValueError) as error:
            result["error"] = str(error)
        banks.append(result)

    failed = sum(bank["status"] == "failed" for bank in banks)
    decoded = sum(track["status"] == "decoded" for bank in banks for track in bank["tracks"])
    report = {
        "version": 1,
        "bank_count": len(banks),
        "failed_bank_count": failed,
        "source_track_count": inventory["track_count"],
        "decoded_track_count": decoded,
        "banks": banks,
    }
    report_path = args.output / "fsb.decode.json"
    temporary_path = report_path.with_suffix(".json.tmp")
    temporary_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    temporary_path.replace(report_path)
    print(f"{len(banks) - failed}/{len(banks)} banks, {decoded}/{inventory['track_count']} tracks")
    return int(failed != 0 or decoded != inventory["track_count"])


if __name__ == "__main__":
    raise SystemExit(main())
