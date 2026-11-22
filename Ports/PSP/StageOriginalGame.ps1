[CmdletBinding()]
param(
    [string] $BuildDirectory = '',
    [string] $InstallDirectory = '',
    [string] $FrontendDirectory = '',
    [string] $FrontendTerrainDirectory = '',
    [string] $FrontendTerrainManifestPath = '',
    [string] $FrontendTerrainPackDirectory = '',
    [string] $FrontendTerrainGrassDirectory = '',
    [string] $FrontendTerrainGlowDirectory = '',
    [string] $FrontendTerrainBarnnorthDirectory = '',
    [string] $FrontendTerrainBGbarneastDirectory = '',
    [string] $FrontendTerrainBGbarnwestDirectory = '',
    [string] $FrontendTerrainHillbaseDirectory = '',
    [string] $FrontendTerrainHilltopDirectory = '',
    [string] $FrontendTerrainMeadowsouthDirectory = '',
    [string] $FrontendInstancesDirectory = '',
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
if (-not $FrontendTerrainGrassDirectory) {
    $FrontendTerrainGrassDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendTerrainGrass'
}
if (-not $FrontendTerrainGlowDirectory) {
    $FrontendTerrainGlowDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendTerrainGlow'
}
if (-not $FrontendTerrainBarnnorthDirectory) {
    $FrontendTerrainBarnnorthDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendTerrainBarnnorth'
}
if (-not $FrontendTerrainBGbarneastDirectory) {
    $FrontendTerrainBGbarneastDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendTerrainBGbarneast'
}
if (-not $FrontendTerrainBGbarnwestDirectory) {
    $FrontendTerrainBGbarnwestDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendTerrainBGbarnwest'
}
if (-not $FrontendTerrainHillbaseDirectory) {
    $FrontendTerrainHillbaseDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendTerrainHillbase'
}
if (-not $FrontendTerrainHilltopDirectory) {
    $FrontendTerrainHilltopDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendTerrainHilltop'
}
if (-not $FrontendTerrainMeadowsouthDirectory) {
    $FrontendTerrainMeadowsouthDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendTerrainMeadowsouth'
}
if (-not $FrontendInstancesDirectory) {
    $FrontendInstancesDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\FrontendInstances'
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
$frontendTerrainGrassPath = [System.IO.Path]::GetFullPath($FrontendTerrainGrassDirectory)
$frontendTerrainGlowPath = [System.IO.Path]::GetFullPath($FrontendTerrainGlowDirectory)
$frontendTerrainBarnnorthPath = [System.IO.Path]::GetFullPath($FrontendTerrainBarnnorthDirectory)
$frontendTerrainBGbarneastPath = [System.IO.Path]::GetFullPath($FrontendTerrainBGbarneastDirectory)
$frontendTerrainBGbarnwestPath = [System.IO.Path]::GetFullPath($FrontendTerrainBGbarnwestDirectory)
$frontendTerrainHillbasePath = [System.IO.Path]::GetFullPath($FrontendTerrainHillbaseDirectory)
$frontendTerrainHilltopPath = [System.IO.Path]::GetFullPath($FrontendTerrainHilltopDirectory)
$frontendTerrainMeadowsouthPath = [System.IO.Path]::GetFullPath($FrontendTerrainMeadowsouthDirectory)
$frontendInstancesPath = [System.IO.Path]::GetFullPath($FrontendInstancesDirectory)
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

    $terrainGrassConversionArguments = @{ OutputDirectory = $frontendTerrainGrassPath }
    if ($TMDLPath) { $terrainGrassConversionArguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $terrainGrassConversionArguments.PythonPath = $PythonPath }
    & (Join-Path $PSScriptRoot 'Tools\ConvertRetailFrontendGrass.ps1') @terrainGrassConversionArguments

    $terrainGlowConversionArguments = @{ OutputDirectory = $frontendTerrainGlowPath }
    if ($TMDLPath) { $terrainGlowConversionArguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $terrainGlowConversionArguments.PythonPath = $PythonPath }
    & (Join-Path $PSScriptRoot 'Tools\ConvertRetailFrontendGlow.ps1') @terrainGlowConversionArguments

    $terrainBarnnorthConversionArguments = @{ OutputDirectory = $frontendTerrainBarnnorthPath }
    if ($TMDLPath) { $terrainBarnnorthConversionArguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $terrainBarnnorthConversionArguments.PythonPath = $PythonPath }
    & (Join-Path $PSScriptRoot 'Tools\ConvertRetailFrontendBarnnorth.ps1') @terrainBarnnorthConversionArguments

    $terrainBGbarneastConversionArguments = @{ OutputDirectory = $frontendTerrainBGbarneastPath }
    if ($TMDLPath) { $terrainBGbarneastConversionArguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $terrainBGbarneastConversionArguments.PythonPath = $PythonPath }
    & (Join-Path $PSScriptRoot 'Tools\ConvertRetailFrontendBGbarneast.ps1') @terrainBGbarneastConversionArguments

    $terrainBGbarnwestConversionArguments = @{ OutputDirectory = $frontendTerrainBGbarnwestPath }
    if ($TMDLPath) { $terrainBGbarnwestConversionArguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $terrainBGbarnwestConversionArguments.PythonPath = $PythonPath }
    & (Join-Path $PSScriptRoot 'Tools\ConvertRetailFrontendBGbarnwest.ps1') @terrainBGbarnwestConversionArguments

    $terrainHillbaseConversionArguments = @{ OutputDirectory = $frontendTerrainHillbasePath }
    if ($TMDLPath) { $terrainHillbaseConversionArguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $terrainHillbaseConversionArguments.PythonPath = $PythonPath }
    & (Join-Path $PSScriptRoot 'Tools\ConvertRetailFrontendHillbase.ps1') @terrainHillbaseConversionArguments

    $terrainHilltopConversionArguments = @{ OutputDirectory = $frontendTerrainHilltopPath }
    if ($TMDLPath) { $terrainHilltopConversionArguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $terrainHilltopConversionArguments.PythonPath = $PythonPath }
    & (Join-Path $PSScriptRoot 'Tools\ConvertRetailFrontendHilltop.ps1') @terrainHilltopConversionArguments

    $terrainMeadowsouthConversionArguments = @{ OutputDirectory = $frontendTerrainMeadowsouthPath }
    if ($TMDLPath) { $terrainMeadowsouthConversionArguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $terrainMeadowsouthConversionArguments.PythonPath = $PythonPath }
    & (Join-Path $PSScriptRoot 'Tools\ConvertRetailFrontendMeadowsouth.ps1') @terrainMeadowsouthConversionArguments

    $frontendInstanceConversionArguments = @{
        ModelNames = @('fncw01', 'fncw02', 'fncw03')
        Prefix = 'frontend_instances_fncw'
        OutputDirectory = Join-Path $frontendInstancesPath 'fncw'
    }
    if ($TMDLPath) { $frontendInstanceConversionArguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $frontendInstanceConversionArguments.PythonPath = $PythonPath }
    & (Join-Path $PSScriptRoot 'Tools\ConvertRetailFrontendInstanceSet.ps1') @frontendInstanceConversionArguments

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

$frontendGrassManifest = Join-Path $frontendTerrainGrassPath 'frontend_grass.json'
if (-not (Test-Path -LiteralPath $frontendGrassManifest -PathType Leaf)) {
    throw "Converted retail front-end grass manifest was not found: $frontendGrassManifest"
}
$frontendGrass = Get-Content -LiteralPath $frontendGrassManifest -Raw | ConvertFrom-Json
if ($frontendGrass.version -ne 1 -or $frontendGrass.source_meshes -ne 6 -or
    $frontendGrass.models.Count -ne 1 -or $frontendGrass.textures.Count -ne 1 -or
    $frontendGrass.models[0].mesh -ne 'frontend_grass_model_0.pmes' -or
    $frontendGrass.textures[0].texture -ne 'frontend_grass_texture_0.ptex') {
    throw "Converted retail front-end grass manifest is invalid: $frontendGrassManifest"
}
foreach ($fileName in @('frontend_grass.pcfg', 'frontend_grass_model_0.pmes', 'frontend_grass_texture_0.ptex')) {
    $sourcePath = Join-Path $frontendTerrainGrassPath $fileName
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Converted retail front-end grass asset was not found: $sourcePath"
    }
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $runtimePath $fileName) -Force
}

