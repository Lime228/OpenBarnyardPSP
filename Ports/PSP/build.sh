#!/usr/bin/env bash
set -euo pipefail

if ! command -v psp-config >/dev/null 2>&1; then
    echo "error: psp-config was not found; install PSPDEV first" >&2
    exit 1
fi

export PSPDEV="${PSPDEV:-$(psp-config --pspdev-path)}"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

make -C "$script_dir" -j"$(nproc)" "$@"
