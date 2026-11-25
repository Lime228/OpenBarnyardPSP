#!/usr/bin/env python3
"""Prepare the original SS01 player-creation atlas textures and section bounds."""

import argparse
import json
import struct
import subprocess
import tempfile
from pathlib import Path

from convert_dds import convert as convert_texture
from inventory_retail_models import run_export, run_tmdl
from inventory_retail_sequence import retail_file


ATLAS_SOURCES = {
    r"hud\cow_hides.tga": ("Data/Assets/lib_ss01.trb", "gui_by_ssequi_TTL"),
    r"hud\mugs_girl_cows.tga": ("Data/Assets/lib_startup.trb", "gui_hud_TTL"),
}


def package(tool: Path, install: Path, output: Path, camera_sequence: Path) -> dict:
    source = retail_file(install, "Data/GUI/texsec.trb")
    timeline = json.loads(camera_sequence.read_text(encoding="utf-8"))
    shot, = [shot for shot in timeline["shots"] if shot["ui"] == "Player_select" and shot["loop"]]
    if timeline["sequence"] != "SS01_start" or Path(shot["file"]).name != shot["file"]:
        raise ValueError("expected the original SS01 character creation camera")
    output.mkdir(parents=True, exist_ok=True)
    atlases = []
    with tempfile.TemporaryDirectory(dir=output) as temporary:
        work = Path(temporary)
        sections = run_export(tool, "-psp-gui-sections", source, work / "gui-sections.json")
        scratch = work / "atlas.dds"
        for texture, (library, symbol) in ATLAS_SOURCES.items():
            entries = [s for s in sections["sections"] if s["texture"].casefold() == texture and s["name"].startswith("Hide_")]
            if len(entries) != 7 or len({s["name"] for s in entries}) != 7:
                raise ValueError(f"expected seven retail player hide sections in {texture}")
            packed = retail_file(install, library)
            source_name = entries[0]["texture"]
            run_tmdl(tool, "-psp-texture", packed, scratch, "-texture-symbol", symbol, "-texture", source_name)
            filename = source_name.replace("\\", "/").split("/")[-1].removesuffix(".tga") + ".ptex"
            details = convert_texture(scratch, work / filename)
            for section in entries:
                x0, y0 = section["point1"]
                x1, y1 = section["point2"]
                width, height = details["width"], details["height"]
                # The original Brahman section extends one texel past the last row.
                # The GUI sampler clamps at the edge, so preserve those coordinates.
                if not (0 <= x0 < width and 0 <= y0 < height and
                        x0 < x1 <= width + 1 and y0 < y1 <= height + 1):
                    raise ValueError(f"section {section['name']} lies outside {source_name}")
            atlases.append({"file": filename, "texture": source_name, "library": library, "symbol": symbol,
                            "width": details["width"], "height": details["height"], "sections": entries})
        camera = (camera_sequence.parent / shot["file"]).read_bytes()
        header = struct.unpack_from("<4s5I", camera)
        if header != (b"PCAM", 1, shot["frames"], shot["sample_rate"], 36, 1) or len(camera) != 24 + shot["frames"] * 36:
            raise ValueError("invalid character creation camera cache")
        (work / "opening-ui.pcam").write_bytes(camera)
        (work / "opening-ui.psui").write_bytes(struct.pack("<4sIff", b"PSUI", 1, shot["horizontal_fov_degrees"], shot["near_clip"]))
        files = ["opening-ui.pcam", "opening-ui.psui", *(atlas["file"] for atlas in atlases)]
        report = {"version": 2, "sequence": "SS01_start", "command": "Player_select", "atlases": atlases,
                  "files": {filename: (work / filename).stat().st_size for filename in files}}
        pending = work / "character_setup.json"
        pending.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        for filename in files:
            (work / filename).replace(output / filename)
        pending.replace(output / pending.name)
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", type=Path, required=True)
    parser.add_argument("--camera-sequence", type=Path, required=True)
    parser.add_argument("install", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        tool = args.tmdl.resolve(strict=True)
        (tool.parent / "Logs").mkdir(exist_ok=True)
        report = package(tool, args.install.resolve(strict=True), args.output.resolve(), args.camera_sequence)
    except (KeyError, OSError, TypeError, ValueError, struct.error, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"Character setup packaging failed: {error}\n")
    print(f"Prepared {sum(len(atlas['sections']) for atlas in report['atlases'])} original player hide sections")


if __name__ == "__main__":
    main()