$frontendGlowManifest = Join-Path $frontendTerrainGlowPath 'frontend_glow.json'
if (-not (Test-Path -LiteralPath $frontendGlowManifest -PathType Leaf)) {
    throw "Converted retail front-end glow manifest was not found: $frontendGlowManifest"
}
$frontendGlow = Get-Content -LiteralPath $frontendGlowManifest -Raw | ConvertFrom-Json
if ($frontendGlow.version -ne 1 -or $frontendGlow.source_meshes -ne 3 -or
    $frontendGlow.models.Count -ne 3 -or $frontendGlow.textures.Count -ne 1 -or
    $frontendGlow.textures[0].texture -ne 'frontend_glow_texture_0.ptex') {
    throw "Converted retail front-end glow manifest is invalid: $frontendGlowManifest"
}
$frontendGlowFiles = @('frontend_glow.pcfg', 'frontend_glow_texture_0.ptex')
for ($modelIndex = 0; $modelIndex -lt 3; $modelIndex++) {
    $expectedModel = "frontend_glow_model_$modelIndex.pmes"
    if ($frontendGlow.models[$modelIndex].mesh -ne $expectedModel) {
        throw "Converted retail front-end glow manifest has an invalid model $modelIndex binding"
    }
    $frontendGlowFiles += $expectedModel
}
$frontendGlowConfigPath = Join-Path $frontendTerrainGlowPath 'frontend_glow.pcfg'
if (Test-Path -LiteralPath $frontendGlowConfigPath -PathType Leaf) {
    $frontendGlowConfig = [System.IO.File]::ReadAllBytes($frontendGlowConfigPath)
    if ($frontendGlowConfig.Length -ne 96 -or
        [System.BitConverter]::ToUInt32($frontendGlowConfig, 0) -ne 0x50444650 -or
        [System.BitConverter]::ToUInt32($frontendGlowConfig, 4) -ne 2 -or
        [System.BitConverter]::ToUInt32($frontendGlowConfig, 8) -ne 3 -or
        [System.BitConverter]::ToUInt32($frontendGlowConfig, 12) -ne 1 -or
        [System.BitConverter]::ToUInt32($frontendGlowConfig, 16) -ne 24 -or
        [System.BitConverter]::ToUInt32($frontendGlowConfig, 20) -ne 0) {
        throw "Converted retail front-end glow config has an invalid size: $frontendGlowConfigPath"
    }
    $expectedLightProfiles = @(1, 2, 2)
    for ($modelIndex = 0; $modelIndex -lt 3; $modelIndex++) {
        $entryOffset = 24 + $modelIndex * 24
        $alphaMode = [System.BitConverter]::ToUInt32($frontendGlowConfig, $entryOffset + 8)
        if ($alphaMode -ne 3) {
            throw "Converted retail front-end glow model $modelIndex is not additive"
        }
        $lightProfile = [System.BitConverter]::ToUInt32($frontendGlowConfig, $entryOffset + 20)
        if ($lightProfile -ne $expectedLightProfiles[$modelIndex]) {
            throw "Converted retail front-end glow model $modelIndex has an invalid original light profile"
        }
    }
}
foreach ($fileName in $frontendGlowFiles) {
    $sourcePath = Join-Path $frontendTerrainGlowPath $fileName
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Converted retail front-end glow asset was not found: $sourcePath"
    }
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $runtimePath $fileName) -Force
}

