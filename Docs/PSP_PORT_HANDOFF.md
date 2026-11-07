# OpenBarnyard PSP port handoff

Updated: 2026-11-07  
Repository: `https://github.com/Lime228/OpenBarnyardPSP.git`  
Working branch: `psp-port`  
Upstream source branch: `InfiniteC0re/OpenBarnyard:new-modcore`

## Read this first

The product goal is a PSP version of the original Barnyard game: its original
startup sequence, frontend, menus, game states, rooms and gameplay, adapted only
where PSP hardware or APIs require it. The runnable EBOOT in this branch is
still a diagnostic bootstrap. It is useful for regression testing PSP platform
code, but it is **not** the game and must not be presented as the original
frontend.

The real game migration has started in the separate
`OpenBarnyardPSPGameSlice` static library. It currently compiles original game
sources with PSP GCC, including the root task, loader, state controller, attract
state and main-menu state. That library is not linked into the EBOOT yet, so
none of those original states execute at runtime.

Current honest estimates:

| Scope | Estimate | Meaning |
| --- | ---: | --- |
| Diagnostic PSP bootstrap infrastructure | 70% | Toolchain, package, selected Toshi services, GU probes and bounded sample assets work |
| Complete playable-game port | 20% | Original startup source compiles in isolation; the real renderer, GUI, content graph and gameplay closure do not run |

Do not spend the next block making the custom bootstrap resemble the retail
menu. Keep it only as a hidden regression harness. The next milestone is to
link and execute original `OpenBarnyard/Source` code against PSP backends.

## Repository and source-control rules

- Work only on `psp-port` unless a new branch is explicitly requested.
- `origin` is `Lime228/OpenBarnyardPSP`; `upstream` is
  `InfiniteC0re/OpenBarnyard`.
- Preserve InfiniteC0re's Conventional Commit style and split work by logical
  subsystem. Examples: `feat(render): ...`, `fix(engine): ...`,
  `build(psp): ...`, `docs(psp): ...`.
- Do not mix engine, frontend, asset tooling and documentation into one commit.
- Use Premake for project generation and ownership of the build graph. Do not
  introduce a parallel CMake project.
- The fork is PSP-only. A Windows game build is not required; Windows host
  exporters such as TMDL are allowed and expected.
- Original retail assets and converted local output must never be committed.
  `Game/` and `Runtime/` are ignored for this purpose.

## Port coding conventions

- Prefer existing Toshi types (`TBOOL`, `TINT`, `TUINT32`, `TSIZE`,
  `TUINTPTR`, and related types) over equivalent standard or platform types.
- If a PSP-specific type is necessary, retain the original name and add the
  `_PSP` suffix.
- Use `static` for file-local data and helper functions. Do not add anonymous
  namespaces to PSP implementation files.
- Keep ordinary logical `TSTATICASSERT` checks enabled. Retail Win32 ABI size
  checks use `TSTATICASSERT_SIZE`, which remains strict on PC and is explicitly
  skipped on PSP.
- Treat Allegrex as a 32-bit little-endian target. Never serialize host-sized
  `size_t` or pointer-dependent layouts.
- Use bounded arrays, bounded scratch buffers and offline conversion. Avoid
  desktop-style permanent heaps, runtime container relocation and hidden
  copies.

## Build architecture at handoff

`Ports/PSP/premake5.lua` defines two projects:

### `OpenBarnyardPSPBootstrap`

This is the current executable and Premake start project. It builds
`bin/PSP/<Configuration>/EBOOT.PBP`, titled **OpenBarnyard PSP**, with
`ICON0.PNG`, `PIC1.PNG` and `MEMSIZE=2` in its SFO. It contains the diagnostic
code below `Ports/PSP/Source`, a selected portable Toshi subset and PSP
platform implementations.

It validates controller input, scheduler/platform services, PSP GU rendering,
bounded static and skinned sample assets, audio, locale, frontend-logo and
terrain-preview paths. Several screens and objects shown by this executable are
synthetic probes. They are not retail Barnyard UI or gameplay.

### `OpenBarnyardPSPGameSlice`

This is a compile-only static library used to expose portability errors in the
original code without pretending its complete link dependency graph exists.
At this handoff it compiles:

- `OpenBarnyard/Source/GameInterface/AFrontEndAttractState.cpp`
- `OpenBarnyard/Source/GameInterface/AFrontEndMainMenuState2.cpp`
- `OpenBarnyard/Source/GameInterface/AGameState.cpp`
- `OpenBarnyard/Source/GameInterface/AGameStateController.cpp`
- `OpenBarnyard/Source/Helpers/AGameLoader.cpp`
- `OpenBarnyard/Source/Platform/PSP/ACollisionModel_PSP.cpp`
- `OpenBarnyard/Source/Platform/PSP/ADisplayModes_PSP.cpp`
- `OpenBarnyard/Source/Platform/PSP/AMoviePlayer_PSP.cpp`
- `OpenBarnyard/Source/Platform/PSP/AppBoot_PSP.cpp`
- `OpenBarnyard/Source/Tasks/ARootTask.cpp`

`AppBoot_PSP.cpp` defines the original `MODAPI AApplication g_oTheApp` and
creates the original dummy-task chain and `ARootTask`. It deliberately omits
Win32 virtual memory, mutex, message-box and `WinMain` code. It does not yet
reproduce all of the PC `CreateOptionalSystems`, locale and free-list setup, so
linking it prematurely would not produce a safe game runtime.

## PSP platform work already present

- `TSystemDLL_PSP`: PSP-safe replacement for the dynamic-library seam.
- `T2Atomic_PSP`: Allegrex-compatible engine atomic integer operations.
- `ADisplayModes_PSP`: one fixed 480x272x32 display mode.
- `ACollisionModel_PSP`: preserves the original collision-model interface and
  keeps x86/MSVC Opcode out of the PSP frontend dependency graph.
- `AMoviePlayer_PSP`: preserves the original movie-player interface without
  pulling proprietary Bink headers into PSP compilation.
- `TTMD_PSP`: PSP-side model LOD/header contracts.
- PSP Toshi implementations for allocation, time, files, threads, mutexes,
  semaphores, controller input and selected render/resource services used by
  the harness.

Important limitations:

- `ACollisionModel_PSP` currently stores model/index information but returns
  `false` from every collision query. This is sufficient only to unblock menu
  compilation and must become a real bounded collision backend before gameplay.
- `AMoviePlayer_PSP::PlayMovie` currently returns `false`. It constructs the
  retail-style `Data/Movies/%s.pmvf` path but no PSP decoder exists yet.
- The original game slice is compiled but not linked or executed.
- The runnable diagnostic class `Ports/PSP/Source/Frontend/AApplication_PSP`
  is unrelated to the original `AApplication` defined by `AppBoot_PSP.cpp`.

## Why key PC dependencies were replaced

- Bundled Opcode is an x86/MSVC implementation containing `__declspec`,
  `__int64` and x86 assembly. Header-case/include fixes were useful for host
  portability, but Opcode cannot execute on Allegrex. PSP therefore selects
  `ACollisionModel_PSP` at the platform boundary.
- The PC root task directly selected Bink and required proprietary `bink.h`.
  PSP selects `AMoviePlayer_PSP`; retail movies will require an offline
  conversion format and a bounded PSP decoder.
- Exact retail class sizes describe MSVC Win32 layout, not portable gameplay
  invariants. `TSTATICASSERT_SIZE` separates those checks without disabling
  normal assertions.

The confirmed failures, root causes, fixes and validation signals are maintained
in `Docs/PSP_PORTING_ISSUES.md`. Continue adding rows only after a root cause is
confirmed.

## Build and CI

From Windows with WSL and PSPDEV installed:

```powershell
.\Ports\PSP\Build.ps1
```

The underlying Linux entry point is:

```sh
./Ports/PSP/build.sh
```

It regenerates all deterministic bootstrap assets, runs:

```sh
premake5 --file=premake5.lua gmake --target=psp
make -C Build/PSP config=release_psp -j"$(nproc)"
```

and produces `bin/PSP/Release/EBOOT.PBP`.

The GitHub workflow is `.github/workflows/psp.yaml` and contains:

- `host-tools`: builds the Windows x86 TMDL exporter on VS2022 and publishes
  `OpenBarnyard-PSP-host-tools`.
- `bootstrap`: tests deterministic converters, generates the Premake PSP
  project, builds it in `ghcr.io/pspdev/pspdev:latest`, and publishes
  `OpenBarnyard-PSP-bootstrap`.

