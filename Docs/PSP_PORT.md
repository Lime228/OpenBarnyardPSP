# OpenBarnyard PSP port plan (`new-modcore`)

## Current state

The upstream game is not portable as-is. Its workspace only declares Win32, its
entry point and bootstrap use Win32 APIs, and the complete render backends are
DirectX 8 and DirectX 11. The `Revolution` directory currently contains only a
texture implementation, so it is useful as a reference but is not a complete
console backend.

`Ports/PSP` began as milestone 0 and is now the M1/M2 bootstrap EBOOT that proves
the toolchain, portable core, input, and PSP GU rendering path independently
from the Windows executable.

## Progress snapshot (2026-10-31)

The PSP platform/render bring-up is approximately **73%** complete, while the
complete playable-game port is approximately **24%** complete. The second value
is intentionally much lower: a compiling probe does not yet include the real
game states, Barnyard asset conversion, animation, audio, UI, world streaming,
or gameplay validation on hardware.

| Area | Estimate | Current boundary |
| --- | ---: | --- |
| PSP toolchain and packaging | 95% | Premake, EBOOT, deterministic assets, CI artifacts and a PPSSPP bundle work |
| Portable Toshi/platform core | 83% | allocator, strings, containers, file I/O, timer, sync, threads, input events and scheduler tasks pass the PPSSPP bootstrap gate |
| PSP renderer foundation | 53% | GU display, depth, indexed static models, textures, materials, colour rectangles, a bootstrap font, budgets and small factories build for PSP |
| Real-game asset pipeline | 10% | only diagnostic PPM/OBJ to PTEX/PMES conversion exists |
| Input integration | 55% | `AInputHandler_PSP` maps PSP controls to frontend actions above the runtime-verified Toshi controller bridge |
| Game bootstrap and frontend | 22% | `AApplication_PSP` owns explicit frame phases and separate menu/probe state objects render the first labelled frontend |
| Audio, movies and saves | 0% | platform implementations are not started |
| World, animation and gameplay | 0% | no game room or player simulation is loaded |

M0 is runtime-complete in PPSSPP. The current M1 and M2 slices are also
runtime-validated in PPSSPP, but still require real-hardware validation. M3 has
an application lifecycle and a scheduler-driven three-phase frontend. M4-M5
remain largely unstarted.

## Port conventions

- Reuse the engine typedefs (`TBOOL`, `TINT`, `TUINT32`, `TSIZE`, `TUINTPTR`,
  and related types) instead of introducing equivalent standard-library or
  platform integer types into port code.
- Follow the name of the original platform implementation when adding a PSP
  equivalent and use the `_PSP` suffix for PSP-specific classes and files.
- Keep file-local state and helper functions `static`; do not introduce
  anonymous namespaces into the PSP implementation files.
- Keep engine, frontend, build, and documentation changes in separate logical
  Conventional Commits with an explicit scope.
- Use Premake as the project-level build description. The `--target=psp`
  workspace owns compilation, linking, and EBOOT packaging for the bootstrap.

## Required platform work

1. **Compiler and ABI portability**
   - Add `TOSHI_SKU_PSP` and replace MSVC-only spellings such as `__declspec`,
     `__forceinline`, `__assume`, calling conventions, and CRT initialiser tricks.
   - Audit layout assertions and every x86 pointer/integer assumption for the
     32-bit little-endian Allegrex target.
   - Compile a small, dependency-ordered Toshi core library before attempting the
     whole game.

2. **System layer**
   - Implement time, threads, mutexes, semaphores, logging, file I/O, and exit
     callbacks with PSPSDK APIs.
   - Replace Windows DLL/mod loading. The PC ModCore hook/injection mechanism is
     not a portable runtime architecture and should initially be disabled.

3. **Renderer**
   - Create a PSP GU backend for render state, vertex/index buffers, textures,
     viewports, order tables, and model drawing.
   - Translate the DX8 fixed-function material path to GU/GUM. Features that have
     no PSP equivalent need CPU-side conversion or simplified shaders/materials.
   - Add offline conversion for unsupported PC texture formats and swizzle PSP
     textures for cache-friendly upload.

