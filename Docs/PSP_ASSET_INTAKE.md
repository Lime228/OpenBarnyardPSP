# Barnyard PC asset intake for the PSP port

Original game data must stay under `Game/BarnyardPC`. The repository already
ignores the complete `Game/` directory, so none of these files may be committed
or uploaded to GitHub.

## Preferred input

Copy the complete, unmodified PC installation into:

```text
Game/BarnyardPC/Install/
```

Keep the original directory structure intact. This gives the porting tools the
best chance of resolving container, model, animation, texture, audio and locale
references without guessing which neighbouring files are required.

## First playable-content slice

For the first real PSP preview, also place a small, explicitly selected set in:

```text
Game/BarnyardPC/Selected/Models/       one character or prop model `.trb`
Game/BarnyardPC/Selected/Animations/   the matching `.tkl`
Game/BarnyardPC/Selected/Textures/     an exported ASCII P3 `.ppm` or ready `.ptex`
Game/BarnyardPC/Selected/Worlds/       the source room/world containers or dumps
Game/BarnyardPC/Selected/Audio/        source music/SFX or lossless extracted WAVs
Game/BarnyardPC/Manifests/             selection notes; never binary game data
Game/BarnyardPC/Tools/                 a working Windows `TMDL.exe` host exporter
```

The current end-to-end bridges can consume one model TRB with a matching `.tkl`
and one selected terrain world/cell/mesh. Retail TTL/DDS decoding stays offline:
the host tool extracts a named texture and the bounded converter emits PSP PTEX,
so the relocatable desktop parser never enters the EBOOT.

Record the first selection in
`Game/BarnyardPC/Manifests/first_skin.txt` with these fields:

```text
model=<path relative to Game/BarnyardPC, under Install/ or Selected/Models>
keylib=<path relative to Game/BarnyardPC, under Install/ or Selected/Animations>
sequence=<exact animation sequence name>
sample_rate=<bounded sampling rate, normally 20 or 30>
material=<exact material name, if the model has more than one>
texture=<relative path under Selected/Textures>
texture_library=<retail TTL path relative to Game/BarnyardPC>
texture_name=<exact texture name stored inside the TTL>
source_room=<optional relative path under Selected/Worlds>
```

Record the first terrain selection in
`Game/BarnyardPC/Manifests/first_terrain.txt`:

```text
terrain=Install\Data\Terrain\ss01\xroad_L0Mod2.trb
world=0
cell=0
mesh=0
all_meshes=true
texture_libraries=Install\Data\Terrain\ss01\xroad_L0Mat.ttl;Install\Data\Terrain\ss01\ss01_Common.ttl
```

## Repeatable conversion tools

