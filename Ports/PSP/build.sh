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
python3 "$script_dir/Tools/generate_bootstrap_audio.py" \
    "$script_dir/Assets/bootstrap_audio.ppcm"
python3 "$script_dir/Tools/generate_bootstrap_audio.py" \
    "$script_dir/Assets/ui_menuclick.ppcm" \
    --sample-frames 256 --amplitude 9000 --half-period-frames 8
python3 "$script_dir/Tools/generate_bootstrap_audio.py" \
    "$script_dir/Assets/ui_menuback.ppcm" \
    --sample-frames 384 --amplitude 7000 --half-period-frames 16
python3 "$script_dir/Tools/generate_bootstrap_audio.py" \
    "$script_dir/Assets/ui_menuok.ppcm" \
    --sample-frames 512 --amplitude 8000 --half-period-frames 32
python3 "$script_dir/Tools/generate_wave_bank.py" \
    "$script_dir/Assets/bootstrap_audio.pbnk" \
    --wave bootstrap=local:bootstrap_audio.ppcm \
    --wave ui_menuclick=local:ui_menuclick.ppcm \
    --wave ui_menuback=local:ui_menuback.ppcm \
    --wave ui_menuok=local:ui_menuok.ppcm \
    --stream bootstrap_stream=local:bootstrap_stream.padp

premake5 --file="$repository_dir/premake5.lua" gmake --target=psp

build_targets=( OpenBarnyardPSPBootstrap OpenBarnyardPSPGameSlice )
if [[ "${PSP_BUILD_ORIGINAL:-0}" == "1" ]]; then
    build_targets+=( OpenBarnyardPSPGame )
fi

make -C "$repository_dir/Build/PSP" \
    config="$build_config" \
    -j"$(nproc)" \
    "${build_targets[@]}" \
    "$@"