4. **Input, audio, and movies**
   - Map `TInputInterface` to `sceCtrl`; PSP has one local controller and fewer
     inputs than the PC/Xbox-oriented code expects.
   - Replace FMOD with a PSPSDK audio mixer/decoder layer.
   - Disable Bink first, then add a PSP-suitable preconverted movie format if
     movies are required.

5. **Memory and content budgets**
   - Remove the current 256 MiB heap request. The port must run within PSP memory
     limits and leave room for the OS, code, streaming buffers, and 2 MiB VRAM.
   - Profile asset residency, reduce texture/model sizes, stream by zone, and set
     explicit budgets for render, audio, world, and transient data.
   - Keep original game assets outside the repository. Build conversion tools
     that operate on a user's legally obtained data.

## PSP technical constraints

- Treat Allegrex as a 32-bit little-endian MIPS target. Pointer casts use the
  existing `TUINTPTR` type and serialized formats use fixed-width Toshi types;
  host-sized C++ types must not leak into file layouts.
- Keep the runtime working set below the PSP user-memory ceiling and budget
  main memory separately from the 2 MiB embedded DRAM used by the display and
  depth buffers. Large PC-style permanent heaps are not acceptable.
- GU-visible allocations require the alignment expected by their format and a
  data-cache writeback before drawing. Textures currently use 16-byte-aligned
  storage and power-of-two dimensions/stride up to the GU limit of 512.
- Prefer offline conversion, compact immutable data, streaming, and bounded
  scratch buffers over runtime decoding and repeated allocations. Avoid hidden
  copies on asset load and keep ownership explicit until a resource is unloaded.
- Do not assume desktop threads, dynamic libraries, filesystem paths, codecs,
  or graphics APIs exist. Every platform service enters through a small PSP
  implementation that can be exercised independently in the smoke EBOOT.

### M1 implementation notes

- `T2GlobalAllocator` uses the PSP C heap through `T2Allocator_PSP.cpp`.
  `TMemory_PSP.cpp` bridges the legacy global `TMalloc`, `TMemalign`,
  `TRealloc`, and `TFree` entry points to the same heap for generic containers
  that still use the original memory API.
- `GetUsedSize` and `GetCapacity` remain unavailable until the bounded PSP
  `TMemory` heap is brought up; both report zero, matching the existing global
  allocator implementation.
- The portability smoke test verifies a 64-byte-aligned allocation before the
  allocator is used by strings and containers.
- `TMutex` uses a PSPSDK semaphore and retains the engine's recursive locking
  behaviour by tracking the owning PSP thread and lock depth. The smoke test
  covers recursive acquisition and the non-blocking `FLAG_NO_WAIT` path.
- `T2String8` and `TString8` now build without the Windows precompiled header;
  the PSP smoke test covers bounded formatting, case-insensitive comparison,
  integer conversion, and allocator-backed dynamic strings.
- The generic `TFile`, `TFileSystem`, and `TFileManager` path layer is compiled
  with the existing `TDList` implementation. Path joining and `..`
  simplification are covered before the PSP-native `sceIo*` backend is added.
- `TNativeFile_PSP` mounts `local:` and `abs:` through the original file-system
  hierarchy and implements open, close, read, write, seek, size, removal, and
  directory enumeration with PSPSDK `sceIo*` calls. The current implementation
  is deliberately unbuffered; engine read/write buffering can be restored after
  the core streaming paths are profiled on hardware.
- The PSP smoke path creates a temporary file, verifies write/size/seek/read,
  finds it through directory enumeration, and removes it before shutdown.
- `TSemaphore` maps directly to a PSPSDK kernel semaphore. `TThread_PSP`
  preserves the existing Toshi priority and manager API while using PSP thread
  creation, start, priority, termination, and self-exit calls. Manager list
  access is protected by the PSP `TMutex` implementation.
