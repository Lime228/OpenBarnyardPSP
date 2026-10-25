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
```

Then build from the repository, either from PowerShell:

```powershell
.\Ports\PSP\Build.ps1
```

or directly in the Ubuntu shell:

```sh
./Ports/PSP/build.sh
```

The result is `Ports/PSP/EBOOT.PBP`. Copy it to a folder such as
`PSP/GAME/OpenBarnyardProbe/EBOOT.PBP` on a homebrew-capable PSP, or open it in
PPSSPP. The analog stick moves the triangle; Cross changes the background. A
red background means that the embedded Toshi ABI/endianness smoke test failed.

## Why this target is separate

The current `new-modcore` build scripts and executable are Windows-only. Keeping
the probe independent gives us a continuously buildable target while platform
services are ported one by one. Game data is intentionally not copied or bundled.
