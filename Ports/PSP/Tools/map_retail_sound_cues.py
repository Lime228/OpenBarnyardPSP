#!/usr/bin/env python3
"""Resolve ordered retail sound-bank properties against encoded PSP audio."""

import argparse
import json
import subprocess
from collections import Counter
from pathlib import Path, PurePosixPath


def entries(value: dict) -> list:
    pairs = value.get("properties") if isinstance(value, dict) else None
    if not isinstance(pairs, list):
        raise ValueError("expected an ordered property list")
    return pairs


def property_value(value: dict, key: str, default=None):
    matches = [item for name, item in entries(value) if name == key]
    if len(matches) > 1:
        raise ValueError(f"duplicate scalar property: {key}")
    return matches[0] if matches else default


def index_wave_banks(library: dict) -> dict[str, dict]:
    result = {}
    for item in library["symbols"]:
        symbol = item["symbol"]
        if not symbol.casefold().startswith("bank_"):
            continue
        roots = entries(item["root"])
        if len(roots) != 1:
            raise ValueError(f"{symbol}: expected one wave-bank definition")
        bank = roots[0][1]
        name = property_value(bank, "name")
        source_path = property_value(bank, "path")
        if not isinstance(name, str) or not isinstance(source_path, str):
            raise ValueError(f"{symbol}: wave-bank name or path missing")
        key = name.casefold()
        if key in result:
            raise ValueError(f"duplicate wave-bank name: {name}")
        path = PurePosixPath(source_path.replace("\\", "/"))
        if path.is_absolute() or not path.parts or ".." in path.parts or ":" in path.parts[0]:
            raise ValueError(f"{symbol}: invalid FSB path")
        result[key] = {
            "name": name,
            "symbol": symbol,
            "path": f"Data/Sound/FSB/{path}.fsb",
            "type": property_value(bank, "type"),
            "wave_count": sum(name.startswith("W") for name, _ in entries(bank)),
        }
    if not result:
        raise ValueError("no wave-bank definitions found")
    return result


def index_encoded_tracks(encoded: dict) -> dict[str, dict]:
    if encoded.get("version") != 1 or encoded["source_track_count"] != encoded["encoded_track_count"]:
        raise ValueError("expected a complete FSB encoding report")
    result = {}
    track_count = 0
    for bank in encoded["banks"]:
        key = bank["path"].casefold()
        if key in result:
            raise ValueError(f"duplicate encoded bank path: {bank['path']}")
        tracks = {}
        for track in bank["tracks"]:
            if track["status"] != "encoded" or not track["outputs"] or track["index"] in tracks:
                raise ValueError(f"{bank['path']}: invalid encoded track #{track['index']}")
            tracks[track["index"]] = track
        track_count += len(tracks)
        result[key] = {"path": bank["path"], "tracks": tracks}
    if track_count != encoded["encoded_track_count"]:
        raise ValueError("encoded track count does not match the report")
    return result


def mapped_track(bank: dict, track: dict) -> dict:
    return {
        "fsb": bank["path"],
        "track_index": track["index"],
        "track_name": track["name"],
        "source_channels": track["source_channels"],
        "requires_chaining": track["requires_chaining"],
        "files": [f"{bank['path']}/{item['path']}" for item in track["outputs"]],
    }


def resolve_wave(wave_banks: dict, tracks: dict, name: str, wave_index: int) -> dict:
    if not isinstance(name, str) or type(wave_index) is not int:
        raise ValueError("expected a wave-bank name and integer index")
    result = {"wave_bank": name, "wave_index": wave_index}
    definition = wave_banks.get(name.casefold())
    if definition is None:
        return {**result, "status": "unknown-wave-bank"}
    result["bank_symbol"] = definition["symbol"]
    if not 0 <= wave_index < definition["wave_count"]:
        return {**result, "status": "invalid-wave-index"}
    bank = tracks.get(definition["path"].casefold())
    if bank is None:
        return {**result, "status": "missing-fsb", "expected_path": definition["path"]}
    track = bank["tracks"].get(wave_index + 1)
    if track is None:
        return {**result, "status": "missing-encoded-track", "fsb": bank["path"]}

    return {**result, "status": "mapped", **mapped_track(bank, track)}