- The worker-thread smoke path synchronises through `TSemaphore` and is compiled
  into the EBOOT. CI validates the complete M1 binary and PPSSPP validates the
  runtime gate; real-hardware validation is still required.

## Milestones

- **M0 — toolchain probe:** `EBOOT.PBP` renders and reads controls (implemented).
- **M1 — portable Toshi core:** math, containers, memory, timer, file, and thread
  smoke tests run on PSP without the game.
- **M2 — render slice:** load one converted texture/model and render it through a
  `TRenderInterface` PSP implementation.
- **M3 — game bootstrap:** start the scheduler and reach a minimal front-end with
  audio and movies disabled.
- **M4 — playable room:** load one reduced level, control the player, and stream
  assets within a measured memory budget.
- **M5 — content pass:** converters, performance fixes, audio, saves, UI scaling,
  and full-game compatibility.

### M2 implementation notes

- `T2Texture_PSP` keeps the existing `SetData`, `Load`, and `Unload` lifecycle.
  Its input is a versioned `PTEX` blob containing a fixed-width Toshi header and
  preconverted RGBA8888 pixels; runtime decoding of PC texture containers is
  intentionally outside the PSP executable.
- Texture dimensions and stride must be powers of two and no larger than 512.
  Upload storage is allocated through `T2GlobalAllocator` with 16-byte alignment
  and written back from the data cache before GU sampling.
- PTEX format 1 stores RGBA8888 pixels in PSP-native 16-byte by 8-row swizzle
  blocks. The converter performs this layout offline and rejects dimensions that
  cannot form complete blocks; `--linear` retains format 0 for diagnostics and
  small textures. The GU swizzle flag is selected from the validated PTEX format.
- The bootstrap renders an embedded 16x16 checker texture as a scaled GU sprite.
  Failure to validate or upload the texture is reported in debug output and in
  the same red screen state used by the M1 smoke tests.
- `Ports/PSP/Tools/convert_texture.py` converts a deterministic ASCII P3 PPM
  source to the versioned `PTEX` layout. The generated `probe.ptex` is copied
  next to `EBOOT.PBP` by the Premake post-build step and both files are published
  in the PSP workflow artifact.
- The frontend loads `local:probe.ptex` through the existing `TFile` layer and
  releases the source file allocation immediately after `T2Texture_PSP` copies
  pixels into its aligned GU-visible storage. Missing or invalid external data
  falls back to the embedded checker so the rendering and filesystem paths
  remain independently diagnosable.
- `TRenderInterface_PSP` currently owns only the GU display lifecycle, scene
  boundaries, a colour-triangle probe, and a textured-sprite probe. It remains
  a small standalone slice until the generic render base can be separated from
  the complete PC resource, model, and context graph.
- `TVertexPoolResource_PSP` and `TIndexPoolResource_PSP` provide bounded,
  16-byte-aligned persistent buffers without pulling in the PC `TResource`
  factory graph. They retain the engine's `Lock`/`Unlock` vocabulary, use the
  existing 16-bit `TIndexType`, and write back only the populated range before
  GU consumption.
- The frontend submits a four-vertex, six-index colour mesh through
  `GU_INDEX_16BIT`. This probe verifies persistent buffer lifetime and indexed
  drawing independently from the per-frame GU display-list allocations used by
  the original triangle and sprite probes.
- `Ports/PSP/Tools/convert_mesh.py` triangulates static OBJ faces offline and
  writes the versioned `PMES` format. Version 1 stores a fixed 24-byte header,
  the engine's 16-bit `TIndexType`, and either 16-byte colour/XYZ or 24-byte
  UV/colour/XYZ vertices. Position/UV pairs are deduplicated offline and mixed
  textured/untextured face references are rejected; both element counts are
  capped at 65,535.
- `TModel_PSP` validates the complete blob and every index before allocating its
  vertex/index pools. Once both aligned buffers have been populated and written
  back, the frontend releases the source file allocation instead of retaining a
  duplicate model in the PSP heap.
