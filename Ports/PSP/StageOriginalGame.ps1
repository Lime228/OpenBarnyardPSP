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
    [string] $MoviesDirectory = '',
    [string] $AudioBanksDirectory = '',
    [string] $SS01TerrainDirectory = '',
    [string] $SS01OpeningDirectory = '',
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

$moviesDirectoryRequired = [bool]$MoviesDirectory
if (-not $MoviesDirectory) {
    $MoviesDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\Movies'
}
$audioBanksDirectoryRequired = [bool]$AudioBanksDirectory
if (-not $AudioBanksDirectory) {
    $AudioBanksDirectory = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\AudioBanks'
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
$frontendInstanceSets = @(
    @{
        Name = 'fncw'
        Prefix = 'frontend_instances_fncw'
        Instances = 126
        Textures = @('World\fencewhite.tga')
        Models = @(
            @{ Name = 'fncw01'; Index = 8; Instances = 52; Material = 'fencewhite'; TextureIndex = 0; Vertices = 4368; Indices = 9048; Bytes = 122952 },
            @{ Name = 'fncw02'; Index = 9; Instances = 53; Material = 'fencewhite'; TextureIndex = 0; Vertices = 4452; Indices = 9222; Bytes = 125316 },
            @{ Name = 'fncw03'; Index = 10; Instances = 21; Material = 'fencewhite'; TextureIndex = 0; Vertices = 1764; Indices = 3654; Bytes = 49668 }
        )
    },
    @{
        Name = 'fncp'
        Prefix = 'frontend_instances_fncp'
        Instances = 71
        Textures = @('World\fencepicket.tga')
        Models = @(
            @{ Name = 'fncp01'; Index = 5; Instances = 31; Material = 'fencepicket'; TextureIndex = 0; Vertices = 4092; Indices = 8184; Bytes = 114600 },
            @{ Name = 'fncp02'; Index = 6; Instances = 33; Material = 'fencepicket'; TextureIndex = 0; Vertices = 4818; Indices = 8712; Bytes = 133080 },
            @{ Name = 'fncp03'; Index = 7; Instances = 7; Material = 'fencepicket'; TextureIndex = 0; Vertices = 1190; Indices = 2058; Bytes = 32700 }
        )
    },
    @{
        Name = 'fncborder'
        Prefix = 'frontend_instances_fncborder'
        Instances = 40
        Textures = @('World\fencepigpen.tga', 'World\fenceborder.tga', 'World\fencewhite.tga')
        Models = @(
            @{ Name = 'fncb02'; Index = 1; Instances = 1; Material = 'fencepigpen'; TextureIndex = 0; Vertices = 84; Indices = 174; Bytes = 2388 },
            @{ Name = 'fncb03'; Index = 2; Instances = 1; Material = 'fencepigpen'; TextureIndex = 0; Vertices = 84; Indices = 174; Bytes = 2388 },
            @{ Name = 'fncborder01'; Index = 12; Instances = 9; Material = 'fenceborder'; TextureIndex = 1; Vertices = 756; Indices = 1566; Bytes = 21300 },
            @{ Name = 'fncborder02'; Index = 13; Instances = 4; Material = 'fenceborder'; TextureIndex = 1; Vertices = 428; Indices = 900; Bytes = 12096 },
            @{ Name = 'fncborder03'; Index = 14; Instances = 18; Material = 'fenceborder'; TextureIndex = 1; Vertices = 2322; Indices = 4752; Bytes = 65256 },
            @{ Name = 'fncbordere'; Index = 16; Instances = 1; Material = 'fenceborder'; TextureIndex = 1; Vertices = 172; Indices = 345; Bytes = 4842 },
            @{ Name = 'fncwe'; Index = 11; Instances = 6; Material = 'fencewhite'; TextureIndex = 2; Vertices = 624; Indices = 1224; Bytes = 17448 }
        )
    },
    @{
        Name = 'flowers'
        Prefix = 'frontend_instances_flowers'
        Instances = 176
        Textures = @('World\flowerpink.tga', 'World\flowerspath.tga', 'World\large_tuft.tga', 'Prop\leafyplant.tga')
        Sources = @(
            @{ Name = 'flwrs01'; Index = 31; Instances = 33; SourceMeshes = 1 },
            @{ Name = 'flwrs02'; Index = 32; Instances = 29; SourceMeshes = 1 },
            @{ Name = 'flwrsorange'; Index = 42; Instances = 32; SourceMeshes = 1 },
            @{ Name = 'flwrsbrown'; Index = 41; Instances = 28; SourceMeshes = 2 },
            @{ Name = 'flwrswhite'; Index = 43; Instances = 4; SourceMeshes = 1 },
            @{ Name = 'flwrp02'; Index = 29; Instances = 9; SourceMeshes = 1 },
            @{ Name = 'flwrp03'; Index = 30; Instances = 10; SourceMeshes = 1 },
            @{ Name = 'leafyplant'; Index = 38; Instances = 31; SourceMeshes = 1 }
        )
        Models = @(
            @{ Name = 'flwrs01'; Index = 31; Material = 'flowerpink'; TextureIndex = 0; SourceMeshIndex = 0; Vertices = 7326; Indices = 18018; Bytes = 211884 },
            @{ Name = 'flwrs02'; Index = 32; Material = 'flowerpink'; TextureIndex = 0; SourceMeshIndex = 0; Vertices = 1566; Indices = 3915; Bytes = 45438 },
            @{ Name = 'flwrsorange'; Index = 42; Material = 'flowerspath'; TextureIndex = 1; SourceMeshIndex = 0; Vertices = 5888; Indices = 13056; Bytes = 167448 },
            @{ Name = 'flwrsbrown'; Index = 41; Material = 'flowerspath'; TextureIndex = 1; SourceMeshIndex = 0; Vertices = 3360; Indices = 7896; Bytes = 96456 },
            @{ Name = 'flwrsbrown'; Index = 41; Material = 'Material_#9421'; TextureIndex = 1; SourceMeshIndex = 1; Vertices = 1344; Indices = 2016; Bytes = 36312 },
            @{ Name = 'flwrswhite'; Index = 43; Material = 'flowerspath'; TextureIndex = 1; SourceMeshIndex = 0; Vertices = 688; Indices = 1560; Bytes = 19656 },
            @{ Name = 'flwrp02'; Index = 29; Material = 'large_tuft'; TextureIndex = 2; SourceMeshIndex = 0; Vertices = 1197; Indices = 2349; Bytes = 33450 },
            @{ Name = 'flwrp03'; Index = 30; Material = 'large_tuft'; TextureIndex = 2; SourceMeshIndex = 0; Vertices = 710; Indices = 1350; Bytes = 19764 },
            @{ Name = 'leafyplant'; Index = 38; Material = 'leafyplant'; TextureIndex = 3; SourceMeshIndex = 0; Vertices = 4836; Indices = 10974; Bytes = 138036 }
        )
    },
    @{
        Name = 'rocks'
        Prefix = 'frontend_instances_rocks'
        Instances = 86
        Textures = @('World\rock_pebble.tga', 'World\sandy_pebble.tga', 'World\rock_ground.tga', 'World\granite_rock.tga')
        Sources = @(
            @{ Name = 'rockpeb03'; Index = 70; Instances = 26; SourceMeshes = 1 },
            @{ Name = 'sandypeb04'; Index = 75; Instances = 25; SourceMeshes = 1 },
            @{ Name = 'rockmedium'; Index = 90; Instances = 14; SourceMeshes = 1 },
            @{ Name = 'rocklarge'; Index = 89; Instances = 8; SourceMeshes = 1 },
            @{ Name = 'pebblebig'; Index = 49; Instances = 13; SourceMeshes = 1 }
        )
        Models = @(
            @{ Name = 'rockpeb03'; Index = 70; Material = 'rock_pebble'; TextureIndex = 0; SourceMeshIndex = 0; Vertices = 2028; Indices = 7722; Bytes = 64140 },
            @{ Name = 'sandypeb04'; Index = 75; Material = 'sandy_pebble'; TextureIndex = 1; SourceMeshIndex = 0; Vertices = 2275; Indices = 9075; Bytes = 72774 },
            @{ Name = 'rockmedium'; Index = 90; Material = 'rock_ground'; TextureIndex = 2; SourceMeshIndex = 0; Vertices = 588; Indices = 3360; Bytes = 20856 },
            @{ Name = 'rocklarge'; Index = 89; Material = 'granite_rock'; TextureIndex = 3; SourceMeshIndex = 0; Vertices = 768; Indices = 2688; Bytes = 23832 },
            @{ Name = 'pebblebig'; Index = 49; Material = 'rock_pebble'; TextureIndex = 0; SourceMeshIndex = 0; Vertices = 2041; Indices = 4992; Bytes = 58992 }
        )
    },
    @{
        Name = 'props'
        Prefix = 'frontend_instances_props'
        Instances = 97
        Textures = @('World\fencepicket.tga', 'Prop\mushy.tga', 'World\straw.tga', 'World\metal_tank.tga', 'Prop\dessert_bush2.tga', 'Prop\Trees\trunk_brown.tga', 'World\root.tga')
        Sources = @(
            @{ Name = 'fncveg01'; Index = 17; Instances = 30; SourceMeshes = 1; Path = 'data/models/fncveg01sim.trb' },
            @{ Name = 'veggiestake'; Index = 130; Instances = 30; SourceMeshes = 1 },
            @{ Name = 'mush_small'; Index = 133; Instances = 13; SourceMeshes = 1 },
            @{ Name = 'mush_big'; Index = 134; Instances = 13; SourceMeshes = 1 },
            @{ Name = 'bale01'; Index = 18; Instances = 5; SourceMeshes = 1 },
            @{ Name = 'oilbarrel'; Index = 132; Instances = 3; SourceMeshes = 1; Path = 'data/models/destruct_oilbarrel.trb' },
            @{ Name = 'desertbush'; Index = 129; Instances = 2; SourceMeshes = 2 },
            @{ Name = 'roots2'; Index = 73; Instances = 1; SourceMeshes = 1 }
        )
        Models = @(
            @{ Name = 'fncveg01'; Index = 17; Material = 'fencepicket'; TextureIndex = 0; SourceMeshIndex = 0; Vertices = 6480; Indices = 10620; Bytes = 176784 },
            @{ Name = 'veggiestake'; Index = 130; Material = 'fencepicket'; TextureIndex = 0; SourceMeshIndex = 0; Vertices = 600; Indices = 900; Bytes = 16224 },
            @{ Name = 'mush_small'; Index = 133; Material = 'mushy'; TextureIndex = 1; SourceMeshIndex = 0; Vertices = 5005; Indices = 13806; Bytes = 147756 },
            @{ Name = 'mush_big'; Index = 134; Material = 'mushy'; TextureIndex = 1; SourceMeshIndex = 0; Vertices = 4069; Indices = 11466; Bytes = 120612 },
            @{ Name = 'bale01'; Index = 18; Material = 'strawnew'; TextureIndex = 2; SourceMeshIndex = 0; Vertices = 1070; Indices = 2280; Bytes = 30264 },
            @{ Name = 'oilbarrel'; Index = 132; Material = 'metal_tank'; TextureIndex = 3; SourceMeshIndex = 0; Vertices = 465; Indices = 1413; Bytes = 14010 },
            @{ Name = 'desertbush'; Index = 129; Material = 'dessert_bush2'; TextureIndex = 4; SourceMeshIndex = 0; Vertices = 168; Indices = 528; Bytes = 5112 },
            @{ Name = 'desertbush'; Index = 129; Material = 'trunk_brown'; TextureIndex = 5; SourceMeshIndex = 1; Vertices = 212; Indices = 672; Bytes = 6456 },
            @{ Name = 'roots2'; Index = 73; Material = 'root'; TextureIndex = 6; SourceMeshIndex = 0; Vertices = 56; Indices = 180; Bytes = 1728 }
        )
    },
    @{
        Name = 'trees'
        Prefix = 'frontend_instances_trees'
        Instances = 44
        Textures = @('Prop\Trees\branches.tga', 'Prop\Trees\test3.tga', 'Prop\Trees\trunk3_10.tga', 'Prop\leafyplant.tga')
        Sources = @(
            @{ Name = 'treemidsize'; Index = 117; Instances = 15; SourceMeshes = 1 },
            @{ Name = 'treemidleaf'; Index = 118; Instances = 15; SourceMeshes = 2; Path = 'data/models/treeleaves.trb' },
            @{ Name = 'treetwiggy'; Index = 76; Instances = 14; SourceMeshes = 2 }
        )
        Models = @(
            @{ Name = 'treemidsize'; Index = 117; Material = 'body'; TextureIndex = 0; SourceMeshIndex = 0; Vertices = 2880; Indices = 10710; Bytes = 90564 },
            @{ Name = 'treemidleaf'; Index = 118; Material = 'bodyz'; TextureIndex = 0; SourceMeshIndex = 0; Vertices = 480; Indices = 720; Bytes = 12984 },
            @{ Name = 'treemidleaf'; Index = 118; Material = 'treeeuc4'; TextureIndex = 1; SourceMeshIndex = 1; Vertices = 4515; Indices = 11250; Bytes = 130884 },
            @{ Name = 'treetwiggy'; Index = 76; Material = 'tree2H'; TextureIndex = 2; SourceMeshIndex = 0; Vertices = 1960; Indices = 3066; Bytes = 53196 },
            @{ Name = 'treetwiggy'; Index = 76; Material = 'leafyplant'; TextureIndex = 3; SourceMeshIndex = 1; Vertices = 2576; Indices = 3864; Bytes = 69576 }
        )
    }
)

foreach ($requiredFile in @($gamePbpPath, $gameSfoPath)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Original PSP game build output was not found: $requiredFile"
    }
}

