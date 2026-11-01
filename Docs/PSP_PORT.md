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

## Progress snapshot (2026-11-01)

The PSP platform/render bring-up is approximately **89%** complete, while the
complete playable-game port is approximately **43%** complete. The second value
is intentionally much lower: a compiling probe does not yet include the real
game states, Barnyard asset conversion, animation, complete audio, UI, world
streaming, or gameplay validation on hardware.

| Area | Estimate | Current boundary |
| --- | ---: | --- |
| PSP toolchain and packaging | 95% | Premake, EBOOT, deterministic assets, CI artifacts and a PPSSPP bundle work |
| Portable Toshi/platform core | 83% | allocator, strings, containers, file I/O, timer, sync, threads, input events and scheduler tasks pass the PPSSPP bootstrap gate |
| PSP renderer foundation | 56% | GU display, depth, indexed static models, textures, materials, alpha-blended atlas fonts, budgets and bounded factories build for PSP |
| Real-game asset pipeline | 27% | deterministic converters produce PTEX, PMES, PFNT, PLOC, PPCM, PADP and PBNK runtime assets |
| Input integration | 58% | `AInputHandler_PSP` maps PSP controls to frontend confirm, cancel, pause, navigation and movement actions |
| Game bootstrap and frontend | 31% | `AApplication_PSP` owns explicit frame phases and cached font/locale resources label the menu/probe/info flow and live budget diagnostics |
| Audio, movies and saves | 58% | bounded IMA ADPCM/PCM prefetch, page-aligned loops, underrun telemetry, PBNK manifests, spatial controls, mixer and audio output work; movies and saves are not started |
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
- The main-menu state owns a fixed two-item selection model with no dynamic
  allocation. D-pad Up/Down selects `START PROBE` or `PORT STATUS`, while Cross
  requests the corresponding state through the existing controller boundary.
- `AFrontendInfoState_PSP` is the second real consumer of the state-object seam.
  It renders bounded core/render readiness text and returns to the menu through
  the frontend Cancel action instead of reading PSP doodads directly.
- Commit `d2cedb77` passed PSP run `41196694395`, including Premake generation,
  the new frontend/input compilation, EBOOT packaging, and artifact publication.
- The bootstrap font is now a deterministic 64x32 alpha atlas generated offline
  and converted to swizzled `bootstrap_font.ptex`. `T2GUIFont_PSP` keeps only the
  compact character map and submits one alpha-blended texture region per glyph
  instead of emitting a GU rectangle for every lit pixel.
- The font atlas is loaded through `TTextureFactory_PSP`, registered in the
  bounded resource cache, required by the bootstrap gate, and released through
  the existing texture lifecycle. The menu checker remains a Menu-state marker;
  all 3D model/mesh probes remain available in the separate Probe state.
- Commit `a119bd18` passed PSP run `41196991679`. The published bundle contains
  `EBOOT.PBP`, `probe.ptex`, `probe.pmes`, and `bootstrap_font.ptex` and is stored
  under `Runtime/PSP/a119bd18`.
- `PFNT` version 1 stores a fixed 40-byte header followed by 8-byte glyph
  records. Atlas dimensions, cell/glyph metrics, advance, code points and source
  positions are serialized with fixed-width Toshi-compatible fields; malformed
  sizes, duplicate code points and out-of-bounds glyphs are rejected.
- `T2GUIFontFactory_PSP` bounds font metadata to 64 glyphs, releases its temporary
  file buffer after creation, and leaves `T2GUIFont_PSP` with a fixed internal
  record array. The font and its atlas are separate named entries in the bounded
  PSP render-resource cache.
- Commit `2bbb2a16` passed PSP run `41197415596`. The five-file bundle adds the
  264-byte `bootstrap_font.pfnt` and is stored under `Runtime/PSP/2bbb2a16`.
- `PLOC` version 1 stores a fixed-width header, `string_count + 1` offsets, and
  a UTF-16LE string table. `T2Locale_PSP` validates the exact file size, bounded
  counts, monotonic string ranges, terminators and surrogate pairs before
  copying text into fixed internal arrays; no source blob remains resident.
