# OpenBarnyard PSP bootstrap

This target is the first executable milestone for the PSP port. It deliberately
does not compile the game yet. It validates the PSPDEV toolchain, 32-bit C++ ABI,
controller input, double buffering, and a basic GU draw call before Toshi is
connected to the platform layer.

## Build on Windows through WSL

Install WSL and PSPDEV using the official PSPDEV Windows guide. In the Ubuntu
shell, make sure these commands succeed:

```sh
psp-config --pspdev-path
psp-g++ --version
premake5 --version
```

Then build from the repository, either from PowerShell:

```powershell
.\Ports\PSP\Build.ps1
```

or directly in the Ubuntu shell:

```sh
./Ports/PSP/build.sh
```

The build script generates the PSP project with
`premake5 gmake --target=psp` and builds its `release_psp` configuration. The
result is `bin/PSP/Release/EBOOT.PBP`. Copy it to a folder such as
`PSP/GAME/OpenBarnyardProbe/EBOOT.PBP` on a homebrew-capable PSP, or open it in
PPSSPP. The analog stick moves the triangle; Cross changes the background. A
red background means that the embedded Toshi ABI/endianness smoke test failed.

## Background PPSSPP runner

For local PPSSPP checks on Windows, use the background runner. It records the
PID of this port's emulator process and reuses it instead of opening duplicate
instances or taking focus on every check:

```powershell
.\Ports\PSP\RunPPSSPP.ps1
.\Ports\PSP\RunPPSSPP.ps1 -Action Status
.\Ports\PSP\RunPPSSPP.ps1 -Action Restart -EbootPath .\Runtime\PSP\0a06d6b5\EBOOT.PBP
.\Ports\PSP\RunPPSSPP.ps1 -Action Press -Key Cross -Repeat 8
.\Ports\PSP\RunPPSSPP.ps1 -Action Stop
```

The runner only stops the PPSSPP process whose PID it created and stored under
the ignored `Runtime/PPSSPP` directory. Other emulator sessions are untouched.
Background input uses window messages and does not activate the emulator window
or take keyboard and mouse focus from the current application. Automated runs
force PPSSPP's software backend for that process only, avoiding driver fallback
dialogs without changing the user's saved graphics backend.

## Why this target is separate

The current `new-modcore` build scripts and executable are Windows-only. Keeping
the probe independent gives us a continuously buildable target while platform
services are ported one by one. Game data is intentionally not copied or bundled.