$retailFiles = @(
    @{ Source = 'Data\Options\Controls_Win.trb'; Destination = 'Data\Options\Controls_Win.trb' },
    @{ Source = 'Data\GUI\fonts.trb'; Destination = 'Data\GUI\fonts.trb' },
    @{ Source = 'Data\GUI\texsec.trb'; Destination = 'Data\GUI\texsec.trb' },
    @{ Source = 'Data\Locale\eng-uk.trb'; Destination = 'Data\Locale\eng-uk.trb' },
    @{ Source = 'Data\ScriptedSequence\SS01_start.tso'; Destination = 'Data\ScriptedSequence\SS01_start.tso' }
)

foreach ($retailFile in $retailFiles) {
    $sourcePath = Join-Path $installPath $retailFile.Source
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Retail runtime file was not found: $sourcePath"
    }
}

New-Item -ItemType Directory -Force -Path $runtimePath | Out-Null

if (-not $SkipConversion) {
    $inventoryArguments = @{
        InstallDirectory = $installPath
        OutputPath = Join-Path $repositoryRoot 'Game\BarnyardPC\Workspace\Inventory\retail_install.inventory.json'
    }
    if ($PythonPath) { $inventoryArguments.PythonPath = $PythonPath }
    & (Join-Path $PSScriptRoot 'Tools\InventoryRetailInstall.ps1') @inventoryArguments

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

    $frontendInstanceBatchArguments = @{ OutputDirectory = $frontendInstancesPath }
    if ($TMDLPath) { $frontendInstanceBatchArguments.TMDLPath = $TMDLPath }
    if ($PythonPath) { $frontendInstanceBatchArguments.PythonPath = $PythonPath }
    & (Join-Path $PSScriptRoot 'Tools\ConvertRetailFrontendInstanceModels.ps1') @frontendInstanceBatchArguments

    foreach ($instanceSet in $frontendInstanceSets) {
        $sourceDefinitions = if ($instanceSet.ContainsKey('Sources')) { $instanceSet.Sources } else { $instanceSet.Models }
        $frontendInstanceConversionArguments = @{
            ModelNames = @($sourceDefinitions | ForEach-Object { $_.Name })
            Prefix = $instanceSet.Prefix
            OutputDirectory = Join-Path $frontendInstancesPath $instanceSet.Name
            ReuseConvertedModels = $true
        }
        if ($TMDLPath) { $frontendInstanceConversionArguments.TMDLPath = $TMDLPath }
        if ($PythonPath) { $frontendInstanceConversionArguments.PythonPath = $PythonPath }
        & (Join-Path $PSScriptRoot 'Tools\ConvertRetailFrontendInstanceSet.ps1') @frontendInstanceConversionArguments
    }

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
    'Rekord_Black_18.ptex',
    'Panel_01.ptex',
    'Left_Arrow.ptex',
    'Statue_GoldStar.ptex',
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

Get-ChildItem -LiteralPath $runtimePath -Filter 'frontend_instances_*' -File -ErrorAction SilentlyContinue |
    Remove-Item -Force
foreach ($instanceSet in $frontendInstanceSets) {
    $frontendInstanceSetPath = Join-Path $frontendInstancesPath $instanceSet.Name
    $frontendInstanceManifestPath = Join-Path $frontendInstanceSetPath "$($instanceSet.Prefix).json"
    if (-not (Test-Path -LiteralPath $frontendInstanceManifestPath -PathType Leaf)) {
        throw "Converted retail $($instanceSet.Name) instance manifest was not found: $frontendInstanceManifestPath"
    }
    $frontendInstanceManifest = Get-Content -LiteralPath $frontendInstanceManifestPath -Raw | ConvertFrom-Json
    $sourceDefinitions = if ($instanceSet.ContainsKey('Sources')) { $instanceSet.Sources } else { $instanceSet.Models }
    $sourceModelCount = $sourceDefinitions.Count
    $modelCount = $instanceSet.Models.Count
    $textureCount = $instanceSet.Textures.Count
    if ($frontendInstanceManifest.version -ne 1 -or
        $frontendInstanceManifest.prefix -ne $instanceSet.Prefix -or
        $frontendInstanceManifest.source_model_count -ne $sourceModelCount -or
        $frontendInstanceManifest.source_instances -ne $instanceSet.Instances -or
        $frontendInstanceManifest.sources.Count -ne $sourceModelCount -or
        $frontendInstanceManifest.models.Count -ne $modelCount -or
        $frontendInstanceManifest.textures.Count -ne $textureCount) {
        throw "Converted retail $($instanceSet.Name) instance manifest is invalid: $frontendInstanceManifestPath"
    }
    for ($textureIndex = 0; $textureIndex -lt $textureCount; $textureIndex++) {
        if ($frontendInstanceManifest.textures[$textureIndex].texture -ne "$($instanceSet.Prefix)_texture_$textureIndex.ptex" -or
            $frontendInstanceManifest.textures[$textureIndex].source_texture -ne $instanceSet.Textures[$textureIndex]) {
            throw "Converted retail $($instanceSet.Name) instance texture $textureIndex is invalid"
        }
    }
    for ($sourceIndex = 0; $sourceIndex -lt $sourceModelCount; $sourceIndex++) {
        $definition = $sourceDefinitions[$sourceIndex]
        $source = $frontendInstanceManifest.sources[$sourceIndex]
        $sourceMeshCount = if ($definition.ContainsKey('SourceMeshes')) { $definition.SourceMeshes } else { 1 }
        $sourceModelPath = if ($definition.ContainsKey('Path')) { $definition.Path } else { "data/models/$($definition.Name).trb" }
        if ($source.model_index -ne $definition.Index -or
            $source.model_name -ne $definition.Name -or
            $source.model_path -ne $sourceModelPath -or
            $source.source_meshes -ne $sourceMeshCount -or
            $source.source_instances -ne $definition.Instances) {
            throw "Converted retail $($definition.Name) instance source is invalid"
        }
    }
    for ($index = 0; $index -lt $modelCount; $index++) {
        $definition = $instanceSet.Models[$index]
        $model = $frontendInstanceManifest.models[$index]
        $sourceMeshIndex = if ($definition.ContainsKey('SourceMeshIndex')) { $definition.SourceMeshIndex } else { 0 }
        if ($model.mesh -ne "$($instanceSet.Prefix)_model_$index.pmes" -or
            $model.source_model_index -ne $definition.Index -or
            $model.source_model_name -ne $definition.Name -or
            $model.source_mesh -ne "frontend_instances_$($definition.Name)_model_$sourceMeshIndex.pmes" -or
            $model.material -ne $definition.Material -or
            $model.source_texture -ne $instanceSet.Textures[$definition.TextureIndex] -or
            $model.texture_index -ne $definition.TextureIndex -or
            $model.vertices -ne $definition.Vertices -or
            $model.indices -ne $definition.Indices -or
            $model.bytes -ne $definition.Bytes) {
            throw "Converted retail $($definition.Name) instance geometry or binding is invalid"
        }
    }
    $frontendInstanceConfigPath = Join-Path $frontendInstanceSetPath "$($instanceSet.Prefix).pcfg"
    $frontendInstanceConfig = [System.IO.File]::ReadAllBytes($frontendInstanceConfigPath)
    if ($frontendInstanceConfig.Length -ne (24 + $modelCount * 20) -or
        [System.BitConverter]::ToUInt32($frontendInstanceConfig, 0) -ne 0x50444650 -or
        [System.BitConverter]::ToUInt32($frontendInstanceConfig, 4) -ne 1 -or
        [System.BitConverter]::ToUInt32($frontendInstanceConfig, 8) -ne $modelCount -or
        [System.BitConverter]::ToUInt32($frontendInstanceConfig, 12) -ne $textureCount -or
        [System.BitConverter]::ToUInt32($frontendInstanceConfig, 16) -ne 20 -or
        [System.BitConverter]::ToUInt32($frontendInstanceConfig, 20) -ne 0) {
        throw "Converted retail $($instanceSet.Name) instance config is invalid: $frontendInstanceConfigPath"
    }
    for ($index = 0; $index -lt $modelCount; $index++) {
        $entryOffset = 24 + $index * 20
        $expectedAlphaMode = if ($instanceSet.Models[$index].ContainsKey('AlphaMode')) { $instanceSet.Models[$index].AlphaMode } else { 1 }
        $expectedAddressU = if ($instanceSet.Models[$index].ContainsKey('AddressU')) { $instanceSet.Models[$index].AddressU } else { 0 }
        $expectedAddressV = if ($instanceSet.Models[$index].ContainsKey('AddressV')) { $instanceSet.Models[$index].AddressV } else { 0 }
        if ([System.BitConverter]::ToUInt32($frontendInstanceConfig, $entryOffset) -ne $index -or
            [System.BitConverter]::ToUInt32($frontendInstanceConfig, $entryOffset + 4) -ne $instanceSet.Models[$index].TextureIndex -or
            [System.BitConverter]::ToUInt32($frontendInstanceConfig, $entryOffset + 8) -ne $expectedAlphaMode -or
            [System.BitConverter]::ToUInt32($frontendInstanceConfig, $entryOffset + 12) -ne $expectedAddressU -or
            [System.BitConverter]::ToUInt32($frontendInstanceConfig, $entryOffset + 16) -ne $expectedAddressV) {
            throw "Converted retail $($instanceSet.Name) instance config has an invalid model $index binding"
        }
    }

    $frontendInstanceFiles = @("$($instanceSet.Prefix).pcfg")
    for ($index = 0; $index -lt $modelCount; $index++) {
        $frontendInstanceFiles += "$($instanceSet.Prefix)_model_$index.pmes"
    }
    for ($textureIndex = 0; $textureIndex -lt $textureCount; $textureIndex++) {
        $frontendInstanceFiles += "$($instanceSet.Prefix)_texture_$textureIndex.ptex"
    }
    foreach ($fileName in $frontendInstanceFiles) {
        $sourcePath = Join-Path $frontendInstanceSetPath $fileName
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
            throw "Converted retail $($instanceSet.Name) instance asset was not found: $sourcePath"
        }
        Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $runtimePath $fileName) -Force
    }
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

