# OpenBarnyard PSP bootstrap

The generated `EBOOT.PBP` is presented as **OpenBarnyard PSP** and embeds the
native PSP package artwork from `Assets/ICON0.PNG` (144x80) and
`Assets/PIC1.PNG` (480x272). Keep those exact dimensions and the bounded file
budgets enforced by `Tools/test_package_assets.py`; Premake passes both images
directly to `pack-pbp`.

The Premake workspace produces two runnable packages. The diagnostic
`OpenBarnyardPSPBootstrap` validates platform paths and remains a regression
target. The separate `OpenBarnyardPSPGame` links the original
`AApplication`/`ARootTask` chain and now reaches the retail legal/logo
presentation and original localized six-entry main menu. It is not yet a
playable game: the frontend 3D scene, downstream states, movies, collision,
saves and rooms remain incomplete. See
[`Docs/PSP_PORT_HANDOFF.md`](../../Docs/PSP_PORT_HANDOFF.md) for the exact
runtime boundary.

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
`premake5 gmake --target=psp` and builds its `release_psp` configuration.
The diagnostic package is `bin/PSP/Release/EBOOT.PBP`; the original-game
package is `bin/PSP/Release/EBOOT_GAME.PBP`. Use the diagnostic package for
platform smoke tests and stage the original-game package with
`StageOriginalGame.ps1` so that its retail resources are present.

## Background PPSSPP runner

Install the pinned official PPSSPP development build once, then use the
background runner. Emulator binaries and configuration remain ignored under
`Runtime/Tools/PPSSPPDev`:

```powershell
.\Ports\PSP\InstallPPSSPP.ps1
```

The runner prefers that pinned build, records the PID of this port's emulator
process and reuses it instead of opening duplicate instances or taking focus on
every check:

```powershell
.\Ports\PSP\RunPPSSPP.ps1
.\Ports\PSP\RunPPSSPP.ps1 -Action Status
.\Ports\PSP\RunPPSSPP.ps1 -Action Restart -EbootPath .\Runtime\PSP\0a06d6b5\EBOOT.PBP
.\Ports\PSP\RunPPSSPP.ps1 -Action Press -Key Start
.\Ports\PSP\RunPPSSPP.ps1 -Action Press -Key Cross -Repeat 8
.\Ports\PSP\RunPPSSPP.ps1 -Action Stop
```

For the staged original-game runtime use:

    .\Ports\PSP\RunPPSSPP.ps1 -Action Start -OriginalGame
    .\Ports\PSP\RunPPSSPP.ps1 -Action Press -OriginalGame -Key DpadDown
    .\Ports\PSP\RunPPSSPP.ps1 -Action Press -OriginalGame -Key Cross
    .\Ports\PSP\RunPPSSPP.ps1 -Action Stop -OriginalGame

The runner stores its PID under the ignored `Runtime/PPSSPP` directory. If that
disposable file is missing, it adopts an existing process using the exact
bundled PPSSPP executable path instead of starting another one. Start, input and
stop operations remove duplicates belonging to that same runner; PPSSPP
installations at other paths are untouched.
Background input uses PPSSPP's local WebSocket debugger and does not activate
the emulator window or take keyboard and mouse focus from the current
application. The debugger identity and process ID are checked before a command
is sent. Screenshot requests use the emulator window command, temporarily
restoring a minimized D3D11 window without activation because its minimized
swap chain is not capturable. The runner keeps the configured graphics backend
and reports an immediate startup failure instead of launching fallback
instances.

## Original-game retail staging

The complete user-owned PC installation is expected at:

    Game/BarnyardPC/Install/

Convert the retail frontend after downloading a matching CI TMDL host tool:

    .\Ports\PSP\Tools\ConvertRetailFrontend.ps1 -TMDLPath <TMDL.exe>

Then assemble an ignored runtime from the original-game build artifact:

    .\Ports\PSP\StageOriginalGame.ps1 -BuildDirectory <artifact-directory> -SkipConversion