def map_sound_banks(source: Path, document: dict, wave_banks: dict, tracks: dict) -> list[dict]:
    result = []
    for item in document["symbols"]:
        for group_name, group in entries(item["root"]):
            count = property_value(group, "numsounds")
            sounds = [sound for name, sound in entries(group) if name == "Sound"]
            if type(count) is not int or count != len(sounds):
                raise ValueError(f"{source}: {group_name} count does not match its Sound rows")
            name = property_value(group, "name")
            if not isinstance(name, str):
                raise ValueError(f"{source}: {group_name} has no bank name")

            cues = []
            for sound in sounds:
                cue_id = property_value(sound, "id")
                cue_name = property_value(sound, "name")
                if type(cue_id) is not int or not isinstance(cue_name, str):
                    raise ValueError(f"{source}: invalid cue name or id")
                banks = property_value(sound, "banks")
                waves = property_value(sound, "waves")
                if not isinstance(banks, list) or not isinstance(waves, list) or len(banks) != len(waves):
                    raise ValueError(f"{source}: {cue_name} bank and wave arrays disagree")
                samples = [resolve_wave(wave_banks, tracks, label, index)
                           for label, index in zip(banks, waves, strict=True)]
                cues.append({
                    "id": cue_id,
                    "name": cue_name,
                    "category": property_value(sound, "category"),
                    "status": "mapped" if samples and all(s["status"] == "mapped" for s in samples)
                              else ("unmapped" if samples else "no-samples"),
                    "samples": samples,
                    "settings": [[key, value] for key, value in entries(sound)
                                 if key not in ("id", "name", "category", "banks", "waves")],
                })
            result.append({
                "source": source.as_posix(), "name": name, "localised": property_value(group, "localised", 0),
                "cues": cues,
            })
    return result


def map_multiplex_streams(document: dict, tracks: dict) -> list[dict]:
    result = []
    for item in document["symbols"]:
        for label, definition in entries(item["root"]):
            if label != "MultiplexStream":
                raise ValueError(f"unexpected multiplex entry: {label}")
            name = property_value(definition, "Name")
            path = property_value(definition, "FSB")
            categories = property_value(definition, "ChannelCategories")
            if not isinstance(name, str) or not isinstance(path, str) or not isinstance(categories, list):
                raise ValueError("incomplete multiplex definition")
            source_path = path.replace("\\", "/")
            bank = tracks.get(source_path.casefold())
            stream = {
                "name": name,
                "source_fsb": source_path,
                "channel_categories": categories,
                "settings": [[key, value] for key, value in entries(definition)
                             if key not in ("Name", "FSB", "ChannelCategories")],
                "status": "missing-fsb",
                "tracks": [],
            }
            if bank:
                stream["tracks"] = [mapped_track(bank, track) for track in bank["tracks"].values()]
                if not stream["tracks"]:
                    stream["status"] = "empty-fsb"
                elif any(track["source_channels"] != len(categories) for track in stream["tracks"]):
                    stream["status"] = "channel-mismatch"
                else:
                    stream["status"] = "mapped"
            result.append(stream)
    return result


def verify_linked_files(cache: Path, tracks: dict, references: list[dict]) -> dict:
    expected = {
        f"{bank['path']}/{item['path']}": item["bytes"]
        for bank in tracks.values()
        for track in bank["tracks"].values()
        for item in track["outputs"]
    }
    paths = sorted({path for item in references for path in item.get("files", [])})
    failures = []
    for name in paths:
        relative = Path(name)
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"invalid encoded file path: {name}")
        path = (cache / relative).resolve()
        if not path.is_relative_to(cache):
            raise ValueError(f"encoded file escapes cache: {name}")
        try:
            actual = path.stat().st_size
        except OSError:
            failures.append({"path": name, "error": "missing or unreadable"})
            continue
        if not path.is_file() or actual != expected[name]:
            failures.append({"path": name, "error": "size or file type differs from encoding report"})
    return {"referenced": len(paths), "verified": len(paths) - len(failures), "failures": failures}