- `T2LocaleFactory_PSP` bounds transient locale input and uses the existing
  Toshi allocator and file layer. Frontend states address the seven bootstrap
  labels through `AFrontendLocaleString_PSP` IDs instead of embedding their
  user-facing wide strings.
- Commit `9e78933f` passed PSP run `41197984217`. Its six-file bundle adds the
  230-byte `bootstrap_locale.ploc` and is stored under
  `Runtime/PSP/9e78933f`.
- `ALocaleManager_PSP` mirrors the original manager's language, filename and
  string-access vocabulary while owning the bounded PLOC resource. The current
  bootstrap deliberately supports only English UK; unsupported language IDs
  fail before attempting file I/O.
- The offline bootstrap font now contains digits and diagnostic punctuation in
  a 64x64 atlas. The port-status state uses those glyphs to render its live
  scheduler frame count through a fixed stack buffer and Toshi scalar types.
- Commit `8428c4c3` passed PSP run `41198218249`. Its bundle contains the
  expanded 16,408-byte font texture, 368-byte PFNT metadata and 248-byte PLOC
  table and is stored under `Runtime/PSP/8428c4c3`.
- The port-status state now reads `TRenderMemory_PSP` directly and renders main
  RAM and eDRAM current/budget values in KiB. Its fixed-capacity integer
  formatter uses `TWCHAR`, `TUINT` and `TSIZE` only and performs no allocation
  during a frame.
- Commit `3ef30fdc` passed PSP run `41218936337`. The resulting bundle carries
  the 290-byte PLOC table with memory labels and is stored under
  `Runtime/PSP/3ef30fdc`.
- `APortReport_PSP` writes `openbarnyard_psp.log` beside the EBOOT at bootstrap
  and clean exit. The bounded report records every readiness gate, frame count,
  main-memory current/peak/budget and eDRAM current/budget through the existing
  PSP `TFile` implementation.
- Commit `5c3a11ac` passed PSP run `41219161474` and was launched through the
  single-instance background runner. Its `RUNNING` report recorded every gate
  as `OK`, 17,676 bytes of tracked main render memory against 16 MiB, and
  1,392,640 bytes of eDRAM against 2 MiB. The bundle is stored under
  `Runtime/PSP/5c3a11ac`.
- The background PPSSPP runner now sends D-pad Up/Down in addition to face and
  analog inputs. Menu-to-status navigation and Circle return were exercised on
  the same emulator process without taking keyboard focus.
- `ASoundManager_PSP` reserves one native PSPSDK stereo channel at 44.1 kHz and
  owns two 1,024-frame, 64-byte-aligned output buffers. The bootstrap submits a
  silent buffer through the blocking hardware API before entering the frame
  loop; FMOD remains excluded from the PSP executable.
- Commit `c31bd9fd` passed PSP run `41219699089` and was runtime-validated in
  PPSSPP. The report records `audio=OK`, all other gates remain `OK`, and the
  updated bundle is stored under `Runtime/PSP/c31bd9fd`.
- Audio output now runs on `ASoundOutputThread_PSP` instead of blocking the main
  frame loop. Two fixed buffers are coordinated by Toshi semaphores for free,
  ready, completed and stopped states; shutdown wakes and joins the worker
  without polling or runtime allocation.
- `PPCM` version 1 stores a 20-byte fixed-width header followed by signed
  interleaved stereo samples. `AWave_PSP` accepts only 44.1 kHz, two-channel,
  bounded 1,024-frame data, while `AWaveFactory_PSP` releases its temporary
  source allocation immediately after copying into aligned storage.
- Commit `e343449d` passed PSP run `41220300179`. Its seven-file bundle includes
  the deterministic 4,116-byte `bootstrap_audio.ppcm`; PPSSPP loaded and played
  it through the worker, retained `audio=OK`, and continued processing frontend
  input. The diagnostic impulse was audibly confirmed by the user, validating
  the path through PPCM loading, the worker queue and PPSSPP audio output. The
  bundle is stored under `Runtime/PSP/e343449d`.
