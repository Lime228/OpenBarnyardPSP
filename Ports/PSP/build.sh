#!/usr/bin/env bash
set -euo pipefail

if ! command -v psp-config >/dev/null 2>&1; then
    echo "error: psp-config was not found; install PSPDEV first" >&2
    exit 1
fi

if ! command -v premake5 >/dev/null 2>&1; then
    echo "error: premake5 was not found; install Premake 5 first" >&2
    exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "error: python3 was not found; it is required for PSP asset conversion" >&2
    exit 1
fi

export PSPDEV="${PSPDEV:-$(psp-config --pspdev-path)}"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repository_dir="$(cd -- "$script_dir/../.." && pwd)"
build_config="${PSP_BUILD_CONFIG:-release_psp}"

python3 "$script_dir/Tools/generate_bootstrap_font.py" \
    "$script_dir/Assets/bootstrap_font.ppm" \
    "$script_dir/Assets/bootstrap_font.pfnt"
python3 "$script_dir/Tools/convert_texture.py" --alpha-mask \
    "$script_dir/Assets/bootstrap_font.ppm" \
    "$script_dir/Assets/bootstrap_font.ptex"
python3 "$script_dir/Tools/generate_bootstrap_locale.py" \
    "$script_dir/Assets/bootstrap_locale.ploc"

premake5 --file="$repository_dir/premake5.lua" gmake --target=psp
make -C "$repository_dir/Build/PSP" config="$build_config" -j"$(nproc)" "$@"
