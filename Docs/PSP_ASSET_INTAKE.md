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
| `Tools/convert_retail_movies.py` | Converts all installed Bink movies to bounded PMVF, verifies every JPEG and records stream sizes and timing adjustments. |
| `Tools/convert_retail_model_animations.py` | Resolves installed key libraries by their original internal names and key ranges, then bakes supported model sequences to bounded PANM. |
| `Tools/inventory_retail_models.py` | Inventories all model TRB and TKL containers and attempts the skin exporter for every TMD geometry entry. |
| `Tools/convert_retail_model_geometry.py` | Bakes exported skins to one PSKL and one PMES per material, preserving original texture names in the report. |
| `Tools/ConvertRetailFrontend.ps1` | Extracts the shipping frontend logo, fonts, panels, startup pictures and the original options arrows and stars. It can publish the converted PTEX files beside an EBOOT. |
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

The original Audio and Controls dialogs add two 64×64 textures from
`Data/Matlibs/gui_HUD.ttl`: `HUD\Left_Arrow.tga` and
`HUD\Statue_GoldStar.tga`. The `left_arrow2` section reverses the source
rectangle on the same arrow texture. The frontend conversion produced the
same PTEX bytes for both textures as the full GUI batch. The staging script
copies both into the PSP game runtime alongside the existing menu assets.

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
files against the encoding report.

Build the resident frontend bank from the mapped original `UI.fsb` and the
decoded WAV cache:

```powershell
python Ports/PSP/Tools/convert_frontend_audio.py Game/BarnyardPC/Workspace/AudioBanks Game/BarnyardPC/Workspace/AudioBanks/FrontendMenu
```

Seven menu cues share six original mono samples. The converter resamples them
to 44.1 kHz and writes 392,784 bytes of PPCM plus a 708-byte PBNK manifest.
`StageOriginalGame.ps1` copies the bank to `Data/Sound/Menu`. With
`-SkipConversion` it uses the existing cache.

