#!/usr/bin/env python3

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from analyse_link_audit import analyse, render_markdown


SAMPLE = """
../../Toshi/Source/Render/TRenderInterface.cpp:247:31: error: 'GUID' does not name a type
ld: object.o: undefined reference to `_ZN9ARenderer9ms_oClassE'
ld: object.o: undefined reference to `_ZN9ARenderer9ms_oClassE'
"""


def main() -> int:
    undefined, errors = analyse(SAMPLE)
    assert undefined["_ZN9ARenderer9ms_oClassE"] == 2
    assert sum(errors.values()) == 1

    report = render_markdown(undefined, errors, 20)
    assert "Compile errors: 1 (1 unique)" in report
    assert "Undefined references: 2 (1 unique)" in report
    assert "ARenderer::ms_oClass" in report or "_ZN9ARenderer9ms_oClassE" in report
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