def dump_properties(executable: Path, source: Path, destination: Path) -> dict:
    destination.parent.mkdir(parents=True, exist_ok=True)
    pending = destination.with_suffix(".json.tmp")
    run = subprocess.run(
        [str(executable), "-psp-properties", str(source), "-o", str(pending)],
        cwd=executable.parent, capture_output=True, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        check=False, timeout=45,
    )
    if run.returncode:
        diagnostic = (run.stdout + run.stderr)[-1200:].decode(errors="replace").strip()
        raise ValueError(f"TMDL exited with {run.returncode}: {diagnostic}")
    document = json.loads(pending.read_text(encoding="utf-8"))
    if document.get("version") != 1 or not document.get("symbols"):
        raise ValueError("expected an ordered TRB property export")
    pending.replace(destination)
    return document


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", type=Path, required=True)
    parser.add_argument("install", type=Path)
    parser.add_argument("encoded", type=Path, help="fsb.encode.json")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    install = args.install.resolve(strict=True)
    output = args.output.resolve()
    executable = args.tmdl.resolve(strict=True)
    tracks = index_encoded_tracks(json.loads(args.encoded.read_text(encoding="utf-8")))
    library = Path("Data/Assets/lib_wavebank.trb")
    multiplex = Path("Data/Sound/MultiplexStreams.trb")
    sources = sorted(
        (path.relative_to(install) for path in (install / "Data/Sound").iterdir()
         if path.is_file() and path.suffix.casefold() == ".trb"
         and path.stem.casefold().startswith("soundbank_")),
        key=lambda path: path.as_posix().casefold(),
    )
    if not sources:
        parser.error("no standalone cue definitions found")

    failures = []
    wave_banks = {}
    sound_banks = []
    streams = []
    for source in [library, *sources, multiplex]:
        try:
            document = dump_properties(executable, install / source, output / "Properties" / source.with_suffix(".json"))
            if source == library:
                wave_banks = index_wave_banks(document)
            elif source == multiplex:
                streams = map_multiplex_streams(document, tracks)
            else:
                sound_banks.extend(map_sound_banks(source, document, wave_banks, tracks))
        except (OSError, ValueError, subprocess.TimeoutExpired) as error:
            failures.append({"source": source.as_posix(), "error": str(error)})

    cues = [cue for bank in sound_banks for cue in bank["cues"]]
    statuses = Counter(cue["status"] for cue in cues)
    samples = [sample for cue in cues for sample in cue["samples"]]
    sample_statuses = Counter(sample["status"] for sample in samples)
    stream_statuses = Counter(stream["status"] for stream in streams)
    cue_ids = Counter(cue["id"] for cue in cues)
    files = verify_linked_files(args.encoded.resolve().parent, tracks,
                                samples + [track for stream in streams for track in stream["tracks"]])
    report = {
        "version": 1,
        "source_sound_bank_count": len(sources),
        "mapped_sound_bank_count": len(sound_banks),
        "wave_bank_count": len(wave_banks),
        "cue_count": len(cues),
        "cue_status": dict(sorted(statuses.items())),
        "sample_status": dict(sorted(sample_statuses.items())),
        "multiplex_stream_status": dict(sorted(stream_statuses.items())),
        "duplicate_cue_ids": sorted(key for key, count in cue_ids.items() if count > 1),
        "linked_files": files,
        "export_failures": failures,
        "wave_banks": list(wave_banks.values()),
        "sound_banks": sound_banks,
        "multiplex_streams": streams,
    }
    output.mkdir(parents=True, exist_ok=True)
    pending = output / "sound_cues.json.tmp"
    pending.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    pending.replace(output / "sound_cues.json")
    print(f"{statuses['mapped']}/{len(cues)} cues mapped, {stream_statuses['mapped']}/{len(streams)} multiplex streams mapped")
    print(f"{files['verified']}/{files['referenced']} referenced files verified, {len(failures)} table failures")
    return int(bool(failures or files["failures"] or report["duplicate_cue_ids"])
               or statuses["unmapped"] != 0 or stream_statuses["mapped"] != len(streams))


if __name__ == "__main__":
    raise SystemExit(main())