This stages fonts.trb, texsec.trb, locale/control resources and converted PTEX
files under Runtime/OpenBarnyardPSPGame. Rekord_Black_26 is intentionally kept
at 512x512 because the retail glyph table uses coordinates from that atlas.
The original GUI keeps its 800x600 logical root and is scaled to the PSP's
480x272 framebuffer by AGUI2Renderer_PSP.

## Optional Barnyard skin preview

Build the Windows `TMDL` host tool, then convert one user-owned skin model and
one explicitly selected base animation directly into an EBOOT directory:

```powershell
py .\Ports\PSP\Tools\convert_barnyard_skin.py `
  --tmdl <TMDL.exe> `
  --model <model.trb> `
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

Use [`Docs/PSP_ASSET_INTAKE.md`](../../Docs/PSP_ASSET_INTAKE.md) for the local
`Game/BarnyardPC` directory layout and the exact first-asset checklist. The
complete PC install and every selected retail source stay under the already
ignored `Game/` tree; only converters, synthetic fixtures and documentation are
versioned.

Use `Tools/ConvertRetailPreview.ps1` to repeat the manifest-driven retail
conversion and optionally publish it into a local runtime bundle. Confirmed
porting failures and their fixes are tracked in
[`Docs/PSP_PORTING_ISSUES.md`](../../Docs/PSP_PORTING_ISSUES.md).
When a manifest supplies `texture_library` and `texture_name`, the same command
extracts the embedded DDS through TMDL, converts its bounded top mip to PTEX and
binds it into the transactional preview set.

## Optional Barnyard terrain preview

`Tools/ConvertRetailTerrain.ps1` reads the ignored `first_terrain.txt` manifest,
exports one explicitly selected retail world/cell/mesh, resolves its material
to the original texture name and emits `barnyard_terrain.pmes` plus
`barnyard_terrain.ptex`. Use `-RuntimeDirectory <EBOOT-directory>` to publish
the set beside the EBOOT. With `all_meshes=true`, up to four numbered
`barnyard_terrain_N.pmes/.ptex` pairs retain one shared cell transform. The
runtime loads them independently from the animated `barnyard_preview.*` set and
records `terrain=ACTIVE` plus `terrain_meshes=N` in report v8.

`StageOriginalGame.ps1` uses the tracked
`Assets/original_frontend_terrain.txt` recipe to convert the four shipping
`frontend/Barn_L0Mod0` meshes and stage their numbered PMES/PTEX pairs beside
the original-game EBOOT. `-SkipConversion` reuses the ignored
`Game/BarnyardPC/Workspace/FrontendTerrain` output while still validating and
copying the complete eight-file set.

## Bounded world layout

`AWorld_PSP` accepts PWLD v3 layouts containing at most 16 fixed-size model
instances, four fixed model/texture resource-name pairs and eight fixed spatial
cells. The complete file is capped at 928 bytes and is read through a stack
buffer. Every transform, ASCII cache name, cell range and cell sphere is
validated before the layout becomes visible. Instance model
indices address the bounded resource table, which resolves transactionally
through `TRenderResourceCache_PSP`; all referenced models, textures and
materials must exist before rendering. The current bootstrap uses resource zero
and three deterministic instances, rejects cells before testing their instances
against the PSP projection with a fixed visible-index array, and moves the view
on two analogue axes. The generated fixture still needs to be packaged and
loaded through the bounded file path before retail terrain replaces this probe.

Convert a bounded JSON layout offline with:

```sh
python3 Ports/PSP/Tools/convert_world.py world.json world.pwld
```

The source contains `version: 3`, a `resources` array and an `instances` array.
Each resource supplies safe `model` and `texture` cache names of at most 23 ASCII
characters. Each instance supplies three-component `position` and `rotation`, a
scalar `scale`, and `model` indexing the resource array. Optional `cell_size`
controls deterministic X/Z partitioning and is limited to 1–256 units. The
converter mirrors all runtime limits before writing anything.

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