$menuAudioSource = Join-Path $AudioBanksDirectory 'FrontendMenu'
if (-not $SkipConversion -and (Test-Path -LiteralPath (Join-Path $AudioBanksDirectory 'Cues\sound_cues.json') -PathType Leaf)) {
    $python = if ($PythonPath) { $PythonPath } else { 'python' }
    & $python (Join-Path $PSScriptRoot 'Tools\convert_frontend_audio.py') $AudioBanksDirectory $menuAudioSource
    if ($LASTEXITCODE -ne 0) { throw 'Original menu audio conversion failed.' }
}
if (Test-Path -LiteralPath (Join-Path $menuAudioSource 'ui.pbnk') -PathType Leaf) {
    $menuAudioFiles = @('ui.pbnk', 'negative.ppcm', 'click.ppcm', 'back.ppcm', 'harp02.ppcm', 'harp03.ppcm', 'harp_strum.ppcm')
    foreach ($name in $menuAudioFiles) {
        if (-not (Test-Path -LiteralPath (Join-Path $menuAudioSource $name) -PathType Leaf)) {
            throw "Converted original menu audio is missing: $name"
        }
    }
    $menuAudioDestination = Join-Path $runtimePath 'Data\Sound\Menu'
    New-Item -ItemType Directory -Force -Path $menuAudioDestination | Out-Null
    foreach ($name in $menuAudioFiles) {
        Copy-Item -LiteralPath (Join-Path $menuAudioSource $name) -Destination $menuAudioDestination -Force
    }
    Write-Output 'Staged six original menu audio samples'
    $menuMusic = Join-Path $menuAudioSource 'start_screen.padp'
    if (Test-Path -LiteralPath $menuMusic -PathType Leaf) {
        Copy-Item -LiteralPath $menuMusic -Destination $menuAudioDestination -Force
        Write-Output 'Staged original main-menu music'
    }
    else {
        Write-Warning 'Original main-menu music is missing. Run convert_frontend_audio.py to refresh the audio cache.'
    }
}
elseif ($audioBanksDirectoryRequired) {
    throw "Converted original menu audio was not found: $menuAudioSource"
}
else {
    Write-Warning "Converted menu audio is missing at $menuAudioSource. Run convert_frontend_audio.py to enable it."
}

