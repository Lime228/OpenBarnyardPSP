# OpenBarnyard PSP port handoff

Updated: 2026-11-22

Repository: https://github.com/Lime228/OpenBarnyardPSP.git

Working branch: psp-port

Upstream source branch: InfiniteC0re/OpenBarnyard:new-modcore

## Product goal and honest runtime boundary

The target is the original Barnyard game on PSP: original startup flow, assets,
menus, game states, rooms and gameplay, with platform changes only where PSP
hardware or APIs require them. Synthetic bootstrap screens are regression tools,
not a substitute for the game.

There are now two runnable PSP packages:

- OpenBarnyardPSPBootstrap is the diagnostic regression EBOOT.
- OpenBarnyardPSPGame is the original-game EBOOT. It starts the original
  AApplication/ARootTask chain and reaches the original six-entry
  AFrontEndMainMenuState2 presentation with retail GUI sections, font atlas and
  localized strings.

The original-game EBOOT has been visually confirmed in PPSSPP through:

1. the retail legal screen;
2. the retail Bink/logo screen;
3. the original localized six-entry main menu.

The same original menu is runtime-confirmed with D-pad focus movement and Cross
activation through the retail `DialogInputMap`.

The original frontend now also runs a measured 3D slice: all eleven meshes from
`Terrain/frontend/Barn_L0Mod0.trb`, all 94 meshes from `Barn_L0Mod1.trb`, all
six grass meshes from `Barn_L0Mod2.trb`, the shipping `camera_loop` sampled
from `EnvFrontEnd_Cam.trb`, and the two-mesh `barnsign` object placed from the
retail `loc_barnsign` transform. The three additive `Barn_L0Mod3` meshes are
also resident with their original barn/cone night-light profiles. The complete
VIS-selected high-detail `Barnnorth_L0Mod0` through `Barnnorth_L0Mod3` section
adds another 141 source meshes as 55 exact material/texture groups backed by 54
textures. The original farmhouse, neighbour, lodge, beady, barn, cone and
night-light profiles retain their retail time windows. The same initial retail
VIS row now supplies complete low-detail BGbarneast, BGbarnwest, Hillbase,
Hilltop and Meadowsouth sections. In total, all 448 terrain source meshes are
resident as 149 bounded PSP models with 144 texture resources. Batching occurs
only where the original material and texture are identical; no original
geometry or world placement is removed.

The type-1 instance inventory reads the original 80-entry frontend model list
and all 5,806 retail locators. The initial VIS row selects 640 instances across
37 model types. The first integrated slice is model `fncw02` (original model ID
`0x09`): all 53 resident white-fence locators are baked with their exact retail
quaternions, positions and scales into one PSP draw group using the original
`fencewhite` material and `World\fencewhite.tga` texture. This is 53/640
resident instances; the remaining 587 are not yet ported.

This is a real original-game frontend milestone, but it is not a playable game
yet. The menu currently uses a bounded PSP implementation of the original class:
the original button construction, locale IDs, panels, colours, transforms and
focus are retained, while the complete world/instance/music/save closure and
downstream state transitions are intentionally deferred. Activating an item
records frontend-menu-selection and reopens the menu.

Current estimates:

| Scope | Estimate | Current boundary |
| --- | ---: | --- |
| PSP toolchain, CI and packaging | 90% | Premake, branded PBP, host tools, PSPDEV build and artifacts work; hardware release packaging remains |
| Original startup plus main-menu milestone | 99% | Legal/Bink screens, original GUI/input, camera, complete initial terrain VIS row, barn sign and first exact instance slice run; remaining instances/environment, audio and hardware validation remain |
| Complete playable-game port | 33% | Original application/frontend runs with its complete initial terrain residency, but rooms, player, world streaming, collision, movies, saves and most audio are absent |

## Non-negotiable repository rules

- Work on psp-port unless another branch is explicitly requested.
- origin is Lime228/OpenBarnyardPSP; upstream is InfiniteC0re/OpenBarnyard.
- Use InfiniteC0re-style Conventional Commits and split logical subsystems:
  fix(engine), feat(frontend), build(psp), docs(psp), and so on.
- Premake owns all project generation and build graph changes. Do not add a
  parallel CMake project.
- This fork is PSP-only. Windows host exporters are allowed; a Windows game
  target is not required.
- Retail data and converted local output stay ignored. Never commit anything
  below Game/ or Runtime/.

## Coding conventions

- Prefer existing Toshi types such as TBOOL, TINT, TUINT32, TSIZE and TUINTPTR.
- If a platform-specific type is necessary, preserve the original name and add
  the _PSP suffix.
