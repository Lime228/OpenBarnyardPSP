# OpenBarnyard PSP bootstrap

This target is the first executable milestone for the PSP port. It deliberately
does not compile the game yet. It validates the PSPDEV toolchain, 32-bit C++ ABI,
controller input, double buffering, and a basic GU draw call before Toshi is
connected to the platform layer.

## Build on Windows through WSL

Install WSL and PSPDEV using the official PSPDEV Windows guide. In the Ubuntu
shell, make sure these commands succeed:

```sh
psp-config --pspdev-path
psp-g++ --version
premake5 --version
```

Then build from the repository, either from PowerShell:

```powershell
.\Ports\PSP\Build.ps1
```

or directly in the Ubuntu shell:

```sh
./Ports/PSP/build.sh
```

The build script generates the PSP project with
`premake5 gmake --target=psp` and builds its `release_psp` configuration. The
result is `bin/PSP/Release/EBOOT.PBP`. Copy it to a folder such as
`PSP/GAME/OpenBarnyardProbe/EBOOT.PBP` on a homebrew-capable PSP, or open it in
PPSSPP. The analog stick moves the triangle; Cross changes the background. A
red background means that the embedded Toshi ABI/endianness smoke test failed.

## Background PPSSPP runner

For local PPSSPP checks on Windows, use the background runner. It records the
PID of this port's emulator process and reuses it instead of opening duplicate
instances or taking focus on every check:

```powershell
.\Ports\PSP\RunPPSSPP.ps1
.\Ports\PSP\RunPPSSPP.ps1 -Action Status
.\Ports\PSP\RunPPSSPP.ps1 -Action Restart -EbootPath .\Runtime\PSP\0a06d6b5\EBOOT.PBP
.\Ports\PSP\RunPPSSPP.ps1 -Action Press -Key Cross -Repeat 8
.\Ports\PSP\RunPPSSPP.ps1 -Action Stop
```

The runner only stops the PPSSPP process whose PID it created and stored under
the ignored `Runtime/PPSSPP` directory. Other emulator sessions are untouched.
Background input uses window messages and does not activate the emulator window
or take keyboard and mouse focus from the current application. The runner keeps
the user's configured PPSSPP graphics backend and reports an immediate startup
failure instead of launching fallback instances.

## Optional Barnyard skin preview

Build the Windows `TMDL` host tool, then convert one user-owned skin model and
one explicitly selected base animation directly into an EBOOT directory:

```powershell
py .\Ports\PSP\Tools\convert_barnyard_skin.py `
  --tmdl <TMDL.exe> `
  --model <model.tmd> `
  --keylib <library.tkl> `
  --sequence <base-sequence> `
  --material <material-name> `
  --texture <texture.ppm-or-ptex> `
  --sample-rate 30 `
  --output <EBOOT-directory> `
  --runtime-preview
```

The command transactionally publishes `barnyard_preview.pmes`, `.pskl`, `.panm`,
optional `.ptex`, and a required `.pset` manifest only after all selected
conversions succeed. The manifest binds each selected file to its exact size
and CRC32; the EBOOT rejects incomplete or mixed preview generations before
allocating model resources. Existing files are restored if a replacement fails
partway through. When
`--runtime-preview` is used without `--texture`, a stale
`barnyard_preview.ptex` is removed in the same transaction so the runtime uses
its probe texture fallback instead of pairing a new model with an old texture.
`--material` is optional for a single-material LOD0 and required when the model
contains several materials; the JSON summary reports the canonical source
material name. PMES v1 intentionally previews only one material at a time. The
same summary reports bind-pose bounds plus the centre and uniform scale that the
EBOOT will use, so unit or pivot problems can be caught before PPSSPP. Original
game data must remain outside the repository.

## Bounded world layout

`AWorld_PSP` accepts PWLD v1 layouts containing at most 16 fixed-size model
instances. The complete file is capped at 528 bytes and is read through a stack
buffer. Every transform is checked for finite, bounded position and scale before
the layout becomes visible. PWLD v1 addresses four fixed model-resource slots;
all referenced models, textures and materials must be bound before rendering.
The current bootstrap uses slot zero and three deterministic instances, culls
them against the PSP projection with a fixed visible-index array, and moves the
view on two analogue axes. Retail terrain still needs offline resource-name
resolution and bounded cells before it can replace this probe.

Convert a bounded JSON layout offline with:

```sh
python3 Ports/PSP/Tools/convert_world.py world.json world.pwld
```

The source contains `version: 1` and an `instances` array. Each instance supplies
three-component `position` and `rotation`, a scalar `scale`, and `model` in the
range 0–3. The converter mirrors all runtime limits before writing anything.

## Audio conversion

Resident GUI and gameplay effects use bounded PPCM files. The source must be an
uncompressed 44.1 kHz stereo PCM16 WAV containing no more than 1,024 sample
frames:

```sh
python3 Ports/PSP/Tools/convert_audio.py input.wav output.ppcm --format ppcm
```

Longer streamed audio uses PADP and requires 1,024-frame-aligned loop points:

```sh
python3 Ports/PSP/Tools/convert_audio.py input.wav output.padp \
  --format padp --loop-start 1024 --loop-end 3072
```

The converter rejects loop metadata for resident sounds and does not resample,
downmix, or retain source data at runtime. Original game audio must be supplied
by the user and remains outside the repository.

## Why this target is separate

The current `new-modcore` build scripts and executable are Windows-only. Keeping
the probe independent gives us a continuously buildable target while platform
services are ported one by one. Game data is intentionally not copied or bundled.