$movieSource = Join-Path $MoviesDirectory 'Data\Movies'
if (Test-Path -LiteralPath $movieSource -PathType Container) {
    foreach ($name in @('NickMovs', 'THQ', 'BTE', 'Attract')) {
        if (-not (Test-Path -LiteralPath (Join-Path $movieSource "$name.pmvf") -PathType Leaf)) {
            throw "Converted startup movie is missing: $name.pmvf"
        }
    }
    $movies = @(Get-ChildItem -LiteralPath $movieSource -Filter '*.pmvf' -File)
    foreach ($movie in $movies) {
        $stream = [System.IO.File]::OpenRead($movie.FullName)
        try {
            $header = [byte[]]::new(12)
            if ($stream.Read($header, 0, $header.Length) -ne $header.Length -or
                [System.Text.Encoding]::ASCII.GetString($header, 0, 4) -cne 'PMVF' -or
                [System.BitConverter]::ToUInt32($header, 4) -ne 2 -or
                [System.BitConverter]::ToUInt32($header, 8) -ne 48) {
                throw "Expected a PMVF v2 cache: $($movie.FullName)"
            }
        }
        finally { $stream.Dispose() }
    }
    $movieDestination = Join-Path $runtimePath 'Data\Movies'
    New-Item -ItemType Directory -Force -Path $movieDestination | Out-Null
    foreach ($movie in $movies) {
        Copy-Item -LiteralPath $movie.FullName -Destination $movieDestination -Force
    }
    Write-Output "Staged $($movies.Count) converted retail movies"
}
elseif ($moviesDirectoryRequired) {
    throw "Converted movie directory was not found: $movieSource"
}
else {
    Write-Warning "Converted movies are missing at $movieSource. Run convert_retail_movies.py to enable startup playback."
}