- Use file-local static data/functions, not anonymous namespaces.
- Keep logical TSTATICASSERT checks. Only confirmed MSVC class-layout checks use
  TSTATICASSERT_SIZE.
- Allegrex is 32-bit little-endian. Do not serialize host size_t, host pointers
  or MSVC-only layouts.
- Prefer bounded arrays, fixed-capacity caches, offline conversion and explicit
  release over STL-heavy runtime conversion.

## Build architecture

Ports/PSP/premake5.lua declares three projects:

### OpenBarnyardPSPBootstrap

The diagnostic EBOOT remains a CI smoke target for PSP system, GU, input, audio
and bounded asset regressions. Its rotating objects/checkerboard/probe screens
are not retail game content.

### OpenBarnyardPSPGameSlice

A static library with the original root task, state controller, loader, menu
classes and their growing dependency closure. It keeps portability failures
small and reviewable.

### OpenBarnyardPSPGame

The runnable original-game target. It links the slice with the portable Toshi
core and PSP implementations, writes a linker map, produces EBOOT_GAME.PBP, and
uses the branded ICON0.PNG/PIC1.PNG plus MEMSIZE=2.

Important PSP platform implementations include:

- AppBoot_PSP.cpp: original g_oTheApp, bounded 20 MiB Toshi heap and original
  task chain;
- ARenderer_PSP and AGUI2Renderer_PSP: GU display, resource budget and original
  GUI rendering;
- AInputHandler_PSP: PSP controller events through the game input layer;
- AMoviePlayer_PSP: platform movie interface without proprietary Bink;
- ACollisionModel_PSP: platform collision interface without x86 Opcode;
- PSP file, allocator, time, sync, thread, input and texture implementations.

## Current original frontend path

AppBoot_PSP creates the original application/root task and opens:

- retail Data/GUI/fonts.trb;
- retail Data/GUI/texsec.trb;
- retail Data/Locale/eng-uk.trb;
- retail Data/Options/Controls_Win.trb;
- PSP-converted PTEX files resolved from the original GUI material names.
- all eleven converted meshes and textures from the original front-end `Barn_L0Mod0` cell;
- all 94 original `Barn_L0Mod1` meshes, losslessly grouped into 36 PSP material draws with 34 shared textures;
- all six original `Barn_L0Mod2` grass meshes, grouped into one draw because all six use the same `grassmain` material and texture;
- all three original `Barn_L0Mod3` additive meshes with explicit barn/cone light profiles and the retail dawn/night fade schedule;
- all 141 original `Barnnorth_L0Mod0`-`Mod3` meshes, losslessly grouped into 55 PSP material draws with 54 shared textures and seven retail light-profile identities;
- all 49 `BGbarneast_L1Mod0`-`Mod4` meshes as 7 exact material/texture groups;
- all 34 `BGbarnwest_L1Mod0`-`Mod4` meshes as 5 exact material/texture groups;
- all 41 `Hillbase_L1Mod0`-`Mod3` meshes as 7 exact material/texture groups, preserving its one blended material;
- all 21 `Hilltop_L1Mod0`-`Mod2` meshes as 9 exact material/texture groups;
- all 48 `Meadowsouth_L1Mod0`-`Mod2` meshes as 15 exact material/texture groups;
- all 53 initial-row `fncw02` instances as one exact `fencewhite` material/texture group, preserving every locator transform;
- the bounded original `camera_loop` as `frontend_camera.pcam`;
- the original two-mesh barn sign baked at `loc_barnsign`.

T2Locale converts the validated retail UTF-16 table to PSP GCC's native 32-bit
TWCHAR storage. T2GUITextureSectionManager validates the TRB header, section
pointer and bounded section count before creating materials.
AGUI2Renderer_PSP owns a fixed 16-entry texture cache and maps the original
800x600 logical GUI root to 480x272.

The Rekord_Black_26 atlas must remain 512x512 because fonts.trb stores glyph
coordinates for that exact atlas. Other frontend textures retain the default
256-pixel conversion cap unless a measured manifest exception is added.