- `ASoundMixer_PSP` owns four fixed one-shot voices and one aligned stereo mix
  buffer. Per-voice integer volume, 32-bit accumulation and signed 16-bit
  saturation avoid floating-point work and allocation in the mixing path.
- Commit `d6ca12d5` passed PSP run `41220458297` and retained `audio=OK` in
  PPSSPP after routing the bootstrap PPCM through the mixer before the output
  worker. The bundle is stored under `Runtime/PSP/d6ca12d5`.
- Mixer voices now support linear left/right pan and optional looping with
  integer volume math. `ASoundUpdateTask_PSP` runs between frontend update and
  render, uses `TryLockOutputBuffer` to avoid stalling the frame, and writes
  mixed samples directly into the worker's next free aligned buffer.
- Commit `1c82ef19` passed PSP run `41220729797`. Its bundle remained responsive
  through background menu navigation while the scheduler continuously queued
  audio, and the runtime report retained `audio=OK` and `scheduler=OK`. The
  bundle is stored under `Runtime/PSP/1c82ef19`.
- `ASoundVoiceHandle_PSP` combines a fixed voice-slot index with a 24-bit
  generation. Stop, volume, pan and active queries reject completed, stopped or
  recycled handles, preventing a stale game command from mutating a newer sound
  that reused the same one of four slots.
- Commit `e3e17bc5` passed PSP run `41220914299`. Bootstrap validation stops a
  looping voice, rejects its stale handle, changes live volume and pan on a new
  voice, then confirms that completion invalidates that handle. PPSSPP retained
  `audio=OK` and responsive frontend input; the bundle is stored under
  `Runtime/PSP/e3e17bc5`.
- `ASoundCommandQueue_PSP` stores up to 16 `Play`, `Stop`, `SetVolume` and
  `SetPan` commands plus 16 bounded asynchronous play results. Play requests use
  monotonically increasing request IDs; queue overflow, exhausted result slots
  and rejected stale voice handles are observable without allocating memory.
- The scheduler processes commands before acquiring the next audio buffer, so
  gameplay-facing controls take effect at a deterministic audio-frame boundary.
  Commit `f9885488` passed PSP run `41221566904`; its bootstrap validates the
  full command/result/control/stale-rejection sequence and the PPSSPP bundle
  retained `audio=OK`. It is stored under `Runtime/PSP/f9885488`.
- `AWaveBank_PSP` provides a non-owning fixed registry of 16 created waves with
  47-character names and case-insensitive lookup. `QueuePlayNamed` resolves the
  name before entering the 16-command ring, so no string copy, lookup or heap
  allocation reaches the scheduler's mixing phase.
- Commit `937c0e5a` passed PSP run `41221818843`. The bootstrap registers the
  PPCM wave as `bootstrap`, resolves `BOOTSTRAP`, rejects a duplicate name and
  drives the existing asynchronous play/control sequence through the named
  path. The single background PPSSPP instance retained every readiness gate as
  `OK`; its bundle and report are stored under `Runtime/PSP/937c0e5a`.
- `ASoundEvent_PSP` exposes the gameplay-facing `PlayAudio`, `StopAudio`,
  `UpdateChannelVolume` and `UpdateChannelPan` vocabulary. The adapter resolves
  named play events and translates controls into the existing bounded command
  queue; unsupported or missing-wave events never reach the mixer.
- Commit `76e07162` passed PSP run `41222040345`. Its bootstrap validates a
  rejected missing-wave event followed by named play, volume, pan, stop and
  stale-handle rejection. The PPSSPP runtime retained every gate as `OK`, with
  17,676 bytes of tracked main render memory and 1,392,640 bytes of eDRAM; the
  bundle is stored under `Runtime/PSP/76e07162`.
- `ASoundDelayedEventQueue_PSP` stores eight fixed event slots. Delays use 44.1
  kHz sample frames and advance only after the
  audio worker exposes a free 1,024-frame output buffer, keeping event time
  aligned with submitted audio instead of fluctuating frontend frame time.
