#!/usr/bin/env python3

import argparse
import json
import pathlib
import re


MAX_LOCATORS = 65535
MAX_VIS_SECTIONS = 4096


def parse_model_definitions(source: pathlib.Path) -> list[dict[str, object]]:
    text = source.read_text(encoding="utf-8")
    pattern = re.compile(
        r'\{\s*\d+\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,?\s*\}',
        re.DOTALL,
    )
    definitions = [
        {"index": index, "name": match.group(1), "path": match.group(2)}
        for index, match in enumerate(pattern.finditer(text))
    ]
    if not definitions:
        raise ValueError("instance model definition table is empty")
    return definitions


def parse_model_indices(source: pathlib.Path, table: str) -> list[int]:
    text = source.read_text(encoding="utf-8")
    match = re.search(
        rf"static\s+TUINT\s+{re.escape(table)}\s*\[\s*\]\s*=\s*\{{(.*?)\}};",
        text,
        re.DOTALL,
    )
    if match is None:
        raise ValueError(f"instance model table {table} was not found")
    indices = [int(token, 0) for token in re.findall(r"0[xX][0-9a-fA-F]+|\d+", match.group(1))]
    if not indices or len(indices) != len(set(indices)):
        raise ValueError(f"instance model table {table} is empty or contains duplicates")
    return indices


def load_locator_dump(source: pathlib.Path) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    document = json.loads(source.read_text(encoding="utf-8"))
    if not isinstance(document, dict) or document.get("version") != 1:
        raise ValueError("unsupported locator table contract version")
    locators = document.get("locators")
    sections = document.get("vis_sections")
    if not isinstance(locators, list) or not 1 <= len(locators) <= MAX_LOCATORS:
        raise ValueError("locator table has an invalid locator count")
    if document.get("locator_count") != len(locators):
        raise ValueError("locator table count does not match its array")
    if not isinstance(sections, list) or not 1 <= len(sections) <= MAX_VIS_SECTIONS:
        raise ValueError("locator table has an invalid VIS section count")
    if document.get("vis_section_count") != len(sections):
        raise ValueError("locator VIS section count does not match its array")
    return locators, sections


def main_game_residency(vis: pathlib.Path, locator_sections: list, visibility_row: str | None = None) -> list[str]:
    sections = json.loads(vis.read_text(encoding="utf-8"))["sections"]
    rows = sections[:1] if visibility_row is None else [section for section in sections if section["name"] == visibility_row]
    if len(rows) != 1:
        raise ValueError(f"missing or ambiguous main game visibility row: {visibility_row}")
    visible = {section["name"].casefold() for section, lod in zip(sections, rows[0]["visibility"], strict=True)
               if lod != "none"}
    # AInstanceManager always includes the undefined locator cell.
    visible.add("undefined")
    # Separate locator ranges can share a terrain name. The individual ranges are preserved in the inventory.
    return list(dict.fromkeys(section["name"] for section in locator_sections if section["name"].casefold() in visible))