if ($SS01OpeningDirectory) {
    $SS01OpeningDirectory = (Resolve-Path -LiteralPath $SS01OpeningDirectory).Path
    $opening = Get-Content -LiteralPath (Join-Path $SS01OpeningDirectory 'opening.json') -Raw | ConvertFrom-Json
    if ($opening.version -notin 1, 2 -or $opening.sequence -cne 'SS01_start' -or $opening.visibility_row -cnotmatch '^[A-Za-z0-9_]+$') {
        throw 'Expected the generated SS01 opening camera manifest.'
    }
    if (-not $SS01TerrainDirectory) {
        $SS01TerrainDirectory = Join-Path (Split-Path -Parent $SS01OpeningDirectory) "TerrainPack_$($opening.visibility_row)"
    }
    $openingTerrain = Get-Content -LiteralPath (Join-Path $SS01TerrainDirectory 'terrain.json') -Raw | ConvertFrom-Json
    if ($openingTerrain.terrain -cne 'ss01' -or $openingTerrain.visibility_row -cne $opening.visibility_row) {
        throw "The opening camera requires the original $($opening.visibility_row) terrain row."
    }
    $openingFiles = @('opening.pcam', 'opening.psco')
    if ($opening.version -eq 2) {
        if ($opening.shots.Count -lt 2 -or $opening.shots.Count -gt 16 -or [int]$opening.voice.index -lt 0) {
            throw 'Invalid SS01 opening timeline.'
        }
        for ($i = 1; $i -lt $opening.shots.Count; $i++) { $openingFiles += "opening-$i.pcam" }
        $voiceSource = Join-Path (Split-Path -Parent $SS01OpeningDirectory) 'Voice'
        $voiceReport = Join-Path $voiceSource 'voice.json'
        if (Test-Path -LiteralPath $voiceReport -PathType Leaf) {
            $voice = Get-Content -LiteralPath $voiceReport -Raw | ConvertFrom-Json
            $selected = @($voice.streams | Where-Object substream -EQ $opening.voice.index)
            $name = "$($opening.voice.index).padp"
            if ($voice.version -ne 1 -or $voice.sequence -cne $opening.sequence -or $selected.Count -ne 1 -or
                $selected[0].path -cne $name -or $selected[0].name -cne $opening.voice.name) {
                throw 'The staged voice manifest does not match the original opening track.'
            }
            $openingVoice = Get-Item -LiteralPath (Join-Path $voiceSource $name)
            if ($openingVoice.PSIsContainer -or $openingVoice.Length -ne $selected[0].bytes) { throw 'Invalid cached SS01 opening voice.' }
        }
    }
    if ($opening.sky) { $openingFiles += @('sky-base.pmes', 'sky-clouds.pmes', 'sky-clouds.ptex', 'sky.psky') }
    $openingSources = foreach ($name in $openingFiles) {
        (Get-Item -LiteralPath (Join-Path $SS01OpeningDirectory $name)).FullName
    }
    $actorManifest = Join-Path $SS01OpeningDirectory 'opening_actors.json'
    $actorSources = @()
    if (Test-Path -LiteralPath $actorManifest -PathType Leaf) {
        $actors = Get-Content -LiteralPath $actorManifest -Raw | ConvertFrom-Json
        if ($opening.version -ne 2 -or $actors.version -notin 1, 2 -or $actors.sequence -cne $opening.sequence -or
            $actors.actors.Count -lt 1 -or $actors.actors.Count -gt 13 -or -not $actors.files.'actors.psac') {
            throw 'Expected the SS01 opening actor manifest.'
        }
        $actorSources = @(foreach ($file in $actors.files.PSObject.Properties) {
            if ($file.Name -cnotmatch '^(actors\.psac|actor-(root|[0-9]|1[0-2])(\.pskl|-([0-9]|1[0-5])\.panm|-[0-2]\.pmes)|texture-([0-9]|1[0-4])\.ptex)$') {
                throw "Invalid cached SS01 actor name: $($file.Name)"
            }
            $source = Get-Item -LiteralPath (Join-Path (Join-Path $SS01OpeningDirectory 'Actors') $file.Name)
            if ($source.PSIsContainer -or $source.Length -ne $file.Value) { throw "Invalid cached SS01 actor: $($file.Name)" }
            $source.FullName
        })
    }
}