$frontendBarnnorthPacks = @(
    @{ Prefix = 'frontend_barnnorth_base'; SourceMeshes = 121; Models = 44; Textures = 44 },
    @{ Prefix = 'frontend_barnnorth_detail'; SourceMeshes = 10; Models = 8; Textures = 8 },
    @{ Prefix = 'frontend_barnnorth_grass'; SourceMeshes = 8; Models = 1; Textures = 1 },
    @{ Prefix = 'frontend_barnnorth_glow'; SourceMeshes = 2; Models = 2; Textures = 1 }
)
Get-ChildItem -LiteralPath $runtimePath -Filter 'frontend_barnnorth_*' -File -ErrorAction SilentlyContinue |
    Remove-Item -Force
foreach ($pack in $frontendBarnnorthPacks) {
    $prefix = $pack.Prefix
    $manifestPath = Join-Path $frontendTerrainBarnnorthPath "$prefix.json"
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw "Converted retail Barnnorth manifest was not found: $manifestPath"
    }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.version -ne 1 -or $manifest.source_meshes -ne $pack.SourceMeshes -or
        $manifest.models.Count -ne $pack.Models -or $manifest.textures.Count -ne $pack.Textures) {
        throw "Converted retail Barnnorth manifest is invalid: $manifestPath"
    }

    $packFiles = @("$prefix.pcfg")
    for ($modelIndex = 0; $modelIndex -lt $pack.Models; $modelIndex++) {
        $expectedModel = "${prefix}_model_$modelIndex.pmes"
        if ($manifest.models[$modelIndex].mesh -ne $expectedModel) {
            throw "Converted retail Barnnorth pack $prefix has an invalid model $modelIndex binding"
        }
        $packFiles += $expectedModel
    }
    for ($textureIndex = 0; $textureIndex -lt $pack.Textures; $textureIndex++) {
        $expectedTexture = "${prefix}_texture_$textureIndex.ptex"
        if ($manifest.textures[$textureIndex].texture -ne $expectedTexture) {
            throw "Converted retail Barnnorth pack $prefix has an invalid texture $textureIndex binding"
        }
        $packFiles += $expectedTexture
    }
    foreach ($fileName in $packFiles) {
        $sourcePath = Join-Path $frontendTerrainBarnnorthPath $fileName
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
            throw "Converted retail Barnnorth asset was not found: $sourcePath"
        }
        Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $runtimePath $fileName) -Force
    }
}

