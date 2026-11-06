param(
    [string] $ManifestPath = '',
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $OutputDirectory = '',
    [string] $RuntimeDirectory = '',
    [switch] $KeepIntermediate
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$assetRoot = Join-Path $repositoryRoot 'Game\BarnyardPC'
if (-not $ManifestPath) {
    $ManifestPath = Join-Path $assetRoot 'Manifests\first_terrain.txt'
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $assetRoot 'Workspace\Terrain'
}

function Resolve-AssetPath([string] $Value) {
    if ([System.IO.Path]::IsPathRooted($Value)) {
        return [System.IO.Path]::GetFullPath($Value)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $assetRoot $Value))
}

if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "Retail terrain manifest was not found: $ManifestPath"
}

$manifest = @{}
foreach ($rawLine in Get-Content -LiteralPath $ManifestPath) {
    $line = $rawLine.Trim()
    if (-not $line -or $line.StartsWith('#')) {
        continue
    }
    $pair = $line.Split('=', 2)
    if ($pair.Count -ne 2 -or -not $pair[0].Trim()) {
        throw "Invalid manifest line: $rawLine"
    }
    $manifest[$pair[0].Trim()] = $pair[1].Trim()
}

if (-not $manifest.ContainsKey('terrain') -or -not $manifest.terrain) {
    throw "Retail terrain manifest requires 'terrain'"
}
$terrain = Resolve-AssetPath $manifest.terrain
if (-not (Test-Path -LiteralPath $terrain -PathType Leaf)) {
    throw "Retail terrain file was not found: $terrain"
}

if (-not $TMDLPath) {
    $TMDLPath = Get-ChildItem (Join-Path $assetRoot 'Tools') -Filter 'TMDL.exe' -Recurse -File |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $TMDLPath -or -not (Test-Path -LiteralPath $TMDLPath -PathType Leaf)) {
    throw 'TMDL.exe was not found; pass -TMDLPath or download the OpenBarnyard-PSP-host-tools artifact'
}
$TMDLPath = [System.IO.Path]::GetFullPath($TMDLPath)

$pythonArguments = @()
if (-not $PythonPath) {
    $launcher = Get-Command py -ErrorAction SilentlyContinue
    if ($launcher) {
        $PythonPath = $launcher.Source
        $pythonArguments = @('-3')
    }
    foreach ($name in @('python3', 'python')) {
        if ($PythonPath) {
            break
        }
        $candidate = Get-Command $name -ErrorAction SilentlyContinue
        if ($candidate -and $candidate.Source -notlike '*\WindowsApps\python*.exe') {
            $PythonPath = $candidate.Source
            break
        }
    }
    if (-not $PythonPath) {
        $codexPython = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
        if (Test-Path -LiteralPath $codexPython -PathType Leaf) {
            $PythonPath = $codexPython
        }
    }
}
if (-not $PythonPath -or -not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) {
    throw 'Python 3 was not found; pass -PythonPath explicitly'
}

$world = if ($manifest.world) { [int] $manifest.world } else { 0 }
$cell = if ($manifest.cell) { [int] $manifest.cell } else { 0 }
$mesh = if ($manifest.mesh) { [int] $manifest.mesh } else { 0 }
if ($world -lt 0 -or $cell -lt 0 -or $mesh -lt 0) {
    throw 'world, cell, and mesh must be non-negative'
}

$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
$dumpPath = Join-Path $assetRoot 'Workspace\Dumps\barnyard_terrain.json'
$meshPath = Join-Path $outputPath 'barnyard_terrain.pmes'
$ddsPath = Join-Path $assetRoot 'Workspace\Dumps\barnyard_terrain.dds'
$texturePath = Join-Path $outputPath 'barnyard_terrain.ptex'
New-Item -ItemType Directory -Force -Path $outputPath,(Split-Path -Parent $dumpPath) | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path (Split-Path -Parent $TMDLPath) 'Logs') | Out-Null

Push-Location (Split-Path -Parent $TMDLPath)
try {
    & $TMDLPath -psp-terrain $terrain -world $world -cell $cell -mesh $mesh -o $dumpPath
    if ($LASTEXITCODE -ne 0) {
        throw "Retail terrain export failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

& $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'convert_terrain_dump.py') $dumpPath $meshPath
if ($LASTEXITCODE -ne 0) {
    throw "Retail terrain conversion failed with exit code $LASTEXITCODE"
}

$dump = Get-Content -LiteralPath $dumpPath -Raw | ConvertFrom-Json
if ($dump.texture) {
    $libraries = [System.Collections.Generic.List[string]]::new()
    if ($manifest.texture_libraries) {
        foreach ($value in $manifest.texture_libraries.Split(';')) {
            if ($value.Trim()) {
                $libraries.Add((Resolve-AssetPath $value.Trim()))
            }
        }
    }
    else {
        $derivedLibrary = $terrain -replace 'Mod\d+\.trb$', 'Mat.ttl'
        if (Test-Path -LiteralPath $derivedLibrary -PathType Leaf) {
            $libraries.Add($derivedLibrary)
        }
        Get-ChildItem (Split-Path -Parent $terrain) -Filter '*_Common.ttl' -File |
            ForEach-Object { $libraries.Add($_.FullName) }
    }
    if ($libraries.Count -eq 0) {
        throw "No texture libraries were supplied or derived for '$($dump.texture)'"
    }

    $textureFound = $false
    foreach ($library in $libraries) {
        if (-not (Test-Path -LiteralPath $library -PathType Leaf)) {
            throw "Retail texture library was not found: $library"
        }
        Push-Location (Split-Path -Parent $TMDLPath)
        try {
            & $TMDLPath -psp-texture $library -texture $dump.texture -o $ddsPath
            $textureFound = $LASTEXITCODE -eq 0
        }
        finally {
            Pop-Location
        }
        if ($textureFound) {
            break
        }
    }
    if (-not $textureFound) {
        throw "Texture '$($dump.texture)' was not found in: $($libraries -join ', ')"
    }
    & $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'convert_dds.py') $ddsPath $texturePath
    if ($LASTEXITCODE -ne 0) {
        throw "Retail terrain texture conversion failed with exit code $LASTEXITCODE"
    }
}

if ($RuntimeDirectory) {
    $runtimePath = [System.IO.Path]::GetFullPath($RuntimeDirectory)
    New-Item -ItemType Directory -Force -Path $runtimePath | Out-Null
    Get-ChildItem $outputPath -Filter 'barnyard_terrain.*' -File |
        Copy-Item -Destination $runtimePath -Force
    Write-Output "Published retail terrain to $runtimePath"
}

if (-not $KeepIntermediate) {
    Remove-Item -LiteralPath $dumpPath,$ddsPath -Force -ErrorAction SilentlyContinue
}