- Named waves are resolved when an event is scheduled and due events retry if
  the command ring is full. Each scheduled handle contains an eight-bit slot
  plus a 24-bit generation and can be cancelled before dispatch. Delayed play
  progresses through scheduled, command-pending and result-ready states; its
  result is exchanged once for the mixer's generation-safe voice handle.
- Commit `b9967e4c` passed PSP run `41222291366`. Its bootstrap validates a
  two-buffer delayed stop, one-shot play, missing-wave and looping-play
  rejection, and pending-result cleanup. PPSSPP retained every readiness gate
  as `OK`; the bundle is stored under `Runtime/PSP/b9967e4c`.
- Commit `84a72771` passed PSP run `41222812317`. The bootstrap reuses a slot
  after cancellation, rejects the stale scheduled handle, dispatches a delayed
  looping play, retrieves its voice exactly once and stops that voice through
  the mixer handle. PPSSPP retained every readiness gate as `OK`; the bundle is
  stored under `Runtime/PSP/84a72771`.
- Mixer voices now keep a 16.16 sample position and precomputed integer step.
  Playback frequency is bounded to 11,025-88,200 Hz and uses nearest-sample
  resampling, avoiding floating-point work and extra buffers in the inner mix
  loop. `UpdateChannelFrequency` is supported by immediate and delayed events.
- Commit `5fb6d11d` passed PSP run `41223741510`. A silent bootstrap voice at
  22,050 Hz remains active after one 1,024-frame output buffer, switches to
  88,200 Hz and completes in the next buffer; invalid and stale frequency
  updates are rejected. PPSSPP retained every readiness gate as `OK`; the
  bundle is stored under `Runtime/PSP/5fb6d11d`.
- `ASoundPosition_PSP` uses bounded integer listener-space coordinates. Position
  updates approximate distance as `max(x,z) + min(x,z)/2 + abs(y)/2`, then
  precompute linear attenuation and stereo pan outside the inner mix loop.
  Base volume remains separate from attenuation, so later volume commands
  preserve spatial falloff.
- Commit `77fe2b15` passed PSP run `41224381594`. The bootstrap verifies exact
  half-right pan and half-volume attenuation, invalid maximum distance, stale
  voice rejection and command-event routing. PPSSPP retained every readiness
  gate as `OK`; the bundle is stored under `Runtime/PSP/77fe2b15`.
- `ASoundListener_PSP` stores a bounded world position plus Q12 horizontal
  right/forward axes. Listener validation checks axis length and orthogonality;
  world-space channel positions are transformed with 64-bit integer dot
  products before entering the existing pan/attenuation calculation.
- Listener changes are commands in the same 16-entry ring as channel position,
  preserving `SetListener` then `UpdateChannelPosition` order at the audio
  phase boundary without sharing mutable float vectors between tasks.
- Commit `c21c47c5` passed PSP run `41224645329`. The bootstrap validates a
  translated, 90-degree rotated listener, rejects a zero basis and checks both
  direct and queued world-position transforms. PPSSPP retained every readiness
  gate as `OK`; the bundle is stored under `Runtime/PSP/c21c47c5`.
- `PBNK` version 1 stores a 12-byte header followed by fixed 48-byte wave names
  and 64-byte PPCM paths. The deterministic generator rejects oversized ASCII
  fields and case-insensitive duplicate names; CI recreates and compares the
  124-byte bootstrap manifest byte-for-byte.
- `AWaveBankLoader_PSP` streams one manifest entry at a time into caller-owned
  `AWave_PSP` storage. It enforces an exact file size and 16-wave ceiling, keeps
  ownership explicit, and rolls back every previously loaded wave if a later
  entry fails without retaining a manifest allocation.
- Commit `663624e8` passed PSP run `41224890013`. The frontend now loads and
  registers `bootstrap_audio.ppcm` only through `bootstrap_audio.pbnk`; the
  artifact contains both files and PPSSPP retained every readiness gate as
  `OK`. The bundle is stored under `Runtime/PSP/663624e8`.
- `AStreamWave_PSP` validates PPCM streams up to five minutes and retains an
  open Toshi file plus two 64-byte-aligned 1,024-frame pages. Each page is
  fetched in one worker read; the mixer performs no file I/O and the 16.16
  cursor uses `TUINT64` to avoid the old 65,535-frame ceiling.