$frontendBarnnorthGlowConfigPath = Join-Path $frontendTerrainBarnnorthPath 'frontend_barnnorth_glow.pcfg'
$frontendBarnnorthGlowConfig = [System.IO.File]::ReadAllBytes($frontendBarnnorthGlowConfigPath)
if ($frontendBarnnorthGlowConfig.Length -ne 72 -or
    [System.BitConverter]::ToUInt32($frontendBarnnorthGlowConfig, 0) -ne 0x50444650 -or
    [System.BitConverter]::ToUInt32($frontendBarnnorthGlowConfig, 4) -ne 2 -or
    [System.BitConverter]::ToUInt32($frontendBarnnorthGlowConfig, 8) -ne 2 -or
    [System.BitConverter]::ToUInt32($frontendBarnnorthGlowConfig, 12) -ne 1 -or
    [System.BitConverter]::ToUInt32($frontendBarnnorthGlowConfig, 16) -ne 24 -or
    [System.BitConverter]::ToUInt32($frontendBarnnorthGlowConfig, 20) -ne 0) {
    throw "Converted retail Barnnorth glow config is invalid: $frontendBarnnorthGlowConfigPath"
}
$expectedBarnnorthLightProfiles = @(3, 7)
for ($modelIndex = 0; $modelIndex -lt 2; $modelIndex++) {
    $entryOffset = 24 + $modelIndex * 24
    if ([System.BitConverter]::ToUInt32($frontendBarnnorthGlowConfig, $entryOffset + 8) -ne 3) {
        throw "Converted retail Barnnorth glow model $modelIndex is not additive"
    }
    if ([System.BitConverter]::ToUInt32($frontendBarnnorthGlowConfig, $entryOffset + 20) -ne $expectedBarnnorthLightProfiles[$modelIndex]) {
        throw "Converted retail Barnnorth glow model $modelIndex has an invalid original light profile"
    }
}

$frontendBGbarneastManifestPath = Join-Path $frontendTerrainBGbarneastPath 'frontend_bgbarneast.json'
if (-not (Test-Path -LiteralPath $frontendBGbarneastManifestPath -PathType Leaf)) {
    throw "Converted retail BGbarneast manifest was not found: $frontendBGbarneastManifestPath"
}
$frontendBGbarneast = Get-Content -LiteralPath $frontendBGbarneastManifestPath -Raw | ConvertFrom-Json
if ($frontendBGbarneast.version -ne 1 -or $frontendBGbarneast.source_meshes -ne 49 -or
    $frontendBGbarneast.models.Count -ne 7 -or $frontendBGbarneast.textures.Count -ne 7) {
    throw "Converted retail BGbarneast manifest is invalid: $frontendBGbarneastManifestPath"
}
$frontendBGbarneastFiles = @('frontend_bgbarneast.pcfg')
for ($modelIndex = 0; $modelIndex -lt 7; $modelIndex++) {
    $expectedModel = "frontend_bgbarneast_model_$modelIndex.pmes"
    if ($frontendBGbarneast.models[$modelIndex].mesh -ne $expectedModel) {
        throw "Converted retail BGbarneast manifest has an invalid model $modelIndex binding"
    }
    $frontendBGbarneastFiles += $expectedModel
}
for ($textureIndex = 0; $textureIndex -lt 7; $textureIndex++) {
    $expectedTexture = "frontend_bgbarneast_texture_$textureIndex.ptex"
    if ($frontendBGbarneast.textures[$textureIndex].texture -ne $expectedTexture) {
        throw "Converted retail BGbarneast manifest has an invalid texture $textureIndex binding"
    }
    $frontendBGbarneastFiles += $expectedTexture
}
$frontendBGbarneastConfigPath = Join-Path $frontendTerrainBGbarneastPath 'frontend_bgbarneast.pcfg'
$frontendBGbarneastConfig = [System.IO.File]::ReadAllBytes($frontendBGbarneastConfigPath)
if ($frontendBGbarneastConfig.Length -ne 164 -or
    [System.BitConverter]::ToUInt32($frontendBGbarneastConfig, 0) -ne 0x50444650 -or
    [System.BitConverter]::ToUInt32($frontendBGbarneastConfig, 4) -ne 1 -or
    [System.BitConverter]::ToUInt32($frontendBGbarneastConfig, 8) -ne 7 -or
    [System.BitConverter]::ToUInt32($frontendBGbarneastConfig, 12) -ne 7 -or
    [System.BitConverter]::ToUInt32($frontendBGbarneastConfig, 16) -ne 20 -or
    [System.BitConverter]::ToUInt32($frontendBGbarneastConfig, 20) -ne 0) {
    throw "Converted retail BGbarneast config is invalid: $frontendBGbarneastConfigPath"
}
Get-ChildItem -LiteralPath $runtimePath -Filter 'frontend_bgbarneast_*' -File -ErrorAction SilentlyContinue |
    Remove-Item -Force
