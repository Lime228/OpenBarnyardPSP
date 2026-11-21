param(
    [string] $TMDLPath = '',
    [string] $PythonPath = '',
    [string] $OutputDirectory = '',
    [string] $RuntimeDirectory = '',
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

$terrainPath = Join-Path $installRoot 'Data\Terrain\frontend\Barn_L0Mod1.trb'
$materialPropertiesPath = Join-Path $installRoot 'Data\MiscTextures\MaterialProperties.trb'
$textureLibraries = @(
    (Join-Path $installRoot 'Data\Terrain\frontend\Barn_L0Mat.ttl'),
    (Join-Path $installRoot 'Data\Terrain\frontend\frontend_Common.ttl')
)
foreach ($requiredPath in @($terrainPath, $materialPropertiesPath) + $textureLibraries) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required original Barnyard asset was not found: $requiredPath"
    }
}

if (-not $TMDLPath) {
    $TMDLPath = Get-ChildItem (Join-Path $repositoryRoot 'Runtime\CIArtifacts') -Filter 'TMDL.exe' -Recurse -File |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
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
$terrainDumpPath = Join-Path $outputPath 'Barn_L0Mod1.json'
$propertiesDumpPath = Join-Path $outputPath 'material_properties.json'
$manifestPath = Join-Path $outputPath 'frontend_detail.json'
$configPath = Join-Path $outputPath 'frontend_detail.pcfg'
New-Item -ItemType Directory -Force -Path $outputPath,(Join-Path (Split-Path -Parent $TMDLPath) 'Logs') | Out-Null

Push-Location (Split-Path -Parent $TMDLPath)
try {
    & $TMDLPath -psp-terrain-cell $terrainPath -world 0 -cell 0 -o $terrainDumpPath
    if ($LASTEXITCODE -ne 0) {
        throw "Original Barn_L0Mod1 export failed with exit code $LASTEXITCODE"
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
    $terrainDumpPath $outputPath --prefix frontend_detail
if ($LASTEXITCODE -ne 0) {
    throw "Original front-end detail conversion failed with exit code $LASTEXITCODE"
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$alphaModes = @()
$intermediatePaths = @()
for ($textureIndex = 0; $textureIndex -lt $manifest.textures.Count; $textureIndex++) {
    $texture = $manifest.textures[$textureIndex]
    $ddsPath = Join-Path $outputPath "frontend_detail_texture_$textureIndex.dds"
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
        throw "Original texture '$($texture.source_texture)' was not found in the Barn front-end libraries"
    }

    $conversionOutput = @(& $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'convert_dds.py') `
        --max-width $TextureMaxWidth $ddsPath $ptexPath)
    if ($LASTEXITCODE -ne 0 -or $conversionOutput.Count -eq 0) {
        throw "Original texture '$($texture.source_texture)' conversion failed"
    }
    $textureSummary = $conversionOutput[-1] | ConvertFrom-Json
    $alphaModes += $textureSummary.alpha_mode
}

& $PythonPath @pythonArguments (Join-Path $PSScriptRoot 'finalize_frontend_terrain_pack.py') `
    $manifestPath $propertiesDumpPath $configPath --texture-alpha-modes ($alphaModes -join ',')
if ($LASTEXITCODE -ne 0) {
    throw "Original front-end detail material finalization failed with exit code $LASTEXITCODE"
}

if ($RuntimeDirectory) {
    $runtimePath = [System.IO.Path]::GetFullPath($RuntimeDirectory)
    New-Item -ItemType Directory -Force -Path $runtimePath | Out-Null
    Get-ChildItem -LiteralPath $runtimePath -Filter 'frontend_detail*' -File -ErrorAction SilentlyContinue |
        Remove-Item -Force
    Get-ChildItem -LiteralPath $outputPath -Filter 'frontend_detail*.pmes' -File |
        Copy-Item -Destination $runtimePath -Force
    Get-ChildItem -LiteralPath $outputPath -Filter 'frontend_detail*.ptex' -File |
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
    Models = $manifest.models.Count
    Textures = $manifest.textures.Count
    GeometryBytes = $geometryBytes
    TextureMaxWidth = $TextureMaxWidth
    AlphaModes = (($alphaModes | Group-Object | ForEach-Object { "$($_.Name):$($_.Count)" }) -join ',')
}