def inventory(
    locator_dump: pathlib.Path,
    model_definitions_source: pathlib.Path,
    game_loader_source: pathlib.Path,
    install_root: pathlib.Path,
    destination: pathlib.Path,
    resident_sections: list[str] | None = None,
    main_game_vis: pathlib.Path | None = None,
    visibility_row: str | None = None,
) -> dict[str, object]:
    locators, sections = load_locator_dump(locator_dump)
    definitions = parse_model_definitions(model_definitions_source)
    table = "s_aFrontEndInstanceModels" if main_game_vis is None else "s_aInstanceModels0"
    model_indices = parse_model_indices(game_loader_source, table)
    if max(model_indices) >= len(definitions):
        raise ValueError("instance model index exceeds the definition table")

    section_by_locator: list[dict[str, object] | None] = [None] * len(locators)
    section_summaries = []
    for expected_index, section in enumerate(sections):
        if not isinstance(section, dict) or section.get("index") != expected_index:
            raise ValueError(f"VIS section {expected_index} has an invalid index")
        name = section.get("name")
        first = section.get("first_locator")
        count = section.get("locator_count")
        if not isinstance(name, str) or not name:
            raise ValueError(f"VIS section {expected_index} has an invalid name")
        if type(first) is not int or type(count) is not int or first < 0 or count < 0:
            raise ValueError(f"VIS section {expected_index} has an invalid range")
        if first > len(locators) or count > len(locators) - first:
            raise ValueError(f"VIS section {expected_index} exceeds the locator table")
        for locator_index in range(first, first + count):
            if section_by_locator[locator_index] is not None:
                raise ValueError(f"VIS section {expected_index} overlaps locator {locator_index}")
            section_by_locator[locator_index] = section
        section_summaries.append(
            {"index": expected_index, "name": name, "source_locator_count": count, "instance_count": 0}
        )

    index_set = set(model_indices)
    models = [definitions[index] for index in model_indices]
    missing_models = []
    for model in models:
        source_path = str(model["path"]).replace("\\", "/")
        if not (install_root / pathlib.PurePosixPath(source_path)).is_file():
            missing_models.append(model)
    if missing_models:
        names = ", ".join(str(model["name"]) for model in missing_models)
        raise ValueError(f"retail instance model files are missing: {names}")

    entries = []
    unmatched_locator_count = 0
    skipped_unloaded_locator_count = 0
    skipped_regrowth_count = 0
    regrowth_names = ()
    if main_game_vis is not None:
        engine = model_definitions_source.with_name("AInstanceManager.cpp").read_text(encoding="utf-8")
        match = re.search(r"s_apszRegrowthNames\[\]\s*=\s*\{([^}]+)\}", engine)
        if match is None:
            raise ValueError("original regrowth locator prefixes were not found")
        regrowth_names = tuple(re.findall(r'"([a-z0-9_]+)"', match[1]))
    used_model_indices = set()
    for expected_index, locator in enumerate(locators):
        if not isinstance(locator, dict) or locator.get("index") != expected_index:
            raise ValueError(f"locator {expected_index} has an invalid index")
        locator_name = locator.get("name")
        if not isinstance(locator_name, str) or not locator_name:
            raise ValueError(f"locator {expected_index} has an invalid name")
        matching_model = next(
            (
                model
                for model in definitions
                if locator_name.casefold().startswith(str(model["name"]).casefold())
            ),
            None,
        )
        if matching_model is None:
            unmatched_locator_count += 1
            continue
        model_index = int(matching_model["index"])
        if model_index not in index_set:
            skipped_unloaded_locator_count += 1
            continue
        if locator_name.casefold().startswith(regrowth_names):
            skipped_regrowth_count += 1
            continue
        section = section_by_locator[expected_index]
        entry = dict(locator)
        entry.update(
            {
                "model_index": model_index,
                "model_name": matching_model["name"],
                "model_path": matching_model["path"],
                "vis_section_index": None if section is None else section["index"],
                "vis_section_name": None if section is None else section["name"],
            }
        )
        entries.append(entry)
        used_model_indices.add(model_index)
        if section is not None:
            section_summaries[int(section["index"])]["instance_count"] += 1

    resident_sections = [] if resident_sections is None else resident_sections
    if main_game_vis is not None:
        if resident_sections:
            raise ValueError("the main game derives its resident sections from the original VIS")
        resident_sections = main_game_residency(main_game_vis, sections, visibility_row)
    if len(resident_sections) != len(set(resident_sections)):
        raise ValueError("resident VIS section list contains duplicates")
    available_section_names = {str(section["name"]) for section in sections}
    unknown_resident_sections = [name for name in resident_sections if name not in available_section_names]
    if unknown_resident_sections:
        raise ValueError(f"resident VIS sections are missing: {', '.join(unknown_resident_sections)}")
    resident_entries = [entry for entry in entries if entry["vis_section_name"] in resident_sections]
    resident_model_indices = {int(entry["model_index"]) for entry in resident_entries}

    kind = "frontend" if main_game_vis is None else "main_game"
    result = {
        "version": 1,
        "source_locator_count": len(locators),
        "vis_section_count": len(sections),
        f"{kind}_model_count": len(model_indices),
        "used_model_count": len(used_model_indices),
        "instance_count": len(entries),
        "resident_section_names": resident_sections,
        "resident_used_model_count": len(resident_model_indices),
        "resident_instance_count": len(resident_entries),
        "unmatched_locator_count": unmatched_locator_count,
        "skipped_unloaded_locator_count": skipped_unloaded_locator_count,
        f"{kind}_models": models,
        "sections": section_summaries,
        "instances": entries,
    }
    if main_game_vis is not None:
        result["skipped_regrowth_locator_count"] = skipped_regrowth_count
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description="Inventory the exact original instance placement")
    parser.add_argument("locator_dump", type=pathlib.Path)
    parser.add_argument("--model-definitions", type=pathlib.Path, required=True)
    parser.add_argument("--game-loader", type=pathlib.Path, required=True)
    parser.add_argument("--install-root", type=pathlib.Path, required=True)
    parser.add_argument("--resident-section", action="append", default=[])
    parser.add_argument("--main-game-vis", type=pathlib.Path)
    parser.add_argument("--visibility-row")
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    if args.visibility_row is not None and args.main_game_vis is None:
        parser.error("--visibility-row requires --main-game-vis")
    result = inventory(
        args.locator_dump,
        args.model_definitions,
        args.game_loader,
        args.install_root,
        args.output,
        args.resident_section,
        args.main_game_vis,
        args.visibility_row,
    )
    kind = "frontend" if args.main_game_vis is None else "main_game"
    print(
        json.dumps(
            {
                f"{kind}_models": result[f"{kind}_model_count"],
                "used_models": result["used_model_count"],
                "instances": result["instance_count"],
                "resident_models": result["resident_used_model_count"],
                "resident_instances": result["resident_instance_count"],
                "vis_sections": result["vis_section_count"],
            },
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