Remove-Item -LiteralPath (Join-Path $runtimePath 'frontend_bgbarneast.pcfg') -Force -ErrorAction SilentlyContinue
foreach ($fileName in $frontendBGbarneastFiles) {
    $sourcePath = Join-Path $frontendTerrainBGbarneastPath $fileName
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Converted retail BGbarneast asset was not found: $sourcePath"
    }
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $runtimePath $fileName) -Force
}

$frontendBGbarnwestManifestPath = Join-Path $frontendTerrainBGbarnwestPath 'frontend_bgbarnwest.json'
if (-not (Test-Path -LiteralPath $frontendBGbarnwestManifestPath -PathType Leaf)) {
    throw "Converted retail BGbarnwest manifest was not found: $frontendBGbarnwestManifestPath"
}
$frontendBGbarnwest = Get-Content -LiteralPath $frontendBGbarnwestManifestPath -Raw | ConvertFrom-Json
if ($frontendBGbarnwest.version -ne 1 -or $frontendBGbarnwest.source_meshes -ne 34 -or
    $frontendBGbarnwest.models.Count -ne 5 -or $frontendBGbarnwest.textures.Count -ne 5) {
    throw "Converted retail BGbarnwest manifest is invalid: $frontendBGbarnwestManifestPath"
}
$frontendBGbarnwestFiles = @('frontend_bgbarnwest.pcfg')
for ($modelIndex = 0; $modelIndex -lt 5; $modelIndex++) {
    $expectedModel = "frontend_bgbarnwest_model_$modelIndex.pmes"
    if ($frontendBGbarnwest.models[$modelIndex].mesh -ne $expectedModel) {
        throw "Converted retail BGbarnwest manifest has an invalid model $modelIndex binding"
    }
    $frontendBGbarnwestFiles += $expectedModel
}
for ($textureIndex = 0; $textureIndex -lt 5; $textureIndex++) {
    $expectedTexture = "frontend_bgbarnwest_texture_$textureIndex.ptex"
    if ($frontendBGbarnwest.textures[$textureIndex].texture -ne $expectedTexture) {
        throw "Converted retail BGbarnwest manifest has an invalid texture $textureIndex binding"
    }
    $frontendBGbarnwestFiles += $expectedTexture
}
$frontendBGbarnwestConfigPath = Join-Path $frontendTerrainBGbarnwestPath 'frontend_bgbarnwest.pcfg'
$frontendBGbarnwestConfig = [System.IO.File]::ReadAllBytes($frontendBGbarnwestConfigPath)
if ($frontendBGbarnwestConfig.Length -ne 124 -or
    [System.BitConverter]::ToUInt32($frontendBGbarnwestConfig, 0) -ne 0x50444650 -or
    [System.BitConverter]::ToUInt32($frontendBGbarnwestConfig, 4) -ne 1 -or
    [System.BitConverter]::ToUInt32($frontendBGbarnwestConfig, 8) -ne 5 -or
    [System.BitConverter]::ToUInt32($frontendBGbarnwestConfig, 12) -ne 5 -or
    [System.BitConverter]::ToUInt32($frontendBGbarnwestConfig, 16) -ne 20 -or
    [System.BitConverter]::ToUInt32($frontendBGbarnwestConfig, 20) -ne 0) {
    throw "Converted retail BGbarnwest config is invalid: $frontendBGbarnwestConfigPath"
}
Get-ChildItem -LiteralPath $runtimePath -Filter 'frontend_bgbarnwest_*' -File -ErrorAction SilentlyContinue |
    Remove-Item -Force
Remove-Item -LiteralPath (Join-Path $runtimePath 'frontend_bgbarnwest.pcfg') -Force -ErrorAction SilentlyContinue
foreach ($fileName in $frontendBGbarnwestFiles) {
    $sourcePath = Join-Path $frontendTerrainBGbarnwestPath $fileName
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Converted retail BGbarnwest asset was not found: $sourcePath"
    }
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $runtimePath $fileName) -Force
}

