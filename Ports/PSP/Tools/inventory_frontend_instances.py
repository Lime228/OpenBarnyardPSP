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


def parse_frontend_model_indices(source: pathlib.Path) -> list[int]:
    text = source.read_text(encoding="utf-8")
    match = re.search(
        r"static\s+TUINT\s+s_aFrontEndInstanceModels\s*\[\s*\]\s*=\s*\{(.*?)\};",
        text,
        re.DOTALL,
    )
    if match is None:
        raise ValueError("frontend instance model list was not found")
    indices = [int(token, 0) for token in re.findall(r"0[xX][0-9a-fA-F]+|\d+", match.group(1))]
    if not indices or len(indices) != len(set(indices)):
        raise ValueError("frontend instance model list is empty or contains duplicates")
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


def inventory(
    locator_dump: pathlib.Path,
    model_definitions_source: pathlib.Path,
    game_loader_source: pathlib.Path,
    install_root: pathlib.Path,
    destination: pathlib.Path,
    resident_sections: list[str] | None = None,
) -> dict[str, object]:
    locators, sections = load_locator_dump(locator_dump)
    definitions = parse_model_definitions(model_definitions_source)
    frontend_indices = parse_frontend_model_indices(game_loader_source)
    if max(frontend_indices) >= len(definitions):
        raise ValueError("frontend instance model index exceeds the definition table")

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

    frontend_index_set = set(frontend_indices)
    frontend_models = [definitions[index] for index in frontend_indices]
    missing_models = []
    for model in frontend_models:
        source_path = str(model["path"]).replace("\\", "/")
        if not (install_root / pathlib.PurePosixPath(source_path)).is_file():
            missing_models.append(model)
    if missing_models:
        names = ", ".join(str(model["name"]) for model in missing_models)
        raise ValueError(f"retail instance model files are missing: {names}")

    entries = []
    unmatched_locator_count = 0
    skipped_unloaded_locator_count = 0
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
        if model_index not in frontend_index_set:
            skipped_unloaded_locator_count += 1
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
    if len(resident_sections) != len(set(resident_sections)):
        raise ValueError("resident VIS section list contains duplicates")
    available_section_names = {str(section["name"]) for section in sections}
    unknown_resident_sections = [name for name in resident_sections if name not in available_section_names]
    if unknown_resident_sections:
        raise ValueError(f"resident VIS sections are missing: {', '.join(unknown_resident_sections)}")
    resident_entries = [entry for entry in entries if entry["vis_section_name"] in resident_sections]
    resident_model_indices = {int(entry["model_index"]) for entry in resident_entries}

    result = {
        "version": 1,
        "source_locator_count": len(locators),
        "vis_section_count": len(sections),
        "frontend_model_count": len(frontend_indices),
        "used_model_count": len(used_model_indices),
        "instance_count": len(entries),
        "resident_section_names": resident_sections,
        "resident_used_model_count": len(resident_model_indices),
        "resident_instance_count": len(resident_entries),
        "unmatched_locator_count": unmatched_locator_count,
        "skipped_unloaded_locator_count": skipped_unloaded_locator_count,
        "frontend_models": frontend_models,
        "sections": section_summaries,
        "instances": entries,
    }
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description="Inventory the exact original frontend instance placement")
    parser.add_argument("locator_dump", type=pathlib.Path)
    parser.add_argument("--model-definitions", type=pathlib.Path, required=True)
    parser.add_argument("--game-loader", type=pathlib.Path, required=True)
    parser.add_argument("--install-root", type=pathlib.Path, required=True)
    parser.add_argument("--resident-section", action="append", default=[])
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    result = inventory(
        args.locator_dump,
        args.model_definitions,
        args.game_loader,
        args.install_root,
        args.output,
        args.resident_section,
    )
    print(
        json.dumps(
            {
                "frontend_models": result["frontend_model_count"],
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