if ($SS01TerrainDirectory) {
    $SS01TerrainDirectory = (Resolve-Path -LiteralPath $SS01TerrainDirectory).Path
    $ss01Index = Join-Path $SS01TerrainDirectory 'terrain.ptld'
    $index = [IO.File]::ReadAllBytes($ss01Index)
    if ($index.Length -lt 16 -or [Text.Encoding]::ASCII.GetString($index, 0, 4) -cne 'PTLD' -or
        [BitConverter]::ToUInt32($index, 4) -ne 1) {
        throw "Expected the generated SS01 terrain index at $ss01Index"
    }
    $meshes = [BitConverter]::ToUInt32($index, 8)
    $textures = [BitConverter]::ToUInt32($index, 12)
    if ($meshes -lt 1 -or $meshes -gt 512 -or $textures -lt 1 -or $textures -gt 256 -or
        $index.Length -ne 16 + 16 * $meshes) {
        throw "Invalid SS01 terrain counts in $ss01Index"
    }

    $ss01Files = @(for ($i = 0; $i -lt $meshes; $i++) { "mesh_$i.pmes" }
                   for ($i = 0; $i -lt $textures; $i++) { "texture_$i.ptex" })
    $sources = foreach ($name in $ss01Files) {
        (Get-Item -LiteralPath (Join-Path $SS01TerrainDirectory $name)).FullName
    }
    $ss01Destination = Join-Path $runtimePath 'Data\Terrain\SS01'
    New-Item -ItemType Directory -Force -Path $ss01Destination | Out-Null
    Copy-Item -LiteralPath $sources -Destination $ss01Destination -Force
    Copy-Item -LiteralPath $ss01Index -Destination $ss01Destination -Force
    Write-Output "Staged SS01 terrain ($meshes meshes and $textures shared textures)"
}

