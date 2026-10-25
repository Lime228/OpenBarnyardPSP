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

## Immediate next slice

After M0 builds on PSPDEV, introduce compiler portability macros in Toshi and a
`ToshiPSPSmoke` target containing only typedefs, math, containers, logging, timer,
and file I/O. Do not add the full `OpenBarnyard/Source` glob to the PSP build: it
hides dependency order and produces thousands of unrelated Windows errors.