- `TRenderInterface_PSP::DrawModel` supplies the minimal GUM projection, view,
  and model transforms, clears and enables the 16-bit depth buffer for the 3D
  draw, then restores the depth-test state used by the independent 2D probes.
- Textured models reuse `T2Texture_PSP` directly. Normalized OBJ UVs are scaled
  to the PTEX dimensions before GU submission and `GU_TFX_MODULATE` combines
  texture samples with the packed vertex colour. Texture scale and offset are
  explicitly reset by the shared bind helper so 2D sprites cannot inherit 3D
  material state.
- `TRenderMaterial_PSP` explicitly selects texture enable/function and none,
  front, or back culling. The probe uses modulated texturing and outward CCW
  faces with backface culling; texture and cull state are disabled again after
  the model draw so subsequent UI rendering starts from a known state.
- `TRenderMemory_PSP` enforces a 16 MiB persistent render-resource budget and
  records current/peak main-memory use. Texture and vertex/index allocations
  reserve their complete capacity before calling the allocator and release the
  same amount on failure or destruction, so budget rejection is deterministic.
- Display creation accounts for 1,392,640 bytes of the PSP's 2 MiB eDRAM: two
  512x272 RGBA8888 frame buffers plus one 512x272 16-bit depth buffer. The
  frontend prints both main-memory and eDRAM current/budget values with the main
  render diagnostics.
- `TTextureFactory_PSP` is the first game-facing factory bridge. It mirrors the
  original `CreateTextureFromFile`/`CreateTextureFromMemory` vocabulary while
  producing the existing caller-owned `T2Texture` slice, bounds transient file
  input to 4 MiB, and releases the source blob immediately after upload. It does
  not yet inherit the PC `TResource` registry or retain a 1024-list name cache.
- `TModelFactory_PSP` applies the same caller-owned and bounded-file policy to
  PMES resources, removing the remaining file-blob loading code from the
  frontend.
- `TRenderResourceCache_PSP` provides 32 fixed caller-owned slots with
  case-insensitive names up to 49 characters. It performs no heap allocation
  and does not own resource lifetime; the frontend clears it before unloading
  its texture/model objects. This is the bounded bridge toward named game
  resources, not a replacement for the full PC runtime-class hierarchy.
- Premake places `probe.pmes` beside `EBOOT.PBP` and `probe.ptex`. CI regenerates
  both binary assets from their text sources and compares them byte-for-byte
  before compiling and publishing the three-file PSP artifact.

## PPSSPP runtime validation (2026-10-29)

- The CI artifact from commit `1cd1e20b` starts in PPSSPP 1.20.4 and remains in
  the render loop without a bad-address exception.
- The first runtime attempt exposed incorrect PSP thread argument handling:
  `sceKernelStartThread` copies an argument buffer instead of passing `this`
  directly. `TThread_PSP` now starts with a copied `TThread*`, validates the
  entry buffer, and dereferences it before the virtual `Main` call.
- The success clear colour is visible. That state is gated by the complete M1
  portability result plus texture, indexed mesh, external PMES model, and named
  resource-cache readiness, so all bootstrap gates passed in the emulator.
- The swizzled checker texture is visible on the rotating octahedron with depth
  and backface culling, and the independent indexed 2D geometry also renders.
- PPSSPP maps the analog stick left/right to `J`/`L`; repeated left input moved
  the triangle and texture sprite while the model continued rotating. The raw
  `sceCtrl` input probe is therefore runtime-confirmed.
- The CI artifact from commit `0c3bc623` replaces the frontend's raw controller
  polling with `TInputInterface_PSP` and `TInputDeviceController_PSP`. It starts
  in PPSSPP 1.20.4, renders the complete probe, and repeated analog-left input
  moves and rotates the probe geometry through the engine-level controller API.
- The PSP device maps the D-pad, Start, Select, four face buttons, both shoulder
  buttons, and the platform's single analog stick to existing Toshi doodads.
  A zero-sample first poll preserves the neutral stick state.