$frontendHillbaseManifestPath = Join-Path $frontendTerrainHillbasePath 'frontend_hillbase.json'
if (-not (Test-Path -LiteralPath $frontendHillbaseManifestPath -PathType Leaf)) {
    throw "Converted retail Hillbase manifest was not found: $frontendHillbaseManifestPath"
}
$frontendHillbase = Get-Content -LiteralPath $frontendHillbaseManifestPath -Raw | ConvertFrom-Json
if ($frontendHillbase.version -ne 1 -or $frontendHillbase.source_meshes -ne 41 -or
    $frontendHillbase.models.Count -ne 7 -or $frontendHillbase.textures.Count -ne 7) {
    throw "Converted retail Hillbase manifest is invalid: $frontendHillbaseManifestPath"
}
$frontendHillbaseFiles = @('frontend_hillbase.pcfg')
for ($modelIndex = 0; $modelIndex -lt 7; $modelIndex++) {
    $expectedModel = "frontend_hillbase_model_$modelIndex.pmes"
    if ($frontendHillbase.models[$modelIndex].mesh -ne $expectedModel) {
        throw "Converted retail Hillbase manifest has an invalid model $modelIndex binding"
    }
    $frontendHillbaseFiles += $expectedModel
}
for ($textureIndex = 0; $textureIndex -lt 7; $textureIndex++) {
    $expectedTexture = "frontend_hillbase_texture_$textureIndex.ptex"
    if ($frontendHillbase.textures[$textureIndex].texture -ne $expectedTexture) {
        throw "Converted retail Hillbase manifest has an invalid texture $textureIndex binding"
    }
    $frontendHillbaseFiles += $expectedTexture
}
$frontendHillbaseConfigPath = Join-Path $frontendTerrainHillbasePath 'frontend_hillbase.pcfg'
$frontendHillbaseConfig = [System.IO.File]::ReadAllBytes($frontendHillbaseConfigPath)
if ($frontendHillbaseConfig.Length -ne 164 -or
    [System.BitConverter]::ToUInt32($frontendHillbaseConfig, 0) -ne 0x50444650 -or
    [System.BitConverter]::ToUInt32($frontendHillbaseConfig, 4) -ne 1 -or
    [System.BitConverter]::ToUInt32($frontendHillbaseConfig, 8) -ne 7 -or
    [System.BitConverter]::ToUInt32($frontendHillbaseConfig, 12) -ne 7 -or
    [System.BitConverter]::ToUInt32($frontendHillbaseConfig, 16) -ne 20 -or
    [System.BitConverter]::ToUInt32($frontendHillbaseConfig, 20) -ne 0) {
    throw "Converted retail Hillbase config is invalid: $frontendHillbaseConfigPath"
}
Get-ChildItem -LiteralPath $runtimePath -Filter 'frontend_hillbase_*' -File -ErrorAction SilentlyContinue |
    Remove-Item -Force
Remove-Item -LiteralPath (Join-Path $runtimePath 'frontend_hillbase.pcfg') -Force -ErrorAction SilentlyContinue
foreach ($fileName in $frontendHillbaseFiles) {
    $sourcePath = Join-Path $frontendTerrainHillbasePath $fileName
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Converted retail Hillbase asset was not found: $sourcePath"
    }
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $runtimePath $fileName) -Force
}

$frontendHilltopManifestPath = Join-Path $frontendTerrainHilltopPath 'frontend_hilltop.json'
if (-not (Test-Path -LiteralPath $frontendHilltopManifestPath -PathType Leaf)) {
    throw "Converted retail Hilltop manifest was not found: $frontendHilltopManifestPath"
}
$frontendHilltop = Get-Content -LiteralPath $frontendHilltopManifestPath -Raw | ConvertFrom-Json
if ($frontendHilltop.version -ne 1 -or $frontendHilltop.source_meshes -ne 21 -or
    $frontendHilltop.models.Count -ne 9 -or $frontendHilltop.textures.Count -ne 9) {
    throw "Converted retail Hilltop manifest is invalid: $frontendHilltopManifestPath"
}
$frontendHilltopFiles = @('frontend_hilltop.pcfg')
for ($modelIndex = 0; $modelIndex -lt 9; $modelIndex++) {
    $expectedModel = "frontend_hilltop_model_$modelIndex.pmes"
    if ($frontendHilltop.models[$modelIndex].mesh -ne $expectedModel) {
        throw "Converted retail Hilltop manifest has an invalid model $modelIndex binding"
    }
    $frontendHilltopFiles += $expectedModel
}
for ($textureIndex = 0; $textureIndex -lt 9; $textureIndex++) {
    $expectedTexture = "frontend_hilltop_texture_$textureIndex.ptex"
    if ($frontendHilltop.textures[$textureIndex].texture -ne $expectedTexture) {
        throw "Converted retail Hilltop manifest has an invalid texture $textureIndex binding"
    }
    $frontendHilltopFiles += $expectedTexture
}
$frontendHilltopConfigPath = Join-Path $frontendTerrainHilltopPath 'frontend_hilltop.pcfg'
$frontendHilltopConfig = [System.IO.File]::ReadAllBytes($frontendHilltopConfigPath)
if ($frontendHilltopConfig.Length -ne 204 -or
    [System.BitConverter]::ToUInt32($frontendHilltopConfig, 0) -ne 0x50444650 -or
    [System.BitConverter]::ToUInt32($frontendHilltopConfig, 4) -ne 1 -or
    [System.BitConverter]::ToUInt32($frontendHilltopConfig, 8) -ne 9 -or
    [System.BitConverter]::ToUInt32($frontendHilltopConfig, 12) -ne 9 -or
    [System.BitConverter]::ToUInt32($frontendHilltopConfig, 16) -ne 20 -or
    [System.BitConverter]::ToUInt32($frontendHilltopConfig, 20) -ne 0) {
    throw "Converted retail Hilltop config is invalid: $frontendHilltopConfigPath"
}
Get-ChildItem -LiteralPath $runtimePath -Filter 'frontend_hilltop_*' -File -ErrorAction SilentlyContinue |
    Remove-Item -Force
