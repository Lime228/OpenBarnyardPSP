[CmdletBinding()]
param(
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $OutputDirectory = '',
    [string] $RuntimeDirectory = '',
    [switch] $KeepIntermediate
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$assetRoot = Join-Path $repositoryRoot 'Game\BarnyardPC'
$modelPath = Join-Path $assetRoot 'Install\Data\Models\barnsign.trb'
$locatorPath = Join-Path $assetRoot 'Install\Data\Terrain\frontend\frontend_skel.trb'
$textureLibraryPath = Join-Path $assetRoot 'Install\Data\Matlibs\windmillhelper.ttl'
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $assetRoot 'Workspace\FrontendBarnSign'
}

foreach ($requiredFile in @($modelPath, $locatorPath, $textureLibraryPath)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Retail barn-sign input was not found: $requiredFile"
    }
}

if (-not $TMDLPath) {
    $TMDLPath = & (Join-Path $PSScriptRoot 'ResolvePSPHostTool.ps1') `
        -RepositoryRoot $repositoryRoot
}
if (-not $TMDLPath -or -not (Test-Path -LiteralPath $TMDLPath -PathType Leaf)) {
    throw 'TMDL.exe was not found; pass -TMDLPath or download the OpenBarnyard-PSP-host-tools artifact'
}
$TMDLPath = [System.IO.Path]::GetFullPath($TMDLPath)

$pythonArguments = @()
if (-not $PythonPath) {
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
    if (-not $PythonPath) {
        $launcher = Get-Command py -ErrorAction SilentlyContinue
        if ($launcher) {
            $PythonPath = $launcher.Source
            $pythonArguments = @('-3')
        }
    }
}
if (-not $PythonPath -or -not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) {
    throw 'Python 3 was not found; pass -PythonPath explicitly'
}

$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
$dumpDirectory = Join-Path $assetRoot 'Workspace\Dumps'
$modelDumpPath = Join-Path $dumpDirectory 'frontend_barnsign_cell.json'
$locatorDumpPath = Join-Path $dumpDirectory 'frontend_barnsign_locator.json'
New-Item -ItemType Directory -Force -Path $outputPath,$dumpDirectory | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path (Split-Path -Parent $TMDLPath) 'Logs') | Out-Null

Push-Location (Split-Path -Parent $TMDLPath)
try {
    & $TMDLPath -psp-terrain-cell $modelPath -world 0 -cell 0 -o $modelDumpPath
    if ($LASTEXITCODE -ne 0) {
        throw "Retail barn-sign model export failed with exit code $LASTEXITCODE"
    }
    & $TMDLPath -psp-locator $locatorPath -locator 'loc_barnsign' -o $locatorDumpPath
    if ($LASTEXITCODE -ne 0) {
        throw "Retail barn-sign locator export failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

& $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'convert_placed_terrain_cell.py') `
    $modelDumpPath $locatorDumpPath $outputPath `
    --prefix 'frontend_barnsign' --locator-name 'loc_barnsign'
if ($LASTEXITCODE -ne 0) {
    throw "Retail barn-sign conversion failed with exit code $LASTEXITCODE"
}

$manifestPath = Join-Path $outputPath 'frontend_barnsign.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.meshes.Count -ne 2) {
    throw "Retail barn-sign conversion produced $($manifest.meshes.Count) meshes instead of 2"
}

$intermediateTextures = @()
for ($index = 0; $index -lt $manifest.meshes.Count; $index++) {
    $mesh = $manifest.meshes[$index]
    if (-not $mesh.source_texture) {
        throw "Retail barn-sign mesh $index does not declare a source texture"
    }
    $ddsPath = Join-Path $dumpDirectory "frontend_barnsign_$index.dds"
    $texturePath = Join-Path $outputPath $mesh.texture
    $intermediateTextures += $ddsPath
    Push-Location (Split-Path -Parent $TMDLPath)
    try {
        & $TMDLPath -psp-texture $textureLibraryPath -texture $mesh.source_texture -o $ddsPath
        if ($LASTEXITCODE -ne 0) {
            throw "Retail barn-sign texture '$($mesh.source_texture)' export failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
    & $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'convert_dds.py') `
        --max-width 256 $ddsPath $texturePath
    if ($LASTEXITCODE -ne 0) {
        throw "Retail barn-sign texture conversion failed with exit code $LASTEXITCODE"
    }
}

foreach ($index in $manifest.meshes.Count..3) {
    Remove-Item -LiteralPath `
        (Join-Path $outputPath "frontend_barnsign_$index.pmes"), `
        (Join-Path $outputPath "frontend_barnsign_$index.ptex") `
        -Force -ErrorAction SilentlyContinue
}

if ($RuntimeDirectory) {
    $runtimePath = [System.IO.Path]::GetFullPath($RuntimeDirectory)
    New-Item -ItemType Directory -Force -Path $runtimePath | Out-Null
    Get-ChildItem $runtimePath -Filter 'frontend_barnsign_*' -File -ErrorAction SilentlyContinue |
        Remove-Item -Force
    Get-ChildItem $outputPath -Filter 'frontend_barnsign_*.p*' -File |
        Copy-Item -Destination $runtimePath -Force
    Write-Output "Published original front-end barn sign to $runtimePath"
}

if (-not $KeepIntermediate) {
    Remove-Item -LiteralPath $modelDumpPath,$locatorDumpPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $intermediateTextures -Force -ErrorAction SilentlyContinue
}

Write-Output "Converted original front-end barn sign to $outputPath"