Last verified code run at handoff: workflow `42352921891`, commit `34848273`.
Both `host-tools` (job `128344709341`) and `bootstrap` (job `128344709574`)
completed successfully. This verifies compilation and packaging, not original
game execution.

## PPSSPP workflow

Use exactly one emulator instance through the background runner:

```powershell
.\Ports\PSP\RunPPSSPP.ps1 -Action Start
.\Ports\PSP\RunPPSSPP.ps1 -Action Status
.\Ports\PSP\RunPPSSPP.ps1 -Action Restart
.\Ports\PSP\RunPPSSPP.ps1 -Action Press -Key Cross
.\Ports\PSP\RunPPSSPP.ps1 -Action Stop
```

The script launches the bundled PPSSPP minimized, tracks/adopts its PID, removes
duplicate instances at the same executable path and injects controller keys
without taking keyboard/mouse focus. Do not launch PPSSPP manually for routine
checks and do not create multiple instances. PPSSPP validation is not a
substitute for testing on 32/64 MiB PSP hardware.

## Retail asset location and current discrepancy

The documented, ignored retail-data root is:

```text
D:\BARNYARDPSP\Game\BarnyardPC\Install\
```

The user previously said the whole game had been copied directly into an
`Install` directory without an additional subfolder. At this handoff neither
`D:\BARNYARDPSP\Install` nor the documented install directory is present in
this checkout. Before any retail conversion, locate the user's existing copy
and place or reference it under the ignored structure above. Do not download,
commit or upload proprietary game data.

Expected local-only layout:

```text
Game/BarnyardPC/Install/               complete untouched PC installation
Game/BarnyardPC/Selected/              deliberately selected small inputs
Game/BarnyardPC/Manifests/             text selection manifests
Game/BarnyardPC/Tools/TMDL.exe         downloaded CI host exporter
Game/BarnyardPC/Workspace/Dumps/       ignored intermediate JSON/DDS dumps
Game/BarnyardPC/Workspace/Converted/   ignored PSP assets
```

See `Docs/PSP_ASSET_INTAKE.md` for manifest formats and the exact first-content
workflow.

## Repeatable asset tooling

Never hand-edit generated PSP assets. Extend and test these repeatable paths:

| Entry point | Purpose |
| --- | --- |
| `ConvertRetailFrontend.ps1` | Extract and reduce the shipping Barnyard logo to a PSP-safe 256x64 PTEX |
| `ConvertRetailPreview.ps1` | Manifest-driven retail skin, skeleton, animation and texture conversion |
| `ConvertRetailTerrain.ps1` | Manifest-driven bounded cell/mesh/material/texture conversion |
| `convert_barnyard_skin.py` | Transactional orchestration and `.pset` manifest publication |
| `convert_tmd_dump.py` | Strip triangulation and two-influence PSP skin reduction |
| `convert_animation.py` | Bounded PANM quantization, max 64 bones/frames |
| `convert_dds.py` | DXT1/3/5 and masked 16/32-bit DDS to reduced/swizzled PTEX |
| `convert_terrain_dump.py` / `convert_terrain_cell.py` | Bounded static PMES terrain conversion |
| `convert_mesh.py` / `convert_skeleton.py` / `convert_texture.py` | Synthetic and intermediate mesh/skeleton/texture formats |
| `convert_world.py` | Bounded PWLD resources, instances and deterministic spatial cells |
| `convert_audio.py` / `generate_wave_bank.py` | Bounded resident PPCM and streamed PADP audio assets |
| `mark_retail_layout_asserts.py` | Repeatable conversion of confirmed Win32 size checks to `TSTATICASSERT_SIZE` |
| `audit_source_dependencies.py` | Detect forbidden desktop dependencies in selected PSP sources |

Converter tests live beside these scripts and run in CI. Add a deterministic
test whenever a new retail edge case or reduction rule is introduced.

## PSP constraints that govern every implementation

- Native resolution is 480x272. Main RAM is constrained to PSP hardware limits
  and display/depth/texture use must fit the separate 2 MiB EDRAM budget.
- The GU expects aligned, cache-written-back data. Current texture contracts use
  16-byte alignment, power-of-two dimensions/strides and a maximum dimension
  of 512.
- Prefer preconverted immutable data, zone streaming, fixed-capacity ownership
  and explicit release to runtime decoding and STL-heavy asset graphs.
- Do not restore the PC 256 MiB heap request.
- PSP has no Win32 DLL/mod-injection model, DirectX, FMOD or Bink runtime.
  Platform services must enter through PSP implementations.