Remove-Item -LiteralPath (Join-Path $runtimePath 'frontend_hilltop.pcfg') -Force -ErrorAction SilentlyContinue
foreach ($fileName in $frontendHilltopFiles) {
    $sourcePath = Join-Path $frontendTerrainHilltopPath $fileName
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Converted retail Hilltop asset was not found: $sourcePath"
    }
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $runtimePath $fileName) -Force
}

$frontendMeadowsouthManifestPath = Join-Path $frontendTerrainMeadowsouthPath 'frontend_meadowsouth.json'
if (-not (Test-Path -LiteralPath $frontendMeadowsouthManifestPath -PathType Leaf)) {
    throw "Converted retail Meadowsouth manifest was not found: $frontendMeadowsouthManifestPath"
}
$frontendMeadowsouth = Get-Content -LiteralPath $frontendMeadowsouthManifestPath -Raw | ConvertFrom-Json
if ($frontendMeadowsouth.version -ne 1 -or $frontendMeadowsouth.source_meshes -ne 48 -or
    $frontendMeadowsouth.models.Count -ne 15 -or $frontendMeadowsouth.textures.Count -ne 15) {
    throw "Converted retail Meadowsouth manifest is invalid: $frontendMeadowsouthManifestPath"
}
$frontendMeadowsouthFiles = @('frontend_meadowsouth.pcfg')
for ($modelIndex = 0; $modelIndex -lt 15; $modelIndex++) {
    $expectedModel = "frontend_meadowsouth_model_$modelIndex.pmes"
    if ($frontendMeadowsouth.models[$modelIndex].mesh -ne $expectedModel) {
        throw "Converted retail Meadowsouth manifest has an invalid model $modelIndex binding"
    }
    $frontendMeadowsouthFiles += $expectedModel
}
for ($textureIndex = 0; $textureIndex -lt 15; $textureIndex++) {
    $expectedTexture = "frontend_meadowsouth_texture_$textureIndex.ptex"
    if ($frontendMeadowsouth.textures[$textureIndex].texture -ne $expectedTexture) {
        throw "Converted retail Meadowsouth manifest has an invalid texture $textureIndex binding"
    }
    $frontendMeadowsouthFiles += $expectedTexture
}
$frontendMeadowsouthConfigPath = Join-Path $frontendTerrainMeadowsouthPath 'frontend_meadowsouth.pcfg'
$frontendMeadowsouthConfig = [System.IO.File]::ReadAllBytes($frontendMeadowsouthConfigPath)
if ($frontendMeadowsouthConfig.Length -ne 324 -or
    [System.BitConverter]::ToUInt32($frontendMeadowsouthConfig, 0) -ne 0x50444650 -or
    [System.BitConverter]::ToUInt32($frontendMeadowsouthConfig, 4) -ne 1 -or
    [System.BitConverter]::ToUInt32($frontendMeadowsouthConfig, 8) -ne 15 -or
    [System.BitConverter]::ToUInt32($frontendMeadowsouthConfig, 12) -ne 15 -or
    [System.BitConverter]::ToUInt32($frontendMeadowsouthConfig, 16) -ne 20 -or
    [System.BitConverter]::ToUInt32($frontendMeadowsouthConfig, 20) -ne 0) {
    throw "Converted retail Meadowsouth config is invalid: $frontendMeadowsouthConfigPath"
}
Get-ChildItem -LiteralPath $runtimePath -Filter 'frontend_meadowsouth_*' -File -ErrorAction SilentlyContinue |
    Remove-Item -Force
Remove-Item -LiteralPath (Join-Path $runtimePath 'frontend_meadowsouth.pcfg') -Force -ErrorAction SilentlyContinue
foreach ($fileName in $frontendMeadowsouthFiles) {
    $sourcePath = Join-Path $frontendTerrainMeadowsouthPath $fileName
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Converted retail Meadowsouth asset was not found: $sourcePath"
    }
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $runtimePath $fileName) -Force
}