Measured original-menu runtime with the complete initial retail terrain VIS row,
the first exact instance slice, corrected D3D/GU camera-up boundary and original
dawn light schedule (report v21, CI run `45033577601`):

    phase=frontend-menu-selection
    heap_free=9827032
    heap_largest_hole=9821140
    heap_used=11136664
    render_main_current=8300180
    render_main_peak=8300180
    render_main_budget=16777216
    frontend_terrain_loaded=1
    frontend_terrain_meshes=11
    frontend_terrain_draws=9240
    frontend_terrain_pack_loaded=1
    frontend_terrain_pack_models=36
    frontend_terrain_pack_textures=34
    frontend_terrain_pack_draws=30240
    frontend_grass_loaded=1
    frontend_grass_models=1
    frontend_grass_textures=1
    frontend_grass_draws=840
    frontend_glow_loaded=1
    frontend_glow_models=3
    frontend_glow_textures=1
    frontend_glow_draws=0
    frontend_barnnorth_loaded=1
    frontend_barnnorth_models=55
    frontend_barnnorth_textures=54
    frontend_barnnorth_draws=44520
    frontend_bgbarneast_loaded=1
    frontend_bgbarneast_models=7
    frontend_bgbarneast_textures=7
    frontend_bgbarneast_draws=5880
    frontend_bgbarnwest_loaded=1
    frontend_bgbarnwest_models=5
    frontend_bgbarnwest_textures=5
    frontend_bgbarnwest_draws=4200
    frontend_hillbase_loaded=1
    frontend_hillbase_models=7
    frontend_hillbase_textures=7
    frontend_hillbase_draws=5880
    frontend_hilltop_loaded=1
    frontend_hilltop_models=9
    frontend_hilltop_textures=9
    frontend_hilltop_draws=7560
    frontend_meadowsouth_loaded=1
    frontend_meadowsouth_models=15
    frontend_meadowsouth_textures=15
    frontend_meadowsouth_draws=12600
    frontend_instances_fncw02_loaded=1
    frontend_instances_fncw02_models=1
    frontend_instances_fncw02_textures=1
    frontend_instances_fncw02_draws=840
    frontend_hour=6.000000
    frontend_barnsign_loaded=1
    frontend_barnsign_meshes=2
    frontend_barnsign_draws=1680
    frontend_camera_loaded=1
    frontend_camera_frames=10
    frontend_camera_sample_rate=30
    controller_observed_buttons=00004040
    controller_nonzero_button_samples=29
    menu_focus=1
    menu_command_down_count=1
    menu_command_ok_count=1
    menu_activated_button=1
    menu_activation_count=1
    menu_close_count=1

The controlled one-process run used PID `12864`, captured the upright original
scene and newly visible white fences in `EBOO00649_00038.jpg`, then stopped the
same PID. The 512x512 font atlas is therefore currently acceptable, but the
figures must also be checked on real 32/64 MiB hardware.

## Known limitations

- The complete initial terrain VIS row is loaded at its original LODs, together
  with the original camera loop, barn sign and 53/640 original type-1 instances.
  The `fncw02` slice uses LOD0 and offline exact-transform batching; per-instance
  distance culling/LOD selection is not yet reproduced. The initial 06:00 light
  state matches retail, but time-driven frontend re-entry, the remaining 587
  frontend instances and remaining environment layers are absent. The original frontend
  states create only the barn sign through `AWindmillHelper`; windmills, the
  weather vane and the water wheel belong to gameplay `ASimState`.
- Menu actions are bounded to a selection checkpoint and menu reopen. New game,
  load game, options, extras, antics and quit do not enter their retail states.
- AMoviePlayer_PSP does not decode Bink. Startup images currently follow the
  original GUI presentation path; full-motion retail movies need offline
  conversion and a bounded PSP stream format.
- ACollisionModel_PSP preserves the interface but returns no hits. Gameplay must
  not start until a bounded collision backend exists.
- Retail FMOD banks/music are not integrated. The diagnostic mixer proves PSP
  audio only.
- The texture cache holds at most 16 unique GUI textures. The current menu fits;
  future screens need measured lifetime/reuse or a larger justified budget.
- Rekord18 and Bremen25 are not staged for the current menu because it only uses
  Rekord26.
- Save data, memory-stick UI, rooms, actors, player control, quests and streaming
  are not connected.

## Retail data and repeatable staging

The user's complete PC installation is local and ignored at:

    D:\BARNYARDPSP\Game\BarnyardPC\Install\

Converted frontend workspace:

    Game/BarnyardPC/Workspace/Frontend/
    Game/BarnyardPC/Workspace/FrontendTerrain/
    Game/BarnyardPC/Workspace/FrontendTerrainPack/
    Game/BarnyardPC/Workspace/FrontendTerrainGrass/
    Game/BarnyardPC/Workspace/FrontendTerrainGlow/
    Game/BarnyardPC/Workspace/FrontendTerrainBarnnorth/
    Game/BarnyardPC/Workspace/FrontendTerrainBGbarneast/
    Game/BarnyardPC/Workspace/FrontendTerrainBGbarnwest/
    Game/BarnyardPC/Workspace/FrontendTerrainHillbase/
    Game/BarnyardPC/Workspace/FrontendTerrainHilltop/
    Game/BarnyardPC/Workspace/FrontendTerrainMeadowsouth/
    Game/BarnyardPC/Workspace/FrontendInstances/
    Game/BarnyardPC/Workspace/FrontendCamera/
    Game/BarnyardPC/Workspace/FrontendBarnSign/