- The stream path remains limited to one 44.1 kHz voice per cursor. Pitch
  changes and duplicate playback are rejected until independent prefetch state
  exists, preventing cache aliasing or an unbounded refill path.
- Commit `1b3d215a` passed PSP run `41225583499`. The bootstrap streams a silent
  3,072-frame probe across three distinct cache pages, verifies lifetime after
  the first two and completion after the third, then rejects its stale handle.
  PPSSPP retained every readiness gate as `OK`; the bundle is stored under
  `Runtime/PSP/1b3d215a`.
- `AStreamWave_PSP` now owns exactly two fixed 1,024-frame pages and one
  low-priority prefetch thread. The first page is read during creation; every
  later read targets the inactive page. `Prepare` only switches to a published
  page under the Toshi mutex and never waits for storage in the mixer path.
- The bounded stream cost is 8 KiB of PCM payload plus an 8 KiB worker stack and
  small synchronization metadata. If storage misses the approximately 23 ms
  page window, the current safe policy stops the voice instead of blocking the
  audio scheduler or allocating a recovery buffer.
- Commit `a943b59f` passed PSP run `41280403051`. Its artifact retained every
  readiness gate as `OK` in PPSSPP, including the three-page stream lifetime
  probe and worker shutdown. The bundle is stored under
  `Runtime/PSP/a943b59f`; the single tracked emulator process was PID `26632`.
- Whole-file stream loops are accepted only when the PCM frame count is aligned
  to the fixed 1,024-frame page. While the final page plays, the worker wraps
  prefetch to page zero; the next mixer buffer therefore switches without a
  storage wait or extra allocation. A missed page stops the voice and increments
  the bounded `stream_underruns` diagnostic.
- Commit `7f2e0614` passed PSP run `41281161509`. Its runtime probe crossed the
  end of the 3,072-frame stream, remained active after wrap, then replayed the
  same cursor as a terminating one-shot. PPSSPP reported `audio=OK` and
  `stream_underruns=0`; the bundle is stored under `Runtime/PSP/7f2e0614` and
  the single tracked emulator process was PID `10080`.
- PADP v1 stores stereo 44.1 kHz IMA ADPCM in fixed 1,024-frame blocks. Each
  channel block has a signed predictor, an 8-bit step index and 4-bit sample
  codes. One compressed block is 1,032 bytes versus 4,096 bytes of decoded PCM;
  the 3,072-frame probe is 3,128 bytes including its header instead of 12,308
  bytes as PPCM.
- The existing prefetch worker reads one PADP block into a fixed 1,032-byte
  staging buffer and decodes directly into the inactive PCM page. No allocation
  or file I/O was added to the mixer path, and PCM/PADP share the same voice,
  loop, page publication and underrun behavior.
- Commit `c5198c61` passed PSP run `41281572454` after the initial compile-only
  scope fix. CI regenerated PADP byte-for-byte before compiling. PPSSPP reported
  `adpcm=OK`, `audio=OK` and `stream_underruns=0`; the bundle is stored under
  `Runtime/PSP/c5198c61` and the single tracked emulator process was PID `17816`.
- FMOD, compressed codecs and movies remain disabled and no full
  `OpenBarnyard/Source` glob has been added. Scheduler, task, and frontend
  sources remain dependency-ordered in Premake.

## Immediate next slice

Run the validated bundle on real hardware and collect `openbarnyard_psp.log`.
In parallel, extend PBNK with explicit resident and streamed entries, then add a
real WAV-to-PADP converter before integrating the full sound manager. Keep one
music stream cursor and resident SFX until independent stream state has a
measured PSP memory budget. Continue bridging the bounded
PFNT/PLOC loaders toward the real
`T2GUIFont` and locale-manager vocabulary. Keep movies disabled and retain the
dependency-ordered source list; do not add the full
`OpenBarnyard/Source` glob. Keep the bounded factories/cache as the resource
seam until a real game caller requires a specific part of the full
`TTextureFactory`/`TResource` graph.