$frontendInstanceSetPath = Join-Path $frontendInstancesPath 'fncw'
$frontendInstanceManifestPath = Join-Path $frontendInstanceSetPath 'frontend_instances_fncw.json'
if (-not (Test-Path -LiteralPath $frontendInstanceManifestPath -PathType Leaf)) {
    throw "Converted retail fncw instance manifest was not found: $frontendInstanceManifestPath"
}
$frontendInstanceManifest = Get-Content -LiteralPath $frontendInstanceManifestPath -Raw | ConvertFrom-Json
$frontendInstanceDefinitions = @(
    @{ Name = 'fncw01'; Index = 8; Instances = 52; Vertices = 4368; Indices = 9048; Bytes = 122952 },
    @{ Name = 'fncw02'; Index = 9; Instances = 53; Vertices = 4452; Indices = 9222; Bytes = 125316 },
    @{ Name = 'fncw03'; Index = 10; Instances = 21; Vertices = 1764; Indices = 3654; Bytes = 49668 }
)
if ($frontendInstanceManifest.version -ne 1 -or
    $frontendInstanceManifest.prefix -ne 'frontend_instances_fncw' -or
    $frontendInstanceManifest.source_model_count -ne 3 -or
    $frontendInstanceManifest.source_instances -ne 126 -or
    $frontendInstanceManifest.sources.Count -ne 3 -or
    $frontendInstanceManifest.models.Count -ne 3 -or
    $frontendInstanceManifest.textures.Count -ne 1 -or
    $frontendInstanceManifest.textures[0].texture -ne 'frontend_instances_fncw_texture_0.ptex' -or
    $frontendInstanceManifest.textures[0].source_texture -ne 'World\fencewhite.tga') {
    throw "Converted retail fncw instance manifest is invalid: $frontendInstanceManifestPath"
}
for ($index = 0; $index -lt $frontendInstanceDefinitions.Count; $index++) {
    $definition = $frontendInstanceDefinitions[$index]
    $source = $frontendInstanceManifest.sources[$index]
    $model = $frontendInstanceManifest.models[$index]
    if ($source.model_index -ne $definition.Index -or
        $source.model_name -ne $definition.Name -or
        $source.model_path -ne "data/models/$($definition.Name).trb" -or
        $source.source_meshes -ne 1 -or
        $source.source_instances -ne $definition.Instances -or
        $model.mesh -ne "frontend_instances_fncw_model_$index.pmes" -or
        $model.source_model_index -ne $definition.Index -or
        $model.source_model_name -ne $definition.Name -or
        $model.material -ne 'fencewhite' -or
        $model.source_texture -ne 'World\fencewhite.tga' -or
        $model.texture_index -ne 0 -or
        $model.vertices -ne $definition.Vertices -or
        $model.indices -ne $definition.Indices -or
        $model.bytes -ne $definition.Bytes) {
        throw "Converted retail $($definition.Name) instance geometry or binding is invalid"
    }
}
$frontendInstanceConfigPath = Join-Path $frontendInstanceSetPath 'frontend_instances_fncw.pcfg'
$frontendInstanceConfig = [System.IO.File]::ReadAllBytes($frontendInstanceConfigPath)
if ($frontendInstanceConfig.Length -ne 84 -or
    [System.BitConverter]::ToUInt32($frontendInstanceConfig, 0) -ne 0x50444650 -or
    [System.BitConverter]::ToUInt32($frontendInstanceConfig, 4) -ne 1 -or
    [System.BitConverter]::ToUInt32($frontendInstanceConfig, 8) -ne 3 -or
    [System.BitConverter]::ToUInt32($frontendInstanceConfig, 12) -ne 1 -or
    [System.BitConverter]::ToUInt32($frontendInstanceConfig, 16) -ne 20 -or
    [System.BitConverter]::ToUInt32($frontendInstanceConfig, 20) -ne 0) {
    throw "Converted retail fncw instance config is invalid: $frontendInstanceConfigPath"
}
for ($index = 0; $index -lt 3; $index++) {
    $entryOffset = 24 + $index * 20
    if ([System.BitConverter]::ToUInt32($frontendInstanceConfig, $entryOffset) -ne $index -or
        [System.BitConverter]::ToUInt32($frontendInstanceConfig, $entryOffset + 4) -ne 0) {
        throw "Converted retail fncw instance config has an invalid model $index binding"
    }
}
Get-ChildItem -LiteralPath $runtimePath -Filter 'frontend_instances_*' -File -ErrorAction SilentlyContinue |
    Remove-Item -Force
foreach ($fileName in @(
    'frontend_instances_fncw.pcfg',
    'frontend_instances_fncw_model_0.pmes',
    'frontend_instances_fncw_model_1.pmes',
    'frontend_instances_fncw_model_2.pmes',
    'frontend_instances_fncw_texture_0.ptex'
)) {
    $sourcePath = Join-Path $frontendInstanceSetPath $fileName
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Converted retail fncw instance asset was not found: $sourcePath"
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