Staged PSP runtime:

    Runtime/OpenBarnyardPSPGame/

Never hand-edit generated assets. Main entry points:

| Entry point | Purpose |
| --- | --- |
| ConvertRetailFrontend.ps1 | Extract retail GUI textures and convert them to bounded PTEX, including the 512 font exception |
| StageOriginalGame.ps1 | Assemble an ignored original-game runtime from a CI/local build and retail files |
| ConvertRetailPreview.ps1 | Manifest-driven retail model/skeleton/animation/texture preview |
| ConvertRetailTerrain.ps1 | Manifest-driven bounded terrain cell conversion |
| ConvertRetailFrontendDetail.ps1 | Complete original Barn_L0Mod1 conversion, material grouping and retail property finalization |
| ConvertRetailFrontendGrass.ps1 | Complete original Barn_L0Mod2 grass conversion through the shared exact-binding batch path |
| ConvertRetailFrontendGlow.ps1 | Complete original Barn_L0Mod3 conversion with additive modes and barn/cone light profiles |
| ConvertRetailFrontendBarnnorth.ps1 | Complete original Barnnorth_L0Mod0-Mod3 conversion with exact material grouping and all retail frontend light profiles |
| ConvertRetailFrontendBGbarneast.ps1 | Merge and convert every original BGbarneast low-LOD cell |
| ConvertRetailFrontendBGbarnwest.ps1 | Merge and convert every original BGbarnwest low-LOD cell |
| ConvertRetailFrontendHillbase.ps1 | Merge and convert every original Hillbase low-LOD cell |
| ConvertRetailFrontendHilltop.ps1 | Merge and convert every original Hilltop low-LOD cell |
| ConvertRetailFrontendMeadowsouth.ps1 | Merge and convert every original Meadowsouth low-LOD cell |
| InventoryRetailFrontendInstances.ps1 | Export all retail locator/VIS tables and match the original type-1 model list and prefix rules |
| ConvertRetailFrontendInstanceModel.ps1 | Bake every initial-row locator of one original model into bounded exact-material PSP groups |
| merge_frontend_terrain_cells.py | Validate and concatenate complete version-2 terrain cells without changing source order |
| ConvertRetailFrontendCamera.ps1 | Export and reduce the original camera hierarchy/loop to PCAM |
| ConvertRetailBarnSign.ps1 | Export the world-model meshes/textures and bake the original locator transform |
| ResolvePSPHostTool.ps1 | Select the newest downloaded/local host exporter for every repeatable retail conversion |
| ValidateOriginalGame.ps1 | One-process validation of a staged runtime through PPSSPP report/input/screenshot evidence |
| convert_dds.py | DXT and masked DDS decode, reduction and PSP swizzle |
| mark_retail_layout_asserts.py | Repeatable marking of confirmed MSVC size assertions |
| audit_source_dependencies.py | Detect forbidden desktop dependencies in selected PSP sources |

Typical local frontend preparation:

    .\Ports\PSP\StageOriginalGame.ps1 -BuildDirectory .\Runtime\CIArtifacts\<commit>

## CI and local PPSSPP workflow

GitHub workflow: .github/workflows/psp.yaml.

- host-tools builds TMDL on VS2022.
- bootstrap runs converter tests, generates the Premake PSP workspace, builds
  the diagnostic and original-game targets, and publishes separate artifacts.

Current green original-menu/3D-slice artifact: PSP run `44997003987`, commit
`4d8d7b04`. Its report v20 proves all 448 source meshes from the complete initial
terrain VIS row are resident at their original LODs, together with the 06:00
light state, camera and barn sign. Down and Cross traverse the complete
PPSSPP/sceCtrl/Toshi/retail-input-map/menu path. The matching one-process
validation captured correctly oriented `EBOO00649_00037.jpg`, exercised input
and stopped PID `28404` cleanly. A full staging pass without `SkipConversion`
also regenerated every retail frontend pack successfully from `Install`.

Install the pinned official PPSSPP development build once:

    .\Ports\PSP\InstallPPSSPP.ps1

