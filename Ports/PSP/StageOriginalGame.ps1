[CmdletBinding()]
param(
    [string] $BuildDirectory = '',
    [string] $InstallDirectory = '',
    [string] $FrontendDirectory = '',
    [string] $FrontendTerrainDirectory = '',
    [string] $FrontendTerrainManifestPath = '',
    [string] $FrontendTerrainPackDirectory = '',
    [string] $FrontendCameraDirectory = '',
    [string] $FrontendBarnSignDirectory = '',
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
if (-not $FrontendTerrainPackDirectory) {
    $FrontendTerrainPackDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendTerrainPack'
}
if (-not $FrontendCameraDirectory) {
    $FrontendCameraDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendCamera'
}
if (-not $FrontendBarnSignDirectory) {
    $FrontendBarnSignDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendBarnSign'
}
if (-not $RuntimeDirectory) {
    $RuntimeDirectory = Join-Path $repositoryRoot 'Runtime\OpenBarnyardPSPGame'
}

$buildPath = [System.IO.Path]::GetFullPath($BuildDirectory)
$installPath = [System.IO.Path]::GetFullPath($InstallDirectory)
$frontendPath = [System.IO.Path]::GetFullPath($FrontendDirectory)
$frontendTerrainPath = [System.IO.Path]::GetFullPath($FrontendTerrainDirectory)
$frontendTerrainManifestPath = [System.IO.Path]::GetFullPath($FrontendTerrainManifestPath)
$frontendTerrainPackPath = [System.IO.Path]::GetFullPath($FrontendTerrainPackDirectory)
$frontendCameraPath = [System.IO.Path]::GetFullPath($FrontendCameraDirectory)
$frontendBarnSignPath = [System.IO.Path]::GetFullPath($FrontendBarnSignDirectory)
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

    $terrainPackConversionArguments = @{ OutputDirectory = $frontendTerrainPackPath }
    if ($TMDLPath) { $terrainPackConversionArguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $terrainPackConversionArguments.PythonPath = $PythonPath }
    & (Join-Path $PSScriptRoot 'Tools\ConvertRetailFrontendDetail.ps1') @terrainPackConversionArguments

    $cameraConversionArguments = @{ OutputDirectory = $frontendCameraPath }
    if ($TMDLPath) { $cameraConversionArguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $cameraConversionArguments.PythonPath = $PythonPath }
    & (Join-Path $PSScriptRoot 'Tools\ConvertRetailFrontendCamera.ps1') @cameraConversionArguments

    $barnSignConversionArguments = @{ OutputDirectory = $frontendBarnSignPath }
    if ($TMDLPath) { $barnSignConversionArguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $barnSignConversionArguments.PythonPath = $PythonPath }
    & (Join-Path $PSScriptRoot 'Tools\ConvertRetailBarnSign.ps1') @barnSignConversionArguments
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

$frontendTerrainManifest = Join-Path $frontendTerrainPath 'barnyard_terrain.json'
if (-not (Test-Path -LiteralPath $frontendTerrainManifest -PathType Leaf)) {
    throw "Converted retail front-end terrain manifest was not found: $frontendTerrainManifest"
}
$frontendTerrain = Get-Content -LiteralPath $frontendTerrainManifest -Raw | ConvertFrom-Json
if ($frontendTerrain.version -ne 1 -or $frontendTerrain.meshes.Count -lt 1 -or $frontendTerrain.meshes.Count -gt 16) {
    throw "Converted retail front-end terrain manifest is invalid: $frontendTerrainManifest"
}
$frontendTerrainFiles = @('barnyard_terrain.pcfg')
for ($meshIndex = 0; $meshIndex -lt $frontendTerrain.meshes.Count; $meshIndex++) {
    $expectedMesh = "barnyard_terrain_$meshIndex.pmes"
    $expectedTexture = "barnyard_terrain_$meshIndex.ptex"
    if ($frontendTerrain.meshes[$meshIndex].mesh -ne $expectedMesh -or
        $frontendTerrain.meshes[$meshIndex].texture -ne $expectedTexture) {
        throw "Converted retail front-end terrain manifest has an invalid mesh $meshIndex binding"
    }
    $frontendTerrainFiles += $expectedMesh,$expectedTexture
}
foreach ($fileName in $frontendTerrainFiles) {
    $sourcePath = Join-Path $frontendTerrainPath $fileName
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Converted retail frontend terrain asset was not found: $sourcePath"
    }
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $runtimePath $fileName) -Force
}

$frontendTerrainPackManifest = Join-Path $frontendTerrainPackPath 'frontend_detail.json'
if (-not (Test-Path -LiteralPath $frontendTerrainPackManifest -PathType Leaf)) {
    throw "Converted retail front-end detail manifest was not found: $frontendTerrainPackManifest"
}
$frontendTerrainPack = Get-Content -LiteralPath $frontendTerrainPackManifest -Raw | ConvertFrom-Json
if ($frontendTerrainPack.version -ne 1 -or $frontendTerrainPack.source_meshes -ne 94 -or
    $frontendTerrainPack.models.Count -lt 1 -or $frontendTerrainPack.models.Count -gt 48 -or
    $frontendTerrainPack.textures.Count -lt 1 -or $frontendTerrainPack.textures.Count -gt 48) {
    throw "Converted retail front-end detail manifest is invalid: $frontendTerrainPackManifest"
}
$frontendTerrainPackFiles = @('frontend_detail.pcfg')
for ($modelIndex = 0; $modelIndex -lt $frontendTerrainPack.models.Count; $modelIndex++) {
    $expectedModel = "frontend_detail_model_$modelIndex.pmes"
    if ($frontendTerrainPack.models[$modelIndex].mesh -ne $expectedModel) {
        throw "Converted retail front-end detail manifest has an invalid model $modelIndex binding"
    }
    $frontendTerrainPackFiles += $expectedModel
}
for ($textureIndex = 0; $textureIndex -lt $frontendTerrainPack.textures.Count; $textureIndex++) {
    $expectedTexture = "frontend_detail_texture_$textureIndex.ptex"
    if ($frontendTerrainPack.textures[$textureIndex].texture -ne $expectedTexture) {
        throw "Converted retail front-end detail manifest has an invalid texture $textureIndex binding"
    }
    $frontendTerrainPackFiles += $expectedTexture
}
foreach ($fileName in $frontendTerrainPackFiles) {
    $sourcePath = Join-Path $frontendTerrainPackPath $fileName
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Converted retail front-end detail asset was not found: $sourcePath"
    }
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $runtimePath $fileName) -Force
}

$frontendCameraSource = Join-Path $frontendCameraPath 'frontend_camera.pcam'
if (-not (Test-Path -LiteralPath $frontendCameraSource -PathType Leaf)) {
    throw "Converted retail frontend camera was not found: $frontendCameraSource"
}
Copy-Item -LiteralPath $frontendCameraSource -Destination (Join-Path $runtimePath 'frontend_camera.pcam') -Force

foreach ($meshIndex in 0..1) {
    foreach ($extension in @('pmes', 'ptex')) {
        $fileName = "frontend_barnsign_$meshIndex.$extension"
        $sourcePath = Join-Path $frontendBarnSignPath $fileName
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
            throw "Converted retail front-end barn-sign asset was not found: $sourcePath"
        }
        Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $runtimePath $fileName) -Force
    }
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
