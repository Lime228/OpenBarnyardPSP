[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $BootstrapEboot,
    [Parameter(Mandatory)] [string] $MoviePath,
    [ValidateRange(5, 120)] [int] $TimeoutSeconds = 45
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$runner = Join-Path $repositoryRoot 'Ports\PSP\RunPPSSPP.ps1'
$bootstrap = Get-Item -LiteralPath $BootstrapEboot
$movie = Get-Item -LiteralPath $MoviePath
if ($bootstrap.PSIsContainer -or $bootstrap.Extension -ine '.pbp' -or $movie.PSIsContainer -or $movie.Extension -ine '.pmvf') {
    throw 'Pass an EBOOT.PBP file and a PMVF movie.'
}
if (Get-Process -Name PPSSPPWindows64, PPSSPPWindows -ErrorAction SilentlyContinue) {
    throw 'An existing PPSSPP process is running.'
}

$stage = Join-Path $repositoryRoot ('Runtime\PSP\MovieProbe\' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage -Force | Out-Null
$eboot = Join-Path $stage 'EBOOT.PBP'
$log = Join-Path $stage 'probe_movie.log'
Copy-Item -LiteralPath $bootstrap.FullName -Destination $eboot
Copy-Item -LiteralPath $movie.FullName -Destination (Join-Path $stage 'probe_movie.pmvf')
$assetExtensions = @('.ptex', '.pmes', '.pskl', '.panm', '.pwld', '.pfnt', '.ploc', '.ppcm', '.pbnk', '.padp')
Get-ChildItem -LiteralPath $bootstrap.DirectoryName -File | Where-Object Extension -in $assetExtensions | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $stage
}

try {
    & $runner -Action Start -EbootPath $eboot
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $report = ''
    do {
        if (Test-Path -LiteralPath $log -PathType Leaf) {
            $report = Get-Content -LiteralPath $log -Raw
            if ($report -match '(?m)^movie_fingerprint=[0-9a-f]{8}\s*$') { break }
        }
        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $deadline)

    if ($report -notmatch '(?m)^movie_fingerprint=[0-9a-f]{8}\s*$') {
        throw "PSP movie report did not finish at $log"
    }
    $report.TrimEnd()
    Write-Output $log
    if ($report -notmatch '(?m)^movie_decode=OK\s*$') {
        throw 'PSP movie decoding failed.'
    }
}
finally {
    & $runner -Action Stop -EbootPath $eboot
}
