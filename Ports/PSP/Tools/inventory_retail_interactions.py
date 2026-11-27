#!/usr/bin/env python3
"""Resolve the original interaction sequences against their shared activity table."""

import argparse
import json
import subprocess
import tempfile
from pathlib import Path

from inventory_retail_models import run_export
from inventory_retail_sequence import main_properties, retail_file


ACTIVITIES = "Data/Interactions/ActivitySets1.trz"
INTERACTIONS = "Data/Interactions/AllInteractions.trz"


def inventory_interactions(tool: Path, install: Path, scratch: Path) -> dict:
    activity_source = retail_file(install, ACTIVITIES)
    interaction_source = retail_file(install, INTERACTIONS)
    activities = []
    for key, activity_set in main_properties(run_export(tool, "-psp-properties", activity_source,
                                                        scratch / "activities.json")):
        if key.casefold() == "activityset":
            activities.extend({"type": name, "properties": data["properties"]}
                              for name, data in activity_set["properties"])
    if not activities:
        raise ValueError("the original activity table is empty")

    interactions = []
    requirements = []
    properties = main_properties(run_export(tool, "-psp-properties", interaction_source,
                                            scratch / "interactions.json"))
    for source_index, (key, data) in enumerate(properties):
        fields = data["properties"]
        if key.casefold() == "requirements":
            requirements.extend(fields)
        if key.casefold() != "interaction":
            continue
        activity_fields = next((value["properties"] for name, value in fields if name == "activities"), [])
        indices = [index for _, group in activity_fields for index in group]
        if any(type(index) is not int or not 0 <= index < len(activities) for index in indices):
            raise ValueError(f"interaction {source_index} references an unknown activity")
        interactions.append({
            "source_index": source_index, "activity_indices": indices,
            "properties": [[name, value] for name, value in fields if name != "activities"],
        })
    if not interactions or not requirements:
        raise ValueError("the original interaction table or requirements are empty")

    return {"version": 1, "sources": [ACTIVITIES, INTERACTIONS], "activities": activities,
            "interactions": interactions, "requirements": requirements}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tmdl", type=Path, required=True)
    parser.add_argument("install", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        tool = args.tmdl.resolve(strict=True)
        (tool.parent / "Logs").mkdir(exist_ok=True)
        output = args.output.resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="interactions-", dir=output.parent) as directory:
            report = inventory_interactions(tool, args.install.resolve(strict=True), Path(directory))
        pending = output.with_suffix(".tmp")
        pending.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        pending.replace(output)
    except (IndexError, KeyError, OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"Story interaction inventory failed: {error}\n")
    print(f"Resolved {len(report['interactions'])} original interactions and {len(report['activities'])} shared activities")


if __name__ == "__main__":
    main()