| Tool | Repeatable bounded operation |
| --- | --- |
| `Tools/InventoryRetailInstall.ps1` | Inventories every retail file before a conversion pass and writes a deterministic, relative-path-only JSON report under the ignored workspace. |
| `Tools/InventoryRetailTextureLibraries.ps1` | Indexes standalone TTL files and `*_TTL` symbols in `Data/Assets` packs, recording their exact texture names and input failures. |
| `Tools/ConvertRetailGUITextures.ps1` | Converts every resolvable source variant referenced by `texsec.trb` to PTEX and reports missing names and unsupported inputs. |
| `Tools/inventory_retail_fsb.py` | Indexes all FSB3.1 audio banks and records original track indices, channel counts, rates and loop fields. |
| `Tools/decode_retail_fsb.py` | Uses vgmstream CLI to decode the indexed tracks once at their native rates and channel counts, then verifies each PCM WAV header. |
| `Tools/encode_retail_fsb.py` | Reads the complete WAV cache, emits 44.1 kHz PSP streams or resident cues, and reports the channel and segment mapping for every track. |
| `Tools/map_retail_sound_cues.py` | Exports ordered TRB properties with TMDL, resolves cue and multiplex references to the encoded cache, and verifies the linked files. |
| `Tools/ConvertRetailFrontend.ps1` | Extracts the shipping `Dialogs\\barnyard_logo.tga`, reduces it to a PSP-safe 256x64 PTEX and can publish it beside an EBOOT without copying retail containers. |
| `Tools/ConvertRetailPreview.ps1` | Reads the manifest, selects the newest local CI-built TMDL, runs the complete skin pipeline and can publish into an EBOOT directory. |
| `Tools/TMDL -psp-dump` | Relocates one retail model TRB on the host and writes the stable JSON skin/skeleton contract. |
| `convert_tmd_dump.py` | Triangulates strips, resolves palettes and reduces four retail influences to the two strongest combined bone weights required by the PSP CPU skinning path. |
| `Tools/TMDL -psp-animation` | Samples one explicitly named retail sequence without adding the relocatable parser to the EBOOT; `-keylib-symbol` selects a bounded key library embedded in an asset-pack TRB. |
| `Tools/TMDL -psp-texture` | Relocates one retail TTL on the host and extracts an explicitly named embedded DDS. |
| `Tools/TMDL -psp-terrain` / `-psp-terrain-cell` | Relocates one terrain TRB, selects one mesh or exports a bounded complete cell of up to 128 meshes and resolves every material to its exact retail texture name. |
| `Tools/TMDL -psp-material-properties` | Exports the retail glow, double-sided, clamp, render-order and UV-animation lists as a bounded host JSON contract. |
| `Tools/TMDL -psp-locator` | Extracts one explicitly named locator from a retail terrain skeleton without adding the relocatable TRB parser to the EBOOT. |
| `convert_terrain_dump.py` / `convert_terrain_cell.py` | Triangulates bounded terrain strips, preserves vertex colours/UVs and emits static textured PMES files with shared cell bounds plus an exact-count PCFG. |
| `convert_placed_terrain_cell.py` | Bakes the original terrain-locator/global-axis matrix into one bounded world-model cell for a PSP static draw path. |
| `Tools/ConvertRetailTerrain.ps1` | Repeats terrain export, searches declared TTLs, deduplicates repeated source textures, converts PMES/PTEX sets and publishes only the complete generated set into a runtime directory. |
| `Tools/ConvertRetailFrontendDetail.ps1` | Converts every mesh in the original `Barn_L0Mod1` cell, merges only identical material/texture bindings, preserves original coordinates/addressing and emits the bounded 36-model/34-texture detail pack. |
| `Tools/ConvertRetailFrontendGrass.ps1` | Reuses the complete-cell converter for all six original `Barn_L0Mod2` grass meshes and emits one draw group because every source mesh has the same `grassmain` material and texture. |
| `Tools/ConvertRetailFrontendCamera.ps1` | Samples the shipping `camera_loop` hierarchy and reduces it to the bounded PCAM runtime contract. |
| `Tools/ConvertRetailBarnSign.ps1` | Converts the shipping two-mesh barn-sign `WorldDatabase`, its original `loc_barnsign` placement and both declared textures. |
| `convert_animation.py` | Quantizes the sampled clip into a maximum of 64 bounded PANM frames. |
| `convert_dds.py` | Decodes bounded DXT1/DXT3/DXT5 or explicitly masked 16/32-bit top mips, optionally applies power-of-two 2x box reduction, and emits swizzled RGBA8888 PTEX data. |
| `convert_texture.py` | Converts P3 PPM into a power-of-two, optionally swizzled PTEX payload. |
| `convert_world.py` | Builds bounded PWLD instances/resources and deterministic X/Z cells. |

Keep orchestration in the PowerShell entry point and format transformations in
the focused Python/C++ tools. Add tests beside a converter whenever a new
reduction rule or retail edge case becomes deterministic.

Every new content category follows a batch-first rule. Inventory the complete
category, convert every discovered source into the ignored workspace, write a
success/failure report, and only then integrate runtime subsets or fix reported
exceptions. A targeted conversion must reuse the successful batch cache rather
than silently re-exporting one hand-picked file. `StageOriginalGame.ps1` runs
the complete install inventory before any converter so each pass has a stable
source snapshot.

The current complete retail install inventory contains 5,616 files. The major
offline-conversion inputs are 3,874 TRB containers, 1,138 TTL texture libraries,
261 TKL animation libraries, 76 FSB audio banks, 47 MP3 streams and 15 Bink
movies. These totals describe the source installation, not content that can be
made resident at once on PSP. Runtime selection remains lifetime- and
memory-budgeted after each whole category has been processed.

Run the complete GUI texture batch locally with a current TMDL build:

```powershell
.\Ports\PSP\Tools\ConvertRetailGUITextures.ps1 -AllowFailures
```

The recipe saves the asset-pack and TTL index in `Workspace/TextureLibraries`
and writes the batch report to `Workspace/GUITextures/gui_textures.batch.json`.
Use `-ReuseInventories` on the same installation to reuse unchanged index dumps.
The cache keys by source file, symbol and full texture name, preserving
localized variants and names with a shared basename. The default 512-pixel
limit preserves native atlas coordinates and extends edges when a power-of-two
size is required. Read the report before selecting any files for staging.
Omit `-AllowFailures` to make missing sources or unsupported libraries fail
the command after it writes the report.

The full GUI pass from PSP CI run `45169619708` converted all 1,155 source
variants found for 195 texture names. The 999 sections reference 287 unique
names. The report lists the 92 names whose sources remain unresolved and the
two alternate-format TTL libraries that TMDL rejected.