- Input is one local PSP controller with fewer controls. Map through the real
  game input layer rather than adding frontend-only shortcuts.

## Exact next implementation path

Work in this order so every step produces a truthful, reviewable milestone:

1. **Complete the original PSP application bootstrap.** Compare PC
   `AppBoot.cpp` with `AppBoot_PSP.cpp`; port required free-list, locale and
   optional-system initialization through PSP-safe services. Exclude Win32 and
   ModCore injection behaviour explicitly.
2. **Build the original root-task link closure.** Add original sources in small
   groups to the compile library: GUI system (`AGUI2`/`AGUISystem`), asset and
   material libraries, renderer task, input handler/maps, sound manager adapters
   and locale. Use unresolved symbols and a linker map as the dependency list;
   do not replace original states with lookalike PSP states.
3. **Create a real-game EBOOT target.** Keep
   `OpenBarnyardPSPBootstrap` as a separate regression executable. Add a new
   original-game executable only when it intentionally references original
   `g_oTheApp`/`ARootTask` and its startup closure can link.
4. **Reach the original frontend with bounded content.** Convert and package
   the retail frontend library, main GUI sections, fonts, locales, camera,
   animation, models/materials and necessary sounds into a deterministic PSP
   manifest. Missing data must fail clearly; synthetic bootstrap UI is not an
   acceptable fallback for this milestone.
5. **Implement movie replacement.** Determine the retail startup movie set,
   add an offline converter to a PSP-suitable stream and implement bounded
   decode/playback behind `AMoviePlayer_PSP`. Bink itself cannot ship.
6. **Implement collision before gameplay.** Replace the all-false
   `ACollisionModel_PSP` fallback with a profiled, bounded Allegrex-compatible
   collision representation produced offline.
7. **Validate the original menu.** Confirm original attract/menu states,
   original assets, input transitions, audio ownership and memory telemetry in
   one PPSSPP instance, then on hardware.
8. **Advance to one playable room.** Only after the real frontend works, bring
   up one reduced room, actor/player control, physics and zone streaming within
   measured budgets.

The immediate next commit should therefore concern original application
initialization or one dependency group of the original root-task closure, not
another custom diagnostic screen.

## Known-good commit boundary

The handoff starts after these logical blocks:

- `f16aee45`–`141c3e78`: restored the original PSP PCH surface, allocator type
  discipline, GCC template portability and PSP atomic support.
- `4ddd4db9`–`878b923c`: compiled original game-state/controller sources and
  added the PSP display-mode backend.
- `1fbfbf96`–`4cd89369`: separated Win32 retail layout checks and added the
  repeatable marking tool.
- `1b2a59b5`–`1437e931`: compiled original menu, attract and loader sources.
- `244cde87`–`51237197`: diagnosed Opcode and introduced the PSP collision
  contract instead of attempting to execute x86 code.
- `7f416f15`–`6abe76f7`: added PSP model contracts, marked transitive retail
  layouts and compiled original `ARootTask`.
- `1d2e6790`–`3e43603c`: introduced the PSP movie contract and explicit fallback.
- `aa9ccdc8`: added the explicit collision fallback.
- `34848273`: added the original PSP `AApplication` bootstrap and completed the
  green compile-only startup slice.

At the moment this file was written, the working tree was clean before adding
the handoff document.

## Claims that must not be made yet

- The original Barnyard menu does not run on PSP yet.
- The original splash/movie sequence does not play yet.
- The original game slice is not linked into the runnable EBOOT.
- The diagnostic rotating models, checkerboard, logo and terrain preview are
  validation probes, not evidence that the corresponding retail game system is
  ported.
- Collision and movie playback are interface-compatible fallbacks, not finished
  implementations.
- A green CI run proves conversion tests, PSP compilation and packaging only;
  it does not prove gameplay or hardware memory safety.

## Companion documents

- `Docs/PSP_PORT.md`: detailed subsystem notes, progress and milestone plan.
- `Docs/PSP_PORTING_ISSUES.md`: reproducible failures and their confirmed fixes.
- `Docs/PSP_ASSET_INTAKE.md`: local retail-data layout, manifests and converters.
- `Ports/PSP/README.md`: bootstrap build, PPSSPP runner and converter usage.

Keep all four documents synchronized when the runtime boundary changes.
