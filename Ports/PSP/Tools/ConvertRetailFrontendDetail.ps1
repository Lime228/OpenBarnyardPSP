param(
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $OutputDirectory = '',
    [string] $RuntimeDirectory = '',
    [string] $SourceModelName = 'Barn_L0Mod1',
    [string] $SourceMaterialLibraryName = 'Barn_L0Mat',
    [string] $Prefix = 'frontend_detail',
    [string] $ForceAlphaMode = '',
    [string[]] $LightProfiles = @(),
    [int] $TextureMaxWidth = 128,
    [switch] $KeepIntermediate
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$assetRoot = Join-Path $repositoryRoot 'Game\BarnyardPC'
$installRoot = Join-Path $assetRoot 'Install'
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $assetRoot 'Workspace\FrontendTerrainPack'
}

if (-not $SourceModelName -or $SourceModelName -notmatch '^[A-Za-z0-9_]+$') {
    throw 'SourceModelName must contain only letters, numbers and underscores'
}
if (-not $SourceMaterialLibraryName -or $SourceMaterialLibraryName -notmatch '^[A-Za-z0-9_]+$') {
    throw 'SourceMaterialLibraryName must contain only letters, numbers and underscores'
}
if (-not $Prefix -or $Prefix -notmatch '^[A-Za-z0-9_]+$') {
    throw 'Prefix must contain only letters, numbers and underscores'
}
if ($ForceAlphaMode -and $ForceAlphaMode -notin @('mask', 'blend', 'additive')) {
    throw 'ForceAlphaMode must be mask, blend or additive'
}
foreach ($lightProfile in $LightProfiles) {
    if ($lightProfile -notmatch '^[A-Za-z0-9_]+=(barn|cone|farmhouse|neighbour|lodge|beady|nightlight)$') {
        throw "LightProfiles entry '$lightProfile' has an invalid original light schedule"
    }
}
$terrainPath = Join-Path $installRoot "Data\Terrain\frontend\$SourceModelName.trb"
$materialPropertiesPath = Join-Path $installRoot 'Data\MiscTextures\MaterialProperties.trb'
$textureLibraries = @(
    (Join-Path $installRoot "Data\Terrain\frontend\$SourceMaterialLibraryName.ttl"),
    (Join-Path $installRoot 'Data\Terrain\frontend\frontend_Common.ttl')
)
foreach ($requiredPath in @($terrainPath, $materialPropertiesPath) + $textureLibraries) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required original Barnyard asset was not found: $requiredPath"
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
if ($TextureMaxWidth -notin @(32, 64, 128, 256, 512)) {
    throw 'TextureMaxWidth must be a PSP-safe power of two between 32 and 512'
}

$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
$terrainDumpPath = Join-Path $outputPath "$SourceModelName.json"
$propertiesDumpPath = Join-Path $outputPath 'material_properties.json'
$manifestPath = Join-Path $outputPath "$Prefix.json"
$configPath = Join-Path $outputPath "$Prefix.pcfg"
New-Item -ItemType Directory -Force -Path $outputPath,(Join-Path (Split-Path -Parent $TMDLPath) 'Logs') | Out-Null

Push-Location (Split-Path -Parent $TMDLPath)
try {
    & $TMDLPath -psp-terrain-cell $terrainPath -world 0 -cell 0 -o $terrainDumpPath
    if ($LASTEXITCODE -ne 0) {
        throw "Original $SourceModelName export failed with exit code $LASTEXITCODE"
    }
    & $TMDLPath -psp-material-properties $materialPropertiesPath -o $propertiesDumpPath
    if ($LASTEXITCODE -ne 0) {
        throw "Original material properties export failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

& $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'convert_frontend_terrain_pack.py') `
    $terrainDumpPath $outputPath --prefix $Prefix
if ($LASTEXITCODE -ne 0) {
    throw "Original front-end detail conversion failed with exit code $LASTEXITCODE"
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$alphaModes = @()
$intermediatePaths = @()
for ($textureIndex = 0; $textureIndex -lt $manifest.textures.Count; $textureIndex++) {
    $texture = $manifest.textures[$textureIndex]
    $ddsPath = Join-Path $outputPath "${Prefix}_texture_$textureIndex.dds"
    $ptexPath = Join-Path $outputPath $texture.texture
    $intermediatePaths += $ddsPath
    $textureFound = $false
    foreach ($libraryPath in $textureLibraries) {
        Push-Location (Split-Path -Parent $TMDLPath)
        try {
            & $TMDLPath -psp-texture $libraryPath -texture $texture.source_texture -o $ddsPath
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
        throw "Original texture '$($texture.source_texture)' was not found in the $SourceMaterialLibraryName front-end libraries"
    }

    $conversionOutput = @(& $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'convert_dds.py') `
        --max-width $TextureMaxWidth $ddsPath $ptexPath)
    if ($LASTEXITCODE -ne 0 -or $conversionOutput.Count -eq 0) {
        throw "Original texture '$($texture.source_texture)' conversion failed"
    }
    $textureSummary = $conversionOutput[-1] | ConvertFrom-Json
    $alphaModes += $textureSummary.alpha_mode
}

$finalizeArguments = @(
    (Join-Path $PSScriptRoot 'finalize_frontend_terrain_pack.py'),
    $manifestPath,
    $propertiesDumpPath,
    $configPath,
    '--texture-alpha-modes',
    ($alphaModes -join ',')
)
if ($ForceAlphaMode) {
    $finalizeArguments += @('--force-alpha-mode', $ForceAlphaMode)
}
foreach ($lightProfile in $LightProfiles) {
    $finalizeArguments += @('--light-profile', $lightProfile)
}
& $PythonPath @pythonArguments @finalizeArguments
if ($LASTEXITCODE -ne 0) {
    throw "Original front-end detail material finalization failed with exit code $LASTEXITCODE"
}

if ($RuntimeDirectory) {
    $runtimePath = [System.IO.Path]::GetFullPath($RuntimeDirectory)
    New-Item -ItemType Directory -Force -Path $runtimePath | Out-Null
    Get-ChildItem -LiteralPath $runtimePath -Filter "$Prefix*" -File -ErrorAction SilentlyContinue |
        Remove-Item -Force
    Get-ChildItem -LiteralPath $outputPath -Filter "$Prefix*.pmes" -File |
        Copy-Item -Destination $runtimePath -Force
    Get-ChildItem -LiteralPath $outputPath -Filter "$Prefix*.ptex" -File |
        Copy-Item -Destination $runtimePath -Force
    Copy-Item -LiteralPath $configPath -Destination $runtimePath -Force
    Write-Output "Published original front-end detail pack to $runtimePath"
}

if (-not $KeepIntermediate) {
    foreach ($intermediatePath in $intermediatePaths) {
        Remove-Item -LiteralPath $intermediatePath -Force -ErrorAction SilentlyContinue
    }
}

$geometryBytes = ($manifest.models | Measure-Object bytes -Sum).Sum
[pscustomobject]@{
    SourceMeshes = $manifest.source_meshes
    Prefix = $Prefix
    Models = $manifest.models.Count
    Textures = $manifest.textures.Count
    GeometryBytes = $geometryBytes
    TextureMaxWidth = $TextureMaxWidth
    AlphaModes = if ($ForceAlphaMode) {
        "${ForceAlphaMode}:$($manifest.models.Count)"
    }
    else {
        (($alphaModes | Group-Object | ForEach-Object { "$($_.Name):$($_.Count)" }) -join ',')
    }
}
