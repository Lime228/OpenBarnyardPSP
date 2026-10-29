# OpenBarnyard PSP port plan (`new-modcore`)

## Current state

The upstream game is not portable as-is. Its workspace only declares Win32, its
entry point and bootstrap use Win32 APIs, and the complete render backends are
DirectX 8 and DirectX 11. The `Revolution` directory currently contains only a
texture implementation, so it is useful as a reference but is not a complete
console backend.

`Ports/PSP` is milestone 0: a small EBOOT that proves the toolchain, input, and
PSP GU rendering path independently from the Windows executable.

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

- `T2GlobalAllocator` currently uses the PSP C heap through
  `T2Allocator_PSP.cpp`. This deliberately keeps the existing allocator API and
  avoids duplicating the global `TMalloc` entry points owned by `TMemory.cpp`.
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
  into the EBOOT. CI validates the complete M1 binary, but runtime validation on
  PPSSPP and real hardware is still required.

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
  16-byte colour/XYZ vertices, and the engine's 16-bit `TIndexType`; both vertex
  and index counts are capped at 65,535.
- `TModel_PSP` validates the complete blob and every index before allocating its
  vertex/index pools. Once both aligned buffers have been populated and written
  back, the frontend releases the source file allocation instead of retaining a
  duplicate model in the PSP heap.
- `TRenderInterface_PSP::DrawModel` supplies the minimal GUM projection, view,
  and model transforms, clears and enables the 16-bit depth buffer for the 3D
  draw, then restores the depth-test state used by the independent 2D probes.
- Premake places `probe.pmes` beside `EBOOT.PBP` and `probe.ptex`. CI regenerates
  both binary assets from their text sources and compares them byte-for-byte
  before compiling and publishing the three-file PSP artifact.

## Immediate next slice

Run the current EBOOT in PPSSPP and on real hardware, capture debug output, and
verify the M1 core probes, external `probe.ptex` sampling, and the rotating
`probe.pmes` octahedron with correct depth. Keep the dependency-ordered source
list; do not add the full `OpenBarnyard/Source` glob. After runtime validation,
add the smallest textured-model vertex/material state needed to combine the
verified PTEX and PMES paths without introducing the PC renderer graph.