For audio, use Python 3.12 or later and the
[vgmstream CLI release](https://github.com/vgmstream/vgmstream/releases/tag/r2168).

```powershell
python Ports/PSP/Tools/inventory_retail_fsb.py Game/BarnyardPC/Install Game/BarnyardPC/Workspace/AudioBanks/fsb.inventory.json
python Ports/PSP/Tools/decode_retail_fsb.py --decoder <vgmstream-cli.exe> Game/BarnyardPC/Install Game/BarnyardPC/Workspace/AudioBanks/fsb.inventory.json Game/BarnyardPC/Workspace/AudioBanks/Decoded
```

All 76 banks and 3,071 tracks decoded successfully in the full local pass.
The 1.99 GB WAV cache preserves channel count, sample rate and frame count.
The FSB index keeps the original loop fields for PSP encoding. Keep each bank
path and track index intact when mapping game cues. The 47 MP3s in this
installation belong to the PC manual.

Run the PSP encoding pass with Python 3.12:

```powershell
python Ports/PSP/Tools/encode_retail_fsb.py Game/BarnyardPC/Workspace/AudioBanks/fsb.inventory.json Game/BarnyardPC/Workspace/AudioBanks/Decoded Game/BarnyardPC/Workspace/AudioBanks/Encoded
```

The local pass produced 3,105 files for 3,071 tracks (863,498,496 bytes).
All output headers use 44.1 kHz stereo. Mono is duplicated to left and right,
and four-channel tracks keep both pairs as separate files. Tracks longer than
the current 300-second stream limit are split at aligned block boundaries.
`fsb.encode.json` records segment order, source channel assignments, original
loop fields, exact resampled loop positions and block-aligned PADP loop
positions. Use the source mode and game cue settings to decide when to loop.
The grouped output still needs paired playback for the 25 four-channel tracks
and ordered playback for the three split tracks.

Build the offline cue map with the TMDL host-tools artifact that supports
`-psp-properties`:

```powershell
python Ports/PSP/Tools/map_retail_sound_cues.py --tmdl <TMDL.exe> Game/BarnyardPC/Install Game/BarnyardPC/Workspace/AudioBanks/Encoded/fsb.encode.json Game/BarnyardPC/Workspace/AudioBanks/Cues
```

`Cues/sound_cues.json` maps all 722 cues from 36 sound bank files through 46
wave-bank definitions. It retains the order of the 2,065 sample references
and their original cue settings. It also maps all 17 multiplex streams and
their channel routing. The run verified all 1,466 distinct referenced cache
files against the encoding report. This map is input for future playback work.

## Generated data

Host-tool dumps and converted PSP files belong in the ignored workspace:

```text
Game/BarnyardPC/Workspace/Dumps/
Game/BarnyardPC/Workspace/Inventory/
Game/BarnyardPC/Workspace/TextureLibraries/
Game/BarnyardPC/Workspace/GUITextures/
Game/BarnyardPC/Workspace/AudioBanks/
Game/BarnyardPC/Workspace/Converted/
Game/BarnyardPC/Workspace/Frontend/
Game/BarnyardPC/Workspace/FrontendTerrain/
Game/BarnyardPC/Workspace/FrontendTerrainPack/
Game/BarnyardPC/Workspace/FrontendTerrainGrass/
Game/BarnyardPC/Workspace/FrontendCamera/
Game/BarnyardPC/Workspace/FrontendBarnSign/
```

Do not put generated PMES, PSKL, PANM, PTEX or PWLD beside the originals. A
successful bounded selection can later be copied into a local runtime bundle for
PPSSPP or hardware validation.

## Minimum handoff checklist

- Complete PC installation copied to `Game/BarnyardPC/Install/`.
- Working host `TMDL.exe` copied to `Game/BarnyardPC/Tools/`; the PSP fork does
  not add or require a Windows game build.
- One relatively small animated model TRB selected; avoid the largest room first.
- Matching `.tkl` and exact base/idle sequence name supplied.
- Material name supplied when LOD0 contains multiple materials.
- Matching texture exported as ASCII P3 PPM, preferably power-of-two and no
  larger than 512x512; preserve the untouched original too.
- If available, include the room/container that references the selected model.
- Tell Codex when the copy is complete; the next step is a read-only inventory,
  then a bounded conversion into `Workspace/Converted/`.

After the manifest is filled, the repeatable Windows entry point is:

```powershell
.\Ports\PSP\Tools\ConvertRetailPreview.ps1 -KeepIntermediate
```

Pass `-RuntimeDirectory <EBOOT-directory>` to publish the generated
`barnyard_preview.*` set into a local PSP runtime bundle. The script keeps all
retail input, dumps and converted output below the ignored `Game/BarnyardPC`
tree.

The matching terrain command is:

```powershell
.\Ports\PSP\Tools\ConvertRetailTerrain.ps1 -KeepIntermediate
```

Pass the same `-RuntimeDirectory <EBOOT-directory>` option to publish
`barnyard_terrain.pmes` and `barnyard_terrain.ptex`. If `texture_libraries` is
omitted, the script tries the model-specific `*Mat.ttl` and local
`*_Common.ttl`; explicit manifest order is preferred for repeatability.