Use exactly one background emulator instance:

    .\Ports\PSP\RunPPSSPP.ps1 -Action Start -OriginalGame
    .\Ports\PSP\RunPPSSPP.ps1 -Action Status -OriginalGame
    .\Ports\PSP\RunPPSSPP.ps1 -Action Press -OriginalGame -Key DpadDown
    .\Ports\PSP\RunPPSSPP.ps1 -Action Press -OriginalGame -Key Cross
    .\Ports\PSP\RunPPSSPP.ps1 -Action Stop -OriginalGame

For the repeatable end-to-end check, use the wrapper below. It starts exactly
one background instance, waits for a fresh report, requests a PSP screenshot,
injects Down/Cross, validates all current frontend gates and stops the same PID:

    .\Ports\PSP\ValidateOriginalGame.ps1

In a sandboxed validation session, run start, wait, input/screenshot, report and
stop inside one shell invocation. The sandbox may terminate background child
processes at command exit; that is not evidence of a game crash. The runner
launches minimized and injects controller state through PPSSPP's local
WebSocket debugger without taking user focus.

Runtime evidence:

    Runtime/OpenBarnyardPSPGame/openbarnyard_psp_game.log
    Runtime/Tools/PPSSPPDev/memstick/PSP/SCREENSHOT/

## Next implementation order

1. Extend the exact `fncw02` type-1 path across the remaining 36 model types and
   587 locators selected by the initial VIS row, preserving model IDs,
   transforms, material ownership and bounded visibility/LOD behaviour.
2. Restore the remaining original environment ownership and time-driven
   frontend re-entry without changing the complete initial terrain VIS row.
3. Connect the original menu's downstream states one at a time, beginning with
   the smallest options or extras screen before new-game world loading.
4. Convert/integrate the retail menu cue/music ownership behind the existing PSP
   audio interfaces.
5. Design and implement offline movie conversion plus bounded PSP playback.
6. Implement a bounded collision representation before any gameplay claim.
7. Advance to one reduced playable room, then add its gameplay-owned windmills,
   weather vane, water wheel, actor/player control, saves and zone streaming
   with memory telemetry.

## Recent logical commit boundary

- c249ef5e, 881f7f00: retain and share the original PSP game heap.
- a8ea23d2: load retail PSP GUI materials.
- a86b0ea9: stage original menu resources.
- b292d80b: convert retail locale strings for PSP.
- 36efbea9: enter the original PSP main menu.
- 8d6433d3: validate PSP GUI section data.
- b9885c57: preserve the 512x512 original font atlas.
- 5790ebb7: fit the original 800x600 GUI to PSP.
- a593704d: preserve the original final vertical orientation on the GU.
- cf28eaa0: inject background controls through PPSSPP's debugger.
- 3097316b, 92e7c229: trace retail commands and raw PSP controller samples.
- 02317c22: preserve PSP controller transitions across repeated acquisition.
- da1d645d, 983c1ffa, ad2df3db: export the retail camera and match its final
  world-up behaviour.
- 01d6da8d: apply the original camera to the PSP frontend.
- 4ac62801: export original terrain locators from TMDL.
- 541247bb: restore the original +pi/2 terrain axis transform.
- 4ace8fa3: convert and stage the original frontend barn sign.
- 336a5563: render the original barn sign and report its residency/draws.
- 37b7399d: automate the single-process original frontend validation.
- c39a0211, 500b5fa5, 63b8ff38: preserve Mod3 light profiles and the original
  dawn/night schedule through conversion, runtime and validation.
- ceaa7dc5: resolve the newest PSP host tool consistently for all converters.
- 29ee7e5e, 13aebffe: export complete locator/VIS tables and inventory the
  original frontend type-1 instances.
- 8bf0b32f, 9370ad99, e7588eab: resolve exact model textures, batch original
  locator transforms and select the correct shared startup texture owner.
- ca5d8548, 93634128: render, stage and validate the first 53 original
  `fncw02` instances.

## Claims that must not be made

- The full game is not playable.
- The current partial barn/sign background is not the finished retail frontend
  scene.
- Legal/Bink presentation does not mean Bink video decoding is implemented.
- Menu selection checkpoints are not downstream menu/game state support.
- Diagnostic models, terrain and audio prove platform paths only.
- A green CI build proves compilation and packaging, not real-hardware safety.

## Companion documents

- Docs/PSP_PORT.md: detailed history, progress and subsystem notes.
- Docs/PSP_PORTING_ISSUES.md: confirmed failures, causes and fixes.
- Docs/PSP_ASSET_INTAKE.md: retail input layout and asset contracts.
- Ports/PSP/README.md: build, staging and runner commands.

Keep these documents synchronized whenever the runtime boundary changes.
