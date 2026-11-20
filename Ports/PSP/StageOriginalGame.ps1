[CmdletBinding()]
param(
    [string] $BuildDirectory = '',
    [string] $InstallDirectory = '',
    [string] $FrontendDirectory = '',
    [string] $FrontendTerrainDirectory = '',
    [string] $FrontendTerrainManifestPath = '',
    [string] $RuntimeDirectory = '',
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [switch] $SkipConversion
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $repositoryRoot 'bin\PSP\Release'
}
if (-not $InstallDirectory) {
    $InstallDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Install'
}
if (-not $FrontendDirectory) {
    $FrontendDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\Frontend'
}
if (-not $FrontendTerrainDirectory) {
    $FrontendTerrainDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendTerrain'
}
if (-not $FrontendTerrainManifestPath) {
    $FrontendTerrainManifestPath = Join-Path $PSScriptRoot 'Assets\original_frontend_terrain.txt'
}
if (-not $RuntimeDirectory) {
    $RuntimeDirectory = Join-Path $repositoryRoot 'Runtime\OpenBarnyardPSPGame'
}

$buildPath = [System.IO.Path]::GetFullPath($BuildDirectory)
$installPath = [System.IO.Path]::GetFullPath($InstallDirectory)
$frontendPath = [System.IO.Path]::GetFullPath($FrontendDirectory)
$frontendTerrainPath = [System.IO.Path]::GetFullPath($FrontendTerrainDirectory)
$frontendTerrainManifestPath = [System.IO.Path]::GetFullPath($FrontendTerrainManifestPath)
$runtimePath = [System.IO.Path]::GetFullPath($RuntimeDirectory)
$gamePbpPath = Join-Path $buildPath 'EBOOT_GAME.PBP'
$gameSfoPath = Join-Path $buildPath 'PARAM_GAME.SFO'

foreach ($requiredFile in @($gamePbpPath, $gameSfoPath)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Original PSP game build output was not found: $requiredFile"
    }
}

$retailFiles = @(
    @{ Source = 'Data\Options\Controls_Win.trb'; Destination = 'Data\Options\Controls_Win.trb' },
    @{ Source = 'Data\GUI\fonts.trb'; Destination = 'Data\GUI\fonts.trb' },
    @{ Source = 'Data\GUI\texsec.trb'; Destination = 'Data\GUI\texsec.trb' },
    @{ Source = 'Data\Locale\eng-uk.trb'; Destination = 'Data\Locale\eng-uk.trb' }
)

foreach ($retailFile in $retailFiles) {
    $sourcePath = Join-Path $installPath $retailFile.Source
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Retail runtime file was not found: $sourcePath"
    }
}

New-Item -ItemType Directory -Force -Path $runtimePath | Out-Null

if (-not $SkipConversion) {
    $conversionArguments = @{
        OutputDirectory = $frontendPath
        RuntimeDirectory = $runtimePath
    }
    if ($TMDLPath) { $conversionArguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $conversionArguments.PythonPath = $PythonPath }

    & (Join-Path $PSScriptRoot 'Tools\ConvertRetailFrontend.ps1') @conversionArguments

    $terrainConversionArguments = @{
        ManifestPath = $frontendTerrainManifestPath
        OutputDirectory = $frontendTerrainPath
    }
    if ($TMDLPath) { $terrainConversionArguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $terrainConversionArguments.PythonPath = $PythonPath }

    & (Join-Path $PSScriptRoot 'Tools\ConvertRetailTerrain.ps1') @terrainConversionArguments
}

$frontendFiles = @(
    'barnyard_logo.ptex',
    'Rekord_Black_26.ptex',
    'Panel_01.ptex',
    'Border_Top_Left.ptex',
    'Border_Top_Right.ptex',
    'Border_Horizontal.ptex',
    'Border_Vertical.ptex',
    'Border_MIddle.ptex',
    'Border_Bottom_Left.ptex',
    'Border_Bottom_Right.ptex'
)
foreach ($picture in @('bkg_by_legal1', 'bkg_Bink_Big')) {
    foreach ($tile in 1..6) {
        $frontendFiles += "${picture}_${tile}.ptex"
    }
}

foreach ($fileName in $frontendFiles) {
    $sourcePath = Join-Path $frontendPath $fileName
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Converted retail frontend asset was not found: $sourcePath"
    }
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $runtimePath $fileName) -Force
}

$frontendTerrainFiles = foreach ($meshIndex in 0..3) {
    "barnyard_terrain_$meshIndex.pmes"
    "barnyard_terrain_$meshIndex.ptex"
}
foreach ($fileName in $frontendTerrainFiles) {
    $sourcePath = Join-Path $frontendTerrainPath $fileName
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Converted retail frontend terrain asset was not found: $sourcePath"
    }
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $runtimePath $fileName) -Force
}

foreach ($retailFile in $retailFiles) {
    $sourcePath = Join-Path $installPath $retailFile.Source
    $destinationPath = Join-Path $runtimePath $retailFile.Destination
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destinationPath) | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
}

Copy-Item -LiteralPath $gamePbpPath -Destination (Join-Path $runtimePath 'EBOOT.PBP') -Force
Copy-Item -LiteralPath $gameSfoPath -Destination (Join-Path $runtimePath 'PARAM.SFO') -Force

Write-Output "Staged original Barnyard PSP runtime at $runtimePath"
Write-Output "Run it with: .\Ports\PSP\RunPPSSPP.ps1 -Action Restart -OriginalGame"