To convert movies, pass the paths to a [Windows FFmpeg build](https://ffmpeg.org/download.html#build-windows):

~~~powershell
python Ports/PSP/Tools/convert_retail_movies.py --ffmpeg <ffmpeg.exe> --ffprobe <ffprobe.exe> Game/BarnyardPC/Install Game/BarnyardPC/Workspace/Movies
~~~

PMVF v2 uses a 48-byte header. The four-byte `PMVF` signature is followed by
little-endian 32-bit words for version, header size, width, height, frame rate
numerator and denominator, frame count, maximum JPEG bytes, audio track count,
audio sample rate and maximum bytes per audio block per track. Each record
has a 32-bit JPEG length, a self-contained baseline JPEG and a block of stereo
PCM16 for each original audio track. At frame boundary `n`, the cumulative
sample frames per track are `floor(n * 44100 * denominator / numerator)`.
Adjacent boundaries determine the size of that record's audio blocks.

The default rate ceiling is 30 FPS. The encoder preserves the exact source
rate and checks that the frame count matches the source for movies under that
ceiling. `--max-fps 15` reproduces the lower rate using the v2 container. V1
caches are accepted by the host verifier. The PSP streaming decoder uses v2.

The full batch kept all 7,274 original frames across 15 movies at about
29.97 FPS. It produced 100,185,130 bytes, up from 67,271,066 bytes at 15 FPS.
The largest JPEG is 23,203 bytes. The busiest 30 consecutive records total
717,196 bytes. The encoder decodes the full MJPEG streams on the host and
rereads each final container before publishing it. The batch includes the
original startup inputs `NickMovs`, `THQ`, `BTE` and `Attract`. This
installation only supplies the default `Attract`, so the language-specific
states fall back to `Attract.pmvf` when their localized file is missing.

`StageOriginalGame.ps1` stages all converted PMVF v2 files in `Data/Movies`
inside the game runtime. It looks for the converted cache in `Workspace/Movies`
by default. Use `-MoviesDirectory` when the cache is elsewhere. The original
startup state chain controls movie order, skipping and transitions. The player
streams the original first stereo track to a PSP audio channel and uses its
played sample count to select the frame. Movies without audio use a monotonic
clock. At 29.97 FPS the player can read four compressed frames ahead while
retaining one decoded image. The PCM queue records an underrun when it had to
wait for input and the PSP channel was empty before the next submission. The
game writes frame counts, audio samples and underruns to
`openbarnyard_psp_movies.log` next to the EBOOT.

Validate the original game end to end with a staged EBOOT:

~~~powershell
.\Ports\PSP\Tools\TestOriginalStartupMovies.ps1 -RuntimeDirectory Runtime/OpenBarnyardPSPGame -RequireSteadyAudio
~~~

The script verifies the four movies in order, matches the PCM duration to each
PMVF header and waits for the visible main menu. `-SkipAttract` also verifies
that Cross stops the last movie and transitions to the menu. Use
`-CaptureAttract` to inspect the framebuffer in a separate run from timing
checks, because the PPSSPP debugger briefly pauses emulation during capture.

Original-game CI run `45224435038` was tested in one hidden PPSSPP instance.
`NickMovs`, `THQ`, `BTE` and `Attract` reached their final frames and the
original main menu opened. The player read 3,815 video records and presented
3,814 distinct frames. It played all 5,613,649 source stereo sample frames
and recorded zero PCM underruns. The largest movie buffer footprint was
662,784 bytes. An earlier render capture matched frame 98 of `Attract` at
3.27 seconds with the same orientation as the source. In a separate run,
Cross stopped `Attract` after frame 118 and the same main menu opened.

Run the optional bootstrap decoder probe in a single hidden PPSSPP instance:

~~~powershell
.\Ports\PSP\Tools\TestMovieProbe.ps1 -BootstrapEboot bin/PSP/Release/EBOOT.PBP -MoviePath Game/BarnyardPC/Workspace/Movies/Data/Movies/NickMovs.pmvf
~~~

The probe reads one compressed frame at a time into a buffer sized from the
movie header and keeps one 480×272 RGBA buffer. It logs the exact rate,
decoded frame count, average and worst frame time and how many frames exceed
the interval. It skips packed PCM while stepping through the records. The
measurements capture decoding in the selected environment, without texture
submission or audible movie playback.

PSP CI run `45180653929` built the decoder. In PPSSPP, `NickMovs` decoded
151/151 frames with a maximum read-and-decode time of 11,097 µs. `Attract`
decoded 2,855/2,855 frames with a maximum of 11,169 µs and a 545,443-byte
JPEG and RGBA buffer allocation. Both runs stayed within their 33,366 µs
frame budget. A copy of `NickMovs` with its final byte removed was rejected
at frame record 151. These are emulator measurements.

For a hardware measurement, copy the same bootstrap files to PSP and put a
local PMVF beside `EBOOT.PBP` under the name `probe_movie.pmvf`. The bootstrap
writes `probe_movie.log` in the same folder.

Inventory the model and animation files and bake the supported skin geometry:

~~~powershell
python Ports/PSP/Tools/inventory_retail_models.py --tmdl <TMDL.exe> --dump-skins Game/BarnyardPC/Install Game/BarnyardPC/Workspace/Models
python Ports/PSP/Tools/convert_retail_model_geometry.py Game/BarnyardPC/Workspace/Models/models.inventory.json Game/BarnyardPC/Workspace/Models/Geometry
~~~

The pass read all 1,172 model TRBs and 247 animation key libraries. TMDL
exported 620 LOD0 skin dumps, and all 620 produced a total of 909 PMES
material meshes and 620 skeletons. The output reports list entries without
a compatible skin dump, including alternate geometry, skeleton-only data
and files rejected by the skin exporter. Additional LODs and runtime texture
staging remain separate.

Use a TMDL build with `-psp-animation-info` to convert model sequences:

~~~powershell
python Ports/PSP/Tools/convert_retail_model_animations.py --tmdl <TMDL.exe> Game/BarnyardPC/Install Game/BarnyardPC/Workspace/Models/models.inventory.json Game/BarnyardPC/Workspace/Models/Animations
~~~

The script indexes every installed TKL, including particle key libraries.
It matches the internal library ID and translation, quaternion and scale
ranges from each retail skeleton. The matching same-stem library takes
precedence, and equivalent copies are recognised by SHA-256. Ambiguous
matches with distinct contents are reported for further investigation.
PANM filenames use sequence order. `model_animations.json` retains the
original clip name, loop or clamp mode, retail duration, sampled duration,
source model and library paths. An upper rate of 30 Hz and a maximum of
64 samples keep a clip at or below 65,568 bytes. Overlay modes and clips beyond
the PANM v1 limits are listed in the report. Runtime selection and layering
of the original clips still need to be connected.

The full pass read 261 TKLs and 1,172 model headers, resolving the libraries
for all 496 models that define clips. It converted 3,511 of 4,028 sequences
across 493 models. The verified PANM output totals 81,373,248 bytes and the
largest clip is 64,544 bytes. The remaining entries are 499 overlay clips,
12 sequences on two skeletons exceeding 64 bones, and six `beeSwarm2` clips
whose rig contains duplicate bone names. The script writes the full report
before returning a nonzero exit code for exporter rejections.

## Generated data

Host-tool dumps and converted PSP files belong in the ignored workspace:

```text
Game/BarnyardPC/Workspace/Dumps/
Game/BarnyardPC/Workspace/Inventory/
Game/BarnyardPC/Workspace/TextureLibraries/
Game/BarnyardPC/Workspace/GUITextures/
Game/BarnyardPC/Workspace/AudioBanks/
Game/BarnyardPC/Workspace/Movies/
Game/BarnyardPC/Workspace/Models/
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

