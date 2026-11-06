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

The current end-to-end bridge can consume one model TRB, one matching `.tkl`, one
named animation sequence, one selected material and one preview texture. It does
not yet decode the retail texture container in the PSP EBOOT. If the original
texture is not already PPM, preserve the original under `Install/` and put only
an exported P3 PPM copy under `Selected/Textures/`.

Record the first selection in
`Game/BarnyardPC/Manifests/first_skin.txt` with these fields:

```text
model=<relative path under Selected/Models>
keylib=<relative path under Selected/Animations>
sequence=<exact animation sequence name>
material=<exact material name, if the model has more than one>
texture=<relative path under Selected/Textures>
source_room=<optional relative path under Selected/Worlds>
```

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

