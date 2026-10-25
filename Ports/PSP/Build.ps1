$ErrorActionPreference = 'Stop'

if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    throw 'WSL is not installed. Follow https://pspdev.github.io/installation/windows.html first.'
}

$linuxPath = (wsl.exe wslpath -a $PSScriptRoot).Trim()
if (-not $linuxPath) {
    throw "WSL could not translate the project path: $PSScriptRoot"
}

wsl.exe bash -lc "cd '$linuxPath' && ./build.sh"
if ($LASTEXITCODE -ne 0) {
    throw "PSP build failed with exit code $LASTEXITCODE"
}
