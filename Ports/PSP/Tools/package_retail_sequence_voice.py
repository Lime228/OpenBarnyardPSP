#!/usr/bin/env python3
"""Select cutscene streams from an existing PSP FSB encode using the original TSO."""

import argparse
import json
import shutil
from pathlib import Path, PurePosixPath


def package(inventory: Path, encoded: Path, output: Path) -> dict:
    sequence = json.loads(inventory.read_text(encoding="utf-8"))
    audio = json.loads((encoded / "fsb.encode.json").read_text(encoding="utf-8"))
    if sequence.get("version") != 1 or audio.get("version") != 1:
        raise ValueError("expected sequence inventory and FSB encode manifests")

    voices = [entry for entry in sequence["script"]["voice_instructions"] if entry["type"] == "vostream"]
    copies = {}
    bank_path = dict(sequence["properties"]).get("VOStream")
    if voices:
        if not bank_path:
            raise ValueError("script uses voice substreams without declaring a VOStream bank")
        key = bank_path.replace("\\", "/").casefold()
        matches = [bank for bank in audio["banks"] if bank["path"].casefold() == key]
        if len(matches) != 1:
            raise ValueError(f"expected one encoded voice bank for {bank_path}")
        bank = matches[0]
        bank_path = bank["path"]
        tracks = {track["index"]: track for track in bank["tracks"]}
        for voice in voices:
            substream = voice["index"]
            # FMOD substreams are zero-indexed. The offline FSB report starts at 1.
            track = tracks.get(substream + 1)
            if not track or track["status"] != "encoded" or track["requires_chaining"] or len(track["outputs"]) != 1:
                raise ValueError(f"expected one encoded stream for substream {substream}")
            if PurePosixPath(track["name"]).stem.casefold() != voice["name"].casefold():
                raise ValueError(f"substream {substream} has mismatched names in TSO and FSB")
            item = track["outputs"][0]
            source = (encoded / bank_path / item["path"]).resolve(strict=True)
            if not source.is_relative_to(encoded) or source.suffix != ".padp" or source.stat().st_size != item["bytes"]:
                raise ValueError(f"invalid cached PADP for substream {substream}")
            with source.open("rb") as stream:
                if stream.read(4) != b"PADP":
                    raise ValueError(f"invalid PADP header for substream {substream}")
            copies[substream] = (source, {
                "substream": substream, "path": f"{substream}.padp", "name": voice["name"],
                "frames": item["frames"], "sample_rate": track["output_sample_rate"], "bytes": item["bytes"],
            })

    output.mkdir(parents=True, exist_ok=True)
    streams = []
    for source, entry in (copies[index] for index in sorted(copies)):
        temporary = output / (entry["path"] + ".tmp")
        shutil.copyfile(source, temporary)
        temporary.replace(output / entry["path"])
        streams.append(entry)
    report = {"version": 1, "sequence": sequence["sequence"], "source_bank": bank_path, "streams": streams}
    temporary = output / "voice.json.tmp"
    temporary.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    temporary.replace(output / "voice.json")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inventory", type=Path)
    parser.add_argument("encoded", type=Path, help="PSP FSB encode directory")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        report = package(args.inventory, args.encoded.resolve(strict=True), args.output)
    except (KeyError, OSError, ValueError) as error:
        parser.exit(1, f"Sequence voice packaging failed: {error}\n")
    print(f"Packaged {len(report['streams'])} original voice streams for {report['sequence']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