- Real PSP validation, sustained memory profiling, vibration policy, and final
  game action bindings remain outstanding.

### M3 implementation notes

- `TScheduler::Update(TFLOAT)` accepts an explicit platform delta while the
  original parameterless desktop path remains available. PSP feeds it from the
  existing `THPTimer_PSP` instead of introducing a second timing API.
- `AFrontendBootstrapTask_PSP` is created through the original `TClass` and
  `TTask::Create` path. It owns the probe movement, model rotation, confirm
  state, and frame count; the PSP main loop now only pumps input, updates the
  scheduler, and renders task state.
- The PSP CI artifact from commit `a3ef92e8` starts in PPSSPP 1.20.4 with the
  success bootstrap gate. The rotating model proves scheduler updates are
  running, and repeated analog-left input changes the task-owned render offset.
- `AApplication_PSP` now mirrors the original application's create, update, and
  destroy boundary without pulling the Win32 debug console or system manager
  into the PSP link. It owns a shared frontend state and creates three explicit
  scheduler phases in retail order: input, update, then render.
- The CI artifact from commit `c2f308d4` (PSP run `41180933944`) was launched
  in PPSSPP 1.20.4. The complete probe renders and continues animating through
  the new application lifecycle; the validated bundle is stored under
  `Runtime/PSP/c2f308d4`.
- `AGameStateController_PSP` preserves the original controller naming and adds
  explicit `Boot`, `Menu`, `Probe`, and `Error` states. Probe update and render
  work is gated by the active state instead of running unconditionally.
- `AGameState_PSP` provides the PSP state-object seam with insertion, removal,
  update, clear-colour, and render hooks. `AFrontEndMainMenuState2_PSP` and
  `AProbeState_PSP` now own their screen-specific behaviour; the controller only
  commits requested transitions.
- `AInputHandler_PSP` is the input phase and maps the Toshi PSP controller to
  frontend `Confirm`, `Cancel`, and `Pause` actions plus the movement axis.
  Frontend states no longer depend on physical PSP button doodads.
- The artifact from commit `54216dcd` (PSP run `41181540622`) was validated in
  PPSSPP 1.20.4. It boots to the menu checker, Cross transitions to the animated
  probe, and Circle returns to the menu. The bundle is stored under
  `Runtime/PSP/54216dcd`.
- `T2GUIFont_PSP` adds a fixed, allocation-free 5x7 bootstrap font using Toshi
  scalar types and the PSP render interface's colour-rectangle primitive. It is
  intentionally a bring-up font rather than a replacement for the real
  `T2GUIFont` resource pipeline. The main-menu state now labels itself and shows
  a Cross prompt without loading another runtime asset.
- Commit `ba11d36c` passed PSP run `41196369218`; Premake generation, converted
  asset verification, compilation, EBOOT packaging, and artifact publication
  all succeeded. The bundle is stored under `Runtime/PSP/ba11d36c`.
- `RunPPSSPP.ps1` owns one recorded emulator PID, reuses it, supports background
  PSP input, and forces PPSSPP's software backend only for automated runs. This
  avoids Vulkan/D3D fallback dialogs and leaves the user's saved emulator
  backend untouched. The current bundle stays alive through a background smoke
  cycle; a visual check of the new bootstrap glyphs is still pending.
- Audio and movies remain disabled and no full `OpenBarnyard/Source` glob has
  been added. Scheduler, task, and frontend sources remain dependency-ordered
  in Premake.

## Immediate next slice

Run the validated bundle on real hardware and capture the debug output and
memory high-water marks. In parallel, replace the bootstrap glyph table with
the smallest asset-backed font seam that can feed `T2GUIFont_PSP`, then add a
bounded frontend menu-item model and selection state. Keep audio and movies
disabled and retain the dependency-ordered source list; do not add the full
`OpenBarnyard/Source` glob. Keep the bounded factories/cache as the resource
seam until a real game caller requires a specific part of the full
`TTextureFactory`/`TResource` graph.
