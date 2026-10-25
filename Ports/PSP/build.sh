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

export PSPDEV="${PSPDEV:-$(psp-config --pspdev-path)}"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repository_dir="$(cd -- "$script_dir/../.." && pwd)"
build_config="${PSP_BUILD_CONFIG:-release_psp}"

premake5 --file="$repository_dir/premake5.lua" gmake --target=psp
make -C "$repository_dir/Build/PSP" config="$build_config" -j"$(nproc)" "$@"