if ($SS01OpeningDirectory) {
    $openingDestination = Join-Path $runtimePath 'Data\ScriptedSequence\SS01'
    New-Item -ItemType Directory -Force -Path $openingDestination | Out-Null
    Copy-Item -LiteralPath $openingSources -Destination $openingDestination -Force
    $actorDestination = Join-Path $openingDestination 'Actors'
    $actorIndex = Join-Path $actorDestination 'actors.psac'
    if ($actorSources.Count -gt 0) {
        New-Item -ItemType Directory -Force -Path $actorDestination | Out-Null
        Copy-Item -LiteralPath $actorSources -Destination $actorDestination -Force
        Write-Output "Staged $($actors.actors.Count) original SS01 opening actors"
    }
    elseif (Test-Path -LiteralPath $actorIndex -PathType Leaf) {
        Remove-Item -LiteralPath $actorIndex -Force
    }
    if ($openingVoice) {
        $voiceDestination = Join-Path $openingDestination 'Voice'
        New-Item -ItemType Directory -Force -Path $voiceDestination | Out-Null
        Copy-Item -LiteralPath $openingVoice.FullName -Destination $voiceDestination -Force
        Write-Output "Staged the original voice stream $($opening.voice.name)"
    }
    Write-Output 'Staged the original SS01 opening'
}

Copy-Item -LiteralPath $gamePbpPath -Destination (Join-Path $runtimePath 'EBOOT.PBP') -Force
Copy-Item -LiteralPath $gameSfoPath -Destination (Join-Path $runtimePath 'PARAM.SFO') -Force

Write-Output "Staged original Barnyard PSP runtime at $runtimePath"
Write-Output "Run it with: .\Ports\PSP\RunPPSSPP.ps1 -Action Restart -OriginalGame"
