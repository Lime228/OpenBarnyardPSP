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
| `Tools/ConvertRetailFrontend.ps1` | Extracts the shipping `Dialogs\\barnyard_logo.tga`, reduces it to a PSP-safe 256x64 PTEX and can publish it beside an EBOOT without copying retail containers. |
| `Tools/ConvertRetailPreview.ps1` | Reads the manifest, selects the newest local CI-built TMDL, runs the complete skin pipeline and can publish into an EBOOT directory. |
| `Tools/TMDL -psp-dump` | Relocates one retail model TRB on the host and writes the stable JSON skin/skeleton contract. |
| `convert_tmd_dump.py` | Triangulates strips, resolves palettes and reduces four retail influences to the two strongest combined bone weights required by the PSP CPU skinning path. |
| `Tools/TMDL -psp-animation` | Samples one explicitly named retail TKL sequence without adding the relocatable parser to the EBOOT. |
| `Tools/TMDL -psp-texture` | Relocates one retail TTL on the host and extracts an explicitly named embedded DDS. |
| `Tools/TMDL -psp-terrain` / `-psp-terrain-cell` | Relocates one terrain TRB, selects one mesh or up to four meshes in a cell and resolves every material to its exact retail texture name. |
| `convert_terrain_dump.py` / `convert_terrain_cell.py` | Triangulates bounded terrain strips, preserves vertex colours/UVs and emits static textured PMES files with shared cell bounds. |
| `Tools/ConvertRetailTerrain.ps1` | Repeats terrain export, searches declared TTLs, deduplicates repeated source textures, converts PMES/PTEX sets and publishes only the complete generated set into a runtime directory. |
| `convert_animation.py` | Quantizes the sampled clip into a maximum of 64 bounded PANM frames. |
| `convert_dds.py` | Decodes bounded DXT1/DXT3/DXT5 or explicitly masked 16/32-bit top mips, optionally applies power-of-two 2x box reduction, and emits swizzled RGBA8888 PTEX data. |
| `convert_texture.py` | Converts P3 PPM into a power-of-two, optionally swizzled PTEX payload. |
| `convert_world.py` | Builds bounded PWLD instances/resources and deterministic X/Z cells. |

Keep orchestration in the PowerShell entry point and format transformations in
the focused Python/C++ tools. Add tests beside a converter whenever a new
reduction rule or retail edge case becomes deterministic.

## Generated data

Host-tool dumps and converted PSP files belong in the ignored workspace:

```text
Game/BarnyardPC/Workspace/Dumps/
Game/BarnyardPC/Workspace/Converted/
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

